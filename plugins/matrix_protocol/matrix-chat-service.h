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

#include "message/message.h"
#include "protocols/services/chat-service.h"

#include <QtCore/QPointer>
#include <QtCore/QSet>

class ChatManager;
class ChatStorage;
class ContactManager;
class FormattedStringFactory;
class MessageStorage;
class MatrixContactAvatarService;

namespace Quotient
{
class Connection;
class Room;
class RoomMessageEvent;
}

class MatrixChatService final : public ChatService
{
    Q_OBJECT

public:
    explicit MatrixChatService(Account account, QObject *parent = nullptr);
    virtual ~MatrixChatService() = default;

    virtual int maxMessageLength() const override;

    void setConnection(Quotient::Connection *connection);
    void setContactAvatarService(MatrixContactAvatarService *contactAvatarService);

public slots:
    virtual bool sendMessage(const Message &message) override;
    virtual bool sendRawMessage(const Chat &chat, const QByteArray &rawMessage) override;
    virtual void leaveChat(const Chat &chat) override;

private:
    QPointer<ChatManager> m_chatManager;
    QPointer<ChatStorage> m_chatStorage;
    QPointer<ContactManager> m_contactManager;
    QPointer<FormattedStringFactory> m_formattedStringFactory;
    QPointer<MessageStorage> m_messageStorage;
    QPointer<MatrixContactAvatarService> m_contactAvatarService;
    QPointer<Quotient::Connection> m_connection;
    QSet<Quotient::Room *> m_watchedRooms;
    QSet<Quotient::Room *> m_loadedRooms;
    QSet<QString> m_historicalEventIds;
    QSet<QString> m_localTransactionIds;
    bool m_initialSyncFinished = false;

    QString directChatId(const Chat &chat) const;
    QString roomId(const Chat &chat) const;
    bool sendText(const Chat &chat, const QString &text, Message message = {});
    void postText(Quotient::Room *room, const QString &text, const QString &transactionId);
    bool isSupportedRoom(const Quotient::Room *room) const;
    Chat roomChat(Quotient::Room *room) const;
    void synchronizeRoom(Quotient::Room *room);
    void synchronizeRoomDetails(Quotient::Room *room);
    void synchronizeRoomMembers(Quotient::Room *room);
    void watchRoom(Quotient::Room *room);
    void handleNewMessages(Quotient::Room *room, int fromIndex, int toIndex);
    void handleDirectMessageEvent(const Quotient::RoomMessageEvent &event, const QString &eventId);
    void handleRoomMessageEvent(Quotient::Room *room, const Quotient::RoomMessageEvent &event,
                                const QString &eventId);

private slots:
    INJEQT_SET void setChatManager(ChatManager *chatManager);
    INJEQT_SET void setChatStorage(ChatStorage *chatStorage);
    INJEQT_SET void setContactManager(ContactManager *contactManager);
    INJEQT_SET void setFormattedStringFactory(FormattedStringFactory *formattedStringFactory);
    INJEQT_SET void setMessageStorage(MessageStorage *messageStorage);
};
