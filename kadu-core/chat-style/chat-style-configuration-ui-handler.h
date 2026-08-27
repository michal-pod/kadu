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
class QComboBox;

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
    ChatTimelinePreview *m_themePreview = nullptr;

private slots:
    INJEQT_SET void setChatStyleManager(ChatStyleManager *chatStyleManager);
    INJEQT_SET void setConfiguration(Configuration *configuration);
    void themeChanged(int index);
};
