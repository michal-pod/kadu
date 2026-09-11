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
#include <Quotient/csapi/pushrules.h>
#include <Quotient/events/accountdataevents.h>
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
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtCore/QVariant>
#include <QtCore/QVariantMap>
#include <QtGui/QImage>
#include <QtGui/QImageReader>
#include <QtGui/QPixmap>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>

namespace MatrixNotificationRules
{
class GetPushRulesJob final : public Quotient::BaseJob
{
public:
    GetPushRulesJob()
            : Quotient::BaseJob{
                  Quotient::HttpVerb::Get, QStringLiteral("MatrixGetPushRulesJob"),
                  makePath("/_matrix/client/v3", "/pushrules/")}
    {
        addExpectedKey(QStringLiteral("global"));
    }

    Quotient::PushRuleset global() const
    {
        return loadFromJson<Quotient::PushRuleset>(QStringLiteral("global"));
    }
};

bool isMuteRule(const QVector<QVariant> &actions)
{
    return actions.isEmpty() ||
           (actions.size() == 1 && actions.constFirst().toString() == QStringLiteral("dont_notify"));
}

bool hasSoundTweak(const QVector<QVariant> &actions)
{
    return std::any_of(actions.cbegin(), actions.cend(), [](const QVariant &action) {
        return action.toMap().value(QStringLiteral("set_tweak")).toString() == QStringLiteral("sound");
    });
}

bool isOverrideMuteRule(const Quotient::PushRule &rule, const QString &roomId)
{
    if (!rule.enabled || !isMuteRule(rule.actions) || rule.conditions.size() != 1)
        return false;

    const auto &condition = rule.conditions.constFirst();
    return condition.kind == QStringLiteral("event_match") && condition.key == QStringLiteral("room_id") &&
           condition.pattern == roomId;
}

void appendNotificationRulesToDelete(
    QVector<QPair<QString, QString>> &result, const Quotient::PushRuleset &rules, const QString &roomId)
{
    for (const auto &rule : rules.override)
        if (!rule.isDefault && isOverrideMuteRule(rule, roomId))
            result.append(qMakePair(QStringLiteral("override"), rule.ruleId));

    for (const auto &rule : rules.room)
        if (!rule.isDefault && rule.ruleId == roomId)
            result.append(qMakePair(QStringLiteral("room"), rule.ruleId));
}

std::optional<ChatNotificationMode> modeForRoom(const Quotient::PushRuleset &rules, const QString &roomId)
{
    for (const auto &rule : rules.override)
        if (isOverrideMuteRule(rule, roomId))
            return ChatNotificationMode::NoNotifications;

    const auto roomRule = std::find_if(rules.room.cbegin(), rules.room.cend(), [&roomId](const auto &rule) {
        return rule.ruleId == roomId;
    });
    if (roomRule == rules.room.cend() || !roomRule->enabled)
        return ChatNotificationMode::Default;

    if (isMuteRule(roomRule->actions))
        return ChatNotificationMode::MentionsOnly;
    if (hasSoundTweak(roomRule->actions))
        return ChatNotificationMode::AllMessages;

    // Preserve an unrecognised custom rule until the user explicitly replaces it.
    return std::nullopt;
}

}

namespace MatrixRoomTags
{
struct Priority
{
    ChatPriority value;
    quint32 order;
};

quint32 priorityOrder(const Quotient::Tag &tag)
{
    if (!tag.order || !std::isfinite(*tag.order))
        return 0;

    const auto order = std::clamp(static_cast<double>(*tag.order), 0.0, 1.0);
    return static_cast<quint32>(
               std::lround((1.0 - order) * static_cast<double>(CHAT_PRIORITY_ORDER_MAXIMUM - 1))) +
           1;
}

Priority priority(const Quotient::TagsMap &tags)
{
    const auto favorite = tags.constFind(QStringLiteral("m.favourite"));
    if (favorite != tags.cend())
        return {ChatPriority::Favorite, priorityOrder(*favorite)};

    const auto lowPriority = tags.constFind(QStringLiteral("m.lowpriority"));
    if (lowPriority != tags.cend())
        return {ChatPriority::LowPriority, priorityOrder(*lowPriority)};

    return {ChatPriority::Default, 0};
}

QString tagName(ChatPriority priority)
{
    switch (priority)
    {
    case ChatPriority::Favorite:
        return QStringLiteral("m.favourite");
    case ChatPriority::LowPriority:
        return QStringLiteral("m.lowpriority");
    case ChatPriority::Default:
        return {};
    }

    return {};
}

std::optional<Quotient::TagsMap> replacePriority(Quotient::TagsMap tags, ChatPriority newPriority)
{
    const auto oldPriority = priority(tags).value;
    if (oldPriority == newPriority)
        return std::nullopt;

    const auto oldTagName = tagName(oldPriority);
    const auto newTagName = tagName(newPriority);
    std::optional<Quotient::Tag> preservedTag;
    if (!newTagName.isEmpty() && tags.contains(newTagName))
        preservedTag = tags.value(newTagName);
    else if (!oldTagName.isEmpty() && tags.contains(oldTagName))
        preservedTag = tags.value(oldTagName);

    tags.remove(QStringLiteral("m.favourite"));
    tags.remove(QStringLiteral("m.lowpriority"));
    if (!newTagName.isEmpty())
        tags.insert(newTagName, preservedTag.value_or(Quotient::Tag{}));

    return tags;
}
}

