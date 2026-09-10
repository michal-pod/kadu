/*
 * %kadu copyright begin%
 * Copyright 2011 Piotr Galiszewski (piotr.galiszewski@kadu.im)
 * Copyright 2011 Bartosz Brachaczek (b.brachaczek@gmail.com)
 * Copyright 2011, 2013, 2014 Rafał Przemysław Malinowski (rafal.przemyslaw.malinowski@gmail.com)
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

#ifndef MULTILOGON_SERVICE_H
#define MULTILOGON_SERVICE_H

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QString>

#include "account-service.h"

#include "exports.h"

struct MultilogonSession;

class KADUAPI MultilogonService : public AccountService
{
    Q_OBJECT

public:
    explicit MultilogonService(Account account, QObject *parent);
    virtual ~MultilogonService();

    virtual const QList<MultilogonSession> &sessions() const = 0;
    virtual void killSession(MultilogonSession session) = 0;
    virtual void provideSessionKillPassword(
        MultilogonSession session, const QString &authenticationSession, const QString &password);
    virtual void refreshSessions();
    virtual bool canKillSession(const MultilogonSession &session) const;
    virtual bool supportsSessionVerification() const;
    virtual QString activityColumnTitle() const;

signals:
    void multilogonSessionAboutToBeConnected(MultilogonSession session);
    void multilogonSessionConnected(MultilogonSession session);
    void multilogonSessionAboutToBeDisconnected(MultilogonSession session);
    void multilogonSessionDisconnected(MultilogonSession session);
    void sessionsAboutToBeReset();
    void sessionsReset();
    void sessionsLoadingChanged(bool loading);
    void sessionsRefreshFailed(const QString &details);
    void sessionKillStarted(MultilogonSession session);
    void sessionKillFinished(MultilogonSession session);
    void sessionKillFailed(MultilogonSession session, const QString &details);
    void sessionKillPasswordRequired(MultilogonSession session, const QString &authenticationSession);
};

#endif   // MULTILOGON_SERVICE_H
