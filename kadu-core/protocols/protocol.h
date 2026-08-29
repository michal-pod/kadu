/*
 * %kadu copyright begin%
 * Copyright 2009, 2010, 2011 Piotr Galiszewski (piotr.galiszewski@kadu.im)
 * Copyright 2009, 2010 Wojciech Treter (juzefwt@gmail.com)
 * Copyright 2008, 2009 Michał Podsiadlik (michal@kadu.net)
 * Copyright 2009 Bartłomiej Zimoń (uzi18@o2.pl)
 * Copyright 2010, 2011, 2012, 2014 Bartosz Brachaczek (b.brachaczek@gmail.com)
 * Copyright 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015 Rafał Przemysław Malinowski (rafal.przemyslaw.malinowski@gmail.com)
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

#include "accounts/account.h"
#include "chat/chat.h"
#include "exports.h"
#include "status/status-change-source.h"
#include "status/status.h"

#include <QtCore/QDateTime>
#include <QtCore/QFlags>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <injeqt/injeqt.h>

typedef quint32 UinType;

enum class RemoteHistorySearchCapability : quint32
{
    None = 0x0,
    Contacts = 0x1,
    Chats = 0x2,
    Messages = 0x4
};
Q_DECLARE_FLAGS(RemoteHistorySearchCapabilities, RemoteHistorySearchCapability)
Q_DECLARE_OPERATORS_FOR_FLAGS(RemoteHistorySearchCapabilities)

class QPixmap;

class AccountShared;
class BuddyListSerializationService;
class ChatImageService;
class ContactManager;
class ContactPersonalInfoService;
class ContactSet;
class FileTransferService;
class Message;
class MultilogonService;
class PersonalInfoService;
class PluginInjectedFactory;
class ProtocolHistoryService;
class ProtocolTimelineService;
class ProtocolFactory;
class ProtocolStateMachine;
class RosterService;
class SearchService;
class SessionService;
class StatusTypeManager;
class Status;
class SubscriptionService;
class KaduIcon;

class KADUAPI Protocol : public QObject
{
    Q_OBJECT

public:
    Protocol(Account account, ProtocolFactory *factory);
    virtual ~Protocol();

    ProtocolFactory *protocolFactory() const
    {
        return Factory;
    }
    Account account() const
    {
        return CurrentAccount;
    }

    virtual BuddyListSerializationService *buddyListSerializationService()
    {
        return nullptr;
    }
    virtual ChatImageService *chatImageService()
    {
        return 0;
    }
    /**
     * @short Return whether chat attachments can be sent through this protocol.
     *
     * Attachments are independent Matrix-style file events. They are not the legacy inline images exposed by
     * ChatImageService.
     */
    virtual bool isAttachmentsSupported() const
    {
        return false;
    }
    /**
     * @short Return the hard attachment upload limit in bytes.
     *
     * A non-positive value means that the protocol does not currently know a limit. The editor uses this value only
     * after isAttachmentsSupported() returned true.
     */
    virtual qint64 maximumAttachmentSize() const
    {
        return 0;
    }
    /**
     * @short Return whether this protocol can send a geographical location.
     *
     * Locations are passed to chat services as RFC 5870 geo: URIs. The
     * protocol decides how that interoperable value is represented on wire.
     */
    virtual bool isLocationSendingSupported() const
    {
        return false;
    }
    virtual ContactPersonalInfoService *contactPersonalInfoService()
    {
        return 0;
    }
    virtual FileTransferService *fileTransferService()
    {
        return 0;
    }
    virtual MultilogonService *multilogonService()
    {
        return 0;
    }
    virtual PersonalInfoService *personalInfoService()
    {
        return 0;
    }
    virtual RosterService *rosterService() const;
    virtual SearchService *searchService()
    {
        return 0;
    }
    virtual SubscriptionService *subscriptionService()
    {
        return 0;
    }
    virtual ProtocolHistoryService *historyService()
    {
        return nullptr;
    }
    virtual ProtocolTimelineService *timelineService()
    {
        return nullptr;
    }

    /**
     * @short Return whether this protocol supports local history.
     *
     * Local history belongs to the protocol. The default preserves the current behavior, where local history is
     * available for every protocol.
     */
    virtual bool isLocalHistorySupported() const
    {
        return true;
    }

    /**
     * @short Return whether this protocol provides history from its server.
     *
     * A protocol returning true must provide a protocol-specific history provider when the history layer requests
     * one. Until such a provider is selected, the local archive remains the only available source.
     */
    virtual bool isRemoteHistorySupported() const
    {
        return false;
    }

    /**
     * @short Return kinds of data that can be searched remotely.
     *
     * The empty default means that the protocol offers no remote search. The flags describe result kinds, not local
     * cache capabilities; a protocol may expose remote contact, chat, and message search independently.
     */
    virtual RemoteHistorySearchCapabilities isRemoteSearchSupported() const
    {
        return {};
    }

    virtual bool contactsListReadOnly() = 0;
    virtual bool supportsPrivateStatus()
    {
        return false;
    }

    bool isConnected() const;
    bool isConnecting() const;
    bool isDisconnecting() const;

    // method called by user
    void setStatus(Status status, StatusChangeSource source);
    Status status() const;
    /**
     * @short How long to wait before trying to log in again, in milliseconds.
     *
     * Asked after a login has failed and before the next one is begun. How long is worth waiting
     * depends on what has already been tried and how often, which is something only the protocol
     * knows; the default is the half second this used to be for everyone whatever had happened.
     */
    virtual int reconnectDelay() const
    {
        return 500;
    }

    virtual int maxDescriptionLength()
    {
        return -1;
    }

    virtual void changePrivateMode(){};

    virtual QString statusPixmapPath() = 0;

    KaduIcon statusIcon();
    KaduIcon statusIcon(const Status &status);

    KaduIcon icon();

    // TODO: workaround
    void emitContactStatusChanged(Contact contact, Status oldStatus)
    {
        emit contactStatusChanged(contact, oldStatus);
    }

