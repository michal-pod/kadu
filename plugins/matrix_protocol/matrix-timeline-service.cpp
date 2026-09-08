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

#include "matrix-megolm-session-recovery.h"

#include "chat/chat-details-room.h"
#include "chat/chat-manager.h"
#include "chat/chat-storage.h"
#include "chat/timeline/chat-timeline-model.h"
#include "chat/type/chat-type-room.h"
#include "html/html-conversion.h"
#include "html/html-string.h"
#include "html/sanitized-html-string.h"

#include <Quotient/connection.h>
#include <Quotient/csapi/event_context.h>
#include <Quotient/csapi/message_pagination.h>
#include <Quotient/csapi/room_send.h>
#include <Quotient/csapi/rooms.h>
#include <Quotient/eventitem.h>
#include <Quotient/events/encryptedevent.h>
#include <Quotient/events/eventcontent.h>
#include <Quotient/events/filesourceinfo.h>
#include <Quotient/events/reactionevent.h>
#include <Quotient/events/redactionevent.h>
#include <Quotient/events/roomavatarevent.h>
#include <Quotient/events/roommemberevent.h>
#include <Quotient/events/roommessageevent.h>
#include <Quotient/events/roompowerlevelsevent.h>
#include <Quotient/events/roomevent.h>
#include <Quotient/events/simplestateevents.h>
#include <Quotient/events/stateevent.h>
#include <Quotient/events/stickerevent.h>
#include <Quotient/jobs/downloadfilejob.h>
#include <Quotient/jobs/mediathumbnailjob.h>
#include <Quotient/room.h>
#include <Quotient/roommember.h>
#include <Quotient/user.h>

#include <QtCore/QBuffer>
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFutureWatcher>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSet>
#include <QtCore/QStandardPaths>
#include <QtCore/QTemporaryFile>
#include <QtCore/QTimer>
#include <QtCore/QUrlQuery>
#include <QtCore/QVariantMap>
#include <QtConcurrent/QtConcurrentRun>
#include <QtGui/QColor>
#include <QtGui/QImageReader>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QVBoxLayout>

#include <algorithm>
#include <iterator>
#include <limits>
#include <utility>
#include <variant>

