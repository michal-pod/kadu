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

#include "info-panel-style-manager.h"
#include "info-panel-style-manager.moc"

#include "configuration/configuration.h"
#include "configuration/deprecated-configuration-api.h"
#include "misc/paths-provider.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QLocale>

InfoPanelStyleManager::InfoPanelStyleManager(QObject *parent) : QObject{parent} {}
InfoPanelStyleManager::~InfoPanelStyleManager() = default;

void InfoPanelStyleManager::setConfiguration(Configuration *configuration)
{
    m_configuration = configuration;
}

void InfoPanelStyleManager::setPathsProvider(PathsProvider *pathsProvider)
{
    m_pathsProvider = pathsProvider;
}

void InfoPanelStyleManager::init()
{
    loadStyles();
}

QMap<QString, InfoPanelStyleInfo> InfoPanelStyleManager::availableStyles() const
{
    return m_styles;
}

QString InfoPanelStyleManager::normalizedStyleName(const QString &styleName) const
{
    return m_styles.contains(styleName) ? styleName : QStringLiteral("Classic");
}

bool InfoPanelStyleManager::isBuiltIn(const QString &styleName) const
{
    return m_styles.value(normalizedStyleName(styleName)).source.scheme() == QStringLiteral("qrc");
}

QList<QmlThemeColorScheme> InfoPanelStyleManager::colorSchemes(const QString &styleName) const
{
    return m_styles.value(normalizedStyleName(styleName)).colorSchemes;
}

QString InfoPanelStyleManager::normalizedColorScheme(const QString &styleName, const QString &scheme) const
{
    return m_styles.value(normalizedStyleName(styleName)).normalizedColorScheme(scheme);
}

QUrl InfoPanelStyleManager::styleSource(const QString &styleName) const
{
    return m_styles.value(normalizedStyleName(styleName)).source;
}

void InfoPanelStyleManager::loadStyles()
{
    m_styles.clear();

    const QList<QmlThemeColorScheme> bundledSchemes = {
        {QStringLiteral("Light"), tr("Light")},
        {QStringLiteral("Dark"), tr("Dark")},
    };
    m_styles = {
        {QStringLiteral("Classic"),
         QmlThemeDescriptionLoader::builtIn(QStringLiteral("Classic"), tr("Classic"), QStringLiteral("Kadu Team"),
                                             QStringLiteral("info-panel"),
                                             QUrl{QStringLiteral("qrc:/Kadu/Chat/widgets/qml/info-panel-styles/Classic.qml")},
                                             bundledSchemes)},
        {QStringLiteral("Compact"),
         QmlThemeDescriptionLoader::builtIn(QStringLiteral("Compact"), tr("Compact"), QStringLiteral("Kadu Team"),
                                             QStringLiteral("info-panel"),
                                             QUrl{QStringLiteral("qrc:/Kadu/Chat/widgets/qml/info-panel-styles/Compact.qml")},
                                             bundledSchemes)},
    };

    if (!m_pathsProvider)
        return;

    loadExternalStyles(m_pathsProvider->dataPath() + QStringLiteral("info-panel-styles"));
    loadExternalStyles(m_pathsProvider->profilePath() + QStringLiteral("info-panel-styles"));
}

void InfoPanelStyleManager::loadExternalStyles(const QString &directory)
{
    const QDir stylesDirectory{directory};
    const auto language = m_configuration
                              ? m_configuration->deprecatedApi()->readEntry("General", "Language")
                              : QLocale::system().name().left(2);
    for (const auto &entry : stylesDirectory.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
    {
        const auto source = QFileInfo{entry.filePath() + QStringLiteral("/BuddyInfoStyle.qml")};
        const auto descriptor = QFileInfo{entry.filePath() + QStringLiteral("/theme.desc")};
        if (!source.isFile() || !descriptor.isFile())
            continue;

        const auto id = entry.fileName();
        const auto style = QmlThemeDescriptionLoader::load(descriptor.absoluteFilePath(), id,
                                                           QUrl::fromLocalFile(source.absoluteFilePath()),
                                                           QStringLiteral("info-panel"), language);
        if (style.isValid())
            m_styles.insert(id, style);
    }
}
