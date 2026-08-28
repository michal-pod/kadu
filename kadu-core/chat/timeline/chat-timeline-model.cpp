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

#include "chat-timeline-model.h"

#include <algorithm>

#include <QtCore/QBuffer>
#include <QtCore/QFileInfo>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>
#include <QtCore/QtGlobal>
#include <QtGui/QPixmap>
#include <QtWidgets/QFileIconProvider>

ChatTimelineModel::ChatTimelineModel(QObject *parent) : QAbstractListModel{parent}
{
}

int ChatTimelineModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

QVariant ChatTimelineModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return {};

    const auto &timelineItem = m_items.at(index.row());
    const auto &sender = timelineItem.sender;
    switch (role)
    {
    case StableIdRole: return timelineItem.stableId;
    case TransactionIdRole: return timelineItem.transactionId;
    case KindRole: return static_cast<int>(timelineItem.kind);
    case TimestampRole: return timelineItem.timestamp;
    case DateRole: return timelineItem.timestamp.date();
    case OwnEventRole: return sender.own;
    case SenderIdRole: return sender.id;
    case SenderDisplayNameRole: return sender.displayName;
    case SenderAvatarSourceRole: return sender.avatarSource;
    case SenderColorRole: return sender.color;
    case PlainTextRole: return timelineItem.content.plainText;
    case FormattedTextRole: return timelineItem.content.formattedText;
    case ReplyToIdRole: return timelineItem.content.replyToId;
    case AttachmentsRole: return attachmentData(timelineItem.content.attachments);
    case ReactionsRole: return reactionData(timelineItem.content.reactions);
    case DeliveryStateRole: return static_cast<int>(timelineItem.state.deliveryState);
    case EditedRole: return timelineItem.state.edited;
    case RedactedRole: return timelineItem.state.redacted;
    case EncryptedRole: return timelineItem.state.encrypted;
    case DecryptionStateRole: return static_cast<int>(timelineItem.state.decryptionState);
    case ErrorTextRole: return timelineItem.state.errorText;
    case GroupPositionRole: return static_cast<int>(groupPositionAt(index.row()));
    case ShowSenderRole: return groupPositionAt(index.row()) != GroupPosition::Middle && groupPositionAt(index.row()) != GroupPosition::Last;
    case ShowAvatarRole: return groupPositionAt(index.row()) != GroupPosition::Middle;
    case ShowTimestampRole: return groupPositionAt(index.row()) != GroupPosition::Middle;
    case StartsNewDayRole:
        return index.row() == 0 || m_items.at(index.row() - 1).timestamp.date() != timelineItem.timestamp.date();
    default: return {};
    }
}

QHash<int, QByteArray> ChatTimelineModel::roleNames() const
{
    return {{StableIdRole, "stableId"}, {TransactionIdRole, "transactionId"}, {KindRole, "kind"},
            {TimestampRole, "timestamp"}, {DateRole, "date"}, {OwnEventRole, "ownEvent"},
            {SenderIdRole, "senderId"}, {SenderDisplayNameRole, "senderDisplayName"},
            {SenderAvatarSourceRole, "senderAvatarSource"}, {SenderColorRole, "senderColor"},
            {PlainTextRole, "plainText"}, {FormattedTextRole, "formattedText"}, {ReplyToIdRole, "replyToId"},
            {AttachmentsRole, "attachments"}, {ReactionsRole, "reactions"}, {DeliveryStateRole, "deliveryState"},
            {EditedRole, "edited"}, {RedactedRole, "redacted"}, {EncryptedRole, "encrypted"},
            {DecryptionStateRole, "decryptionState"}, {ErrorTextRole, "errorText"},
            {GroupPositionRole, "groupPosition"}, {ShowSenderRole, "showSender"},
            {ShowAvatarRole, "showAvatar"}, {ShowTimestampRole, "showTimestamp"}, {StartsNewDayRole, "startsNewDay"}};
}

QVector<ChatTimelineItem> ChatTimelineModel::items() const { return m_items; }

