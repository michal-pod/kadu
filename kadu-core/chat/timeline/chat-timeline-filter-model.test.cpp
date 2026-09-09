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

#include "chat/timeline/chat-timeline-filter-model.h"

#include <QtTest/QtTest>

class ChatTimelineFilterModelTest : public QObject
{
    Q_OBJECT

private slots:
    void shouldFilterLevelsCumulatively();
    void shouldKeepHiddenItemsAvailableById();
    void shouldRegroupMessagesAcrossHiddenEvents();
    void shouldReactWhenAnItemLevelChanges();

private:
    ChatTimelineItem makeItem(const QString &stableId, const QByteArray &sourceOrder,
                              ChatTimelineItemLevel level) const;
};

ChatTimelineItem ChatTimelineFilterModelTest::makeItem(const QString &stableId, const QByteArray &sourceOrder,
                                                       ChatTimelineItemLevel level) const
{
    ChatTimelineItem item;
    item.stableId = stableId;
    item.sourceOrder = sourceOrder;
    item.timestamp = QDateTime::fromSecsSinceEpoch(1000 + sourceOrder.toLongLong());
    item.sender.id = QStringLiteral("@alice:example.org");
    item.content.plainText = stableId;
    item.level = level;
    return item;
}

void ChatTimelineFilterModelTest::shouldFilterLevelsCumulatively()
{
    ChatTimelineModel source;
    ChatTimelineFilterModel model{&source};
    source.reset({makeItem(QStringLiteral("$chat"), QByteArrayLiteral("001"), ChatTimelineItemLevel::Chat),
                  makeItem(QStringLiteral("$important"), QByteArrayLiteral("002"),
                           ChatTimelineItemLevel::Important),
                  makeItem(QStringLiteral("$information"), QByteArrayLiteral("003"),
                           ChatTimelineItemLevel::Informational),
                  makeItem(QStringLiteral("$debug"), QByteArrayLiteral("004"), ChatTimelineItemLevel::Debug)});

    QCOMPARE(model.rowCount(), 3);
    model.setDetails(ChatTimelineDetails::ChatOnly);
    QCOMPARE(model.rowCount(), 1);
    model.setDetails(ChatTimelineDetails::Important);
    QCOMPARE(model.rowCount(), 2);
    model.setDetails(ChatTimelineDetails::AllEvents);
    QCOMPARE(model.rowCount(), 3);
    model.setDetails(ChatTimelineDetails::Debug);
    QCOMPARE(model.rowCount(), 4);
}

void ChatTimelineFilterModelTest::shouldKeepHiddenItemsAvailableById()
{
    ChatTimelineModel source;
    ChatTimelineFilterModel model{&source};
    source.upsert(
        makeItem(QStringLiteral("$debug"), QByteArrayLiteral("001"), ChatTimelineItemLevel::Debug));

    QVERIFY(model.contains(QStringLiteral("$debug")));
    QCOMPARE(model.rowForStableId(QStringLiteral("$debug")), -1);
    QCOMPARE(model.item(QStringLiteral("$debug")).stableId, QStringLiteral("$debug"));
}

void ChatTimelineFilterModelTest::shouldRegroupMessagesAcrossHiddenEvents()
{
    ChatTimelineModel source;
    ChatTimelineFilterModel model{&source};
    source.reset({makeItem(QStringLiteral("$first"), QByteArrayLiteral("001"), ChatTimelineItemLevel::Chat),
                  makeItem(QStringLiteral("$hidden"), QByteArrayLiteral("002"),
                           ChatTimelineItemLevel::Informational),
                  makeItem(QStringLiteral("$last"), QByteArrayLiteral("003"), ChatTimelineItemLevel::Chat)});
    model.setDetails(ChatTimelineDetails::ChatOnly);

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.data(model.index(0, 0), ChatTimelineModel::GroupPositionRole).toInt(),
             static_cast<int>(ChatTimelineModel::GroupPosition::First));
    QCOMPARE(model.data(model.index(1, 0), ChatTimelineModel::GroupPositionRole).toInt(),
             static_cast<int>(ChatTimelineModel::GroupPosition::Last));
}

void ChatTimelineFilterModelTest::shouldReactWhenAnItemLevelChanges()
{
    ChatTimelineModel source;
    ChatTimelineFilterModel model{&source};
    model.setDetails(ChatTimelineDetails::ChatOnly);
    auto item = makeItem(QStringLiteral("$event"), QByteArrayLiteral("001"), ChatTimelineItemLevel::Debug);
    source.upsert(item);
    QCOMPARE(model.rowCount(), 0);

    item.level = ChatTimelineItemLevel::Chat;
    item.revision = 1;
    source.upsert(item);
    QCOMPARE(model.rowCount(), 1);
}

QTEST_APPLESS_MAIN(ChatTimelineFilterModelTest)
#include "chat-timeline-filter-model.test.moc"
