/*
 * %kadu copyright begin%
 * Copyright 2009, 2011 Piotr Galiszewski (piotr.galiszewski@kadu.im)
 * Copyright 2009 Wojciech Treter (juzefwt@gmail.com)
 * Copyright 2011 Piotr Dąbrowski (ultr@ultr.pl)
 * Copyright 2009 Michał Podsiadlik (michal@kadu.net)
 * Copyright 2011, 2014 Bartosz Brachaczek (b.brachaczek@gmail.com)
 * Copyright 2009, 2010, 2011, 2012, 2013, 2014 Rafał Przemysław Malinowski (rafal.przemyslaw.malinowski@gmail.com)
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

#include "buddies/buddy-list.h"
#include "chat/chat.h"
#include "exports.h"

#include "protocols/services/account-service.h"

#include <QtCore/QPointer>
#include <QtCore/QString>
#include <injeqt/injeqt.h>

class FormattedString;
class Message;
class RawMessageTransformerService;

/**
 * @addtogroup Protocol
 * @{
 */

/**
 * @class ChatService
 * @short Chat service allows sending and receiving messages.
 *
 * This service allows sending and receiving messages. Each message can be modified before sending or after
 * receiving to allow protocol-independent encryption and filtering.
 *
 * One method must be reimplemented by derivered sevices: sendMessage().
 */
class KADUAPI ChatService : public AccountService
{
    Q_OBJECT

protected:
    explicit ChatService(Account account, QObject *parent = nullptr);
    virtual ~ChatService();

public:
    /**
     * @short Return raw message transformer service of this service.
     * @return raw message transformer service of this service
     */
    RawMessageTransformerService *rawMessageTransformerService() const;

    /**
     * @short Return max message length for this implementation.
     * @return max message length for this implementation
     */
    virtual int maxMessageLength() const = 0;

public slots:
    /**
     * @short Send new message to given chat.
     * @param message message to be sent
     *
     * This methods sends a message. Service is allowed to ignore this requst and to ignore any formatting
     * that is present in message.
     *
     * Message can be altered by RawMessageTransformerService to allow any encryption on any protocol.
     */
    virtual bool sendMessage(const Message &message) = 0;

    /**
     * @short Send a reply to a native protocol event.
     *
     * The default keeps timeline relations opt-in for individual protocols.
     */
    virtual bool sendReply(const Message &message, const QString &targetEventId)
    {
        Q_UNUSED(message)
        Q_UNUSED(targetEventId)
        return false;
    }

    /**
     * @short Send a replacement for a native protocol event.
     */
    virtual bool editMessage(const Message &message, const QString &targetEventId)
    {
        Q_UNUSED(message)
        Q_UNUSED(targetEventId)
        return false;
    }

    /**
     * @short Send raw message to given chat.
     * @param chat chat for the message
     * @param message message to be sent
     * @param transform is message should be transformed by RawMessageTransformerService
     *
     * This methods sends a message. Service is allowed to ignore this requst.
     *
     * This message won't be altered by RawMessageTransformerService.
     */
    virtual bool sendRawMessage(const Chat &chat, const QByteArray &rawMessage) = 0;

    /**
     * @short Send a file attachment with a plain-text description.
     *
     * The default keeps attachments opt-in for individual protocols. Unlike sendMessage(), this does not create a
     * legacy Message because the protocol owns both the upload and the resulting timeline event.
     */
    virtual bool sendAttachment(const Chat &chat, const QString &filePath, const QString &description)
    {
        Q_UNUSED(chat)
        Q_UNUSED(filePath)
        Q_UNUSED(description)
        return false;
    }

    /**
     * @short Send a geographical location represented by an RFC 5870 geo: URI.
     *
     * Location sending is opt-in, just like attachment sending. A protocol that
     * does not expose the capability rejects the request without creating a
     * legacy Message.
     */
    virtual bool sendLocation(const Chat &chat, const QString &geoUri)
    {
        Q_UNUSED(chat)
        Q_UNUSED(geoUri)
        return false;
    }

    /**
     * @short Leave @p chat.
     *
     * Use to leave chats in GG and room chats in XMPP. In current implemntations does nothing for single
     * contact chats in XMPP. May change in future.
     */
    virtual void leaveChat(const Chat &chat) = 0;

signals:
    /**
     * @short Signal emitted when sent message status has changed.
     * @param message message with changed status
     *
     * This signal is emitted every time a protocol learns about delivery status of sent message.
     */
    void sentMessageStatusChanged(const Message &message);

    /**
     * @short Signal emitted when message was sent.
     * @param message sent message
     *
     * This signal is emited every time a message is sent trought one of registered acocunts.
     */
    void messageSent(const Message &message);

    /**
     * @short Signal emitted when message is received.
     * @param message received message
     *
     * This signal is emitted every message is received and not ignored.
     */
    void messageReceived(const Message &message);

private:
    QPointer<RawMessageTransformerService> m_rawMessageTransformerService;

private slots:
    /**
     * @short Set raw message transformer service for this service.
     * @param rawMessageTransformerService raw message transformer service for this service
     */
    INJEQT_SET void setRawMessageTransformerService(RawMessageTransformerService *rawMessageTransformerService);
};

/**
 * @}
 */