MatrixChatService::MatrixChatService(Account account, QObject *parent) : ChatService{account, parent}
{
}

int MatrixChatService::maxMessageLength() const
{
    return -1;
}

bool MatrixChatService::setChatNotificationMode(const Chat &chat, ChatNotificationMode mode)
{
    if (chat.isNull() || !m_connection || !m_connection->isLoggedIn() || roomId(chat).isEmpty())
        return false;

    replaceNotificationModeRules(chat, mode);
    return true;
}

bool MatrixChatService::setChatPriority(const Chat &chat, ChatPriority priority)
{
    if (chat.isNull() || !m_connection || !m_connection->isLoggedIn())
        return false;

    if (priority != ChatPriority::Favorite && priority != ChatPriority::Default &&
        priority != ChatPriority::LowPriority)
        return false;

    const auto id = roomId(chat);
    auto *room = id.isEmpty() ? nullptr : m_connection->room(id, Quotient::JoinState::Join);
    if (!isSupportedRoom(room))
        return false;

    const auto tags = MatrixRoomTags::replacePriority(room->tags(), priority);
    if (tags)
    {
        room->setTags(*tags);
        synchronizeRoomPriority(room);
    }
    return true;
}

bool MatrixChatService::markChatRead(const Chat &chat)
{
    if (chat.isNull() || !m_connection || !m_connection->isLoggedIn())
        return false;

    const auto id = roomId(chat);
    if (id.isEmpty())
        return false;

    auto *room = m_connection->room(id, Quotient::JoinState::Join);
    if (!isSupportedRoom(room))
        return false;

    room->markAllMessagesAsRead();
    chat.setUnreadMessagesCount(0);
    return true;
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
    m_notificationRulesLoaded = false;
    m_notificationRulesLoading = false;

    if (!m_connection)
        return;

    connect(m_connection, &Quotient::Connection::newRoom, this, [this](Quotient::Room *room) {
        watchRoom(room);
        m_notificationRulesLoaded = false;
    });
    connect(m_connection, &Quotient::Connection::joinedRoom, this,
            [this](Quotient::Room *room, Quotient::Room *) {
                watchRoom(room);
                m_notificationRulesLoaded = false;
            });
    connect(m_connection, &Quotient::Connection::syncDone, this, [this] {
        if (m_initialSyncFinished)
        {
            synchronizeAfterSync();
            return;
        }

        const QPointer<Quotient::Connection> synchronizedConnection{m_connection};
        // libQuotient emits syncDone before applying the queued room updates. Keep
        // notifications disabled until events from a cache-less initial sync are consumed.
        QTimer::singleShot(0, this, [this, synchronizedConnection] {
            if (synchronizedConnection && synchronizedConnection == m_connection)
                synchronizeAfterSync();
        });
    });
    connect(m_connection, &Quotient::Connection::accountDataChanged, this, [this](const QString &type) {
        if (type == QStringLiteral("m.push_rules"))
            refreshNotificationModes();
    });
    connect(m_connection, &Quotient::Connection::directChatsListChanged, this,
            [this](const Quotient::DirectChatsMap &, const Quotient::DirectChatsMap &) {
                for (auto *room : m_connection->allRooms())
                    synchronizeRoom(room);
                m_notificationRulesLoaded = false;
            });

    for (auto *room : m_connection->allRooms())
        watchRoom(room);
}