public slots:
    void passwordProvided();

signals:
    void connecting(Account account);
    void connected(Account account);
    void disconnected(Account account);

    void statusChanged(Account account, Status newStatus);
    void remoteStatusChangeRequest(Account account, Status requestedStatus);
    void contactStatusChanged(Contact contact, Status oldStatus);

    // TODO: REVIEW
    void connectionError(Account account, const QString &server, const QString &reason);
    void invalidPassword(Account account);

    // state machine signals
    void stateMachineLoggedIn();
    void stateMachineLoggedOut();

    void stateMachineChangeStatus();
    void stateMachineLogout();

    void stateMachinePasswordRequired();
    void stateMachinePasswordAvailable();
    void stateMachinePasswordNotAvailable();

    void stateMachineConnectionError();
    void stateMachineConnectionClosed();

    void stateMachineSslError();
    void stateMachineSslErrorResolved();
    void stateMachineSslErrorNotResolved();

protected:
    ContactManager *contactManager() const;
    PluginInjectedFactory *pluginInjectedFactory() const;
    StatusTypeManager *statusTypeManager() const;

    Status loginStatus() const;

    virtual void login() = 0;
    virtual void afterLoggedIn()
    {
    }
    virtual void logout() = 0;
    virtual void sendStatusToServer() = 0;

    virtual void disconnectedCleanup();
    void statusChanged(Status newStatus);

    void doSetStatus(Status status);

    // services
    void setRosterService(RosterService *const rosterService);

protected slots:
    void loggedIn();
    void loggedOut();
    void passwordRequired();
    void connectionError();
    void connectionClosed();
    void sslError();
    void reconnect();

private:
    QPointer<ContactManager> m_contactManager;
    QPointer<PluginInjectedFactory> m_pluginInjectedFactory;
    QPointer<RosterService> m_rosterService;
    QPointer<SessionService> m_sessionService;
    QPointer<StatusTypeManager> m_statusTypeManager;

    ProtocolFactory *Factory;
    ProtocolStateMachine *Machine;

    Account CurrentAccount;

    // real status, can be offline after connection error
    Status CurrentStatus;
    // status used by user to login, after connection error its value does not change
    // it can only by changed by user or status changer
    Status LoginStatus;

    void setAllOffline();

private slots:
    INJEQT_SET void setContactManager(ContactManager *contactManager);
    INJEQT_SET void setPluginInjectedFactory(PluginInjectedFactory *pluginInjectedFactory);
    INJEQT_SET void setSessionService(SessionService *sessionService);
    INJEQT_SET void setStatusTypeManager(StatusTypeManager *statusTypeManager);
    INJEQT_INIT void init();

    // state machine slots
    void prepareStateMachine();

    void loggingInStateEntered();
    void loggedInStateEntered();
    void loggingOutStateEntered();
    void loggedOutAnyStateEntered();
    void wantToLogInStateEntered();
    void passwordRequiredStateEntered();
};