ChatTimelineItem ChatTimelineModel::item(const QString &stableId) const
{
    const auto row = rowForStableId(stableId);
    return row < 0 ? ChatTimelineItem{} : m_items.at(row);
}

int ChatTimelineModel::rowForStableId(const QString &stableId) const { return m_rowsByStableId.value(stableId, -1); }
int ChatTimelineModel::rowForTransactionId(const QString &transactionId) const { return m_rowsByTransactionId.value(transactionId, -1); }

int ChatTimelineModel::groupingIntervalSeconds() const
{
    return m_groupingIntervalSeconds;
}

void ChatTimelineModel::setGroupingIntervalSeconds(int seconds)
{
    const auto interval = std::max(0, seconds);
    if (m_groupingIntervalSeconds == interval)
        return;

    m_groupingIntervalSeconds = interval;
    emit groupingIntervalChanged();
    if (!m_items.isEmpty())
        emit dataChanged(index(0), index(m_items.size() - 1), presentationRoles());
}

void ChatTimelineModel::reset(const QVector<ChatTimelineItem> &items)
{
    beginResetModel();
    m_items.clear();
    m_rowsByStableId.clear();
    m_rowsByTransactionId.clear();
    for (const auto &timelineItem : items)
    {
        const auto knownRow = timelineItem.stableId.isEmpty() ? -1 : m_rowsByStableId.value(timelineItem.stableId, -1);
        if (knownRow < 0)
            m_items.append(timelineItem);
        else if (m_items[knownRow].revision <= timelineItem.revision)
            m_items[knownRow] = timelineItem;
        rebuildRows();
    }
    std::sort(m_items.begin(), m_items.end(), comesBefore);
    rebuildRows();
    endResetModel();
}

void ChatTimelineModel::prepend(const ChatTimelinePage &page)
{
    for (const auto &timelineItem : page.items)
        upsert(timelineItem);
}

void ChatTimelineModel::append(const ChatTimelinePage &page)
{
    for (const auto &timelineItem : page.items)
        upsert(timelineItem);
}

void ChatTimelineModel::upsert(const ChatTimelineItem &timelineItem)
{
    if (!timelineItem.stableId.isEmpty())
    {
        const auto existingRow = rowForStableId(timelineItem.stableId);
        if (existingRow >= 0)
        {
            if (m_items.at(existingRow).revision > timelineItem.revision)
                return;
            replaceItem(existingRow, timelineItem);
            return;
        }
    }
    if (!timelineItem.transactionId.isEmpty())
    {
        const auto existingRow = rowForTransactionId(timelineItem.transactionId);
        if (existingRow >= 0)
        {
            if (m_items.at(existingRow).revision > timelineItem.revision)
                return;
            replaceItem(existingRow, timelineItem);
            return;
        }
    }
    insertItem(timelineItem);
}

void ChatTimelineModel::replaceLocalEcho(const QString &transactionId, const ChatTimelineItem &serverItem)
{
    const auto row = rowForTransactionId(transactionId);
    if (row < 0)
    {
        upsert(serverItem);
        return;
    }

    auto replacement = serverItem;
    replacement.transactionId = transactionId;
    replaceItem(row, replacement);
}

void ChatTimelineModel::update(const QString &stableId, const ChatTimelineItem &timelineItem)
{
    const auto row = rowForStableId(stableId);
    if (row < 0)
        return;

    auto replacement = timelineItem;
    replacement.stableId = stableId;
    if (m_items.at(row).revision > replacement.revision)
        return;
    if (replacement.transactionId.isEmpty())
        replacement.transactionId = m_items.at(row).transactionId;
    if (replacement.sourceOrder.isEmpty())
        replacement.sourceOrder = m_items.at(row).sourceOrder;
    replaceItem(row, replacement);
}

void ChatTimelineModel::redact(const QString &stableId, const QString &reason)
{
    const auto row = rowForStableId(stableId);
    if (row < 0)
        return;

    auto replacement = m_items.at(row);
    replacement.state.redacted = true;
    replacement.content.plainText.clear();
    replacement.content.formattedText.clear();
    replacement.content.attachments.clear();
    replacement.content.reactions.clear();
    replacement.state.errorText = reason;
    ++replacement.revision;
    replaceItem(row, replacement);
}

