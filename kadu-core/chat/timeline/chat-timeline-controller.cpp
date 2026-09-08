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

#include "chat-timeline-controller.h"
#include "chat-timeline-controller.moc"

#include "protocols/services/protocol-timeline-service.h"

#include <QtCore/QTimer>

#include <algorithm>
#include <utility>

ChatTimelineController::ChatTimelineController(Chat chat, ProtocolTimelineService *timelineService, QObject *parent)
        : QObject{parent}, m_chat{chat}, m_timelineService{timelineService},
          m_timeline{new ChatTimelineModel{this}}
{
    Q_ASSERT(m_timelineService);

    connect(m_timelineService, &ProtocolTimelineService::eventReceived, this, &ChatTimelineController::eventReceived);
    connect(m_timelineService, &ProtocolTimelineService::eventUpdated, this, &ChatTimelineController::eventUpdated);
    connect(m_timelineService, &ProtocolTimelineService::eventRedacted, this, &ChatTimelineController::eventRedacted);
}

ChatTimelineController::~ChatTimelineController()
{
    cancelRequests();
}

Chat ChatTimelineController::chat() const
{
    return m_chat;
}

ChatTimelineModel *ChatTimelineController::timeline() const
{
    return m_timeline;
}

bool ChatTimelineController::isLoadingInitial() const
{
    return m_loadingInitial;
}

bool ChatTimelineController::isLoadingOlder() const
{
    return m_loadingOlder;
}

bool ChatTimelineController::isLoadingNewer() const
{
    return m_loadingNewer;
}

bool ChatTimelineController::hasOlder() const
{
    return m_hasOlder;
}

bool ChatTimelineController::hasNewer() const
{
    return m_hasNewer;
}

QString ChatTimelineController::historyError() const
{
    return m_historyError;
}

bool ChatTimelineController::isActive() const
{
    return m_active;
}

bool ChatTimelineController::isAtNewest() const
{
    return m_atNewest;
}

QString ChatTimelineController::readMarkerId() const
{
    return m_readMarkerId;
}

int ChatTimelineController::newEventsBelow() const
{
    return m_newEventsBelow;
}

void ChatTimelineController::loadInitial(int limit)
{
    loadLatest(limit);
}

void ChatTimelineController::loadLatest(int limit)
{
    if (!m_timelineService)
        return;

    cancelRequests();
    m_timeline->clear();
    m_olderCursor.clear();
    m_newerCursor.clear();
    m_deferredLiveItems.clear();
    setHasOlder(false);
    setHasNewer(false);
    setHistoryError({});
    setReadMarkerId({});
    setNewEventsBelow(0);
    requestPage(RequestKind::Latest, {}, {}, qMin(limit, MaximumWindowSize));
}

void ChatTimelineController::loadOlder(int limit)
{
    if (!m_timelineService || m_loadingInitial || m_loadingOlder || m_loadingNewer || !m_hasOlder ||
        m_hasFailedRequest)
        return;

    setHistoryError({});
    const auto first = m_timeline->items().value(0).stableId;
    requestPage(RequestKind::Older, m_olderCursor, first, qMin(limit, MaximumWindowSize));
}

void ChatTimelineController::loadNewer(int limit)
{
    if (!m_timelineService || m_loadingInitial || m_loadingOlder || m_loadingNewer || !m_hasNewer ||
        m_hasFailedRequest)
        return;

    setHistoryError({});
    const auto items = m_timeline->items();
    const auto last = items.isEmpty() ? QString{} : items.constLast().stableId;
    requestPage(RequestKind::Newer, m_newerCursor, last, qMin(limit, MaximumWindowSize));
}

void ChatTimelineController::jumpTo(const QString &stableId, int limit)
{
    if (!m_timelineService || stableId.isEmpty())
        return;

    setAtNewest(false);
    if (m_timeline->rowForStableId(stableId) >= 0)
    {
        emit timelinePositionRequested(stableId);
        return;
    }

    cancelRequests();
    setHistoryError({});
    requestPage(RequestKind::Around, {}, stableId, qMin(limit, MaximumWindowSize));
}

void ChatTimelineController::retryHistory()
{
    if (m_loadingInitial || m_loadingOlder || m_loadingNewer || !m_hasFailedRequest)
        return;

    const auto requestKind = m_failedRequestKind;
    auto cursor = m_failedRequestCursor;
    auto anchor = m_failedRequestAnchor;
    // Live events may have trimmed the window since an edge request failed.
    const auto items = m_timeline->items();
    if (requestKind == RequestKind::Older)
    {
        cursor = m_olderCursor;
        anchor = items.isEmpty() ? QString{} : items.constFirst().stableId;
    }
    else if (requestKind == RequestKind::Newer)
    {
        cursor = m_newerCursor;
        anchor = items.isEmpty() ? QString{} : items.constLast().stableId;
    }
    const auto limit = m_failedRequestLimit;
    clearFailedRequest();
    setHistoryError({});
    requestPage(requestKind, cursor, anchor, limit);
}

