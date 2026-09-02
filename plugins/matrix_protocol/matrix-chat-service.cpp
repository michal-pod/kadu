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

#include "matrix-chat-service.h"
#include "matrix-chat-service.moc"

#include "matrix-contact-avatar-service.h"

#include "accounts/account.h"
#include "avatars/avatars.h"
#include "chat/chat.h"
#include "chat/chat-details-room.h"
#include "chat/chat-manager.h"
#include "chat/chat-storage.h"
#include "chat/type/chat-type-contact.h"
#include "chat/type/chat-type-room.h"
#include "contacts/contact-manager.h"
#include "contacts/contact-set.h"
#include "formatted-string/formatted-string-factory.h"
#include "formatted-string/formatted-string-plain-text-visitor.h"
#include "html/html-conversion.h"
#include "html/html-string.h"
#include "message/message-storage.h"
#include "services/raw-message-transformer-service.h"

#include <Quotient/connection.h>
#include <Quotient/avatar.h>
#include <Quotient/events/encryptedevent.h>
#include <Quotient/events/eventcontent.h>
#include <Quotient/events/roommessageevent.h>
#include <Quotient/events/roomevent.h>
#include <Quotient/room.h>
#include <Quotient/roommember.h>

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QMimeDatabase>
#include <QtCore/QObject>
#include <QtCore/QSize>
#include <QtCore/QTemporaryFile>
#include <QtCore/QUrl>
#include <QtGui/QImage>
#include <QtGui/QImageReader>
#include <QtGui/QPixmap>

#include <memory>
#include <optional>
#include <iostream>

MatrixChatService::MatrixChatService(Account account, QObject *parent) : ChatService{account, parent}
{
}

int MatrixChatService::maxMessageLength() const
{
    return -1;
}

void MatrixChatService::setConnection(Quotient::Connection *connection)
{
    if (m_connection == connection)
        return;

    if (m_connection)
        disconnect(m_connection, nullptr, this, nullptr);

    m_connection = connection;
    m_watchedRooms.clear();
    m_loadedRooms.clear();
    m_historicalEventIds.clear();
    m_localTransactionIds.clear();
    m_initialSyncFinished = false;

    if (!m_connection)
        return;

    connect(m_connection, &Quotient::Connection::newRoom, this, &MatrixChatService::watchRoom);
    connect(m_connection, &Quotient::Connection::joinedRoom, this,
            [this](Quotient::Room *room, Quotient::Room *) { watchRoom(room); });
    connect(m_connection, &Quotient::Connection::syncDone, this, [this] {
        m_initialSyncFinished = true;
        for (auto *room : m_connection->allRooms())
            synchronizeRoom(room);
    });

    for (auto *room : m_connection->allRooms())
        watchRoom(room);
}

void MatrixChatService::setContactAvatarService(MatrixContactAvatarService *contactAvatarService)
{
    m_contactAvatarService = contactAvatarService;
}

void MatrixChatService::setChatManager(ChatManager *chatManager)
{
    m_chatManager = chatManager;
}

void MatrixChatService::setChatStorage(ChatStorage *chatStorage)
{
    m_chatStorage = chatStorage;
}

void MatrixChatService::setContactManager(ContactManager *contactManager)
{
    m_contactManager = contactManager;
}

void MatrixChatService::setFormattedStringFactory(FormattedStringFactory *formattedStringFactory)
{
    m_formattedStringFactory = formattedStringFactory;
}

void MatrixChatService::setMessageStorage(MessageStorage *messageStorage)
{
    m_messageStorage = messageStorage;
}

QString MatrixChatService::directChatId(const Chat &chat) const
{
    const auto contacts = chat.contacts().toContactVector();
    return contacts.size() == 1 ? contacts.constFirst().id() : QString{};
}

QString MatrixChatService::roomId(const Chat &chat) const
{
    const auto *details = qobject_cast<ChatDetailsRoom *>(chat.details());
    return details ? details->room() : QString{};
}

