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

#include <QtCore/QDateTime>
#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QSet>

namespace Quotient
{
class Connection;
class EncryptedEvent;
class Room;
}

class MatrixMegolmSessionRecovery final : public QObject
{
    Q_OBJECT

public:
    enum class Failure
    {
        MissingKey,
        RecoveryUnavailable,
        UnsupportedAlgorithm,
        UnsupportedBackup,
        InvalidBackup,
        Unknown
    };
    Q_ENUM(Failure)

    explicit MatrixMegolmSessionRecovery(QObject *parent = nullptr);

    void setConnection(Quotient::Connection *connection);
    void requestFromBackup(Quotient::Room *room, const Quotient::EncryptedEvent &event);

signals:
    void sessionRecoveryStarted(Quotient::Room *room, const QString &sessionId);
    void sessionRecoveryFailed(Quotient::Room *room, const QString &sessionId,
                               MatrixMegolmSessionRecovery::Failure failure, const QString &errorText);
    void sessionRestored(Quotient::Room *room, const QString &sessionId);
    void backupRestored();

private:
    struct WaitingRequest
    {
        QPointer<Quotient::Room> room;
        QString sessionId;
    };

    QPointer<Quotient::Connection> m_connection;
    QSet<QString> m_pendingRequests;
    QHash<QString, QDateTime> m_requestAttempts;
    QHash<QString, WaitingRequest> m_waitingForBackupKey;
    bool m_crossSigningRequested = false;
    bool m_backupKeyRequestStarted = false;
    bool m_backupKeyAvailable = false;
    quint64 m_connectionGeneration = 0;

    void finishRequest(const QString &requestId, Quotient::Connection *connection);
    void failRequest(const QString &requestId, Quotient::Connection *connection, Quotient::Room *room,
                     const QString &sessionId, Failure failure, const QString &errorText);
    bool requestBackupKeyFromVerifiedDevice();
    void finishBackupKeyRequest();
};
