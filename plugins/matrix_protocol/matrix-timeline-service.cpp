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

#include "matrix-timeline-service.h"

#include "chat/chat-details-room.h"
#include "chat/chat-manager.h"
#include "chat/chat-storage.h"
#include "chat/type/chat-type-contact.h"
#include "chat/type/chat-type-room.h"
#include "contacts/contact-manager.h"
#include "html/html-conversion.h"
#include "html/html-string.h"
#include "html/sanitized-html-string.h"

#include <Quotient/connection.h>
#include <Quotient/events/encryptedevent.h>
#include <Quotient/events/eventcontent.h>
#include <Quotient/events/roommessageevent.h>
#include <Quotient/events/roomevent.h>
#include <Quotient/room.h>
#include <Quotient/user.h>

#include <QtCore/QFutureWatcher>

#include <algorithm>
#include <limits>
#include <utility>

MatrixTimelineService::MatrixTimelineService(Account account, QObject *parent)
        : ProtocolTimelineService{account, parent}
{
}

void MatrixTimelineService::setConnection(Quotient::Connection *connection)
{
    if (m_connection == connection)
        return;

    if (m_connection)
        disconnect(m_connection, nullptr, this, nullptr);
    for (auto *watchedRoom : std::as_const(m_watchedRooms))
        disconnect(watchedRoom, nullptr, this, nullptr);

    m_connection = connection;
    m_watchedRooms.clear();
    m_loadedRooms.clear();
    m_historicalEventIds.clear();
    if (!m_connection)
        return;

    connect(m_connection, &Quotient::Connection::newRoom, this, &MatrixTimelineService::watchRoom);
    connect(m_connection, &Quotient::Connection::joinedRoom, this,
            [this](Quotient::Room *room, Quotient::Room *) { watchRoom(room); });

    for (auto *room : m_connection->allRooms())
        watchRoom(room);
}

QFuture<ChatTimelinePage> MatrixTimelineService::requestTimeline(const ChatTimelineRequest &request)
{
    if (!m_connection || !request.chat)
    {
        ChatTimelinePage page;
        page.error = tr("Matrix room is not available.");
        return completedPage(std::move(page));
    }

    return requestTimelineForRoom(request, roomForChat(request.chat));
}

QFuture<ChatTimelinePage> MatrixTimelineService::requestTimelineForRoom(const ChatTimelineRequest &request,
                                                                          Quotient::Room *room)
{
    if (!room)
    {
        ChatTimelinePage page;
        page.error = tr("Matrix room is not available.");
        return completedPage(std::move(page));
    }

    if (!m_loadedRooms.contains(room))
        return waitForRoomInitialState(request, room);

    if (request.direction != ChatTimelineDirection::Older || room->allHistoryLoaded())
        return completedPage(pageForRoom(request, room));

    const auto requestedLimit = request.limit > 0 ? request.limit : 50;
    const auto loadLimit = std::max(requestedLimit, 50);
    auto promise = std::make_shared<QPromise<ChatTimelinePage>>();
    auto future = promise->future();
    promise->start();

    const QPointer<Quotient::Room> watchedRoom{room};
    room->getPreviousContent(loadLimit).then(
        this,
        [this, promise, request, watchedRoom] {
            if (!watchedRoom)
            {
                ChatTimelinePage page;
                page.error = tr("Matrix room is no longer available.");
                finishRequest(promise, std::move(page));
                return;
            }

            finishRequest(promise, pageForRoom(request, watchedRoom.data()));
        },
        [this, promise] {
            ChatTimelinePage page;
            page.error = tr("Could not retrieve Matrix history.");
            finishRequest(promise, std::move(page));
        });
    return future;
}

QFuture<ChatTimelinePage> MatrixTimelineService::waitForRoomInitialState(const ChatTimelineRequest &request,
                                                                            Quotient::Room *room)
{
    auto promise = std::make_shared<QPromise<ChatTimelinePage>>();
    auto future = promise->future();
    promise->start();

    const QPointer<Quotient::Room> watchedRoom{room};
    connect(room, &Quotient::Room::baseStateLoaded, this, [this, promise, request, watchedRoom] {
        if (!watchedRoom)
        {
            ChatTimelinePage page;
            page.error = tr("Matrix room is no longer available.");
            finishRequest(promise, std::move(page));
            return;
        }

        auto *futureWatcher = new QFutureWatcher<ChatTimelinePage>{this};
        connect(futureWatcher, &QFutureWatcher<ChatTimelinePage>::finished, this,
                [this, promise, futureWatcher] {
                    finishRequest(promise, futureWatcher->future().result());
                    futureWatcher->deleteLater();
                });
        futureWatcher->setFuture(requestTimelineForRoom(request, watchedRoom.data()));
    });
    connect(room, &QObject::destroyed, this, [this, promise] {
        ChatTimelinePage page;
        page.error = tr("Matrix room is no longer available.");
        finishRequest(promise, std::move(page));
    });
    return future;
}

