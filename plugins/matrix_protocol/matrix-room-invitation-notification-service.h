/*
 * %kadu copyright begin%
 * Copyright 2026 Kadu Qt6 port
 * %kadu copyright end%
 */

#pragma once

#include "injeqt-type-roles.h"
#include "notification/notification-callback.h"
#include "notification/notification-event.h"

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <injeqt/injeqt.h>

class Account;
struct Notification;
class NotificationCallbackRepository;
class NotificationEventRepository;
class NotificationService;

namespace Quotient
{
class Room;
}

class MatrixRoomInvitationNotificationService final : public QObject
{
    Q_OBJECT
    INJEQT_TYPE_ROLE(SERVICE)

public:
    Q_INVOKABLE explicit MatrixRoomInvitationNotificationService(QObject *parent = nullptr);
    virtual ~MatrixRoomInvitationNotificationService();

    void notifyInvitation(const Account &account, Quotient::Room *room);

private:
    QPointer<NotificationCallbackRepository> m_notificationCallbackRepository;
    QPointer<NotificationEventRepository> m_notificationEventRepository;
    QPointer<NotificationService> m_notificationService;
    NotificationCallback m_joinCallback;
    NotificationCallback m_rejectCallback;
    NotificationCallback m_showDialogCallback;
    NotificationEvent m_matrixEvent;
    NotificationEvent m_roomInvitationEvent;

    void joinRoom(const Notification &notification);
    void rejectRoom(const Notification &notification);
    void showInvitationDialog(const Notification &notification);

private slots:
    INJEQT_SET void setNotificationCallbackRepository(NotificationCallbackRepository *notificationCallbackRepository);
    INJEQT_SET void setNotificationEventRepository(NotificationEventRepository *notificationEventRepository);
    INJEQT_SET void setNotificationService(NotificationService *notificationService);
    INJEQT_INIT void init();
    INJEQT_DONE void done();
};
