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

#pragma once

#include "chat/chat.h"
#include "chat/timeline/chat-timeline-model.h"
#include "exports.h"

#include <QtCore/QFuture>
#include <QtCore/QPointer>

class ProtocolTimelineService;

/**
 * @short Coordinates one chat's timeline model and its protocol source.
 *
 * The controller owns no network state. It issues page requests through the
 * protocol service and ignores results belonging to a superseded session.
 * Live events are applied immediately in the latest window and deferred while
 * a historical window is visible. ChatTimelineModel deduplicates merged pages
 * by stable and transaction IDs.
 */
class KADUAPI ChatTimelineController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(ChatTimelineModel *timeline READ timeline CONSTANT)
    Q_PROPERTY(bool loadingInitial READ isLoadingInitial NOTIFY loadingInitialChanged)
    Q_PROPERTY(bool loadingOlder READ isLoadingOlder NOTIFY loadingOlderChanged)
    Q_PROPERTY(bool loadingNewer READ isLoadingNewer NOTIFY loadingNewerChanged)
    Q_PROPERTY(bool hasOlder READ hasOlder NOTIFY hasOlderChanged)
    Q_PROPERTY(bool hasNewer READ hasNewer NOTIFY hasNewerChanged)
    Q_PROPERTY(QString historyError READ historyError NOTIFY historyErrorChanged)
    Q_PROPERTY(bool active READ isActive WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(bool atNewest READ isAtNewest WRITE setAtNewest NOTIFY atNewestChanged)
    Q_PROPERTY(QString readMarkerId READ readMarkerId NOTIFY readMarkerIdChanged)
    Q_PROPERTY(int newEventsBelow READ newEventsBelow NOTIFY newEventsBelowChanged)

public:
    explicit ChatTimelineController(Chat chat, ProtocolTimelineService *timelineService, QObject *parent = nullptr);
    virtual ~ChatTimelineController();

    Chat chat() const;
    ChatTimelineModel *timeline() const;
    bool isLoadingInitial() const;
    bool isLoadingOlder() const;
    bool isLoadingNewer() const;
    bool hasOlder() const;
    bool hasNewer() const;
    QString historyError() const;
    bool isActive() const;
    bool isAtNewest() const;
    QString readMarkerId() const;
    int newEventsBelow() const;

public slots:
    void loadInitial(int limit = 50);
    void loadLatest(int limit = 50);
    void loadOlder(int limit = 50);
    void loadNewer(int limit = 50);
    void jumpTo(const QString &stableId, int limit = 100);
    void retryHistory();
    void cancelRequests();
    void setActive(bool active);
    void setAtNewest(bool atNewest);
    void markVisible(const QString &stableId);

signals:
    void loadingInitialChanged();
    void loadingOlderChanged();
    void loadingNewerChanged();
    void hasOlderChanged();
    void hasNewerChanged();
    void historyErrorChanged();
    void activeChanged();
    void atNewestChanged();
    void readMarkerIdChanged();
    void newEventsBelowChanged();
    void timelinePositionRequested(const QString &stableId);

private:
    enum class RequestKind
    {
        Latest,
        Older,
        Newer,
        Around
    };

    Chat m_chat;
    QPointer<ProtocolTimelineService> m_timelineService;
    ChatTimelineModel *m_timeline = nullptr;
    QByteArray m_olderCursor;
    QByteArray m_newerCursor;
    QVector<ChatTimelineItem> m_deferredLiveItems;
    QString m_historyError;
    QString m_readMarkerId;
    QFuture<void> m_pageContinuation;
    quint64 m_requestGeneration = 0;
    int m_newEventsBelow = 0;
    bool m_loadingInitial = false;
    bool m_loadingOlder = false;
    bool m_loadingNewer = false;
    bool m_hasOlder = false;
    bool m_hasNewer = false;
    bool m_active = false;
    bool m_atNewest = false;

    static constexpr int MaximumWindowSize = 100;

    void requestPage(RequestKind requestKind, const QByteArray &cursor, const QString &anchorId, int limit);
    void pageAvailable(RequestKind requestKind, quint64 generation, const QByteArray &requestedCursor,
                       const QString &requestedAnchor, const ChatTimelinePage &page);
    void setLoadingInitial(bool loading);
    void setLoadingOlder(bool loading);
    void setLoadingNewer(bool loading);
    void setHasOlder(bool hasOlder);
    void setHasNewer(bool hasNewer);
    void setHistoryError(const QString &error);
    void setReadMarkerId(const QString &stableId);
    void setNewEventsBelow(int count);
    void markNewestEventVisible();

private slots:
    void eventReceived(const Chat &chat, const ChatTimelineItem &item);
    void eventUpdated(const Chat &chat, const ChatTimelineItem &item);
    void eventRedacted(const Chat &chat, const QString &stableId, const QString &reason);
};