void ChatTimelineController::cancelRequests()
{
    ++m_requestGeneration;
    if (m_pageContinuation.isValid())
        m_pageContinuation.cancel();
    m_pageContinuation = {};
    setLoadingInitial(false);
    setLoadingOlder(false);
    setLoadingNewer(false);
}

void ChatTimelineController::setActive(bool active)
{
    if (m_active == active)
        return;

    m_active = active;
    emit activeChanged();
    if (m_active && m_atNewest && !m_hasNewer)
        markNewestEventVisible();
}

void ChatTimelineController::setAtNewest(bool atNewest)
{
    if (m_atNewest == atNewest)
        return;

    m_atNewest = atNewest;
    emit atNewestChanged();
    if (m_active && m_atNewest && !m_hasNewer)
        markNewestEventVisible();
}

void ChatTimelineController::markVisible(const QString &stableId)
{
    // A historical window must never move the protocol read marker backwards.
    if (m_hasNewer)
        return;

    const auto row = m_timeline->rowForStableId(stableId);
    if (row < 0)
        return;

    const auto currentReadRow = m_timeline->rowForStableId(m_readMarkerId);
    if (currentReadRow >= row)
        return;

    setReadMarkerId(stableId);
    if (m_timelineService)
        m_timelineService->markTimelineItemRead(m_chat, stableId);
    if (row == m_timeline->rowCount() - 1 && !m_hasNewer)
        setNewEventsBelow(0);
}

void ChatTimelineController::requestPage(RequestKind requestKind, const QByteArray &cursor, const QString &anchorId,
                                         int limit)
{
    if (!m_timelineService)
        return;

    clearFailedRequest();

    ChatTimelineRequest request;
    request.chat = m_chat;
    request.cursor = cursor;
    request.anchorId = anchorId;
    switch (requestKind)
    {
    case RequestKind::Latest: request.mode = ChatTimelineRequestMode::Latest; break;
    case RequestKind::Older: request.mode = ChatTimelineRequestMode::Older; break;
    case RequestKind::Newer: request.mode = ChatTimelineRequestMode::Newer; break;
    case RequestKind::Around: request.mode = ChatTimelineRequestMode::Around; break;
    }
    request.limit = qMax(1, limit);
    const auto requestedLimit = request.limit;

    if (requestKind == RequestKind::Latest || requestKind == RequestKind::Around)
        setLoadingInitial(true);
    else if (requestKind == RequestKind::Older)
        setLoadingOlder(true);
    else
        setLoadingNewer(true);

    const auto generation = ++m_requestGeneration;
    auto future = m_timelineService->requestTimeline(request);
    m_pageContinuation = future.then(
        this, [this, requestKind, generation, cursor, anchorId, requestedLimit](const ChatTimelinePage &page) {
            pageAvailable(requestKind, generation, cursor, anchorId, requestedLimit, page);
        });
}

