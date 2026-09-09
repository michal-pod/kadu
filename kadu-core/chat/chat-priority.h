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

#include <QtCore/QMetaType>
#include <QtCore/QtGlobal>

enum class ChatPriority : qint32
{
    LowPriority = -100,
    Default = 0,
    Favorite = 100
};

// Priority levels intentionally leave room for future protocol mappings.
// The order component is a descending tie-breaker within one level; zero
// means that a protocol did not provide an explicit order.
constexpr qint64 CHAT_PRIORITY_ORDER_SCALE = 1'000'000;
constexpr quint32 CHAT_PRIORITY_ORDER_MAXIMUM = 999'999;

inline qint64 chatSortingPriority(ChatPriority priority, quint32 priorityOrder)
{
    return static_cast<qint64>(priority) * CHAT_PRIORITY_ORDER_SCALE +
           qMin(priorityOrder, CHAT_PRIORITY_ORDER_MAXIMUM);
}

Q_DECLARE_METATYPE(ChatPriority)
