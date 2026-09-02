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

#include "exports.h"

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QUrl>

struct QmlThemeColorScheme
{
    QString id;
    QString displayName;
};

struct QmlThemeDescription
{
    QString id;
    QString displayName;
    QString author;
    QString type;
    QString version;
    QUrl source;
    QList<QmlThemeColorScheme> colorSchemes;

    bool isValid() const;
    QString normalizedColorScheme(const QString &scheme) const;
};

/**
 * @short Reads the common theme.desc format used by QML styles.
 *
 * A descriptor contains [Theme] metadata, an optional MainComponent file name
 * and optional [Colors] entries.
 * The System colour scheme is injected for external descriptors. Built-in
 * styles may opt out when their design intentionally has one fixed palette.
 */
class KADUAPI QmlThemeDescriptionLoader
{
public:
    static QmlThemeDescription builtIn(const QString &id, const QString &displayName, const QString &author,
                                       const QString &type, const QUrl &source,
                                       const QList<QmlThemeColorScheme> &colorSchemes = {},
                                       bool includeSystemColorScheme = true);
    // Returns the optional relative MainComponent value, or the caller's
    // conventional entry-point name when the descriptor omits it.
    static QString mainComponentFileName(const QString &descriptorPath, const QString &fallbackFileName);
    static QmlThemeDescription load(const QString &descriptorPath, const QString &id, const QUrl &source,
                                    const QString &expectedType, const QString &language);
};
