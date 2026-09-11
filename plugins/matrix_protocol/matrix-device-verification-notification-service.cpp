/*
 * %kadu copyright begin%
 * Copyright 2026 Kadu Qt6 port
 * %kadu copyright end%
 */

#include "matrix-device-verification-notification-service.h"
#include "matrix-device-verification-notification-service.moc"

#include "accounts/account.h"
#include "gui/matrix-device-verification-dialog.h"
#include "html/html-conversion.h"
#include "html/html-string.h"
#include "notification/notification-callback-repository.h"
#include "notification/notification-event-repository.h"
#include "notification/notification-service.h"
#include "notification/notification.h"

#include <Quotient/keyverificationsession.h>
#include <QtCore/QUuid>

#include <utility>

MatrixDeviceVerificationNotificationService::MatrixDeviceVerificationNotificationService(QObject *parent)
        : QObject{parent},
          m_showDialogCallback{QStringLiteral("matrix-device-verification-show-dialog"), tr("Show verification"),
                               [this](const Notification &notification) { showVerificationDialog(notification); }},
          m_rejectCallback{QStringLiteral("matrix-device-verification-reject"), tr("Reject verification"),
                           [this](const Notification &notification) { rejectVerification(notification); }},
          m_deviceVerificationEvent{QStringLiteral("Matrix/DeviceVerification"),
                                    QStringLiteral(QT_TRANSLATE_NOOP("@default", "Matrix device verification"))}
{
}

MatrixDeviceVerificationNotificationService::~MatrixDeviceVerificationNotificationService()
{
}

void MatrixDeviceVerificationNotificationService::setNotificationCallbackRepository(
    NotificationCallbackRepository *notificationCallbackRepository)
{
    m_notificationCallbackRepository = notificationCallbackRepository;
}

void MatrixDeviceVerificationNotificationService::setNotificationEventRepository(
    NotificationEventRepository *notificationEventRepository)
{
    m_notificationEventRepository = notificationEventRepository;
}

void MatrixDeviceVerificationNotificationService::setNotificationService(NotificationService *notificationService)
{
    m_notificationService = notificationService;
}

void MatrixDeviceVerificationNotificationService::init()
{
    m_notificationEventRepository->addNotificationEvent(m_deviceVerificationEvent);
    m_notificationCallbackRepository->addCallback(m_showDialogCallback);
    m_notificationCallbackRepository->addCallback(m_rejectCallback);
}

void MatrixDeviceVerificationNotificationService::done()
{
    m_notificationEventRepository->removeNotificationEvent(m_deviceVerificationEvent);
    m_notificationCallbackRepository->removeCallback(m_showDialogCallback);
    m_notificationCallbackRepository->removeCallback(m_rejectCallback);
}

void MatrixDeviceVerificationNotificationService::notifyVerificationRequest(
    const Account &account, Quotient::KeyVerificationSession *session)
{
    if (!account || !session || session->state() != Quotient::KeyVerificationSession::INCOMING || !m_notificationService)
        return;

    const auto key = sessionKey(account, session);
    if (m_sessions.value(key) == session)
        return;
    m_sessions.insert(key, session);
    connect(session, &QObject::destroyed, this, [this, key, session] {
        if (!m_sessions.value(key) || m_sessions.value(key) == session)
            m_sessions.remove(key);
    });

    auto notification = Notification{};
    notification.type = m_deviceVerificationEvent.name();
    notification.title = tr("Matrix device verification");
    notification.text = normalizeHtml(
        plainToHtml(tr("Device %1 requests verification.").arg(session->remoteDeviceId())));
    notification.callbacks = {m_showDialogCallback.name(), m_rejectCallback.name()};
    notification.acceptCallback = m_showDialogCallback.name();
    notification.data.insert(QStringLiteral("matrix:verification-session"), key);
    m_notificationService->notify(notification);
}

QString MatrixDeviceVerificationNotificationService::sessionKey(
    const Account &account, Quotient::KeyVerificationSession *session) const
{
    // In-room sessions have an empty transactionId in libQuotient 0.9.6.1.
    auto id = session->property("kaduVerificationId").toString();
    if (id.isEmpty())
    {
        id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        session->setProperty("kaduVerificationId", id);
    }
    return account.uuid().toString() + QLatin1Char('\x1f') + id;
}

Quotient::KeyVerificationSession *
MatrixDeviceVerificationNotificationService::sessionFor(const Notification &notification) const
{
    return m_sessions.value(notification.data.value(QStringLiteral("matrix:verification-session")).toString());
}

void MatrixDeviceVerificationNotificationService::showVerificationDialog(const Notification &notification)
{
    const auto key = notification.data.value(QStringLiteral("matrix:verification-session")).toString();
    auto *session = sessionFor(notification);
    if (auto *dialog = m_dialogs.value(key).data())
    {
        dialog->raise();
        dialog->activateWindow();
        return;
    }

    if (!session || session->state() != Quotient::KeyVerificationSession::INCOMING)
        return;

    auto *dialog = new MatrixDeviceVerificationDialog{session};
    m_dialogs.insert(key, dialog);
    connect(dialog, &QObject::destroyed, this, [this, key] { m_dialogs.remove(key); });
    dialog->show();
}

void MatrixDeviceVerificationNotificationService::rejectVerification(const Notification &notification)
{
    if (auto *session = sessionFor(notification);
        session && session->state() == Quotient::KeyVerificationSession::INCOMING)
        session->cancelVerification(Quotient::KeyVerificationSession::USER);
}
