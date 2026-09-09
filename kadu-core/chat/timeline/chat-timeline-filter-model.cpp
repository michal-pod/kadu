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

#include "chat-timeline-filter-model.h"
#include "chat-timeline-filter-model.moc"

#include <QtCore/QtGlobal>

static_assert(static_cast<int>(ChatTimelineItemLevel::Chat) == static_cast<int>(ChatTimelineDetails::ChatOnly));
static_assert(static_cast<int>(ChatTimelineItemLevel::Important) == static_cast<int>(ChatTimelineDetails::Important));
static_assert(
    static_cast<int>(ChatTimelineItemLevel::Informational) == static_cast<int>(ChatTimelineDetails::AllEvents));
static_assert(static_cast<int>(ChatTimelineItemLevel::Debug) == static_cast<int>(ChatTimelineDetails::Debug));

ChatTimelineFilterModel::ChatTimelineFilterModel(ChatTimelineModel *sourceModel, QObject *parent)
        : QSortFilterProxyModel{parent}, m_sourceModel{sourceModel}
{
    Q_ASSERT(m_sourceModel);
    setDynamicSortFilter(true);
    setFilterRole(ChatTimelineModel::LevelRole);
    setSourceModel(m_sourceModel);
    connect(m_sourceModel, &ChatTimelineModel::groupingIntervalChanged, this, [this] {
        emit groupingIntervalChanged();
        emitPresentationChanged();
    });
    connect(m_sourceModel, &QAbstractItemModel::rowsInserted, this,
            [this] { emitPresentationChanged(); });
    connect(m_sourceModel, &QAbstractItemModel::rowsRemoved, this,
            [this] { emitPresentationChanged(); });
    connect(m_sourceModel, &QAbstractItemModel::modelReset, this,
            [this] { emitPresentationChanged(); });
    connect(m_sourceModel, &QAbstractItemModel::dataChanged, this,
            [this] { emitPresentationChanged(); });
}

ChatTimelineDetails ChatTimelineFilterModel::details() const
{
    return m_details;
}

void ChatTimelineFilterModel::setDetails(ChatTimelineDetails details)
{
    if (!isConcreteChatTimelineDetails(details))
        details = ChatTimelineDetails::AllEvents;
    if (m_details == details)
        return;

    m_details = details;
    invalidateFilter();
    emit detailsChanged();
    emitPresentationChanged();
}

int ChatTimelineFilterModel::groupingIntervalSeconds() const
{
    return m_sourceModel->groupingIntervalSeconds();
}

void ChatTimelineFilterModel::setGroupingIntervalSeconds(int seconds)
{
    m_sourceModel->setGroupingIntervalSeconds(seconds);
}

QVector<ChatTimelineItem> ChatTimelineFilterModel::items() const
{
    QVector<ChatTimelineItem> result;
    result.reserve(rowCount());
    for (auto row = 0; row < rowCount(); ++row)
        if (const auto *timelineItem = itemAt(row))
            result.append(*timelineItem);
    return result;
}

ChatTimelineItem ChatTimelineFilterModel::item(const QString &stableId) const
{
    return m_sourceModel->item(stableId);
}

int ChatTimelineFilterModel::rowForStableId(const QString &stableId) const
{
    const auto sourceRow = m_sourceModel->rowForStableId(stableId);
    if (sourceRow < 0)
        return -1;
    const auto proxyIndex = mapFromSource(m_sourceModel->index(sourceRow, 0));
    return proxyIndex.isValid() ? proxyIndex.row() : -1;
}

int ChatTimelineFilterModel::rowForTransactionId(const QString &transactionId) const
{
    const auto sourceRow = m_sourceModel->rowForTransactionId(transactionId);
    if (sourceRow < 0)
        return -1;
    const auto proxyIndex = mapFromSource(m_sourceModel->index(sourceRow, 0));
    return proxyIndex.isValid() ? proxyIndex.row() : -1;
}

bool ChatTimelineFilterModel::contains(const QString &stableId) const
{
    return m_sourceModel->rowForStableId(stableId) >= 0;
}

bool ChatTimelineFilterModel::acceptsItem(const ChatTimelineItem &timelineItem) const
{
    return static_cast<int>(timelineItem.level) <= static_cast<int>(m_details);
}

void ChatTimelineFilterModel::reset(const QVector<ChatTimelineItem> &items)
{
    m_sourceModel->reset(items);
}

void ChatTimelineFilterModel::upsert(const ChatTimelineItem &item)
{
    m_sourceModel->upsert(item);
}

void ChatTimelineFilterModel::clear()
{
    m_sourceModel->clear();
}

QVariant ChatTimelineFilterModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return {};

    const auto position = groupPositionAt(index.row());
    switch (role)
    {
    case ChatTimelineModel::GroupPositionRole:
        return static_cast<int>(position);
    case ChatTimelineModel::ShowSenderRole:
        return position != ChatTimelineModel::GroupPosition::Middle &&
               position != ChatTimelineModel::GroupPosition::Last;
    case ChatTimelineModel::ShowAvatarRole:
        return position != ChatTimelineModel::GroupPosition::Middle;
    case ChatTimelineModel::ShowTimestampRole:
        return position != ChatTimelineModel::GroupPosition::Middle;
    case ChatTimelineModel::StartsNewDayRole:
    {
        const auto *timelineItem = itemAt(index.row());
        const auto *previous = itemAt(index.row() - 1);
        return timelineItem && (!previous || previous->timestamp.date() != timelineItem->timestamp.date());
    }
    default:
        return QSortFilterProxyModel::data(index, role);
    }
}

bool ChatTimelineFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (sourceParent.isValid())
        return false;
    const auto *timelineItem = m_sourceModel->itemAtRow(sourceRow);
    return timelineItem && acceptsItem(*timelineItem);
}

bool ChatTimelineFilterModel::isMessage(const ChatTimelineItem &timelineItem)
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

const ChatTimelineItem *ChatTimelineFilterModel::itemAt(int row) const
{
    if (row < 0 || row >= rowCount())
        return nullptr;
    const auto sourceIndex = mapToSource(index(row, 0));
    return sourceIndex.isValid() ? m_sourceModel->itemAtRow(sourceIndex.row()) : nullptr;
}

ChatTimelineModel::GroupPosition ChatTimelineFilterModel::groupPositionAt(int row) const
{
    const auto *timelineItem = itemAt(row);
    if (!timelineItem || !isMessage(*timelineItem))
        return ChatTimelineModel::GroupPosition::Single;

    const auto matches = [this, timelineItem](const ChatTimelineItem *candidate) {
        return candidate && isMessage(*candidate) && candidate->sender.id == timelineItem->sender.id &&
               qAbs(candidate->timestamp.secsTo(timelineItem->timestamp)) <= groupingIntervalSeconds();
    };
    const auto previous = matches(itemAt(row - 1));
    const auto next = matches(itemAt(row + 1));
    if (previous && next)
        return ChatTimelineModel::GroupPosition::Middle;
    if (previous)
        return ChatTimelineModel::GroupPosition::Last;
    if (next)
        return ChatTimelineModel::GroupPosition::First;
    return ChatTimelineModel::GroupPosition::Single;
}

void ChatTimelineFilterModel::emitPresentationChanged()
{
    if (rowCount() == 0)
        return;
    emit dataChanged(
        index(0, 0), index(rowCount() - 1, 0),
        {ChatTimelineModel::GroupPositionRole, ChatTimelineModel::ShowSenderRole,
         ChatTimelineModel::ShowAvatarRole, ChatTimelineModel::ShowTimestampRole,
         ChatTimelineModel::StartsNewDayRole});
}
