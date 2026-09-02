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

#include "chat-style-manager.h"
#include "chat-style-manager.moc"

#include "configuration/configuration.h"
#include "configuration/deprecated-configuration-api.h"
#include "misc/paths-provider.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QLocale>

ChatStyleManager::ChatStyleManager(QObject *parent) : QObject{parent} {}
ChatStyleManager::~ChatStyleManager() = default;

void ChatStyleManager::setConfiguration(Configuration *configuration)
{
    m_configuration = configuration;
}

void ChatStyleManager::setPathsProvider(PathsProvider *pathsProvider)
{
    m_pathsProvider = pathsProvider;
}

void ChatStyleManager::init()
{
    loadStyles();
    configurationUpdated();
}

void ChatStyleManager::loadStyles()
{
    m_availableStyles.clear();

    const QList<QmlThemeColorScheme> bundledSchemes = {
        {QStringLiteral("Light"), tr("Light")},
        {QStringLiteral("Dark"), tr("Dark")},
    };
    const QList<QmlThemeColorScheme> irssiSchemes = {
        {QStringLiteral("Irssi"), tr("Irssi")},
    };
    m_availableStyles = {
        {QStringLiteral("KaduClassic"),
         QmlThemeDescriptionLoader::builtIn(QStringLiteral("KaduClassic"), tr("Classic Kadu"),
                                             QStringLiteral("Kadu Team"), QStringLiteral("chat"),
                                             QUrl{QStringLiteral("qrc:/Kadu/Chat/chat/qml/styles/KaduClassic/KaduClassicChatStyle.qml")},
                                             bundledSchemes)},
        {QStringLiteral("Bubbles"),
         QmlThemeDescriptionLoader::builtIn(QStringLiteral("Bubbles"), tr("Bubbles"), QStringLiteral("Kadu Team"),
                                             QStringLiteral("chat"),
                                             QUrl{QStringLiteral("qrc:/Kadu/Chat/chat/qml/styles/Bubbles/BubblesChatStyle.qml")},
                                             bundledSchemes)},
        {QStringLiteral("Irssi"),
         QmlThemeDescriptionLoader::builtIn(QStringLiteral("Irssi"), tr("Irssi"), QStringLiteral("Kadu Team"),
                                             QStringLiteral("chat"),
                                             QUrl{QStringLiteral("qrc:/Kadu/Chat/chat/qml/styles/Irssi/IrssiChatStyle.qml")},
                                             irssiSchemes, false)},
    };

    if (!m_pathsProvider)
        return;

    loadExternalStyles(m_pathsProvider->dataPath() + QStringLiteral("chat-styles"));
    loadExternalStyles(m_pathsProvider->profilePath() + QStringLiteral("chat-styles"));
}

QString ChatStyleManager::normalizedStyleName(const QString &styleName) const
{
    if (m_availableStyles.contains(styleName))
        return styleName;

    // Legacy Adium styles are deliberately migrated to the native default.
    return QStringLiteral("KaduClassic");
}

void ChatStyleManager::configurationUpdated()
{
    if (m_availableStyles.isEmpty())
        loadStyles();

    const auto configuredStyle = m_configuration
                                     ? m_configuration->deprecatedApi()->readEntry("Look", "Style", "KaduClassic")
                                     : QStringLiteral("KaduClassic");
    const auto styleName = normalizedStyleName(configuredStyle);
    const auto configuredScheme = m_configuration
                                      ? m_configuration->deprecatedApi()->readEntry("Look", "ChatStyleVariant", "System")
                                      : QStringLiteral("System");
    const ChatStyle nextStyle{styleName, normalizedColorScheme(styleName, configuredScheme)};
    if (nextStyle == m_currentChatStyle)
        return;

    m_currentChatStyle = nextStyle;
    emit chatStyleConfigurationUpdated();
}

bool ChatStyleManager::isChatStyleValid(const QString &name) const
{
    return m_availableStyles.contains(name);
}

bool ChatStyleManager::isBuiltIn(const QString &name) const
{
    return m_availableStyles.value(normalizedStyleName(name)).source.scheme() == QStringLiteral("qrc");
}

ChatStyleInfo ChatStyleManager::chatStyleInfo(const QString &name) const
{
    return m_availableStyles.value(name);
}

QList<QmlThemeColorScheme> ChatStyleManager::colorSchemes(const QString &name) const
{
    return m_availableStyles.value(normalizedStyleName(name)).colorSchemes;
}

QString ChatStyleManager::normalizedColorScheme(const QString &styleName, const QString &scheme) const
{
    return m_availableStyles.value(normalizedStyleName(styleName)).normalizedColorScheme(scheme);
}

QUrl ChatStyleManager::styleSource(const QString &name) const
{
    return m_availableStyles.value(normalizedStyleName(name)).source;
}

void ChatStyleManager::loadExternalStyles(const QString &directory)
{
    const QDir stylesDirectory{directory};
    const auto language = m_configuration
                              ? m_configuration->deprecatedApi()->readEntry("General", "Language")
                              : QLocale::system().name().left(2);
    for (const auto &entry : stylesDirectory.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
    {
        const auto descriptor = QFileInfo{entry.filePath() + QStringLiteral("/theme.desc")};
        if (!descriptor.isFile())
            continue;

        const auto componentFileName =
            QmlThemeDescriptionLoader::mainComponentFileName(descriptor.absoluteFilePath(), QStringLiteral("ChatStyle.qml"));
        const auto source = QFileInfo{entry.filePath() + QLatin1Char{'/'} + componentFileName};
        if (componentFileName.isEmpty() || !source.isFile())
            continue;

        const auto id = entry.fileName();
        const auto style = QmlThemeDescriptionLoader::load(descriptor.absoluteFilePath(), id,
                                                           QUrl::fromLocalFile(source.absoluteFilePath()),
                                                           QStringLiteral("chat"), language);
        if (style.isValid())
            m_availableStyles.insert(id, style);
    }
}
