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
class SSSSHandler;
}

class MatrixMegolmSessionRecovery final : public QObject
{
    Q_OBJECT

public:
    explicit MatrixMegolmSessionRecovery(QObject *parent = nullptr);

    void setConnection(Quotient::Connection *connection);
    void requestFromBackup(Quotient::Room *room, const Quotient::EncryptedEvent &event);

signals:
    void sessionRestored(Quotient::Room *room, const QString &sessionId);
    void backupRestored();

private:
    QPointer<Quotient::Connection> m_connection;
    QPointer<Quotient::SSSSHandler> m_crossSigningRecovery;
    QSet<QString> m_pendingRequests;
    QHash<QString, QDateTime> m_requestAttempts;
    bool m_crossSigningRequested = false;

    void finishRequest(const QString &requestId, Quotient::Connection *connection);
    void requestBackupKeyFromVerifiedDevice();
};
