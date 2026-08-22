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

#include "matrix-account-data.h"

#include "accounts/account-shared.h"

MatrixAccountData::MatrixAccountData(AccountShared *data) : m_data{data}
{
}

QString MatrixAccountData::homeserver() const
{
    m_data->ensureLoaded();
    return m_data->isValidStorage() ? m_data->loadValue<QString>("Homeserver", "https://matrix.org")
                                   : QString{"https://matrix.org"};
}

void MatrixAccountData::setHomeserver(const QString &homeserver) const
{
    m_data->ensureLoaded();
    if (m_data->isValidStorage())
        m_data->storeValue("Homeserver", homeserver);
}

QString MatrixAccountData::deviceId() const
{
    m_data->ensureLoaded();
    return m_data->isValidStorage() ? m_data->loadValue<QString>("DeviceId", QString{}) : QString{};
}

void MatrixAccountData::setDeviceId(const QString &deviceId) const
{
    m_data->ensureLoaded();
    if (m_data->isValidStorage())
        m_data->storeValue("DeviceId", deviceId);
}
