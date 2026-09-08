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
#include <iterator>

#include <QtCore/QBuffer>
#include <QtCore/QFileInfo>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>
#include <QtCore/QtGlobal>
#include <QtGui/QColor>
#include <QtGui/QPixmap>
#include <QtWidgets/QFileIconProvider>

bool ChatTimelineModel::reactionsEqual(const QVector<ChatTimelineReaction> &left,
                                       const QVector<ChatTimelineReaction> &right)
{
    if (left.size() != right.size())
        return false;
    for (auto index = 0; index < left.size(); ++index)
    {
        const auto &leftReaction = left.at(index);
        const auto &rightReaction = right.at(index);
        if (leftReaction.key != rightReaction.key || leftReaction.senderIds != rightReaction.senderIds ||
            leftReaction.senderDisplayNames != rightReaction.senderDisplayNames ||
            leftReaction.own != rightReaction.own)
            return false;
    }
    return true;
}

bool ChatTimelineModel::attachmentsEqual(const QVector<ChatTimelineAttachment> &left,
                                         const QVector<ChatTimelineAttachment> &right)
{
    if (left.size() != right.size())
        return false;
    for (auto index = 0; index < left.size(); ++index)
    {
        const auto &leftAttachment = left.at(index);
        const auto &rightAttachment = right.at(index);
        if (leftAttachment.kind != rightAttachment.kind || leftAttachment.fileName != rightAttachment.fileName ||
            leftAttachment.mimeType != rightAttachment.mimeType || leftAttachment.size != rightAttachment.size ||
            leftAttachment.dimensions != rightAttachment.dimensions ||
            leftAttachment.duration != rightAttachment.duration ||
            leftAttachment.sourceUri != rightAttachment.sourceUri ||
            leftAttachment.thumbnailUri != rightAttachment.thumbnailUri ||
            leftAttachment.encryptedFileMetadata != rightAttachment.encryptedFileMetadata ||
            leftAttachment.state != rightAttachment.state ||
            leftAttachment.sourceState != rightAttachment.sourceState ||
            leftAttachment.progress != rightAttachment.progress ||
            leftAttachment.localResourceId != rightAttachment.localResourceId ||
            leftAttachment.errorText != rightAttachment.errorText)
            return false;
    }
    return true;
}
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
    case ProtocolEventTypeRole: return timelineItem.protocolEventType;
    case KindRole: return static_cast<int>(timelineItem.kind);
    case TimestampRole: return timelineItem.timestamp;
    case DateRole: return timelineItem.timestamp.date();
    case OwnEventRole: return sender.own;
    case SenderIdRole: return sender.id;
    case SenderDisplayNameRole: return sender.displayName;
    case SenderAvatarSourceRole: return sender.avatarSource;
    case SenderColorRole: return sender.color.isValid() ? sender.color : QColor{Qt::transparent};
    case PlainTextRole: return timelineItem.content.plainText;
    case FormattedTextRole: return timelineItem.content.formattedText;
    case ReplyToIdRole: return timelineItem.content.replyToId;
    case ReplyRole: return replyData(timelineItem);
    case AttachmentsRole: return attachmentData(timelineItem.content.attachments);
    case LocationUriRole: return timelineItem.content.locationUri;
    case ReactionsRole: return reactionData(timelineItem.content.reactions);
    case DeliveryStateRole: return static_cast<int>(timelineItem.state.deliveryState);
    case EditedRole: return timelineItem.state.edited;
    case RedactedRole: return timelineItem.state.redacted;
    case EncryptedRole: return timelineItem.state.encrypted;
    case DecryptionStateRole: return static_cast<int>(timelineItem.state.decryptionState);
    case ErrorTextRole: return timelineItem.state.errorText;
    case SystemEventRole: return !isMessage(timelineItem);
    case EmoteRole: return timelineItem.kind == ChatTimelineItemKind::EmoteMessage;
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
    return {{StableIdRole, "stableId"}, {TransactionIdRole, "transactionId"},
            {ProtocolEventTypeRole, "protocolEventType"}, {KindRole, "kind"},
            {TimestampRole, "timestamp"}, {DateRole, "date"}, {OwnEventRole, "ownEvent"},
            {SenderIdRole, "senderId"}, {SenderDisplayNameRole, "senderDisplayName"},
            {SenderAvatarSourceRole, "senderAvatarSource"}, {SenderColorRole, "senderColor"},
            {PlainTextRole, "plainText"}, {FormattedTextRole, "formattedText"}, {ReplyToIdRole, "replyToId"},
            {ReplyRole, "reply"},
            {AttachmentsRole, "attachments"}, {LocationUriRole, "locationUri"}, {ReactionsRole, "reactions"}, {DeliveryStateRole, "deliveryState"},
            {EditedRole, "edited"}, {RedactedRole, "redacted"}, {EncryptedRole, "encrypted"},
            {DecryptionStateRole, "decryptionState"}, {ErrorTextRole, "errorText"},
            {SystemEventRole, "systemEvent"}, {EmoteRole, "emote"},
            {GroupPositionRole, "groupPosition"}, {ShowSenderRole, "showSender"},
            {ShowAvatarRole, "showAvatar"}, {ShowTimestampRole, "showTimestamp"}, {StartsNewDayRole, "startsNewDay"}};
}

