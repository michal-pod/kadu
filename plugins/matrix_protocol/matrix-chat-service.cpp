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

#include "matrix-chat-service.h"
#include "matrix-chat-service.moc"

#include "chat/chat.h"
#include "chat/chat-details-room.h"
#include "chat/chat-manager.h"
#include "chat/chat-storage.h"
#include "chat/type/chat-type-contact.h"
#include "chat/type/chat-type-room.h"
#include "contacts/contact-manager.h"
#include "contacts/contact-set.h"
#include "formatted-string/formatted-string-factory.h"
#include "formatted-string/formatted-string-plain-text-visitor.h"
#include "html/html-conversion.h"
#include "html/html-string.h"
#include "message/message-storage.h"
#include "services/raw-message-transformer-service.h"

#include <Quotient/connection.h>
#include <Quotient/events/roommessageevent.h>
#include <Quotient/room.h>

#include <QtCore/QDateTime>

MatrixChatService::MatrixChatService(Account account, QObject *parent) : ChatService{account, parent}
{
}

int MatrixChatService::maxMessageLength() const
{
    return -1;
}

void MatrixChatService::setConnection(Quotient::Connection *connection)
{
    if (m_connection == connection)
        return;

    if (m_connection)
        disconnect(m_connection, nullptr, this, nullptr);

    m_connection = connection;
    m_watchedRooms.clear();
    m_initialSyncFinished = false;

    if (!m_connection)
        return;

    connect(m_connection, &Quotient::Connection::newRoom, this, &MatrixChatService::watchRoom);
    connect(m_connection, &Quotient::Connection::joinedRoom, this,
            [this](Quotient::Room *room, Quotient::Room *) { watchRoom(room); });
    connect(m_connection, &Quotient::Connection::syncDone, this, [this] {
        m_initialSyncFinished = true;
        for (auto *room : m_connection->allRooms())
            synchronizeRoom(room);
    });

    for (auto *room : m_connection->allRooms())
        watchRoom(room);
}

void MatrixChatService::setChatManager(ChatManager *chatManager)
{
    m_chatManager = chatManager;
}

void MatrixChatService::setChatStorage(ChatStorage *chatStorage)
{
    m_chatStorage = chatStorage;
}

void MatrixChatService::setContactManager(ContactManager *contactManager)
{
    m_contactManager = contactManager;
}

void MatrixChatService::setFormattedStringFactory(FormattedStringFactory *formattedStringFactory)
{
    m_formattedStringFactory = formattedStringFactory;
}

void MatrixChatService::setMessageStorage(MessageStorage *messageStorage)
{
    m_messageStorage = messageStorage;
}

QString MatrixChatService::directChatId(const Chat &chat) const
{
    const auto contacts = chat.contacts().toContactVector();
    return contacts.size() == 1 ? contacts.constFirst().id() : QString{};
}

QString MatrixChatService::roomId(const Chat &chat) const
{
    const auto *details = qobject_cast<ChatDetailsRoom *>(chat.details());
    return details ? details->room() : QString{};
}

bool MatrixChatService::sendText(const Chat &chat, const QString &text, Message message)
{
    if (!m_connection || !m_connection->isLoggedIn())
        return false;

    if (const auto id = roomId(chat); !id.isEmpty())
    {
        auto *room = m_connection->room(id, Quotient::JoinState::Join);
        if (!isSupportedRoom(room))
            return false;

        const auto transactionId = room->postText(text);
        if (!message.isNull())
            message.setId(transactionId);
        return true;
    }

    const auto recipientId = directChatId(chat);
    if (recipientId.isEmpty())
        return false;

    m_connection->getDirectChat(recipientId).then(
        this, [text, message = std::move(message)](Quotient::Room *room) mutable {
            if (!room)
                return;

            const auto transactionId = room->postText(text);
            if (!message.isNull())
                message.setId(transactionId);
        });
    return true;
}

bool MatrixChatService::sendMessage(const Message &message)
{
    if (!m_formattedStringFactory)
        return false;

    auto formattedContent = m_formattedStringFactory->fromHtml(message.content());
    FormattedStringPlainTextVisitor plainTextVisitor;
    formattedContent->accept(&plainTextVisitor);

    auto plainText = plainTextVisitor.result();
    if (rawMessageTransformerService())
        plainText = QString::fromUtf8(
            rawMessageTransformerService()->transform(plainText.toUtf8(), message).rawContent());

    return sendText(message.messageChat(), plainText, message);
}

bool MatrixChatService::sendRawMessage(const Chat &chat, const QByteArray &rawMessage)
{
    return sendText(chat, QString::fromUtf8(rawMessage));
}

void MatrixChatService::leaveChat(const Chat &chat)
{
    if (const auto id = roomId(chat); !id.isEmpty())
    {
        if (auto *room = m_connection ? m_connection->room(id, Quotient::JoinState::Join) : nullptr)
            room->leaveRoom();
        return;
    }

    chat.setIgnoreAllMessages(true);
}

