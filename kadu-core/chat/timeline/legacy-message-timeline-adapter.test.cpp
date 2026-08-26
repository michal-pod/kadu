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

#include "chat/timeline/legacy-message-timeline-adapter.h"

#include "html/html-conversion.h"
#include "html/normalized-html-string.h"
#include "message/message-shared.h"
#include "message/message.h"
#include "message/sorted-messages.h"

#include <QtTest/QtTest>

class LegacyMessageTimelineAdapterTest : public QObject
{
    Q_OBJECT

private slots:
    void shouldMapSentMessage();
    void shouldCreateDeterministicIdWithoutLegacyId();
    void shouldMapSortedMessagesToPage();

private:
    Message makeMessage(const QString &id, const QDateTime &receivedAt) const;
};

Message LegacyMessageTimelineAdapterTest::makeMessage(const QString &id, const QDateTime &receivedAt) const
{
    Message message{new MessageShared{}};
    message.setId(id);
    message.setReceiveDate(receivedAt);
    message.setSendDate(receivedAt);
    message.setType(MessageTypeSent);
    message.setStatus(MessageStatusDelivered);
    message.setContent(normalizeHtml(plainToHtml(QStringLiteral("Legacy message"))));
    return message;
}

void LegacyMessageTimelineAdapterTest::shouldMapSentMessage()
{
    LegacyMessageTimelineAdapter adapter;
    const auto timelineItem = adapter.item(makeMessage(QStringLiteral("legacy-id"), QDateTime::fromSecsSinceEpoch(1000)));

    QCOMPARE(timelineItem.stableId, QStringLiteral("legacy:legacy-id"));
    QCOMPARE(static_cast<int>(timelineItem.kind), static_cast<int>(ChatTimelineItemKind::TextMessage));
    QVERIFY(timelineItem.sender.own);
    QCOMPARE(static_cast<int>(timelineItem.state.deliveryState), static_cast<int>(ChatTimelineDeliveryState::Delivered));
    QCOMPARE(timelineItem.content.plainText, QStringLiteral("Legacy message"));
}

void LegacyMessageTimelineAdapterTest::shouldCreateDeterministicIdWithoutLegacyId()
{
    LegacyMessageTimelineAdapter adapter;
    const auto message = makeMessage({}, QDateTime::fromSecsSinceEpoch(1000));

    const auto first = adapter.item(message);
    const auto second = adapter.item(message);
    QCOMPARE(first.stableId, second.stableId);
    QVERIFY(first.stableId.startsWith(QStringLiteral("legacy:")));
}

void LegacyMessageTimelineAdapterTest::shouldMapSortedMessagesToPage()
{
    LegacyMessageTimelineAdapter adapter;
    SortedMessages messages;
    messages.add(makeMessage(QStringLiteral("second"), QDateTime::fromSecsSinceEpoch(2000)));
    messages.add(makeMessage(QStringLiteral("first"), QDateTime::fromSecsSinceEpoch(1000)));

    const auto page = adapter.page(messages, QByteArrayLiteral("cursor"), true);
    QCOMPARE(page.items.size(), 2);
    QCOMPARE(page.cursor, QByteArrayLiteral("cursor"));
    QVERIFY(page.hasMore);
    QCOMPARE(page.items.at(0).stableId, QStringLiteral("legacy:first"));
}

QTEST_APPLESS_MAIN(LegacyMessageTimelineAdapterTest)
#include "legacy-message-timeline-adapter.test.moc"
