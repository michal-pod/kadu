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

#include "matrix-history-service.h"

#include "matrix-room-state-registry.h"

#include "chat/chat-details-room.h"
#include "contacts/contact-manager.h"
#include "html/html-conversion.h"
#include "html/html-string.h"
#include "html/sanitized-html-string.h"
#include "message/message-storage.h"

#include <Quotient/connection.h>
#include <Quotient/csapi/search.h>
#include <Quotient/events/encryptedevent.h>
#include <Quotient/events/eventcontent.h>
#include <Quotient/events/roommessageevent.h>
#include <Quotient/events/roomevent.h>
#include <Quotient/room.h>
#include <Quotient/user.h>

#include <QtCore/QFutureWatcher>

#include <algorithm>
#include <limits>

MatrixHistoryService::MatrixHistoryService(Account account, QObject *parent)
        : ProtocolHistoryService{account, parent}
{
}

void MatrixHistoryService::setConnection(Quotient::Connection *connection)
{
    if (m_connection == connection)
        return;

    if (m_connection)
        disconnect(m_connection, nullptr, this, nullptr);

    m_connection = connection;
    m_watchedRooms.clear();
    if (!m_connection)
        return;

    connect(m_connection, &Quotient::Connection::newRoom, this, &MatrixHistoryService::watchRoom);
    connect(m_connection, &Quotient::Connection::joinedRoom, this,
            [this](Quotient::Room *room, Quotient::Room *) { watchRoom(room); });

    for (auto *room : m_connection->allRooms())
        watchRoom(room);
}

bool MatrixHistoryService::isLocalHistoryEnabled() const
{
    // A protocol-owned encrypted local store has not been implemented yet. In particular,
    // decrypted E2EE content must never fall back to the legacy unencrypted SQL history.
    return false;
}

void MatrixHistoryService::storeMessage(const Message &message)
{
    Q_UNUSED(message)
}

QFuture<ProtocolHistoryPage> MatrixHistoryService::requestHistory(const ProtocolHistoryRequest &request)
{
    if (!m_connection || !request.chat())
    {
        ProtocolHistoryPage page;
        page.setError(tr("Matrix room is not available."));
        return completedPage(std::move(page));
    }

    return requestHistoryForRoom(request, roomForChat(request.chat()));
}

QFuture<ProtocolHistoryPage> MatrixHistoryService::requestHistoryForRoom(
    const ProtocolHistoryRequest &request, Quotient::Room *room)
{
    if (!room)
    {
        ProtocolHistoryPage page;
        page.setError(tr("Matrix room is not available."));
        return completedPage(std::move(page));
    }

    if (!m_roomStateRegistry || !m_roomStateRegistry->isLoaded(room))
        return waitForRoomInitialState(request, room);

    if (!request.text().isEmpty())
        return searchRoom(request, room);

    if (request.direction() != ProtocolHistoryRequest::Direction::Older || room->allHistoryLoaded())
        return completedPage(pageForRoom(request, room));

    const auto requestedLimit = request.limit() > 0 ? request.limit() : 50;
    const auto loadLimit = std::max(requestedLimit, 50);
    auto promise = std::make_shared<QPromise<ProtocolHistoryPage>>();
    auto future = promise->future();
    promise->start();

    const QPointer<Quotient::Room> watchedRoom{room};
    room->getPreviousContent(loadLimit).then(
        this,
        [this, promise, request, watchedRoom] {
            if (!watchedRoom)
            {
                ProtocolHistoryPage page;
                page.setError(tr("Matrix room is no longer available."));
                finishRequest(promise, std::move(page));
                return;
            }

            finishRequest(promise, pageForRoom(request, watchedRoom.data()));
        },
        [this, promise] {
            ProtocolHistoryPage page;
            page.setError(tr("Could not retrieve Matrix history."));
            finishRequest(promise, std::move(page));
        });
    return future;
}

