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
        ++m_requestCount;
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

    void update(const Chat &chat, const ChatTimelineItem &item)
    {
        emit eventUpdated(chat, item);
    }

    void markTimelineItemRead(const Chat &, const QString &stableId) override
    {
        m_lastReadMarkerId = stableId;
    }

    int requestCount() const { return m_requestCount; }
    QString lastReadMarkerId() const { return m_lastReadMarkerId; }

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
    QString m_lastReadMarkerId;
    int m_requestCount = 0;
};

class ChatTimelineControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void shouldMergeInitialPageAndLiveEvents();
    void shouldRequestOlderPageWithThePreviousCursor();
    void shouldTrackNewEventsOutsideTheNewestViewport();
    void shouldKeepABoundedWindowWhenLoadingOlderMessages();
    void shouldKeepOlderRequestActiveWhileApplyingItsPage();
    void shouldExposeTheBeginningAfterTheFinalOlderPage();
    void shouldRetryTheFailedHistoryRequest();
    void shouldLoadContextAroundAnArbitraryMessage();
    void shouldNotInsertLiveEventsIntoAHistoricalWindow();
    void shouldPreserveTheOppositeWindowCursor_data();
    void shouldPreserveTheOppositeWindowCursor();
    void shouldReanchorAfterTrimmingTheWindow_data();
    void shouldReanchorAfterTrimmingTheWindow();
    void shouldReanchorAfterLiveEventsTrimTheWindow();
    void shouldKeepAHistoricalWindowAfterANewerRequestFails();
    void shouldIgnoreUpdatesOutsideTheWindow();
    void shouldContinuePagesWithoutBoundaryProgress_data();
    void shouldContinuePagesWithoutBoundaryProgress();
    void shouldCancelQueuedPagination();
    void shouldKeepFollowingLiveEventsWhenOlderPagesDoNotTrimTheWindow();
    void shouldAdvanceTheProtocolReadMarkerAcrossHiddenEvents();
    void shouldNotCountHiddenLiveEventsBelowTheViewport();

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
    initialPage.olderCursor = QByteArrayLiteral("older-cursor");
    initialPage.hasOlder = true;
    timelineService.enqueue(initialPage);
    controller.loadInitial();
    QTRY_VERIFY(controller.hasOlder());

    controller.loadOlder();
    QCOMPARE(timelineService.lastRequest().cursor, QByteArrayLiteral("older-cursor"));
    QCOMPARE(static_cast<int>(timelineService.lastRequest().mode), static_cast<int>(ChatTimelineRequestMode::Older));
}

void ChatTimelineControllerTest::shouldKeepABoundedWindowWhenLoadingOlderMessages()
{
    Account account{new AccountShared{}};
    const auto chat = Chat::null;
    TimelineServiceStub timelineService{account};
    ChatTimelineController controller{chat, &timelineService};

    ChatTimelinePage initialPage;
    for (auto index = 50; index < 150; ++index)
        initialPage.items.append(
            makeItem(QStringLiteral("$%1").arg(index), QByteArray::number(index).rightJustified(3, '0')));
    initialPage.olderCursor = QByteArrayLiteral("older");
    initialPage.hasOlder = true;
    timelineService.enqueue(initialPage);
    controller.loadInitial(100);
    QTRY_COMPARE(controller.timeline()->rowCount(), 100);
    controller.setActive(true);
    controller.setAtNewest(true);
    QCOMPARE(controller.readMarkerId(), QStringLiteral("$149"));

    ChatTimelinePage olderPage;
    for (auto index = 0; index < 50; ++index)
        olderPage.items.append(
            makeItem(QStringLiteral("$%1").arg(index), QByteArray::number(index).rightJustified(3, '0')));
    olderPage.olderCursor = QByteArrayLiteral("even-older");
    olderPage.newerCursor = QByteArrayLiteral("newer");
    olderPage.hasOlder = true;
    olderPage.hasNewer = true;
    timelineService.enqueue(olderPage);
    controller.loadOlder(50);

    QTRY_COMPARE(controller.timeline()->rowCount(), 100);
    QCOMPARE(controller.timeline()->rowForStableId(QStringLiteral("$0")), 0);
    QCOMPARE(controller.timeline()->rowForStableId(QStringLiteral("$99")), 99);
    QCOMPARE(controller.timeline()->rowForStableId(QStringLiteral("$100")), -1);
    QVERIFY(controller.hasOlder());
    QVERIFY(controller.hasNewer());
    QCOMPARE(controller.readMarkerId(), QStringLiteral("$149"));
}

