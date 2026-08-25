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

#include "protocols/services/protocol-history-service.h"

#include <QtCore/QPointer>
#include <QtCore/QPromise>
#include <QtCore/QSet>
#include <injeqt/injeqt.h>

#include <memory>

class ContactManager;
class MessageStorage;

namespace Quotient
{
class Connection;
class Room;
class RoomMessageEvent;
}

class MatrixHistoryService final : public ProtocolHistoryService
{
    Q_OBJECT

public:
    explicit MatrixHistoryService(Account account, QObject *parent = nullptr);
    virtual ~MatrixHistoryService() = default;

    void setConnection(Quotient::Connection *connection);

    virtual bool isLocalHistoryEnabled() const override;
    virtual void storeMessage(const Message &message) override;
    virtual QFuture<ProtocolHistoryPage> requestHistory(const ProtocolHistoryRequest &request) override;

private:
    QPointer<ContactManager> m_contactManager;
    QPointer<MessageStorage> m_messageStorage;
    QPointer<Quotient::Connection> m_connection;
    QSet<Quotient::Room *> m_watchedRooms;
    QSet<Quotient::Room *> m_loadedRooms;

    Quotient::Room *roomForChat(const Chat &chat) const;
    void watchRoom(Quotient::Room *room);
    QFuture<ProtocolHistoryPage> waitForRoomInitialState(const ProtocolHistoryRequest &request,
                                                          Quotient::Room *room);
    QFuture<ProtocolHistoryPage> requestHistoryForRoom(const ProtocolHistoryRequest &request,
                                                        Quotient::Room *room);
    ProtocolHistoryPage pageForRoom(const ProtocolHistoryRequest &request, Quotient::Room *room) const;
    Message messageForEvent(const Chat &chat, const Quotient::RoomMessageEvent &event) const;
    QFuture<ProtocolHistoryPage> completedPage(ProtocolHistoryPage page) const;
    void finishRequest(const std::shared_ptr<QPromise<ProtocolHistoryPage>> &promise,
                       ProtocolHistoryPage page) const;

private slots:
    INJEQT_SET void setContactManager(ContactManager *contactManager);
    INJEQT_SET void setMessageStorage(MessageStorage *messageStorage);
};