void ChatTimelineController::pageAvailable(RequestKind requestKind, quint64 generation,
                                           const QByteArray &requestedCursor, const QString &requestedAnchor,
                                           int requestedLimit, const ChatTimelinePage &page)
{
    if (generation != m_requestGeneration)
        return;

    if (!page.error.isEmpty())
    {
        m_failedRequestKind = requestKind;
        m_failedRequestCursor = requestedCursor;
        m_failedRequestAnchor = requestedAnchor;
        m_failedRequestLimit = requestedLimit;
        m_hasFailedRequest = true;
        setHistoryError(page.error);
        // A failed request says nothing about the end of history. In particular,
        // keep historical windows isolated from live events until retry succeeds.
        if (requestKind == RequestKind::Latest || requestKind == RequestKind::Around)
            setLoadingInitial(false);
        else if (requestKind == RequestKind::Older)
            setLoadingOlder(false);
        else
            setLoadingNewer(false);
        return;
    }

    clearFailedRequest();

    auto windowTrimmed = false;
    if (requestKind == RequestKind::Latest)
    {
        // loadLatest() clears the previous window before issuing the request,
        // so anything in the model now arrived from live sync while it was pending.
        const auto liveItems = m_timeline->items();
        m_timeline->reset(page.items);
        ChatTimelinePage livePage;
        livePage.items = liveItems;
        windowTrimmed = m_timeline->append(livePage, MaximumWindowSize);
    }
    else if (requestKind == RequestKind::Around)
        m_timeline->reset(page.items);
    else if (requestKind == RequestKind::Older)
        windowTrimmed = m_timeline->prepend(page, MaximumWindowSize);
    else
        windowTrimmed = m_timeline->append(page, MaximumWindowSize);

    const auto overflow = m_timeline->rowCount() - MaximumWindowSize;
    if (overflow > 0)
    {
        windowTrimmed = true;
        if (requestKind == RequestKind::Older)
            m_timeline->removeLast(overflow);
        else
            m_timeline->removeFirst(overflow);
    }

    const auto usableOlder = page.hasOlder && !page.olderCursor.isEmpty() &&
                             (requestKind != RequestKind::Older || page.olderCursor != requestedCursor);
    const auto usableNewer = page.hasNewer && !page.newerCursor.isEmpty() &&
                             (requestKind != RequestKind::Newer || page.newerCursor != requestedCursor);
    // Page cursors describe that page, not the entire merged window. The
    // opposite edge still belongs to the previously loaded window.
    if (requestKind != RequestKind::Newer)
    {
        m_olderCursor = usableOlder ? page.olderCursor : QByteArray{};
        setHasOlder(usableOlder);
    }
    if (requestKind != RequestKind::Older)
    {
        m_newerCursor = usableNewer ? page.newerCursor : QByteArray{};
        setHasNewer(usableNewer);
    }

    if (windowTrimmed)
    {
        if (requestKind == RequestKind::Older)
        {
            // Trimming moved the edge inside the old page. Re-anchor the next
            // request on the last retained item instead of reusing a page token.
            m_newerCursor.clear();
            setHasNewer(true);
        }
        else
        {
            m_olderCursor.clear();
            setHasOlder(true);
        }
    }

    if (requestKind == RequestKind::Newer && !m_hasNewer && !m_deferredLiveItems.isEmpty())
    {
        ChatTimelinePage deferredPage;
        deferredPage.items = std::move(m_deferredLiveItems);
        const auto deferredTrimmed = m_timeline->append(deferredPage, MaximumWindowSize);
        m_deferredLiveItems.clear();
        const auto liveOverflow = m_timeline->rowCount() - MaximumWindowSize;
        if (liveOverflow > 0)
        {
            m_timeline->removeFirst(liveOverflow);
            m_olderCursor.clear();
            setHasOlder(true);
        }
        else if (deferredTrimmed)
        {
            m_olderCursor.clear();
            setHasOlder(true);
        }
        setNewEventsBelow(0);
    }

    if (m_active && m_atNewest && !m_hasNewer)
        markNewestEventVisible();

    if (requestKind == RequestKind::Latest)
        setNewEventsBelow(0);
    if (requestKind == RequestKind::Around)
        emit timelinePositionRequested(requestedAnchor);

    // Keep the request marked as active until the model and both cursors are
    // consistent. ListView reacts synchronously to row changes; clearing this
    // earlier allowed it to start another edge request with the stale cursor.
    if (requestKind == RequestKind::Latest || requestKind == RequestKind::Around)
        setLoadingInitial(false);
    else if (requestKind == RequestKind::Older)
        setLoadingOlder(false);
    else
        setLoadingNewer(false);

    // Empty or overlapping pages need no scroll event to continue. A queued
    // continuation must not outlive cancellation or a jump to another window.
    const auto items = m_timeline->items();
    const auto boundary = items.isEmpty() ? QString{} :
                          requestKind == RequestKind::Older ? items.constFirst().stableId : items.constLast().stableId;
    if ((page.items.isEmpty() || boundary == requestedAnchor) &&
        ((requestKind == RequestKind::Older && m_hasOlder) || (requestKind == RequestKind::Newer && m_hasNewer)))
        QTimer::singleShot(0, this, [this, generation, requestKind, requestedLimit] {
            if (generation != m_requestGeneration)
                return;
            if (requestKind == RequestKind::Older)
                loadOlder(requestedLimit);
            else
                loadNewer(requestedLimit);
        });
}

void ChatTimelineController::setLoadingInitial(bool loading)
{
    if (m_loadingInitial == loading)
        return;

    m_loadingInitial = loading;
    emit loadingInitialChanged();
}

void ChatTimelineController::setLoadingOlder(bool loading)
{
    if (m_loadingOlder == loading)
        return;

    m_loadingOlder = loading;
    emit loadingOlderChanged();
}