void MatrixTimelineService::setChatManager(ChatManager *chatManager)
{
    m_chatManager = chatManager;
}

void MatrixTimelineService::setChatStorage(ChatStorage *chatStorage)
{
    m_chatStorage = chatStorage;
}

void MatrixTimelineService::setContactManager(ContactManager *contactManager)
{
    m_contactManager = contactManager;
}

Chat MatrixTimelineService::chatForRoom(Quotient::Room *room) const
{
    if (!m_connection || !m_chatManager || !m_chatStorage || !room)
        return Chat::null;

    if (!m_connection->isDirectChat(room->id()))
        return ChatTypeRoom::findChat(m_chatManager, m_chatStorage, account(), room->id(), ActionCreateAndAdd);

    if (!m_contactManager)
        return Chat::null;

    const auto directChats = m_connection->directChats();
    for (auto it = directChats.cbegin(); it != directChats.cend(); ++it)
    {
        if (it.value() != room->id() || !it.key())
            continue;

        const auto contact = m_contactManager->byId(account(), it.key()->id(), ActionCreateAndAdd);
        return contact ? ChatTypeContact::findChat(m_chatManager, m_chatStorage, contact, ActionCreateAndAdd)
                       : Chat::null;
    }
    return Chat::null;
}

Quotient::Room *MatrixTimelineService::roomForChat(const Chat &chat) const
{
    if (!m_connection || !chat)
        return nullptr;

    if (const auto *details = qobject_cast<ChatDetailsRoom *>(chat.details()))
        return m_connection->room(details->room(), Quotient::JoinState::Join);

    const auto contacts = chat.contacts().toContactVector();
    if (contacts.size() != 1)
        return nullptr;

    const auto directChats = m_connection->directChats();
    for (auto it = directChats.cbegin(); it != directChats.cend(); ++it)
    {
        if (it.key() && it.key()->id() == contacts.constFirst().id())
            return m_connection->room(it.value(), Quotient::JoinState::Join);
    }
    return nullptr;
}

void MatrixTimelineService::watchRoom(Quotient::Room *room)
{
    if (!room || m_watchedRooms.contains(room))
        return;

    m_watchedRooms.insert(room);
    connect(room, &Quotient::Room::addedMessages, this,
            [this, room](int fromIndex, int toIndex) { handleNewMessages(room, fromIndex, toIndex); });
    connect(room, &Quotient::Room::aboutToAddHistoricalMessages, this,
            [this](Quotient::RoomEventsRange events) {
                for (const auto &event : events)
                    if (event)
                        m_historicalEventIds.insert(event->id());
            });
    connect(room, &Quotient::Room::baseStateLoaded, this,
            [this, room] { m_loadedRooms.insert(room); });
    connect(room, &QObject::destroyed, this, [this, room] {
        m_watchedRooms.remove(room);
        m_loadedRooms.remove(room);
    });
}

void MatrixTimelineService::handleNewMessages(Quotient::Room *room, int fromIndex, int toIndex)
{
    if (!m_loadedRooms.contains(room))
        return;

    const auto chat = chatForRoom(room);
    if (!chat || chat.isIgnoreAllMessages())
        return;

    for (const auto &timelineItem : room->messageEvents())
    {
        if (timelineItem.index() < fromIndex || timelineItem.index() > toIndex)
            continue;
        if (m_historicalEventIds.remove(timelineItem->id()))
            continue;

        const auto *event = timelineItem.viewAs<Quotient::RoomMessageEvent>();
        Quotient::RoomEventPtr decryptedEvent;
        auto encrypted = false;
        if (!event)
        {
            if (const auto *encryptedEvent = timelineItem.viewAs<Quotient::EncryptedEvent>())
            {
                encrypted = true;
                decryptedEvent = room->decryptMessage(*encryptedEvent);
                event = Quotient::eventCast<const Quotient::RoomMessageEvent>(decryptedEvent);
            }
        }
        if (!event || event->isRedacted())
            continue;

        emit eventReceived(chat, itemForEvent(*event, timelineItem->id(), timelineItem.index(), encrypted));
    }
}

