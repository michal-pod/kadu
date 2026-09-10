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

#include "exports.h"

#include <QtCore/QByteArray>
#include <QtCore/QDateTime>
#include <QtCore/QMetaType>
#include <QtCore/QSize>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QUrl>
#include <QtCore/QVector>
#include <QtGui/QColor>

enum class ChatTimelineItemKind
{
    TextMessage,
    NoticeMessage,
    EmoteMessage,
    ImageMessage,
    FileMessage,
    AudioMessage,
    VideoMessage,
    TopicChanged,
    RoomNameChanged,
    RoomAvatarChanged,
    MemberJoined,
    MemberLeft,
    MemberInvited,
    MemberKicked,
    MemberBanned,
    MemberProfileChanged,
    RoomCreated,
    EncryptionEnabled,
    CallEvent,
    ReactionAdded,
    MessageEdited,
    MessageRedacted,
    LocalNotice,
    ErrorNotice,
    UnsupportedEvent,
    EncryptedEvent,
    LocationMessage,
    RoomSettingsChanged
};

enum class ChatTimelineItemLevel
{
    Chat = 0,
    Important = 100,
    Informational = 200,
    Debug = 300
};

enum class ChatTimelineDeliveryState
{
    Unknown,
    Sending,
    Sent,
    Delivered,
    Failed
};

enum class ChatTimelineDecryptionState
{
    NotEncrypted = 0,
    Pending = 1,
    Decrypted = 2,
    Failed = 3,
    MissingKey = 4
};

enum class ChatTimelineAttachmentKind
{
    Image,
    File,
    Audio,
    Video
};

enum class ChatTimelineAttachmentState
{
    NotRequested,
    Downloading,
    Available,
    Failed
};

struct KADUAPI ChatTimelineSender
{
    QString id;
    QString displayName;
    QUrl avatarSource;
    QColor color;
    bool own = false;
};

struct KADUAPI ChatTimelineReaction
{
    QString key;
    QStringList senderIds;
    QStringList senderDisplayNames;
    bool own = false;
};

struct KADUAPI ChatTimelineAttachment
{
    ChatTimelineAttachmentKind kind = ChatTimelineAttachmentKind::File;
    QString fileName;
    QString mimeType;
    qint64 size = 0;
    QSize dimensions;
    qint64 duration = 0;
    QUrl sourceUri;
    QUrl thumbnailUri;
    QString encryptedFileMetadata;
    // The timeline preview and the original resource can be loaded independently.
    ChatTimelineAttachmentState state = ChatTimelineAttachmentState::NotRequested;
    ChatTimelineAttachmentState sourceState = ChatTimelineAttachmentState::NotRequested;
    qreal progress = 0.0;
    QString localResourceId;
    QString errorText;
};

struct KADUAPI ChatTimelineContent
{
    QString plainText;
    QString formattedText;
    QString replyToId;
    QVector<ChatTimelineReaction> reactions;
    QVector<ChatTimelineAttachment> attachments;
    QString locationUri;
};

struct KADUAPI ChatTimelineState
{
    ChatTimelineDeliveryState deliveryState = ChatTimelineDeliveryState::Unknown;
    bool edited = false;
    bool redacted = false;
    bool encrypted = false;
    ChatTimelineDecryptionState decryptionState = ChatTimelineDecryptionState::NotEncrypted;
    QString errorText;
};

struct KADUAPI ChatTimelineItem
{
    QString stableId;
    QString transactionId;
    QString protocolEventType;
    QByteArray sourceOrder;
    QDateTime timestamp;
    quint64 revision = 0;
    ChatTimelineItemKind kind = ChatTimelineItemKind::TextMessage;
    ChatTimelineItemLevel level = ChatTimelineItemLevel::Chat;
    ChatTimelineSender sender;
    ChatTimelineContent content;
    ChatTimelineState state;
};

Q_DECLARE_METATYPE(ChatTimelineItem)
