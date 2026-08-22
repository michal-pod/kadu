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

#include "matrix-contact-avatar-service.h"
#include "matrix-contact-avatar-service.moc"

#include "avatars/contact-avatar-id.h"

#include <Quotient/avatar.h>
#include <Quotient/connection.h>
#include <Quotient/user.h>

#include <QtCore/QBuffer>
#include <QtGui/QImage>

MatrixContactAvatarService::MatrixContactAvatarService(Account account, QObject *parent)
        : ContactAvatarService{account, parent}
{
}

MatrixContactAvatarService::~MatrixContactAvatarService()
{
    setConnection(nullptr);
}

void MatrixContactAvatarService::setConnection(Quotient::Connection *connection)
{
    if (m_connection == connection)
        return;

    for (auto *user : m_observedUsers)
        disconnect(user, nullptr, this, nullptr);

    m_connection = connection;
    m_observedUsers.clear();
    m_avatarUrls.clear();
}

void MatrixContactAvatarService::observeContact(const QString &matrixId)
{
    if (!m_connection || matrixId.isEmpty() || matrixId == m_connection->userId())
        return;

    auto *user = m_connection->user(matrixId);
    if (!user)
        return;

    if (!m_observedUsers.contains(user))
    {
        m_observedUsers.insert(user);
        connect(user, &Quotient::User::defaultAvatarChanged, this,
                [this, matrixId] { updateAvatar(matrixId); });
        connect(user, &QObject::destroyed, this,
                [this, user] { m_observedUsers.remove(user); });
        user->load();
    }

    updateAvatar(matrixId);
}

void MatrixContactAvatarService::download(const ContactAvatarId &id)
{
    if (!m_connection)
        return;

    const auto matrixId = QString::fromUtf8(id.contact.value);
    const auto url = m_avatarUrls.value(matrixId);
    if (url.isEmpty() || avatarId(url) != id.id)
        return;

    const QPointer<MatrixContactAvatarService> service{this};
    const auto image = m_connection->userAvatar(url).get(AvatarSize, [service, id] {
        if (service)
            service->download(id);
    });
    if (!image.isNull())
        emit downloaded(id, imageData(image));
}

QByteArray MatrixContactAvatarService::avatarId(const QUrl &url)
{
    return url.toEncoded();
}

QByteArray MatrixContactAvatarService::imageData(const QImage &image)
{
    QByteArray content;
    QBuffer buffer{&content};
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return content;
}

void MatrixContactAvatarService::updateAvatar(const QString &matrixId)
{
    if (!m_connection)
        return;

    if (auto *user = m_connection->user(matrixId))
        setAvatarUrl(matrixId, user->avatarUrl());
}

void MatrixContactAvatarService::setAvatarUrl(const QString &matrixId, const QUrl &url)
{
    const auto contactId = ContactId{matrixId.toUtf8()};
    if (!Quotient::Avatar::isUrlValid(url))
    {
        if (m_avatarUrls.remove(matrixId))
            emit removed(contactId);
        return;
    }

    if (m_avatarUrls.value(matrixId) == url)
        return;

    m_avatarUrls.insert(matrixId, url);
    emit available({contactId, avatarId(url)});
}
