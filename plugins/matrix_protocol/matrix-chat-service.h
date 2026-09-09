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

#include <Quotient/events/eventrelation.h>

#include <QtCore/QPointer>
#include <QtCore/QPair>
#include <QtCore/QSet>
#include <QtCore/QVector>

#include <optional>

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
    virtual bool setChatNotificationMode(const Chat &chat, ChatNotificationMode mode) override;
    virtual bool setChatPriority(const Chat &chat, ChatPriority priority) override;
    virtual bool markChatRead(const Chat &chat) override;

    void setConnection(Quotient::Connection *connection);
    void setContactAvatarService(MatrixContactAvatarService *contactAvatarService);

public slots:
    virtual bool sendMessage(const Message &message) override;
    virtual bool sendReply(const Message &message, const QString &targetEventId) override;
    virtual bool editMessage(const Message &message, const QString &targetEventId) override;
    virtual bool sendRawMessage(const Chat &chat, const QByteArray &rawMessage) override;
    virtual bool sendAttachment(const Chat &chat, const QString &filePath, const QString &description) override;
    virtual bool sendLocation(const Chat &chat, const QString &geoUri) override;
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
    bool m_notificationRulesLoaded = false;
    bool m_notificationRulesLoading = false;

    QString roomId(const Chat &chat) const;
    bool sendText(const Chat &chat, const QString &text, Message message = {},
                  const std::optional<Quotient::EventRelation> &relation = std::nullopt);
    bool sendMessageWithRelation(const Message &message, const std::optional<Quotient::EventRelation> &relation);
    bool sendAttachmentToRoom(const Chat &chat, const QString &filePath, const QString &description);
    bool sendLocationToRoom(const Chat &chat, const QString &geoUri);
    void postText(Quotient::Room *room, const QString &text, const QString &transactionId,
                  const std::optional<Quotient::EventRelation> &relation = std::nullopt);
    void dumpTimeline(Quotient::Room *room);
    void postAttachment(Quotient::Room *room, const QString &filePath, const QString &description);
    void postLocation(Quotient::Room *room, const QString &geoUri);
    bool isSupportedRoom(const Quotient::Room *room) const;
    QString directPeerId(const Quotient::Room *room) const;
    Chat roomChat(Quotient::Room *room) const;
    void synchronizeRoom(Quotient::Room *room);
    void synchronizeRoomDetails(Quotient::Room *room);
    void synchronizeRoomMembers(Quotient::Room *room);
    void synchronizeRoomPriority(Quotient::Room *room);
    void synchronizeRoomUnreadCount(Quotient::Room *room);
    void watchRoom(Quotient::Room *room);
    void handleNewMessages(Quotient::Room *room, int fromIndex, int toIndex);
    void handleRoomMessageEvent(Quotient::Room *room, const Quotient::RoomMessageEvent &event,
                                const QString &eventId);
    void refreshNotificationModes();
    void replaceNotificationModeRules(const Chat &chat, ChatNotificationMode mode);
    void deleteNotificationModeRules(
        const Chat &chat, ChatNotificationMode mode, const QVector<QPair<QString, QString>> &rules, int index);
    void createNotificationModeRule(const Chat &chat, ChatNotificationMode mode);
    void failNotificationModeChange(const Chat &chat, const QString &details = {});

private slots:
    INJEQT_SET void setChatManager(ChatManager *chatManager);
    INJEQT_SET void setChatStorage(ChatStorage *chatStorage);
    INJEQT_SET void setContactManager(ContactManager *contactManager);
    INJEQT_SET void setFormattedStringFactory(FormattedStringFactory *formattedStringFactory);
    INJEQT_SET void setMessageStorage(MessageStorage *messageStorage);
};