void ChatTimelineControllerTest::shouldKeepOlderRequestActiveWhileApplyingItsPage()
{
    Account account{new AccountShared{}};
    const auto chat = Chat::null;
    TimelineServiceStub timelineService{account};
    ChatTimelineController controller{chat, &timelineService};

    ChatTimelinePage initialPage;
    initialPage.items.append(makeItem(QStringLiteral("$newer"), QByteArrayLiteral("002")));
    initialPage.olderCursor = QByteArrayLiteral("older");
    initialPage.hasOlder = true;
    timelineService.enqueue(initialPage);
    controller.loadInitial();
    QTRY_VERIFY(controller.hasOlder());

    auto loadingWhileInserting = false;
    connect(controller.timeline(), &QAbstractItemModel::rowsInserted, this,
            [&controller, &loadingWhileInserting] { loadingWhileInserting = controller.isLoadingOlder(); });

    ChatTimelinePage olderPage;
    olderPage.items.append(makeItem(QStringLiteral("$older"), QByteArrayLiteral("001")));
    timelineService.enqueue(olderPage);
    controller.loadOlder();

    QTRY_COMPARE(controller.timeline()->rowCount(), 2);
    QVERIFY(loadingWhileInserting);
}

void ChatTimelineControllerTest::shouldExposeTheBeginningAfterTheFinalOlderPage()
{
    Account account{new AccountShared{}};
    const auto chat = Chat::null;
    TimelineServiceStub timelineService{account};
    ChatTimelineController controller{chat, &timelineService};

    ChatTimelinePage initialPage;
    initialPage.items.append(makeItem(QStringLiteral("$newer"), QByteArrayLiteral("002")));
    initialPage.olderCursor = QByteArrayLiteral("older");
    initialPage.hasOlder = true;
    timelineService.enqueue(initialPage);
    controller.loadInitial();
    QTRY_VERIFY(controller.hasOlder());

    ChatTimelinePage oldestPage;
    oldestPage.items.append(makeItem(QStringLiteral("$oldest"), QByteArrayLiteral("001")));
    timelineService.enqueue(oldestPage);
    controller.loadOlder();

    QTRY_COMPARE(controller.timeline()->rowCount(), 2);
    QVERIFY(!controller.hasOlder());
    QCOMPARE(controller.timeline()->rowForStableId(QStringLiteral("$oldest")), 0);
}

void ChatTimelineControllerTest::shouldRetryTheFailedHistoryRequest()
{
    Account account{new AccountShared{}};
    const auto chat = Chat::null;
    TimelineServiceStub timelineService{account};
    ChatTimelineController controller{chat, &timelineService};

    ChatTimelinePage initialPage;
    initialPage.items.append(makeItem(QStringLiteral("$newer"), QByteArrayLiteral("002")));
    initialPage.olderCursor = QByteArrayLiteral("older-cursor");
    initialPage.hasOlder = true;
    timelineService.enqueue(initialPage);
    controller.loadInitial();
    QTRY_VERIFY(controller.hasOlder());

    ChatTimelinePage failedPage;
    failedPage.error = QStringLiteral("network error");
    timelineService.enqueue(failedPage);
    controller.loadOlder(17);
    QTRY_COMPARE(controller.historyError(), QStringLiteral("network error"));
    QVERIFY(controller.hasOlder());

    ChatTimelinePage retriedPage;
    retriedPage.items.append(makeItem(QStringLiteral("$older"), QByteArrayLiteral("001")));
    timelineService.enqueue(retriedPage);
    controller.retryHistory();

    QCOMPARE(static_cast<int>(timelineService.lastRequest().mode), static_cast<int>(ChatTimelineRequestMode::Older));
    QCOMPARE(timelineService.lastRequest().cursor, QByteArrayLiteral("older-cursor"));
    QCOMPARE(timelineService.lastRequest().limit, 17);
    QTRY_COMPARE(controller.timeline()->rowForStableId(QStringLiteral("$older")), 0);
    QVERIFY(controller.historyError().isEmpty());
}

