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

#include "avatars/account-avatar-service.h"

#include <QtCore/QPointer>

class QBuffer;
class QUrl;

namespace Quotient
{
class Connection;
}

class MatrixAccountAvatarService final : public AccountAvatarService
{
    Q_OBJECT

public:
    explicit MatrixAccountAvatarService(Account account, QObject *parent = nullptr);
    virtual ~MatrixAccountAvatarService() = default;

    void setConnection(Quotient::Connection *connection);
    virtual void upload(const QPixmap &avatar) override;
    virtual bool canRemove() override
    {
        return true;
    }

private:
    QPointer<Quotient::Connection> m_connection;
    QPointer<QBuffer> m_uploadBuffer;
    quint64 m_operationId = 0;
    bool m_uploading = false;

    quint64 startOperation();
    void uploadAvatar(const QPixmap &avatar, quint64 operationId);
    void setAvatarUrl(const QUrl &url, quint64 operationId);
    void finish(quint64 operationId, bool ok);
};
