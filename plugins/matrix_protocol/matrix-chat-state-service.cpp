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

#include "contacts/contact-manager.h"
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

void MatrixChatStateService::setContactManager(ContactManager *contactManager)
{
    m_contactManager = contactManager;
}

void MatrixChatStateService::sendState(const Contact &contact, ChatState state)
{
    if (!m_connection || !m_connection->isLoggedIn() || !contact)
        return;

    const auto roomId = directRoomId(contact);
    if (roomId.isEmpty())
        return;

    const auto typing = state == ChatState::Composing;
    if (m_sentTypingStates.value(roomId, false) == typing)
        return;

    m_sentTypingStates.insert(roomId, typing);
    if (typing)
        m_connection->callApi<Quotient::SetTypingJob>(m_connection->userId(), roomId, true, TypingTimeout);
    else
        m_connection->callApi<Quotient::SetTypingJob>(m_connection->userId(), roomId, false);
}

QString MatrixChatStateService::directRoomId(const Contact &contact) const
{
    if (!m_connection)
        return {};

    const auto directChats = m_connection->directChats();
    for (auto it = directChats.cbegin(); it != directChats.cend(); ++it)
    {
        if (!it.key() || it.key()->id() != contact.id())
            continue;

        if (m_connection->room(it.value(), Quotient::JoinState::Join))
            return it.value();
    }
    return {};
}

void MatrixChatStateService::watchRoom(Quotient::Room *room)
{
    if (!room || m_watchedRooms.contains(room))
        return;

    m_watchedRooms.insert(room);
    connect(room, &Quotient::Room::typingChanged, this,
            [this, room] { synchronizeTypingMembers(room); });
    connect(room, &QObject::destroyed, this, [this, room] {
        m_watchedRooms.remove(room);
        m_typingMembers.remove(room);
    });
    synchronizeTypingMembers(room);
}

void MatrixChatStateService::synchronizeTypingMembers(Quotient::Room *room)
{
    if (!m_connection || !m_contactManager || !room)
        return;

    QSet<QString> currentMembers;
    if (m_connection->isDirectChat(room->id()))
    {
        for (const auto &member : room->otherMembersTyping())
            currentMembers.insert(member.id());
    }

    const auto previousMembers = m_typingMembers.value(room);
    for (const auto &matrixId : previousMembers)
    {
        if (currentMembers.contains(matrixId))
            continue;

        const auto contact = m_contactManager->byId(account(), matrixId, ActionCreateAndAdd);
        emit peerStateChanged(contact, ChatState::Paused);
    }
    for (const auto &matrixId : currentMembers)
    {
        if (previousMembers.contains(matrixId))
            continue;

        const auto contact = m_contactManager->byId(account(), matrixId, ActionCreateAndAdd);
        emit peerStateChanged(contact, ChatState::Composing);
    }

    if (currentMembers.isEmpty())
        m_typingMembers.remove(room);
    else
        m_typingMembers.insert(room, currentMembers);
}
