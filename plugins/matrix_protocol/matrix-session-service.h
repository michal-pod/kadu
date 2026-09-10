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

#include "multilogon/multilogon-session.h"
#include "protocols/services/multilogon-service.h"

#include <QtCore/QHash>
#include <QtCore/QPointer>
#include <QtCore/QSet>

namespace Quotient
{
class Connection;
}

class MatrixSessionService final : public MultilogonService
{
    Q_OBJECT

public:
    explicit MatrixSessionService(Account account, QObject *parent = nullptr);
    virtual ~MatrixSessionService();

    void setConnection(Quotient::Connection *connection);

    virtual const QList<MultilogonSession> &sessions() const override;
    virtual void killSession(MultilogonSession session) override;
    virtual void provideSessionKillPassword(
        MultilogonSession session, const QString &authenticationSession, const QString &password) override;
    virtual void refreshSessions() override;
    virtual bool canKillSession(const MultilogonSession &session) const override;
    virtual bool supportsSessionVerification() const override;
    virtual QString activityColumnTitle() const override;

private:
    QPointer<Quotient::Connection> m_connection;
    QList<MultilogonSession> m_sessions;
    QHash<QByteArray, MultilogonSession> m_disconnectingSessions;
    QSet<QByteArray> m_sessionsAwaitingPassword;
    bool m_loading{false};

    void clearSessions();
    void connectionLoggedOut();
    void deleteSession(
        const MultilogonSession &session, const QString &authenticationSession = {}, const QString &password = {});
    void finishSessionDeletion(const MultilogonSession &session);
    void failSessionDeletion(const MultilogonSession &session, const QString &details);
    MultilogonSessionVerificationState verificationState(const QString &deviceId) const;
    void updateVerificationStates();
    void setLoading(bool loading);
};
