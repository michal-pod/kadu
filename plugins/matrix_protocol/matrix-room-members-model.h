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

#pragma once

#include "accounts/account.h"
#include "model/kadu-abstract-model.h"

#include <QtCore/QAbstractListModel>
#include <QtCore/QHash>
#include <QtCore/QPointer>
#include <QtCore/QStringList>
#include <QtCore/QUrl>

class Contact;
class ContactManager;

namespace Quotient
{
class Room;
class RoomMember;
}

class MatrixRoomMembersModel final : public QAbstractListModel, public KaduAbstractModel
{
public:
    explicit MatrixRoomMembersModel(
        Account account, Quotient::Room *room, ContactManager *contactManager, QObject *parent = nullptr);
    virtual ~MatrixRoomMembersModel();

    virtual int rowCount(const QModelIndex &parent = {}) const override;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual bool canFetchMore(const QModelIndex &parent) const override;
    virtual void fetchMore(const QModelIndex &parent) override;
    virtual QModelIndexList indexListForValue(const QVariant &value) const override;

private:
    struct InvitedMember
    {
        QString displayName;
        QUrl avatarUrl;
    };

    static constexpr auto PageSize = 100;
    static constexpr auto AvatarSize = 48;

    Account m_account;
    QPointer<Quotient::Room> m_room;
    QPointer<ContactManager> m_contactManager;
    QHash<QString, InvitedMember> m_invitedMembers;
    QStringList m_memberIds;
    int m_visibleMemberCount = 0;
    bool m_invitedMembersLoading = false;
    bool m_invitedMembersReloadPending = false;

    Contact contactForMember(const QString &memberId) const;
    void loadInvitedMembers();
    void reload();
    void updateMember(const QString &memberId);
};
