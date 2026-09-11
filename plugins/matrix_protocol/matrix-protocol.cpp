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

#include "matrix-protocol.h"
#include "matrix-protocol.moc"

#include "chat/chat-service-repository.h"
#include "chat/chat-state-service-repository.h"
#include "plugin/plugin-injected-factory.h"

#include "avatars/aggregated-account-avatar-service.h"
#include "avatars/aggregated-contact-avatar-service.h"
#include "buddies/buddy-manager.h"
#include "chat/chat-details-room.h"
#include "chat/chat-manager.h"
#include "contacts/contact-manager.h"
#include "icons/icons-manager.h"

#include "matrix-account-data.h"
#include "matrix-chat-service.h"
#include "matrix-chat-state-service.h"
#include "matrix-account-avatar-service.h"
#include "matrix-contact-avatar-service.h"
#include "matrix-device-verification-notification-service.h"
#include "matrix-history-service.h"
#include "matrix-room-members-model.h"
#include "matrix-session-service.h"
#include "matrix-timeline-service.h"
#include "matrix-room-invitation-notification-service.h"
#include "gui/matrix-device-verification-dialog.h"
#include "gui/matrix-restore-recovery-key-dialog.h"
#include "gui/matrix-room-settings-window.h"

#include <Quotient/connection.h>
#include <Quotient/csapi/authed-content-repo.h>
#include <Quotient/database.h>
#include <Quotient/events/encryptedevent.h>
#include <Quotient/events/keyverificationevent.h>
#include <Quotient/events/roommessageevent.h>
#include <Quotient/keyverificationsession.h>
#include <Quotient/room.h>
#include <Quotient/user.h>

#include <qt6keychain/keychain.h>

#include <QtCore/QByteArray>
#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtWidgets/QMessageBox>

MatrixProtocol::MatrixProtocol(Account account, ProtocolFactory *factory) : Protocol{account, factory}
{
    account.setRememberPassword(false);
    account.setHasPassword(true);
    m_sessionService = new MatrixSessionService{account, this};
    connect(m_sessionService, &MatrixSessionService::sessionVerificationRequested, this, &MatrixProtocol::verifyDevice);
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this,
            [this] { m_applicationQuitting = true; });
}

MatrixProtocol::~MatrixProtocol()
{
    if (m_aggregatedAccountAvatarService && m_accountAvatarService)
        m_aggregatedAccountAvatarService->remove(m_accountAvatarService);
    if (m_aggregatedContactAvatarService && m_contactAvatarService)
        m_aggregatedContactAvatarService->remove(m_contactAvatarService);
    if (m_chatStateServiceRepository && m_chatStateService)
        m_chatStateServiceRepository->removeChatStateService(m_chatStateService);
    if (m_chatServiceRepository && m_chatService)
        m_chatServiceRepository->removeChatService(m_chatService);
}

void MatrixProtocol::setBuddyManager(BuddyManager *buddyManager)
{
    m_buddyManager = buddyManager;
}

void MatrixProtocol::setChatManager(ChatManager *chatManager)
{
    m_chatManager = chatManager;
}

void MatrixProtocol::setContactManager(ContactManager *contactManager)
{
    m_contactManager = contactManager;
}

void MatrixProtocol::setIconsManager(IconsManager *iconsManager)
{
    m_iconsManager = iconsManager;
}

void MatrixProtocol::setChatServiceRepository(ChatServiceRepository *chatServiceRepository)
{
    m_chatServiceRepository = chatServiceRepository;
}

void MatrixProtocol::setChatStateServiceRepository(ChatStateServiceRepository *chatStateServiceRepository)
{
    m_chatStateServiceRepository = chatStateServiceRepository;
}

void MatrixProtocol::setAggregatedAccountAvatarService(
    AggregatedAccountAvatarService *aggregatedAccountAvatarService)
{
    m_aggregatedAccountAvatarService = aggregatedAccountAvatarService;
}

void MatrixProtocol::setAggregatedContactAvatarService(
    AggregatedContactAvatarService *aggregatedContactAvatarService)
{
    m_aggregatedContactAvatarService = aggregatedContactAvatarService;
}

void MatrixProtocol::setPluginInjectedFactory(PluginInjectedFactory *pluginInjectedFactory)
{
    m_pluginInjectedFactory = pluginInjectedFactory;
}

