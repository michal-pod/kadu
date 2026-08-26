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

#include "chat/timeline/chat-timeline-controller.h"

#include "accounts/account-shared.h"
#include "protocols/services/protocol-timeline-service.h"

#include <QtCore/QList>
#include <QtCore/QPromise>
#include <QtTest/QtTest>

class TimelineServiceStub final : public ProtocolTimelineService
{
public:
    explicit TimelineServiceStub(Account account) : ProtocolTimelineService{account}
    {
    }

    QFuture<ChatTimelinePage> requestTimeline(const ChatTimelineRequest &request) override
    {
        m_lastRequest = request;
        if (m_pages.isEmpty())
            return completedPage({});

        return completedPage(m_pages.takeFirst());
    }

    ChatTimelineRequest lastRequest() const
    {
        return m_lastRequest;
    }

    void enqueue(const ChatTimelinePage &page)
    {
        m_pages.append(page);
    }

    void receive(const Chat &chat, const ChatTimelineItem &item)
    {
        emit eventReceived(chat, item);
    }

private:
    QFuture<ChatTimelinePage> completedPage(ChatTimelinePage page) const
    {
        QPromise<ChatTimelinePage> promise;
        auto future = promise.future();
        promise.start();
        promise.addResult(std::move(page));
        promise.finish();
        return future;
    }

    ChatTimelineRequest m_lastRequest;
    QList<ChatTimelinePage> m_pages;
};

class ChatTimelineControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void shouldMergeInitialPageAndLiveEvents();
    void shouldRequestOlderPageWithThePreviousCursor();
    void shouldTrackNewEventsOutsideTheNewestViewport();

private:
    ChatTimelineItem makeItem(const QString &stableId, const QByteArray &sourceOrder) const;
};

ChatTimelineItem ChatTimelineControllerTest::makeItem(const QString &stableId, const QByteArray &sourceOrder) const
{
    ChatTimelineItem item;
    item.stableId = stableId;
    item.sourceOrder = sourceOrder;
    item.timestamp = QDateTime::fromSecsSinceEpoch(1000);
    item.sender.id = QStringLiteral("@alice:example.org");
    item.content.plainText = stableId;
    return item;
}

void ChatTimelineControllerTest::shouldMergeInitialPageAndLiveEvents()
{
    Account account{new AccountShared{}};
    const auto chat = Chat::null;
    TimelineServiceStub timelineService{account};
    ChatTimelineController controller{chat, &timelineService};

    ChatTimelinePage page;
    page.items.append(makeItem(QStringLiteral("$history"), QByteArrayLiteral("001")));
    timelineService.enqueue(page);
    controller.loadInitial();

    QTRY_COMPARE(controller.timeline()->rowCount(), 1);
    timelineService.receive(chat, makeItem(QStringLiteral("$live"), QByteArrayLiteral("002")));

    QTRY_COMPARE(controller.timeline()->rowCount(), 2);
    QCOMPARE(controller.timeline()->rowForStableId(QStringLiteral("$history")), 0);
    QCOMPARE(controller.timeline()->rowForStableId(QStringLiteral("$live")), 1);
}

void ChatTimelineControllerTest::shouldRequestOlderPageWithThePreviousCursor()
{
    Account account{new AccountShared{}};
    const auto chat = Chat::null;
    TimelineServiceStub timelineService{account};
    ChatTimelineController controller{chat, &timelineService};

    ChatTimelinePage initialPage;
    initialPage.cursor = QByteArrayLiteral("older-cursor");
    initialPage.hasMore = true;
    timelineService.enqueue(initialPage);
    controller.loadInitial();
    QTRY_VERIFY(controller.hasOlder());

    controller.loadOlder();
    QCOMPARE(timelineService.lastRequest().cursor, QByteArrayLiteral("older-cursor"));
    QCOMPARE(static_cast<int>(timelineService.lastRequest().direction), static_cast<int>(ChatTimelineDirection::Older));
}

void ChatTimelineControllerTest::shouldTrackNewEventsOutsideTheNewestViewport()
{
    Account account{new AccountShared{}};
    const auto chat = Chat::null;
    TimelineServiceStub timelineService{account};
    ChatTimelineController controller{chat, &timelineService};

    timelineService.receive(chat, makeItem(QStringLiteral("$new"), QByteArrayLiteral("001")));
    QCOMPARE(controller.newEventsBelow(), 1);
    QVERIFY(controller.readMarkerId().isEmpty());

    controller.setActive(true);
    controller.setAtNewest(true);

    QCOMPARE(controller.newEventsBelow(), 0);
    QCOMPARE(controller.readMarkerId(), QStringLiteral("$new"));
}

QTEST_APPLESS_MAIN(ChatTimelineControllerTest)
#include "chat-timeline-controller.test.moc"
