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

#pragma once

#include "configuration/gui/configuration-ui-handler.h"

#include <QtCore/QPointer>
#include <injeqt/injeqt.h>

class ChatStyleManager;
class ChatTimelinePreview;
class Configuration;
class ConfigurationWidget;
class QComboBox;
class QCheckBox;
class QLabel;

class ChatStyleConfigurationUiHandler : public QObject, public ConfigurationUiHandler
{
    Q_OBJECT

public:
    Q_INVOKABLE explicit ChatStyleConfigurationUiHandler(QObject *parent = nullptr);
    ~ChatStyleConfigurationUiHandler() override;

protected:
    void mainConfigurationWindowCreated(MainConfigurationWindow *mainConfigurationWindow) override;
    void mainConfigurationWindowDestroyed() override;
    void mainConfigurationWindowApplied() override;

private:
    QPointer<Configuration> m_configuration;
    QPointer<ChatStyleManager> m_chatStyleManager;
    QComboBox *m_themeListCombo = nullptr;
    QComboBox *m_colorSchemeCombo = nullptr;
    QLabel *m_themeAuthor = nullptr;
    ChatTimelinePreview *m_themePreview = nullptr;
    QPointer<ConfigurationWidget> m_configurationWidget;
    QCheckBox *m_customColors = nullptr;
    QCheckBox *m_customBackground = nullptr;
    QCheckBox *m_customTextEditColors = nullptr;

private slots:
    INJEQT_SET void setChatStyleManager(ChatStyleManager *chatStyleManager);
    INJEQT_SET void setConfiguration(Configuration *configuration);
    void themeChanged(int index);
    void colorSchemeChanged(int index);

private:
    void updateThemeDetails();
    void updateCustomColorsAvailability();
};
