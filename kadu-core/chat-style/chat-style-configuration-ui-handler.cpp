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
#include <QtCore/QSignalBlocker>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLabel>

ChatStyleConfigurationUiHandler::ChatStyleConfigurationUiHandler(QObject *parent) : QObject{parent}
{
}
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

    m_configurationWidget = mainConfigurationWindow->widget();
    auto *groupBox = mainConfigurationWindow->widget()->configGroupBox("Look", "Chat", "Style");
    auto *label = new QLabel(QCoreApplication::translate("@default", "Chat theme") + ':', groupBox->widget());
    m_themeListCombo = new QComboBox(groupBox->widget());
    m_themeListCombo->setToolTip(
        QCoreApplication::translate("@default", "Choose the global QML theme of chat windows"));
    auto *colorSchemeLabel =
        new QLabel(QCoreApplication::translate("@default", "Color scheme") + ':', groupBox->widget());
    m_colorSchemeCombo = new QComboBox(groupBox->widget());
    m_themeAuthor = new QLabel(groupBox->widget());

    const auto styles = m_chatStyleManager->availableStyles();
    for (auto iterator = styles.cbegin(); iterator != styles.cend(); ++iterator)
        m_themeListCombo->addItem(iterator.value().displayName, iterator.key());

    m_themeListCombo->setCurrentIndex(m_themeListCombo->findData(m_chatStyleManager->currentChatStyle().name()));
    if (m_themeListCombo->currentIndex() < 0)
        m_themeListCombo->setCurrentIndex(0);

    m_themePreview = new ChatTimelinePreview(groupBox->widget());
    m_customColors = qobject_cast<QCheckBox *>(m_configurationWidget->widgetById("chatCustomColors"));
    m_customBackground = qobject_cast<QCheckBox *>(m_configurationWidget->widgetById("chatBgFilled"));
    m_customTextEditColors = qobject_cast<QCheckBox *>(m_configurationWidget->widgetById("chatTextCustomColors"));
    connect(
        m_themeListCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &ChatStyleConfigurationUiHandler::themeChanged);
    connect(
        m_colorSchemeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &ChatStyleConfigurationUiHandler::colorSchemeChanged);
    if (m_customColors)
        connect(
            m_customColors, &QCheckBox::toggled, this,
            &ChatStyleConfigurationUiHandler::updateCustomColorsAvailability);
    if (m_customBackground)
        connect(
            m_customBackground, &QCheckBox::toggled, this,
            &ChatStyleConfigurationUiHandler::updateCustomColorsAvailability);
    if (m_customTextEditColors)
        connect(
            m_customTextEditColors, &QCheckBox::toggled, this,
            &ChatStyleConfigurationUiHandler::updateCustomColorsAvailability);

    groupBox->addWidgets(label, m_themeListCombo);
    groupBox->addWidgets(colorSchemeLabel, m_colorSchemeCombo);
    groupBox->addWidgets(
        new QLabel(QCoreApplication::translate("@default", "Author") + ':', groupBox->widget()), m_themeAuthor);
    groupBox->addWidgets(
        new QLabel(QCoreApplication::translate("@default", "Preview") + ':', groupBox->widget()), m_themePreview,
        Qt::AlignRight | Qt::AlignTop);
    themeChanged(m_themeListCombo->currentIndex());
}

void ChatStyleConfigurationUiHandler::mainConfigurationWindowDestroyed()
{
    m_themeListCombo = nullptr;
    m_colorSchemeCombo = nullptr;
    m_themeAuthor = nullptr;
    m_themePreview = nullptr;
    m_configurationWidget = nullptr;
    m_customColors = nullptr;
    m_customBackground = nullptr;
    m_customTextEditColors = nullptr;
}

void ChatStyleConfigurationUiHandler::mainConfigurationWindowApplied()
{
    if (!m_configuration || !m_themeListCombo)
        return;

    m_configuration->deprecatedApi()->writeEntry("Look", "Style", m_themeListCombo->currentData().toString());
    m_configuration->deprecatedApi()->writeEntry(
        "Look", "ChatStyleVariant",
        m_colorSchemeCombo ? m_colorSchemeCombo->currentData().toString() : QStringLiteral("System"));
}

