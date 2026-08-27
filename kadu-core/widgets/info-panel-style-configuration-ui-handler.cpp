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

#include "info-panel-style-configuration-ui-handler.h"
#include "info-panel-style-configuration-ui-handler.moc"

#include "configuration/configuration.h"
#include "configuration/deprecated-configuration-api.h"
#include "widgets/configuration/config-group-box.h"
#include "widgets/configuration/configuration-widget.h"
#include "widgets/info-panel-style-manager.h"
#include "widgets/preview.h"
#include "windows/main-configuration-window.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QSignalBlocker>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QLabel>

InfoPanelStyleConfigurationUiHandler::InfoPanelStyleConfigurationUiHandler(QObject *parent) : QObject{parent} {}
InfoPanelStyleConfigurationUiHandler::~InfoPanelStyleConfigurationUiHandler() = default;

void InfoPanelStyleConfigurationUiHandler::setConfiguration(Configuration *configuration)
{
    m_configuration = configuration;
}

void InfoPanelStyleConfigurationUiHandler::setInfoPanelStyleManager(InfoPanelStyleManager *infoPanelStyleManager)
{
    m_infoPanelStyleManager = infoPanelStyleManager;
}

void InfoPanelStyleConfigurationUiHandler::mainConfigurationWindowCreated(MainConfigurationWindow *mainConfigurationWindow)
{
    if (!m_infoPanelStyleManager)
        return;

    m_configurationWidget = mainConfigurationWindow->widget();
    auto *groupBox = mainConfigurationWindow->widget()->configGroupBox("Buddies list", "Information", "Information Panel");
    auto *styleLabel = new QLabel(QCoreApplication::translate("@default", "Information panel theme") + ':', groupBox->widget());
    m_styleCombo = new QComboBox(groupBox->widget());
    m_styleCombo->setToolTip(QCoreApplication::translate("@default", "Choose the global QML theme of the information panel"));
    auto *colorSchemeLabel = new QLabel(QCoreApplication::translate("@default", "Color scheme") + ':', groupBox->widget());
    m_colorSchemeCombo = new QComboBox(groupBox->widget());
    m_styleAuthor = new QLabel(groupBox->widget());

    const auto styles = m_infoPanelStyleManager->availableStyles();
    for (auto iterator = styles.cbegin(); iterator != styles.cend(); ++iterator)
        m_styleCombo->addItem(iterator.value().displayName, iterator.key());

    const auto configuredStyle = m_configuration
                                     ? m_configuration->deprecatedApi()->readEntry("Look", "InfoPanelStyle", "Classic")
                                     : QStringLiteral("Classic");
    m_styleCombo->setCurrentIndex(
        m_styleCombo->findData(m_infoPanelStyleManager->normalizedStyleName(configuredStyle)));
    if (m_styleCombo->currentIndex() < 0)
        m_styleCombo->setCurrentIndex(0);

    m_stylePreview = new Preview(groupBox->widget());
    m_customColors = qobject_cast<QCheckBox *>(m_configurationWidget->widgetById("infoPanelCustomColors"));
    m_customBackground = qobject_cast<QCheckBox *>(m_configurationWidget->widgetById("infoPanelBgFilled"));
    connect(m_styleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &InfoPanelStyleConfigurationUiHandler::styleChanged);
    connect(m_colorSchemeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &InfoPanelStyleConfigurationUiHandler::colorSchemeChanged);
    if (m_customColors)
        connect(m_customColors, &QCheckBox::toggled, this,
                &InfoPanelStyleConfigurationUiHandler::updateCustomColorsAvailability);
    if (m_customBackground)
        connect(m_customBackground, &QCheckBox::toggled, this,
                &InfoPanelStyleConfigurationUiHandler::updateCustomColorsAvailability);

    groupBox->addWidgets(styleLabel, m_styleCombo);
    groupBox->addWidgets(colorSchemeLabel, m_colorSchemeCombo);
    groupBox->addWidgets(new QLabel(QCoreApplication::translate("@default", "Author") + ':', groupBox->widget()),
                         m_styleAuthor);
    groupBox->addWidgets(new QLabel(QCoreApplication::translate("@default", "Preview") + ':', groupBox->widget()),
                         m_stylePreview, Qt::AlignRight | Qt::AlignTop);
    styleChanged(m_styleCombo->currentIndex());
}

