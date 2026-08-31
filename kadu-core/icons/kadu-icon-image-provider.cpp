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
    auto iconPath = QUrl::fromPercentEncoding(id.section(u'?', 0, 0).toUtf8());
    // QQuickImageProvider passes the URL path as the ID. Depending on the
    // Qt path used by Image, that can retain its leading slash; KaduIcon
    // treats such a path as an absolute file name instead of an icon name.
    while (iconPath.startsWith(u'/'))
        iconPath.remove(0, 1);
    if (!m_iconsManager || iconPath.isEmpty())
        return transparentImage();

    const auto imageSize = requestedSize.isValid() ? requestedSize : QSize{16, 16};
    KaduIcon icon{iconPath, QStringLiteral("%1x%2").arg(imageSize.width()).arg(imageSize.height())};
    const auto filePath = m_iconsManager->iconPath(icon, IconsManager::EmptyAllowed);
    const auto pixmap = filePath.isEmpty() ? m_iconsManager->iconByPath(icon).pixmap(imageSize)
                                           : QPixmap{filePath}.scaled(imageSize, Qt::KeepAspectRatio,
                                                                       Qt::SmoothTransformation);
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
