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

#include <QtCore/QFlags>

/**
 * @short Operations which a protocol may expose for a timeline event.
 *
 * The integer values form the small QML-facing action contract. They are kept
 * separate from ProtocolTimelineService so renderers and development tools can
 * use the contract without depending on account services.
 */
enum class ChatTimelineAction
{
    Reply = 0x1,
    Edit = 0x2,
    Delete = 0x4,
    SaveAttachment = 0x8,
    ShowSource = 0x10,
    Unpin = 0x20
};
Q_DECLARE_FLAGS(ChatTimelineActions, ChatTimelineAction)
Q_DECLARE_OPERATORS_FOR_FLAGS(ChatTimelineActions)