QVector<ChatTimelineItem> ChatTimelineModel::items() const { return m_items; }

ChatTimelineItem ChatTimelineModel::item(const QString &stableId) const
{
    const auto row = rowForStableId(stableId);
    return row < 0 ? ChatTimelineItem{} : m_items.at(row);
}

QVariantMap ChatTimelineModel::itemData(const ChatTimelineItem &item, bool showSender, bool showAvatar,
                                        bool showTimestamp, bool startsNewDay)
{
    return {{QStringLiteral("stableId"), item.stableId},
            {QStringLiteral("protocolEventType"), item.protocolEventType},
            {QStringLiteral("kind"), static_cast<int>(item.kind)},
            {QStringLiteral("timestamp"), item.timestamp},
            {QStringLiteral("ownEvent"), item.sender.own},
            {QStringLiteral("senderId"), item.sender.id},
            {QStringLiteral("senderDisplayName"), item.sender.displayName},
            {QStringLiteral("senderAvatarSource"), item.sender.avatarSource},
            {QStringLiteral("senderColor"), item.sender.color.isValid() ? item.sender.color : QColor{Qt::transparent}},
            {QStringLiteral("plainText"), item.content.plainText},
            {QStringLiteral("formattedText"), item.content.formattedText},
            {QStringLiteral("replyToId"), item.content.replyToId},
            {QStringLiteral("reply"), QVariantMap{}},
            {QStringLiteral("attachments"), attachmentData(item.content.attachments)},
            {QStringLiteral("locationUri"), item.content.locationUri},
            {QStringLiteral("reactions"), reactionData(item.content.reactions)},
            {QStringLiteral("deliveryState"), static_cast<int>(item.state.deliveryState)},
            {QStringLiteral("edited"), item.state.edited},
            {QStringLiteral("redacted"), item.state.redacted},
            {QStringLiteral("encrypted"), item.state.encrypted},
            {QStringLiteral("decryptionState"), static_cast<int>(item.state.decryptionState)},
            {QStringLiteral("errorText"), item.state.errorText},
            {QStringLiteral("systemEvent"), !isMessage(item)},
            {QStringLiteral("emote"), item.kind == ChatTimelineItemKind::EmoteMessage},
            {QStringLiteral("showSender"), showSender},
            {QStringLiteral("showAvatar"), showAvatar},
            {QStringLiteral("showTimestamp"), showTimestamp},
            {QStringLiteral("startsNewDay"), startsNewDay}};
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
    m_items.reserve(items.size());
    QHash<QString, int> rowsByStableId;
    for (const auto &timelineItem : items)
    {
        const auto knownRow = timelineItem.stableId.isEmpty() ? -1 : rowsByStableId.value(timelineItem.stableId, -1);
        if (knownRow < 0)
        {
            if (!timelineItem.stableId.isEmpty())
                rowsByStableId.insert(timelineItem.stableId, m_items.size());
            m_items.append(timelineItem);
        }
        else if (m_items[knownRow].revision <= timelineItem.revision)
            m_items[knownRow] = timelineItem;
    }
    std::sort(m_items.begin(), m_items.end(), comesBefore);
    rebuildRows();
    endResetModel();
}