void ChatTimelineModel::remove(const QString &stableId)
{
    const auto row = rowForStableId(stableId);
    if (row < 0)
        return;

    beginRemoveRows({}, row, row);
    m_items.removeAt(row);
    rebuildRows();
    endRemoveRows();
    emitGroupingChangedAround(row);
}

void ChatTimelineModel::clear()
{
    if (m_items.isEmpty())
        return;
    beginResetModel();
    m_items.clear();
    rebuildRows();
    endResetModel();
}

bool ChatTimelineModel::comesBefore(const ChatTimelineItem &left, const ChatTimelineItem &right)
{
    if (left.sourceOrder != right.sourceOrder)
        return left.sourceOrder < right.sourceOrder;
    return left.stableId < right.stableId;
}

bool ChatTimelineModel::isMessage(const ChatTimelineItem &timelineItem)
{
    return !timelineItem.state.redacted && (timelineItem.kind == ChatTimelineItemKind::TextMessage ||
           timelineItem.kind == ChatTimelineItemKind::NoticeMessage ||
           timelineItem.kind == ChatTimelineItemKind::EmoteMessage ||
           timelineItem.kind == ChatTimelineItemKind::ImageMessage ||
           timelineItem.kind == ChatTimelineItemKind::FileMessage ||
           timelineItem.kind == ChatTimelineItemKind::AudioMessage ||
           timelineItem.kind == ChatTimelineItemKind::VideoMessage);
}

int ChatTimelineModel::insertionRow(const ChatTimelineItem &timelineItem) const
{
    auto row = 0;
    while (row < m_items.size() && comesBefore(m_items.at(row), timelineItem))
        ++row;
    return row;
}

ChatTimelineModel::GroupPosition ChatTimelineModel::groupPositionAt(int row) const
{
    const auto &timelineItem = m_items.at(row);
    if (!isMessage(timelineItem))
        return GroupPosition::Single;

    const auto matches = [this, &timelineItem](const ChatTimelineItem &candidate) {
        return isMessage(candidate) && candidate.sender.id == timelineItem.sender.id &&
               qAbs(candidate.timestamp.secsTo(timelineItem.timestamp)) <= m_groupingIntervalSeconds;
    };
    const auto previous = row > 0 && matches(m_items.at(row - 1));
    const auto next = row + 1 < m_items.size() && matches(m_items.at(row + 1));
    if (previous && next)
        return GroupPosition::Middle;
    if (previous)
        return GroupPosition::Last;
    if (next)
        return GroupPosition::First;
    return GroupPosition::Single;
}

void ChatTimelineModel::rebuildRows()
{
    m_rowsByStableId.clear();
    m_rowsByTransactionId.clear();
    for (auto row = 0; row < m_items.size(); ++row)
    {
        const auto &timelineItem = m_items.at(row);
        if (!timelineItem.stableId.isEmpty())
            m_rowsByStableId.insert(timelineItem.stableId, row);
        if (!timelineItem.transactionId.isEmpty())
            m_rowsByTransactionId.insert(timelineItem.transactionId, row);
    }
}

void ChatTimelineModel::emitGroupingChangedAround(int row)
{
    if (m_items.isEmpty())
        return;
    if (row > 0)
        emit dataChanged(index(row - 1), index(row - 1), presentationRoles());
    if (row + 1 < m_items.size())
        emit dataChanged(index(row + 1), index(row + 1), presentationRoles());
}

void ChatTimelineModel::insertItem(const ChatTimelineItem &timelineItem)
{
    const auto row = insertionRow(timelineItem);
    beginInsertRows({}, row, row);
    m_items.insert(row, timelineItem);
    rebuildRows();
    endInsertRows();
    emitGroupingChangedAround(row);
}