void ChatTimelineControllerTest::shouldLoadContextAroundAnArbitraryMessage()
{
    Account account{new AccountShared{}};
    const auto chat = Chat::null;
    TimelineServiceStub timelineService{account};
    ChatTimelineController controller{chat, &timelineService};
    QSignalSpy positionSpy{&controller, &ChatTimelineController::timelinePositionRequested};

    ChatTimelinePage contextPage;
    contextPage.items.append(makeItem(QStringLiteral("$target"), QByteArrayLiteral("050")));
    contextPage.olderCursor = QByteArrayLiteral("older");
    contextPage.newerCursor = QByteArrayLiteral("newer");
    contextPage.hasOlder = true;
    contextPage.hasNewer = true;
    timelineService.enqueue(contextPage);

    controller.jumpTo(QStringLiteral("$target"));
    QTRY_COMPARE(positionSpy.size(), 1);
    QCOMPARE(static_cast<int>(timelineService.lastRequest().mode), static_cast<int>(ChatTimelineRequestMode::Around));
    QCOMPARE(timelineService.lastRequest().anchorId, QStringLiteral("$target"));
    QCOMPARE(controller.timeline()->rowForStableId(QStringLiteral("$target")), 0);
}

void ChatTimelineControllerTest::shouldNotInsertLiveEventsIntoAHistoricalWindow()
{
    Account account{new AccountShared{}};
    const auto chat = Chat::null;
    TimelineServiceStub timelineService{account};
    ChatTimelineController controller{chat, &timelineService};

    ChatTimelinePage contextPage;
    contextPage.items.append(makeItem(QStringLiteral("$old"), QByteArrayLiteral("001")));
    contextPage.newerCursor = QByteArrayLiteral("newer");
    contextPage.hasNewer = true;
    timelineService.enqueue(contextPage);
    controller.jumpTo(QStringLiteral("$old"));
    QTRY_VERIFY(controller.hasNewer());

    timelineService.receive(chat, makeItem(QStringLiteral("$live"), QByteArrayLiteral("999")));
    QCOMPARE(controller.timeline()->rowForStableId(QStringLiteral("$live")), -1);
    QCOMPARE(controller.newEventsBelow(), 1);

    timelineService.enqueue({});
    controller.loadNewer();
    QTRY_COMPARE(controller.timeline()->rowForStableId(QStringLiteral("$live")), 1);
    QCOMPARE(controller.newEventsBelow(), 0);
    QVERIFY(!controller.hasNewer());
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

void ChatTimelineControllerTest::shouldPreserveTheOppositeWindowCursor_data()
{
    QTest::addColumn<bool>("older");
    QTest::newRow("older") << true;
    QTest::newRow("newer") << false;
}

void ChatTimelineControllerTest::shouldPreserveTheOppositeWindowCursor()
{
    QFETCH(bool, older);
    Account account{new AccountShared{}};
    TimelineServiceStub service{account};
    ChatTimelineController controller{Chat::null, &service};
    ChatTimelinePage initial;
    initial.items = {makeItem("$50", "050")};
    initial.hasOlder = initial.hasNewer = true;
    initial.olderCursor = "window-older";
    initial.newerCursor = "window-newer";
    service.enqueue(initial);
    controller.jumpTo("$50");
    QTRY_VERIFY(!controller.isLoadingInitial());

    ChatTimelinePage page;
    page.items = {makeItem(older ? "$49" : "$51", older ? "049" : "051")};
    page.hasOlder = page.hasNewer = true;
    page.olderCursor = "page-older";
    page.newerCursor = "page-newer";
    service.enqueue(page);
    if (older)
        controller.loadOlder();
    else
        controller.loadNewer();
    QTRY_VERIFY(!controller.isLoadingOlder() && !controller.isLoadingNewer());

    if (older)
        controller.loadNewer();
    else
        controller.loadOlder();
    QCOMPARE(service.lastRequest().cursor, older ? QByteArray{"window-newer"} : QByteArray{"window-older"});
    QCOMPARE(service.lastRequest().anchorId, QString{"$50"});
}

void ChatTimelineControllerTest::shouldReanchorAfterTrimmingTheWindow_data()
{
    QTest::addColumn<bool>("older");
    QTest::newRow("older") << true;
    QTest::newRow("newer") << false;
}

void ChatTimelineControllerTest::shouldReanchorAfterTrimmingTheWindow()
{
    QFETCH(bool, older);
    Account account{new AccountShared{}};
    TimelineServiceStub service{account};
    ChatTimelineController controller{Chat::null, &service};
    ChatTimelinePage initial;
    for (int i = 50; i < 150; ++i)
        initial.items.append(makeItem(QString{"$%1"}.arg(i), QByteArray::number(i).rightJustified(3, '0')));
    initial.hasOlder = initial.hasNewer = true;
    initial.olderCursor = "window-older";
    initial.newerCursor = "window-newer";
    service.enqueue(initial);
    controller.jumpTo("$100");
    QTRY_VERIFY(!controller.isLoadingInitial());

    ChatTimelinePage page;
    for (int i = older ? 0 : 150; i < (older ? 50 : 200); ++i)
        page.items.append(makeItem(QString{"$%1"}.arg(i), QByteArray::number(i).rightJustified(3, '0')));
    page.hasOlder = page.hasNewer = true;
    page.olderCursor = "page-older";
    page.newerCursor = "page-newer";
    service.enqueue(page);
    if (older)
        controller.loadOlder();
    else
        controller.loadNewer();
    QTRY_VERIFY(!controller.isLoadingOlder() && !controller.isLoadingNewer());
    QCOMPARE(controller.timeline()->rowCount(), 100);

    ChatTimelinePage returningPage;
    for (int i = older ? 100 : 50; i < (older ? 150 : 100); ++i)
        returningPage.items.append(makeItem(QString{"$%1"}.arg(i), QByteArray::number(i).rightJustified(3, '0')));
    service.enqueue(returningPage);
    if (older)
        controller.loadNewer();
    else
        controller.loadOlder();
    QVERIFY(service.lastRequest().cursor.isEmpty());
    QCOMPARE(service.lastRequest().anchorId, older ? QString{"$99"} : QString{"$100"});
    QTRY_VERIFY(!controller.isLoadingOlder() && !controller.isLoadingNewer());
    for (int i = 50; i < 150; ++i)
        QCOMPARE(controller.timeline()->rowForStableId(QString{"$%1"}.arg(i)), i - 50);
}

void ChatTimelineControllerTest::shouldReanchorAfterLiveEventsTrimTheWindow()
{
    Account account{new AccountShared{}};
    TimelineServiceStub service{account};
    ChatTimelineController controller{Chat::null, &service};
    ChatTimelinePage initial;
    for (int i = 0; i < 100; ++i)
        initial.items.append(makeItem(QString{"$%1"}.arg(i), QByteArray::number(i).rightJustified(3, '0')));
    initial.hasOlder = true;
    initial.olderCursor = "before-zero";
    service.enqueue(initial);
    controller.loadInitial(100);
    QTRY_VERIFY(!controller.isLoadingInitial());
    service.receive(Chat::null, makeItem("$100", "100"));
    QCOMPARE(controller.timeline()->rowCount(), 100);
    controller.loadOlder();
    QVERIFY(service.lastRequest().cursor.isEmpty());
    QCOMPARE(service.lastRequest().anchorId, QString{"$1"});
}

void ChatTimelineControllerTest::shouldKeepAHistoricalWindowAfterANewerRequestFails()
{
    Account account{new AccountShared{}};
    TimelineServiceStub service{account};
    ChatTimelineController controller{Chat::null, &service};
    ChatTimelinePage initial;
    initial.items = {makeItem("$old", "001")};
    initial.hasNewer = true;
    initial.newerCursor = "newer";
    service.enqueue(initial);
    controller.jumpTo("$old");
    QTRY_VERIFY(!controller.isLoadingInitial());

    ChatTimelinePage failed;
    failed.error = "network error";
    service.enqueue(failed);
    controller.loadNewer(17);
    QTRY_VERIFY(!controller.isLoadingNewer());
    QVERIFY(controller.hasNewer());
    const auto requests = service.requestCount();
    controller.loadNewer();
    QCOMPARE(service.requestCount(), requests);
    service.receive(Chat::null, makeItem("$live", "999"));
    QCOMPARE(controller.timeline()->rowCount(), 1);
    controller.setActive(true);
    controller.setAtNewest(true);
    QVERIFY(controller.readMarkerId().isEmpty());

    ChatTimelinePage middle;
    middle.items = {makeItem("$middle", "500")};
    service.enqueue(middle);
    controller.retryHistory();
    QCOMPARE(service.lastRequest().cursor, QByteArray{"newer"});
    QCOMPARE(service.lastRequest().limit, 17);
    QTRY_VERIFY(!controller.isLoadingNewer());
    QCOMPARE(controller.timeline()->rowForStableId("$middle"), 1);
    QCOMPARE(controller.timeline()->rowForStableId("$live"), 2);
}

void ChatTimelineControllerTest::shouldIgnoreUpdatesOutsideTheWindow()
{
    Account account{new AccountShared{}};
    TimelineServiceStub service{account};
    ChatTimelineController controller{Chat::null, &service};
    service.receive(Chat::null, makeItem("$visible", "500"));
    service.update(Chat::null, makeItem("$old-decrypted", "001"));
    QCOMPARE(controller.timeline()->rowCount(), 1);
    auto edited = makeItem("$visible", "500");
    edited.content.plainText = "edited";
    service.update(Chat::null, edited);
    QCOMPARE(controller.timeline()->item("$visible").content.plainText, QString{"edited"});
}

void ChatTimelineControllerTest::shouldContinuePagesWithoutBoundaryProgress_data()
{
    QTest::addColumn<bool>("older");
    QTest::addColumn<bool>("overlapping");
    QTest::newRow("older-empty") << true << false;
    QTest::newRow("older-overlapping") << true << true;
    QTest::newRow("newer-empty") << false << false;
    QTest::newRow("newer-overlapping") << false << true;
}

void ChatTimelineControllerTest::shouldContinuePagesWithoutBoundaryProgress()
{
    QFETCH(bool, older);
    QFETCH(bool, overlapping);
    Account account{new AccountShared{}};
    TimelineServiceStub service{account};
    ChatTimelineController controller{Chat::null, &service};
    ChatTimelinePage initial;
    initial.items = {makeItem("$middle", "500")};
    initial.hasOlder = initial.hasNewer = true;
    initial.olderCursor = "older";
    initial.newerCursor = "newer";
    service.enqueue(initial);
    controller.jumpTo("$middle");
    QTRY_VERIFY(!controller.isLoadingInitial());

    auto page = initial;
    if (!overlapping)
        page.items.clear();
    page.olderCursor = "next-older";
    page.newerCursor = "next-newer";
    service.enqueue(page);
    ChatTimelinePage last;
    last.items = {makeItem(older ? "$old" : "$new", older ? "001" : "999")};
    service.enqueue(last);
    if (older)
        controller.loadOlder(17);
    else
        controller.loadNewer(17);
    QTRY_COMPARE(service.requestCount(), 3);
    QTRY_COMPARE(controller.timeline()->rowCount(), 2);
    QCOMPARE(service.lastRequest().limit, 17);
    QCOMPARE(service.lastRequest().cursor, older ? QByteArray{"next-older"} : QByteArray{"next-newer"});
}

void ChatTimelineControllerTest::shouldCancelQueuedPagination()
{
    Account account{new AccountShared{}};
    TimelineServiceStub service{account};
    ChatTimelineController controller{Chat::null, &service};
    ChatTimelinePage initial;
    initial.items = {makeItem("$middle", "500")};
    initial.hasOlder = true;
    initial.olderCursor = "older";
    service.enqueue(initial);
    controller.loadInitial();
    QTRY_VERIFY(!controller.isLoadingInitial());
    connect(&controller, &ChatTimelineController::loadingOlderChanged, &controller, [&controller] {
        if (!controller.isLoadingOlder())
            controller.cancelRequests();
    });
    ChatTimelinePage empty;
    empty.hasOlder = true;
    empty.olderCursor = "next-older";
    service.enqueue(empty);
    controller.loadOlder();
    QTRY_VERIFY(!controller.isLoadingOlder());
    QCoreApplication::processEvents();
    QCOMPARE(service.requestCount(), 2);
}

void ChatTimelineControllerTest::shouldKeepFollowingLiveEventsWhenOlderPagesDoNotTrimTheWindow()
{
    Account account{new AccountShared{}};
    TimelineServiceStub service{account};
    ChatTimelineController controller{Chat::null, &service};
    ChatTimelinePage initial;
    initial.items = {makeItem("$latest", "500")};
    initial.hasOlder = true;
    initial.olderCursor = "older";
    service.enqueue(initial);
    controller.loadInitial();
    QTRY_VERIFY(!controller.isLoadingInitial());
    ChatTimelinePage older;
    older.items = {makeItem("$older", "001")};
    older.hasNewer = true;
    older.newerCursor = "after-older-page";
    service.enqueue(older);
    controller.loadOlder();
    QTRY_VERIFY(!controller.isLoadingOlder());
    QVERIFY(!controller.hasNewer());
    service.receive(Chat::null, makeItem("$live", "999"));
    QCOMPARE(controller.timeline()->rowForStableId("$live"), 2);
}

void ChatTimelineControllerTest::shouldAdvanceTheProtocolReadMarkerAcrossHiddenEvents()
{
    Account account{new AccountShared{}};
    const auto chat = Chat::null;
    TimelineServiceStub timelineService{account};
    ChatTimelineController controller{chat, &timelineService};
    controller.timeline()->setDetails(ChatTimelineDetails::ChatOnly);

    auto visible = makeItem(QStringLiteral("$visible"), QByteArrayLiteral("001"));
    ChatTimelinePage page;
    page.items = {visible};
    timelineService.enqueue(page);

    controller.setActive(true);
    controller.setAtNewest(true);
    controller.loadInitial();

    QTRY_COMPARE(controller.timeline()->rowCount(), 1);
    QTRY_COMPARE(timelineService.lastReadMarkerId(), QStringLiteral("$visible"));

    auto hidden = makeItem(QStringLiteral("$hidden"), QByteArrayLiteral("002"));
    hidden.level = ChatTimelineItemLevel::Debug;
    timelineService.receive(chat, hidden);

    QTRY_COMPARE(timelineService.lastReadMarkerId(), QStringLiteral("$hidden"));
    QCOMPARE(controller.readMarkerId(), QStringLiteral("$visible"));
}

void ChatTimelineControllerTest::shouldNotCountHiddenLiveEventsBelowTheViewport()
{
    Account account{new AccountShared{}};
    const auto chat = Chat::null;
    TimelineServiceStub timelineService{account};
    ChatTimelineController controller{chat, &timelineService};
    controller.timeline()->setDetails(ChatTimelineDetails::ChatOnly);

    auto hidden = makeItem(QStringLiteral("$hidden"), QByteArrayLiteral("001"));
    hidden.level = ChatTimelineItemLevel::Debug;
    timelineService.receive(chat, hidden);

    QCOMPARE(controller.timeline()->rowCount(), 0);
    QCOMPARE(controller.newEventsBelow(), 0);
    QCOMPARE(controller.timeline()->item(QStringLiteral("$hidden")).stableId, QStringLiteral("$hidden"));
}

QTEST_GUILESS_MAIN(ChatTimelineControllerTest)
#include "chat-timeline-controller.test.moc"