QFuture<ProtocolHistoryPage> MatrixHistoryService::searchRoom(const ProtocolHistoryRequest &request,
                                                               Quotient::Room *room)
{
    if (!m_connection || !room)
    {
        ProtocolHistoryPage page;
        page.setError(tr("Matrix room is not available."));
        return completedPage(std::move(page));
    }

    if (room->usesEncryption())
    {
        ProtocolHistoryPage page;
        page.setError(tr("Server-side search is not available for encrypted Matrix rooms."));
        return completedPage(std::move(page));
    }

    const auto limit = request.limit() > 0 ? request.limit() : 50;
    Quotient::SearchJob::RoomEventsCriteria criteria;
    criteria.searchTerm = request.text();
    criteria.keys = {QStringLiteral("content.body")};
    criteria.orderBy = QStringLiteral("recent");
    criteria.filter.limit = limit;
    criteria.filter.rooms = {room->id()};
    criteria.filter.types = {QStringLiteral("m.room.message")};

    Quotient::SearchJob::Categories categories;
    categories.roomEvents = std::move(criteria);

    auto promise = std::make_shared<QPromise<ProtocolHistoryPage>>();
    auto future = promise->future();
    promise->start();

    const QPointer<Quotient::Room> watchedRoom{room};
    m_connection->callApi<Quotient::SearchJob>(categories, QString::fromUtf8(request.cursor())).then(
        this,
        [this, promise, request, watchedRoom](Quotient::SearchJob *job) {
            ProtocolHistoryPage page;
            if (!watchedRoom || !job)
            {
                page.setError(tr("Matrix room is no longer available."));
                finishRequest(promise, std::move(page));
                return;
            }

            const auto categories = job->searchCategories();
            if (!categories.roomEvents)
            {
                finishRequest(promise, std::move(page));
                return;
            }

            SortedMessages messages;
            for (const auto &result : categories.roomEvents->results)
            {
                const auto *event = Quotient::eventCast<const Quotient::RoomMessageEvent>(result.result);
                if (!event || event->roomId() != watchedRoom->id() || event->isRedacted()
                    || event->msgtype() != Quotient::RoomMessageEvent::MsgType::Text)
                    continue;

                const auto message = messageForEvent(request.chat(), *event, event->id());
                if (!message.isNull())
                    messages.add(message);
            }

            page.setMessages(messages);
            page.setCursor(categories.roomEvents->nextBatch.toUtf8());
            page.setHasMore(!categories.roomEvents->nextBatch.isEmpty());
            finishRequest(promise, std::move(page));
        },
        [this, promise] {
            ProtocolHistoryPage page;
            page.setError(tr("Could not search Matrix history."));
            finishRequest(promise, std::move(page));
        });
    return future;
}

void MatrixHistoryService::watchRoom(Quotient::Room *room)
{
    if (!room || m_watchedRooms.contains(room))
        return;

    m_watchedRooms.insert(room);
    connect(room, &QObject::destroyed, this, [this, room] {
        m_watchedRooms.remove(room);
    });
}

QFuture<ProtocolHistoryPage> MatrixHistoryService::waitForRoomInitialState(
    const ProtocolHistoryRequest &request, Quotient::Room *room)
{
    auto promise = std::make_shared<QPromise<ProtocolHistoryPage>>();
    auto future = promise->future();
    promise->start();

    const QPointer<Quotient::Room> watchedRoom{room};
    if (!m_roomStateRegistry)
    {
        ProtocolHistoryPage page;
        page.setError(tr("Matrix room state service is not available."));
        finishRequest(promise, std::move(page));
        return future;
    }

    auto *requestContextObject = new QObject{this};
    const QPointer<QObject> requestContext{requestContextObject};
    connect(m_roomStateRegistry, &MatrixRoomStateRegistry::roomLoaded, requestContextObject,
            [this, promise, request, watchedRoom, requestContext](Quotient::Room *loadedRoom) {
                if (!watchedRoom || loadedRoom != watchedRoom.data())
                    return;

                auto *futureWatcher = new QFutureWatcher<ProtocolHistoryPage>{requestContext.data()};
                connect(futureWatcher, &QFutureWatcher<ProtocolHistoryPage>::finished, requestContext.data(),
                        [this, promise, requestContext, futureWatcher] {
                            finishRequest(promise, futureWatcher->future().result());
                            futureWatcher->deleteLater();
                            if (requestContext)
                                requestContext->deleteLater();
                        });
                futureWatcher->setFuture(requestHistoryForRoom(request, watchedRoom.data()));
            });
    connect(room, &QObject::destroyed, requestContextObject, [this, promise, requestContext] {
        ProtocolHistoryPage page;
        page.setError(tr("Matrix room is no longer available."));
        finishRequest(promise, std::move(page));
        if (requestContext)
            requestContext->deleteLater();
    });
    return future;
}

void MatrixHistoryService::setContactManager(ContactManager *contactManager)
{
    m_contactManager = contactManager;
}

void MatrixHistoryService::setMessageStorage(MessageStorage *messageStorage)
{
    m_messageStorage = messageStorage;
}

Quotient::Room *MatrixHistoryService::roomForChat(const Chat &chat) const
{
    if (!m_connection || !chat)
        return nullptr;

    if (const auto *details = qobject_cast<ChatDetailsRoom *>(chat.details()))
        return m_connection->room(details->room(), Quotient::JoinState::Join);
    return nullptr;
}

void MatrixHistoryService::setRoomStateRegistry(MatrixRoomStateRegistry *roomStateRegistry)
{
    m_roomStateRegistry = roomStateRegistry;
}

