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

#include "chat-style/chat-style.h"
#include "configuration/configuration-aware-object.h"
#include "exports.h"
#include "themes/qml-theme-description.h"

#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QUrl>
#include <injeqt/injeqt.h>

class Configuration;
class PathsProvider;

using ChatStyleInfo = QmlThemeDescription;

/**
 * @short Global selector for native QML chat themes.
 *
 * The former manager discovered Adium/Kadu HTML renderers and made a renderer
 * factory. The renderer is no longer part of the executable; this manager
 * deliberately retains the configuration entry and resolves the QML timeline
 * component selected by a chat view. Built-in styles are packaged in Kadu.Chat.
 * Each external style is self-contained in a directory:
 * data/chat-styles/<style name>/ChatStyle.qml together with theme.desc, or the
 * corresponding directory below the user's profile. Its exact directory name,
 * including spaces, is its identifier in configuration. The descriptor carries
 * its localized label, author and supported colour schemes. The QML root
 * provides a writable colorScheme property, visual settings and a timelineItem
 * Component. Profile styles override system styles with the same ID.
 */
class KADUAPI ChatStyleManager : public QObject, ConfigurationAwareObject
{
    Q_OBJECT

    QPointer<Configuration> m_configuration;
    QPointer<PathsProvider> m_pathsProvider;
    ChatStyle m_currentChatStyle;
    QMap<QString, ChatStyleInfo> m_availableStyles;

    QString normalizedStyleName(const QString &styleName) const;
    void loadExternalStyles(const QString &directory);

private slots:
    INJEQT_SET void setConfiguration(Configuration *configuration);
    INJEQT_SET void setPathsProvider(PathsProvider *pathsProvider);
    INJEQT_INIT void init();

protected:
    void configurationUpdated() override;

public:
    Q_INVOKABLE explicit ChatStyleManager(QObject *parent = nullptr);
    ~ChatStyleManager() override;

    ChatStyle currentChatStyle() const { return m_currentChatStyle; }
    QMap<QString, ChatStyleInfo> availableStyles() const { return m_availableStyles; }
    bool hasChatStyle(const QString &name) const { return m_availableStyles.contains(name); }
    bool isChatStyleValid(const QString &name) const;
    bool isBuiltIn(const QString &name) const;
    ChatStyleInfo chatStyleInfo(const QString &name) const;
    QList<QmlThemeColorScheme> colorSchemes(const QString &name) const;
    QString normalizedColorScheme(const QString &styleName, const QString &scheme) const;
    QUrl styleSource(const QString &name) const;

    void loadStyles();

signals:
    void chatStyleConfigurationUpdated();
};