void MatrixProtocol::setRoomInvitationNotificationService(
    MatrixRoomInvitationNotificationService *roomInvitationNotificationService)
{
    m_roomInvitationNotificationService = roomInvitationNotificationService;
}

void MatrixProtocol::setDeviceVerificationNotificationService(
    MatrixDeviceVerificationNotificationService *deviceVerificationNotificationService)
{
    m_deviceVerificationNotificationService = deviceVerificationNotificationService;
}

void MatrixProtocol::init()
{
    createConnection();
    m_accountAvatarService = m_pluginInjectedFactory->makeInjected<MatrixAccountAvatarService>(account(), this);
    m_accountAvatarService->setConnection(m_connection);
    m_contactAvatarService = m_pluginInjectedFactory->makeInjected<MatrixContactAvatarService>(account(), this);
    m_contactAvatarService->setConnection(m_connection);
    m_chatService = m_pluginInjectedFactory->makeInjected<MatrixChatService>(account(), this);
    m_chatService->setContactAvatarService(m_contactAvatarService);
    m_chatService->setConnection(m_connection);
    m_chatStateService = m_pluginInjectedFactory->makeInjected<MatrixChatStateService>(account(), this);
    m_chatStateService->setConnection(m_connection);
    m_historyService = m_pluginInjectedFactory->makeInjected<MatrixHistoryService>(account(), this);
    m_historyService->setConnection(m_connection);
    m_timelineService = m_pluginInjectedFactory->makeInjected<MatrixTimelineService>(account(), this);
    m_timelineService->setConnection(m_connection);
    connect(m_timelineService, &MatrixTimelineService::verificationEventDecrypted, this,
            &MatrixProtocol::scheduleVerificationRefresh);
    m_sessionService->setConnection(m_connection);
    m_aggregatedAccountAvatarService->add(m_accountAvatarService);
    m_aggregatedContactAvatarService->add(m_contactAvatarService);
    m_chatServiceRepository->addChatService(m_chatService);
    m_chatStateServiceRepository->addChatStateService(m_chatStateService);
}

ProtocolHistoryService *MatrixProtocol::historyService()
{
    return m_historyService;
}

ProtocolTimelineService *MatrixProtocol::timelineService()
{
    return m_timelineService;
}

MultilogonService *MatrixProtocol::multilogonService()
{
    return m_sessionService;
}

QAbstractItemModel *MatrixProtocol::createChatMembersModel(const Chat &chat, QObject *parent)
{
    if (!m_connection || !m_contactManager)
        return nullptr;

    const auto *details = qobject_cast<ChatDetailsRoom *>(chat.details());
    auto *room = details ? m_connection->room(details->room(), Quotient::JoinState::Join) : nullptr;
    if (!room || m_connection->isDirectChat(room->id()))
        return nullptr;

    return new MatrixRoomMembersModel{account(), room, m_contactManager, parent};
}

QWidget *MatrixProtocol::createChatSettingsWindow(const Chat &chat, QWidget *parent)
{
    const auto *details = qobject_cast<ChatDetailsRoom *>(chat.details());
    auto *room = details && m_connection
                     ? m_connection->room(details->room(), Quotient::JoinState::Join)
                     : nullptr;
    return new MatrixRoomSettingsWindow{chat, m_chatService, m_connection, room, m_iconsManager, parent};
}