bool MatrixChatService::isSupportedRoom(const Quotient::Room *room) const
{
    return room && room->joinState() == Quotient::JoinState::Join &&
           (!m_connection || !m_connection->isDirectChat(room->id()));
}

Chat MatrixChatService::roomChat(Quotient::Room *room) const
{
    if (!m_chatManager || !m_chatStorage || !isSupportedRoom(room))
        return Chat::null;

    auto chat = ChatTypeRoom::findChat(m_chatManager, m_chatStorage, account(), room->id(), ActionCreateAndAdd);
    if (!chat)
        return Chat::null;

    const auto displayName = room->displayName();
    chat.setDisplay(displayName.isEmpty() ? room->id() : displayName);
    if (auto *details = qobject_cast<ChatDetailsRoom *>(chat.details()))
        details->setConnected(true);
    return chat;
}

void MatrixChatService::synchronizeRoom(Quotient::Room *room)
{
    if (!m_initialSyncFinished)
        return;

    roomChat(room);
}

void MatrixChatService::watchRoom(Quotient::Room *room)
{
    if (!room || m_watchedRooms.contains(room))
        return;

    m_watchedRooms.insert(room);
    connect(room, &Quotient::Room::addedMessages, this,
            [this, room](int fromIndex, int toIndex) { handleNewMessages(room, fromIndex, toIndex); });
    connect(room, &Quotient::Room::baseStateLoaded, this, [this, room] { synchronizeRoom(room); });
    connect(room, &Quotient::Room::displaynameChanged, this,
            [this, room](Quotient::Room *, const QString &) { synchronizeRoom(room); });
    connect(room, &Quotient::Room::encryption, this, [this, room] { synchronizeRoom(room); });
    connect(room, &QObject::destroyed, this, [this, room] { m_watchedRooms.remove(room); });
}

void MatrixChatService::handleNewMessages(Quotient::Room *room, int fromIndex, int toIndex)
{
    if (!m_initialSyncFinished || !m_connection)
        return;

    const auto directChat = m_connection->isDirectChat(room->id());
    if (!directChat && !isSupportedRoom(room))
        return;

    for (const auto &item : room->messageEvents())
    {
        if (item.index() < fromIndex || item.index() > toIndex)
            continue;

        const auto *event = item.viewAs<Quotient::RoomMessageEvent>();
        if (!event || event->isRedacted() || event->senderId() == m_connection->userId() ||
            event->msgtype() != Quotient::RoomMessageEvent::MsgType::Text)
            continue;

        if (directChat)
            handleDirectMessageEvent(*event);
        else
            handleRoomMessageEvent(room, *event);
    }
}

void MatrixChatService::handleDirectMessageEvent(const Quotient::RoomMessageEvent &event)
{
    if (!m_chatManager || !m_chatStorage || !m_contactManager || !m_messageStorage)
        return;

    const auto contact = m_contactManager->byId(account(), event.senderId(), ActionCreateAndAdd);
    const auto chat = ChatTypeContact::findChat(m_chatManager, m_chatStorage, contact, ActionCreateAndAdd);
    if (!chat || chat.isIgnoreAllMessages())
        return;

    auto message = m_messageStorage->create();
    message.setMessageChat(chat);
    message.setMessageSender(contact);
    message.setType(MessageTypeReceived);
    message.setSendDate(event.originTimestamp().toLocalTime());
    message.setReceiveDate(QDateTime::currentDateTime());

    auto text = event.plainBody();
    if (rawMessageTransformerService())
        text = QString::fromUtf8(rawMessageTransformerService()->transform(text.toUtf8(), message).rawContent());
    message.setContent(normalizeHtml(plainToHtml(text)));

    emit messageReceived(message);
}

void MatrixChatService::handleRoomMessageEvent(Quotient::Room *room, const Quotient::RoomMessageEvent &event)
{
    if (!m_contactManager || !m_messageStorage)
        return;

    const auto chat = roomChat(room);
    if (!chat || chat.isIgnoreAllMessages())
        return;

    const auto contact = m_contactManager->byId(account(), event.senderId(), ActionCreateAndAdd);
    if (auto *details = qobject_cast<ChatDetailsRoom *>(chat.details()))
        details->addContact(contact);

    auto message = m_messageStorage->create();
    message.setMessageChat(chat);
    message.setMessageSender(contact);
    message.setType(MessageTypeReceived);
    message.setSendDate(event.originTimestamp().toLocalTime());
    message.setReceiveDate(QDateTime::currentDateTime());

    auto text = event.plainBody();
    if (rawMessageTransformerService())
        text = QString::fromUtf8(rawMessageTransformerService()->transform(text.toUtf8(), message).rawContent());
    message.setContent(normalizeHtml(plainToHtml(text)));

    emit messageReceived(message);
}
