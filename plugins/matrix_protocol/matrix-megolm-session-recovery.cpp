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

#include "matrix-megolm-session-recovery.h"

#include <Quotient/connection.h>
#include <Quotient/csapi/key_backup.h>
#include <Quotient/database.h>
#include <Quotient/e2ee/cryptoutils.h>
#include <Quotient/e2ee/qolminboundsession.h>
#include <Quotient/events/encryptedevent.h>
#include <Quotient/room.h>

#include <QtCore/QJsonDocument>
#include <QtCore/QTimer>

#include <algorithm>
#include <limits>

MatrixMegolmSessionRecovery::MatrixMegolmSessionRecovery(QObject *parent) : QObject{parent}
{
}

void MatrixMegolmSessionRecovery::setConnection(Quotient::Connection *connection)
{
    if (m_connection == connection)
        return;
    ++m_connectionGeneration;

    if (m_connection)
        disconnect(m_connection, nullptr, this, nullptr);
    m_connection = connection;
    m_pendingRequests.clear();
    m_requestAttempts.clear();
    m_waitingForBackupKey.clear();
    m_crossSigningRequested = false;
    m_backupKeyRequestStarted = false;
    m_backupKeyAvailable = false;

    if (!m_connection)
        return;

    connect(m_connection, &Quotient::Connection::sessionVerified, this,
            [this](const QString &userId, const QString &) {
                if (m_connection && userId == m_connection->userId())
                {
                    m_requestAttempts.clear();
                    emit backupRestored();
                }
            });
}

