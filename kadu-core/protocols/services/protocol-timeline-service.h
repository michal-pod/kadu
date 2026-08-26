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

#include "chat/timeline/chat-timeline-page.h"
#include "chat/timeline/chat-timeline-request.h"
#include "protocols/services/account-service.h"

#include <QtCore/QFuture>

/**
 * @short Source of protocol-native timeline pages and live events.
 *
 * Unlike ProtocolHistoryService, this service preserves the identities and
 * ordering supplied by the remote protocol. It intentionally coexists with
 * the legacy history service while the WebEngine chat view is still present.
 */
class KADUAPI ProtocolTimelineService : public AccountService
{
    Q_OBJECT

public:
    explicit ProtocolTimelineService(Account account, QObject *parent = nullptr);
    virtual ~ProtocolTimelineService();

    /**
     * @short Fetch one timeline page for a chat.
     *
     * Items in the page must retain their stable ID, transaction ID and source
     * order. A page can be empty while still carrying a usable next cursor.
     */
    virtual QFuture<ChatTimelinePage> requestTimeline(const ChatTimelineRequest &request) = 0;

signals:
    /**
     * @short A new protocol event became available for a chat.
     */
    void eventReceived(const Chat &chat, const ChatTimelineItem &item);

    /**
     * @short An existing protocol event changed, for example after an edit or decryption.
     */
    void eventUpdated(const Chat &chat, const ChatTimelineItem &item);

    /**
     * @short A protocol event was redacted without requiring another history page.
     */
    void eventRedacted(const Chat &chat, const QString &stableId, const QString &reason);
};
