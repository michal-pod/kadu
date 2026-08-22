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

#pragma once

#include "protocols/services/chat-state-service.h"

#include <QtCore/QHash>
#include <QtCore/QPointer>
#include <QtCore/QSet>

class ContactManager;

namespace Quotient
{
class Connection;
class Room;
}

class MatrixChatStateService final : public ChatStateService
{
    Q_OBJECT

public:
    explicit MatrixChatStateService(Account account, QObject *parent = nullptr);
    virtual ~MatrixChatStateService() = default;

    void setConnection(Quotient::Connection *connection);
    virtual void sendState(const Contact &contact, ChatState state) override;

private:
    static constexpr auto TypingTimeout = 5000;

    QPointer<ContactManager> m_contactManager;
    QPointer<Quotient::Connection> m_connection;
    QSet<Quotient::Room *> m_watchedRooms;
    QHash<Quotient::Room *, QSet<QString>> m_typingMembers;
    QHash<QString, bool> m_sentTypingStates;

    QString directRoomId(const Contact &contact) const;
    void watchRoom(Quotient::Room *room);
    void synchronizeTypingMembers(Quotient::Room *room);

private slots:
    INJEQT_SET void setContactManager(ContactManager *contactManager);
};
