/*
 * %kadu copyright begin%
 * Copyright 2026 Kadu Qt6 port
 * %kadu copyright end%
 */

#include "matrix-room-invitation-notification-service.h"
#include "matrix-room-invitation-notification-service.moc"

#include "accounts/account.h"
#include "gui/matrix-room-invitation-dialog.h"
#include "html/html-conversion.h"
#include "html/html-string.h"
#include "matrix-protocol.h"
#include "notification/notification-callback-repository.h"
#include "notification/notification-event-repository.h"
#include "notification/notification-service.h"
#include "notification/notification.h"

#include <Quotient/room.h>

#include <utility>

MatrixRoomInvitationNotificationService::MatrixRoomInvitationNotificationService(QObject *parent)
        : QObject{parent},
          m_joinCallback{QStringLiteral("matrix-room-invitation-join"), tr("Join"),
                         [this](const Notification &notification) { joinRoom(notification); }},
          m_rejectCallback{QStringLiteral("matrix-room-invitation-reject"), tr("Reject invitation"),
                           [this](const Notification &notification) { rejectRoom(notification); }},
          m_showDialogCallback{QStringLiteral("matrix-room-invitation-show-dialog"), tr("Show invitation"),
                               [this](const Notification &notification) { showInvitationDialog(notification); }},
          m_matrixEvent{QStringLiteral("Matrix"), QStringLiteral(QT_TRANSLATE_NOOP("@default", "Matrix"))},
          m_roomInvitationEvent{QStringLiteral("Matrix/RoomInvitation"),
                                QStringLiteral(QT_TRANSLATE_NOOP("@default", "Matrix room invitation"))}
{
}

MatrixRoomInvitationNotificationService::~MatrixRoomInvitationNotificationService()
{
}

void MatrixRoomInvitationNotificationService::setNotificationCallbackRepository(
    NotificationCallbackRepository *notificationCallbackRepository)
{
    m_notificationCallbackRepository = notificationCallbackRepository;
}

void MatrixRoomInvitationNotificationService::setNotificationEventRepository(
    NotificationEventRepository *notificationEventRepository)
{
    m_notificationEventRepository = notificationEventRepository;
}

void MatrixRoomInvitationNotificationService::setNotificationService(NotificationService *notificationService)
{
    m_notificationService = notificationService;
}

void MatrixRoomInvitationNotificationService::init()
{
    m_notificationEventRepository->addNotificationEvent(m_matrixEvent);
    m_notificationEventRepository->addNotificationEvent(m_roomInvitationEvent);
    m_notificationCallbackRepository->addCallback(m_joinCallback);
    m_notificationCallbackRepository->addCallback(m_rejectCallback);
    m_notificationCallbackRepository->addCallback(m_showDialogCallback);
}

void MatrixRoomInvitationNotificationService::done()
{
    m_notificationEventRepository->removeNotificationEvent(m_matrixEvent);
    m_notificationEventRepository->removeNotificationEvent(m_roomInvitationEvent);
    m_notificationCallbackRepository->removeCallback(m_joinCallback);
    m_notificationCallbackRepository->removeCallback(m_rejectCallback);
    m_notificationCallbackRepository->removeCallback(m_showDialogCallback);
}

void MatrixRoomInvitationNotificationService::notifyInvitation(const Account &account, Quotient::Room *room)
{
    if (!account || !room || !m_notificationService)
        return;

    const auto roomName = room->displayName().isEmpty() ? room->id() : room->displayName();

    auto data = QVariantMap{};
    data.insert(QStringLiteral("matrix:account"), QVariant::fromValue(account));
    data.insert(QStringLiteral("matrix:room-id"), room->id());
    data.insert(QStringLiteral("matrix:room-name"), roomName);

    auto notification = Notification{};
    notification.type = m_roomInvitationEvent.name();
    notification.title = tr("Matrix room invitation");
    notification.text = normalizeHtml(
        plainToHtml(tr("You have been invited to the Matrix room %1.").arg(roomName)));
    notification.data = std::move(data);
    notification.callbacks = {m_joinCallback.name(), m_rejectCallback.name()};
    notification.acceptCallback = m_showDialogCallback.name();
    m_notificationService->notify(notification);
}

void MatrixRoomInvitationNotificationService::joinRoom(const Notification &notification)
{
    const auto account = qvariant_cast<Account>(notification.data.value(QStringLiteral("matrix:account")));
    const auto roomId = notification.data.value(QStringLiteral("matrix:room-id")).toString();
    auto *protocol = account ? qobject_cast<MatrixProtocol *>(account.protocolHandler()) : nullptr;
    if (protocol)
        protocol->joinRoom(roomId);
}

void MatrixRoomInvitationNotificationService::rejectRoom(const Notification &notification)
{
    const auto account = qvariant_cast<Account>(notification.data.value(QStringLiteral("matrix:account")));
    const auto roomId = notification.data.value(QStringLiteral("matrix:room-id")).toString();
    auto *protocol = account ? qobject_cast<MatrixProtocol *>(account.protocolHandler()) : nullptr;
    if (protocol)
        protocol->rejectRoomInvitation(roomId);
}

void MatrixRoomInvitationNotificationService::showInvitationDialog(const Notification &notification)
{
    const auto roomName = notification.data.value(QStringLiteral("matrix:room-name")).toString();
    auto *dialog = new MatrixRoomInvitationDialog{roomName};
    connect(dialog, &MatrixRoomInvitationDialog::joinRequested, this,
            [this, notification] { joinRoom(notification); });
    connect(dialog, &MatrixRoomInvitationDialog::rejectRequested, this,
            [this, notification] { rejectRoom(notification); });
    connect(dialog, &MatrixRoomInvitationDialog::joinRequested, dialog, &QDialog::accept);
    connect(dialog, &MatrixRoomInvitationDialog::rejectRequested, dialog, &QDialog::reject);
    dialog->show();
}
