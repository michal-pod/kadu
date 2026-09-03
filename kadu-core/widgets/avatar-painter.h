/*
 * %kadu copyright begin%
 * Copyright 2012 Wojciech Treter (juzefwt@gmail.com)
 * Copyright 2012, 2014 Bartosz Brachaczek (b.brachaczek@gmail.com)
 * Copyright 2011, 2013, 2014 Rafał Przemysław Malinowski (rafal.przemyslaw.malinowski@gmail.com)
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

#ifndef AVATAR_PAINTER_H
#define AVATAR_PAINTER_H

#include "status/status-type.h"

#include <QtCore/QString>
#include <QtGui/QColor>
#include <QtGui/QPixmap>
#include <QtWidgets/QStyleOptionViewItem>

class QModelIndex;
class QPainter;

class TalkableDelegateConfiguration;

class AvatarPainter
{
    TalkableDelegateConfiguration *Configuration;
    const QStyleOptionViewItem &Option;
    const QRect &AvatarRect;
    const QModelIndex &Index;

    QPixmap Avatar;

    bool greyOut() const;
    StatusType statusType() const;
    QColor statusDotColor() const;
    QPixmap cropped();
    QString cacheKey(qreal devicePixelRatio);
    QPixmap getOrCreateCacheItem(qreal devicePixelRatio);
    void paintFromCache(QPainter *painter);
    void paintStatusDot(QPainter *painter, const QRect &displayRect) const;

    void doPaint(QPainter *painter, const QSize &size, qreal devicePixelRatio);

public:
    AvatarPainter(
        TalkableDelegateConfiguration *configuration, const QStyleOptionViewItem &option, const QRect &avatarRect,
        const QModelIndex &index);

    void paint(QPainter *painter);
};

#endif   // AVATAR_PAINTER_H
