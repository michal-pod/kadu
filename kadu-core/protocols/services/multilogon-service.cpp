/*
 * %kadu copyright begin%
 * Copyright 2016 Rafał Przemysław Malinowski (rafal.przemyslaw.malinowski@gmail.com)
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

#include "multilogon-service.h"
#include "multilogon-service.moc"

#include "multilogon/multilogon-session.h"

MultilogonService::MultilogonService(Account account, QObject *parent) : AccountService{account, parent}
{
}

MultilogonService::~MultilogonService()
{
}

void MultilogonService::provideSessionKillPassword(
    MultilogonSession session, const QString &authenticationSession, const QString &password)
{
    Q_UNUSED(authenticationSession)
    Q_UNUSED(password)
    emit sessionKillFailed(session, tr("Password authentication is not supported by this protocol."));
}

void MultilogonService::refreshSessions()
{
}

bool MultilogonService::sessionsLoading() const
{
    return false;
}

bool MultilogonService::canKillSession(const MultilogonSession &session) const
{
    return session != MultilogonSession{} && !session.current;
}

bool MultilogonService::supportsSessionVerification() const
{
    return false;
}

bool MultilogonService::canVerifySession(const MultilogonSession &) const
{
    return false;
}

void MultilogonService::verifySession(const MultilogonSession &)
{
}

QString MultilogonService::activityColumnTitle() const
{
    return tr("Logon time");
}
