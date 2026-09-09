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
#include <QtCore/QVariantMap>

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
     * Cursors describe the edges of this page. For Older/Newer requests with
     * an empty cursor, resume immediately beyond anchorId in that direction;
     * the controller uses this after trimming its visible window.
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
     * @short Remove the current user's annotation with @a key from a timeline event.
     *
     * A reaction chip only passes its target event ID and key across the QML
     * boundary. The protocol keeps ownership of reaction event IDs and decides
     * whether the current user has an annotation that can be removed.
     */
    virtual bool removeOwnReaction(const Chat &chat, const QString &stableId, const QString &key);

    /**
     * @short Add an annotation to one timeline event.
     *
     * The view supplies only the protocol-neutral event ID and the selected
     * Unicode key. Protocols own the relation/event representation.
     */
    virtual bool addReaction(const Chat &chat, const QString &stableId, const QString &key);

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
     * @short Return protocol-neutral room state and the local member's capabilities.
     *
     * The returned map is deliberately a small, stable QML contract.  Its
     * standard keys are:
     * - group: whether the chat has room/group semantics;
     * - ircModes: compact, display-only mode string (for example +imntE);
     * - flags: named protocol-neutral room properties;
     * - historyVisibility: visibility policy, when the protocol defines one;
     * - memberPrefix and memberRole: the local member's IRC-like display
     *   prefix and normalized role;
     * - permissions: a map of normalized boolean capabilities; and
     * - native: diagnostic protocol details.
     *
     * Values in native must not be required by a chat style.  A protocol must
     * prefer a named flag over an approximate IRC mode whenever the two
     * concepts do not have the same semantics.
     *
     * Protocols that do not expose room metadata return an empty map.  This
     * lets a renderer keep the same status-bar contract for rooms, MUCs and
     * one-to-one chats without learning protocol-specific state formats.
     */
    virtual QVariantMap roomInfo(const Chat &chat) const;

    /**
     * @short Return a protocol-owned title for the timeline header.
     *
     * The title is deliberately separate from Chat::display(), which is also
     * used by tabs and window titles. A timeline provider can therefore
     * describe direct conversations and rooms in its own vocabulary without
     * teaching the QML view about protocol-specific chat types.
     */
    virtual QString chatHeaderTitle(const Chat &chat) const;

    /**
     * @short Mark a timeline event as read in the native protocol.
     *
     * The controller calls this only for an event at or before the visible read
     * boundary. Presentation-only filters may hide the event itself. A protocol
     * without read receipts can retain the default no-op behaviour.
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

    /**
     * @short A protocol changed room state or the local member's capabilities.
     */
    void roomInfoChanged(const Chat &chat);

    /**
     * @short The protocol changed title, avatar or description metadata shown above a timeline.
     */
    void chatHeaderChanged(const Chat &chat);

    /**
     * @short A non-blocking warning associated with a timeline event.
     *
     * Timeline implementations must use this instead of showing a modal
     * QWidget message box while handling an action initiated from QML.  The
     * view anchors the transient notification next to the event when it is
     * visible.
     */
    void timelineWarning(const Chat &chat, const QString &stableId, const QString &title, const QString &message);
};
