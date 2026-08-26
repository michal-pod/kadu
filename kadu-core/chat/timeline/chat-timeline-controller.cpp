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

bool ChatTimelineController::hasOlder() const
{
    return m_hasOlder;
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
    if (!m_timelineService)
        return;

    cancelRequests();
    m_timeline->clear();
    m_olderCursor.clear();
    setHasOlder(false);
    setHistoryError({});
    setReadMarkerId({});
    setNewEventsBelow(0);
    requestPage(RequestKind::Initial, {}, limit);
}

void ChatTimelineController::loadOlder(int limit)
{
    if (!m_timelineService || m_loadingInitial || m_loadingOlder || !m_hasOlder || m_olderCursor.isEmpty())
        return;

    setHistoryError({});
    requestPage(RequestKind::Older, m_olderCursor, limit);
}

void ChatTimelineController::retryHistory()
{
    if (m_loadingInitial || m_loadingOlder)
        return;

    if (m_timeline->rowCount() == 0)
        loadInitial();
    else
        loadOlder();
}

void ChatTimelineController::cancelRequests()
{
    ++m_requestGeneration;
    if (m_pageContinuation.isValid())
        m_pageContinuation.cancel();
    m_pageContinuation = {};
    setLoadingInitial(false);
    setLoadingOlder(false);
}

void ChatTimelineController::setActive(bool active)
{
    if (m_active == active)
        return;

    m_active = active;
    emit activeChanged();
    if (m_active && m_atNewest)
        markNewestEventVisible();
}

void ChatTimelineController::setAtNewest(bool atNewest)
{
    if (m_atNewest == atNewest)
        return;

    m_atNewest = atNewest;
    emit atNewestChanged();
    if (m_active && m_atNewest)
        markNewestEventVisible();
}

void ChatTimelineController::markVisible(const QString &stableId)
{
    const auto row = m_timeline->rowForStableId(stableId);
    if (row < 0)
        return;

    setReadMarkerId(stableId);
    if (row == m_timeline->rowCount() - 1)
        setNewEventsBelow(0);
}

void ChatTimelineController::requestPage(RequestKind requestKind, const QByteArray &cursor, int limit)
{
    if (!m_timelineService)
        return;

    ChatTimelineRequest request;
    request.chat = m_chat;
    request.cursor = cursor;
    request.direction = ChatTimelineDirection::Older;
    request.limit = qMax(1, limit);

    if (requestKind == RequestKind::Initial)
        setLoadingInitial(true);
    else
        setLoadingOlder(true);

    const auto generation = ++m_requestGeneration;
    auto future = m_timelineService->requestTimeline(request);
    m_pageContinuation = future.then(this, [this, requestKind, generation, cursor](const ChatTimelinePage &page) {
        pageAvailable(requestKind, generation, cursor, page);
    });
}

void ChatTimelineController::pageAvailable(RequestKind requestKind, quint64 generation, const QByteArray &requestedCursor,
                                           const ChatTimelinePage &page)
{
    if (generation != m_requestGeneration)
        return;

    if (requestKind == RequestKind::Initial)
        setLoadingInitial(false);
    else
        setLoadingOlder(false);

    if (!page.error.isEmpty())
    {
        setHistoryError(page.error);
        if (requestKind == RequestKind::Older)
            setHasOlder(false);
        return;
    }

    // Do not reset after the first request: live events can arrive while the
    // future is pending. Upserting the page preserves those events and lets
    // the model merge any duplicates by stable or transaction ID.
    if (requestKind == RequestKind::Initial)
        m_timeline->append(page);
    else
        m_timeline->prepend(page);

    if (m_active && m_atNewest)
        markNewestEventVisible();

    const auto hasUsableNextCursor = page.hasMore && !page.cursor.isEmpty() && page.cursor != requestedCursor;
    m_olderCursor = hasUsableNextCursor ? page.cursor : QByteArray{};
    setHasOlder(hasUsableNextCursor);

    // Servers can return a page containing only state events already merged
    // from live sync. Continue transparently while their cursor advances.
    if (page.items.isEmpty() && m_hasOlder)
        QTimer::singleShot(0, this, [this] { loadOlder(); });
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

void ChatTimelineController::setHasOlder(bool hasOlder)
{
    if (m_hasOlder == hasOlder)
        return;

    m_hasOlder = hasOlder;
    emit hasOlderChanged();
}

void ChatTimelineController::setHistoryError(const QString &error)
{
    if (m_historyError == error)
        return;

    m_historyError = error;
    emit historyErrorChanged();
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
    m_timeline->upsert(item);
    if (knownItem || item.sender.own)
        return;

    if (m_active && m_atNewest)
        markNewestEventVisible();
    else
        setNewEventsBelow(m_newEventsBelow + 1);
}

void ChatTimelineController::eventUpdated(const Chat &chat, const ChatTimelineItem &item)
{
    if (chat != m_chat)
        return;

    m_timeline->upsert(item);
}

void ChatTimelineController::eventRedacted(const Chat &chat, const QString &stableId, const QString &reason)
{
    if (chat != m_chat)
        return;

    m_timeline->redact(stableId, reason);
}
