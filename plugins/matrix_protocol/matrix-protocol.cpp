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

#include "matrix-account-data.h"
#include "matrix-chat-service.h"
#include "matrix-chat-state-service.h"
#include "matrix-account-avatar-service.h"
#include "matrix-contact-avatar-service.h"
#include "matrix-device-verification-notification-service.h"
#include "matrix-history-service.h"
#include "matrix-timeline-service.h"
#include "matrix-room-invitation-notification-service.h"
#include "gui/matrix-device-verification-dialog.h"
#include "gui/matrix-restore-recovery-key-dialog.h"

#include <Quotient/connection.h>
#include <Quotient/csapi/authed-content-repo.h>
#include <Quotient/database.h>
#include <Quotient/keyverificationsession.h>
#include <Quotient/room.h>

#include <qt6keychain/keychain.h>

#include <QtCore/QByteArray>
#include <QtCore/QCoreApplication>
#include <QtCore/QSignalBlocker>
#include <QtCore/QUrl>

MatrixProtocol::MatrixProtocol(Account account, ProtocolFactory *factory) : Protocol{account, factory}
{
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

void MatrixProtocol::createConnection()
{
    m_connection = new Quotient::Connection{QUrl{MatrixAccountData{account()}.homeserver()}, this};
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

    connect(m_connection, &Quotient::Connection::connected, this, [this] {
        if (!m_connection)
            return;

        MatrixAccountData{account()}.setDeviceId(m_connection->deviceId());
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
    connect(m_connection, &Quotient::Connection::newKeyVerificationSession, this,
            [this](Quotient::KeyVerificationSession *session) {
                if (m_deviceVerificationNotificationService)
                    m_deviceVerificationNotificationService->notifyVerificationRequest(account(), session);
            });
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

void MatrixProtocol::promptForRecoveryKeyRestore()
{
    if (!m_connection || m_recoveryKeyRestorePrompted || !m_connection->encryptionEnabled()
        || !m_connection->hasAccountData(QStringLiteral("m.secret_storage.default_key")))
        return;

    auto *database = m_connection->database();
    if (!database || !database->loadEncrypted(QStringLiteral("m.cross_signing.master")).isEmpty())
        return;

    m_recoveryKeyRestorePrompted = true;
    auto *dialog = new MatrixRestoreRecoveryKeyDialog{m_connection};
    dialog->show();
}

void MatrixProtocol::handleConnectionError(const QString &message, const QString &details)
{
    const auto reason = details.isEmpty() ? message : QStringLiteral("%1: %2").arg(message, details);
    emit connectionError(account(), MatrixAccountData{account()}.homeserver(), reason);
    connectionError();
}

void MatrixProtocol::login()
{
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
    }

    const auto accountData = MatrixAccountData{account()};
    if (accountData.deviceId().isEmpty())
    {
        loginWithPassword();
        return;
    }

    auto *accessTokenJob = new QKeychain::ReadPasswordJob{qAppName(), this};
    accessTokenJob->setKey(account().id());
    connect(accessTokenJob, &QKeychain::Job::finished, this, [this, accessTokenJob] {
        if (!m_connection)
            return;

        const auto accessToken = accessTokenJob->error() == QKeychain::Error::NoError
                                     ? accessTokenJob->binaryData()
                                     : QByteArray{};
        if (accessToken.isEmpty())
        {
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
    if (!m_connection || !m_connection->isLoggedIn() || !m_connection->encryptionEnabled())
        return {};

    auto devices = m_connection->devicesForUser(m_connection->userId());
    devices.removeAll(m_connection->deviceId());
    devices.sort();
    return devices;
}

void MatrixProtocol::verifyDevice(const QString &deviceId)
{
    if (!m_connection || !availableVerificationDevices().contains(deviceId))
        return;

    Quotient::KeyVerificationSession *session = nullptr;
    {
        QSignalBlocker blocker{m_connection};
        session = m_connection->startKeyVerificationSession(m_connection->userId(), deviceId);
    }
    if (!session)
        return;

    showDeviceVerificationDialog(session);
    session->sendRequest();
}

void MatrixProtocol::showDeviceVerificationDialog(Quotient::KeyVerificationSession *session)
{
    if (!session || session->userVerification())
        return;

    auto *dialog = new MatrixDeviceVerificationDialog{session};
    dialog->show();
}

void MatrixProtocol::logout()
{
    if (m_applicationQuitting)
    {
        if (m_connection)
            m_connection->stopSync();
        loggedOut();
        return;
    }

    if (m_connection && m_connection->isLoggedIn())
    {
        m_connection->logout();
        return;
    }

    if (m_connection)
    {
        disconnect(m_connection, nullptr, this, nullptr);
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
        m_connection->deleteLater();
        m_connection = nullptr;
    }
    loggedOut();
}

void MatrixProtocol::sendStatusToServer()
{
    // Presence export belongs to the future libQuotient-backed connection.
}
