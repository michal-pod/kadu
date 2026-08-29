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
#include <Quotient/events/filesourceinfo.h>
#include <Quotient/events/reactionevent.h>
#include <Quotient/events/redactionevent.h>
#include <Quotient/events/roomavatarevent.h>
#include <Quotient/events/roommemberevent.h>
#include <Quotient/events/roommessageevent.h>
#include <Quotient/events/roomevent.h>
#include <Quotient/events/simplestateevents.h>
#include <Quotient/jobs/downloadfilejob.h>
#include <Quotient/room.h>
#include <Quotient/roommember.h>
#include <Quotient/user.h>

#include <QtCore/QBuffer>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFutureWatcher>
#include <QtCore/QJsonDocument>
#include <QtCore/QStandardPaths>
#include <QtCore/QTemporaryFile>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QVBoxLayout>

#include <algorithm>
#include <iterator>
#include <limits>
#include <utility>
#include <variant>

MatrixTimelineService::MatrixTimelineService(Account account, QObject *parent)
        : ProtocolTimelineService{account, parent}
{
}

MatrixTimelineService::~MatrixTimelineService()
{
    clearAttachmentDownloads();
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
    clearAttachmentDownloads();
    m_attachmentImages.clear();
    m_attachmentStates.clear();
    m_attachmentProgress.clear();
    m_attachmentErrors.clear();
    m_attachmentSources.clear();
    m_attachmentFileNames.clear();
    m_decryptedEventSources.clear();
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

ChatTimelineActions MatrixTimelineService::availableActions(const Chat &chat, const QString &stableId) const
{
    auto *room = roomForChat(chat);
    if (!room || stableId.isEmpty())
        return {};

    for (const auto &timelineItem : room->messageEvents())
    {
        if (timelineItem->id() != stableId)
            continue;

        Quotient::RoomEventPtr decryptedEvent;
        auto encrypted = false;
        const auto *event = eventForTimelineItem(room, timelineItem, decryptedEvent, encrypted);
        if (!event)
            return {};

        ChatTimelineActions actions{ChatTimelineAction::ShowSource};
        const auto *messageEvent = Quotient::eventCast<const Quotient::RoomMessageEvent>(event);
        if (!messageEvent || event->isRedacted())
            return actions;

        actions |= ChatTimelineAction::Reply;
        if (m_connection && messageEvent->senderId() == m_connection->userId())
        {
            actions |= ChatTimelineAction::Edit;
            actions |= ChatTimelineAction::Delete;
        }
        if (messageEvent->get<Quotient::EventContent::FileContentBase>())
            actions |= ChatTimelineAction::SaveAttachment;
        return actions;
    }
    return {};
}

bool MatrixTimelineService::executeAction(const Chat &chat, const QString &stableId, ChatTimelineAction action)
{
    auto *room = roomForChat(chat);
    if (!room || !availableActions(chat, stableId).testFlag(action))
        return false;

    if (action == ChatTimelineAction::ShowSource)
    {
        for (const auto &timelineItem : room->messageEvents())
        {
            if (timelineItem->id() == stableId)
            {
                showEventSource(stableId, *timelineItem.event());
                return true;
            }
        }
        return false;
    }

    if (action == ChatTimelineAction::Delete)
    {
        room->redactEvent(stableId);
        return true;
    }
    if (action != ChatTimelineAction::SaveAttachment)
        return false;

    const auto source = m_attachmentSources.constFind(stableId);
    if (source == m_attachmentSources.cend())
    {
        QMessageBox::warning(nullptr, tr("Save attachment"), tr("The attachment is no longer available locally."));
        return false;
    }

    const auto fileName = m_attachmentFileNames.value(stableId, tr("attachment"));
    const auto downloadsPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    const auto destination = QFileDialog::getSaveFileName(
        nullptr, tr("Save attachment"), QDir{downloadsPath}.filePath(fileName));
    if (destination.isEmpty())
        return false;

    Quotient::DownloadFileJob *job = nullptr;
    if (const auto encryptedFile = std::get_if<Quotient::EncryptedFileMetadata>(&source.value()))
        job = m_connection->downloadFile(encryptedFile->url, *encryptedFile, destination);
    else if (const auto fileUrl = std::get_if<QUrl>(&source.value()))
        job = m_connection->downloadFile(*fileUrl, destination);

    if (!job)
    {
        QMessageBox::warning(nullptr, tr("Save attachment"), tr("Could not start downloading the attachment."));
        return false;
    }

    connect(job, &Quotient::BaseJob::failure, this, [job] {
        QMessageBox::warning(nullptr, tr("Save attachment"), tr("Could not save the attachment: %1").arg(job->errorString()));
    });
    return true;
}

QImage MatrixTimelineService::requestAttachmentImage(const Chat &chat, const QUrl &sourceUri, const QSize &)
{
    const auto eventId = eventIdForAttachmentUri(sourceUri);
    auto *room = roomForChat(chat);
    if (eventId.isEmpty() || !room)
        return {};

    if (const auto image = m_attachmentImages.constFind(eventId); image != m_attachmentImages.cend())
        return image.value();
    if (m_attachmentDownloadPaths.contains(eventId))
        return {};

    QTemporaryFile temporaryFile{QDir::tempPath() + QStringLiteral("/kadu-matrix-image-XXXXXX")};
    temporaryFile.setAutoRemove(false);
    if (!temporaryFile.open())
    {
        m_attachmentStates.insert(eventId, ChatTimelineAttachmentState::Failed);
        m_attachmentErrors.insert(eventId, tr("Could not create a temporary file for the image."));
        updateAttachmentEvent(room, eventId);
        return {};
    }

    const auto temporaryPath = temporaryFile.fileName();
    temporaryFile.close();
    m_attachmentDownloadPaths.insert(eventId, temporaryPath);
    m_attachmentStates.insert(eventId, ChatTimelineAttachmentState::Downloading);
    m_attachmentProgress.insert(eventId, 0.0);
    updateAttachmentEvent(room, eventId);

    const auto source = m_attachmentSources.constFind(eventId);
    if (source == m_attachmentSources.cend())
    {
        handleAttachmentDownloadFailed(room, eventId, tr("The image source is not available."));
        return {};
    }

    Quotient::DownloadFileJob *job = nullptr;
    if (const auto encryptedFile = std::get_if<Quotient::EncryptedFileMetadata>(&source.value()))
        job = m_connection->downloadFile(encryptedFile->url, *encryptedFile, temporaryPath);
    else if (const auto fileUrl = std::get_if<QUrl>(&source.value()))
        job = m_connection->downloadFile(*fileUrl, temporaryPath);

    if (!job)
    {
        handleAttachmentDownloadFailed(room, eventId, tr("Could not start downloading the image."));
        return {};
    }

    const QPointer<Quotient::Room> downloadRoom{room};
    connect(job, &Quotient::BaseJob::downloadProgress, this,
            [this, downloadRoom, eventId](qint64 received, qint64 total) {
                if (downloadRoom)
                    handleAttachmentDownloadProgress(downloadRoom.data(), eventId, received, total);
            });
    connect(job, &Quotient::BaseJob::success, this, [this, downloadRoom, eventId, job] {
        if (downloadRoom)
            handleAttachmentDownloadCompleted(downloadRoom.data(), eventId, QUrl::fromLocalFile(job->targetFileName()));
    });
    connect(job, &Quotient::BaseJob::failure, this, [this, downloadRoom, eventId, job] {
        if (downloadRoom)
            handleAttachmentDownloadFailed(downloadRoom.data(), eventId, job->errorString());
    });
    return {};
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

const Quotient::RoomEvent *MatrixTimelineService::eventForTimelineItem(
    Quotient::Room *room, const Quotient::TimelineItem &timelineItem, Quotient::RoomEventPtr &decryptedEvent,
    bool &encrypted) const
{
    const auto *event = timelineItem.event();
    if (!event)
        return nullptr;

    encrypted = !event->encryptedJson().isEmpty();
    if (const auto *encryptedEvent = timelineItem.viewAs<Quotient::EncryptedEvent>())
    {
        encrypted = true;
        decryptedEvent = room->decryptMessage(*encryptedEvent);
        if (decryptedEvent)
        {
            m_decryptedEventSources.insert(timelineItem->id(), decryptedEvent->fullJson());
            event = decryptedEvent.get();
        }
    }
    return event;
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
    connect(room, &Quotient::Room::updatedEvent, this,
            [this, room](const QString &eventId) { updateTimelineEvent(room, eventId); });
    connect(room, &Quotient::Room::replacedEvent, this,
            [this, room](const Quotient::RoomEvent *newEvent, const Quotient::RoomEvent *) {
                if (!newEvent)
                    return;

                const auto chat = chatForRoom(room);
                if (!chat)
                    return;
                if (newEvent->isRedacted())
                    emit eventRedacted(chat, newEvent->id(), newEvent->redactionReason());
                else
                    updateTimelineEvent(room, newEvent->id());
            });
    connect(room, &Quotient::Room::memberAvatarUpdated, this,
            [this, room](const Quotient::RoomMember &member) { updateTimelineEventsForMember(room, member.id()); });
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

        Quotient::RoomEventPtr decryptedEvent;
        auto encrypted = false;
        const auto *event = eventForTimelineItem(room, timelineItem, decryptedEvent, encrypted);
        if (!event)
            continue;

        // An undecrypted m.room.encrypted envelope is transport state, not a
        // timeline entry. Once Quotient decrypts it, updatedEvent/replacedEvent
        // will feed the decrypted event through this service.
        if (event->matrixType() == QStringLiteral("m.room.encrypted"))
            continue;

        if (event->isRedacted())
        {
            emit eventRedacted(chat, timelineItem->id(), event->redactionReason());
            continue;
        }
        emit eventReceived(chat, itemForEvent(room, *event, timelineItem->id(), timelineItem.index(), encrypted));
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

        Quotient::RoomEventPtr decryptedEvent;
        auto encrypted = false;
        const auto *event = eventForTimelineItem(room, *it, decryptedEvent, encrypted);
        if (!event || event->isRedacted() || event->matrixType() == QStringLiteral("m.room.encrypted"))
            continue;

        page.items.append(itemForEvent(room, *event, (*it)->id(), index, encrypted));
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

ChatTimelineItem MatrixTimelineService::itemForEvent(Quotient::Room *room, const Quotient::RoomEvent &event,
                                                      const QString &eventId, qint64 timelineIndex, bool encrypted) const
{
    ChatTimelineItem item;
    item.stableId = eventId;
    item.transactionId = event.transactionId();
    item.protocolEventType = event.matrixType();
    item.sourceOrder = sourceOrderForIndex(timelineIndex);
    item.timestamp = event.originTimestamp().toLocalTime();
    item.sender.id = event.senderId();
    item.sender.displayName = event.senderId();
    item.sender.own = m_connection && event.senderId() == m_connection->userId();
    if (room && !event.senderId().isEmpty())
    {
        const auto member = room->member(event.senderId());
        if (!member.id().isEmpty())
        {
            item.sender.displayName = member.displayName();
            item.sender.color = member.color();
            const auto avatar = room->memberAvatar(member.id(), 48);
            if (!avatar.isNull())
            {
                QByteArray avatarData;
                QBuffer buffer{&avatarData};
                if (buffer.open(QIODevice::WriteOnly) && avatar.save(&buffer, "PNG"))
                    item.sender.avatarSource = QUrl{QStringLiteral("data:image/png;base64,") +
                                                     QString::fromLatin1(avatarData.toBase64())};
            }
        }
    }
    item.state.deliveryState = item.sender.own ? ChatTimelineDeliveryState::Sent
                                                : ChatTimelineDeliveryState::Delivered;
    item.state.encrypted = encrypted;
    item.state.decryptionState = encrypted
                                       ? (event.matrixType() == QStringLiteral("m.room.encrypted")
                                              ? ChatTimelineDecryptionState::Pending
                                              : ChatTimelineDecryptionState::Decrypted)
                                       : ChatTimelineDecryptionState::NotEncrypted;

    const auto senderName = item.sender.displayName.isEmpty() ? item.sender.id : item.sender.displayName;
    const auto eventType = event.matrixType();
    const auto content = event.contentJson();
    if (eventType == QStringLiteral("m.room.name"))
    {
        item.kind = ChatTimelineItemKind::RoomNameChanged;
        item.content.plainText = tr("%1 changed the room name to %2.").arg(senderName, content.value("name").toString());
        return item;
    }
    if (eventType == QStringLiteral("m.room.topic"))
    {
        item.kind = ChatTimelineItemKind::TopicChanged;
        item.content.plainText = tr("%1 changed the room topic to %2.").arg(senderName, content.value("topic").toString());
        return item;
    }
    if (eventType == QStringLiteral("m.room.avatar"))
    {
        item.kind = ChatTimelineItemKind::RoomAvatarChanged;
        item.content.plainText = tr("%1 changed the room avatar.").arg(senderName);
        return item;
    }
    if (eventType == QStringLiteral("m.room.create"))
    {
        item.kind = ChatTimelineItemKind::RoomCreated;
        item.content.plainText = tr("%1 created the room.").arg(senderName);
        return item;
    }
    if (eventType == QStringLiteral("m.room.encryption"))
    {
        item.kind = ChatTimelineItemKind::EncryptionEnabled;
        item.content.plainText = tr("%1 enabled end-to-end encryption.").arg(senderName);
        return item;
    }
    if (const auto *memberEvent = Quotient::eventCast<const Quotient::RoomMemberEvent>(&event))
    {
        const auto memberId = memberEvent->userId();
        const auto memberName = memberEvent->newDisplayName().value_or(memberId);
        if (memberEvent->isRename() || memberEvent->isAvatarUpdate())
        {
            item.kind = ChatTimelineItemKind::MemberProfileChanged;
            item.content.plainText = tr("%1 updated their room profile.").arg(memberName);
        }
        else if (memberEvent->isJoin())
        {
            item.kind = ChatTimelineItemKind::MemberJoined;
            item.content.plainText = tr("%1 joined the room.").arg(memberName);
        }
        else if (memberEvent->isInvite())
        {
            item.kind = ChatTimelineItemKind::MemberInvited;
            item.content.plainText = tr("%1 invited %2 to the room.").arg(senderName, memberName);
        }
        else if (memberEvent->isBan())
        {
            item.kind = ChatTimelineItemKind::MemberBanned;
            item.content.plainText = tr("%1 banned %2 from the room.").arg(senderName, memberName);
        }
        else if (memberEvent->isLeave() && event.senderId() != memberId)
        {
            item.kind = ChatTimelineItemKind::MemberKicked;
            item.content.plainText = tr("%1 removed %2 from the room.").arg(senderName, memberName);
        }
        else if (memberEvent->isLeave() || memberEvent->isRejectedInvite())
        {
            item.kind = ChatTimelineItemKind::MemberLeft;
            item.content.plainText = tr("%1 left the room.").arg(memberName);
        }
        else
        {
            item.kind = ChatTimelineItemKind::MemberProfileChanged;
            item.content.plainText = tr("%1 updated their room profile.").arg(memberName);
        }
        return item;
    }
    if (const auto *reactionEvent = Quotient::eventCast<const Quotient::ReactionEvent>(&event))
    {
        item.kind = ChatTimelineItemKind::ReactionAdded;
        item.content.plainText = tr("%1 reacted with %2.").arg(senderName, reactionEvent->key());
        return item;
    }
    if (const auto *redactionEvent = Quotient::eventCast<const Quotient::RedactionEvent>(&event))
    {
        item.kind = ChatTimelineItemKind::MessageRedacted;
        item.content.plainText = tr("%1 removed a message.").arg(senderName);
        item.content.replyToId = redactionEvent->redactedEvent();
        return item;
    }
    if (eventType.startsWith(QStringLiteral("m.call.")))
    {
        item.kind = ChatTimelineItemKind::CallEvent;
        item.content.plainText = tr("%1 sent a call event (%2).").arg(senderName, eventType);
        return item;
    }
    if (eventType == QStringLiteral("m.room.encrypted"))
    {
        item.kind = ChatTimelineItemKind::UnsupportedEvent;
        item.content.plainText = tr("Encrypted Matrix event is waiting for a key (%1).").arg(eventType);
        item.state.errorText = tr("The event could not be decrypted yet.");
        return item;
    }

    const auto *messageEvent = Quotient::eventCast<const Quotient::RoomMessageEvent>(&event);
    if (!messageEvent)
    {
        item.kind = ChatTimelineItemKind::UnsupportedEvent;
        item.content.plainText = tr("Unsupported Matrix event: %1").arg(eventType);
        return item;
    }
    if (!messageEvent->replacedEvent().isEmpty())
    {
        item.kind = ChatTimelineItemKind::MessageEdited;
        item.content.plainText = tr("%1 edited a message.").arg(senderName);
        item.content.replyToId = messageEvent->replacedEvent();
        return item;
    }

    item.content.plainText = messageEvent->plainBody();
    item.content.replyToId = messageEvent->replyEventId(true);
    item.state.edited = messageEvent->isReplaced();
    if (const auto textContent = messageEvent->get<Quotient::EventContent::TextContent>();
        textContent && textContent->mimeType.inherits(QStringLiteral("text/html")))
    {
        item.content.formattedText = sanitizeHtml(HtmlString{textContent->body}).string();
    }

    switch (messageEvent->msgtype())
    {
    case Quotient::RoomMessageEvent::MsgType::Text: item.kind = ChatTimelineItemKind::TextMessage; break;
    case Quotient::RoomMessageEvent::MsgType::Image: item.kind = ChatTimelineItemKind::ImageMessage; break;
    case Quotient::RoomMessageEvent::MsgType::File: item.kind = ChatTimelineItemKind::FileMessage; break;
    case Quotient::RoomMessageEvent::MsgType::Audio: item.kind = ChatTimelineItemKind::AudioMessage; break;
    case Quotient::RoomMessageEvent::MsgType::Video: item.kind = ChatTimelineItemKind::VideoMessage; break;
    case Quotient::RoomMessageEvent::MsgType::Notice: item.kind = ChatTimelineItemKind::NoticeMessage; break;
    case Quotient::RoomMessageEvent::MsgType::Emote: item.kind = ChatTimelineItemKind::EmoteMessage; break;
    default:
        item.kind = ChatTimelineItemKind::UnsupportedEvent;
        item.content.plainText = tr("Unsupported Matrix message type: %1").arg(messageEvent->rawMsgtype());
        return item;
    }

    if (room)
    {
        for (const auto *relatedEvent : room->relatedEvents(event, Quotient::EventRelation::AnnotationType))
        {
            const auto *reactionEvent = Quotient::eventCast<const Quotient::ReactionEvent>(relatedEvent);
            if (!reactionEvent || relatedEvent->isRedacted())
                continue;

            const auto reactionKey = reactionEvent->key();
            auto reaction = std::find_if(item.content.reactions.begin(), item.content.reactions.end(),
                                         [&reactionKey](const ChatTimelineReaction &candidate) {
                                             return candidate.key == reactionKey;
                                         });
            if (reaction == item.content.reactions.end())
            {
                item.content.reactions.append({reactionKey});
                reaction = std::prev(item.content.reactions.end());
            }
            const auto reactionMember = room->member(reactionEvent->senderId());
            reaction->senderIds.append(reactionEvent->senderId());
            reaction->senderDisplayNames.append(reactionMember.id().isEmpty() ? reactionEvent->senderId()
                                                                              : reactionMember.displayName());
            reaction->own = reaction->own || (m_connection && reactionEvent->senderId() == m_connection->userId());
        }
    }

    if (const auto fileContent = messageEvent->get<Quotient::EventContent::FileContentBase>())
    {
        const auto fileInfo = fileContent->commonInfo();
        ChatTimelineAttachment attachment;
        attachment.fileName = fileInfo.originalName.isEmpty() ? messageEvent->fileNameToDownload() : fileInfo.originalName;
        attachment.mimeType = fileInfo.mimeType.name();
        attachment.size = fileInfo.payloadSize;
        attachment.sourceUri = attachmentUri(eventId);
        attachment.localResourceId = eventId;
        attachment.state = m_attachmentStates.value(eventId, ChatTimelineAttachmentState::NotRequested);
        attachment.progress = m_attachmentProgress.value(eventId, 0.0);
        attachment.errorText = m_attachmentErrors.value(eventId);
        m_attachmentSources.insert(eventId, fileInfo.source);
        m_attachmentFileNames.insert(eventId, attachment.fileName);

        switch (messageEvent->msgtype())
        {
        case Quotient::RoomMessageEvent::MsgType::Image:
        {
            attachment.kind = ChatTimelineAttachmentKind::Image;
            if (const auto imageContent = messageEvent->get<Quotient::EventContent::ImageContent>())
                attachment.dimensions = imageContent->imageSize;
            break;
        }
        case Quotient::RoomMessageEvent::MsgType::Audio: attachment.kind = ChatTimelineAttachmentKind::Audio; break;
        case Quotient::RoomMessageEvent::MsgType::Video:
        {
            attachment.kind = ChatTimelineAttachmentKind::Video;
            if (const auto videoContent = messageEvent->get<Quotient::EventContent::VideoContent>())
            {
                attachment.dimensions = videoContent->imageSize;
                attachment.duration = videoContent->duration;
            }
            break;
        }
        default: attachment.kind = ChatTimelineAttachmentKind::File; break;
        }
        item.content.attachments.append(std::move(attachment));
    }
    return item;
}

void MatrixTimelineService::updateTimelineEvent(Quotient::Room *room, const QString &eventId)
{
    const auto chat = chatForRoom(room);
    if (!chat || !room || eventId.isEmpty())
        return;

    for (const auto &timelineItem : room->messageEvents())
    {
        if (timelineItem->id() != eventId)
            continue;

        Quotient::RoomEventPtr decryptedEvent;
        auto encrypted = false;
        const auto *event = eventForTimelineItem(room, timelineItem, decryptedEvent, encrypted);
        if (!event)
            return;
        if (event->matrixType() == QStringLiteral("m.room.encrypted"))
            return;
        if (event->isRedacted())
        {
            emit eventRedacted(chat, eventId, event->redactionReason());
            return;
        }

        emit eventUpdated(chat, itemForEvent(room, *event, eventId, timelineItem.index(), encrypted));
        return;
    }
}

void MatrixTimelineService::updateTimelineEventsForMember(Quotient::Room *room, const QString &memberId)
{
    if (!room || memberId.isEmpty())
        return;

    for (const auto &timelineItem : room->messageEvents())
    {
        if (timelineItem->senderId() == memberId)
            updateTimelineEvent(room, timelineItem->id());
    }
}

void MatrixTimelineService::showEventSource(const QString &eventId, const Quotient::RoomEvent &event) const
{
    QDialog dialog;
    dialog.setWindowTitle(tr("Matrix event source"));
    dialog.resize(720, 520);

    auto *layout = new QVBoxLayout{&dialog};
    auto *tabs = new QTabWidget{&dialog};
    const auto originalEncryptedJson = event.encryptedJson();
    const auto decryptedJson = m_decryptedEventSources.value(eventId);
    const auto encryptedJson = originalEncryptedJson.isEmpty() && !decryptedJson.isEmpty()
                                   ? event.fullJson()
                                   : originalEncryptedJson;
    if (!encryptedJson.isEmpty())
    {
        auto *encryptedSource = new QPlainTextEdit{tabs};
        encryptedSource->setReadOnly(true);
        encryptedSource->setPlainText(QString::fromUtf8(QJsonDocument{encryptedJson}.toJson(QJsonDocument::Indented)));
        tabs->addTab(encryptedSource, tr("Encrypted event"));
    }

    auto *eventSource = new QPlainTextEdit{tabs};
    eventSource->setReadOnly(true);
    const auto visibleEventJson = originalEncryptedJson.isEmpty() && !decryptedJson.isEmpty()
                                      ? decryptedJson
                                      : event.fullJson();
    eventSource->setPlainText(QString::fromUtf8(QJsonDocument{visibleEventJson}.toJson(QJsonDocument::Indented)));
    tabs->addTab(eventSource, encryptedJson.isEmpty() ? tr("Matrix event") : tr("Decrypted event"));
    layout->addWidget(tabs);

    auto *buttons = new QDialogButtonBox{QDialogButtonBox::Close, &dialog};
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    dialog.exec();
}

void MatrixTimelineService::updateAttachmentEvent(Quotient::Room *room, const QString &eventId)
{
    updateTimelineEvent(room, eventId);
}

void MatrixTimelineService::handleAttachmentDownloadProgress(Quotient::Room *room, const QString &eventId,
                                                             qint64 received, qint64 total)
{
    if (!m_attachmentDownloadPaths.contains(eventId))
        return;

    m_attachmentProgress.insert(eventId, total > 0 ? qreal(received) / qreal(total) : 0.0);
    updateAttachmentEvent(room, eventId);
}

void MatrixTimelineService::handleAttachmentDownloadCompleted(Quotient::Room *room, const QString &eventId,
                                                              const QUrl &localFile)
{
    if (!m_attachmentDownloadPaths.contains(eventId))
        return;

    const auto temporaryPath = m_attachmentDownloadPaths.take(eventId);
    const auto localPath = localFile.toLocalFile().isEmpty() ? temporaryPath : localFile.toLocalFile();
    const QImage image{localPath};
    QFile::remove(temporaryPath);
    if (localPath != temporaryPath)
        QFile::remove(localPath);

    if (image.isNull())
    {
        m_attachmentStates.insert(eventId, ChatTimelineAttachmentState::Failed);
        m_attachmentErrors.insert(eventId, tr("The downloaded attachment is not a valid image."));
    }
    else
    {
        m_attachmentImages.insert(eventId, image);
        m_attachmentStates.insert(eventId, ChatTimelineAttachmentState::Available);
        m_attachmentProgress.insert(eventId, 1.0);
        m_attachmentErrors.remove(eventId);
    }
    updateAttachmentEvent(room, eventId);
}

void MatrixTimelineService::handleAttachmentDownloadFailed(Quotient::Room *room, const QString &eventId,
                                                           const QString &errorMessage)
{
    const auto temporaryPath = m_attachmentDownloadPaths.take(eventId);
    if (temporaryPath.isEmpty())
        return;

    QFile::remove(temporaryPath);
    m_attachmentStates.insert(eventId, ChatTimelineAttachmentState::Failed);
    m_attachmentProgress.remove(eventId);
    m_attachmentErrors.insert(eventId, errorMessage.isEmpty() ? tr("Could not download the image.") : errorMessage);
    updateAttachmentEvent(room, eventId);
}

void MatrixTimelineService::clearAttachmentDownloads()
{
    for (const auto &temporaryPath : std::as_const(m_attachmentDownloadPaths))
        QFile::remove(temporaryPath);
    m_attachmentDownloadPaths.clear();
}

QUrl MatrixTimelineService::attachmentUri(const QString &eventId)
{
    QUrl uri;
    uri.setScheme(QStringLiteral("kaduimg"));
    uri.setPath(QStringLiteral("/") + eventId);
    return uri;
}

QString MatrixTimelineService::eventIdForAttachmentUri(const QUrl &sourceUri)
{
    if (sourceUri.scheme() != QStringLiteral("kaduimg"))
        return {};
    return sourceUri.path(QUrl::FullyDecoded).mid(1);
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