void MatrixMegolmSessionRecovery::requestFromBackup(Quotient::Room *room,
                                                    const Quotient::EncryptedEvent &event)
{
    if (!m_connection || !m_connection->isLoggedIn() || !room || room->connection() != m_connection
        || event.sessionId().isEmpty())
        return;

    const auto roomId = room->id();
    const auto sessionId = event.sessionId();
    const auto requestId = roomId + QStringLiteral("\x1f") + sessionId;
    if (m_pendingRequests.contains(requestId))
        return;

    auto *database = m_connection->database();
    const auto backupDecryptionKey = database
        ? database->loadEncrypted(QStringLiteral("m.megolm_backup.v1")) : QByteArray{};
    if (!backupDecryptionKey.isEmpty() && !m_backupKeyAvailable)
    {
        // Manual recovery must also release requests waiting for another device.
        m_backupKeyAvailable = true;
        m_waitingForBackupKey.clear();
        m_requestAttempts.clear();
    }
    if (m_waitingForBackupKey.contains(requestId))
        return;
    const auto now = QDateTime::currentDateTimeUtc();
    const auto previousAttempt = m_requestAttempts.value(requestId);
    if (previousAttempt.isValid() && previousAttempt.secsTo(now) < 60)
        return;
    // Set this before any signal: timeline refreshes can re-enter this function.
    m_requestAttempts.insert(requestId, now);

    if (event.algorithm() != QStringLiteral("m.megolm.v1.aes-sha2"))
    {
        emit sessionRecoveryFailed(room, sessionId, Failure::UnsupportedAlgorithm,
                                   tr("The event uses an unsupported encryption algorithm."));
        return;
    }

    if (!database)
    {
        emit sessionRecoveryFailed(room, sessionId, Failure::RecoveryUnavailable,
                                   tr("The local encryption database is unavailable."));
        return;
    }

    if (backupDecryptionKey.isEmpty())
    {
        m_waitingForBackupKey.insert(requestId, WaitingRequest{room, sessionId});
        emit sessionRecoveryStarted(room, sessionId);
        if (!requestBackupKeyFromVerifiedDevice())
        {
            m_waitingForBackupKey.remove(requestId);
            emit sessionRecoveryFailed(room, sessionId, Failure::RecoveryUnavailable,
                                       tr("No key backup decryption key is available."));
        }
        return;
    }

    m_pendingRequests.insert(requestId);
    emit sessionRecoveryStarted(room, sessionId);

    const QPointer<Quotient::Connection> requestedConnection{m_connection};
    const QPointer<Quotient::Room> requestedRoom{room};
    const auto generation = m_connectionGeneration;
    auto versionJob =
        m_connection->callApi<Quotient::GetRoomKeysVersionCurrentJob>(Quotient::BackgroundRequest);
    connect(versionJob, &Quotient::BaseJob::failure, this,
            [this, requestedConnection, requestedRoom, requestId, sessionId, versionJob, generation] {
                if (generation != m_connectionGeneration)
                    return;
                const auto failure = versionJob->error() == Quotient::BaseJob::NotFound
                                         ? Failure::RecoveryUnavailable
                                         : Failure::Unknown;
                failRequest(requestId, requestedConnection.data(), requestedRoom.data(), sessionId, failure,
                            failure == Failure::RecoveryUnavailable
                                ? tr("No server-side key backup is available.")
                                : tr("The server-side key backup could not be read: %1").arg(versionJob->errorString()));
            });
    connect(versionJob, &Quotient::BaseJob::success, this,
            [this, requestedConnection, requestedRoom, requestId, roomId, sessionId, backupDecryptionKey,
             versionJob, generation] {
                if (generation != m_connectionGeneration)
                    return;
                if (!requestedConnection || requestedConnection != m_connection || !requestedRoom)
                {
                    finishRequest(requestId, requestedConnection.data());
                    return;
                }

                if (versionJob->algorithm() != QStringLiteral("m.megolm_backup.v1.curve25519-aes-sha2")
                    || versionJob->version().isEmpty())
                {
                    failRequest(requestId, requestedConnection.data(), requestedRoom.data(), sessionId,
                                Failure::UnsupportedBackup, tr("The key backup format is not supported."));
                    return;
                }

                auto keyJob = requestedConnection->callApi<Quotient::GetRoomKeyBySessionIdJob>(
                    Quotient::BackgroundRequest, roomId, sessionId, versionJob->version());
                connect(keyJob, &Quotient::BaseJob::failure, this,
                        [this, requestedConnection, requestedRoom, requestId, sessionId, keyJob, generation] {
                            if (generation != m_connectionGeneration)
                                return;
                            const auto failure = keyJob->error() == Quotient::BaseJob::NotFound
                                                     ? Failure::MissingKey
                                                     : Failure::Unknown;
                            failRequest(requestId, requestedConnection.data(), requestedRoom.data(), sessionId,
                                        failure,
                                        failure == Failure::MissingKey
                                            ? tr("The decryption key is not present in the server-side backup.")
                                            : tr("The decryption key could not be downloaded: %1")
                                                  .arg(keyJob->errorString()));
                        });
                connect(keyJob, &Quotient::BaseJob::success, this,
                        [this, requestedConnection, requestedRoom, requestId, sessionId, backupDecryptionKey,
                         keyJob, generation] {
                            if (generation != m_connectionGeneration)
                                return;
                            if (!requestedConnection || requestedConnection != m_connection || !requestedRoom)
                            {
                                finishRequest(requestId, requestedConnection.data());
                                return;
                            }

                            const auto backupData = keyJob->jsonData();
                            const auto firstMessageIndex =
                                backupData.value(QStringLiteral("first_message_index")).toInteger(-1);
                            const auto sessionData =
                                backupData.value(QStringLiteral("session_data")).toObject();
                            const auto decrypted = Quotient::curve25519AesSha2Decrypt(
                                sessionData.value(QStringLiteral("ciphertext")).toString().toLatin1(),
                                backupDecryptionKey,
                                sessionData.value(QStringLiteral("ephemeral")).toString().toLatin1(),
                                sessionData.value(QStringLiteral("mac")).toString().toLatin1());
                            if (!decrypted.has_value())
                            {
                                failRequest(requestId, requestedConnection.data(), requestedRoom.data(), sessionId,
                                            Failure::InvalidBackup, tr("The backed-up session key could not be decrypted."));
                                return;
                            }

                            QJsonParseError parseError;
                            const auto sessionDocument = QJsonDocument::fromJson(decrypted.value(), &parseError);
                            if (parseError.error != QJsonParseError::NoError || !sessionDocument.isObject())
                            {
                                failRequest(requestId, requestedConnection.data(), requestedRoom.data(), sessionId,
                                            Failure::InvalidBackup, tr("The backed-up session key is invalid."));
                                return;
                            }

                            const auto session = sessionDocument.object();
                            const auto sessionKey = session.value(QStringLiteral("session_key")).toString().toLatin1();
                            const auto senderKey = session.value(QStringLiteral("sender_key")).toString().toLatin1();
                            const auto senderEdKey = session.value(QStringLiteral("sender_claimed_keys"))
                                                         .toObject()
                                                         .value(QStringLiteral("ed25519"))
                                                         .toString()
                                                         .toLatin1();
                            if (session.value(QStringLiteral("algorithm")).toString()
                                    != QStringLiteral("m.megolm.v1.aes-sha2")
                                || sessionKey.isEmpty() || senderKey.isEmpty() || senderEdKey.isEmpty()
                                || firstMessageIndex < 0 || firstMessageIndex > std::numeric_limits<uint32_t>::max())
                            {
                                failRequest(requestId, requestedConnection.data(), requestedRoom.data(), sessionId,
                                            Failure::InvalidBackup, tr("The backed-up session key is incomplete."));
                                return;
                            }

                            const auto imported = Quotient::QOlmInboundGroupSession::importSession(sessionKey);
                            if (!imported || imported->sessionId() != sessionId.toLatin1()
                                || imported->firstKnownIndex() != static_cast<uint32_t>(firstMessageIndex))
                            {
                                failRequest(requestId, requestedConnection.data(), requestedRoom.data(), sessionId,
                                            Failure::InvalidBackup, tr("The backed-up key does not match the requested session."));
                                return;
                            }

                            finishRequest(requestId, requestedConnection.data());
                            requestedRoom->addMegolmSessionFromBackup(
                                sessionId.toLatin1(), sessionKey,
                                static_cast<uint32_t>(firstMessageIndex), senderKey, senderEdKey);
                            emit sessionRestored(requestedRoom.data(), sessionId);
                        });
            });
}