bool ChatTimelineModel::prepend(const ChatTimelinePage &page, int maximumSize)
{
    return mergePage(page, true, maximumSize);
}

bool ChatTimelineModel::append(const ChatTimelinePage &page, int maximumSize)
{
    return mergePage(page, false, maximumSize);
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
    replacement.content.plainText = tr("Message removed.");
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
    emitReplyChangedFor(stableId);
    emitPresentationChangedAt(row - 1);
    emitPresentationChangedAt(row);
}

void ChatTimelineModel::removeFirst(int count)
{
    count = qBound(0, count, static_cast<int>(m_items.size()));
    if (count == 0)
        return;

    QStringList removedIds;
    removedIds.reserve(count);
    for (auto index = 0; index < count; ++index)
        removedIds.append(m_items.at(index).stableId);
    beginRemoveRows({}, 0, count - 1);
    m_items.remove(0, count);
    rebuildRows();
    endRemoveRows();
    for (const auto &stableId : removedIds)
        emitReplyChangedFor(stableId);
    emitPresentationChangedAt(0);
}

void ChatTimelineModel::removeLast(int count)
{
    count = qBound(0, count, static_cast<int>(m_items.size()));
    if (count == 0)
        return;

    const auto first = static_cast<int>(m_items.size()) - count;
    QStringList removedIds;
    removedIds.reserve(count);
    for (auto index = first; index < static_cast<int>(m_items.size()); ++index)
        removedIds.append(m_items.at(index).stableId);
    beginRemoveRows({}, first, static_cast<int>(m_items.size()) - 1);
    m_items.remove(first, count);
    rebuildRows();
    endRemoveRows();
    for (const auto &stableId : removedIds)
        emitReplyChangedFor(stableId);
    emitPresentationChangedAt(static_cast<int>(m_items.size()) - 1);
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
    return timelineItem.kind == ChatTimelineItemKind::TextMessage ||
           timelineItem.kind == ChatTimelineItemKind::NoticeMessage ||
           timelineItem.kind == ChatTimelineItemKind::EmoteMessage ||
           timelineItem.kind == ChatTimelineItemKind::ImageMessage ||
           timelineItem.kind == ChatTimelineItemKind::FileMessage ||
           timelineItem.kind == ChatTimelineItemKind::AudioMessage ||
           timelineItem.kind == ChatTimelineItemKind::VideoMessage ||
           timelineItem.kind == ChatTimelineItemKind::LocationMessage;
}