bool MatrixChatService::sendText(const Chat &chat, const QString &text, Message message,
                                 const std::optional<Quotient::EventRelation> &relation)
{
    if (!m_connection || !m_connection->isLoggedIn())
        return false;

    const auto transactionId = m_connection->generateTxnId();
    if (!message.isNull())
        message.setId(transactionId);

    if (const auto id = roomId(chat); !id.isEmpty())
    {
        auto *room = m_connection->room(id, Quotient::JoinState::Join);
        if (!isSupportedRoom(room))
            return false;

        postText(room, text, transactionId, relation);
        return true;
    }

    const auto recipientId = directChatId(chat);
    if (recipientId.isEmpty())
        return false;

    m_connection->getDirectChat(recipientId).then(
        this, [this, text, transactionId, relation](Quotient::Room *room) {
            if (!room)
                return;

            postText(room, text, transactionId, relation);
        });
    return true;
}

void MatrixChatService::postText(Quotient::Room *room, const QString &text,
                                 const QString &transactionId, const std::optional<Quotient::EventRelation> &relation)
{
    if (!room)
        return;

    m_localTransactionIds.insert(transactionId);
    auto event = Quotient::makeEvent<Quotient::RoomMessageEvent>(text, Quotient::RoomMessageEvent::MsgType::Text,
                                                                  nullptr, relation);
    event->setTransactionId(transactionId);
    room->post(std::move(event));

    // Temporary Matrix timeline diagnostic. Enable only while investigating a protocol issue.
    // if (text == QStringLiteral("dump"))
    //     dumpTimeline(room);
}

