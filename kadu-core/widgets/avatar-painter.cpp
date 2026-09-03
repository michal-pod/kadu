/*
 * %kadu copyright begin%
 * Copyright 2012 Wojciech Treter (juzefwt@gmail.com)
 * Copyright 2012, 2014 Bartosz Brachaczek (b.brachaczek@gmail.com)
 * Copyright 2011, 2013, 2014 Rafał Przemysław Malinowski (rafal.przemyslaw.malinowski@gmail.com)
 * %kadu copyright end%
 * Copyright 2010 Dariusz Markowicz
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

#include <QtGui/QPainterPath>
#include <QtCore/QModelIndex>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>
#include <QtGui/QPixmapCache>
#include <QtWidgets/QStyleOptionViewItem>

#include "contacts/contact.h"
#include "model/roles.h"
#include "status/status.h"
#include "widgets/talkable-delegate-configuration.h"

#include "avatar-painter.h"

AvatarPainter::AvatarPainter(
    TalkableDelegateConfiguration *configuration, const QStyleOptionViewItem &option, const QRect &avatarRect,
    const QModelIndex &index)
        : Configuration(configuration), Option(option), AvatarRect(avatarRect), Index(index)
{
    Avatar = Index.data(AvatarRole).value<QPixmap>();
}

bool AvatarPainter::greyOut() const
{
    if (Configuration->avatarStyle() != TalkableDelegateConfiguration::AvatarStyle::GreyOut)
        return false;

    const auto contact = Index.data(ContactRole).value<Contact>();
    return contact && contact.currentStatus().isDisconnected();
}

StatusType AvatarPainter::statusType() const
{
    const auto statusData = Index.data(StatusRole);
    return statusData.isValid() ? statusData.value<Status>().type() : StatusType::None;
}

QColor AvatarPainter::statusDotColor() const
{
    if (Configuration->avatarStyle() != TalkableDelegateConfiguration::AvatarStyle::StatusDot)
        return {};

    switch (statusType())
    {
    case StatusType::FreeForChat:
    case StatusType::Online:
        return QColor{34, 197, 94};
    case StatusType::Away:
        return QColor{234, 179, 8};
    case StatusType::DoNotDisturb:
        return QColor{239, 68, 68};
    default:
        return {};
    }
}

QString AvatarPainter::cacheKey(qreal devicePixelRatio)
{
    // The ratio belongs in the key: an item rendered for one screen must not be reused on another
    // with a different scale, which is exactly what happens with two monitors magnified
    // differently.
    return QString("msi-%1-%2,%3,%4,%5,%6,%7")
        .arg(Avatar.cacheKey())
        .arg(static_cast<int>(Configuration->avatarStyle()))
        .arg(static_cast<int>(statusType()))
        .arg(Configuration->avatarBorder())
        .arg(Option.state & QStyle::State_Selected ? 1 : 0)
        .arg(Option.palette.cacheKey())
        .arg(devicePixelRatio);
}

QPixmap AvatarPainter::getOrCreateCacheItem(qreal devicePixelRatio)
{
    QString key = cacheKey(devicePixelRatio);

    QPixmap cached;
    if (QPixmapCache::find(key, &cached))
        return cached;

    // The item has to hold as many pixels as the screen shows, while the painting below stays in
    // the logical coordinates the rest of the delegate works in.
    QPixmap item = QPixmap((QSizeF{AvatarRect.size()} * devicePixelRatio).toSize());
    item.setDevicePixelRatio(devicePixelRatio);
    item.fill(QColor(0, 0, 0, 0));

    QPainter cachePainter;
    cachePainter.begin(&item);
    doPaint(&cachePainter, AvatarRect.size(), devicePixelRatio);
    cachePainter.end();

    QPixmapCache::insert(key, item);

    return item;
}

void AvatarPainter::paintFromCache(QPainter *painter)
{
    QPixmap cached = getOrCreateCacheItem(painter->device()->devicePixelRatio());

    painter->drawPixmap(AvatarRect, cached);
}

QPixmap AvatarPainter::cropped()
{
    int minDimension = Avatar.height() < Avatar.width() ? Avatar.height() : Avatar.width();

    int x = (Avatar.width() - minDimension) / 2;
    int y = (Avatar.height() - minDimension) / 2;

    QImage cropped = Avatar.toImage().copy(x, y, minDimension, minDimension);

    return QPixmap::fromImage(cropped);
}

void AvatarPainter::paintStatusDot(QPainter *painter, const QRect &displayRect) const
{
    const auto color = statusDotColor();
    if (!color.isValid())
        return;

    const auto diameter = qBound(8.0, qMin(displayRect.width(), displayRect.height()) * 0.32, 12.0);
    QRectF dotRect{0.0, 0.0, diameter, diameter};
    dotRect.moveBottomRight(QPointF{displayRect.right() + 0.5, displayRect.bottom() + 0.5});

    const auto backgroundRole =
        Option.state & QStyle::State_Selected ? QPalette::Highlight : QPalette::Base;

    painter->save();
    painter->setPen(QPen{Option.palette.color(backgroundRole), 2.0});
    painter->setBrush(color);
    painter->drawEllipse(dotRect);

    if (statusType() == StatusType::FreeForChat)
    {
        const auto center = dotRect.center();
        const auto armLength = diameter * 0.2;
        painter->setPen(
            QPen{QColor{Qt::white}, qMax(1.25, diameter * 0.12), Qt::SolidLine, Qt::RoundCap});
        painter->drawLine(QPointF{center.x() - armLength, center.y()}, QPointF{center.x() + armLength, center.y()});
        painter->drawLine(QPointF{center.x(), center.y() - armLength}, QPointF{center.x(), center.y() + armLength});
    }
    painter->restore();
}

void AvatarPainter::doPaint(QPainter *painter, const QSize &size, qreal devicePixelRatio)
{
    QPixmap croppedAvatar = cropped();

    // The area the avatar covers, in logical units: shrink to fit, never enlarge -- as before.
    QSize displaySize = croppedAvatar.size();
    if (displaySize.width() > size.width() || displaySize.height() > size.height())
        displaySize.scale(size, Qt::KeepAspectRatio);

    // Fill that area with the pixel count the screen actually has. Scaling down to the logical
    // size and leaving the enlargement to the painter is what made avatars soft: the stored file
    // is usually larger than the contact list shows, and every pixel above the logical size was
    // being discarded.
    QSize const targetSize = (QSizeF{displaySize} * devicePixelRatio).toSize();
    QPixmap displayAvatar = croppedAvatar.size() == targetSize
                                ? croppedAvatar
                                : croppedAvatar.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QRect displayRect{QPoint{0, 0}, displaySize};
    displayRect.moveTop((size.height() - displayRect.height()) / 2);
    displayRect.moveLeft((size.width() - displayRect.width()) / 2);

    // grey out offline contacts' avatar
    displayAvatar = greyOut() ? QIcon(displayAvatar).pixmap(displayAvatar.size(), QIcon::Disabled) : displayAvatar;
    displayAvatar.setDevicePixelRatio(devicePixelRatio);

    int radius = 3;
    QPainterPath displayRectPath;
    displayRectPath.addRoundedRect(displayRect, radius, radius);

    painter->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    painter->setClipPath(displayRectPath);
    painter->drawPixmap(displayRect, displayAvatar);
    painter->setClipping(false);

    // draw avatar border
    if (Configuration->avatarBorder())
        painter->drawRoundedRect(displayRect, radius, radius);

    paintStatusDot(painter, displayRect);
}

void AvatarPainter::paint(QPainter *painter)
{
    if (!Configuration->showAvatars() || AvatarRect.isEmpty() || Avatar.isNull())
        return;

    paintFromCache(painter);
}
