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

#include "chat-style/chat-style.h"
#include "configuration/configuration-aware-object.h"
#include "exports.h"

#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <injeqt/injeqt.h>

class Configuration;

struct StyleInfo
{
    QString displayName;
};

/**
 * @short Global selector for native QML chat themes.
 *
 * The former manager discovered Adium/Kadu HTML renderers and made a renderer
 * factory. The renderer is no longer part of the executable; this manager
 * deliberately retains the configuration entry and exposes only built-in QML
 * theme identifiers to chat views.
 */
class KADUAPI ChatStyleManager : public QObject, ConfigurationAwareObject
{
    Q_OBJECT

    QPointer<Configuration> m_configuration;
    ChatStyle m_currentChatStyle;
    QMap<QString, StyleInfo> m_availableStyles;

    QString normalizedStyleName(const QString &styleName) const;

private slots:
    INJEQT_SET void setConfiguration(Configuration *configuration);
    INJEQT_INIT void init();

protected:
    void configurationUpdated() override;

public:
    Q_INVOKABLE explicit ChatStyleManager(QObject *parent = nullptr);
    ~ChatStyleManager() override;

    ChatStyle currentChatStyle() const { return m_currentChatStyle; }
    QMap<QString, StyleInfo> availableStyles() const { return m_availableStyles; }
    bool hasChatStyle(const QString &name) const { return m_availableStyles.contains(name); }
    bool isChatStyleValid(const QString &name) const;
    StyleInfo chatStyleInfo(const QString &name) const;

    void loadStyles();

signals:
    void chatStyleConfigurationUpdated();
};