ChatTimelinePage MatrixTimelineService::pageForRoom(const ChatTimelineRequest &request, Quotient::Room *room) const
{
    ChatTimelinePage page;
    if (!room)
    {
        page.error = tr("Matrix room is not available.");
        return page;
    }

    const auto limit = request.limit > 0 ? request.limit : 50;
    bool validCursor = false;
    const auto cursor = QString::fromUtf8(request.cursor).toLongLong(&validCursor);
    const auto boundary = validCursor ? cursor : std::numeric_limits<qint64>::max();

    qint64 nextCursor = 0;
    auto accepted = 0;
    for (auto it = room->messageEvents().crbegin(); it != room->messageEvents().crend(); ++it)
    {
        const auto index = static_cast<qint64>(it->index());
        if (index >= boundary)
            continue;

        const auto *event = it->viewAs<Quotient::RoomMessageEvent>();
        Quotient::RoomEventPtr decryptedEvent;
        auto encrypted = false;
        if (!event)
        {
            if (const auto *encryptedEvent = it->viewAs<Quotient::EncryptedEvent>())
            {
                encrypted = true;
                decryptedEvent = room->decryptMessage(*encryptedEvent);
                event = Quotient::eventCast<const Quotient::RoomMessageEvent>(decryptedEvent);
            }
        }
        if (!event || event->isRedacted())
            continue;

        page.items.append(itemForEvent(*event, (*it)->id(), index, encrypted));
        nextCursor = index;
        ++accepted;
        if (accepted == limit)
            break;
    }

    if (accepted > 0)
        page.cursor = QByteArray::number(nextCursor);
    else if (!room->messageEvents().empty() && !room->allHistoryLoaded())
        page.cursor = QByteArray::number(room->minTimelineIndex());
    page.hasMore = accepted == limit || !room->allHistoryLoaded();
    return page;
}

ChatTimelineItem MatrixTimelineService::itemForEvent(const Quotient::RoomMessageEvent &event, const QString &eventId,
                                                      qint64 timelineIndex, bool encrypted) const
{
    ChatTimelineItem item;
    item.stableId = eventId;
    item.transactionId = event.transactionId();
    item.sourceOrder = sourceOrderForIndex(timelineIndex);
    item.timestamp = event.originTimestamp().toLocalTime();
    item.sender.id = event.senderId();
    item.sender.displayName = event.senderId();
    item.sender.own = m_connection && event.senderId() == m_connection->userId();
    item.content.plainText = event.plainBody();
    if (const auto textContent = event.get<Quotient::EventContent::TextContent>();
        textContent && textContent->mimeType.inherits(QStringLiteral("text/html")))
    {
        item.content.formattedText = sanitizeHtml(HtmlString{textContent->body}).string();
    }
    item.state.deliveryState = item.sender.own ? ChatTimelineDeliveryState::Sent
                                                : ChatTimelineDeliveryState::Delivered;
    item.state.encrypted = encrypted;
    item.state.decryptionState = encrypted ? ChatTimelineDecryptionState::Decrypted
                                            : ChatTimelineDecryptionState::NotEncrypted;

    switch (event.msgtype())
    {
    case Quotient::RoomMessageEvent::MsgType::Notice: item.kind = ChatTimelineItemKind::NoticeMessage; break;
    case Quotient::RoomMessageEvent::MsgType::Emote: item.kind = ChatTimelineItemKind::EmoteMessage; break;
    default: item.kind = ChatTimelineItemKind::TextMessage; break;
    }
    return item;
}

QByteArray MatrixTimelineService::sourceOrderForIndex(qint64 timelineIndex) const
{
    const auto unsignedIndex = static_cast<quint64>(timelineIndex) ^ (quint64{1} << 63);
    return QByteArray::number(unsignedIndex, 16).rightJustified(16, '0');
}

QFuture<ChatTimelinePage> MatrixTimelineService::completedPage(ChatTimelinePage page) const
{
    QPromise<ChatTimelinePage> promise;
    auto future = promise.future();
    promise.start();
    promise.addResult(std::move(page));
    promise.finish();
    return future;
}

void MatrixTimelineService::finishRequest(const std::shared_ptr<QPromise<ChatTimelinePage>> &promise,
                                          ChatTimelinePage page) const
{
    promise->addResult(std::move(page));
    promise->finish();
}
