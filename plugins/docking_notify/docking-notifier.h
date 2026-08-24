#pragma once

#include "notification/notification.h"

#include <QtCore/QObject>
#include <optional>
#include "notification/notifier.h"
#include "windows/main-configuration-window.h"

#include "docking-notify-configuration-widget.h"

#include <injeqt/injeqt.h>

class Configuration;
class Docking;
class PluginInjectedFactory;
class NormalizedHtmlString;
class NotificationConfiguration;
class NotificationService;
class Parser;

/**
 * @defgroup qt4_notify Qt4 Notify
 * @{
 */
class DockingNotifier : public QObject, public Notifier
{
    Q_OBJECT

public:
    Q_INVOKABLE explicit DockingNotifier(QObject *parent = nullptr);
    virtual ~DockingNotifier();

    virtual void notify(const Notification &notification);

    virtual NotifierConfigurationWidget *createConfigurationWidget(QWidget *parent = nullptr);

public slots:
    void messageClicked();

private:
    QPointer<Configuration> m_configuration;
    QPointer<Docking> m_docking;
    QPointer<PluginInjectedFactory> m_pluginInjectedFactory;
    QPointer<NotificationConfiguration> m_notificationConfiguration;
    QPointer<NotificationService> m_notificationService;
    QPointer<Parser> m_parser;
    std::optional<Notification> m_pendingNotification;

    void createDefaultConfiguration();
    QString parseText(const QString &text, const Notification &notification, const NormalizedHtmlString &def);

private slots:
    INJEQT_SET void setConfiguration(Configuration *configuration);
    INJEQT_SET void setDocking(Docking *docking);
    INJEQT_SET void setPluginInjectedFactory(PluginInjectedFactory *pluginInjectedFactory);
    INJEQT_SET void setNotificationConfiguration(NotificationConfiguration *notificationConfiguration);
    INJEQT_SET void setNotificationService(NotificationService *notificationService);
    INJEQT_SET void setParser(Parser *parser);
    INJEQT_INIT void init();
};

/** @} */