void MatrixChatService::dumpTimeline(Quotient::Room *room)
{
    if (!room)
        return;

    QJsonArray events;
    auto count = 0;
    for (auto it = room->messageEvents().crbegin(); it != room->messageEvents().crend() && count < 20; ++it, ++count)
    {
        const auto &timelineItem = *it;
        const auto *rawEvent = timelineItem.event();
        const auto rawJson = rawEvent && !rawEvent->encryptedJson().isEmpty() ? rawEvent->encryptedJson()
                                                                               : rawEvent ? rawEvent->fullJson()
                                                                                          : QJsonObject{};
        QJsonObject record{{QStringLiteral("timelineIndex"), QString::number(static_cast<qint64>(timelineItem.index()))},
                           {QStringLiteral("eventId"), timelineItem->id()},
                           {QStringLiteral("raw"), rawJson}};

        if (!rawEvent)
            record.insert(QStringLiteral("decryption"), QStringLiteral("event unavailable"));
        else if (rawEvent->isRedacted())
            record.insert(QStringLiteral("decryption"), QStringLiteral("skipped: event is redacted"));
        else if (rawEvent->originalEvent())
        {
            record.insert(QStringLiteral("decryption"), QStringLiteral("succeeded"));
            record.insert(QStringLiteral("decrypted"), rawEvent->fullJson());
        }
        else if (const auto *encryptedEvent = timelineItem.viewAs<Quotient::EncryptedEvent>())
        {
            if (const auto decryptedEvent = room->decryptMessage(*encryptedEvent))
            {
                record.insert(QStringLiteral("decryption"), QStringLiteral("succeeded"));
                record.insert(QStringLiteral("decrypted"), decryptedEvent->fullJson());
            }
            else
                record.insert(QStringLiteral("decryption"), QStringLiteral("unavailable"));
        }
        else
        {
            record.insert(QStringLiteral("decryption"), QStringLiteral("not encrypted"));
            record.insert(QStringLiteral("decrypted"), rawEvent->fullJson());
        }
        events.append(record);
    }

    const QJsonObject dump{{QStringLiteral("roomId"), room->id()},
                           {QStringLiteral("createdAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
                           {QStringLiteral("order"), QStringLiteral("newest-first")},
                           {QStringLiteral("events"), events}};
    const auto fileName = QStringLiteral("matrix-timeline-%1.json").arg(
        QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss-zzz")));
    const auto path = QDir::current().absoluteFilePath(fileName);
    QFile file{path};
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        std::cout << "Matrix timeline dump failed: " << path.toStdString() << std::endl;
        return;
    }

    const auto json = QJsonDocument{dump}.toJson(QJsonDocument::Indented);
    if (file.write(json) != json.size())
    {
        std::cout << "Matrix timeline dump failed: " << path.toStdString() << std::endl;
        return;
    }

    std::cout << "Matrix timeline dump saved: " << path.toStdString() << std::endl;
}

bool MatrixChatService::sendAttachmentToRoom(const Chat &chat, const QString &filePath, const QString &description)
{
    if (!m_connection || !m_connection->isLoggedIn() || !QFileInfo{filePath}.isFile())
        return false;

    if (const auto id = roomId(chat); !id.isEmpty())
    {
        auto *room = m_connection->room(id, Quotient::JoinState::Join);
        if (!isSupportedRoom(room))
            return false;

        postAttachment(room, filePath, description);
        return true;
    }

    const auto recipientId = directChatId(chat);
    if (recipientId.isEmpty())
        return false;

    m_connection->getDirectChat(recipientId).then(
        this, [this, filePath, description](Quotient::Room *room) { postAttachment(room, filePath, description); });
    return true;
}

void MatrixChatService::postAttachment(Quotient::Room *room, const QString &filePath, const QString &description)
{
    const QFileInfo fileInfo{filePath};
    if (!room || !m_connection || !fileInfo.isFile())
        return;

    const auto plainText = description.isEmpty() ? fileInfo.fileName() : description;
    const auto mimeType = QMimeDatabase{}.mimeTypeForFile(fileInfo);
    QImageReader imageReader{fileInfo.absoluteFilePath()};
    imageReader.setAutoTransform(true);
    const auto imageAttachment = mimeType.name().startsWith(QStringLiteral("image/")) && imageReader.canRead();
    const auto imageSize = imageAttachment ? imageReader.size() : QSize{};
    const auto messageType = imageAttachment
                                 ? QStringLiteral("m.image")
                                 : (mimeType.name().startsWith(QStringLiteral("image/"))
                                        ? QStringLiteral("m.file")
                                        : Quotient::RoomMessageEvent::rawMsgTypeForFile(fileInfo));
    auto thumbnailFile = std::shared_ptr<QTemporaryFile>{};
    QSize thumbnailSize;
    qint64 thumbnailPayloadSize = 0;
    if (room->usesEncryption() && imageSize.isValid())
    {
        QImageReader thumbnailReader{fileInfo.absoluteFilePath()};
        thumbnailReader.setAutoTransform(true);
        thumbnailReader.setScaledSize(imageSize.scaled(QSize{320, 240}, Qt::KeepAspectRatio));
        const auto thumbnail = thumbnailReader.read();
        auto candidate = std::make_shared<QTemporaryFile>();
        if (!thumbnail.isNull() && candidate->open() && thumbnail.save(candidate.get(), "PNG"))
        {
            candidate->flush();
            thumbnailPayloadSize = candidate->size();
            thumbnailSize = thumbnail.size();
            candidate->close();
            thumbnailFile = std::move(candidate);
        }
    }

    const auto uploadId = m_connection->generateTxnId();
    const auto thumbnailUploadId = thumbnailFile ? m_connection->generateTxnId() : QString{};
    const QPointer<Quotient::Room> uploadRoom{room};
    auto *uploadContext = new QObject{room};
    const auto uploadedFileMetadata = std::make_shared<std::optional<Quotient::FileSourceInfo>>();
    const auto uploadedThumbnailMetadata = std::make_shared<std::optional<Quotient::FileSourceInfo>>();
    const auto postEvent = [uploadRoom, plainText, fileInfo, mimeType, imageAttachment, imageSize, messageType,
                            thumbnailFile, thumbnailSize,
                            thumbnailPayloadSize, uploadedFileMetadata, uploadedThumbnailMetadata, uploadContext] {
        if (!uploadRoom || !uploadedFileMetadata->has_value() ||
            (thumbnailFile && !uploadedThumbnailMetadata->has_value()))
            return;

        std::unique_ptr<Quotient::EventContent::FileContentBase> content;
        if (imageAttachment)
        {
            auto imageContent = std::make_unique<Quotient::EventContent::ImageContent>(
                **uploadedFileMetadata, fileInfo.size(), mimeType, imageSize, fileInfo.fileName());
            if (uploadedThumbnailMetadata->has_value())
            {
                const auto thumbnailMimeType = QMimeDatabase{}.mimeTypeForName(QStringLiteral("image/png"));
                imageContent->thumbnail = Quotient::EventContent::Thumbnail{
                    **uploadedThumbnailMetadata, thumbnailPayloadSize, thumbnailMimeType, thumbnailSize};
            }
            content = std::move(imageContent);
        }
        else
        {
            content = std::make_unique<Quotient::EventContent::FileContent>(
                **uploadedFileMetadata, fileInfo.size(), mimeType, fileInfo.fileName());
        }

        auto event = Quotient::makeEvent<Quotient::RoomMessageEvent>(
            plainText, messageType, std::move(content));
        uploadRoom->post(std::move(event));
        uploadContext->deleteLater();
    };

    // Room::postFile() creates a pending event with the local file URL and replaces it after uploading. In the
    // libQuotient version used by Kadu, the replacement leaves that local URL next to encrypted `file` metadata.
    // Upload first and construct the event from FileSourceInfo so only the server media URL is serialised.
    connect(room, &Quotient::Room::fileTransferCompleted, uploadContext,
            [uploadId, uploadedFileMetadata, postEvent](
                const QString &completedId, const QUrl &, const Quotient::FileSourceInfo &fileMetadata) {
                if (completedId != uploadId)
                    return;

                *uploadedFileMetadata = fileMetadata;
                postEvent();
            });
    connect(room, &Quotient::Room::fileTransferFailed, uploadContext,
            [uploadId, thumbnailUploadId, uploadContext](const QString &failedId, const QString &) {
                if (failedId == uploadId || failedId == thumbnailUploadId)
                    uploadContext->deleteLater();
            });

    if (thumbnailFile)
    {
        connect(room, &Quotient::Room::fileTransferCompleted, uploadContext,
                [thumbnailUploadId, uploadedThumbnailMetadata, postEvent](
                    const QString &completedId, const QUrl &, const Quotient::FileSourceInfo &fileMetadata) {
                    if (completedId != thumbnailUploadId)
                        return;

                    *uploadedThumbnailMetadata = fileMetadata;
                    postEvent();
                });
    }

    // In the libQuotient version used by Kadu, Connection::uploadContent() opens the QFile only when the
    // override content type is empty. Keep the MIME type above for Matrix event metadata, but let the upload
    // path determine it and open its source file itself.
    room->uploadFile(uploadId, QUrl::fromLocalFile(fileInfo.absoluteFilePath()));
    if (thumbnailFile)
        room->uploadFile(thumbnailUploadId, QUrl::fromLocalFile(thumbnailFile->fileName()));
}

bool MatrixChatService::sendLocationToRoom(const Chat &chat, const QString &geoUri)
{
    if (!m_connection || !m_connection->isLoggedIn() || !QUrl{geoUri}.isValid() || !geoUri.startsWith(QStringLiteral("geo:")))
        return false;

    if (const auto id = roomId(chat); !id.isEmpty())
    {
        auto *room = m_connection->room(id, Quotient::JoinState::Join);
        if (!isSupportedRoom(room))
            return false;

        postLocation(room, geoUri);
        return true;
    }

    const auto recipientId = directChatId(chat);
    if (recipientId.isEmpty())
        return false;

    m_connection->getDirectChat(recipientId).then(
        this, [this, geoUri](Quotient::Room *room) { postLocation(room, geoUri); });
    return true;
}

void MatrixChatService::postLocation(Quotient::Room *room, const QString &geoUri)
{
    if (!room)
        return;

    auto content = std::make_unique<Quotient::EventContent::LocationContent>(geoUri);
    auto event = Quotient::makeEvent<Quotient::RoomMessageEvent>(geoUri, QStringLiteral("m.location"), std::move(content));
    room->post(std::move(event));
}

bool MatrixChatService::sendMessage(const Message &message)
{
    return sendMessageWithRelation(message, std::nullopt);
}

bool MatrixChatService::sendReply(const Message &message, const QString &targetEventId)
{
    return targetEventId.isEmpty() ? false
                                   : sendMessageWithRelation(message, Quotient::EventRelation::replyTo(targetEventId));
}

bool MatrixChatService::editMessage(const Message &message, const QString &targetEventId)
{
    return targetEventId.isEmpty() ? false
                                   : sendMessageWithRelation(message, Quotient::EventRelation::replace(targetEventId));
}

bool MatrixChatService::sendMessageWithRelation(const Message &message,
                                                 const std::optional<Quotient::EventRelation> &relation)
{
    if (!m_formattedStringFactory)
        return false;

    auto formattedContent = m_formattedStringFactory->fromHtml(message.content());
    FormattedStringPlainTextVisitor plainTextVisitor;
    formattedContent->accept(&plainTextVisitor);

    auto plainText = plainTextVisitor.result();
    if (rawMessageTransformerService())
        plainText = QString::fromUtf8(
            rawMessageTransformerService()->transform(plainText.toUtf8(), message).rawContent());

    return sendText(message.messageChat(), plainText, message, relation);
}

bool MatrixChatService::sendRawMessage(const Chat &chat, const QByteArray &rawMessage)
{
    return sendText(chat, QString::fromUtf8(rawMessage));
}

bool MatrixChatService::sendAttachment(const Chat &chat, const QString &filePath, const QString &description)
{
    return sendAttachmentToRoom(chat, filePath, description);
}

bool MatrixChatService::sendLocation(const Chat &chat, const QString &geoUri)
{
    return sendLocationToRoom(chat, geoUri);
}

void MatrixChatService::leaveChat(const Chat &chat)
{
    if (const auto id = roomId(chat); !id.isEmpty())
    {
        if (auto *room = m_connection ? m_connection->room(id, Quotient::JoinState::Join) : nullptr)
            room->leaveRoom();
        return;
    }

    chat.setIgnoreAllMessages(true);
}

bool MatrixChatService::isSupportedRoom(const Quotient::Room *room) const
{
    return room && room->joinState() == Quotient::JoinState::Join &&
           (!m_connection || !m_connection->isDirectChat(room->id()));
}

Chat MatrixChatService::roomChat(Quotient::Room *room) const
{
    if (!m_chatManager || !m_chatStorage || !isSupportedRoom(room))
        return Chat::null;

    auto chat = ChatTypeRoom::findChat(m_chatManager, m_chatStorage, account(), room->id(), ActionCreateAndAdd);
    if (!chat)
        return Chat::null;

    const auto displayName = room->displayName();
    chat.setDisplay(displayName.isEmpty() ? room->id() : displayName);
    if (auto *details = qobject_cast<ChatDetailsRoom *>(chat.details()))
        details->setConnected(true);
    return chat;
}

void MatrixChatService::synchronizeRoom(Quotient::Room *room)
{
    if (!m_initialSyncFinished)
        return;

    if (!roomChat(room))
        return;

    room->setDisplayed(true);
    synchronizeRoomDetails(room);
    synchronizeRoomMembers(room);
}

void MatrixChatService::synchronizeRoomDetails(Quotient::Room *room)
{
    if (!m_initialSyncFinished)
        return;

    const auto chat = roomChat(room);
    auto *details = chat ? qobject_cast<ChatDetailsRoom *>(chat.details()) : nullptr;
    if (!details)
        return;

    details->setDescription(room->topic());
    if (!Quotient::Avatar::isUrlValid(room->avatarUrl()))
    {
        details->setAvatar({});
        return;
    }

    const QPointer<MatrixChatService> service{this};
    const QPointer<Quotient::Room> watchedRoom{room};
    const auto image = room->avatarObject().get(AVATAR_SIZE, [service, watchedRoom] {
        if (service && watchedRoom)
            service->synchronizeRoomDetails(watchedRoom);
    });
    details->setAvatar(QPixmap::fromImage(image));
}

void MatrixChatService::synchronizeRoomMembers(Quotient::Room *room)
{
    if (!m_initialSyncFinished || !m_contactManager)
        return;

    const auto chat = roomChat(room);
    auto *details = chat ? qobject_cast<ChatDetailsRoom *>(chat.details()) : nullptr;
    if (!details)
        return;

    ContactSet roomContacts;
    for (const auto &member : room->joinedMembers())
    {
        const auto contact = member.isLocalMember()
                                 ? account().accountContact()
                                 : m_contactManager->byId(account(), member.id(), ActionCreateAndAdd);
        if (!contact)
            continue;

        roomContacts.insert(contact);
        if (!member.isLocalMember() && m_contactAvatarService)
            m_contactAvatarService->observeContact(member.id());
    }

    const auto existingContacts = details->contacts();
    for (const auto &contact : existingContacts)
        if (!roomContacts.contains(contact))
            details->removeContact(contact);

    for (const auto &contact : roomContacts)
        details->addContact(contact);
}

void MatrixChatService::watchRoom(Quotient::Room *room)
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
    connect(room, &Quotient::Room::baseStateLoaded, this, [this, room] {
        m_loadedRooms.insert(room);
        synchronizeRoom(room);
    });
    connect(room, &Quotient::Room::memberListChanged, this,
            [this, room] { synchronizeRoomMembers(room); });
    connect(room, &Quotient::Room::allMembersLoaded, this,
            [this, room] { synchronizeRoomMembers(room); });
    connect(room, &Quotient::Room::displaynameChanged, this,
            [this, room](Quotient::Room *, const QString &) { synchronizeRoom(room); });
    connect(room, &Quotient::Room::topicChanged, this, [this, room] { synchronizeRoomDetails(room); });
    connect(room, &Quotient::Room::avatarChanged, this, [this, room] { synchronizeRoomDetails(room); });
    connect(room, &Quotient::Room::encryption, this, [this, room] { synchronizeRoom(room); });
    connect(room, &QObject::destroyed, this, [this, room] {
        m_watchedRooms.remove(room);
        m_loadedRooms.remove(room);
    });
}

void MatrixChatService::handleNewMessages(Quotient::Room *room, int fromIndex, int toIndex)
{
    if (!m_connection || !m_loadedRooms.contains(room))
        return;

    const auto directChat = m_connection->isDirectChat(room->id());
    if (!directChat && !isSupportedRoom(room))
        return;

    for (const auto &item : room->messageEvents())
    {
        if (item.index() < fromIndex || item.index() > toIndex)
            continue;

        if (m_historicalEventIds.remove(item->id()))
            continue;

        const auto *event = item.viewAs<Quotient::RoomMessageEvent>();
        if (!event)
            continue;
        if (!event->transactionId().isEmpty() && m_localTransactionIds.remove(event->transactionId()))
            continue;
        if (event->isRedacted() || event->rawMsgtype() != QStringLiteral("m.text"))
            continue;

        // The decrypted RoomMessageEvent is a view of the timeline event. Keep the ID
        // from TimelineItem: for encrypted messages it is the only stable server event ID.
        if (directChat)
            handleDirectMessageEvent(*event, item->id());
        else
            handleRoomMessageEvent(room, *event, item->id());
    }
}

void MatrixChatService::handleDirectMessageEvent(const Quotient::RoomMessageEvent &event,
                                                 const QString &eventId)
{
    if (!m_chatManager || !m_chatStorage || !m_contactManager || !m_messageStorage)
        return;

    const auto sentByCurrentAccount = m_connection && event.senderId() == m_connection->userId();
    const auto contact = sentByCurrentAccount ? account().accountContact()
                                              : m_contactManager->byId(account(), event.senderId(), ActionCreateAndAdd);
    if (!sentByCurrentAccount && m_contactAvatarService)
        m_contactAvatarService->observeContact(event.senderId());
    const auto chat = ChatTypeContact::findChat(m_chatManager, m_chatStorage, contact, ActionCreateAndAdd);
    if (!chat || chat.isIgnoreAllMessages())
        return;

    auto message = m_messageStorage->create();
    message.setId(eventId);
    message.setMessageChat(chat);
    message.setMessageSender(contact);
    message.setType(MessageTypeReceived);
    message.setSendDate(event.originTimestamp().toLocalTime());
    message.setReceiveDate(QDateTime::currentDateTime());

    auto text = event.plainBody();
    if (rawMessageTransformerService())
        text = QString::fromUtf8(rawMessageTransformerService()->transform(text.toUtf8(), message).rawContent());
    message.setContent(normalizeHtml(plainToHtml(text)));

    emit messageReceived(message);
}

void MatrixChatService::handleRoomMessageEvent(Quotient::Room *room, const Quotient::RoomMessageEvent &event,
                                               const QString &eventId)
{
    if (!m_contactManager || !m_messageStorage)
        return;

    const auto chat = roomChat(room);
    if (!chat || chat.isIgnoreAllMessages())
        return;

    const auto sentByCurrentAccount = m_connection && event.senderId() == m_connection->userId();
    const auto contact = sentByCurrentAccount ? account().accountContact()
                                              : m_contactManager->byId(account(), event.senderId(), ActionCreateAndAdd);
    if (!sentByCurrentAccount && m_contactAvatarService)
        m_contactAvatarService->observeContact(event.senderId());
    if (auto *details = qobject_cast<ChatDetailsRoom *>(chat.details()))
        details->addContact(contact);

    auto message = m_messageStorage->create();
    message.setId(eventId);
    message.setMessageChat(chat);
    message.setMessageSender(contact);
    message.setType(MessageTypeReceived);
    message.setSendDate(event.originTimestamp().toLocalTime());
    message.setReceiveDate(QDateTime::currentDateTime());

    auto text = event.plainBody();
    if (rawMessageTransformerService())
        text = QString::fromUtf8(rawMessageTransformerService()->transform(text.toUtf8(), message).rawContent());
    message.setContent(normalizeHtml(plainToHtml(text)));

    emit messageReceived(message);
}
