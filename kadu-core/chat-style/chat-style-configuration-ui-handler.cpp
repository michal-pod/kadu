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

#include "chat-style-configuration-ui-handler.h"
#include "chat-style-configuration-ui-handler.moc"

#include "chat-style/chat-style-manager.h"
#include "configuration/configuration.h"
#include "configuration/deprecated-configuration-api.h"
#include "widgets/chat-timeline-preview.h"
#include "widgets/configuration/config-group-box.h"
#include "widgets/configuration/configuration-widget.h"
#include "windows/main-configuration-window.h"

#include <QtCore/QCoreApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLabel>

ChatStyleConfigurationUiHandler::ChatStyleConfigurationUiHandler(QObject *parent) : QObject{parent} {}
ChatStyleConfigurationUiHandler::~ChatStyleConfigurationUiHandler() = default;

void ChatStyleConfigurationUiHandler::setChatStyleManager(ChatStyleManager *chatStyleManager)
{
    m_chatStyleManager = chatStyleManager;
}

void ChatStyleConfigurationUiHandler::setConfiguration(Configuration *configuration)
{
    m_configuration = configuration;
}

void ChatStyleConfigurationUiHandler::mainConfigurationWindowCreated(MainConfigurationWindow *mainConfigurationWindow)
{
    if (!m_chatStyleManager)
        return;

    auto *groupBox = mainConfigurationWindow->widget()->configGroupBox("Look", "Chat", "Style");
    auto *label = new QLabel(QCoreApplication::translate("@default", "Chat theme") + ':', groupBox->widget());
    m_themeListCombo = new QComboBox(groupBox->widget());
    m_themeListCombo->setToolTip(QCoreApplication::translate("@default", "Choose the global QML theme of chat windows"));

    const auto styles = m_chatStyleManager->availableStyles();
    for (auto iterator = styles.cbegin(); iterator != styles.cend(); ++iterator)
        m_themeListCombo->addItem(iterator.value().displayName, iterator.key());

    m_themeListCombo->setCurrentIndex(m_themeListCombo->findData(m_chatStyleManager->currentChatStyle().name()));
    if (m_themeListCombo->currentIndex() < 0)
        m_themeListCombo->setCurrentIndex(0);

    m_themePreview = new ChatTimelinePreview(groupBox->widget());
    connect(m_themeListCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &ChatStyleConfigurationUiHandler::themeChanged);

    groupBox->addWidgets(label, m_themeListCombo);
    groupBox->addWidgets(new QLabel(QCoreApplication::translate("@default", "Preview") + ':', groupBox->widget()),
                         m_themePreview, Qt::AlignRight | Qt::AlignTop);
    themeChanged(m_themeListCombo->currentIndex());
}

void ChatStyleConfigurationUiHandler::mainConfigurationWindowDestroyed()
{
    m_themeListCombo = nullptr;
    m_themePreview = nullptr;
}

void ChatStyleConfigurationUiHandler::mainConfigurationWindowApplied()
{
    if (!m_configuration || !m_themeListCombo)
        return;

    m_configuration->deprecatedApi()->writeEntry("Look", "Style", m_themeListCombo->currentData().toString());
    m_configuration->deprecatedApi()->removeVariable("Look", "ChatStyleVariant");
}

void ChatStyleConfigurationUiHandler::themeChanged(int index)
{
    Q_UNUSED(index)

    if (m_themePreview && m_themeListCombo)
        m_themePreview->setTheme(m_themeListCombo->currentData().toString());
}
