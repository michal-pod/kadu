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

#include "matrix-timeline-relation-cache.h"

#include <Quotient/connection.h>

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSaveFile>
#include <QtCore/QTimer>

#include <algorithm>

MatrixTimelineRelationCache::MatrixTimelineRelationCache(QObject *parent)
        : QObject{parent}, m_saveTimer{new QTimer{this}}
{
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(1000);
    connect(m_saveTimer, &QTimer::timeout, this, [this] { save(); });
}

MatrixTimelineRelationCache::~MatrixTimelineRelationCache()
{
    save();
}

void MatrixTimelineRelationCache::setConnection(Quotient::Connection *connection)
{
    if (m_connection == connection)
        return;

    save();
    m_saveTimer->stop();
    m_connection = connection;
    m_reactions.clear();
    m_cachePath.clear();
    m_loaded = false;
    m_dirty = false;
}

void MatrixTimelineRelationCache::rememberReaction(const QString &roomId, const QString &reactionEventId,
                                                   const QString &targetEventId) const
{
    if (roomId.isEmpty() || reactionEventId.isEmpty() || targetEventId.isEmpty() || !ensureLoaded())
        return;

    auto &roomRelations = m_reactions[roomId];
    const auto existing = roomRelations.constFind(reactionEventId);
    if (existing != roomRelations.cend() && existing->targetEventId == targetEventId)
        return;

    roomRelations.insert(reactionEventId, {targetEventId, QDateTime::currentMSecsSinceEpoch()});
    pruneRoom(roomId);
    m_dirty = true;
    scheduleSave();
}

QString MatrixTimelineRelationCache::reactionTarget(const QString &roomId, const QString &reactionEventId) const
{
    if (roomId.isEmpty() || reactionEventId.isEmpty() || !ensureLoaded())
        return {};

    const auto roomIterator = m_reactions.constFind(roomId);
    if (roomIterator == m_reactions.cend())
        return {};
    const auto relationIterator = roomIterator->constFind(reactionEventId);
    return relationIterator == roomIterator->cend() ? QString{} : relationIterator->targetEventId;
}

bool MatrixTimelineRelationCache::ensureLoaded() const
{
    if (m_loaded)
        return true;
    if (!m_connection || m_connection->userId().isEmpty())
        return false;

    m_loaded = true;
    m_cachePath = m_connection->stateCacheDir().filePath(QStringLiteral("kadu-timeline-relations.json"));

    QFile cacheFile{m_cachePath};
    if (!cacheFile.open(QIODevice::ReadOnly))
        return true;

    const auto document = QJsonDocument::fromJson(cacheFile.readAll());
    const auto root = document.object();
    if (root.value(QStringLiteral("version")).toInt() != CacheVersion ||
        root.value(QStringLiteral("accountMxid")).toString() != m_connection->userId())
        return true;

    const auto rooms = root.value(QStringLiteral("rooms")).toObject();
    for (auto roomIterator = rooms.begin(); roomIterator != rooms.end(); ++roomIterator)
    {
        auto &roomRelations = m_reactions[roomIterator.key()];
        const auto relations = roomIterator.value().toArray();
        for (const auto &relationValue : relations)
        {
            const auto relation = relationValue.toObject();
            const auto eventId = relation.value(QStringLiteral("eventId")).toString();
            const auto targetEventId = relation.value(QStringLiteral("targetEventId")).toString();
            if (eventId.isEmpty() || targetEventId.isEmpty())
                continue;
            roomRelations.insert(eventId,
                                 {targetEventId, relation.value(QStringLiteral("observedAt")).toInteger()});
        }
        pruneRoom(roomIterator.key());
    }
    return true;
}

void MatrixTimelineRelationCache::scheduleSave() const
{
    if (!m_saveTimer->isActive())
        m_saveTimer->start();
}

void MatrixTimelineRelationCache::save() const
{
    if (!m_dirty || !m_loaded || m_cachePath.isEmpty() || !m_connection)
        return;

    QJsonObject rooms;
    for (auto roomIterator = m_reactions.cbegin(); roomIterator != m_reactions.cend(); ++roomIterator)
    {
        QJsonArray relations;
        for (auto relationIterator = roomIterator->cbegin(); relationIterator != roomIterator->cend();
             ++relationIterator)
        {
            relations.append(QJsonObject{{QStringLiteral("eventId"), relationIterator.key()},
                                         {QStringLiteral("targetEventId"), relationIterator->targetEventId},
                                         {QStringLiteral("observedAt"), relationIterator->observedAt}});
        }
        if (!relations.isEmpty())
            rooms.insert(roomIterator.key(), relations);
    }

    QDir{}.mkpath(QFileInfo{m_cachePath}.absolutePath());
    QSaveFile cacheFile{m_cachePath};
    if (!cacheFile.open(QIODevice::WriteOnly))
        return;

    const QJsonObject root{{QStringLiteral("version"), CacheVersion},
                           {QStringLiteral("accountMxid"), m_connection->userId()},
                           {QStringLiteral("rooms"), rooms}};
    const auto payload = QJsonDocument{root}.toJson(QJsonDocument::Compact);
    if (cacheFile.write(payload) != payload.size() || !cacheFile.commit())
        return;

    m_dirty = false;
}

void MatrixTimelineRelationCache::pruneRoom(const QString &roomId) const
{
    auto roomIterator = m_reactions.find(roomId);
    if (roomIterator == m_reactions.end() || roomIterator->size() <= MaximumRelationsPerRoom)
        return;

    auto eventIds = roomIterator->keys();
    std::sort(eventIds.begin(), eventIds.end(), [&roomIterator](const QString &left, const QString &right) {
        return roomIterator->value(left).observedAt < roomIterator->value(right).observedAt;
    });
    const auto removeCount = roomIterator->size() - RetainedRelationsPerRoom;
    for (auto index = 0; index < removeCount; ++index)
        roomIterator->remove(eventIds.at(index));
}