void MatrixChatService::completeCachedStateLoading(bool cacheLoaded)
{
    if (cacheLoaded)
        synchronizeAfterSync();
}

void MatrixChatService::synchronizeAfterSync()
{
    if (!m_connection)
        return;

    m_initialSyncFinished = true;
    for (auto *room : m_connection->allRooms())
        synchronizeRoom(room);
    if (!m_notificationRulesLoaded)
        refreshNotificationModes();
}

void MatrixChatService::setContactAvatarService(MatrixContactAvatarService *contactAvatarService)
{
    m_contactAvatarService = contactAvatarService;
}

void MatrixChatService::refreshNotificationModes()
{
    if (!m_connection || !m_connection->isLoggedIn() || !m_initialSyncFinished || m_notificationRulesLoading)
        return;

    m_notificationRulesLoading = true;
    auto job = m_connection->callApi<MatrixNotificationRules::GetPushRulesJob>();
    connect(job, &Quotient::BaseJob::success, this, [this, job] {
        m_notificationRulesLoading = false;
        if (!m_connection)
            return;

        const auto rules = job->global();
        m_notificationRulesLoaded = true;
        for (auto *room : m_connection->allRooms())
        {
            const auto chat = roomChat(room);
            if (!chat)
                continue;

            const auto mode = MatrixNotificationRules::modeForRoom(rules, room->id());
            if (!mode || chat.notificationMode() == *mode)
                continue;

            chat.setNotificationMode(*mode);
            emit chatNotificationModeChanged(chat, *mode);
        }
    });
    connect(job, &Quotient::BaseJob::failure, this, [this] { m_notificationRulesLoading = false; });
}

void MatrixChatService::replaceNotificationModeRules(const Chat &chat, ChatNotificationMode mode)
{
    if (!m_connection)
    {
        failNotificationModeChange(chat);
        return;
    }

    const auto id = roomId(chat);
    auto job = m_connection->callApi<MatrixNotificationRules::GetPushRulesJob>();
    connect(job, &Quotient::BaseJob::success, this, [this, job, chat, mode, id] {
        const auto rules = job->global();
        QVector<QPair<QString, QString>> rulesToDelete;
        MatrixNotificationRules::appendNotificationRulesToDelete(rulesToDelete, rules, id);
        deleteNotificationModeRules(chat, mode, rulesToDelete, 0);
    });
    connect(job, &Quotient::BaseJob::failure, this,
            [this, job, chat] { failNotificationModeChange(chat, job->errorString()); });
}

void MatrixChatService::deleteNotificationModeRules(
    const Chat &chat, ChatNotificationMode mode, const QVector<QPair<QString, QString>> &rules, int index)
{
    if (!m_connection)
    {
        failNotificationModeChange(chat);
        return;
    }
    if (index >= rules.size())
    {
        createNotificationModeRule(chat, mode);
        return;
    }

    const auto rule = rules.at(index);
    auto job = m_connection->callApi<Quotient::DeletePushRuleJob>(rule.first, rule.second);
    connect(job, &Quotient::BaseJob::success, this,
            [this, chat, mode, rules, index] { deleteNotificationModeRules(chat, mode, rules, index + 1); });
    connect(job, &Quotient::BaseJob::failure, this, [this, job, chat, mode, rules, index] {
        if (job->error() == Quotient::BaseJob::NotFound)
            deleteNotificationModeRules(chat, mode, rules, index + 1);
        else
            failNotificationModeChange(chat, job->errorString());
    });
}