void MatrixProtocol::createConnection()
{
    m_inRoomVerificationSessions.clear();
    m_handledInRoomVerificationEvents.clear();
    m_inRoomVerificationRooms.clear();
    m_verificationRefreshScheduled.clear();
    m_deviceVerificationDialogs.clear();
    m_recoveryKeyRestorePrompted = false;
    m_connectionReady = false;
    for (auto *room : m_debugWatchedRooms)
        if (room)
            disconnect(room, nullptr, this, nullptr);
    m_debugWatchedRooms.clear();

    m_connection = new Quotient::Connection{QUrl{MatrixAccountData{account()}.homeserver()}, this};
    m_connection->setLazyLoading(true);
    // Encryption must be enabled before logging in: this makes libQuotient initialise
    // the local Olm account and publish this client's device keys.
    m_connection->enableEncryption(true);
    m_connection->enableDirectChatEncryption(true);
    if (m_chatService)
        m_chatService->setConnection(m_connection);
    if (m_chatStateService)
        m_chatStateService->setConnection(m_connection);
    if (m_accountAvatarService)
        m_accountAvatarService->setConnection(m_connection);
    if (m_contactAvatarService)
        m_contactAvatarService->setConnection(m_connection);
    if (m_historyService)
        m_historyService->setConnection(m_connection);
    if (m_timelineService)
        m_timelineService->setConnection(m_connection);
    if (m_sessionService)
        m_sessionService->setConnection(m_connection);

    connect(m_connection, &Quotient::Connection::connected, this, [this] {
        if (!m_connection)
            return;

        MatrixAccountData{account()}.setDeviceId(m_connection->deviceId());
        m_connectionReady = true;
        m_loginInProgress = false;
        account().setRememberPassword(false);
        account().setPassword({});
        account().setHasPassword(true);
        if (!isConnecting())
            return;
        m_connection->callApi<Quotient::GetConfigAuthedJob>(Quotient::BackgroundRequest)
            .then(this, [this](Quotient::GetConfigAuthedJob *job) {
                if (job)
                    m_maximumAttachmentSize = job->uploadSize().value_or(0);
            });
        if (m_contactAvatarService)
            m_contactAvatarService->observeContact(m_connection->userId());
        m_connection->syncLoop();
        loggedIn();
    });
    connect(m_connection, &Quotient::Connection::syncDone, this, &MatrixProtocol::promptForRecoveryKeyRestore);
    // Temporary Matrix room/contact diagnostic. Uncomment while investigating list mapping.
    // connect(m_connection, &Quotient::Connection::syncDone, this, &MatrixProtocol::dumpMatrixRooms);
    // connect(m_connection, &Quotient::Connection::newRoom, this, [this](Quotient::Room *room) {
    //     watchRoomForDebug(room);
    //     dumpMatrixRooms();
    // });
    // connect(m_connection, &Quotient::Connection::directChatsListChanged, this,
    //         [this](const Quotient::DirectChatsMap &, const Quotient::DirectChatsMap &) {
    //             dumpMatrixRooms();
    //         });
    connect(m_connection, &Quotient::Connection::newKeyVerificationSession, this,
            [this](Quotient::KeyVerificationSession *session) {
                if (m_deviceVerificationNotificationService)
                    m_deviceVerificationNotificationService->notifyVerificationRequest(account(), session);
            });
    connect(m_connection, &Quotient::Connection::newRoom, this, &MatrixProtocol::watchVerificationRoom);
    connect(m_connection, &Quotient::Connection::invitedRoom, this,
            [this](Quotient::Room *room, Quotient::Room *) {
                if (m_roomInvitationNotificationService)
                    m_roomInvitationNotificationService->notifyInvitation(account(), room);
            });
    connect(m_connection, &Quotient::Connection::loggedOut, this, [this] {
        if (m_chatService)
            m_chatService->setConnection(nullptr);
        if (m_chatStateService)
            m_chatStateService->setConnection(nullptr);
        if (m_accountAvatarService)
            m_accountAvatarService->setConnection(nullptr);
        if (m_contactAvatarService)
            m_contactAvatarService->setConnection(nullptr);
        if (m_historyService)
            m_historyService->setConnection(nullptr);
        if (m_timelineService)
            m_timelineService->setConnection(nullptr);
        m_recoveryKeyRestorePrompted = false;
        loggedOut();
    });
    connect(
        m_connection, &Quotient::Connection::loginError, this,
        [this](const QString &message, const QString &details) { handleConnectionError(message, details); });
    connect(
        m_connection, &Quotient::Connection::resolveError, this,
        [this](const QString &message) { handleConnectionError(message); });
}

void MatrixProtocol::watchRoomForDebug(Quotient::Room *room)
{
    if (!room || m_debugWatchedRooms.contains(room))
        return;

    m_debugWatchedRooms.insert(room);
    connect(room, &Quotient::Room::changed, this,
            [this](Quotient::Room::Changes) { dumpMatrixRooms(); });
    connect(room, &QObject::destroyed, this,
            [this, room] { m_debugWatchedRooms.remove(room); });
}

