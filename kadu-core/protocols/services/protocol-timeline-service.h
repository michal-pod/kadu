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

#include "chat/timeline/chat-timeline-action.h"
#include "chat/timeline/chat-timeline-page.h"
#include "chat/timeline/chat-timeline-request.h"
#include "protocols/services/account-service.h"

#include <QtCore/QFuture>
#include <QtCore/QSize>
#include <QtCore/QUrl>
#include <QtCore/QVariantList>

class QImage;

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

    /**
     * @short Return operations offered by the protocol for one timeline event.
     *
     * The QML view only receives the event ID and an action identifier.  The
     * protocol remains responsible for locating the event and any attachment
     * data, so protocol internals and local paths never cross the QML boundary.
     */
    virtual ChatTimelineActions availableActions(const Chat &chat, const QString &stableId) const;

    /**
     * @short Execute an operation previously returned by availableActions().
     */
    virtual bool executeAction(const Chat &chat, const QString &stableId, ChatTimelineAction action);

    /**
     * @short Return protocol-native pinned entries for a chat.
     *
     * The list keeps the protocol order and contains lightweight QVariantMap
     * records only. A record can deliberately represent an event that is not
     * in the locally loaded timeline yet; the pinned-messages view must not
     * force history pagination just to build its preview.
     */
    virtual QVariantList pinnedMessages(const Chat &chat) const;

    /**
     * @short Mark a timeline event as read in the native protocol.
     *
     * The controller calls this only while the event is visibly read.  A
     * protocol without read receipts can retain the default no-op behaviour.
     */
    virtual void markTimelineItemRead(const Chat &chat, const QString &stableId);

    /**
     * @short Return a cached timeline attachment image and start loading it when necessary.
     *
     * The QML image provider calls this method with a protocol-owned kaduimg:
     * URI. Implementations must return promptly; an empty image means that the
     * attachment is still being retrieved. Once it becomes available, the
     * implementation updates the matching timeline event through eventUpdated().
     */
    virtual QImage requestAttachmentImage(const Chat &chat, const QUrl &sourceUri, const QSize &requestedSize);

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

    /**
     * @short Permissions or protocol state changed the operations offered for timeline events.
     */
    void availableActionsChanged(const Chat &chat);

    /**
     * @short The protocol changed the pinned entries of a chat.
     */
    void pinnedMessagesChanged(const Chat &chat);
};
