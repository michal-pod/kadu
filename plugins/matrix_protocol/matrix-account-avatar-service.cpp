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

#include "matrix-account-avatar-service.h"
#include "matrix-account-avatar-service.moc"

#include "avatars/avatars.h"

#include <Quotient/connection.h>
#include <Quotient/csapi/profile.h>

#include <QtCore/QBuffer>
#include <QtGui/QPixmap>

MatrixAccountAvatarService::MatrixAccountAvatarService(Account account, QObject *parent)
        : AccountAvatarService{account, parent}
{
}

void MatrixAccountAvatarService::setConnection(Quotient::Connection *connection)
{
    if (m_connection == connection)
        return;

    finish(m_operationId, false);
    m_connection = connection;
}

void MatrixAccountAvatarService::upload(const QPixmap &avatar)
{
    if (!m_connection || !m_connection->isLoggedIn() || m_uploading)
    {
        emit finished(false);
        return;
    }

    const auto operationId = startOperation();
    if (avatar.isNull())
    {
        setAvatarUrl({}, operationId);
        return;
    }

    uploadAvatar(avatar, operationId);
}

quint64 MatrixAccountAvatarService::startOperation()
{
    m_uploading = true;
    return ++m_operationId;
}

void MatrixAccountAvatarService::uploadAvatar(const QPixmap &avatar, quint64 operationId)
{
    m_uploadBuffer = new QBuffer{this};
    if (!m_uploadBuffer->open(QIODevice::WriteOnly) ||
        !avatar.scaled(AVATAR_SIZE, AVATAR_SIZE, Qt::KeepAspectRatio, Qt::SmoothTransformation)
             .save(m_uploadBuffer, "PNG"))
    {
        finish(operationId, false);
        return;
    }

    m_uploadBuffer->close();
    if (!m_uploadBuffer->open(QIODevice::ReadOnly))
    {
        finish(operationId, false);
        return;
    }

    const QPointer<Quotient::Connection> connection{m_connection};
    connection->uploadContent(m_uploadBuffer, QStringLiteral("avatar.png"), QStringLiteral("image/png"))
        .then(this, [this, connection, operationId](const QUrl &url) {
            if (!connection || connection.data() != m_connection.data())
            {
                finish(operationId, false);
                return;
            }

            setAvatarUrl(url, operationId);
        }, [this, operationId] { finish(operationId, false); });
}

void MatrixAccountAvatarService::setAvatarUrl(const QUrl &url, quint64 operationId)
{
    if (!m_connection)
    {
        finish(operationId, false);
        return;
    }

    m_connection->callApi<Quotient::SetAvatarUrlJob>(m_connection->userId(), url)
        .then(this, [this, operationId] { finish(operationId, true); },
              [this, operationId] { finish(operationId, false); });
}

void MatrixAccountAvatarService::finish(quint64 operationId, bool ok)
{
    if (!m_uploading || operationId != m_operationId)
        return;

    m_uploading = false;
    if (m_uploadBuffer)
    {
        m_uploadBuffer->deleteLater();
        m_uploadBuffer = nullptr;
    }
    emit finished(ok);
}
