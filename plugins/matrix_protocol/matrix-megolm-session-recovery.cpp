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
#include <Quotient/e2ee/sssshandler.h>
#include <Quotient/events/encryptedevent.h>
#include <Quotient/room.h>

#include <QtCore/QJsonDocument>

MatrixMegolmSessionRecovery::MatrixMegolmSessionRecovery(QObject *parent) : QObject{parent}
{
}

void MatrixMegolmSessionRecovery::setConnection(Quotient::Connection *connection)
{
    if (m_connection == connection)
        return;

    m_connection = connection;
    m_pendingRequests.clear();
    m_requestAttempts.clear();
    m_crossSigningRequested = false;
    if (m_crossSigningRecovery)
        m_crossSigningRecovery->deleteLater();
    m_crossSigningRecovery = nullptr;

    if (!m_connection)
        return;

    m_crossSigningRecovery = new Quotient::SSSSHandler{this};
    m_crossSigningRecovery->setConnection(m_connection);
    connect(m_crossSigningRecovery, &Quotient::SSSSHandler::finished, this, [this] { emit backupRestored(); });
}

void MatrixMegolmSessionRecovery::requestFromBackup(Quotient::Room *room,
                                                    const Quotient::EncryptedEvent &event)
{
    if (!m_connection || !room || event.sessionId().isEmpty())
        return;

    auto *database = m_connection->database();
    if (!database)
        return;

    const auto backupDecryptionKey = database->loadEncrypted(QStringLiteral("m.megolm_backup.v1"));
    if (backupDecryptionKey.isEmpty())
    {
        requestBackupKeyFromVerifiedDevice();
        return;
    }

    const auto roomId = room->id();
    const auto sessionId = event.sessionId();
    const auto requestId = roomId + QStringLiteral("\x1f") + sessionId;
    if (m_pendingRequests.contains(requestId))
        return;

    const auto now = QDateTime::currentDateTimeUtc();
    const auto previousAttempt = m_requestAttempts.value(requestId);
    if (previousAttempt.isValid() && previousAttempt.secsTo(now) < 60)
        return;

    m_pendingRequests.insert(requestId);
    m_requestAttempts.insert(requestId, now);

    const QPointer<Quotient::Connection> requestedConnection{m_connection};
    const QPointer<Quotient::Room> requestedRoom{room};
    auto versionJob =
        m_connection->callApi<Quotient::GetRoomKeysVersionCurrentJob>(Quotient::BackgroundRequest);
    connect(versionJob, &Quotient::BaseJob::failure, this,
            [this, requestedConnection, requestId] { finishRequest(requestId, requestedConnection.data()); });
    connect(versionJob, &Quotient::BaseJob::success, this,
            [this, requestedConnection, requestedRoom, requestId, roomId, sessionId, backupDecryptionKey,
             versionJob] {
                if (!requestedConnection || requestedConnection != m_connection || !requestedRoom)
                {
                    finishRequest(requestId, requestedConnection.data());
                    return;
                }

                if (versionJob->algorithm() != QStringLiteral("m.megolm_backup.v1.curve25519-aes-sha2")
                    || versionJob->version().isEmpty())
                {
                    finishRequest(requestId, requestedConnection.data());
                    return;
                }

                auto keyJob = requestedConnection->callApi<Quotient::GetRoomKeyBySessionIdJob>(
                    Quotient::BackgroundRequest, roomId, sessionId, versionJob->version());
                connect(keyJob, &Quotient::BaseJob::failure, this,
                        [this, requestedConnection, requestId] {
                            finishRequest(requestId, requestedConnection.data());
                        });
                connect(keyJob, &Quotient::BaseJob::success, this,
                        [this, requestedConnection, requestedRoom, requestId, sessionId, backupDecryptionKey,
                         keyJob] {
                            finishRequest(requestId, requestedConnection.data());
                            if (!requestedConnection || requestedConnection != m_connection || !requestedRoom)
                                return;

                            const auto backupData = keyJob->jsonData();
                            const auto firstMessageIndex =
                                backupData.value(QStringLiteral("first_message_index")).toInt(-1);
                            const auto sessionData =
                                backupData.value(QStringLiteral("session_data")).toObject();
                            const auto decrypted = Quotient::curve25519AesSha2Decrypt(
                                sessionData.value(QStringLiteral("ciphertext")).toString().toLatin1(),
                                backupDecryptionKey,
                                sessionData.value(QStringLiteral("ephemeral")).toString().toLatin1(),
                                sessionData.value(QStringLiteral("mac")).toString().toLatin1());
                            if (!decrypted.has_value())
                                return;

                            QJsonParseError parseError;
                            const auto sessionDocument = QJsonDocument::fromJson(decrypted.value(), &parseError);
                            if (parseError.error != QJsonParseError::NoError || !sessionDocument.isObject())
                                return;

                            const auto session = sessionDocument.object();
                            const auto sessionKey = session.value(QStringLiteral("session_key")).toString().toLatin1();
                            const auto senderKey = session.value(QStringLiteral("sender_key")).toString().toLatin1();
                            const auto senderEdKey = session.value(QStringLiteral("sender_claimed_keys"))
                                                         .toObject()
                                                         .value(QStringLiteral("ed25519"))
                                                         .toString()
                                                         .toLatin1();
                            if (sessionKey.isEmpty() || senderKey.isEmpty() || senderEdKey.isEmpty()
                                || firstMessageIndex < 0)
                                return;

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

void MatrixMegolmSessionRecovery::requestBackupKeyFromVerifiedDevice()
{
    if (m_crossSigningRequested || !m_crossSigningRecovery || !m_connection)
        return;

    m_crossSigningRequested = true;
    m_crossSigningRecovery->unlockSSSSFromCrossSigning();
}
