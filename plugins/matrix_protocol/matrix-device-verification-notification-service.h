/*
 * %kadu copyright begin%
 * Copyright 2026 Kadu Qt6 port
 * %kadu copyright end%
 */

#pragma once

#include "injeqt-type-roles.h"
#include "notification/notification-callback.h"
#include "notification/notification-event.h"

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <injeqt/injeqt.h>

class Account;
class MatrixDeviceVerificationDialog;
struct Notification;
class NotificationCallbackRepository;
class NotificationEventRepository;
class NotificationService;

namespace Quotient
{
class KeyVerificationSession;
}

class MatrixDeviceVerificationNotificationService final : public QObject
{
    Q_OBJECT
    INJEQT_TYPE_ROLE(SERVICE)

public:
    Q_INVOKABLE explicit MatrixDeviceVerificationNotificationService(QObject *parent = nullptr);
    ~MatrixDeviceVerificationNotificationService() override;

    void notifyVerificationRequest(const Account &account, Quotient::KeyVerificationSession *session);

private:
    QPointer<NotificationCallbackRepository> m_notificationCallbackRepository;
    QPointer<NotificationEventRepository> m_notificationEventRepository;
    QPointer<NotificationService> m_notificationService;
    QHash<QString, QPointer<Quotient::KeyVerificationSession>> m_sessions;
    QHash<QString, QPointer<MatrixDeviceVerificationDialog>> m_dialogs;
    NotificationCallback m_showDialogCallback;
    NotificationCallback m_rejectCallback;
    NotificationEvent m_deviceVerificationEvent;

    QString sessionKey(const Account &account, Quotient::KeyVerificationSession *session) const;
    Quotient::KeyVerificationSession *sessionFor(const Notification &notification) const;
    void showVerificationDialog(const Notification &notification);
    void rejectVerification(const Notification &notification);

private slots:
    INJEQT_SET void setNotificationCallbackRepository(NotificationCallbackRepository *notificationCallbackRepository);
    INJEQT_SET void setNotificationEventRepository(NotificationEventRepository *notificationEventRepository);
    INJEQT_SET void setNotificationService(NotificationService *notificationService);
    INJEQT_INIT void init();
    INJEQT_DONE void done();
};
