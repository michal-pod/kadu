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

ChatStyleManager::ChatStyleManager(QObject *parent) : QObject{parent} {}
ChatStyleManager::~ChatStyleManager() = default;

void ChatStyleManager::setConfiguration(Configuration *configuration)
{
    m_configuration = configuration;
}

void ChatStyleManager::init()
{
    loadStyles();
    configurationUpdated();
}

void ChatStyleManager::loadStyles()
{
    m_availableStyles = {
        {QStringLiteral("KaduClassic"), {tr("Classic Kadu")}},
        {QStringLiteral("Bubbles"), {tr("Bubbles")}},
    };
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
    const ChatStyle nextStyle{normalizedStyleName(configuredStyle), QString{}};
    if (nextStyle == m_currentChatStyle)
        return;

    m_currentChatStyle = nextStyle;
    emit chatStyleConfigurationUpdated();
}

bool ChatStyleManager::isChatStyleValid(const QString &name) const
{
    return m_availableStyles.contains(name);
}

StyleInfo ChatStyleManager::chatStyleInfo(const QString &name) const
{
    return m_availableStyles.value(name);
}
