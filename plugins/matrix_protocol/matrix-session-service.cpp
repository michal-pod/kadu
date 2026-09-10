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

#include "matrix-session-service.h"
#include "matrix-session-service.moc"

#include <Quotient/connection.h>
#include <Quotient/csapi/device_management.h>

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QVariantMap>

#include <algorithm>
#include <optional>
#include <utility>

MatrixSessionService::MatrixSessionService(Account account, QObject *parent)
        : MultilogonService{account, parent}
{
}

MatrixSessionService::~MatrixSessionService()
{
}

void MatrixSessionService::setConnection(Quotient::Connection *connection)
{
    if (m_connection == connection)
        return;

    if (m_connection)
        disconnect(m_connection, nullptr, this, nullptr);

    const auto interruptedSessions = m_disconnectingSessions;
    m_disconnectingSessions.clear();
    m_sessionsAwaitingPassword.clear();
    for (const auto &session : interruptedSessions)
        emit sessionKillFailed(session, tr("The connection changed."));

    m_connection = connection;
    setLoading(false);
    clearSessions();

    if (!m_connection)
        return;

    connect(m_connection, &Quotient::Connection::connected, this, &MatrixSessionService::refreshSessions);
    connect(m_connection, &Quotient::Connection::loggedOut, this, &MatrixSessionService::connectionLoggedOut);
    connect(m_connection, &Quotient::Connection::devicesListLoaded, this,
            &MatrixSessionService::updateVerificationStates);
    connect(m_connection, &Quotient::Connection::finishedQueryingKeys, this,
            &MatrixSessionService::updateVerificationStates);
    connect(m_connection, &Quotient::Connection::sessionVerified, this,
            [this](const QString &userId, const QString &) {
                if (m_connection && userId == m_connection->userId())
                    updateVerificationStates();
            });
    connect(m_connection, &Quotient::Connection::userVerified, this,
            [this](const QString &userId) {
                if (m_connection && userId == m_connection->userId())
                    updateVerificationStates();
            });
    connect(m_connection, &QObject::destroyed, this, [this] {
        setLoading(false);
        clearSessions();
    });

    if (m_connection->isLoggedIn())
        refreshSessions();
}

const QList<MultilogonSession> &MatrixSessionService::sessions() const
{
    return m_sessions;
}

void MatrixSessionService::killSession(MultilogonSession session)
{
    if (!canKillSession(session))
        return;

    m_disconnectingSessions.insert(session.id, session);
    emit sessionKillStarted(session);
    deleteSession(session);
}

void MatrixSessionService::provideSessionKillPassword(
    MultilogonSession session, const QString &authenticationSession, const QString &password)
{
    if (!m_disconnectingSessions.contains(session.id))
        return;
    m_sessionsAwaitingPassword.remove(session.id);
    if (password.isEmpty())
    {
        failSessionDeletion(session, tr("Authentication was cancelled."));
        return;
    }

    deleteSession(session, authenticationSession, password);
}

void MatrixSessionService::refreshSessions()
{
    if (m_loading)
        return;

    if (!m_connection || !m_connection->isLoggedIn())
    {
        emit sessionsRefreshFailed(tr("The account is not connected."));
        return;
    }

    setLoading(true);
    const QPointer<Quotient::Connection> requestedConnection{m_connection};
    auto job = requestedConnection->callApi<Quotient::GetDevicesJob>();
    connect(job, &Quotient::BaseJob::success, this, [this, job, requestedConnection] {
        if (!requestedConnection || requestedConnection.data() != m_connection.data())
            return;

        QList<MultilogonSession> sessions;
        const auto currentDeviceId = requestedConnection->deviceId();
        for (const auto &device : job->devices())
        {
            const auto activityTime = device.lastSeenTs
                                          ? QDateTime::fromMSecsSinceEpoch(*device.lastSeenTs, Qt::UTC).toLocalTime()
                                          : QDateTime{};
            const auto name = device.displayName.isEmpty()
                                  ? device.deviceId
                                  : QStringLiteral("%1 (%2)").arg(device.displayName, device.deviceId);
            sessions.append(MultilogonSession{
                account(), device.deviceId.toUtf8(), name,
                device.lastSeenIp, activityTime, device.deviceId == currentDeviceId,
                verificationState(device.deviceId)});
        }

        std::sort(sessions.begin(), sessions.end(), [](const auto &left, const auto &right) {
            if (left.current != right.current)
                return left.current;
            return left.activityTime > right.activityTime;
        });

        emit sessionsAboutToBeReset();
        m_sessions = std::move(sessions);
        emit sessionsReset();
        setLoading(false);
    });
    connect(job, &Quotient::BaseJob::failure, this, [this, job, requestedConnection] {
        if (!requestedConnection || requestedConnection.data() != m_connection.data())
            return;

        setLoading(false);
        emit sessionsRefreshFailed(job->errorString());
    });
}

bool MatrixSessionService::canKillSession(const MultilogonSession &session) const
{
    return m_connection && m_connection->isLoggedIn() && session != MultilogonSession{} && !session.current
           && !m_disconnectingSessions.contains(session.id);
}

bool MatrixSessionService::supportsSessionVerification() const
{
    return true;
}

QString MatrixSessionService::activityColumnTitle() const
{
    return tr("Last activity");
}

