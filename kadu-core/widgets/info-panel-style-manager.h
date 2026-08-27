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

#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtCore/QUrl>
#include <injeqt/injeqt.h>

class PathsProvider;

struct InfoPanelStyleInfo
{
    QString displayName;
    QUrl source;
};

/**
 * @short Discovers QML information-panel styles.
 *
 * Built-in styles are packaged in the Kadu.Chat QML module. External styles
 * are directories containing style.json and the QML file named by its qml
 * property. System styles live in data/info-panel-styles and profile styles in
 * info-panel-styles below the user's profile; profile styles override system
 * styles with the same identifier. The QML root has to expose an infoPanel
 * property plus selectedText and copySelection() for the shell's copy action.
 */
class KADUAPI InfoPanelStyleManager : public QObject
{
    Q_OBJECT

public:
    Q_INVOKABLE explicit InfoPanelStyleManager(QObject *parent = nullptr);
    ~InfoPanelStyleManager() override;

    QMap<QString, InfoPanelStyleInfo> availableStyles() const;
    QString normalizedStyleName(const QString &styleName) const;
    QUrl styleSource(const QString &styleName) const;

    void loadStyles();

private:
    QPointer<PathsProvider> m_pathsProvider;
    QMap<QString, InfoPanelStyleInfo> m_styles;

    void loadExternalStyles(const QString &directory);

private slots:
    INJEQT_SET void setPathsProvider(PathsProvider *pathsProvider);
    INJEQT_INIT void init();
};
