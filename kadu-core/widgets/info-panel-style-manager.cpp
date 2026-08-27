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

#include "misc/paths-provider.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QRegularExpression>

InfoPanelStyleManager::InfoPanelStyleManager(QObject *parent) : QObject{parent} {}
InfoPanelStyleManager::~InfoPanelStyleManager() = default;

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

QUrl InfoPanelStyleManager::styleSource(const QString &styleName) const
{
    return m_styles.value(normalizedStyleName(styleName)).source;
}

void InfoPanelStyleManager::loadStyles()
{
    m_styles = {
        {QStringLiteral("Classic"), {tr("Classic"), QUrl{QStringLiteral("qrc:/Kadu/Chat/widgets/qml/info-panel-styles/Classic.qml")}}},
        {QStringLiteral("Compact"), {tr("Compact"), QUrl{QStringLiteral("qrc:/Kadu/Chat/widgets/qml/info-panel-styles/Compact.qml")}}},
    };

    if (!m_pathsProvider)
        return;

    loadExternalStyles(m_pathsProvider->dataPath() + QStringLiteral("info-panel-styles"));
    loadExternalStyles(m_pathsProvider->profilePath() + QStringLiteral("info-panel-styles"));
}

void InfoPanelStyleManager::loadExternalStyles(const QString &directory)
{
    const QDir stylesDirectory{directory};
    for (const auto &entry : stylesDirectory.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
    {
        QFile manifest{entry.filePath() + QStringLiteral("/style.json")};
        if (!manifest.open(QIODevice::ReadOnly))
            continue;

        const auto document = QJsonDocument::fromJson(manifest.readAll());
        const auto object = document.object();
        const auto id = object.value(QStringLiteral("id")).toString();
        const auto name = object.value(QStringLiteral("name")).toString();
        const auto qmlFile = object.value(QStringLiteral("qml")).toString(QStringLiteral("InfoPanelStyle.qml"));
        const auto source = QFileInfo{entry.filePath() + QLatin1Char('/') + qmlFile};

        if (!QRegularExpression{QStringLiteral("^[A-Za-z0-9_.-]+$")}.match(id).hasMatch() || name.isEmpty() ||
            !source.isFile())
            continue;

        m_styles.insert(id, {name, QUrl::fromLocalFile(source.absoluteFilePath())});
    }
}
