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

#include "matrix-room-state-registry.h"

#include <Quotient/connection.h>
#include <Quotient/room.h>

#include <utility>

MatrixRoomStateRegistry::MatrixRoomStateRegistry(QObject *parent) : QObject{parent}
{
}

void MatrixRoomStateRegistry::setConnection(Quotient::Connection *connection)
{
    if (m_connection == connection)
        return;

    if (m_connection)
        disconnect(m_connection, nullptr, this, nullptr);
    for (auto *room : std::as_const(m_watchedRooms))
        disconnect(room, nullptr, this, nullptr);

    m_connection = connection;
    m_loadedRooms.clear();
    m_watchedRooms.clear();
    if (!m_connection)
        return;

    connect(m_connection, &Quotient::Connection::newRoom, this, &MatrixRoomStateRegistry::watchRoom);
    connect(m_connection, &Quotient::Connection::loadedRoomState, this, &MatrixRoomStateRegistry::markRoomLoaded);

    for (auto *room : m_connection->allRooms())
    {
        watchRoom(room);
        // libQuotient does not expose a base-state-loaded property. A room
        // with current state has necessarily completed its first update, so
        // this recovers rooms that were loaded before this registry attached.
        if (room && !room->currentState().events().isEmpty())
            markRoomLoaded(room);
    }
}

bool MatrixRoomStateRegistry::isLoaded(const Quotient::Room *room) const
{
    return room && m_loadedRooms.contains(room);
}

void MatrixRoomStateRegistry::watchRoom(Quotient::Room *room)
{
    if (!room || m_watchedRooms.contains(room))
        return;

    m_watchedRooms.insert(room);
    connect(room, &QObject::destroyed, this, [this, room] {
        m_loadedRooms.remove(room);
        m_watchedRooms.remove(room);
    });
}

void MatrixRoomStateRegistry::markRoomLoaded(Quotient::Room *room)
{
    if (!room || m_loadedRooms.contains(room))
        return;

    watchRoom(room);
    m_loadedRooms.insert(room);
    emit roomLoaded(room);
}
