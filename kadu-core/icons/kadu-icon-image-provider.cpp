/*
 * %kadu copyright begin%
 * Copyright 2026 Kadu Qt6 port
 * %kadu copyright end%
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#include "kadu-icon-image-provider.h"

#include "icons/icons-manager.h"
#include "icons/kadu-icon.h"

#include <QtCore/QUrl>
#include <QtGui/QPixmap>

KaduIconImageProvider::KaduIconImageProvider(IconsManager *iconsManager)
        : QQuickImageProvider{QQuickImageProvider::Image}, m_iconsManager{iconsManager}
{
}

QImage KaduIconImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    const auto iconPath = QUrl::fromPercentEncoding(id.section(u'?', 0, 0).toUtf8());
    if (!m_iconsManager || iconPath.isEmpty())
        return transparentImage();

    const auto imageSize = requestedSize.isValid() ? requestedSize : QSize{16, 16};
    const auto pixmap = m_iconsManager->iconByPath(KaduIcon{iconPath}).pixmap(imageSize);
    if (pixmap.isNull())
        return transparentImage();

    const auto image = pixmap.toImage();
    if (size)
        *size = image.size();
    return image;
}

QImage KaduIconImageProvider::transparentImage()
{
    QImage image{1, 1, QImage::Format_ARGB32_Premultiplied};
    image.fill(Qt::transparent);
    return image;
}
