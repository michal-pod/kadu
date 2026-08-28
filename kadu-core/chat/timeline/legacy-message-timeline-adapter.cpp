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

#include "legacy-message-timeline-adapter.h"

#include "contacts/contact.h"
#include "html/html-conversion.h"
#include "html/html-string.h"
#include "html/normalized-html-string.h"
#include "html/sanitized-html-string.h"
#include "message/message.h"
#include "message/sorted-messages.h"

#include <QtCore/QCryptographicHash>

ChatTimelineItem LegacyMessageTimelineAdapter::item(const Message &message) const
{
    ChatTimelineItem timelineItem;
    timelineItem.stableId = stableId(message);
    timelineItem.sourceOrder = sourceOrder(message);
    timelineItem.timestamp = message.receiveDate().isValid() ? message.receiveDate() : message.sendDate();
    timelineItem.kind = message.type() == MessageTypeSystem ? ChatTimelineItemKind::LocalNotice
                                                            : ChatTimelineItemKind::TextMessage;
    timelineItem.content.formattedText = sanitizeHtml(HtmlString{message.content().string()}).string();
    timelineItem.content.plainText = htmlToPlain(message.content());

    timelineItem.sender.own = message.type() == MessageTypeSent;
    if (timelineItem.sender.own)
    {
        timelineItem.sender.id = QStringLiteral("legacy:self");
    }
    else if (!message.messageSender().isNull())
    {
        timelineItem.sender.id = message.messageSender().id();
        timelineItem.sender.displayName = message.messageSender().display(true);
    }
    else
    {
        timelineItem.sender.id = QStringLiteral("legacy:unknown");
    }

    if (message.status() == MessageStatusDelivered)
        timelineItem.state.deliveryState = ChatTimelineDeliveryState::Delivered;
    else if (message.status() == MessageStatusWontDeliver)
        timelineItem.state.deliveryState = ChatTimelineDeliveryState::Failed;
    else if (timelineItem.sender.own)
        timelineItem.state.deliveryState = ChatTimelineDeliveryState::Sent;

    return timelineItem;
}

ChatTimelinePage LegacyMessageTimelineAdapter::page(const SortedMessages &messages, const QByteArray &cursor,
                                                    bool hasMore) const
{
    ChatTimelinePage timelinePage;
    timelinePage.cursor = cursor;
    timelinePage.hasMore = hasMore;
    timelinePage.items.reserve(static_cast<qsizetype>(messages.size()));
    for (const auto &message : messages)
        timelinePage.items.append(item(message));
    return timelinePage;
}

QString LegacyMessageTimelineAdapter::stableId(const Message &message)
{
    if (!message.id().isEmpty())
        return QStringLiteral("legacy:%1").arg(message.id());

    QByteArray fingerprint;
    fingerprint.append(message.receiveDate().toUTC().toString(Qt::ISODateWithMs).toUtf8());
    fingerprint.append('\0');
    fingerprint.append(message.sendDate().toUTC().toString(Qt::ISODateWithMs).toUtf8());
    fingerprint.append('\0');
    fingerprint.append(QByteArray::number(message.type()));
    fingerprint.append('\0');
    fingerprint.append(message.content().string().toUtf8());
    if (!message.messageSender().isNull())
    {
        fingerprint.append('\0');
        fingerprint.append(message.messageSender().id().toUtf8());
    }
    const auto digest = QCryptographicHash::hash(fingerprint, QCryptographicHash::Sha256).toHex();
    return QStringLiteral("legacy:%1").arg(QString::fromLatin1(digest));
}

QByteArray LegacyMessageTimelineAdapter::sourceOrder(const Message &message)
{
    const auto timestamp = message.receiveDate().isValid() ? message.receiveDate() : message.sendDate();
    return timestamp.toUTC().toString(Qt::ISODateWithMs).toUtf8() + ':' + stableId(message).toUtf8();
}