void InfoPanelStyleConfigurationUiHandler::mainConfigurationWindowDestroyed()
{
    m_styleCombo = nullptr;
    m_colorSchemeCombo = nullptr;
    m_styleAuthor = nullptr;
    m_stylePreview = nullptr;
    m_configurationWidget = nullptr;
    m_customColors = nullptr;
    m_customBackground = nullptr;
}

void InfoPanelStyleConfigurationUiHandler::mainConfigurationWindowApplied()
{
    if (!m_configuration || !m_styleCombo)
        return;

    m_configuration->deprecatedApi()->writeEntry("Look", "InfoPanelStyle", m_styleCombo->currentData().toString());
    m_configuration->deprecatedApi()->writeEntry("Look", "InfoPanelStyleVariant",
                                                  m_colorSchemeCombo ? m_colorSchemeCombo->currentData().toString()
                                                                     : QStringLiteral("System"));
}

void InfoPanelStyleConfigurationUiHandler::styleChanged(int index)
{
    Q_UNUSED(index)

    updateStyleDetails();
}

void InfoPanelStyleConfigurationUiHandler::colorSchemeChanged(int index)
{
    Q_UNUSED(index)

    if (m_stylePreview && m_colorSchemeCombo)
        m_stylePreview->setColorScheme(m_colorSchemeCombo->currentData().toString());
}

void InfoPanelStyleConfigurationUiHandler::updateStyleDetails()
{
    if (!m_infoPanelStyleManager || !m_styleCombo)
        return;

    const auto styleName = m_styleCombo->currentData().toString();
    const auto style = m_infoPanelStyleManager->availableStyles().value(styleName);
    if (m_styleAuthor)
        m_styleAuthor->setText(style.author.isEmpty() ? QCoreApplication::translate("@default", "Unknown") : style.author);

    if (m_colorSchemeCombo)
    {
        const QSignalBlocker blocker{m_colorSchemeCombo};
        m_colorSchemeCombo->clear();
        for (const auto &scheme : m_infoPanelStyleManager->colorSchemes(styleName))
            m_colorSchemeCombo->addItem(scheme.displayName, scheme.id);

        const auto configuredStyle = m_configuration
                                         ? m_infoPanelStyleManager->normalizedStyleName(
                                               m_configuration->deprecatedApi()->readEntry("Look", "InfoPanelStyle", "Classic"))
                                         : QStringLiteral("Classic");
        const auto configuredScheme = configuredStyle == styleName && m_configuration
                                          ? m_configuration->deprecatedApi()->readEntry("Look", "InfoPanelStyleVariant", "System")
                                          : QStringLiteral("System");
        m_colorSchemeCombo->setCurrentIndex(m_colorSchemeCombo->findData(configuredScheme));
        if (m_colorSchemeCombo->currentIndex() < 0)
            m_colorSchemeCombo->setCurrentIndex(0);
    }

    if (m_stylePreview)
    {
        m_stylePreview->setStyleSource(m_infoPanelStyleManager->styleSource(styleName));
        m_stylePreview->setColorScheme(m_colorSchemeCombo ? m_colorSchemeCombo->currentData().toString()
                                                           : QStringLiteral("System"));
    }

    updateCustomColorsAvailability();
}

void InfoPanelStyleConfigurationUiHandler::updateCustomColorsAvailability()
{
    if (!m_configurationWidget || !m_styleCombo)
        return;

    const auto builtIn = m_infoPanelStyleManager && m_infoPanelStyleManager->isBuiltIn(m_styleCombo->currentData().toString());
    const auto customColorsEnabled = builtIn && m_customColors && m_customColors->isChecked();
    const auto customBackgroundEnabled = customColorsEnabled && m_customBackground && m_customBackground->isChecked();

    if (m_customColors)
        m_customColors->setEnabled(builtIn);
    if (m_colorSchemeCombo)
        m_colorSchemeCombo->setEnabled(!customColorsEnabled);

    const auto setEnabled = [this](const QString &id, bool enabled) {
        if (auto *widget = m_configurationWidget->widgetById(id))
            widget->setEnabled(enabled);
    };
    setEnabled(QStringLiteral("infoPanelFgColor"), customColorsEnabled);
    setEnabled(QStringLiteral("infoPanelBgFilled"), customColorsEnabled);
    setEnabled(QStringLiteral("infoPanelBgColor"), customBackgroundEnabled);
}
