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

#include "protocol-timeline-service.h"
#include "protocol-timeline-service.moc"

#include <QtGui/QImage>

ProtocolTimelineService::ProtocolTimelineService(Account account, QObject *parent) : AccountService{account, parent}
{
}

ProtocolTimelineService::~ProtocolTimelineService()
{
}

ChatTimelineActions ProtocolTimelineService::availableActions(const Chat &, const QString &) const
{
    return {};
}

bool ProtocolTimelineService::executeAction(const Chat &, const QString &, ChatTimelineAction)
{
    return false;
}

bool ProtocolTimelineService::removeOwnReaction(const Chat &, const QString &, const QString &)
{
    return false;
}

bool ProtocolTimelineService::addReaction(const Chat &, const QString &, const QString &)
{
    return false;
}

QVariantList ProtocolTimelineService::pinnedMessages(const Chat &) const
{
    return {};
}

QString ProtocolTimelineService::chatHeaderTitle(const Chat &) const
{
    return {};
}

void ProtocolTimelineService::markTimelineItemRead(const Chat &, const QString &)
{
}

QImage ProtocolTimelineService::requestAttachmentImage(const Chat &, const QUrl &, const QSize &)
{
    return {};
}