void MatrixProtocol::dumpMatrixRooms()
{
    if (!m_connection)
        return;

    const auto joinStateName = [](Quotient::JoinState state) {
        switch (state)
        {
        case Quotient::JoinState::Invite:
            return QStringLiteral("invite");
        case Quotient::JoinState::Join:
            return QStringLiteral("join");
        case Quotient::JoinState::Leave:
            return QStringLiteral("leave");
        }

        return QStringLiteral("unknown");
    };

    QJsonArray roomsJson;
    const auto rooms = m_connection->allRooms();
    for (auto *room : rooms)
    {
        if (!room)
            continue;

        watchRoomForDebug(room);

        QJsonObject roomJson;
        roomJson.insert(QStringLiteral("roomId"), room->id());
        roomJson.insert(QStringLiteral("joinState"), joinStateName(room->joinState()));
        roomJson.insert(QStringLiteral("joinStateValue"), static_cast<int>(room->joinState()));
        roomJson.insert(QStringLiteral("displayName"), room->displayName());
        roomJson.insert(QStringLiteral("name"), room->name());
        roomJson.insert(QStringLiteral("canonicalAlias"), room->canonicalAlias());
        roomJson.insert(QStringLiteral("aliases"), QJsonArray::fromStringList(room->aliases()));
        roomJson.insert(QStringLiteral("isDirect"), m_connection->isDirectChat(room->id()));
        roomJson.insert(
            QStringLiteral("directMxids"),
            QJsonArray::fromStringList(m_connection->directChatMemberIds(room)));
        roomJson.insert(
            QStringLiteral("joinedMxids"), QJsonArray::fromStringList(room->joinedMemberIds()));
        roomJson.insert(QStringLiteral("joinedCount"), room->joinedCount());
        roomJson.insert(QStringLiteral("invitedCount"), room->invitedCount());
        roomJson.insert(QStringLiteral("totalMemberCount"), room->totalMemberCount());
        roomJson.insert(QStringLiteral("encrypted"), room->usesEncryption());
        roomJson.insert(QStringLiteral("predecessorRoomId"), room->predecessorId());
        roomJson.insert(QStringLiteral("successorRoomId"), room->successorId());
        roomsJson.append(roomJson);
    }

    QJsonArray directChatsJson;
    const auto directChats = m_connection->directChats();
    for (auto it = directChats.cbegin(); it != directChats.cend(); ++it)
    {
        QJsonObject directChatJson;
        directChatJson.insert(QStringLiteral("mxid"), it.key() ? it.key()->id() : QString{});
        directChatJson.insert(
            QStringLiteral("globalDisplayName"), it.key() ? it.key()->displayname() : QString{});
        directChatJson.insert(
            QStringLiteral("globalAvatarUrl"), it.key() ? it.key()->avatarUrl().toString() : QString{});
        directChatJson.insert(QStringLiteral("roomId"), it.value());
        const auto *mappedRoom = m_connection->room(
            it.value(), Quotient::JoinState::Invite | Quotient::JoinState::Join | Quotient::JoinState::Leave);
        directChatJson.insert(QStringLiteral("roomKnown"), mappedRoom != nullptr);
        directChatJson.insert(
            QStringLiteral("roomJoinState"),
            mappedRoom ? joinStateName(mappedRoom->joinState()) : QStringLiteral("unknown"));
        directChatsJson.append(directChatJson);
    }

    QJsonObject dump;
    dump.insert(QStringLiteral("accountMxid"), m_connection->userId());
    dump.insert(QStringLiteral("directChats"), directChatsJson);
    dump.insert(QStringLiteral("rooms"), roomsJson);

    QJsonArray contactsJson;
    if (m_contactManager)
    {
        for (const auto &contact : m_contactManager->contacts(account()))
        {
            const auto buddy = contact.ownerBuddy();
            QJsonObject contactJson;
            contactJson.insert(QStringLiteral("mxid"), contact.id());
            contactJson.insert(QStringLiteral("anonymous"), contact.isAnonymous());
            contactJson.insert(
                QStringLiteral("buddyUuid"), buddy ? buddy.uuid().toString(QUuid::WithoutBraces) : QString{});
            contactJson.insert(QStringLiteral("buddyDisplay"), buddy ? buddy.display() : QString{});
            contactJson.insert(
                QStringLiteral("buddyContactCount"), buddy ? buddy.contacts().size() : 0);
            contactsJson.append(contactJson);
        }
    }
    dump.insert(QStringLiteral("kaduContacts"), contactsJson);

    QJsonArray buddiesJson;
    if (m_buddyManager)
    {
        for (const auto &buddy : m_buddyManager->buddies(account(), true))
        {
            QStringList contactMxids;
            for (const auto &contact : buddy.contacts(account()))
                contactMxids.append(contact.id());

            QJsonObject buddyJson;
            buddyJson.insert(QStringLiteral("uuid"), buddy.uuid().toString(QUuid::WithoutBraces));
            buddyJson.insert(QStringLiteral("display"), buddy.display());
            buddyJson.insert(QStringLiteral("anonymous"), buddy.isAnonymous());
            buddyJson.insert(QStringLiteral("contactMxids"), QJsonArray::fromStringList(contactMxids));
            buddiesJson.append(buddyJson);
        }
    }
    dump.insert(QStringLiteral("kaduBuddies"), buddiesJson);

    QJsonArray chatsJson;
    if (m_chatManager)
    {
        for (const auto &chat : m_chatManager->chats(account()))
        {
            QStringList contactMxids;
            for (const auto &contact : chat.contacts())
                contactMxids.append(contact.id());

            const auto *roomDetails = qobject_cast<ChatDetailsRoom *>(chat.details());
            const auto roomId = roomDetails ? roomDetails->room() : QString{};
            QJsonObject chatJson;
            chatJson.insert(QStringLiteral("type"), chat.type());
            chatJson.insert(QStringLiteral("display"), chat.display());
            chatJson.insert(QStringLiteral("roomId"), roomId);
            chatJson.insert(QStringLiteral("contactMxids"), QJsonArray::fromStringList(contactMxids));
            chatJson.insert(
                QStringLiteral("matrixRoomIsDirect"),
                !roomId.isEmpty() && m_connection->isDirectChat(roomId));
            chatsJson.append(chatJson);
        }
    }
    dump.insert(QStringLiteral("kaduChats"), chatsJson);

    qInfo().noquote() << "[Matrix room dump]"
                      << QString::fromUtf8(QJsonDocument{dump}.toJson(QJsonDocument::Indented));
}