QList<int> ChatTimelineModel::changedItemDataRoles(const ChatTimelineItem &current,
                                                   const ChatTimelineItem &replacement)
{
    QList<int> roles;
    const auto addRole = [&roles](int role) {
        if (!roles.contains(role))
            roles.append(role);
    };

    if (current.stableId != replacement.stableId)
        addRole(StableIdRole);
    if (current.transactionId != replacement.transactionId)
        addRole(TransactionIdRole);
    if (current.protocolEventType != replacement.protocolEventType)
        addRole(ProtocolEventTypeRole);
    if (current.kind != replacement.kind)
    {
        addRole(KindRole);
        addRole(SystemEventRole);
        addRole(EmoteRole);
    }
    if (current.timestamp != replacement.timestamp)
    {
        addRole(TimestampRole);
        addRole(DateRole);
    }
    if (current.sender.own != replacement.sender.own)
        addRole(OwnEventRole);
    if (current.sender.id != replacement.sender.id)
        addRole(SenderIdRole);
    if (current.sender.displayName != replacement.sender.displayName)
        addRole(SenderDisplayNameRole);
    if (current.sender.avatarSource != replacement.sender.avatarSource)
        addRole(SenderAvatarSourceRole);
    if (current.sender.color != replacement.sender.color)
        addRole(SenderColorRole);
    if (current.content.plainText != replacement.content.plainText)
        addRole(PlainTextRole);
    if (current.content.formattedText != replacement.content.formattedText)
        addRole(FormattedTextRole);
    if (current.content.replyToId != replacement.content.replyToId)
    {
        addRole(ReplyToIdRole);
        addRole(ReplyRole);
    }
    if (!attachmentsEqual(current.content.attachments, replacement.content.attachments))
        addRole(AttachmentsRole);
    if (current.content.locationUri != replacement.content.locationUri)
        addRole(LocationUriRole);
    if (!reactionsEqual(current.content.reactions, replacement.content.reactions))
        addRole(ReactionsRole);
    if (current.state.deliveryState != replacement.state.deliveryState)
        addRole(DeliveryStateRole);
    if (current.state.edited != replacement.state.edited)
        addRole(EditedRole);
    if (current.state.redacted != replacement.state.redacted)
        addRole(RedactedRole);
    if (current.state.encrypted != replacement.state.encrypted)
        addRole(EncryptedRole);
    if (current.state.decryptionState != replacement.state.decryptionState)
        addRole(DecryptionStateRole);
    if (current.state.errorText != replacement.state.errorText)
        addRole(ErrorTextRole);

    if (current.kind != replacement.kind || current.timestamp != replacement.timestamp ||
        current.sender.id != replacement.sender.id)
        for (const auto role : presentationRoles())
            addRole(role);

    return roles;
}

int ChatTimelineModel::insertionRow(const ChatTimelineItem &timelineItem) const
{
    const auto position = std::lower_bound(m_items.cbegin(), m_items.cend(), timelineItem, comesBefore);
    return static_cast<int>(std::distance(m_items.cbegin(), position));
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
    emitPresentationChangedAt(row - 1);
    emitPresentationChangedAt(row + 1);
}

void ChatTimelineModel::emitPresentationChangedAt(int row)
{
    if (row >= 0 && row < m_items.size())
        emit dataChanged(index(row), index(row), presentationRoles());
}

bool ChatTimelineModel::mergePage(const ChatTimelinePage &page, bool prependPage, int maximumSize)
{
    QVector<ChatTimelineItem> additions;
    additions.reserve(page.items.size());

    for (const auto &timelineItem : page.items)
    {
        auto existingRow = timelineItem.stableId.isEmpty() ? -1 : rowForStableId(timelineItem.stableId);
        if (existingRow < 0 && !timelineItem.transactionId.isEmpty())
            existingRow = rowForTransactionId(timelineItem.transactionId);
        if (existingRow >= 0)
        {
            if (m_items.at(existingRow).revision <= timelineItem.revision)
                replaceItem(existingRow, timelineItem);
            continue;
        }

        const auto duplicate = std::find_if(additions.begin(), additions.end(), [&timelineItem](const auto &candidate) {
            return (!timelineItem.stableId.isEmpty() && candidate.stableId == timelineItem.stableId) ||
                   (!timelineItem.transactionId.isEmpty() &&
                    candidate.transactionId == timelineItem.transactionId);
        });
        if (duplicate == additions.end())
            additions.append(timelineItem);
        else if (duplicate->revision <= timelineItem.revision)
            *duplicate = timelineItem;
    }

    if (additions.isEmpty())
        return false;

    std::sort(additions.begin(), additions.end(), comesBefore);
    const auto formsContiguousBoundary =
        m_items.isEmpty() ||
        (prependPage ? comesBefore(additions.constLast(), m_items.constFirst())
                     : comesBefore(m_items.constLast(), additions.constFirst()));
    if (!formsContiguousBoundary)
    {
        for (const auto &timelineItem : additions)
            insertItem(timelineItem);
        const auto overflow = maximumSize > 0 ? m_items.size() - maximumSize : 0;
        if (overflow > 0)
        {
            if (prependPage)
                removeLast(overflow);
            else
                removeFirst(overflow);
        }
        return overflow > 0;
    }

    const auto overflow = maximumSize > 0 ? m_items.size() + additions.size() - maximumSize : 0;
    if (overflow > 0)
    {
        if (prependPage)
            removeLast(qMin(overflow, m_items.size()));
        else
            removeFirst(qMin(overflow, m_items.size()));
    }
    insertItems(prependPage ? 0 : m_items.size(), additions);
    const auto remainingOverflow = maximumSize > 0 ? m_items.size() - maximumSize : 0;
    if (remainingOverflow > 0)
    {
        if (prependPage)
            removeLast(remainingOverflow);
        else
            removeFirst(remainingOverflow);
    }
    return overflow > 0 || remainingOverflow > 0;
}

