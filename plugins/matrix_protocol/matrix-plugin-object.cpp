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

#include "matrix-plugin-object.h"
#include "matrix-plugin-object.moc"

#include "matrix-protocol-factory.h"
#include "matrix-device-verification-notification-service.h"
#include "matrix-join-room-action.h"
#include "matrix-room-invitation-notification-service.h"
#include "matrix-verify-device-action.h"
#include "protocols/protocols-manager.h"

MatrixPluginObject::MatrixPluginObject(QObject *parent) : QObject{parent}
{
}

MatrixPluginObject::~MatrixPluginObject()
{
}

void MatrixPluginObject::setMatrixProtocolFactory(MatrixProtocolFactory *matrixProtocolFactory)
{
    m_matrixProtocolFactory = matrixProtocolFactory;
}

void MatrixPluginObject::setMatrixJoinRoomAction(MatrixJoinRoomAction *matrixJoinRoomAction)
{
    m_matrixJoinRoomAction = matrixJoinRoomAction;
}

void MatrixPluginObject::setMatrixDeviceVerificationNotificationService(
    MatrixDeviceVerificationNotificationService *matrixDeviceVerificationNotificationService)
{
    m_matrixDeviceVerificationNotificationService = matrixDeviceVerificationNotificationService;
}

void MatrixPluginObject::setMatrixRoomInvitationNotificationService(
    MatrixRoomInvitationNotificationService *matrixRoomInvitationNotificationService)
{
    m_matrixRoomInvitationNotificationService = matrixRoomInvitationNotificationService;
}

void MatrixPluginObject::setMatrixVerifyDeviceAction(MatrixVerifyDeviceAction *matrixVerifyDeviceAction)
{
    m_matrixVerifyDeviceAction = matrixVerifyDeviceAction;
}

void MatrixPluginObject::setProtocolsManager(ProtocolsManager *protocolsManager)
{
    m_protocolsManager = protocolsManager;
}

void MatrixPluginObject::init()
{
    m_protocolsManager->registerProtocolFactory(m_matrixProtocolFactory);
}

void MatrixPluginObject::done()
{
    m_protocolsManager->unregisterProtocolFactory(m_matrixProtocolFactory);
}
