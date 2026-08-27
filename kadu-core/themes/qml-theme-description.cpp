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

#include "qml-theme-description.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QSet>
#include <QtCore/QSettings>

namespace ThemeDescription
{
QString localizedValue(QSettings &settings, const QString &key, const QString &language)
{
    return settings.value(key + QStringLiteral("[") + language + QLatin1Char{']'}, settings.value(key)).toString();
}
} // namespace ThemeDescription

bool QmlThemeDescription::isValid() const
{
    return !id.isEmpty() && !displayName.isEmpty() && !type.isEmpty() && source.isValid();
}

QString QmlThemeDescription::normalizedColorScheme(const QString &scheme) const
{
    for (const auto &availableScheme : colorSchemes)
        if (availableScheme.id == scheme)
            return scheme;

    return QStringLiteral("System");
}

QmlThemeDescription QmlThemeDescriptionLoader::builtIn(const QString &id, const QString &displayName,
                                                        const QString &author, const QString &type, const QUrl &source,
                                                        const QList<QmlThemeColorScheme> &colorSchemes)
{
    QmlThemeDescription result;
    result.id = id;
    result.displayName = displayName;
    result.author = author;
    result.type = type;
    result.version = QStringLiteral("core");
    result.source = source;
    result.colorSchemes.append(
        {QStringLiteral("System"), QCoreApplication::translate("@default", "System")});
    result.colorSchemes.append(colorSchemes);
    return result;
}

QmlThemeDescription QmlThemeDescriptionLoader::load(const QString &descriptorPath, const QString &id,
                                                     const QUrl &source, const QString &expectedType,
                                                     const QString &language)
{
    QSettings settings{descriptorPath, QSettings::IniFormat};
    settings.beginGroup(QStringLiteral("Theme"));

    QmlThemeDescription result;
    result.id = id;
    result.displayName = ThemeDescription::localizedValue(settings, QStringLiteral("DisplayName"), language);
    result.author = ThemeDescription::localizedValue(settings, QStringLiteral("Author"), language);
    result.type = settings.value(QStringLiteral("Type")).toString();
    result.version = settings.value(QStringLiteral("Version")).toString();
    result.source = source;
    settings.endGroup();

    if (result.displayName.isEmpty() || result.type != expectedType || !source.isValid())
        return {};

    result.colorSchemes.append(
        {QStringLiteral("System"), QCoreApplication::translate("@default", "System")});

    settings.beginGroup(QStringLiteral("Colors"));
    QSet<QString> schemeIds;
    for (const auto &key : settings.childKeys())
    {
        const auto schemeId = key.section(u'[', 0, 0);
        if (schemeId.isEmpty() || schemeId == QStringLiteral("System") || schemeIds.contains(schemeId))
            continue;

        schemeIds.insert(schemeId);
        result.colorSchemes.append({schemeId, ThemeDescription::localizedValue(settings, schemeId, language)});
    }
    settings.endGroup();

    return result;
}
