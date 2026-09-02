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

#include "chat/timeline/chat-timeline-item.h"
#include "chat/timeline/chat-timeline-page.h"
#include "exports.h"

#include <QtCore/QAbstractListModel>
#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>

class KADUAPI ChatTimelineModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int groupingIntervalSeconds READ groupingIntervalSeconds WRITE setGroupingIntervalSeconds NOTIFY groupingIntervalChanged)

public:
    enum Role
    {
        StableIdRole = Qt::UserRole + 1,
        TransactionIdRole,
        ProtocolEventTypeRole,
        KindRole,
        TimestampRole,
        DateRole,
        OwnEventRole,
        SenderIdRole,
        SenderDisplayNameRole,
        SenderAvatarSourceRole,
        SenderColorRole,
        PlainTextRole,
        FormattedTextRole,
        ReplyToIdRole,
        ReplyRole,
        AttachmentsRole,
        LocationUriRole,
        ReactionsRole,
        DeliveryStateRole,
        EditedRole,
        RedactedRole,
        EncryptedRole,
        DecryptionStateRole,
        ErrorTextRole,
        SystemEventRole,
        EmoteRole,
        GroupPositionRole,
        ShowSenderRole,
        ShowAvatarRole,
        ShowTimestampRole,
        StartsNewDayRole
    };
    Q_ENUM(Role)

    enum class GroupPosition
    {
        Single,
        First,
        Middle,
        Last
    };
    Q_ENUM(GroupPosition)

    explicit ChatTimelineModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QVector<ChatTimelineItem> items() const;
    ChatTimelineItem item(const QString &stableId) const;
    /**
     * Convert a timeline entry to the QML-facing data contract without exposing
     * protocol objects. Auxiliary views, such as pinned messages, use this to
     * render the same entry component as the main timeline.
     */
    static QVariantMap itemData(const ChatTimelineItem &item, bool showSender = true, bool showAvatar = true,
                                bool showTimestamp = true, bool startsNewDay = false);
    Q_INVOKABLE int rowForStableId(const QString &stableId) const;
    Q_INVOKABLE int rowForTransactionId(const QString &transactionId) const;
    int groupingIntervalSeconds() const;
    void setGroupingIntervalSeconds(int seconds);

    void reset(const QVector<ChatTimelineItem> &items);
    void prepend(const ChatTimelinePage &page);
    void append(const ChatTimelinePage &page);
    void upsert(const ChatTimelineItem &item);
    void replaceLocalEcho(const QString &transactionId, const ChatTimelineItem &serverItem);
    void update(const QString &stableId, const ChatTimelineItem &item);
    void redact(const QString &stableId, const QString &reason = QString());
    void remove(const QString &stableId);
    void removeFirst(int count);
    void removeLast(int count);
    void clear();

signals:
    void groupingIntervalChanged();

private:
    QVector<ChatTimelineItem> m_items;
    QHash<QString, int> m_rowsByStableId;
    QHash<QString, int> m_rowsByTransactionId;
    int m_groupingIntervalSeconds = 5 * 60;

    static bool comesBefore(const ChatTimelineItem &left, const ChatTimelineItem &right);
    static bool isMessage(const ChatTimelineItem &item);
    static const QList<int> &itemDataRoles();
    static const QList<int> &presentationRoles();
    static QVariantList attachmentData(const QVector<ChatTimelineAttachment> &attachments);
    static QVariantList reactionData(const QVector<ChatTimelineReaction> &reactions);
    QVariantMap replyData(const ChatTimelineItem &item) const;
    int insertionRow(const ChatTimelineItem &item) const;
    GroupPosition groupPositionAt(int row) const;
    void rebuildRows();
    void emitGroupingChangedAround(int row);
    void emitReplyChangedFor(const QString &stableId);
    void insertItem(const ChatTimelineItem &item);
    void replaceItem(int row, const ChatTimelineItem &item);
};