void MatrixChatService::createNotificationModeRule(const Chat &chat, ChatNotificationMode mode)
{
    if (!m_connection)
    {
        failNotificationModeChange(chat);
        return;
    }

    if (mode == ChatNotificationMode::Default)
    {
        chat.setNotificationMode(mode);
        emit chatNotificationModeChanged(chat, mode);
        refreshNotificationModes();
        return;
    }

    const auto id = roomId(chat);
    QVector<QVariant> actions;
    QVector<Quotient::PushCondition> conditions;
    auto kind = QStringLiteral("room");
    if (mode == ChatNotificationMode::AllMessages)
    {
        // Element recognises an explicit all-messages room rule only when it also enables the default sound.
        actions.append(QVariant{QStringLiteral("notify")});
        actions.append(QVariantMap{{QStringLiteral("set_tweak"), QStringLiteral("sound")},
                                   {QStringLiteral("value"), QStringLiteral("default")}});
    }
    else if (mode == ChatNotificationMode::MentionsOnly)
        actions.append(QVariant{QStringLiteral("dont_notify")});
    else if (mode == ChatNotificationMode::NoNotifications)
    {
        kind = QStringLiteral("override");
        actions.append(QVariant{QStringLiteral("dont_notify")});
        conditions.append(Quotient::PushCondition{
            QStringLiteral("event_match"), QStringLiteral("room_id"), id, {}, {}});
    }

    auto job = m_connection->callApi<Quotient::SetPushRuleJob>(kind, id, actions, QString{}, QString{}, conditions);
    connect(job, &Quotient::BaseJob::success, this, [this, chat, mode] {
        chat.setNotificationMode(mode);
        emit chatNotificationModeChanged(chat, mode);
        refreshNotificationModes();
    });
    connect(job, &Quotient::BaseJob::failure, this,
            [this, job, chat] { failNotificationModeChange(chat, job->errorString()); });
}

void MatrixChatService::failNotificationModeChange(const Chat &chat, const QString &details)
{
    emit chatNotificationModeChangeFailed(
        chat, details.isEmpty() ? tr("The Matrix server rejected the notification setting.")
                                : tr("The Matrix notification setting could not be changed: %1").arg(details));
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

    auto *room = m_connection->room(roomId(chat), Quotient::JoinState::Join);
    if (!isSupportedRoom(room))
        return false;

    postText(room, text, transactionId, relation);
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

    auto *room = m_connection->room(roomId(chat), Quotient::JoinState::Join);
    if (!isSupportedRoom(room))
        return false;

    postAttachment(room, filePath, description);
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

    auto *room = m_connection->room(roomId(chat), Quotient::JoinState::Join);
    if (!isSupportedRoom(room))
        return false;

    postLocation(room, geoUri);
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
    const auto id = roomId(chat);
    if (id.isEmpty())
        return;

    if (auto *room = m_connection ? m_connection->room(id, Quotient::JoinState::Join) : nullptr)
        room->leaveRoom();
}

bool MatrixChatService::isSupportedRoom(const Quotient::Room *room) const
{
    return room && room->joinState() == Quotient::JoinState::Join;
}

QString MatrixChatService::directPeerId(const Quotient::Room *room) const
{
    if (!m_connection || !room || !m_connection->isDirectChat(room->id()))
        return {};

    auto peerIds = m_connection->directChatMemberIds(room);
    peerIds.removeAll(m_connection->userId());
    peerIds.removeDuplicates();
    return peerIds.size() == 1 ? peerIds.constFirst() : QString{};
}

Chat MatrixChatService::roomChat(Quotient::Room *room) const
{
    if (!m_chatManager || !m_chatStorage || !isSupportedRoom(room))
        return Chat::null;

    auto chat = ChatTypeRoom::findChat(m_chatManager, m_chatStorage, account(), room->id(), ActionCreateAndAdd);
    if (!chat)
        return Chat::null;

    chat.setUnreadCountSource(ChatUnreadCountSource::ProtocolManaged);
    chat.addProperty(
        QStringLiteral("chat-widget:show-contacts-list"), !m_connection->isDirectChat(room->id()),
        CustomProperties::NonStorable);
    chat.addProperty(
        QStringLiteral("chat-widget:participant-count"),
        qMax(room->joinedCount(), static_cast<int>(chat.contacts().size())),
        CustomProperties::NonStorable);
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

    if (!isSupportedRoom(room))
    {
        if (!m_chatManager || !m_chatStorage || !room)
            return;

        const auto chat = ChatTypeRoom::findChat(
            m_chatManager, m_chatStorage, account(), room->id(), ActionReturnNull);
        if (!chat)
            return;

        if (auto *details = qobject_cast<ChatDetailsRoom *>(chat.details()))
            details->setConnected(false);
        m_chatManager->removeItem(chat);
        return;
    }

    if (!roomChat(room))
        return;

    synchronizeRoomDetails(room);
    synchronizeRoomMembers(room);
    synchronizeRoomPriority(room);
    synchronizeRoomUnreadCount(room);
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
        const auto peerId = directPeerId(room);
        details->setAvatar(
            peerId.isEmpty() ? QPixmap{} : QPixmap::fromImage(room->memberAvatar(peerId, AVATAR_SIZE)));
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

    ContactSet roomContacts{account().accountContact()};
    for (const auto &contact : details->contacts())
    {
        if (contact == account().accountContact() || room->memberState(contact.id()) == Quotient::Membership::Join)
            roomContacts.insert(contact);
    }

    if (m_connection && m_connection->isDirectChat(room->id()))
    {
        auto directMemberIds = m_connection->directChatMemberIds(room);
        directMemberIds.removeAll(m_connection->userId());
        directMemberIds.removeDuplicates();
        for (const auto &memberId : directMemberIds)
        {
            const auto contact = m_contactManager->byId(account(), memberId, ActionCreateAndAdd);
            if (!contact)
                continue;

            roomContacts.insert(contact);
            if (m_contactAvatarService)
                m_contactAvatarService->observeContact(memberId);
        }
    }

    const auto existingContacts = details->contacts();
    for (const auto &contact : existingContacts)
        if (!roomContacts.contains(contact))
            details->removeContact(contact);

    for (const auto &contact : roomContacts)
        details->addContact(contact);
}

