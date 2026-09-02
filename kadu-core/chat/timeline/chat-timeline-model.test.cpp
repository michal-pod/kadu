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

#include "chat/timeline/chat-timeline-model.h"

#include <QtTest/QtTest>

class ChatTimelineModelTest : public QObject
{
    Q_OBJECT

private slots:
    void shouldKeepSourceOrderWhenItemsArriveOutOfOrder();
    void shouldReplaceLocalEchoWithServerEvent();
    void shouldMergeLocalEchoWhenServerEventArrivesThroughUpsert();
    void shouldIgnoreAnOlderRevision();
    void shouldEmitDataChangedOnlyForUpdatedItem();
    void shouldEmitOnlyChangedRoles();
    void shouldInsertTimelinePagesInSingleBatches();
    void shouldKeepNewestRevisionWhenResetting();
    void shouldRedactExistingItem();
    void shouldExposeLocationUri();

private:
    ChatTimelineItem makeItem(const QString &stableId, const QByteArray &sourceOrder, quint64 revision = 0) const;
};

ChatTimelineItem ChatTimelineModelTest::makeItem(const QString &stableId, const QByteArray &sourceOrder, quint64 revision) const
{
    ChatTimelineItem timelineItem;
    timelineItem.stableId = stableId;
    timelineItem.sourceOrder = sourceOrder;
    timelineItem.revision = revision;
    timelineItem.timestamp = QDateTime::fromSecsSinceEpoch(1000);
    timelineItem.content.plainText = stableId;
    ChatTimelineSender sender;
    sender.id = QStringLiteral("@alice:example.org");
    sender.displayName = QStringLiteral("Alicja");
    timelineItem.sender = sender;
    return timelineItem;
}

void ChatTimelineModelTest::shouldKeepSourceOrderWhenItemsArriveOutOfOrder()
{
    ChatTimelineModel model;
    model.upsert(makeItem(QStringLiteral("second"), QByteArrayLiteral("002")));
    model.upsert(makeItem(QStringLiteral("first"), QByteArrayLiteral("001")));

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.data(model.index(0, 0), ChatTimelineModel::StableIdRole).toString(), QStringLiteral("first"));
    QCOMPARE(model.data(model.index(1, 0), ChatTimelineModel::StableIdRole).toString(), QStringLiteral("second"));
}

void ChatTimelineModelTest::shouldReplaceLocalEchoWithServerEvent()
{
    ChatTimelineModel model;
    auto localEcho = makeItem(QStringLiteral("local:transaction"), QByteArrayLiteral("temporary"));
    localEcho.transactionId = QStringLiteral("transaction");
    model.upsert(localEcho);

    auto serverItem = makeItem(QStringLiteral("$server-event"), QByteArrayLiteral("002"));
    serverItem.content.plainText = QStringLiteral("confirmed");
    model.replaceLocalEcho(QStringLiteral("transaction"), serverItem);

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.rowForStableId(QStringLiteral("$server-event")), 0);
    QCOMPARE(model.rowForTransactionId(QStringLiteral("transaction")), 0);
    QCOMPARE(model.data(model.index(0, 0), ChatTimelineModel::PlainTextRole).toString(), QStringLiteral("confirmed"));
}

void ChatTimelineModelTest::shouldMergeLocalEchoWhenServerEventArrivesThroughUpsert()
{
    ChatTimelineModel model;
    auto localEcho = makeItem(QStringLiteral("local:transaction"), QByteArrayLiteral("temporary"));
    localEcho.transactionId = QStringLiteral("transaction");
    model.upsert(localEcho);

    auto serverItem = makeItem(QStringLiteral("$server-event"), QByteArrayLiteral("002"));
    serverItem.transactionId = QStringLiteral("transaction");
    model.upsert(serverItem);

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.rowForStableId(QStringLiteral("$server-event")), 0);
    QCOMPARE(model.rowForTransactionId(QStringLiteral("transaction")), 0);
}

void ChatTimelineModelTest::shouldIgnoreAnOlderRevision()
{
    ChatTimelineModel model;
    model.upsert(makeItem(QStringLiteral("$event"), QByteArrayLiteral("001"), 2));
    auto older = makeItem(QStringLiteral("$event"), QByteArrayLiteral("001"), 1);
    older.content.plainText = QStringLiteral("obsolete");
    model.upsert(older);

    QCOMPARE(model.data(model.index(0, 0), ChatTimelineModel::PlainTextRole).toString(), QStringLiteral("$event"));
}

void ChatTimelineModelTest::shouldEmitDataChangedOnlyForUpdatedItem()
{
    ChatTimelineModel model;
    model.upsert(makeItem(QStringLiteral("$event"), QByteArrayLiteral("001"), 1));
    QSignalSpy changes{&model, &QAbstractItemModel::dataChanged};

    auto replacement = makeItem(QStringLiteral("ignored"), QByteArray{}, 2);
    replacement.content.plainText = QStringLiteral("edited");
    model.update(QStringLiteral("$event"), replacement);

    QCOMPARE(changes.size(), 1);
    const auto arguments = changes.takeFirst();
    QCOMPARE(arguments.at(0).value<QModelIndex>().row(), 0);
    QCOMPARE(arguments.at(1).value<QModelIndex>().row(), 0);
}