void MatrixProtocol::promptForRecoveryKeyRestore()
{
    if (!m_connection || m_recoveryKeyRestorePrompted || !m_connection->encryptionEnabled()
        || !m_connection->hasAccountData(QStringLiteral("m.secret_storage.default_key"))
        || !m_connection->hasAccountData(QStringLiteral("m.megolm_backup.v1")))
        return;

    auto *database = m_connection->database();
    if (!database || !database->loadEncrypted(QStringLiteral("m.megolm_backup.v1")).isEmpty())
        return;

    restoreRecoveryKey();
}

void MatrixProtocol::restoreRecoveryKey()
{
    if (m_recoveryDialog)
    {
        m_recoveryDialog->show();
        m_recoveryDialog->raise();
        m_recoveryDialog->activateWindow();
        return;
    }
    if (!isConnected() || !m_connection || !m_connectionReady
        || !m_connection->isLoggedIn() || !m_connection->encryptionEnabled())
    {
        QMessageBox::information(nullptr, tr("Restore Matrix Recovery Key"),
                                 tr("Connect the Matrix account with encryption enabled before restoring its keys."));
        return;
    }
    if (!m_connection->hasAccountData(QStringLiteral("m.secret_storage.default_key"))
        || !m_connection->hasAccountData(QStringLiteral("m.megolm_backup.v1")))
    {
        QMessageBox::information(nullptr, tr("Restore Matrix Recovery Key"),
                                 tr("No recovery data is available yet. Wait for synchronisation or check the key backup on your other device."));
        return;
    }
    m_recoveryKeyRestorePrompted = true;
    auto *dialog = new MatrixRestoreRecoveryKeyDialog{m_connection};
    m_recoveryDialog = dialog;
    connect(dialog, &MatrixRestoreRecoveryKeyDialog::keysRestored, this, [this] {
        if (m_timelineService)
            m_timelineService->refreshEncryptedEvents();
    });
    dialog->show();
}

void MatrixProtocol::handleConnectionError(const QString &message, const QString &details)
{
    m_loginInProgress = false;
    const auto errorCode = QJsonDocument::fromJson(details.toUtf8()).object().value(QStringLiteral("errcode")).toString();
    if (errorCode == QStringLiteral("M_UNKNOWN_TOKEN") || errorCode == QStringLiteral("M_FORBIDDEN"))
    {
        m_accessTokenRejected = true;
        account().setPassword({});
        if (m_connection)
            m_connection->stopSync();
        if (m_sessionService)
            m_sessionService->setConnection(nullptr);
    }
    const auto reason = details.isEmpty() ? message : QStringLiteral("%1: %2").arg(message, details);
    emit connectionError(account(), MatrixAccountData{account()}.homeserver(), reason);
    connectionError();
}