void ChatStyleConfigurationUiHandler::themeChanged(int index)
{
    Q_UNUSED(index)

    updateThemeDetails();
}

void ChatStyleConfigurationUiHandler::colorSchemeChanged(int index)
{
    Q_UNUSED(index)

    if (m_themePreview && m_colorSchemeCombo)
        m_themePreview->setColorScheme(m_colorSchemeCombo->currentData().toString());
}

void ChatStyleConfigurationUiHandler::updateThemeDetails()
{
    if (!m_chatStyleManager || !m_themeListCombo)
        return;

    const auto styleName = m_themeListCombo->currentData().toString();
    const auto style = m_chatStyleManager->chatStyleInfo(styleName);
    if (m_themeAuthor)
        m_themeAuthor->setText(
            style.author.isEmpty() ? QCoreApplication::translate("@default", "Unknown") : style.author);

    if (m_colorSchemeCombo)
    {
        const QSignalBlocker blocker{m_colorSchemeCombo};
        m_colorSchemeCombo->clear();
        for (const auto &scheme : m_chatStyleManager->colorSchemes(styleName))
            m_colorSchemeCombo->addItem(scheme.displayName, scheme.id);

        const auto currentScheme = m_chatStyleManager->currentChatStyle().name() == styleName
                                       ? m_chatStyleManager->currentChatStyle().variant()
                                       : QStringLiteral("System");
        m_colorSchemeCombo->setCurrentIndex(m_colorSchemeCombo->findData(currentScheme));
        if (m_colorSchemeCombo->currentIndex() < 0)
            m_colorSchemeCombo->setCurrentIndex(0);
    }

    if (m_themePreview)
    {
        m_themePreview->setThemeSource(m_chatStyleManager->styleSource(m_themeListCombo->currentData().toString()));
        m_themePreview->setColorScheme(
            m_colorSchemeCombo ? m_colorSchemeCombo->currentData().toString() : QStringLiteral("System"));
    }

    updateCustomColorsAvailability();
}

void ChatStyleConfigurationUiHandler::updateCustomColorsAvailability()
{
    if (!m_configurationWidget || !m_themeListCombo)
        return;

    const auto builtIn =
        m_chatStyleManager && m_chatStyleManager->isBuiltIn(m_themeListCombo->currentData().toString());
    const auto customColorsEnabled = builtIn && m_customColors && m_customColors->isChecked();
    const auto customBackgroundEnabled = customColorsEnabled && m_customBackground && m_customBackground->isChecked();
    const auto customTextEditColorsEnabled =
        customColorsEnabled && m_customTextEditColors && m_customTextEditColors->isChecked();

    if (m_customColors)
        m_customColors->setEnabled(builtIn);
    if (m_colorSchemeCombo)
        m_colorSchemeCombo->setEnabled(!customColorsEnabled);

    const auto setEnabled = [this](const QString &id, bool enabled) {
        if (auto *widget = m_configurationWidget->widgetById(id))
            widget->setEnabled(enabled);
    };
    setEnabled(QStringLiteral("chatMyFontColor"), customColorsEnabled);
    setEnabled(QStringLiteral("chatUsrFontColor"), customColorsEnabled);
    setEnabled(QStringLiteral("chatMyNickColor"), customColorsEnabled);
    setEnabled(QStringLiteral("chatUsrNickColor"), customColorsEnabled);
    setEnabled(QStringLiteral("chatMyBgColor"), customColorsEnabled);
    setEnabled(QStringLiteral("chatUsrBgColor"), customColorsEnabled);
    setEnabled(QStringLiteral("chatBgFilled"), customColorsEnabled);
    setEnabled(QStringLiteral("chatBgColor"), customBackgroundEnabled);
    setEnabled(QStringLiteral("chatTextCustomColors"), customColorsEnabled);
    setEnabled(QStringLiteral("chatTextBgColor"), customTextEditColorsEnabled);
    setEnabled(QStringLiteral("chatTextFontColor"), customTextEditColorsEnabled);
}
