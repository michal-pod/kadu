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

#pragma once

#include "exports.h"

#include <QtCore/QPointer>
#include <QtGui/QImage>
#include <QtQuick/QQuickImageProvider>

class IconsManager;

/**
 * @short Exposes icons from the active Kadu icon theme to QML.
 *
 * The provider accepts an icon path after image://kaduicon/, for example
 * image://kaduicon/edit-copy. It keeps QML renderers independent from a
 * particular icon theme while still using the same icons as widget actions.
 */
class KADUAPI KaduIconImageProvider final : public QQuickImageProvider
{
public:
    explicit KaduIconImageProvider(IconsManager *iconsManager);

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    QPointer<IconsManager> m_iconsManager;

    static QImage transparentImage();
};
