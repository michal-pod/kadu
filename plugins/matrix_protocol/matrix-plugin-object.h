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

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <injeqt/injeqt.h>

#include "injeqt-type-roles.h"

/**
 * Lifecycle object for the Matrix plugin.
 *
 * The protocol factory is registered here; networking remains deliberately absent until the
 * libQuotient integration is added.
 */
class MatrixProtocolFactory;
class MatrixDeviceVerificationNotificationService;
class MatrixJoinRoomAction;
class MatrixRoomInvitationNotificationService;
class MatrixVerifyDeviceAction;
class ProtocolsManager;

class MatrixPluginObject : public QObject
{
    Q_OBJECT
    INJEQT_TYPE_ROLE(PLUGIN)

public:
    Q_INVOKABLE explicit MatrixPluginObject(QObject *parent = nullptr);
    virtual ~MatrixPluginObject();

private slots:
    INJEQT_SET void setMatrixProtocolFactory(MatrixProtocolFactory *matrixProtocolFactory);
    INJEQT_SET void setMatrixDeviceVerificationNotificationService(
        MatrixDeviceVerificationNotificationService *matrixDeviceVerificationNotificationService);
    INJEQT_SET void setMatrixJoinRoomAction(MatrixJoinRoomAction *matrixJoinRoomAction);
    INJEQT_SET void setMatrixRoomInvitationNotificationService(
        MatrixRoomInvitationNotificationService *matrixRoomInvitationNotificationService);
    INJEQT_SET void setMatrixVerifyDeviceAction(MatrixVerifyDeviceAction *matrixVerifyDeviceAction);
    INJEQT_SET void setProtocolsManager(ProtocolsManager *protocolsManager);
    INJEQT_INIT void init();
    INJEQT_DONE void done();

private:
    QPointer<MatrixProtocolFactory> m_matrixProtocolFactory;
    QPointer<MatrixDeviceVerificationNotificationService> m_matrixDeviceVerificationNotificationService;
    QPointer<MatrixJoinRoomAction> m_matrixJoinRoomAction;
    QPointer<MatrixRoomInvitationNotificationService> m_matrixRoomInvitationNotificationService;
    QPointer<MatrixVerifyDeviceAction> m_matrixVerifyDeviceAction;
    QPointer<ProtocolsManager> m_protocolsManager;
};