void ChatTimelineModel::insertItems(int row, const QVector<ChatTimelineItem> &items)
{
    if (items.isEmpty())
        return;

    const auto previousRow = row - 1;
    const auto followingRow = row + items.size();
    beginInsertRows({}, row, row + items.size() - 1);
    for (auto index = 0; index < items.size(); ++index)
        m_items.insert(row + index, items.at(index));
    rebuildRows();
    endInsertRows();

    emitPresentationChangedAt(previousRow);
    emitPresentationChangedAt(followingRow);
    for (const auto &timelineItem : items)
        emitReplyChangedFor(timelineItem.stableId);
}

void ChatTimelineModel::insertItem(const ChatTimelineItem &timelineItem)
{
    const auto row = insertionRow(timelineItem);
    insertItems(row, {timelineItem});
}

void ChatTimelineModel::replaceItem(int row, const ChatTimelineItem &timelineItem)
{
    const auto current = m_items.at(row);
    if (current.sourceOrder == timelineItem.sourceOrder && current.stableId == timelineItem.stableId)
    {
        const auto changedRoles = changedItemDataRoles(current, timelineItem);
        const auto groupingChanged = current.kind != timelineItem.kind || current.timestamp != timelineItem.timestamp ||
                                     current.sender.id != timelineItem.sender.id;
        const auto replyPreviewChanged = current.sender.displayName != timelineItem.sender.displayName ||
                                         current.content.plainText != timelineItem.content.plainText ||
                                         current.content.formattedText != timelineItem.content.formattedText;
        m_items[row] = timelineItem;
        if (current.transactionId != timelineItem.transactionId)
            rebuildRows();
        if (!changedRoles.isEmpty())
            emit dataChanged(index(row), index(row), changedRoles);
        if (replyPreviewChanged)
            emitReplyChangedFor(timelineItem.stableId);
        if (groupingChanged)
            emitGroupingChangedAround(row);
        return;
    }

    beginRemoveRows({}, row, row);
    m_items.removeAt(row);
    rebuildRows();
    endRemoveRows();
    emitReplyChangedFor(current.stableId);
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
        data.insert(QStringLiteral("sourceState"), static_cast<int>(attachment.sourceState));
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

QVariantMap ChatTimelineModel::replyData(const ChatTimelineItem &timelineItem) const
{
    QVariantMap data;
    const auto &replyToId = timelineItem.content.replyToId;
    if (replyToId.isEmpty())
        return data;

    data.insert(QStringLiteral("id"), replyToId);
    const auto row = rowForStableId(replyToId);
    if (row < 0)
    {
        data.insert(QStringLiteral("found"), false);
        return data;
    }

    const auto &reply = m_items.at(row);
    data.insert(QStringLiteral("found"), true);
    data.insert(QStringLiteral("senderDisplayName"), reply.sender.displayName);
    data.insert(QStringLiteral("plainText"), reply.content.plainText);
    data.insert(QStringLiteral("formattedText"), reply.content.formattedText);
    return data;
}

void ChatTimelineModel::emitReplyChangedFor(const QString &stableId)
{
    if (stableId.isEmpty())
        return;

    for (auto row = 0; row < m_items.size(); ++row)
        if (m_items.at(row).content.replyToId == stableId)
            emit dataChanged(index(row), index(row), {ReplyRole});
}

const QList<int> &ChatTimelineModel::presentationRoles()
{
    static const QList<int> roles{GroupPositionRole, ShowSenderRole, ShowAvatarRole, ShowTimestampRole,
                                  StartsNewDayRole};
    return roles;
}
