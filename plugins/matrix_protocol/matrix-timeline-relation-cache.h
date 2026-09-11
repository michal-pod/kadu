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

#include <QtCore/QHash>
#include <QtCore/QJsonObject>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QString>

class QTimer;

namespace Quotient
{
class Connection;
}

class MatrixTimelineRelationCache final : public QObject
{
public:
    explicit MatrixTimelineRelationCache(QObject *parent = nullptr);
    ~MatrixTimelineRelationCache() override;

    void setConnection(Quotient::Connection *connection);
    void rememberReaction(const QString &roomId, const QString &reactionEventId, const QString &targetEventId) const;
    QString reactionTarget(const QString &roomId, const QString &reactionEventId) const;
    void rememberReplacement(const QString &roomId, const QString &targetEventId,
                             const QJsonObject &replacementEvent) const;
    QJsonObject replacement(const QString &roomId, const QString &targetEventId) const;
    QString replacementTarget(const QString &roomId, const QString &replacementEventId) const;
    void forgetReplacement(const QString &roomId, const QString &targetEventId) const;

private:
    struct Relation
    {
        QString targetEventId;
        qint64 observedAt = 0;
    };

    struct Replacement
    {
        QJsonObject event;
        qint64 originTimestamp = 0;
        qint64 observedAt = 0;
    };

    static constexpr auto CacheVersion = 1;
    static constexpr auto MaximumRelationsPerRoom = 20000;
    static constexpr auto RetainedRelationsPerRoom = 18000;
    static constexpr auto MaximumReplacementsPerRoom = 2000;
    static constexpr auto RetainedReplacementsPerRoom = 1800;

    QPointer<Quotient::Connection> m_connection;
    QTimer *m_saveTimer;
    mutable QHash<QString, QHash<QString, Relation>> m_reactions;
    mutable QHash<QString, QHash<QString, Replacement>> m_replacements;
    mutable QString m_cachePath;
    mutable bool m_loaded = false;
    mutable bool m_dirty = false;

    bool ensureLoaded() const;
    void scheduleSave() const;
    void save() const;
    void pruneRoom(const QString &roomId) const;
    void pruneReplacements(const QString &roomId) const;
};
