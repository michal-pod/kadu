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

#include "protocols/services/protocol-timeline-service.h"

#include <Quotient/events/filesourceinfo.h>

#include <QtCore/QPointer>
#include <QtCore/QPromise>
#include <QtCore/QSet>
#include <QtCore/QHash>
#include <QtGui/QImage>
#include <injeqt/injeqt.h>

#include <memory>

class ChatManager;
class ChatStorage;
class ContactManager;

namespace Quotient
{
class Connection;
class DownloadFileJob;
class Room;
class RoomMessageEvent;
}

class MatrixTimelineService final : public ProtocolTimelineService
{
    Q_OBJECT

public:
    explicit MatrixTimelineService(Account account, QObject *parent = nullptr);
    virtual ~MatrixTimelineService();

    void setConnection(Quotient::Connection *connection);

    QFuture<ChatTimelinePage> requestTimeline(const ChatTimelineRequest &request) override;
    ChatTimelineActions availableActions(const Chat &chat, const QString &stableId) const override;
    bool executeAction(const Chat &chat, const QString &stableId, ChatTimelineAction action) override;
    QImage requestAttachmentImage(const Chat &chat, const QUrl &sourceUri, const QSize &requestedSize) override;

private:
    QPointer<ChatManager> m_chatManager;
    QPointer<ChatStorage> m_chatStorage;
    QPointer<ContactManager> m_contactManager;
    QPointer<Quotient::Connection> m_connection;
    QSet<Quotient::Room *> m_watchedRooms;
    QSet<Quotient::Room *> m_loadedRooms;
    QSet<QString> m_historicalEventIds;
    QHash<QString, QImage> m_attachmentImages;
    QHash<QString, ChatTimelineAttachmentState> m_attachmentStates;
    QHash<QString, qreal> m_attachmentProgress;
    QHash<QString, QString> m_attachmentErrors;
    QHash<QString, QString> m_attachmentDownloadPaths;
    mutable QHash<QString, Quotient::FileSourceInfo> m_attachmentSources;
    mutable QHash<QString, QString> m_attachmentFileNames;

    Chat chatForRoom(Quotient::Room *room) const;
    Quotient::Room *roomForChat(const Chat &chat) const;
    void watchRoom(Quotient::Room *room);
    void handleNewMessages(Quotient::Room *room, int fromIndex, int toIndex);
    QFuture<ChatTimelinePage> waitForRoomInitialState(const ChatTimelineRequest &request, Quotient::Room *room);
    QFuture<ChatTimelinePage> requestTimelineForRoom(const ChatTimelineRequest &request, Quotient::Room *room);
    ChatTimelinePage pageForRoom(const ChatTimelineRequest &request, Quotient::Room *room) const;
    ChatTimelineItem itemForEvent(const Quotient::RoomMessageEvent &event, const QString &eventId,
                                  qint64 timelineIndex, bool encrypted) const;
    void updateAttachmentEvent(Quotient::Room *room, const QString &eventId);
    void handleAttachmentDownloadProgress(Quotient::Room *room, const QString &eventId, qint64 received,
                                          qint64 total);
    void handleAttachmentDownloadCompleted(Quotient::Room *room, const QString &eventId,
                                           const QUrl &localFile);
    void handleAttachmentDownloadFailed(Quotient::Room *room, const QString &eventId, const QString &errorMessage);
    void clearAttachmentDownloads();
    static QUrl attachmentUri(const QString &eventId);
    static QString eventIdForAttachmentUri(const QUrl &sourceUri);
    QByteArray sourceOrderForIndex(qint64 timelineIndex) const;
    QFuture<ChatTimelinePage> completedPage(ChatTimelinePage page) const;
    void finishRequest(const std::shared_ptr<QPromise<ChatTimelinePage>> &promise, ChatTimelinePage page) const;

private slots:
    INJEQT_SET void setChatManager(ChatManager *chatManager);
    INJEQT_SET void setChatStorage(ChatStorage *chatStorage);
    INJEQT_SET void setContactManager(ContactManager *contactManager);
};