void MatrixProtocol::login()
{
    if (m_loginInProgress)
        return;
    if (m_accessTokenRejected)
    {
        if (account().password().isEmpty())
        {
            passwordRequired();
            return;
        }
        // A rejected token must not be read from the credential store again.
        // A new password login gets a fresh device and encryption context.
        discardConnection();
        createConnection();
        loginWithPassword();
        return;
    }
    if (!m_connection)
        createConnection();
    else
    {
        if (m_chatService)
            m_chatService->setConnection(m_connection);
        if (m_chatStateService)
            m_chatStateService->setConnection(m_connection);
        if (m_accountAvatarService)
            m_accountAvatarService->setConnection(m_connection);
        if (m_contactAvatarService)
            m_contactAvatarService->setConnection(m_connection);
        if (m_historyService)
            m_historyService->setConnection(m_connection);
        if (m_timelineService)
            m_timelineService->setConnection(m_connection);
        if (m_sessionService)
            m_sessionService->setConnection(m_connection);
    }

    const auto accountData = MatrixAccountData{account()};
    if (m_connection->isLoggedIn() && m_connectionReady)
    {
        // Resuming after a network interruption must not initialise Olm twice.
        m_connection->syncLoop();
        loggedIn();
        return;
    }
    if (accountData.deviceId().isEmpty())
    {
        loginWithPassword();
        return;
    }

    m_loginInProgress = true;
    auto *accessTokenJob = new QKeychain::ReadPasswordJob{qAppName(), this};
    accessTokenJob->setKey(account().id());
    const QPointer<Quotient::Connection> requestedConnection{m_connection};
    connect(accessTokenJob, &QKeychain::Job::finished, this, [this, accessTokenJob, requestedConnection] {
        if (!requestedConnection || requestedConnection != m_connection)
            return;
        if (!isConnecting())
        {
            m_loginInProgress = false;
            return;
        }

        if (accessTokenJob->error() != QKeychain::Error::NoError
            && accessTokenJob->error() != QKeychain::Error::EntryNotFound)
        {
            handleConnectionError(tr("The saved Matrix session could not be read from the system credential store."),
                                  accessTokenJob->errorString());
            return;
        }
        const auto accessToken = accessTokenJob->error() == QKeychain::Error::NoError
                                     ? accessTokenJob->binaryData()
                                     : QByteArray{};
        if (accessToken.isEmpty())
        {
            m_loginInProgress = false;
            loginWithPassword();
            return;
        }

        m_connection->assumeIdentity(
            account().id(), MatrixAccountData{account()}.deviceId(), QString::fromUtf8(accessToken));
    });
    accessTokenJob->start();
}

void MatrixProtocol::loginWithPassword()
{
    if (!m_connection)
        return;

    if (account().password().isEmpty())
    {
        passwordRequired();
        return;
    }

    m_accessTokenRejected = false;
    m_loginInProgress = true;
    m_connection->loginWithPassword(account().id(), account().password(), QStringLiteral("Kadu"));
}

void MatrixProtocol::joinRoom(const QString &roomIdOrAlias)
{
    if (!m_connection || !m_connection->isLoggedIn() || roomIdOrAlias.isEmpty())
        return;

    m_connection->joinRoom(roomIdOrAlias);
}

void MatrixProtocol::rejectRoomInvitation(const QString &roomId)
{
    if (!m_connection || !m_connection->isLoggedIn() || roomId.isEmpty())
        return;

    if (auto *room = m_connection->room(roomId, Quotient::JoinState::Invite))
        room->leaveRoom();
}

QStringList MatrixProtocol::availableVerificationDevices() const
{
    if (!isConnected() || !m_connection || !m_connectionReady
        || !m_connection->isLoggedIn() || !m_connection->encryptionEnabled())
        return {};

    auto devices = m_connection->devicesForUser(m_connection->userId());
    devices.removeAll(m_connection->deviceId());
    devices.sort();
    return devices;
}

void MatrixProtocol::verifyDevice(const QString &deviceId)
{
    if (auto *dialog = m_deviceVerificationDialogs.value(deviceId).data())
    {
        dialog->show();
        dialog->raise();
        dialog->activateWindow();
        return;
    }
    if (!m_connection || !availableVerificationDevices().contains(deviceId))
        return;
    if (m_connection->hasConflictingDeviceIdsAndCrossSigningKeys(m_connection->userId()))
    {
        QMessageBox::warning(nullptr, tr("Verify Matrix Device"),
                             tr("The account has conflicting device and cross-signing keys. Verification cannot continue."));
        return;
    }

    auto *session = m_connection->startKeyVerificationSession(m_connection->userId(), deviceId);
    if (!session)
        return;

    showDeviceVerificationDialog(session);
    // The libQuotient constructor has already sent the request.
}