void ChatTimelineModel::replaceItem(int row, const ChatTimelineItem &timelineItem)
{
    const auto current = m_items.at(row);
    if (current.sourceOrder == timelineItem.sourceOrder && current.stableId == timelineItem.stableId)
    {
        m_items[row] = timelineItem;
        rebuildRows();
        emit dataChanged(index(row), index(row), itemDataRoles());
        emitGroupingChangedAround(row);
        return;
    }

    beginRemoveRows({}, row, row);
    m_items.removeAt(row);
    rebuildRows();
    endRemoveRows();
    insertItem(timelineItem);
}

QVariantList ChatTimelineModel::attachmentData(const QVector<ChatTimelineAttachment> &attachments)
{
    QVariantList result;
    result.reserve(attachments.size());
    for (const auto &attachment : attachments)
    {
        QVariantMap data;
        data.insert(QStringLiteral("kind"), static_cast<int>(attachment.kind));
        data.insert(QStringLiteral("fileName"), attachment.fileName);
        data.insert(QStringLiteral("mimeType"), attachment.mimeType);
        data.insert(QStringLiteral("size"), attachment.size);
        data.insert(QStringLiteral("dimensions"), attachment.dimensions);
        data.insert(QStringLiteral("duration"), attachment.duration);
        data.insert(QStringLiteral("sourceUri"), attachment.sourceUri);
        data.insert(QStringLiteral("thumbnailUri"), attachment.thumbnailUri);
        data.insert(QStringLiteral("state"), static_cast<int>(attachment.state));
        data.insert(QStringLiteral("progress"), attachment.progress);
        data.insert(QStringLiteral("localResourceId"), attachment.localResourceId);
        data.insert(QStringLiteral("errorText"), attachment.errorText);

        static QHash<QString, QString> iconSources;
        const auto iconKey = QFileInfo{attachment.fileName}.suffix().toCaseFolded();
        auto iconSource = iconSources.value(iconKey);
        if (iconSource.isEmpty())
        {
            const auto icon = QFileIconProvider{}.icon(QFileInfo{attachment.fileName});
            const auto pixmap = icon.pixmap(32, 32);
            QByteArray imageData;
            QBuffer buffer{&imageData};
            if (buffer.open(QIODevice::WriteOnly) && pixmap.save(&buffer, "PNG"))
            {
                iconSource = QStringLiteral("data:image/png;base64,") + QString::fromLatin1(imageData.toBase64());
                iconSources.insert(iconKey, iconSource);
            }
        }
        data.insert(QStringLiteral("iconSource"), iconSource);
        result.append(data);
    }
    return result;
}

QVariantList ChatTimelineModel::reactionData(const QVector<ChatTimelineReaction> &reactions)
{
    QVariantList result;
    result.reserve(reactions.size());
    for (const auto &reaction : reactions)
    {
        QVariantMap data;
        data.insert(QStringLiteral("key"), reaction.key);
        data.insert(QStringLiteral("senderIds"), reaction.senderIds);
        data.insert(QStringLiteral("senderDisplayNames"), reaction.senderDisplayNames);
        data.insert(QStringLiteral("own"), reaction.own);
        result.append(data);
    }
    return result;
}

const QList<int> &ChatTimelineModel::itemDataRoles()
{
    static const QList<int> roles{StableIdRole, TransactionIdRole, KindRole, TimestampRole, DateRole, OwnEventRole,
                                  SenderIdRole, SenderDisplayNameRole, SenderAvatarSourceRole, SenderColorRole,
                                  PlainTextRole, FormattedTextRole, ReplyToIdRole, AttachmentsRole, ReactionsRole,
                                  DeliveryStateRole, EditedRole, RedactedRole, EncryptedRole, DecryptionStateRole,
                                  ErrorTextRole, GroupPositionRole, ShowSenderRole, ShowAvatarRole, ShowTimestampRole,
                                  StartsNewDayRole};
    return roles;
}

const QList<int> &ChatTimelineModel::presentationRoles()
{
    static const QList<int> roles{GroupPositionRole, ShowSenderRole, ShowAvatarRole, ShowTimestampRole,
                                  StartsNewDayRole};
    return roles;
}
