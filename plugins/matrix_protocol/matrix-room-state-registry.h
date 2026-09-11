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

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QSet>

namespace Quotient
{
class Connection;
class Room;
}

class MatrixRoomStateRegistry final : public QObject
{
    Q_OBJECT

public:
    explicit MatrixRoomStateRegistry(QObject *parent = nullptr);

    void setConnection(Quotient::Connection *connection);
    bool isLoaded(const Quotient::Room *room) const;

signals:
    void roomLoaded(Quotient::Room *room);

private:
    QPointer<Quotient::Connection> m_connection;
    QSet<const Quotient::Room *> m_loadedRooms;
    QSet<Quotient::Room *> m_watchedRooms;

    void watchRoom(Quotient::Room *room);
    void markRoomLoaded(Quotient::Room *room);
};
