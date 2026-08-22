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

#include "avatars/contact-avatar-service.h"

#include <QtCore/QHash>
#include <QtCore/QPointer>
#include <QtCore/QSet>
#include <QtCore/QUrl>

class QImage;

namespace Quotient
{
class Connection;
class User;
}

class MatrixContactAvatarService final : public ContactAvatarService
{
    Q_OBJECT

public:
    explicit MatrixContactAvatarService(Account account, QObject *parent = nullptr);
    virtual ~MatrixContactAvatarService();

    void setConnection(Quotient::Connection *connection);
    void observeContact(const QString &matrixId);
    virtual void download(const ContactAvatarId &id) override;

private:
    static constexpr auto AvatarSize = 96;

    QPointer<Quotient::Connection> m_connection;
    QHash<QString, QUrl> m_avatarUrls;
    QSet<Quotient::User *> m_observedUsers;

    static QByteArray avatarId(const QUrl &url);
    static QByteArray imageData(const QImage &image);
    void updateAvatar(const QString &matrixId);
    void setAvatarUrl(const QString &matrixId, const QUrl &url);
};