void ChatTimelineModelTest::shouldEmitOnlyChangedRoles()
{
    ChatTimelineModel model;
    model.upsert(makeItem(QStringLiteral("$event"), QByteArrayLiteral("001"), 1));
    QSignalSpy changes{&model, &QAbstractItemModel::dataChanged};

    auto replacement = makeItem(QStringLiteral("ignored"), QByteArray{}, 2);
    ChatTimelineReaction reaction;
    reaction.key = QStringLiteral("👍");
    reaction.own = true;
    replacement.content.reactions.append(reaction);
    model.update(QStringLiteral("$event"), replacement);

    QCOMPARE(changes.size(), 1);
    QCOMPARE(changes.constFirst().at(2).value<QList<int>>(), QList<int>{ChatTimelineModel::ReactionsRole});
}

void ChatTimelineModelTest::shouldInsertTimelinePagesInSingleBatches()
{
    ChatTimelineModel model;
    model.upsert(makeItem(QStringLiteral("middle"), QByteArrayLiteral("003")));

    ChatTimelinePage older;
    older.items = {makeItem(QStringLiteral("first"), QByteArrayLiteral("001")),
                   makeItem(QStringLiteral("second"), QByteArrayLiteral("002"))};
    QSignalSpy insertions{&model, &QAbstractItemModel::rowsInserted};
    model.prepend(older);

    QCOMPARE(insertions.size(), 1);
    QCOMPARE(insertions.constFirst().at(1).toInt(), 0);
    QCOMPARE(insertions.constFirst().at(2).toInt(), 1);

    ChatTimelinePage newer;
    newer.items = {makeItem(QStringLiteral("fourth"), QByteArrayLiteral("004")),
                   makeItem(QStringLiteral("fifth"), QByteArrayLiteral("005"))};
    model.append(newer);

    QCOMPARE(insertions.size(), 2);
    QCOMPARE(insertions.constLast().at(1).toInt(), 3);
    QCOMPARE(insertions.constLast().at(2).toInt(), 4);
    QCOMPARE(model.rowCount(), 5);

    ChatTimelinePage latest;
    latest.items = {makeItem(QStringLiteral("sixth"), QByteArrayLiteral("006")),
                    makeItem(QStringLiteral("seventh"), QByteArrayLiteral("007"))};
    QVERIFY(model.append(latest, 5));
    QCOMPARE(model.rowCount(), 5);
    QCOMPARE(model.rowForStableId(QStringLiteral("first")), -1);
    QCOMPARE(model.rowForStableId(QStringLiteral("middle")), 0);
}

void ChatTimelineModelTest::shouldKeepNewestRevisionWhenResetting()
{
    ChatTimelineModel model;
    auto oldItem = makeItem(QStringLiteral("event"), QByteArrayLiteral("001"), 1);
    auto newItem = makeItem(QStringLiteral("event"), QByteArrayLiteral("001"), 2);
    newItem.content.plainText = QStringLiteral("newest");

    model.reset({newItem, oldItem});

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0, 0), ChatTimelineModel::PlainTextRole).toString(), QStringLiteral("newest"));
}

void ChatTimelineModelTest::shouldRedactExistingItem()
{
    ChatTimelineModel model;
    model.upsert(makeItem(QStringLiteral("$event"), QByteArrayLiteral("001")));
    model.redact(QStringLiteral("$event"), QStringLiteral("removed by moderator"));

    QVERIFY(model.data(model.index(0, 0), ChatTimelineModel::RedactedRole).toBool());
    QCOMPARE(model.data(model.index(0, 0), ChatTimelineModel::PlainTextRole).toString(),
             QStringLiteral("Message removed."));
    QVERIFY(!model.data(model.index(0, 0), ChatTimelineModel::SystemEventRole).toBool());
    QCOMPARE(model.data(model.index(0, 0), ChatTimelineModel::ErrorTextRole).toString(), QStringLiteral("removed by moderator"));
}

void ChatTimelineModelTest::shouldExposeLocationUri()
{
    ChatTimelineModel model;
    auto location = makeItem(QStringLiteral("$location"), QByteArrayLiteral("001"));
    location.kind = ChatTimelineItemKind::LocationMessage;
    location.content.locationUri = QStringLiteral("geo:52.229700,21.012200");
    model.upsert(location);

    QCOMPARE(model.data(model.index(0, 0), ChatTimelineModel::LocationUriRole).toString(),
             QStringLiteral("geo:52.229700,21.012200"));
}

QTEST_APPLESS_MAIN(ChatTimelineModelTest)
#include "chat-timeline-model.test.moc"