void MatrixSessionService::deleteSession(
    const MultilogonSession &session, const QString &authenticationSession, const QString &password)
{
    if (!m_connection || !m_connection->isLoggedIn())
    {
        failSessionDeletion(session, tr("The account is not connected."));
        return;
    }

    std::optional<Quotient::AuthenticationData> authentication;
    if (!authenticationSession.isEmpty())
    {
        authentication.emplace();
        authentication->type = QStringLiteral("m.login.password");
        authentication->session = authenticationSession;
        authentication->authInfo.insert(
            QStringLiteral("identifier"),
            QVariantMap{{QStringLiteral("type"), QStringLiteral("m.id.user")},
                        {QStringLiteral("user"), account().id()}});
        authentication->authInfo.insert(QStringLiteral("password"), password);
    }

    const QPointer<Quotient::Connection> requestedConnection{m_connection};
    auto job = requestedConnection->callApi<Quotient::DeleteDeviceJob>(
        QString::fromUtf8(session.id), authentication);
    connect(job, &Quotient::BaseJob::success, this, [this, session, requestedConnection] {
        if (!requestedConnection || requestedConnection.data() != m_connection.data())
            return;
        finishSessionDeletion(session);
    });
    connect(job, &Quotient::BaseJob::failure, this,
            [this, job, session, authenticationSession, requestedConnection] {
                if (!requestedConnection || requestedConnection.data() != m_connection.data())
                    return;

                if (job->error() == Quotient::BaseJob::Unauthorised)
                {
                    const auto response = job->jsonData();
                    const auto sessionId = response.value(QStringLiteral("session")).toString();
                    bool passwordCompleted = false;
                    for (const auto &completedStage : response.value(QStringLiteral("completed")).toArray())
                        if (completedStage.toString() == QStringLiteral("m.login.password"))
                        {
                            passwordCompleted = true;
                            break;
                        }
                    bool passwordSupported = false;
                    for (const auto &flowValue : response.value(QStringLiteral("flows")).toArray())
                    {
                        const auto stages = flowValue.toObject().value(QStringLiteral("stages")).toArray();
                        for (const auto &stage : stages)
                            if (stage.toString() == QStringLiteral("m.login.password"))
                            {
                                passwordSupported = true;
                                break;
                            }
                        if (passwordSupported)
                            break;
                    }

                    if (!passwordCompleted && passwordSupported && !sessionId.isEmpty())
                    {
                        m_sessionsAwaitingPassword.insert(session.id);
                        emit sessionKillPasswordRequired(session, sessionId);
                        if (m_sessionsAwaitingPassword.remove(session.id))
                            failSessionDeletion(
                                session, tr("The account password is required to disconnect this session."));
                        return;
                    }

                    failSessionDeletion(
                        session, tr("The server requires an unsupported interactive authentication method."));
                    return;
                }

                failSessionDeletion(session, job->errorString());
            });
}

void MatrixSessionService::finishSessionDeletion(const MultilogonSession &session)
{
    m_disconnectingSessions.remove(session.id);
    m_sessionsAwaitingPassword.remove(session.id);

    const auto index = std::find_if(m_sessions.cbegin(), m_sessions.cend(), [&session](const auto &candidate) {
        return candidate.id == session.id;
    });
    if (index != m_sessions.cend())
    {
        emit sessionsAboutToBeReset();
        m_sessions.erase(index);
        emit sessionsReset();
    }

    emit sessionKillFinished(session);
}

void MatrixSessionService::failSessionDeletion(const MultilogonSession &session, const QString &details)
{
    m_disconnectingSessions.remove(session.id);
    m_sessionsAwaitingPassword.remove(session.id);
    emit sessionKillFailed(session, details);
}

MultilogonSessionVerificationState MatrixSessionService::verificationState(const QString &deviceId) const
{
    if (!m_connection || !m_connection->encryptionEnabled())
        return MultilogonSessionVerificationState::NotAvailable;
    if (deviceId == m_connection->deviceId()
        || m_connection->isVerifiedDevice(m_connection->userId(), deviceId))
        return MultilogonSessionVerificationState::Verified;
    if (m_connection->isKnownE2eeCapableDevice(m_connection->userId(), deviceId))
        return MultilogonSessionVerificationState::Unverified;
    return MultilogonSessionVerificationState::Unknown;
}

void MatrixSessionService::updateVerificationStates()
{
    if (!m_connection || m_sessions.isEmpty())
        return;

    auto sessions = m_sessions;
    bool changed = false;
    for (auto &session : sessions)
    {
        const auto state = verificationState(QString::fromUtf8(session.id));
        if (session.verificationState == state)
            continue;
        session.verificationState = state;
        changed = true;
    }

    if (!changed)
        return;

    emit sessionsAboutToBeReset();
    m_sessions = std::move(sessions);
    emit sessionsReset();
}

void MatrixSessionService::clearSessions()
{
    if (m_sessions.isEmpty())
        return;

    emit sessionsAboutToBeReset();
    m_sessions.clear();
    emit sessionsReset();
}

void MatrixSessionService::connectionLoggedOut()
{
    setLoading(false);
    const auto interruptedSessions = m_disconnectingSessions;
    m_disconnectingSessions.clear();
    m_sessionsAwaitingPassword.clear();
    for (const auto &session : interruptedSessions)
        emit sessionKillFailed(session, tr("The account was disconnected."));
    clearSessions();
}

void MatrixSessionService::setLoading(bool loading)
{
    if (m_loading == loading)
        return;

    m_loading = loading;
    emit sessionsLoadingChanged(loading);
}
