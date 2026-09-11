/*
 * %kadu copyright begin%
 * Copyright 2026 Kadu Qt6 port
 * %kadu copyright end%
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "matrix-chat-state-service.h"
#include "matrix-chat-state-service.moc"

#include "chat/chat-details-room.h"
#include "chat/chat-manager.h"
#include "chat/chat-storage.h"
#include "chat/type/chat-type-room.h"
#include "protocols/services/chat-state.h"

#include <Quotient/connection.h>
#include <Quotient/csapi/typing.h>
#include <Quotient/room.h>
#include <Quotient/roommember.h>
#include <Quotient/user.h>

MatrixChatStateService::MatrixChatStateService(Account account, QObject *parent)
        : ChatStateService{account, parent}
{
}

void MatrixChatStateService::setConnection(Quotient::Connection *connection)
{
    if (m_connection == connection)
        return;

    if (m_connection)
        disconnect(m_connection, nullptr, this, nullptr);
    for (auto *room : m_watchedRooms)
        disconnect(room, nullptr, this, nullptr);

    m_connection = connection;
    m_watchedRooms.clear();
    m_typingMembers.clear();
    m_sentTypingStates.clear();

    if (!m_connection)
        return;

    connect(m_connection, &Quotient::Connection::newRoom, this, &MatrixChatStateService::watchRoom);
    connect(m_connection, &Quotient::Connection::joinedRoom, this,
            [this](Quotient::Room *room, Quotient::Room *) { watchRoom(room); });
    connect(m_connection, &Quotient::Connection::syncDone, this, [this] {
        for (auto *room : m_connection->allRooms())
            synchronizeTypingMembers(room);
    });

    for (auto *room : m_connection->allRooms())
        watchRoom(room);
}

void MatrixChatStateService::sendState(const Contact &contact, ChatState state)
{
    Q_UNUSED(contact)
    Q_UNUSED(state)
}

void MatrixChatStateService::sendState(const Chat &chat, ChatState state)
{
    if (!m_connection || !m_connection->isLoggedIn())
        return;

    auto *room = roomForChat(chat);
    if (!room)
        return;

    const auto roomId = room->id();
    const auto typing = state == ChatState::Composing;
    if (m_sentTypingStates.value(roomId, false) == typing)
        return;

    m_sentTypingStates.insert(roomId, typing);
    if (typing)
        m_connection->callApi<Quotient::SetTypingJob>(m_connection->userId(), roomId, true, TypingTimeout);
    else
        m_connection->callApi<Quotient::SetTypingJob>(m_connection->userId(), roomId, false);
}

QVector<ChatStatePeer> MatrixChatStateService::activePeerStates(const Chat &chat) const
{
    auto *room = roomForChat(chat);
    if (!room)
        return {};

    QVector<ChatStatePeer> result;
    const auto memberIds = m_typingMembers.value(room);
    result.reserve(memberIds.size());
    for (const auto &memberId : memberIds)
    {
        const auto member = room->member(memberId);
        const auto displayName = member.isEmpty() ? memberId : member.disambiguatedName();
        result.push_back({memberId, displayName, ChatState::Composing});
    }
    return result;
}

void MatrixChatStateService::setChatManager(ChatManager *chatManager)
{
    m_chatManager = chatManager;
}

void MatrixChatStateService::setChatStorage(ChatStorage *chatStorage)
{
    m_chatStorage = chatStorage;
}

Quotient::Room *MatrixChatStateService::roomForChat(const Chat &chat) const
{
    if (!m_connection || !chat)
        return nullptr;

    const auto *details = qobject_cast<ChatDetailsRoom *>(chat.details());
    return details ? m_connection->room(details->room(), Quotient::JoinState::Join) : nullptr;
}

Chat MatrixChatStateService::chatForRoom(Quotient::Room *room) const
{
    if (!m_chatManager || !m_chatStorage || !room || room->joinState() != Quotient::JoinState::Join)
        return Chat::null;

    return ChatTypeRoom::findChat(m_chatManager, m_chatStorage, account(), room->id(), ActionCreateAndAdd);
}

void MatrixChatStateService::watchRoom(Quotient::Room *room)
{
    if (!room || m_watchedRooms.contains(room))
        return;

    m_watchedRooms.insert(room);
    connect(room, &Quotient::Room::typingChanged, this,
            [this, room] { synchronizeTypingMembers(room); });
    connect(room, &Quotient::Room::joinStateChanged, this,
            [this, room](Quotient::JoinState, Quotient::JoinState newState) {
                m_sentTypingStates.remove(room->id());
                if (newState == Quotient::JoinState::Join)
                    synchronizeTypingMembers(room);
                else
                    m_typingMembers.remove(room);
            });
    const auto roomId = room->id();
    connect(room, &QObject::destroyed, this, [this, room, roomId] {
        m_watchedRooms.remove(room);
        m_typingMembers.remove(room);
        m_sentTypingStates.remove(roomId);
    });
    synchronizeTypingMembers(room);
}

void MatrixChatStateService::synchronizeTypingMembers(Quotient::Room *room)
{
    if (!m_connection || !room || room->joinState() != Quotient::JoinState::Join)
        return;

    QSet<QString> currentMembers;
    for (const auto &member : room->otherMembersTyping())
        currentMembers.insert(member.id());

    const auto previousMembers = m_typingMembers.value(room);
    const auto chat = chatForRoom(room);
    if (!chat)
        return;
    for (const auto &matrixId : previousMembers)
    {
        if (currentMembers.contains(matrixId))
            continue;

        const auto member = room->member(matrixId);
        const auto displayName = member.isEmpty() ? matrixId : member.disambiguatedName();
        emit peerStateChangedInChat(chat, matrixId, displayName, ChatState::Paused);
    }
    for (const auto &matrixId : currentMembers)
    {
        if (previousMembers.contains(matrixId))
            continue;

        const auto member = room->member(matrixId);
        const auto displayName = member.isEmpty() ? matrixId : member.disambiguatedName();
        emit peerStateChangedInChat(chat, matrixId, displayName, ChatState::Composing);
    }

    if (currentMembers.isEmpty())
        m_typingMembers.remove(room);
    else
        m_typingMembers.insert(room, currentMembers);
}