void MatrixChatService::synchronizeRoomPriority(Quotient::Room *room)
{
    if (!m_initialSyncFinished || !isSupportedRoom(room))
        return;

    const auto chat = roomChat(room);
    if (!chat)
        return;

    const auto priority = MatrixRoomTags::priority(room->tags());
    if (chat.priority() == priority.value && chat.priorityOrder() == priority.order)
        return;

    chat.setPriority(priority.value);
    chat.setPriorityOrder(priority.order);
    emit chatPriorityChanged(chat, priority.value);
}

void MatrixChatService::synchronizeRoomUnreadCount(Quotient::Room *room)
{
    if (!m_initialSyncFinished || !isSupportedRoom(room))
        return;

    const auto chat = roomChat(room);
    if (!chat)
        return;

    const auto notificationCount = room->notificationCount();
    const auto unreadMessagesCount = notificationCount <= 0
                                         ? 0
                                         : static_cast<quint32>(qMin<quint64>(
                                               static_cast<quint64>(notificationCount),
                                               std::numeric_limits<quint32>::max()));
    chat.setUnreadMessagesCount(unreadMessagesCount);
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
        // A room first seen in an incremental sync emits addedMessages before
        // baseStateLoaded. Process its initial timeline once the room is usable.
        if (m_initialSyncFinished)
            handleNewMessages(room, room->minTimelineIndex(), room->maxTimelineIndex());
    });
    connect(room, &Quotient::Room::memberListChanged, this,
            [this, room] { synchronizeRoomMembers(room); });
    connect(room, &Quotient::Room::tagsChanged, this, [this, room] { synchronizeRoomPriority(room); });
    connect(room, &Quotient::Room::displaynameChanged, this,
            [this, room](Quotient::Room *, const QString &) { synchronizeRoom(room); });
    connect(room, &Quotient::Room::joinStateChanged, this,
            [this, room](Quotient::JoinState, Quotient::JoinState) { synchronizeRoom(room); });
    connect(room, &Quotient::Room::topicChanged, this, [this, room] { synchronizeRoomDetails(room); });
    connect(room, &Quotient::Room::avatarChanged, this, [this, room] { synchronizeRoomDetails(room); });
    connect(room, &Quotient::Room::notificationCountChanged, this,
            [this, room] { synchronizeRoomUnreadCount(room); });
    connect(room, &Quotient::Room::unreadStatsChanged, this,
            [this, room] { synchronizeRoomUnreadCount(room); });
    connect(room, &Quotient::Room::memberAvatarUpdated, this,
            [this, room](const Quotient::RoomMember &member) {
                if (member.id() == directPeerId(room))
                    synchronizeRoomDetails(room);
            });
    connect(room, &Quotient::Room::encryption, this, [this, room] {
        synchronizeRoom(room);
        m_notificationRulesLoaded = false;
    });
    connect(room, &QObject::destroyed, this, [this, room] {
        m_watchedRooms.remove(room);
        m_loadedRooms.remove(room);
    });
}

void MatrixChatService::handleNewMessages(Quotient::Room *room, int fromIndex, int toIndex)
{
    if (!m_connection || !m_initialSyncFinished || !m_loadedRooms.contains(room))
        return;

    if (!isSupportedRoom(room))
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
        handleRoomMessageEvent(room, *event, item->id());
    }
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
