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

#include "matrix-module.h"

#include "matrix-plugin-object.h"
#include "matrix-protocol-factory.h"
#include "matrix-device-verification-notification-service.h"
#include "matrix-join-room-action.h"
#include "matrix-room-invitation-notification-service.h"
#include "matrix-ssl-certificate-service.h"
#include "matrix-verify-device-action.h"

MatrixModule::MatrixModule()
{
    add_type<MatrixPluginObject>();
    add_type<MatrixJoinRoomAction>();
    add_type<MatrixDeviceVerificationNotificationService>();
    add_type<MatrixRoomInvitationNotificationService>();
    add_type<MatrixSslCertificateService>();
    add_type<MatrixVerifyDeviceAction>();
    add_type<MatrixProtocolFactory>();
}
