/*
 * %kadu copyright begin%
 * Copyright 2011 Piotr Galiszewski (piotr.galiszewski@kadu.im)
 * Copyright 2011 Piotr Dąbrowski (ultr@ultr.pl)
 * Copyright 2011 Bartosz Brachaczek (b.brachaczek@gmail.com)
 * Copyright 2011, 2012, 2013, 2014 Rafał Przemysław Malinowski (rafal.przemyslaw.malinowski@gmail.com)
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

#ifndef CHAT_STATE_SERVICE_H
#define CHAT_STATE_SERVICE_H

#include "protocols/services/account-service.h"

#include "exports.h"

#include <QtCore/QString>
#include <QtCore/QVector>

class Chat;
class Contact;

enum class ChatState;

struct KADUAPI ChatStatePeer
{
    QString id;
    QString displayName;
    ChatState state;
};

/**
 * @addtogroup Protocol
 * @{
 */

/**
 * @class ChatStateService
 * @short ChatStateService account service allows sending and receiving information about composing state in chats.
 *
 * This service allows sending and receiving information about composing state in chats. It supports several states
 * defined in ChatStateService::State enum.
 *
 * Subclasses implement contact-based state sending and may override its chat-aware variant when the conversation
 * cannot be identified by a single contact. State changes can likewise be reported with or without chat context.
 */
class KADUAPI ChatStateService : public AccountService
{
    Q_OBJECT

public:
    /**
     * @short Create new instance of ChatStateService bound to given Account.
     * @param account account to bound this service to
     */
    explicit ChatStateService(Account account, QObject *parent = nullptr);
    virtual ~ChatStateService();

    /**
     * @short Send our state to given contact.
     * @param contact state of chat with this contact changed
     * @param state new state to send
     */
    virtual void sendState(const Contact &contact, ChatState state) = 0;

    /**
     * @short Send our state in the context of a particular chat.
     *
     * The default implementation forwards one-contact chats to sendState(Contact, ChatState).
     * Protocols whose chat identity is independent of its participants can override this method.
     */
    virtual void sendState(const Chat &chat, ChatState state);

    /**
     * @short Return peer states that are already active when a chat view is opened.
     */
    virtual QVector<ChatStatePeer> activePeerStates(const Chat &chat) const;

signals:
    /**
     * @short Signal emited when peer changed its chat state.
     * @param contact peer contact
     * @param state new state received from peer
     */
    void peerStateChanged(const Contact &contact, ChatState state);

    /**
     * @short Signal emitted when a peer changed state in a particular chat.
     */
    void peerStateChangedInChat(
        const Chat &chat, const QString &peerId, const QString &displayName, ChatState state);
};

/**
 * @}
 */

#endif   // CHAT_STATE_SERVICE_H