ProtocolHistoryPage MatrixHistoryService::pageForRoom(
    const ProtocolHistoryRequest &request, Quotient::Room *room) const
{
    ProtocolHistoryPage page;
    if (!room)
    {
        page.setError(tr("Matrix room is not available."));
        return page;
    }

    const auto limit = request.limit() > 0 ? request.limit() : 50;
    bool validCursor = false;
    const auto cursor = QString::fromUtf8(request.cursor()).toLongLong(&validCursor);
    const auto isOlderRequest = request.direction() == ProtocolHistoryRequest::Direction::Older;
    const auto boundary = validCursor
                              ? cursor
                              : (isOlderRequest ? std::numeric_limits<qint64>::max()
                                                : std::numeric_limits<qint64>::min());

    SortedMessages messages;
    qint64 nextCursor = 0;
    auto accepted = 0;
    for (auto it = room->messageEvents().crbegin(); it != room->messageEvents().crend(); ++it)
    {
        const auto index = static_cast<qint64>(it->index());
        if ((isOlderRequest && index >= boundary) || (!isOlderRequest && index <= boundary))
            continue;

        const auto *event = it->viewAs<Quotient::RoomMessageEvent>();
        Quotient::RoomEventPtr decryptedEvent;
        if (!event)
        {
            if (const auto *encryptedEvent = it->viewAs<Quotient::EncryptedEvent>())
            {
                // A recovery-key import adds Megolm sessions but libQuotient does not
                // revisit encrypted events already present in the timeline. Decrypt
                // those events when the history page is assembled instead.
                decryptedEvent = room->decryptMessage(*encryptedEvent);
                if (decryptedEvent)
                    event = Quotient::eventCast<const Quotient::RoomMessageEvent>(decryptedEvent);
            }
        }
        if (!event || event->isRedacted()
            || event->msgtype() != Quotient::RoomMessageEvent::MsgType::Text)
            continue;
        if (request.from().isValid() && event->originTimestamp() < request.from())
            continue;
        if (request.to().isValid() && event->originTimestamp() > request.to())
            continue;
        if (!request.text().isEmpty()
            && !event->plainBody().contains(request.text(), Qt::CaseInsensitive))
            continue;

        const auto message = messageForEvent(request.chat(), *event, (*it)->id());
        if (message.isNull())
            continue;

        messages.add(message);
        nextCursor = index;
        ++accepted;
        if (accepted == limit)
            break;
    }

    page.setMessages(messages);
    if (accepted > 0)
        page.setCursor(QByteArray::number(nextCursor));
    else if (isOlderRequest && !room->messageEvents().empty() && !room->allHistoryLoaded())
        page.setCursor(QByteArray::number(room->minTimelineIndex()));
    page.setHasMore(accepted == limit || !room->allHistoryLoaded());
    return page;
}

Message MatrixHistoryService::messageForEvent(const Chat &chat, const Quotient::RoomMessageEvent &event,
                                              const QString &eventId) const
{
    if (!m_messageStorage || !m_connection)
        return Message::null;

    const auto sentByCurrentAccount = event.senderId() == m_connection->userId();
    if (!sentByCurrentAccount && !m_contactManager)
        return Message::null;

    const auto contact = sentByCurrentAccount
                             ? account().accountContact()
                             : m_contactManager->byId(account(), event.senderId(), ActionCreateAndAdd);
    if (!contact)
        return Message::null;

    auto message = m_messageStorage->create();
    // A local echo is rendered under its transaction ID. A decrypted event may not
    // expose an event ID itself, so use the TimelineItem ID supplied by the caller.
    message.setId(event.transactionId().isEmpty() ? eventId : event.transactionId());
    message.setMessageChat(chat);
    message.setMessageSender(contact);
    message.setType(sentByCurrentAccount ? MessageTypeSent : MessageTypeReceived);
    message.setSendDate(event.originTimestamp().toLocalTime());
    message.setReceiveDate(event.originTimestamp().toLocalTime());
    if (const auto textContent = event.get<Quotient::EventContent::TextContent>();
        textContent && textContent->mimeType.inherits(QStringLiteral("text/html")))
    {
        message.setContent(normalizeHtml(HtmlString{sanitizeHtml(HtmlString{textContent->body}).string()}));
    }
    else
    {
        message.setContent(normalizeHtml(plainToHtml(event.plainBody())));
    }
    return message;
}

QFuture<ProtocolHistoryPage> MatrixHistoryService::completedPage(ProtocolHistoryPage page) const
{
    QPromise<ProtocolHistoryPage> promise;
    auto future = promise.future();
    promise.start();
    promise.addResult(std::move(page));
    promise.finish();
    return future;
}

void MatrixHistoryService::finishRequest(const std::shared_ptr<QPromise<ProtocolHistoryPage>> &promise,
                                         ProtocolHistoryPage page) const
{
    promise->addResult(std::move(page));
    promise->finish();
}
