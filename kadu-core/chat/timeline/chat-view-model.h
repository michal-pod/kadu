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
#include "chat/timeline/legacy-message-timeline-adapter.h"
#include "exports.h"

class ChatTimelineController;
class ChatStyleManager;
class Message;
class ProtocolTimelineService;
class SortedMessages;

/**
 * @short QML-facing state of one chat card.
 *
 * The view model chooses a protocol-native timeline when it is available. A
 * legacy protocol still receives a neutral timeline through
 * LegacyMessageTimelineAdapter, so QML never has to inspect Message objects.
 */
class KADUAPI ChatViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(ChatTimelineModel *timeline READ timeline CONSTANT)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(QString theme READ theme NOTIFY themeChanged)
    Q_PROPERTY(bool usesProtocolTimeline READ usesProtocolTimeline CONSTANT)
    Q_PROPERTY(bool loadingInitial READ loadingInitial NOTIFY timelineStateChanged)
    Q_PROPERTY(bool loadingOlder READ loadingOlder NOTIFY timelineStateChanged)
    Q_PROPERTY(bool hasOlder READ hasOlder NOTIFY timelineStateChanged)

public:
    explicit ChatViewModel(Chat chat, ProtocolTimelineService *timelineService = nullptr,
                           ChatStyleManager *chatStyleManager = nullptr, QObject *parent = nullptr);
    virtual ~ChatViewModel();

    Chat chat() const;
    ChatTimelineModel *timeline() const;
    QString title() const;
    QString theme() const;
    bool usesProtocolTimeline() const;
    bool loadingInitial() const;
    bool loadingOlder() const;
    bool hasOlder() const;

    void addLegacyMessage(const Message &message);
    void addLegacyMessages(const SortedMessages &messages);

public slots:
    void open();
    void close();
    void loadOlder();

signals:
    void titleChanged();
    void themeChanged();
    void timelineStateChanged();

private:
    Chat m_chat;
    ChatTimelineController *m_timelineController = nullptr;
    ChatTimelineModel *m_timeline = nullptr;
    LegacyMessageTimelineAdapter m_legacyAdapter;
    ChatStyleManager *m_chatStyleManager = nullptr;
    bool m_open = false;

    ProtocolTimelineService *timelineService(ProtocolTimelineService *service) const;

private slots:
    void chatUpdated();
    void styleChanged();
    void timelineStateChangedSlot();
};
