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

import QtQuick
import QtQuick.Controls

Item {
    id: root

    property bool encrypted: false
    property int decryptionState: 0
    property string errorText: ""
    property int iconSize: 14

    readonly property string statusText: {
        if (!encrypted || decryptionState === 0)
            return ""
        if (decryptionState === 1)
            return qsTr("Encrypted event; waiting for a decryption key")
        if (decryptionState === 2)
            return qsTr("Encrypted and successfully decrypted")
        if (decryptionState === 4)
            return qsTr("Encrypted event; decryption key is unavailable")
        return qsTr("Encrypted event could not be decrypted")
    }
    readonly property string iconName: {
        if (decryptionState === 1)
            return "view-refresh"
        if (decryptionState === 2)
            return "security-high"
        if (decryptionState === 4)
            return "dialog-password"
        return "dialog-error"
    }

    visible: encrypted && decryptionState !== 0
    implicitWidth: visible ? iconSize : 0
    implicitHeight: visible ? iconSize : 0
    Accessible.name: statusText

    Image {
        anchors.fill: parent
        source: root.visible ? "image://kaduicon/" + encodeURIComponent(root.iconName) : ""
        sourceSize.width: root.iconSize
        sourceSize.height: root.iconSize
        fillMode: Image.PreserveAspectFit
    }

    HoverHandler {
        id: hoverHandler
    }

    ToolTip.visible: root.visible && hoverHandler.hovered
    ToolTip.text: root.decryptionState !== 2 && root.errorText.length > 0
                  ? root.statusText + ": " + root.errorText : root.statusText
    ToolTip.delay: 350
}