void ChatTimelineController::setLoadingNewer(bool loading)
{
    if (m_loadingNewer == loading)
        return;

    m_loadingNewer = loading;
    emit loadingNewerChanged();
}

void ChatTimelineController::setHasOlder(bool hasOlder)
{
    if (m_hasOlder == hasOlder)
        return;

    m_hasOlder = hasOlder;
    emit hasOlderChanged();
}

void ChatTimelineController::setHasNewer(bool hasNewer)
{
    if (m_hasNewer == hasNewer)
        return;

    m_hasNewer = hasNewer;
    emit hasNewerChanged();
}

void ChatTimelineController::setHistoryError(const QString &error)
{
    if (m_historyError == error)
        return;

    m_historyError = error;
    emit historyErrorChanged();
}

void ChatTimelineController::clearFailedRequest()
{
    m_hasFailedRequest = false;
    m_failedRequestCursor.clear();
    m_failedRequestAnchor.clear();
    m_failedRequestLimit = 0;
}

void ChatTimelineController::setReadMarkerId(const QString &stableId)
{
    if (m_readMarkerId == stableId)
        return;

    m_readMarkerId = stableId;
    emit readMarkerIdChanged();
}

void ChatTimelineController::setNewEventsBelow(int count)
{
    if (m_newEventsBelow == count)
        return;

    m_newEventsBelow = count;
    emit newEventsBelowChanged();
}

void ChatTimelineController::markNewestEventVisible()
{
    const auto lastRow = m_timeline->rowCount() - 1;
    if (lastRow < 0)
        return;

    markVisible(m_timeline->data(m_timeline->index(lastRow, 0), ChatTimelineModel::StableIdRole).toString());
}

void ChatTimelineController::eventReceived(const Chat &chat, const ChatTimelineItem &item)
{
    if (chat != m_chat)
        return;

    const auto knownItem = m_timeline->rowForStableId(item.stableId) >= 0 ||
                           m_timeline->rowForTransactionId(item.transactionId) >= 0;
    if (knownItem)
        m_timeline->upsert(item);
    else if (!m_hasNewer)
    {
        ChatTimelinePage livePage;
        livePage.items = {item};
        if (m_timeline->append(livePage, MaximumWindowSize))
        {
            m_olderCursor.clear();
            setHasOlder(true);
        }
    }
    else
    {
        const auto deferred = std::find_if(m_deferredLiveItems.begin(), m_deferredLiveItems.end(),
                                           [&item](const ChatTimelineItem &candidate) {
                                               return (!item.stableId.isEmpty() &&
                                                       candidate.stableId == item.stableId) ||
                                                      (!item.transactionId.isEmpty() &&
                                                       candidate.transactionId == item.transactionId);
                                           });
        if (deferred == m_deferredLiveItems.end())
            m_deferredLiveItems.append(item);
        else if (deferred->revision <= item.revision)
            *deferred = item;
        if (m_deferredLiveItems.size() > MaximumWindowSize)
            m_deferredLiveItems.removeFirst();
    }
    if (knownItem || item.sender.own)
        return;

    if (m_active && m_atNewest && !m_hasNewer)
        markNewestEventVisible();
    else
        setNewEventsBelow(m_newEventsBelow + 1);
}

void ChatTimelineController::eventUpdated(const Chat &chat, const ChatTimelineItem &item)
{
    if (chat != m_chat)
        return;

    // Updates can concern any cached event, including messages outside this
    // window (decryption, reactions, member details). They must not extend it.
    if (m_timeline->rowForStableId(item.stableId) >= 0 ||
        m_timeline->rowForTransactionId(item.transactionId) >= 0)
        m_timeline->upsert(item);
    else
    {
        const auto deferred = std::find_if(m_deferredLiveItems.begin(), m_deferredLiveItems.end(),
                                           [&item](const ChatTimelineItem &candidate) {
                                               return (!item.stableId.isEmpty() &&
                                                       candidate.stableId == item.stableId) ||
                                                      (!item.transactionId.isEmpty() &&
                                                       candidate.transactionId == item.transactionId);
                                           });
        if (deferred != m_deferredLiveItems.end() && deferred->revision <= item.revision)
            *deferred = item;
    }
}

void ChatTimelineController::eventRedacted(const Chat &chat, const QString &stableId, const QString &reason)
{
    if (chat != m_chat)
        return;

    m_timeline->redact(stableId, reason);
    m_deferredLiveItems.erase(
        std::remove_if(m_deferredLiveItems.begin(), m_deferredLiveItems.end(),
                       [&stableId](const ChatTimelineItem &item) { return item.stableId == stableId; }),
        m_deferredLiveItems.end());
}
