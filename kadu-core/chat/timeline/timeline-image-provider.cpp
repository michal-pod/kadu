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

#include "timeline-image-provider.h"

#include "protocols/services/protocol-timeline-service.h"

#include <QtCore/QUrl>
#include <QtGui/QImage>

TimelineImageProvider::TimelineImageProvider(Chat chat, ProtocolTimelineService *timelineService)
        : QQuickImageProvider{QQuickImageProvider::Image}, m_chat{chat}, m_timelineService{timelineService}
{
}

QImage TimelineImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    if (!m_timelineService)
        return placeholderImage();

    auto image = m_timelineService->requestAttachmentImage(m_chat, sourceUri(id), requestedSize);
    if (image.isNull())
        return placeholderImage();

    if (requestedSize.isValid())
        image = image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (size)
        *size = image.size();
    return image;
}

QUrl TimelineImageProvider::sourceUri(const QString &id)
{
    const auto encodedUri = id.section(u'?', 0, 0).toUtf8();
    return QUrl{QString::fromUtf8(QByteArray::fromPercentEncoding(encodedUri))};
}

QImage TimelineImageProvider::placeholderImage()
{
    QImage image{1, 1, QImage::Format_ARGB32_Premultiplied};
    image.fill(Qt::transparent);
    return image;
}