MatrixTimelineService::MatrixTimelineService(Account account, QObject *parent)
        : ProtocolTimelineService{account, parent}, m_sessionRecovery{new MatrixMegolmSessionRecovery{this}}
{
    m_attachmentImages.setMaxCost(AttachmentImageCacheSizeKiB);
    connect(m_sessionRecovery, &MatrixMegolmSessionRecovery::sessionRestored, this,
            &MatrixTimelineService::updateTimelineEventsForMegolmSession);
    connect(m_sessionRecovery, &MatrixMegolmSessionRecovery::backupRestored, this,
            &MatrixTimelineService::refreshEncryptedEvents);
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
    m_attachmentImageDimensions.clear();
    m_attachmentRequestedSizes.clear();
    m_attachmentStates.clear();
    m_attachmentProgress.clear();
    m_attachmentErrors.clear();
    m_attachmentThumbnailRequests.clear();
    m_attachmentImageDecodes.clear();
    m_unavailableAttachmentThumbnails.clear();
    m_attachmentPreviewsUsingOriginal.clear();
    m_invalidImageAttachments.clear();
    m_attachmentSources.clear();
    m_attachmentFileNames.clear();
    m_attachmentKinds.clear();
    m_memberAvatarSources.clear();
    m_decryptedEventSources.clear();
    m_eventTransactionIds.clear();
    m_sessionRecovery->setConnection(connection);
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

bool MatrixTimelineService::canManagePinnedMessages(const Quotient::Room *room) const
{
    if (!room || !m_connection || m_connection->userId().isEmpty())
        return false;

    return room->memberEffectivePowerLevel() >= room->powerLevelFor(
               QStringLiteral("m.room.pinned_events"), true);
}

bool MatrixTimelineService::canRedactEvent(const Quotient::Room *room, const Quotient::RoomEvent &event) const
{
    if (!room || !m_connection)
        return false;
    if (event.senderId() == m_connection->userId())
        return true;

    const auto *powerLevels = room->currentState().get<Quotient::RoomPowerLevelsEvent>();
    const auto requiredPowerLevel = powerLevels ? powerLevels->redact() : 50;
    return room->memberEffectivePowerLevel() >= requiredPowerLevel;
}

ChatTimelineActions MatrixTimelineService::availableActions(const Chat &chat, const QString &stableId) const
{
    auto *room = roomForChat(chat);
    if (!room || stableId.isEmpty())
        return {};

    ChatTimelineActions actions;
    const auto isPinned = room->pinnedEventIds().contains(stableId);
    if (isPinned)
    {
        actions |= ChatTimelineAction::ShowSource;
        if (canManagePinnedMessages(room))
            actions |= ChatTimelineAction::Unpin;
    }

    const auto timelineItem = room->findInTimeline(stableId);
    if (timelineItem != room->historyEdge())
    {
        Quotient::RoomEventPtr decryptedEvent;
        auto encrypted = false;
        const auto *event = eventForTimelineItem(room, *timelineItem, decryptedEvent, encrypted);
        if (!event)
            return actions;

        actions |= ChatTimelineAction::ShowSource;
        const auto *messageEvent = Quotient::eventCast<const Quotient::RoomMessageEvent>(event);
        if (!messageEvent || event->isRedacted())
            return actions;

        actions |= ChatTimelineAction::React;
        actions |= ChatTimelineAction::Reply;
        if (!isPinned && canManagePinnedMessages(room))
            actions |= ChatTimelineAction::Pin;
        if (m_connection && messageEvent->senderId() == m_connection->userId())
            actions |= ChatTimelineAction::Edit;
        if (canRedactEvent(room, *messageEvent))
            actions |= ChatTimelineAction::Delete;
        if (messageEvent->get<Quotient::EventContent::FileContentBase>())
            actions |= ChatTimelineAction::SaveAttachment;
        return actions;
    }

    const auto transactionId = transactionIdForLocalEcho(stableId);
    if (transactionId.isEmpty())
        return actions;

    for (const auto &pendingEvent : room->pendingEvents())
        if (pendingEvent->transactionId() == transactionId)
            return actions | ChatTimelineAction::ShowSource;
    return actions;
}

bool MatrixTimelineService::executeAction(const Chat &chat, const QString &stableId, ChatTimelineAction action)
{
    auto *room = roomForChat(chat);
    if (!room || !availableActions(chat, stableId).testFlag(action))
        return false;

    if (action == ChatTimelineAction::ShowSource)
    {
        const auto timelineItem = room->findInTimeline(stableId);
        if (timelineItem != room->historyEdge())
        {
            showEventSource(stableId, *timelineItem->event());
            return true;
        }

        const auto transactionId = transactionIdForLocalEcho(stableId);
        if (!transactionId.isEmpty())
        {
            for (const auto &pendingEvent : room->pendingEvents())
            {
                if (pendingEvent->transactionId() == transactionId)
                {
                    showEventSource(stableId, *pendingEvent.event());
                    return true;
                }
            }
        }

        if (room->pinnedEventIds().contains(stableId) && m_connection)
        {
            m_connection->callApi<Quotient::GetOneRoomEventJob>(room->id(), stableId).then(
                this,
                [this, stableId](Quotient::GetOneRoomEventJob *job) {
                    const auto event = job->event();
                    if (event)
                        showEventSource(stableId, *event);
                },
                [this, chat, stableId] {
                    emit timelineWarning(chat, stableId, tr("Matrix event source"),
                                         tr("Could not retrieve the event source."));
                });
            return true;
        }
        return false;
    }

    if (action == ChatTimelineAction::Unpin)
    {
        auto pinnedEventIds = room->pinnedEventIds();
        pinnedEventIds.removeAll(stableId);
        room->setPinnedEvents(pinnedEventIds);
        return true;
    }
    if (action == ChatTimelineAction::Pin)
    {
        auto pinnedEventIds = room->pinnedEventIds();
        if (!pinnedEventIds.contains(stableId))
            pinnedEventIds.append(stableId);
        room->setPinnedEvents(pinnedEventIds);
        return true;
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
        emit timelineWarning(chat, stableId, tr("Save attachment"),
                             tr("The attachment is no longer available locally."));
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
        emit timelineWarning(chat, stableId, tr("Save attachment"), tr("Could not start downloading the attachment."));
        return false;
    }

    m_attachmentStates.insert(stableId, ChatTimelineAttachmentState::Downloading);
    m_attachmentProgress.insert(stableId, 0.0);
    m_attachmentErrors.remove(stableId);
    updateAttachmentEvent(room, stableId);

    const QPointer<Quotient::Room> downloadRoom{room};
    connect(job, &Quotient::BaseJob::downloadProgress, this,
            [this, downloadRoom, stableId](qint64 received, qint64 total) {
                if (!downloadRoom)
                    return;

                m_attachmentProgress.insert(stableId, total > 0 ? qreal(received) / qreal(total) : 0.0);
                updateAttachmentEvent(downloadRoom.data(), stableId);
            });
    connect(job, &Quotient::BaseJob::success, this, [this, downloadRoom, stableId] {
        if (!downloadRoom)
            return;

        m_attachmentStates.insert(stableId, ChatTimelineAttachmentState::Available);
        m_attachmentProgress.insert(stableId, 1.0);
        m_attachmentErrors.remove(stableId);
        updateAttachmentEvent(downloadRoom.data(), stableId);
    });
    connect(job, &Quotient::BaseJob::failure, this, [this, downloadRoom, chat, stableId, job] {
        if (!downloadRoom)
            return;

        const auto errorText = job->errorString().isEmpty() ? tr("Could not save the attachment.") : job->errorString();
        m_attachmentStates.insert(stableId, ChatTimelineAttachmentState::Failed);
        m_attachmentProgress.remove(stableId);
        m_attachmentErrors.insert(stableId, errorText);
        updateAttachmentEvent(downloadRoom.data(), stableId);
        emit timelineWarning(chat, stableId, tr("Save attachment"), tr("Could not save the attachment: %1").arg(errorText));
    });
    return true;
}

bool MatrixTimelineService::removeOwnReaction(const Chat &chat, const QString &stableId, const QString &key)
{
    auto *room = roomForChat(chat);
    if (!room || !m_connection || stableId.isEmpty() || key.isEmpty() ||
        !availableActions(chat, stableId).testFlag(ChatTimelineAction::React))
        return false;

    for (const auto *relatedEvent : room->relatedEvents(stableId, Quotient::EventRelation::AnnotationType))
    {
        const auto *reactionEvent = Quotient::eventCast<const Quotient::ReactionEvent>(relatedEvent);
        if (!reactionEvent || relatedEvent->isRedacted() || reactionEvent->key() != key ||
            reactionEvent->senderId() != m_connection->userId())
            continue;

        rememberReactionEvent(relatedEvent->id(), stableId);
        room->redactEvent(relatedEvent->id());
        return true;
    }
    return false;
}

bool MatrixTimelineService::addReaction(const Chat &chat, const QString &stableId, const QString &key)
{
    auto *room = roomForChat(chat);
    if (!room || !m_connection || stableId.isEmpty() || key.isEmpty() ||
        !availableActions(chat, stableId).testFlag(ChatTimelineAction::React))
        return false;

    // Do not create duplicate annotations when the user chooses an already
    // active reaction from the quick selector.
    for (const auto *relatedEvent : room->relatedEvents(stableId, Quotient::EventRelation::AnnotationType))
    {
        const auto *reactionEvent = Quotient::eventCast<const Quotient::ReactionEvent>(relatedEvent);
        if (reactionEvent && !relatedEvent->isRedacted() && reactionEvent->key() == key &&
            reactionEvent->senderId() == m_connection->userId())
            return false;
    }

    auto event = Quotient::makeEvent<Quotient::ReactionEvent>(stableId, key);
    const auto job = m_connection->callApi<Quotient::SendMessageJob>(
        Quotient::BackgroundRequest, room->id(), event->matrixType(), m_connection->generateTxnId(), event->contentJson());
    return !job.isNull();
}

QVariantList MatrixTimelineService::pinnedMessages(const Chat &chat) const
{
    auto *room = roomForChat(chat);
    if (!room)
        return {};

    QVariantList result;
    for (const auto &eventId : room->pinnedEventIds())
    {
        QVariantMap record{{QStringLiteral("stableId"), eventId},
                           {QStringLiteral("available"), false},
                           {QStringLiteral("plainText"), tr("This pinned message is not loaded yet.")},
                           {QStringLiteral("formattedText"), QString{}},
                           {QStringLiteral("senderDisplayName"), QString{}},
                           {QStringLiteral("senderAvatarSource"), QUrl{}},
                           {QStringLiteral("senderColor"), QColor{}},
                           {QStringLiteral("protocolEventType"), QString{}},
                           {QStringLiteral("timestamp"), QDateTime{}},
                           {QStringLiteral("encrypted"), false},
                           {QStringLiteral("decryptionState"),
                            static_cast<int>(ChatTimelineDecryptionState::NotEncrypted)},
                           {QStringLiteral("redacted"), false}};

        const auto timelineItem = room->findInTimeline(eventId);
        if (timelineItem != room->historyEdge())
        {
            Quotient::RoomEventPtr decryptedEvent;
            auto encrypted = false;
            const auto *event = eventForTimelineItem(room, *timelineItem, decryptedEvent, encrypted);
            if (event)
            {
                record = ChatTimelineModel::itemData(
                    itemForEvent(room, *event, eventId, timelineItem->index(), encrypted));
                record.insert(QStringLiteral("available"), true);
            }
        }
        result.append(record);
    }
    return result;
}

QVariantMap MatrixTimelineService::roomInfo(const Chat &chat) const
{
    const auto *room = roomForChat(chat);
    if (!room)
        return {};

    const auto state = room->currentState();
    const auto powerLevels = state.contentJson(QStringLiteral("m.room.power_levels"));
    const auto ownPowerLevel = room->memberEffectivePowerLevel();
    const auto usersDefault = powerLevels.value(QStringLiteral("users_default")).toInt(0);
    const auto messagePowerLevel = room->powerLevelFor(QStringLiteral("m.room.message"));
    const auto topicPowerLevel = room->powerLevelFor(QStringLiteral("m.room.topic"), true);
    const auto invitePowerLevel = powerLevels.value(QStringLiteral("invite")).toInt(0);
    const auto kickPowerLevel = powerLevels.value(QStringLiteral("kick")).toInt(50);
    const auto banPowerLevel = powerLevels.value(QStringLiteral("ban")).toInt(50);
    const auto redactPowerLevel = powerLevels.value(QStringLiteral("redact")).toInt(50);
    const auto pinnedPowerLevel = room->powerLevelFor(QStringLiteral("m.room.pinned_events"), true);
    const auto joinRulesPowerLevel = room->powerLevelFor(QStringLiteral("m.room.join_rules"), true);
    const auto powerLevelsPowerLevel = room->powerLevelFor(QStringLiteral("m.room.power_levels"), true);
    const auto namePowerLevel = room->powerLevelFor(QStringLiteral("m.room.name"), true);
    const auto historyVisibility =
        state.contentJson(QStringLiteral("m.room.history_visibility")).value(QStringLiteral("history_visibility")).toString(
            QStringLiteral("shared"));
    const auto guestAccess =
        state.contentJson(QStringLiteral("m.room.guest_access")).value(QStringLiteral("guest_access")).toString(
            QStringLiteral("forbidden"));
    const auto create = state.contentJson(QStringLiteral("m.room.create"));
    const auto federated = !create.contains(QStringLiteral("m.federate")) ||
                           create.value(QStringLiteral("m.federate")).toBool(true);
    const auto joinRules = state.contentJson(QStringLiteral("m.room.join_rules"));
    const auto joinRule = joinRuleName(joinRules);

    const auto canSendMessages = ownPowerLevel >= messagePowerLevel;
    const auto canSetTopic = ownPowerLevel >= topicPowerLevel;
    const auto canInvite = ownPowerLevel >= invitePowerLevel;
    const auto canKick = ownPowerLevel >= kickPowerLevel;
    const auto canBan = ownPowerLevel >= banPowerLevel;
    const auto canRedactOthers = ownPowerLevel >= redactPowerLevel;
    const auto canManagePins = ownPowerLevel >= pinnedPowerLevel;
    const auto canManageState = ownPowerLevel >= namePowerLevel || ownPowerLevel >= joinRulesPowerLevel;
    const auto canManagePermissions = ownPowerLevel >= powerLevelsPowerLevel;

    QStringList flags{QStringLiteral("group"), QStringLiteral("noExternalMessages")};
    QStringList ircModes{QStringLiteral("n")};
    const auto addIrcMode = [&ircModes](const QString &mode) {
        if (!ircModes.contains(mode))
            ircModes.append(mode);
    };
    const auto addFlag = [&flags](const QString &flag) {
        if (!flags.contains(flag))
            flags.append(flag);
    };

    if (joinRule == QStringLiteral("invite") || joinRule == QStringLiteral("private"))
    {
        addFlag(QStringLiteral("inviteOnly"));
        addIrcMode(QStringLiteral("i"));
    }
    else if (joinRule == QStringLiteral("restricted"))
    {
        addFlag(QStringLiteral("restrictedAccess"));
    }
    else if (joinRule == QStringLiteral("knock"))
    {
        addFlag(QStringLiteral("knockAllowed"));
    }
    else if (joinRule == QStringLiteral("knock_restricted"))
    {
        addFlag(QStringLiteral("restrictedAccess"));
        addFlag(QStringLiteral("knockAllowed"));
    }
    if (messagePowerLevel > usersDefault)
    {
        addFlag(QStringLiteral("moderated"));
        addIrcMode(QStringLiteral("m"));
    }
    if (topicPowerLevel > usersDefault)
    {
        addFlag(QStringLiteral("topicProtected"));
        addIrcMode(QStringLiteral("t"));
    }
    if (room->usesEncryption())
    {
        addFlag(QStringLiteral("encrypted"));
        addIrcMode(QStringLiteral("E")); // Kadu extension: Matrix E2EE has no IRC mode equivalent.
    }
    if (guestAccess == QStringLiteral("can_join"))
        addFlag(QStringLiteral("guestsCanJoin"));
    if (!federated)
        addFlag(QStringLiteral("nonFederated"));

    QString memberPrefix;
    QString memberRole = QStringLiteral("member");
    if (canManagePermissions)
    {
        memberPrefix = QStringLiteral("&");
        memberRole = QStringLiteral("administrator");
    }
    else if (canKick || canBan || canRedactOthers)
    {
        memberPrefix = QStringLiteral("@");
        memberRole = QStringLiteral("moderator");
    }
    else if (messagePowerLevel > usersDefault && canSendMessages)
    {
        memberPrefix = QStringLiteral("+");
        memberRole = QStringLiteral("participant");
    }

    QVariantList allowedRooms;
    for (const auto &allowedRoom : joinRules.value(QStringLiteral("allow")).toArray())
    {
        const auto roomId = allowedRoom.toObject().value(QStringLiteral("room_id")).toString();
        if (!roomId.isEmpty())
            allowedRooms.append(roomId);
    }

    return {{QStringLiteral("group"), true},
            {QStringLiteral("ircModes"), QStringLiteral("+") + ircModes.join(QString{})},
            {QStringLiteral("flags"), flags},
            {QStringLiteral("historyVisibility"), historyVisibility},
            {QStringLiteral("memberPrefix"), memberPrefix},
            {QStringLiteral("memberRole"), memberRole},
            {QStringLiteral("permissions"),
             QVariantMap{{QStringLiteral("sendMessages"), canSendMessages},
                         {QStringLiteral("setTopic"), canSetTopic},
                         {QStringLiteral("invite"), canInvite},
                         {QStringLiteral("kick"), canKick},
                         {QStringLiteral("ban"), canBan},
                         {QStringLiteral("redactOthers"), canRedactOthers},
                         {QStringLiteral("managePins"), canManagePins},
                         {QStringLiteral("manageState"), canManageState},
                         {QStringLiteral("managePermissions"), canManagePermissions}}},
            {QStringLiteral("native"),
             QVariantMap{{QStringLiteral("protocol"), QStringLiteral("matrix")},
                         {QStringLiteral("joinRule"), joinRule},
                         {QStringLiteral("allowedRooms"), allowedRooms},
                         {QStringLiteral("guestAccess"), guestAccess},
                         {QStringLiteral("federated"), federated},
                         {QStringLiteral("ownPowerLevel"), ownPowerLevel},
                         {QStringLiteral("usersDefault"), usersDefault},
                         {QStringLiteral("messagePowerLevel"), messagePowerLevel},
                         {QStringLiteral("topicPowerLevel"), topicPowerLevel}}}};
}

QString MatrixTimelineService::chatHeaderTitle(const Chat &chat) const
{
    const auto name = chat.display().isEmpty() ? chat.name() : chat.display();
    if (name.isEmpty())
        return {};

    const auto *room = roomForChat(chat);
    const auto directChat = room && m_connection && m_connection->isDirectChat(room->id());
    return directChat ? tr("Chat with %1").arg(name) : tr("Room %1").arg(name);
}

void MatrixTimelineService::markTimelineItemRead(const Chat &chat, const QString &stableId)
{
    if (stableId.isEmpty() || !transactionIdForLocalEcho(stableId).isEmpty())
        return;

    if (auto *room = roomForChat(chat))
        room->markMessagesAsRead(stableId);
}

QImage MatrixTimelineService::requestAttachmentImage(const Chat &chat, const QUrl &sourceUri, const QSize &requestedSize)
{
    const auto eventId = eventIdForAttachmentUri(sourceUri);
    const auto thumbnail = isAttachmentThumbnailUri(sourceUri);
    const auto resourceId = attachmentResourceId(eventId, thumbnail);
    auto *room = roomForChat(chat);
    if (eventId.isEmpty() || !room)
        return {};

    if (m_attachmentKinds.value(eventId, ChatTimelineAttachmentKind::File) != ChatTimelineAttachmentKind::Image)
        return {};

    if (const auto *image = m_attachmentImages.object(resourceId))
        return *image;
    if (m_attachmentStates.value(resourceId) == ChatTimelineAttachmentState::Failed)
        return {};
    if (m_attachmentDownloadPaths.contains(resourceId) || m_attachmentThumbnailRequests.contains(resourceId) ||
        m_attachmentImageDecodes.contains(resourceId))
        return {};

    const auto source = m_attachmentSources.constFind(resourceId);
    if (thumbnail && source == m_attachmentSources.cend())
    {
        const auto originalSource = m_attachmentSources.constFind(eventId);
        if (originalSource == m_attachmentSources.cend())
        {
            m_attachmentStates.insert(resourceId, ChatTimelineAttachmentState::Failed);
            m_attachmentErrors.insert(resourceId, tr("The image source is not available."));
            updateAttachmentEvent(room, eventId);
            return {};
        }

        if (const auto fileUrl = std::get_if<QUrl>(&originalSource.value()))
        {
            const auto size = requestedSize.isValid() ? requestedSize : QSize{640, 480};
            auto *job = m_connection->getThumbnail(*fileUrl, size);
            if (!job)
            {
                m_unavailableAttachmentThumbnails.insert(eventId);
                m_attachmentStates.remove(resourceId);
                m_attachmentErrors.remove(resourceId);
                updateAttachmentEvent(room, eventId);
                return {};
            }

            m_attachmentThumbnailRequests.insert(resourceId);
            m_attachmentStates.insert(resourceId, ChatTimelineAttachmentState::Downloading);
            m_attachmentProgress.insert(resourceId, 0.0);
            m_attachmentErrors.remove(resourceId);
            updateAttachmentEvent(room, eventId);

            const QPointer<Quotient::Room> downloadRoom{room};
            connect(job, &Quotient::BaseJob::success, this, [this, downloadRoom, resourceId, eventId, job] {
                m_attachmentThumbnailRequests.remove(resourceId);
                if (!downloadRoom)
                    return;

                const auto image = job->thumbnail();
                if (image.isNull())
                {
                    m_unavailableAttachmentThumbnails.insert(eventId);
                    m_attachmentStates.remove(resourceId);
                    m_attachmentProgress.remove(resourceId);
                    m_attachmentErrors.remove(resourceId);
                }
                else
                {
                    m_attachmentImages.insert(resourceId, new QImage{image}, attachmentImageCacheCost(image));
                    m_attachmentImageDimensions.insert(eventId, image.size());
                    m_attachmentStates.insert(resourceId, ChatTimelineAttachmentState::Available);
                    m_attachmentProgress.insert(resourceId, 1.0);
                    m_attachmentErrors.remove(resourceId);
                }
                updateAttachmentEvent(downloadRoom.data(), eventId);
            });
            connect(job, &Quotient::BaseJob::failure, this, [this, downloadRoom, resourceId, eventId] {
                m_attachmentThumbnailRequests.remove(resourceId);
                if (!downloadRoom)
                    return;

                m_unavailableAttachmentThumbnails.insert(eventId);
                m_attachmentStates.remove(resourceId);
                m_attachmentProgress.remove(resourceId);
                m_attachmentErrors.remove(resourceId);
                updateAttachmentEvent(downloadRoom.data(), eventId);
            });
            return {};
        }
    }

    QTemporaryFile temporaryFile{QDir::tempPath() + QStringLiteral("/kadu-matrix-image-XXXXXX")};
    temporaryFile.setAutoRemove(false);
    if (!temporaryFile.open())
    {
        m_attachmentStates.insert(resourceId, ChatTimelineAttachmentState::Failed);
        m_attachmentErrors.insert(resourceId, tr("Could not create a temporary file for the image."));
        updateAttachmentEvent(room, eventId);
        return {};
    }

    const auto temporaryPath = temporaryFile.fileName();
    temporaryFile.close();
    m_attachmentDownloadPaths.insert(resourceId, temporaryPath);
    if (requestedSize.isValid())
        m_attachmentRequestedSizes.insert(resourceId, requestedSize);
    m_attachmentStates.insert(resourceId, ChatTimelineAttachmentState::Downloading);
    m_attachmentProgress.insert(resourceId, 0.0);
    m_attachmentErrors.remove(resourceId);
    updateAttachmentEvent(room, eventId);

    if (source == m_attachmentSources.cend())
    {
        handleAttachmentDownloadFailed(room, resourceId, eventId, tr("The image source is not available."));
        return {};
    }

    Quotient::DownloadFileJob *job = nullptr;
    if (const auto encryptedFile = std::get_if<Quotient::EncryptedFileMetadata>(&source.value()))
        job = m_connection->downloadFile(encryptedFile->url, *encryptedFile, temporaryPath);
    else if (const auto fileUrl = std::get_if<QUrl>(&source.value()))
        job = m_connection->downloadFile(*fileUrl, temporaryPath);

    if (!job)
    {
        handleAttachmentDownloadFailed(room, resourceId, eventId, tr("Could not start downloading the image."));
        return {};
    }

    const QPointer<Quotient::Room> downloadRoom{room};
    connect(job, &Quotient::BaseJob::downloadProgress, this,
            [this, downloadRoom, resourceId, eventId](qint64 received, qint64 total) {
                if (downloadRoom)
                    handleAttachmentDownloadProgress(downloadRoom.data(), resourceId, eventId, received, total);
            });
    connect(job, &Quotient::BaseJob::success, this, [this, downloadRoom, resourceId, eventId, job] {
        if (downloadRoom)
            handleAttachmentDownloadCompleted(
                downloadRoom.data(), resourceId, eventId, QUrl::fromLocalFile(job->targetFileName()));
    });
    connect(job, &Quotient::BaseJob::failure, this, [this, downloadRoom, resourceId, eventId, job] {
        if (downloadRoom)
            handleAttachmentDownloadFailed(downloadRoom.data(), resourceId, eventId, job->errorString());
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

    static const auto remoteCursorPrefix = QByteArrayLiteral("matrix:");
    if ((request.mode == ChatTimelineRequestMode::Older || request.mode == ChatTimelineRequestMode::Newer) &&
        request.cursor.startsWith(remoteCursorPrefix))
        return requestPaginatedPage(request, room);
    // A local page containing only hidden events has a numeric cursor but no
    // visible anchor. Keep scanning local history until an anchor is available.
    if (request.mode != ChatTimelineRequestMode::Latest &&
        !(request.mode == ChatTimelineRequestMode::Older && request.anchorId.isEmpty()))
        return requestContextPage(request, room);

    if (room->allHistoryLoaded())
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

            auto page = pageForRoom(request, watchedRoom.data());
            const auto chat = chatForRoom(watchedRoom.data());
            if (chat)
                emit pinnedMessagesChanged(chat);
            finishRequest(promise, std::move(page));
        },
        [this, promise] {
            ChatTimelinePage page;
            page.error = tr("Could not retrieve Matrix history.");
            finishRequest(promise, std::move(page));
        });
    return future;
}

QFuture<ChatTimelinePage> MatrixTimelineService::requestContextPage(const ChatTimelineRequest &request,
                                                                    Quotient::Room *room)
{
    if (!m_connection || !room || request.anchorId.isEmpty())
    {
        ChatTimelinePage page;
        page.error = tr("Matrix event is not available.");
        return completedPage(std::move(page));
    }

    const auto requestedLimit = request.limit > 0 ? request.limit : 50;
    const auto contextLimit = requestedLimit;
    auto promise = std::make_shared<QPromise<ChatTimelinePage>>();
    auto future = promise->future();
    promise->start();

    const QPointer<Quotient::Room> watchedRoom{room};
    m_connection->callApi<Quotient::GetEventContextJob>(room->id(), request.anchorId, contextLimit).then(
        this,
        [this, promise, request, watchedRoom](Quotient::GetEventContextJob *job) {
            ChatTimelinePage page;
            if (!watchedRoom || !job)
            {
                page.error = tr("Matrix room is no longer available.");
                finishRequest(promise, std::move(page));
                return;
            }

            auto before = job->eventsBefore();
            auto event = job->event();
            auto after = job->eventsAfter();
            static const auto remoteCursorPrefix = QByteArrayLiteral("matrix:");
            page.olderCursor = job->begin().isEmpty() ? QByteArray{} : remoteCursorPrefix + job->begin().toUtf8();
            page.newerCursor = job->end().isEmpty() ? QByteArray{} : remoteCursorPrefix + job->end().toUtf8();
            page.hasOlder = !page.olderCursor.isEmpty();
            page.hasNewer = !page.newerCursor.isEmpty();

            auto appendEvent = [this, &page, watchedRoom](const Quotient::RoomEventPtr &remoteEvent) {
                if (!remoteEvent)
                    return;
                auto item = itemForDetachedEvent(watchedRoom.data(), *remoteEvent);
                if (!item.stableId.isEmpty())
                    page.items.append(std::move(item));
            };

            if (request.mode == ChatTimelineRequestMode::Older)
            {
                // events_before is reverse chronological. Reverse the whole
                // returned page so no events between it and the anchor are
                // skipped when begin() becomes the next pagination token.
                for (auto index = before.size(); index > 0; --index)
                    appendEvent(before.at(index - 1));
                page.hasNewer = true;
                if (page.newerCursor.isEmpty())
                    page.newerCursor = request.anchorId.toUtf8();
            }
            else if (request.mode == ChatTimelineRequestMode::Newer)
            {
                for (const auto &remoteEvent : after)
                    appendEvent(remoteEvent);
                page.hasOlder = true;
                if (page.olderCursor.isEmpty())
                    page.olderCursor = request.anchorId.toUtf8();
            }
            else
            {
                for (auto index = before.size(); index > 0; --index)
                    appendEvent(before.at(index - 1));
                appendEvent(event);
                for (const auto &remoteEvent : after)
                    appendEvent(remoteEvent);
            }

            finishRequest(promise, std::move(page));
        },
        [this, promise] {
            ChatTimelinePage page;
            page.error = tr("Could not retrieve the Matrix event context.");
            finishRequest(promise, std::move(page));
        });
    return future;
}

QFuture<ChatTimelinePage> MatrixTimelineService::requestPaginatedPage(const ChatTimelineRequest &request,
                                                                      Quotient::Room *room)
{
    static const auto remoteCursorPrefix = QByteArrayLiteral("matrix:");
    const auto token = QString::fromUtf8(request.cursor.sliced(remoteCursorPrefix.size()));
    const auto older = request.mode == ChatTimelineRequestMode::Older;
    const auto direction = older ? QStringLiteral("b") : QStringLiteral("f");
    const auto requestedLimit = request.limit > 0 ? request.limit : 50;
    auto promise = std::make_shared<QPromise<ChatTimelinePage>>();
    auto future = promise->future();
    promise->start();

    const QPointer<Quotient::Room> watchedRoom{room};
    m_connection->callApi<Quotient::GetRoomEventsJob>(room->id(), direction, token, QString{}, requestedLimit).then(
        this,
        [this, promise, request, older, watchedRoom](Quotient::GetRoomEventsJob *job) {
            ChatTimelinePage page;
            if (!watchedRoom || !job)
            {
                page.error = tr("Matrix room is no longer available.");
                finishRequest(promise, std::move(page));
                return;
            }

            static const auto remoteCursorPrefix = QByteArrayLiteral("matrix:");
            const auto nextCursor = job->end().isEmpty() ? QByteArray{}
                                                         : remoteCursorPrefix + job->end().toUtf8();
            auto events = job->chunk();
            // Filtering/visibility can produce an empty chunk before the end.
            // Only the pagination token determines whether to keep fetching.
            const auto hasMore = !nextCursor.isEmpty() && nextCursor != request.cursor;
            auto appendEvent = [this, &page, watchedRoom](const Quotient::RoomEventPtr &remoteEvent) {
                if (!remoteEvent)
                    return;
                auto item = itemForDetachedEvent(watchedRoom.data(), *remoteEvent);
                if (!item.stableId.isEmpty())
                    page.items.append(std::move(item));
            };

            if (older)
            {
                for (auto index = events.size(); index > 0; --index)
                    appendEvent(events.at(index - 1));
                page.olderCursor = nextCursor;
                page.hasOlder = hasMore;
                page.newerCursor = request.cursor;
                page.hasNewer = true;
            }
            else
            {
                for (const auto &remoteEvent : events)
                    appendEvent(remoteEvent);
                page.newerCursor = nextCursor;
                page.hasNewer = hasMore;
                page.olderCursor = request.cursor;
                page.hasOlder = true;
            }
            finishRequest(promise, std::move(page));
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

    auto requestStarted = std::make_shared<bool>(false);
    auto requestFinished = std::make_shared<bool>(false);
    auto *requestContextObject = new QObject{this};
    const QPointer<QObject> requestContext{requestContextObject};
    auto *initialStateTimer = new QTimer{requestContextObject};
    initialStateTimer->setSingleShot(true);
    initialStateTimer->setInterval(30000);
    const QPointer<Quotient::Room> watchedRoom{room};
    connect(room, &Quotient::Room::baseStateLoaded, requestContextObject,
            [this, promise, request, watchedRoom, requestStarted, requestFinished, requestContext, initialStateTimer] {
        if (*requestStarted || *requestFinished)
            return;
        *requestStarted = true;
        initialStateTimer->stop();
        if (!watchedRoom)
        {
            ChatTimelinePage page;
            page.error = tr("Matrix room is no longer available.");
            finishInitialStateRequest(promise, requestFinished, requestContext, std::move(page));
            return;
        }

        m_loadedRooms.insert(watchedRoom.data());
        auto *futureWatcher = new QFutureWatcher<ChatTimelinePage>{requestContext.data()};
        connect(futureWatcher, &QFutureWatcher<ChatTimelinePage>::finished, requestContext.data(),
                [this, promise, requestFinished, requestContext, futureWatcher] {
                    const auto page = futureWatcher->future().result();
                    futureWatcher->deleteLater();
                    finishInitialStateRequest(promise, requestFinished, requestContext, page);
                });
        futureWatcher->setFuture(requestTimelineForRoom(request, watchedRoom.data()));
    });
    connect(room, &QObject::destroyed, requestContextObject,
            [this, promise, requestFinished, requestContext] {
        ChatTimelinePage page;
        page.error = tr("Matrix room is no longer available.");
        finishInitialStateRequest(promise, requestFinished, requestContext, std::move(page));
    });
    connect(initialStateTimer, &QTimer::timeout, requestContextObject,
            [this, promise, requestFinished, requestContext] {
        ChatTimelinePage page;
        page.error = tr("Timed out while waiting for the Matrix room state.");
        finishInitialStateRequest(promise, requestFinished, requestContext, std::move(page));
    });
    initialStateTimer->start();
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

Chat MatrixTimelineService::chatForRoom(Quotient::Room *room) const
{
    if (!m_connection || !m_chatManager || !m_chatStorage || !room ||
        room->joinState() != Quotient::JoinState::Join)
        return Chat::null;

    return ChatTypeRoom::findChat(m_chatManager, m_chatStorage, account(), room->id(), ActionCreateAndAdd);
}

Quotient::Room *MatrixTimelineService::roomForChat(const Chat &chat) const
{
    if (!m_connection || !chat)
        return nullptr;

    if (const auto *details = qobject_cast<ChatDetailsRoom *>(chat.details()))
        return m_connection->room(details->room(), Quotient::JoinState::Join);
    return nullptr;
}

QUrl MatrixTimelineService::memberAvatarSource(Quotient::Room *room, const QString &memberId) const
{
    if (!room || memberId.isEmpty())
        return {};

    auto &roomCache = m_memberAvatarSources[room->id()];
    const auto cached = roomCache.constFind(memberId);
    if (cached != roomCache.cend())
        return cached.value();

    QUrl source;
    const auto avatar = room->memberAvatar(memberId, 48);
    if (!avatar.isNull())
    {
        QByteArray avatarData;
        QBuffer buffer{&avatarData};
        if (buffer.open(QIODevice::WriteOnly) && avatar.save(&buffer, "PNG"))
            source = QUrl{QStringLiteral("data:image/png;base64,") + QString::fromLatin1(avatarData.toBase64())};
    }
    roomCache.insert(memberId, source);
    return source;
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

void MatrixTimelineService::rememberReactionEvent(const QString &reactionEventId, const QString &targetEventId) const
{
    if (!reactionEventId.isEmpty() && !targetEventId.isEmpty())
        m_reactionEventTargets.insert(reactionEventId, targetEventId);
}

QString MatrixTimelineService::reactionTargetForEvent(const QString &eventId) const
{
    return m_reactionEventTargets.value(eventId);
}

void MatrixTimelineService::watchRoom(Quotient::Room *room)
{
    if (!room || m_watchedRooms.contains(room))
        return;

    m_watchedRooms.insert(room);
    connect(room, &Quotient::Room::addedMessages, this,
            [this, room](int fromIndex, int toIndex) { handleNewMessages(room, fromIndex, toIndex); });
    connect(room, &Quotient::Room::pendingEventAdded, this,
            [this, room](const Quotient::RoomEvent *event) { handlePendingEventAdded(room, event); });
    connect(room, &Quotient::Room::pendingEventChanged, this,
            [this, room](int pendingEventIndex) { updatePendingEvent(room, pendingEventIndex); });
    connect(room, &Quotient::Room::messageSent, this,
            [this](const QString &transactionId, const QString &eventId) {
                if (!transactionId.isEmpty() && !eventId.isEmpty())
                    m_eventTransactionIds.insert(eventId, transactionId);
            });
    connect(room, &Quotient::Room::pendingEventAboutToMerge, this,
            [this, room](Quotient::RoomEvent *serverEvent, int) {
                if (!serverEvent || serverEvent->id().isEmpty() || serverEvent->transactionId().isEmpty())
                    return;

                const auto eventId = serverEvent->id();
                m_eventTransactionIds.insert(eventId, serverEvent->transactionId());

                // Quotient merges an own remote echo without emitting
                // addedMessages(). Defer until it has placed that event in
                // the timeline, then let upsert() replace the local echo by
                // its server event ID through the transaction ID.
                const QPointer<Quotient::Room> watchedRoom{room};
                QTimer::singleShot(0, this, [this, watchedRoom, eventId] {
                    if (watchedRoom)
                        updateTimelineEvent(watchedRoom.data(), eventId);
                });
            });
    connect(room, &Quotient::Room::aboutToAddHistoricalMessages, this,
            [this](Quotient::RoomEventsRange events) {
                for (const auto &event : events)
                    if (event)
                        m_historicalEventIds.insert(event->id());
            });
    connect(room, &Quotient::Room::baseStateLoaded, this,
            [this, room] { m_loadedRooms.insert(room); });
    connect(room, &Quotient::Room::pinnedEventsChanged, this, [this, room] {
        const auto chat = chatForRoom(room);
        if (chat)
        {
            emit pinnedMessagesChanged(chat);
            emit availableActionsChanged(chat);
        }
    });
    connect(room, &Quotient::Room::changed, this, [this, room](Quotient::Room::Changes changes) {
        const auto chat = chatForRoom(room);
        if (!chat)
            return;

        // Change::Other also covers read-marker movement, so using it here
        // invalidates the action model for every visible timeline item while
        // the user reads the room. Permission-related state events are handled
        // in handleNewMessages() instead.
        const auto capabilitiesChanged = changes.testFlag(Quotient::Room::Change::JoinState);
        if (capabilitiesChanged)
        {
            emit availableActionsChanged(chat);
            emit roomInfoChanged(chat);
        }
        if (changes.testFlag(Quotient::Room::Change::RoomNames) ||
            changes.testFlag(Quotient::Room::Change::Topic) ||
            changes.testFlag(Quotient::Room::Change::Avatar) ||
            changes.testFlag(Quotient::Room::Change::Members) ||
            changes.testFlag(Quotient::Room::Change::Summary) || capabilitiesChanged)
            emit chatHeaderChanged(chat);
    });
    connect(room, &Quotient::Room::updatedEvent, this,
            [this, room](const QString &eventId) { updateTimelineEvent(room, eventId); });
    connect(room, &Quotient::Room::replacedEvent, this,
            [this, room](const Quotient::RoomEvent *newEvent, const Quotient::RoomEvent *oldEvent) {
                if (!newEvent)
                    return;

                const auto chat = chatForRoom(room);
                if (!chat)
                    return;
                if (newEvent->isRedacted())
                {
                    const auto reactionTarget = reactionTargetForEvent(newEvent->id());
                    if (!reactionTarget.isEmpty())
                    {
                        updateTimelineEvent(room, reactionTarget);
                        return;
                    }
                    if (const auto *reactionEvent = Quotient::eventCast<const Quotient::ReactionEvent>(oldEvent))
                    {
                        rememberReactionEvent(oldEvent->id(), reactionEvent->eventId());
                        updateTimelineEvent(room, reactionEvent->eventId());
                        return;
                    }
                    emit eventRedacted(chat, newEvent->id(), newEvent->redactionReason());
                    emit availableActionsChanged(chat);
                    if (oldEvent && room->pinnedEventIds().contains(oldEvent->id()))
                        emit pinnedMessagesChanged(chat);
                }
                else
                    updateTimelineEvent(room, newEvent->id());
            });
    connect(room, &Quotient::Room::memberAvatarUpdated, this, [this, room](const Quotient::RoomMember &member) {
        m_memberAvatarSources[room->id()].remove(member.id());
        updateTimelineEventsForMember(room, member.id());
    });
    connect(room, &QObject::destroyed, this, [this, room, roomId = room->id()] {
        m_memberAvatarSources.remove(roomId);
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
    if (fromIndex > toIndex)
        return;

    for (auto messageIndex = fromIndex; messageIndex <= toIndex; ++messageIndex)
    {
        const auto iterator = room->findInTimeline(messageIndex);
        if (iterator == room->historyEdge())
            continue;
        const auto &timelineItem = *iterator;
        if (m_historicalEventIds.remove(timelineItem->id()))
            continue;

        const auto reactionTarget = reactionTargetForEvent(timelineItem->id());
        if (timelineItem->isRedacted() && !reactionTarget.isEmpty())
        {
            updateTimelineEvent(room, reactionTarget);
            continue;
        }

        Quotient::RoomEventPtr decryptedEvent;
        auto encrypted = false;
        const auto *event = eventForTimelineItem(room, timelineItem, decryptedEvent, encrypted);
        if (!event)
            continue;

        const auto eventType = event->matrixType();
        const auto permissionsChanged = eventType == QStringLiteral("m.room.power_levels");
        const auto roomInfoChangedByEvent = permissionsChanged ||
                                            eventType == QStringLiteral("m.room.join_rules") ||
                                            eventType == QStringLiteral("m.room.history_visibility") ||
                                            eventType == QStringLiteral("m.room.guest_access") ||
                                            eventType == QStringLiteral("m.room.canonical_alias") ||
                                            eventType == QStringLiteral("m.room.create") ||
                                            eventType == QStringLiteral("m.room.encryption");
        if (permissionsChanged)
            emit availableActionsChanged(chat);
        if (roomInfoChangedByEvent)
            emit roomInfoChanged(chat);

        if (const auto *reactionEvent = Quotient::eventCast<const Quotient::ReactionEvent>(event))
        {
            rememberReactionEvent(timelineItem->id(), reactionEvent->eventId());
            updateTimelineEvent(room, reactionEvent->eventId());
            continue;
        }
        if (shouldHideEventFromTimeline(*event))
            continue;

        if (event->matrixType() == QStringLiteral("m.room.encrypted"))
        {
            if (const auto *encryptedEvent = timelineItem.viewAs<Quotient::EncryptedEvent>())
                m_sessionRecovery->requestFromBackup(room, *encryptedEvent);
        }

        if (event->isRedacted())
        {
            emit eventUpdated(
                chat, itemForEvent(room, *event, timelineItem->id(), timelineItem.index(), encrypted));
            emit availableActionsChanged(chat);
            if (room->pinnedEventIds().contains(timelineItem->id()))
                emit pinnedMessagesChanged(chat);
            continue;
        }

        const auto eventId = timelineItem->id();
        emit eventReceived(chat, itemForEvent(room, *event, eventId, timelineItem.index(), encrypted));
        if (room->pinnedEventIds().contains(eventId))
            emit pinnedMessagesChanged(chat);
        m_eventTransactionIds.remove(eventId);
    }
}

void MatrixTimelineService::handlePendingEventAdded(Quotient::Room *room, const Quotient::RoomEvent *event)
{
    if (!room || !event || event->transactionId().isEmpty())
        return;
    if (shouldHideEventFromTimeline(*event))
        return;

    const auto chat = chatForRoom(room);
    if (!chat || chat.isIgnoreAllMessages())
        return;

    auto item = itemForEvent(room, *event, localEchoId(event->transactionId()),
                             static_cast<qint64>(room->maxTimelineIndex()) + 1, false);
    item.transactionId = event->transactionId();
    item.timestamp = item.timestamp.isValid() ? item.timestamp : QDateTime::currentDateTime();
    item.state.deliveryState = ChatTimelineDeliveryState::Sending;
    emit eventReceived(chat, item);
}

void MatrixTimelineService::updatePendingEvent(Quotient::Room *room, int pendingEventIndex)
{
    if (!room)
        return;

    const auto &pendingEvents = room->pendingEvents();
    if (pendingEventIndex < 0 || pendingEventIndex >= static_cast<int>(pendingEvents.size()))
        return;

    const auto chat = chatForRoom(room);
    const auto &pendingEvent = pendingEvents.at(static_cast<Quotient::Room::PendingEvents::size_type>(pendingEventIndex));
    const auto *event = pendingEvent.event();
    if (!chat || !event || event->transactionId().isEmpty())
        return;
    if (shouldHideEventFromTimeline(*event))
        return;

    auto item = itemForEvent(room, *event, localEchoId(event->transactionId()),
                             static_cast<qint64>(room->maxTimelineIndex()) + pendingEventIndex + 1, false);
    item.transactionId = event->transactionId();
    item.timestamp = item.timestamp.isValid() ? item.timestamp : QDateTime::currentDateTime();
    switch (pendingEvent.deliveryStatus())
    {
    case Quotient::EventStatus::ReachedServer: item.state.deliveryState = ChatTimelineDeliveryState::Sent; break;
    case Quotient::EventStatus::SendingFailed:
        item.state.deliveryState = ChatTimelineDeliveryState::Failed;
        item.state.errorText = pendingEvent.annotation();
        break;
    default: item.state.deliveryState = ChatTimelineDeliveryState::Sending; break;
    }
    emit eventUpdated(chat, item);
}

ChatTimelinePage MatrixTimelineService::pageForRoom(const ChatTimelineRequest &request, Quotient::Room *room)
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
    auto inspected = false;
    auto accepted = 0;
    auto firstEvent = room->messageEvents().crbegin();
    if (validCursor && boundary <= room->maxTimelineIndex())
        firstEvent = boundary <= room->minTimelineIndex()
                         ? room->historyEdge()
                         : room->findInTimeline(static_cast<Quotient::TimelineItem::index_t>(boundary - 1));
    for (auto it = firstEvent; it != room->historyEdge(); ++it)
    {
        const auto index = static_cast<qint64>(it->index());
        inspected = true;
        nextCursor = index;

        if ((*it)->isRedacted() && !reactionTargetForEvent((*it)->id()).isEmpty())
            continue;

        Quotient::RoomEventPtr decryptedEvent;
        auto encrypted = false;
        const auto *event = eventForTimelineItem(room, *it, decryptedEvent, encrypted);
        if (!event)
            continue;

        if (shouldHideEventFromTimeline(*event))
            continue;

        if (event->matrixType() == QStringLiteral("m.room.encrypted"))
        {
            if (const auto *encryptedEvent = it->viewAs<Quotient::EncryptedEvent>())
                m_sessionRecovery->requestFromBackup(room, *encryptedEvent);
        }

        const auto eventId = (*it)->id();
        page.items.append(itemForEvent(room, *event, eventId, index, encrypted));
        ++accepted;
        if (accepted == limit)
            break;
    }

    if (inspected)
        page.olderCursor = QByteArray::number(nextCursor);
    else if (!room->messageEvents().empty() && !room->allHistoryLoaded())
        page.olderCursor = QByteArray::number(room->minTimelineIndex());
    page.hasOlder = accepted == limit || !room->allHistoryLoaded();
    return page;
}

ChatTimelineItem MatrixTimelineService::itemForDetachedEvent(Quotient::Room *room,
                                                              const Quotient::RoomEvent &remoteEvent)
{
    const auto eventId = remoteEvent.id();
    if (remoteEvent.isRedacted() && !reactionTargetForEvent(eventId).isEmpty())
        return {};
    const Quotient::RoomEvent *event = &remoteEvent;
    Quotient::RoomEventPtr decryptedEvent;
    auto encrypted = !remoteEvent.encryptedJson().isEmpty();
    if (const auto *encryptedEvent = Quotient::eventCast<const Quotient::EncryptedEvent>(&remoteEvent))
    {
        encrypted = true;
        decryptedEvent = room ? room->decryptMessage(*encryptedEvent) : nullptr;
        if (decryptedEvent)
        {
            m_decryptedEventSources.insert(eventId, decryptedEvent->fullJson());
            event = decryptedEvent.get();
        }
        else if (room)
            m_sessionRecovery->requestFromBackup(room, *encryptedEvent);
    }

    if (!event || shouldHideEventFromTimeline(*event))
        return {};
    auto item = itemForEvent(room, *event, eventId, 0, encrypted);
    item.sourceOrder = sourceOrderForEvent(remoteEvent, eventId, 0);
    if (!item.timestamp.isValid())
        item.timestamp = remoteEvent.originTimestamp().toLocalTime();
    return item;
}

ChatTimelineItem MatrixTimelineService::itemForEvent(Quotient::Room *room, const Quotient::RoomEvent &event,
                                                      const QString &eventId, qint64 timelineIndex, bool encrypted) const
{
    ChatTimelineItem item;
    item.stableId = eventId;
    item.transactionId = event.transactionId();
    if (item.transactionId.isEmpty())
        item.transactionId = m_eventTransactionIds.value(eventId);
    item.protocolEventType = event.matrixType();
    item.sourceOrder = sourceOrderForEvent(event, eventId, timelineIndex);
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
            item.sender.avatarSource = memberAvatarSource(room, member.id());
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

    const auto eventType = event.matrixType();
    const auto content = event.contentJson();
    if (event.isRedacted())
    {
        const auto stateEvent = Quotient::eventCast<const Quotient::StateEvent>(&event);
        item.kind = stateEvent
                        ? ChatTimelineItemKind::RoomSettingsChanged
                        : ChatTimelineItemKind::TextMessage;
        item.state.redacted = true;
        item.state.errorText = event.redactionReason();
        item.content.plainText = stateEvent ? tr("removed a room setting.") : tr("Message removed.");
        return item;
    }
    if (eventType == QStringLiteral("m.room.name"))
    {
        item.kind = ChatTimelineItemKind::RoomNameChanged;
        item.content.plainText = tr("changed the room name to %1.").arg(content.value("name").toString());
        return item;
    }
    if (eventType == QStringLiteral("m.room.topic"))
    {
        item.kind = ChatTimelineItemKind::TopicChanged;
        item.content.plainText = tr("changed the room topic to %1.").arg(content.value("topic").toString());
        return item;
    }
    if (eventType == QStringLiteral("m.room.avatar"))
    {
        item.kind = ChatTimelineItemKind::RoomAvatarChanged;
        item.content.plainText = tr("changed the room avatar.");
        return item;
    }
    if (eventType == QStringLiteral("m.room.create"))
    {
        item.kind = ChatTimelineItemKind::RoomCreated;
        item.content.plainText = tr("created the room.");
        return item;
    }
    if (eventType == QStringLiteral("m.room.encryption"))
    {
        item.kind = ChatTimelineItemKind::EncryptionEnabled;
        item.content.plainText = tr("enabled end-to-end encryption.");
        return item;
    }
    if (eventType == QStringLiteral("m.room.join_rules"))
    {
        item.kind = ChatTimelineItemKind::RoomSettingsChanged;
        const auto joinRule = content.value(QStringLiteral("join_rule")).toString();
        if (joinRule == QStringLiteral("public"))
            item.content.plainText = tr("made the room public.");
        else if (joinRule == QStringLiteral("invite"))
            item.content.plainText = tr("made the room invite-only.");
        else if (joinRule == QStringLiteral("knock"))
            item.content.plainText = tr("allowed users to request an invitation to the room.");
        else if (joinRule == QStringLiteral("restricted"))
            item.content.plainText = tr("restricted access to members of selected rooms.");
        else if (joinRule == QStringLiteral("knock_restricted"))
            item.content.plainText = tr("restricted access and allowed users to request an invitation.");
        else if (joinRule == QStringLiteral("private"))
            item.content.plainText = tr("made the room private.");
        else
            item.content.plainText = tr("changed the room access rules.");
        return item;
    }
    if (eventType == QStringLiteral("m.room.guest_access"))
    {
        item.kind = ChatTimelineItemKind::RoomSettingsChanged;
        const auto guestAccess = content.value(QStringLiteral("guest_access")).toString();
        if (guestAccess == QStringLiteral("can_join"))
            item.content.plainText = tr("allowed guests to join the room.");
        else if (guestAccess == QStringLiteral("forbidden"))
            item.content.plainText = tr("prevented guests from joining the room.");
        else
            item.content.plainText = tr("changed guest access to the room.");
        return item;
    }
    if (eventType == QStringLiteral("m.room.history_visibility"))
    {
        item.kind = ChatTimelineItemKind::RoomSettingsChanged;
        const auto historyVisibility = content.value(QStringLiteral("history_visibility")).toString();
        if (historyVisibility == QStringLiteral("world_readable"))
            item.content.plainText = tr("made the room history visible to anyone.");
        else if (historyVisibility == QStringLiteral("shared"))
            item.content.plainText = tr("made the full room history visible to members.");
        else if (historyVisibility == QStringLiteral("invited"))
            item.content.plainText = tr("made room history visible from the time a user is invited.");
        else if (historyVisibility == QStringLiteral("joined"))
            item.content.plainText = tr("made room history visible from the time a user joins.");
        else
            item.content.plainText = tr("changed the room history visibility.");
        return item;
    }
    if (eventType == QStringLiteral("m.room.canonical_alias"))
    {
        item.kind = ChatTimelineItemKind::RoomSettingsChanged;
        const auto alias = content.value(QStringLiteral("alias")).toString();
        const auto previousAlias = event.unsignedJson()
                                       .value(QStringLiteral("prev_content"))
                                       .toObject()
                                       .value(QStringLiteral("alias"))
                                       .toString();
        if (!alias.isEmpty())
            item.content.plainText = tr("set the main room address to %1.").arg(alias);
        else if (!previousAlias.isEmpty())
            item.content.plainText = tr("removed the main room address.");
        else
            item.content.plainText = tr("changed the room addresses.");
        return item;
    }
    if (eventType == QStringLiteral("m.room.aliases"))
    {
        item.kind = ChatTimelineItemKind::RoomSettingsChanged;
        item.content.plainText = tr("changed the room addresses.");
        return item;
    }
    if (eventType == QStringLiteral("m.room.power_levels"))
    {
        item.kind = ChatTimelineItemKind::RoomSettingsChanged;
        item.content.plainText = tr("changed the room permissions.");
        return item;
    }
    if (eventType == QStringLiteral("m.room.pinned_events"))
    {
        item.kind = ChatTimelineItemKind::RoomSettingsChanged;
        const auto pinned = content.value(QStringLiteral("pinned")).toArray();
        const auto previousPinned = event.unsignedJson()
                                        .value(QStringLiteral("prev_content"))
                                        .toObject()
                                        .value(QStringLiteral("pinned"))
                                        .toArray();
        QSet<QString> pinnedIds;
        QSet<QString> previousPinnedIds;
        for (const auto &id : pinned)
            pinnedIds.insert(id.toString());
        for (const auto &id : previousPinned)
            previousPinnedIds.insert(id.toString());
        const auto added = pinnedIds - previousPinnedIds;
        const auto removed = previousPinnedIds - pinnedIds;
        if (added.size() == 1 && removed.isEmpty())
            item.content.plainText = tr("pinned a message.");
        else if (removed.size() == 1 && added.isEmpty())
            item.content.plainText = tr("unpinned a message.");
        else
            item.content.plainText = tr("changed the pinned messages.");
        return item;
    }
    if (eventType == QStringLiteral("m.room.tombstone"))
    {
        item.kind = ChatTimelineItemKind::RoomSettingsChanged;
        item.content.plainText = tr("upgraded the room.");
        return item;
    }
    if (eventType == QStringLiteral("m.room.server_acl"))
    {
        item.kind = ChatTimelineItemKind::RoomSettingsChanged;
        item.content.plainText = tr("changed which servers can participate in the room.");
        return item;
    }
    if (eventType == QStringLiteral("m.room.retention"))
    {
        item.kind = ChatTimelineItemKind::RoomSettingsChanged;
        item.content.plainText = tr("changed the room message retention policy.");
        return item;
    }
    if (eventType == QStringLiteral("m.room.third_party_invite"))
    {
        item.kind = ChatTimelineItemKind::RoomSettingsChanged;
        item.content.plainText = content.isEmpty() ? tr("revoked a third-party invitation.")
                                                   : tr("created a third-party invitation.");
        return item;
    }
    if (const auto *memberEvent = Quotient::eventCast<const Quotient::RoomMemberEvent>(&event))
    {
        const auto memberId = memberEvent->userId();
        const auto memberName = memberEvent->newDisplayName().value_or(memberId);
        if (memberEvent->isRename() || memberEvent->isAvatarUpdate())
        {
            item.kind = ChatTimelineItemKind::MemberProfileChanged;
            item.content.plainText = tr("updated their room profile.");
        }
        else if (memberEvent->isJoin())
        {
            item.kind = ChatTimelineItemKind::MemberJoined;
            item.content.plainText = tr("joined the room.");
        }
        else if (memberEvent->isInvite())
        {
            item.kind = ChatTimelineItemKind::MemberInvited;
            item.content.plainText = tr("invited %1 to the room.").arg(memberName);
        }
        else if (memberEvent->isBan())
        {
            item.kind = ChatTimelineItemKind::MemberBanned;
            item.content.plainText = tr("banned %1 from the room.").arg(memberName);
        }
        else if (memberEvent->isLeave() && event.senderId() != memberId)
        {
            item.kind = ChatTimelineItemKind::MemberKicked;
            item.content.plainText = tr("removed %1 from the room.").arg(memberName);
        }
        else if (memberEvent->isLeave() || memberEvent->isRejectedInvite())
        {
            item.kind = ChatTimelineItemKind::MemberLeft;
            item.content.plainText = tr("left the room.");
        }
        else
        {
            item.kind = ChatTimelineItemKind::MemberProfileChanged;
            item.content.plainText = tr("updated their room profile.");
        }
        return item;
    }
    if (const auto *redactionEvent = Quotient::eventCast<const Quotient::RedactionEvent>(&event))
    {
        item.kind = ChatTimelineItemKind::MessageRedacted;
        item.content.plainText = tr("removed a message.");
        item.content.replyToId = redactionEvent->redactedEvent();
        return item;
    }
    if (eventType.startsWith(QStringLiteral("m.call.")))
    {
        item.kind = ChatTimelineItemKind::CallEvent;
        item.content.plainText = tr("sent a call event (%1).").arg(eventType);
        return item;
    }
    if (eventType == QStringLiteral("m.room.encrypted"))
    {
        item.kind = ChatTimelineItemKind::EncryptedEvent;
        item.content.plainText = tr("Encrypted Matrix event is waiting for a key (%1).").arg(eventType);
        item.state.errorText = tr("The event could not be decrypted yet.");
        appendReactions(item, room, event);
        return item;
    }

    if (const auto *stickerEvent = Quotient::eventCast<const Quotient::StickerEvent>(&event))
    {
        item.kind = ChatTimelineItemKind::ImageMessage;
        item.content.plainText = stickerEvent->body();
        appendReactions(item, room, event);

        const auto &imageContent = stickerEvent->image();
        const auto fileInfo = imageContent.commonInfo();
        ChatTimelineAttachment attachment;
        attachment.kind = ChatTimelineAttachmentKind::Image;
        attachment.fileName = fileInfo.originalName.isEmpty() ? stickerEvent->body() : fileInfo.originalName;
        if (attachment.fileName.isEmpty())
            attachment.fileName = tr("Sticker");
        attachment.mimeType = fileInfo.mimeType.name();
        attachment.size = fileInfo.payloadSize;
        attachment.dimensions = imageContent.imageSize;
        attachment.sourceUri = attachmentUri(eventId);
        attachment.thumbnailUri = attachmentUri(eventId, true);
        attachment.localResourceId = eventId;
        m_attachmentSources.insert(eventId, fileInfo.source);
        m_attachmentFileNames.insert(eventId, attachment.fileName);
        m_attachmentKinds.insert(eventId, attachment.kind);
        if (!m_unavailableAttachmentThumbnails.contains(eventId) && imageContent.thumbnail.isValid())
        {
            m_attachmentSources.insert(attachmentResourceId(eventId, true), imageContent.thumbnail.source);
            m_attachmentPreviewsUsingOriginal.remove(eventId);
        }
        else if (!m_unavailableAttachmentThumbnails.contains(eventId) &&
                 std::holds_alternative<QUrl>(fileInfo.source))
        {
            m_attachmentSources.remove(attachmentResourceId(eventId, true));
            m_attachmentPreviewsUsingOriginal.remove(eventId);
        }
        else
        {
            m_attachmentSources.insert(attachmentResourceId(eventId, true), fileInfo.source);
            m_attachmentPreviewsUsingOriginal.insert(eventId);
        }
        if (attachment.dimensions.isEmpty() && !m_attachmentImageDimensions.value(eventId).isEmpty())
            attachment.dimensions = m_attachmentImageDimensions.value(eventId);
        const auto previewResourceId = attachmentResourceId(eventId, true);
        attachment.state = m_attachmentStates.value(previewResourceId, ChatTimelineAttachmentState::NotRequested);
        attachment.progress = m_attachmentProgress.value(previewResourceId, 0.0);
        attachment.errorText = m_attachmentErrors.value(previewResourceId);
        item.content.attachments.append(std::move(attachment));
        return item;
    }

    const auto *messageEvent = Quotient::eventCast<const Quotient::RoomMessageEvent>(&event);
    if (!messageEvent)
    {
        if (Quotient::eventCast<const Quotient::StateEvent>(&event))
        {
            item.kind = ChatTimelineItemKind::RoomSettingsChanged;
            item.content.plainText = tr("changed a room setting (%1).").arg(eventType);
            return item;
        }
        item.kind = ChatTimelineItemKind::UnsupportedEvent;
        item.content.plainText = tr("Unsupported Matrix event: %1").arg(eventType);
        return item;
    }
    if (!messageEvent->replacedEvent().isEmpty())
    {
        item.kind = ChatTimelineItemKind::MessageEdited;
        item.content.plainText = tr("edited a message.");
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
    case Quotient::RoomMessageEvent::MsgType::Location:
        item.kind = ChatTimelineItemKind::LocationMessage;
        if (const auto locationContent = messageEvent->get<Quotient::EventContent::LocationContent>())
            item.content.locationUri = locationContent->geoUri;
        break;
    default:
        item.kind = ChatTimelineItemKind::UnsupportedEvent;
        item.content.plainText = tr("Unsupported Matrix message type: %1").arg(messageEvent->rawMsgtype());
        return item;
    }

    appendReactions(item, room, event);

    if (const auto fileContent = messageEvent->get<Quotient::EventContent::FileContentBase>())
    {
        const auto fileInfo = fileContent->commonInfo();
        ChatTimelineAttachment attachment;
        attachment.fileName = fileInfo.originalName.isEmpty() ? messageEvent->fileNameToDownload() : fileInfo.originalName;
        attachment.mimeType = fileInfo.mimeType.name();
        attachment.size = fileInfo.payloadSize;
        attachment.sourceUri = attachmentUri(eventId);
        attachment.localResourceId = eventId;
        m_attachmentSources.insert(eventId, fileInfo.source);
        m_attachmentFileNames.insert(eventId, attachment.fileName);

        switch (messageEvent->msgtype())
        {
        case Quotient::RoomMessageEvent::MsgType::Image:
        {
            const auto explicitNonImageMime = !attachment.mimeType.isEmpty() &&
                                              attachment.mimeType != QStringLiteral("application/octet-stream") &&
                                              !attachment.mimeType.startsWith(QStringLiteral("image/"));
            if (explicitNonImageMime || m_invalidImageAttachments.contains(eventId))
            {
                attachment.kind = ChatTimelineAttachmentKind::File;
                item.kind = ChatTimelineItemKind::FileMessage;
                break;
            }

            attachment.kind = ChatTimelineAttachmentKind::Image;
            attachment.thumbnailUri = attachmentUri(eventId, true);
            if (!m_unavailableAttachmentThumbnails.contains(eventId) && fileContent->thumbnail.isValid())
            {
                m_attachmentSources.insert(attachmentResourceId(eventId, true), fileContent->thumbnail.source);
                m_attachmentPreviewsUsingOriginal.remove(eventId);
            }
            else if (!m_unavailableAttachmentThumbnails.contains(eventId) &&
                     std::holds_alternative<QUrl>(fileInfo.source))
            {
                // With no explicit thumbnail, leave the preview source empty so
                // requestAttachmentImage() asks the homeserver thumbnail API.
                m_attachmentSources.remove(attachmentResourceId(eventId, true));
                m_attachmentPreviewsUsingOriginal.remove(eventId);
            }
            else
            {
                // Encrypted media without thumbnail, or a thumbnail rejected by
                // the server/decoder, still uses a distinct preview cache entry.
                m_attachmentSources.insert(attachmentResourceId(eventId, true), fileInfo.source);
                m_attachmentPreviewsUsingOriginal.insert(eventId);
            }
            if (const auto imageContent = messageEvent->get<Quotient::EventContent::ImageContent>())
                attachment.dimensions = imageContent->imageSize;
            if (attachment.dimensions.isEmpty() && !m_attachmentImageDimensions.value(eventId).isEmpty())
                attachment.dimensions = m_attachmentImageDimensions.value(eventId);
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
        m_attachmentKinds.insert(eventId, attachment.kind);
        const auto previewResourceId = attachmentResourceId(eventId, !attachment.thumbnailUri.isEmpty());
        attachment.state = m_attachmentStates.value(previewResourceId, ChatTimelineAttachmentState::NotRequested);
        attachment.progress = m_attachmentProgress.value(previewResourceId, 0.0);
        attachment.errorText = m_attachmentErrors.value(previewResourceId);
        item.content.attachments.append(std::move(attachment));
    }
    return item;
}

void MatrixTimelineService::updateTimelineEvent(Quotient::Room *room, const QString &eventId)
{
    const auto chat = chatForRoom(room);
    if (!chat || !room || eventId.isEmpty())
        return;

    const auto iterator = room->findInTimeline(eventId);
    if (iterator == room->historyEdge())
        return;
    const auto &timelineItem = *iterator;

    const auto reactionTarget = reactionTargetForEvent(eventId);
    if (timelineItem->isRedacted() && !reactionTarget.isEmpty())
    {
        updateTimelineEvent(room, reactionTarget);
        return;
    }

    Quotient::RoomEventPtr decryptedEvent;
    auto encrypted = false;
    const auto *event = eventForTimelineItem(room, timelineItem, decryptedEvent, encrypted);
    if (!event)
        return;
    if (const auto *reactionEvent = Quotient::eventCast<const Quotient::ReactionEvent>(event))
    {
        rememberReactionEvent(eventId, reactionEvent->eventId());
        updateTimelineEvent(room, reactionEvent->eventId());
        return;
    }
    if (shouldHideEventFromTimeline(*event))
        return;
    if (event->matrixType() == QStringLiteral("m.room.encrypted"))
    {
        if (const auto *encryptedEvent = timelineItem.viewAs<Quotient::EncryptedEvent>())
            m_sessionRecovery->requestFromBackup(room, *encryptedEvent);
    }
    if (event->isRedacted())
    {
        emit eventRedacted(chat, eventId, event->redactionReason());
        emit availableActionsChanged(chat);
        if (room->pinnedEventIds().contains(eventId))
            emit pinnedMessagesChanged(chat);
        return;
    }

    emit eventUpdated(chat, itemForEvent(room, *event, eventId, timelineItem.index(), encrypted));
    if (room->pinnedEventIds().contains(eventId))
        emit pinnedMessagesChanged(chat);
    m_eventTransactionIds.remove(eventId);
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

void MatrixTimelineService::updateTimelineEventsForMegolmSession(Quotient::Room *room, const QString &sessionId)
{
    if (!room || sessionId.isEmpty())
        return;

    for (const auto &timelineItem : room->messageEvents())
        if (const auto *encryptedEvent = timelineItem.viewAs<Quotient::EncryptedEvent>();
            encryptedEvent && encryptedEvent->sessionId() == sessionId)
            updateTimelineEvent(room, timelineItem->id());
}

void MatrixTimelineService::appendReactions(ChatTimelineItem &item, Quotient::Room *room,
                                            const Quotient::RoomEvent &event) const
{
    if (!room)
        return;

    for (const auto *relatedEvent : room->relatedEvents(event, Quotient::EventRelation::AnnotationType))
    {
        const auto *reactionEvent = Quotient::eventCast<const Quotient::ReactionEvent>(relatedEvent);
        if (!reactionEvent || relatedEvent->isRedacted())
            continue;

        rememberReactionEvent(relatedEvent->id(), reactionEvent->eventId());
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

void MatrixTimelineService::refreshEncryptedEvents()
{
    for (auto *room : std::as_const(m_watchedRooms))
    {
        if (!room)
            continue;

        for (const auto &timelineItem : room->messageEvents())
            if (timelineItem.viewAs<Quotient::EncryptedEvent>())
                updateTimelineEvent(room, timelineItem->id());
    }
}

void MatrixTimelineService::showEventSource(const QString &eventId, const Quotient::RoomEvent &event) const
{
    auto *dialog = new QDialog{};
    dialog->setWindowTitle(tr("Matrix event source"));
    dialog->resize(720, 520);

    auto *layout = new QVBoxLayout{dialog};
    auto *tabs = new QTabWidget{dialog};
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

    auto *buttons = new QDialogButtonBox{QDialogButtonBox::Close, dialog};
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    connect(dialog, &QDialog::finished, dialog, &QObject::deleteLater);
    layout->addWidget(buttons);
    dialog->open();
}

void MatrixTimelineService::updateAttachmentEvent(Quotient::Room *room, const QString &eventId)
{
    updateTimelineEvent(room, eventId);
}

void MatrixTimelineService::handleAttachmentDownloadProgress(Quotient::Room *room, const QString &resourceId,
                                                             const QString &eventId, qint64 received, qint64 total)
{
    if (!m_attachmentDownloadPaths.contains(resourceId))
        return;

    m_attachmentProgress.insert(resourceId, total > 0 ? qreal(received) / qreal(total) : 0.0);
    updateAttachmentEvent(room, eventId);
}

void MatrixTimelineService::handleAttachmentDownloadCompleted(Quotient::Room *room, const QString &resourceId,
                                                              const QString &eventId, const QUrl &localFile)
{
    if (!m_attachmentDownloadPaths.contains(resourceId))
        return;

    const auto temporaryPath = m_attachmentDownloadPaths.take(resourceId);
    const auto localPath = localFile.toLocalFile().isEmpty() ? temporaryPath : localFile.toLocalFile();
    const auto requestedSize = m_attachmentRequestedSizes.take(resourceId);
    m_attachmentImageDecodes.insert(resourceId);
    const QPointer<Quotient::Room> downloadRoom{room};
    const QPointer<Quotient::Connection> downloadConnection{m_connection};
    auto *watcher = new QFutureWatcher<DecodedAttachmentImage>{this};
    connect(watcher, &QFutureWatcher<DecodedAttachmentImage>::finished, this,
            [this, watcher, downloadRoom, downloadConnection, resourceId, eventId] {
        const auto decoded = watcher->future().result();
        watcher->deleteLater();
        if (downloadConnection != m_connection)
            return;
        m_attachmentImageDecodes.remove(resourceId);
        if (!downloadRoom)
            return;

        if (decoded.image.isNull())
        {
            if (resourceId == attachmentResourceId(eventId, true))
            {
                if (m_attachmentPreviewsUsingOriginal.contains(eventId))
                    m_invalidImageAttachments.insert(eventId);
                else
                    m_unavailableAttachmentThumbnails.insert(eventId);
                m_attachmentStates.remove(resourceId);
                m_attachmentProgress.remove(resourceId);
                m_attachmentErrors.remove(resourceId);
            }
            else
            {
                m_attachmentStates.insert(resourceId, ChatTimelineAttachmentState::Failed);
                m_attachmentErrors.insert(resourceId, tr("The downloaded attachment is not a valid image."));
            }
        }
        else
        {
            m_attachmentImages.insert(resourceId, new QImage{decoded.image},
                                      attachmentImageCacheCost(decoded.image));
            m_attachmentImageDimensions.insert(
                eventId, decoded.originalSize.isValid() ? decoded.originalSize : decoded.image.size());
            m_attachmentStates.insert(resourceId, ChatTimelineAttachmentState::Available);
            m_attachmentProgress.insert(resourceId, 1.0);
            m_attachmentErrors.remove(resourceId);
        }
        updateAttachmentEvent(downloadRoom.data(), eventId);
    });
    watcher->setFuture(QtConcurrent::run(
        &MatrixTimelineService::decodeAttachmentImage, localPath, temporaryPath, requestedSize));
}

MatrixTimelineService::DecodedAttachmentImage MatrixTimelineService::decodeAttachmentImage(
    const QString &localPath, const QString &temporaryPath, const QSize &requestedSize)
{
    QImageReader imageReader{localPath};
    imageReader.setAutoTransform(true);
    DecodedAttachmentImage result;
    result.originalSize = imageReader.size();
    if (requestedSize.isValid() && result.originalSize.isValid())
        imageReader.setScaledSize(result.originalSize.scaled(requestedSize, Qt::KeepAspectRatio));
    result.image = imageReader.read();

    QFile::remove(temporaryPath);
    if (localPath != temporaryPath)
        QFile::remove(localPath);
    return result;
}

int MatrixTimelineService::attachmentImageCacheCost(const QImage &image)
{
    const auto sizeKiB = (image.sizeInBytes() + 1023) / 1024;
    return static_cast<int>(qBound(
        qint64{1}, static_cast<qint64>(sizeKiB), static_cast<qint64>(AttachmentImageCacheSizeKiB)));
}

void MatrixTimelineService::handleAttachmentDownloadFailed(Quotient::Room *room, const QString &resourceId,
                                                           const QString &eventId, const QString &errorMessage)
{
    const auto temporaryPath = m_attachmentDownloadPaths.take(resourceId);
    m_attachmentRequestedSizes.remove(resourceId);
    if (temporaryPath.isEmpty())
        return;

    QFile::remove(temporaryPath);
    if (resourceId == attachmentResourceId(eventId, true) &&
        !m_attachmentPreviewsUsingOriginal.contains(eventId))
    {
        m_unavailableAttachmentThumbnails.insert(eventId);
        m_attachmentStates.remove(resourceId);
        m_attachmentProgress.remove(resourceId);
        m_attachmentErrors.remove(resourceId);
        updateAttachmentEvent(room, eventId);
        return;
    }

    m_attachmentStates.insert(resourceId, ChatTimelineAttachmentState::Failed);
    m_attachmentProgress.remove(resourceId);
    m_attachmentErrors.insert(resourceId, errorMessage.isEmpty() ? tr("Could not download the image.") : errorMessage);
    updateAttachmentEvent(room, eventId);
}

void MatrixTimelineService::clearAttachmentDownloads()
{
    for (const auto &temporaryPath : std::as_const(m_attachmentDownloadPaths))
        QFile::remove(temporaryPath);
    m_attachmentDownloadPaths.clear();
    m_attachmentThumbnailRequests.clear();
    m_attachmentRequestedSizes.clear();
    m_attachmentImageDecodes.clear();
}

bool MatrixTimelineService::shouldHideEventFromTimeline(const Quotient::RoomEvent &event)
{
    if (Quotient::eventCast<const Quotient::RedactionEvent>(&event))
        return true;
    if (Quotient::eventCast<const Quotient::ReactionEvent>(&event))
        return true;
    if (const auto *stateEvent = Quotient::eventCast<const Quotient::StateEvent>(&event);
        stateEvent && stateEvent->repeatsState())
        return true;
    if (event.matrixType().startsWith(QStringLiteral("m.key.verification.")))
        return true;
    if (const auto *messageEvent = Quotient::eventCast<const Quotient::RoomMessageEvent>(&event);
        messageEvent && messageEvent->rawMsgtype() == QStringLiteral("m.key.verification.request"))
        return true;

    const auto eventType = event.matrixType();
    return eventType.startsWith(QStringLiteral("m.space."));
}

QUrl MatrixTimelineService::attachmentUri(const QString &eventId, bool thumbnail)
{
    QUrl uri;
    uri.setScheme(QStringLiteral("kaduimg"));
    uri.setPath(QStringLiteral("/") + eventId);
    if (thumbnail)
    {
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("thumbnail"), QStringLiteral("1"));
        uri.setQuery(query);
    }
    return uri;
}

QString MatrixTimelineService::eventIdForAttachmentUri(const QUrl &sourceUri)
{
    if (sourceUri.scheme() != QStringLiteral("kaduimg"))
        return {};
    return sourceUri.path(QUrl::FullyDecoded).mid(1);
}

bool MatrixTimelineService::isAttachmentThumbnailUri(const QUrl &sourceUri)
{
    return QUrlQuery{sourceUri}.queryItemValue(QStringLiteral("thumbnail")) == QStringLiteral("1");
}

QString MatrixTimelineService::attachmentResourceId(const QString &eventId, bool thumbnail)
{
    return thumbnail ? eventId + QStringLiteral("/thumbnail") : eventId;
}

QString MatrixTimelineService::localEchoId(const QString &transactionId)
{
    return QStringLiteral("matrix:pending:") + transactionId;
}

QString MatrixTimelineService::transactionIdForLocalEcho(const QString &stableId)
{
    static const auto prefix = QStringLiteral("matrix:pending:");
    return stableId.startsWith(prefix) ? stableId.sliced(prefix.size()) : QString{};
}

QString MatrixTimelineService::joinRuleName(const QJsonObject &joinRules)
{
    const auto joinRule = joinRules.value(QStringLiteral("join_rule")).toString();
    if (joinRule == QStringLiteral("knock") || joinRule == QStringLiteral("invite") ||
        joinRule == QStringLiteral("private") || joinRule == QStringLiteral("restricted") ||
        joinRule == QStringLiteral("knock_restricted"))
        return joinRule;
    return QStringLiteral("public");
}

QByteArray MatrixTimelineService::sourceOrderForEvent(const Quotient::RoomEvent &event, const QString &eventId,
                                                       qint64 timelineIndex) const
{
    if (event.originTimestamp().isValid())
        return event.originTimestamp().toUTC().toString(Qt::ISODateWithMs).toUtf8() + ':' + eventId.toUtf8();

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

void MatrixTimelineService::finishInitialStateRequest(
    const std::shared_ptr<QPromise<ChatTimelinePage>> &promise, const std::shared_ptr<bool> &finished,
    const QPointer<QObject> &context, ChatTimelinePage page) const
{
    if (*finished)
        return;

    *finished = true;
    finishRequest(promise, std::move(page));
    if (context)
        context->deleteLater();
}