void MatrixProtocol::showDeviceVerificationDialog(Quotient::KeyVerificationSession *session)
{
    if (!session || session->userVerification())
        return;

    auto *dialog = new MatrixDeviceVerificationDialog{session};
    const auto deviceId = session->remoteDeviceId();
    m_deviceVerificationDialogs.insert(deviceId, dialog);
    connect(dialog, &QObject::destroyed, this, [this, deviceId, dialog] {
        if (!m_deviceVerificationDialogs.value(deviceId) || m_deviceVerificationDialogs.value(deviceId) == dialog)
            m_deviceVerificationDialogs.remove(deviceId);
    });
    dialog->show();
}

void MatrixProtocol::watchVerificationRoom(Quotient::Room *room)
{
    if (!room || m_inRoomVerificationRooms.contains(room))
        return;
    m_inRoomVerificationRooms.insert(room);
    connect(room, &QObject::destroyed, this, [this, room] {
        m_inRoomVerificationRooms.remove(room);
        m_verificationRefreshScheduled.remove(room);
    });
    connect(room, &Quotient::Room::addedMessages, this,
            [this, room](int fromIndex, int toIndex) {
                handleInRoomVerificationEvents(room, fromIndex, toIndex);
            });
    connect(room, &Quotient::Room::replacedEvent, this, [this, room](const Quotient::RoomEvent *event) {
        const auto *request = Quotient::eventCast<const Quotient::RoomMessageEvent>(event);
        if (!Quotient::eventCast<const Quotient::KeyVerificationEvent>(event)
            && (!request || request->rawMsgtype() != QStringLiteral("m.key.verification.request")))
            return;
        scheduleVerificationRefresh(room);
    });
}

void MatrixProtocol::scheduleVerificationRefresh(Quotient::Room *room)
{
    if (!room || m_verificationRefreshScheduled.contains(room))
        return;
    m_verificationRefreshScheduled.insert(room);
    // Decryption can reveal a request after its replies are already present.
    const QPointer<Quotient::Room> watchedRoom{room};
    QTimer::singleShot(0, this, [this, watchedRoom] {
        if (!watchedRoom)
            return;
        m_verificationRefreshScheduled.remove(watchedRoom);
        handleInRoomVerificationEvents(watchedRoom, watchedRoom->minTimelineIndex(), watchedRoom->maxTimelineIndex());
    });
}