void MatrixMegolmSessionRecovery::finishRequest(const QString &requestId, Quotient::Connection *connection)
{
    if (connection == m_connection)
        m_pendingRequests.remove(requestId);
}

void MatrixMegolmSessionRecovery::failRequest(const QString &requestId, Quotient::Connection *connection,
                                              Quotient::Room *room, const QString &sessionId, Failure failure,
                                              const QString &errorText)
{
    finishRequest(requestId, connection);
    if (connection == m_connection && room)
        emit sessionRecoveryFailed(room, sessionId, failure, errorText);
}

bool MatrixMegolmSessionRecovery::requestBackupKeyFromVerifiedDevice()
{
    if (m_crossSigningRequested)
        return true;
    if (!m_connection || m_backupKeyRequestStarted)
        return false;

    const auto devices = m_connection->devicesForUser(m_connection->userId());
    if (std::none_of(devices.cbegin(), devices.cend(), [this](const QString &device) {
            return device != m_connection->deviceId()
                   && m_connection->isVerifiedDevice(m_connection->userId(), device);
        }))
        return false;

    m_crossSigningRequested = true;
    m_backupKeyRequestStarted = true;
    const QPointer<Quotient::Connection> requestedConnection{m_connection};
    const auto generation = m_connectionGeneration;
    // SSSSHandler downloads and imports the entire backup on the GUI thread.
    // Request only its decryption key; room sessions are restored on demand.
    m_connection->requestKeyFromDevices(QLatin1String{"m.megolm_backup.v1"})
        .then(this, [this, requestedConnection, generation](const QByteArray &) {
            if (requestedConnection && requestedConnection == m_connection && generation == m_connectionGeneration)
                finishBackupKeyRequest();
        });
    QTimer::singleShot(30000, this, [this, requestedConnection, generation] {
        if (requestedConnection && requestedConnection == m_connection && generation == m_connectionGeneration
            && m_crossSigningRequested)
            finishBackupKeyRequest();
    });
    return true;
}

void MatrixMegolmSessionRecovery::finishBackupKeyRequest()
{
    m_crossSigningRequested = false;
    if (!m_connection)
    {
        m_waitingForBackupKey.clear();
        return;
    }

    auto *database = m_connection->database();
    const auto backupDecryptionKey = database
                                         ? database->loadEncrypted(QStringLiteral("m.megolm_backup.v1"))
                                         : QByteArray{};
    if (!backupDecryptionKey.isEmpty())
    {
        m_waitingForBackupKey.clear();
        m_requestAttempts.clear();
        emit backupRestored();
        return;
    }

    const auto waitingRequests = m_waitingForBackupKey;
    m_waitingForBackupKey.clear();
    for (const auto &request : waitingRequests)
        if (request.room)
            emit sessionRecoveryFailed(request.room.data(), request.sessionId, Failure::RecoveryUnavailable,
                                       tr("No key backup decryption key is available."));
}
