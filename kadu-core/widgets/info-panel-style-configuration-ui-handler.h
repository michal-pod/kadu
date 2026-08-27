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

#include "configuration/gui/configuration-ui-handler.h"

#include <QtCore/QPointer>
#include <injeqt/injeqt.h>

class Configuration;
class ConfigurationWidget;
class InfoPanelStyleManager;
class Preview;
class QComboBox;
class QCheckBox;
class QLabel;

class InfoPanelStyleConfigurationUiHandler : public QObject, public ConfigurationUiHandler
{
    Q_OBJECT

public:
    Q_INVOKABLE explicit InfoPanelStyleConfigurationUiHandler(QObject *parent = nullptr);
    ~InfoPanelStyleConfigurationUiHandler() override;

protected:
    void mainConfigurationWindowCreated(MainConfigurationWindow *mainConfigurationWindow) override;
    void mainConfigurationWindowDestroyed() override;
    void mainConfigurationWindowApplied() override;

private:
    QPointer<Configuration> m_configuration;
    QPointer<InfoPanelStyleManager> m_infoPanelStyleManager;
    QComboBox *m_styleCombo = nullptr;
    QComboBox *m_colorSchemeCombo = nullptr;
    QLabel *m_styleAuthor = nullptr;
    Preview *m_stylePreview = nullptr;
    QPointer<ConfigurationWidget> m_configurationWidget;
    QCheckBox *m_customColors = nullptr;
    QCheckBox *m_customBackground = nullptr;

private slots:
    INJEQT_SET void setConfiguration(Configuration *configuration);
    INJEQT_SET void setInfoPanelStyleManager(InfoPanelStyleManager *infoPanelStyleManager);
    void styleChanged(int index);
    void colorSchemeChanged(int index);

private:
    void updateStyleDetails();
    void updateCustomColorsAvailability();
};