void MatrixProtocol::handleInRoomVerificationEvents(Quotient::Room *room, int fromIndex, int toIndex)
{
    if (!m_connection || !m_connection->isLoggedIn() || !room || room->connection() != m_connection)
        return;

    for (const auto &item : room->messageEvents())
    {
        if (item.index() < fromIndex || item.index() > toIndex)
            continue;

        const auto *visibleEvent = item.event();
        const auto *originalEvent = item.viewAs<Quotient::EncryptedEvent>();
        Quotient::RoomEventPtr decryptedEvent;
        if (originalEvent)
        {
            const auto age = originalEvent->originTimestamp().secsTo(QDateTime::currentDateTimeUtc());
            if (originalEvent->senderId() != m_connection->userId() || age < -60 || age > 600)
                continue;
            decryptedEvent = room->decryptMessage(*originalEvent);
            if (!decryptedEvent)
                continue;
            visibleEvent = decryptedEvent.get();
        }
        else
            originalEvent = visibleEvent->originalEvent();

        // libQuotient skips every own-user event here, including requests from
        // another device. Create the session directly from this exact request.
        if (const auto *request = Quotient::eventCast<const Quotient::RoomMessageEvent>(visibleEvent);
            request && request->senderId() == m_connection->userId()
            && request->rawMsgtype() == QStringLiteral("m.key.verification.request")
            && request->contentPart<QString>(QStringLiteral("to")) == m_connection->userId())
        {
            const auto key = room->id() + QLatin1Char('\x1f') + request->id();
            const auto device = request->contentPart<QString>(QStringLiteral("from_device"));
            const auto age = request->originTimestamp().secsTo(QDateTime::currentDateTimeUtc());
            if (device.isEmpty() || device == m_connection->deviceId() || age < -60 || age > 600
                || request->id().isEmpty() || !request->originTimestamp().isValid()
                || (originalEvent && !originalEvent->deviceId().isEmpty() && originalEvent->deviceId() != device)
                || m_connection->hasConflictingDeviceIdsAndCrossSigningKeys(m_connection->userId())
                || m_inRoomVerificationSessions.contains(key))
                continue;
            auto *session = new Quotient::KeyVerificationSession{request, room};
            m_inRoomVerificationSessions.insert(key, session);
            connect(session, &QObject::destroyed, this, [this, key, session] {
                // Retain the null entry so a timeline refresh cannot revive a finished request.
                if (!m_inRoomVerificationSessions.value(key) || m_inRoomVerificationSessions.value(key) == session)
                    m_handledInRoomVerificationEvents.remove(key);
            });
            if (m_deviceVerificationNotificationService)
                m_deviceVerificationNotificationService->notifyVerificationRequest(account(), session);
            continue;
        }

        const auto *event = Quotient::eventCast<const Quotient::KeyVerificationEvent>(visibleEvent);
        if (!event || event->senderId() != m_connection->userId()
            || !static_cast<const Quotient::RoomEvent *>(event)->transactionId().isEmpty()
            || (originalEvent && originalEvent->deviceId() == m_connection->deviceId()))
            continue;

        const auto relation = event->contentPart<QJsonObject>(QStringLiteral("m.relates_to"));
        if (relation.value(QStringLiteral("rel_type")).toString() != QStringLiteral("m.reference"))
            continue;
        const auto requestEventId = relation.value(QStringLiteral("event_id")).toString();
        const auto key = room->id() + QLatin1Char('\x1f') + requestEventId;
        const auto session = m_inRoomVerificationSessions.value(key);
        const auto eventId = item.event()->id();
        if (!session || session->state() == Quotient::KeyVerificationSession::DONE
            || session->state() == Quotient::KeyVerificationSession::CANCELED
            || m_handledInRoomVerificationEvents[key].contains(eventId))
            continue;
        const auto fromDevice = event->contentPart<QString>(QStringLiteral("from_device"));
        if ((!fromDevice.isEmpty() && fromDevice != session->remoteDeviceId())
            || (originalEvent && !originalEvent->deviceId().isEmpty()
                && originalEvent->deviceId() != session->remoteDeviceId()))
            continue;
        m_handledInRoomVerificationEvents[key].insert(eventId);
        session->handleEvent(*event);
    }
}

void MatrixProtocol::logout()
{
    // Protocol::logout() is also used for going offline. Revoking the Matrix
    // token here would delete the device and its local pickling key on every
    // status change. Keep the identity so it can resume after going online.
    if (m_connection)
    {
        if (!m_applicationQuitting)
            for (auto *session : m_connection->findChildren<Quotient::KeyVerificationSession *>(
                     QString{}, Qt::FindDirectChildrenOnly))
                if (session->state() != Quotient::KeyVerificationSession::DONE
                    && session->state() != Quotient::KeyVerificationSession::CANCELED)
                    session->cancelVerification(Quotient::KeyVerificationSession::USER);
        m_connection->stopSync();
    }
    if (m_recoveryDialog)
        m_recoveryDialog->reject();
    detachConnectionServices();
    loggedOut();
}

void MatrixProtocol::discardConnection()
{
    m_connectionReady = false;
    m_loginInProgress = false;
    if (m_recoveryDialog)
        m_recoveryDialog->reject();
    if (m_connection)
    {
        m_connection->stopSync();
        disconnect(m_connection, nullptr, this, nullptr);
        detachConnectionServices();
        m_connection->deleteLater();
        m_connection = nullptr;
    }
}

void MatrixProtocol::detachConnectionServices()
{
    if (m_chatService)
        m_chatService->setConnection(nullptr);
    if (m_chatStateService)
        m_chatStateService->setConnection(nullptr);
    if (m_accountAvatarService)
        m_accountAvatarService->setConnection(nullptr);
    if (m_contactAvatarService)
        m_contactAvatarService->setConnection(nullptr);
    if (m_historyService)
        m_historyService->setConnection(nullptr);
    if (m_timelineService)
        m_timelineService->setConnection(nullptr);
    if (m_sessionService)
        m_sessionService->setConnection(nullptr);
}

void MatrixProtocol::sendStatusToServer()
{
    // Presence export belongs to the future libQuotient-backed connection.
}
