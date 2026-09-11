/*
 * %kadu copyright begin%
 * Copyright 2026 Kadu Qt6 port
 * %kadu copyright end%
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#include "matrix-room-members-model.h"

#include "contacts/contact-manager.h"
#include "contacts/contact.h"
#include "model/roles.h"
#include "talkable/talkable.h"

#include <Quotient/avatar.h>
#include <Quotient/connection.h>
#include <Quotient/csapi/rooms.h>
#include <Quotient/events/roommemberevent.h>
#include <Quotient/room.h>
#include <Quotient/roommember.h>

#include <QtGui/QPixmap>

#include <algorithm>
#include <utility>

MatrixRoomMembersModel::MatrixRoomMembersModel(
    Account account, Quotient::Room *room, ContactManager *contactManager, QObject *parent)
        : QAbstractListModel{parent}, m_account{account}, m_room{room}, m_contactManager{contactManager}
{
    if (!m_room)
        return;

    connect(m_room, &Quotient::Room::memberListChanged, this, [this] {
        reload();
        loadInvitedMembers();
    });
    connect(m_room, &Quotient::Room::allMembersLoaded, this, &MatrixRoomMembersModel::reload);
    connect(m_room, &Quotient::Room::memberNameUpdated, this,
            [this](const Quotient::RoomMember &member) { updateMember(member.id()); });
    connect(m_room, &Quotient::Room::memberAvatarUpdated, this,
            [this](const Quotient::RoomMember &member) { updateMember(member.id()); });
    connect(m_room, &QObject::destroyed, this, [this] {
        beginResetModel();
        m_memberIds.clear();
        m_visibleMemberCount = 0;
        endResetModel();
    });

    reload();
    loadInvitedMembers();
    m_room->setDisplayed(true);
}

MatrixRoomMembersModel::~MatrixRoomMembersModel()
{
    if (m_room)
        m_room->setDisplayed(false);
}

int MatrixRoomMembersModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_visibleMemberCount;
}

QVariant MatrixRoomMembersModel::data(const QModelIndex &index, int role) const
{
    if (!m_room || !index.isValid() || index.row() < 0 || index.row() >= m_visibleMemberCount)
        return {};

    const auto memberId = m_memberIds.at(index.row());
    const auto invitedMember = m_invitedMembers.constFind(memberId);
    const auto invited = invitedMember != m_invitedMembers.cend();
    const auto member = m_room->member(memberId);
    const auto displayName = invited
                                 ? (invitedMember->displayName.isEmpty() ? memberId : invitedMember->displayName)
                                 : (member.isEmpty() ? memberId : member.disambiguatedName());

    switch (role)
    {
    case Qt::DisplayRole:
        return displayName;
    case Qt::ToolTipRole:
        return invited || member.isEmpty() ? memberId : member.fullName();
    case DescriptionRole:
    {
        const auto mxid = displayName == memberId ? QString{} : memberId;
        if (!invited)
            return mxid;
        return mxid.isEmpty() ? tr("Invited") : tr("%1 — invited").arg(mxid);
    }
    case AvatarRole:
        if (invited && m_room->connection() && !invitedMember->avatarUrl.isEmpty())
        {
            const QPointer<MatrixRoomMembersModel> model{
                const_cast<MatrixRoomMembersModel *>(this)};
            return QPixmap::fromImage(m_room->connection()->userAvatar(invitedMember->avatarUrl).get(
                AvatarSize, [model, memberId] {
                    if (model)
                        model->updateMember(memberId);
                }));
        }
        return QPixmap::fromImage(m_room->memberAvatar(memberId, AvatarSize));
    case AccountRole:
        return QVariant::fromValue(m_account);
    case ContactRole:
        return QVariant::fromValue(contactForMember(memberId));
    case ItemTypeRole:
        return ContactRole;
    case TalkableRole:
        return QVariant::fromValue(Talkable{contactForMember(memberId)});
    default:
        return {};
    }
}

bool MatrixRoomMembersModel::canFetchMore(const QModelIndex &parent) const
{
    return !parent.isValid() && m_visibleMemberCount < m_memberIds.size();
}

void MatrixRoomMembersModel::fetchMore(const QModelIndex &parent)
{
    if (parent.isValid() || m_visibleMemberCount >= m_memberIds.size())
        return;

    const auto addedCount =
        std::min(PageSize, static_cast<int>(m_memberIds.size()) - m_visibleMemberCount);
    beginInsertRows({}, m_visibleMemberCount, m_visibleMemberCount + addedCount - 1);
    m_visibleMemberCount += addedCount;
    endInsertRows();
}

QModelIndexList MatrixRoomMembersModel::indexListForValue(const QVariant &value) const
{
    const auto contact = value.value<Contact>();
    if (!contact || contact.contactAccount() != m_account)
        return {};

    const auto row = m_memberIds.indexOf(contact.id());
    return row >= 0 && row < m_visibleMemberCount ? QModelIndexList{index(row, 0)} : QModelIndexList{};
}

Contact MatrixRoomMembersModel::contactForMember(const QString &memberId) const
{
    if (m_account.accountContact().id() == memberId)
        return m_account.accountContact();
    return m_contactManager ? m_contactManager->byId(m_account, memberId, ActionCreateAndAdd) : Contact::null;
}

void MatrixRoomMembersModel::loadInvitedMembers()
{
    if (!m_room || !m_room->connection())
        return;
    if (m_invitedMembersLoading)
    {
        m_invitedMembersReloadPending = true;
        return;
    }

    m_invitedMembersLoading = true;
    m_invitedMembersReloadPending = false;
    m_room->connection()
        ->callApi<Quotient::GetMembersByRoomJob>(
            m_room->id(), QString{}, QStringLiteral("invite"))
        .then(this, [this](Quotient::GetMembersByRoomJob *job) {
            QHash<QString, InvitedMember> invitedMembers;
            for (const auto &event : job->chunk())
            {
                const auto *memberEvent = Quotient::eventCast<const Quotient::RoomMemberEvent>(event);
                if (!memberEvent || memberEvent->membership() != Quotient::Membership::Invite ||
                    memberEvent->userId().isEmpty())
                    continue;

                invitedMembers.insert(
                    memberEvent->userId(),
                    {memberEvent->newDisplayName().value_or(QString{}),
                     memberEvent->newAvatarUrl().value_or(QUrl{})});
            }

            m_invitedMembers = std::move(invitedMembers);
            m_invitedMembersLoading = false;
            reload();
            if (m_invitedMembersReloadPending)
                loadInvitedMembers();
        }, [this](Quotient::GetMembersByRoomJob *) {
            m_invitedMembersLoading = false;
            if (m_invitedMembersReloadPending)
                loadInvitedMembers();
        });
}

void MatrixRoomMembersModel::reload()
{
    if (!m_room)
        return;

    auto memberIds = m_room->joinedMemberIds();
    for (auto invitedMember = m_invitedMembers.cbegin(); invitedMember != m_invitedMembers.cend(); ++invitedMember)
        if (!memberIds.contains(invitedMember.key()))
            memberIds.append(invitedMember.key());
    const auto room = m_room.data();
    const auto invitedMembers = &m_invitedMembers;
    std::stable_sort(
        memberIds.begin(), memberIds.end(), [room, invitedMembers](const QString &left, const QString &right) {
            const auto leftInvited = invitedMembers->contains(left);
            const auto rightInvited = invitedMembers->contains(right);
            if (leftInvited != rightInvited)
                return !leftInvited;
            const auto leftName = leftInvited
                                      ? invitedMembers->value(left).displayName
                                      : room->member(left).disambiguatedName();
            const auto rightName = rightInvited
                                       ? invitedMembers->value(right).displayName
                                       : room->member(right).disambiguatedName();
            return (leftName.isEmpty() ? left : leftName).localeAwareCompare(
                       rightName.isEmpty() ? right : rightName) < 0;
        });

    beginResetModel();
    m_memberIds = std::move(memberIds);
    m_visibleMemberCount =
        std::min(std::max(m_visibleMemberCount, PageSize), static_cast<int>(m_memberIds.size()));
    endResetModel();
}

void MatrixRoomMembersModel::updateMember(const QString &memberId)
{
    const auto row = m_memberIds.indexOf(memberId);
    if (row < 0 || row >= m_visibleMemberCount)
        return;

    const auto memberIndex = index(row, 0);
    emit dataChanged(memberIndex, memberIndex);
}
