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

#include "chat/chat-timeline-details.h"
#include "chat/timeline/chat-timeline-model.h"
#include "exports.h"

#include <QtCore/QSortFilterProxyModel>

/**
 * @short Presentation-only view of a complete chat timeline.
 *
 * The source model keeps every protocol event for pagination, replies and read
 * markers. This proxy only decides which rows QML sees and recalculates visual
 * grouping after rows have been hidden.
 */
class KADUAPI ChatTimelineFilterModel final : public QSortFilterProxyModel
{
    Q_OBJECT

    Q_PROPERTY(int groupingIntervalSeconds READ groupingIntervalSeconds WRITE setGroupingIntervalSeconds NOTIFY
                   groupingIntervalChanged)
    Q_PROPERTY(ChatTimelineDetails details READ details WRITE setDetails NOTIFY detailsChanged)

public:
    explicit ChatTimelineFilterModel(ChatTimelineModel *sourceModel, QObject *parent = nullptr);

    ChatTimelineDetails details() const;
    void setDetails(ChatTimelineDetails details);
    int groupingIntervalSeconds() const;
    void setGroupingIntervalSeconds(int seconds);

    QVector<ChatTimelineItem> items() const;
    ChatTimelineItem item(const QString &stableId) const;
    Q_INVOKABLE int rowForStableId(const QString &stableId) const;
    Q_INVOKABLE int rowForTransactionId(const QString &transactionId) const;
    bool contains(const QString &stableId) const;
    bool acceptsItem(const ChatTimelineItem &item) const;

    void reset(const QVector<ChatTimelineItem> &items);
    void upsert(const ChatTimelineItem &item);
    void clear();

    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

signals:
    void groupingIntervalChanged();
    void detailsChanged();

protected:
    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    ChatTimelineModel *m_sourceModel = nullptr;
    ChatTimelineDetails m_details = ChatTimelineDetails::AllEvents;

    static bool isMessage(const ChatTimelineItem &item);
    const ChatTimelineItem *itemAt(int row) const;
    ChatTimelineModel::GroupPosition groupPositionAt(int row) const;
    void emitPresentationChanged();
};
