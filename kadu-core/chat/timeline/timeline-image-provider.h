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

#include "chat/chat.h"

#include <QtCore/QPointer>
#include <QtCore/QUrl>
#include <QtGui/QImage>
#include <QtQuick/QQuickImageProvider>

class ProtocolTimelineService;

/**
 * @short Bridges QML image://kaduimg requests to the active protocol timeline.
 *
 * A provider belongs to one chat view, so the URI only identifies a resource
 * within that chat. Protocols keep remote locations and encryption metadata
 * private and return a decoded QImage only after it has been loaded.
 */
class KADUAPI TimelineImageProvider final : public QQuickImageProvider
{
public:
    TimelineImageProvider(Chat chat, ProtocolTimelineService *timelineService);

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    Chat m_chat;
    QPointer<ProtocolTimelineService> m_timelineService;

    static QUrl sourceUri(const QString &id);
    static QImage placeholderImage();
};
