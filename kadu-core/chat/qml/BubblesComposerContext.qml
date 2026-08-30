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

    property string colorScheme: "System"
    property var customColors: ({ "enabled": false })
    property var context: ({})
    property var cancelComposerContext: null

    readonly property var target: context && context.target ? context.target : ({})
    readonly property bool darkSurface: colorScheme === "Dark" ||
                                       (colorScheme !== "Light" && systemPalette.base.r * 0.2126 +
                                        systemPalette.base.g * 0.7152 +
                                        systemPalette.base.b * 0.0722 < 0.5)
    readonly property color accentColor: darkSurface ? "#5b9bd5" : "#2878b7"

    implicitHeight: context && context.mode ? content.implicitHeight + 16 : 0

    SystemPalette {
        id: systemPalette
        colorGroup: SystemPalette.Active
    }

    Rectangle {
        anchors.fill: parent
        color: colorScheme === "System" ? systemPalette.alternateBase
                                         : (darkSurface ? "#28333e" : "#e9eef4")
        border.width: 1
        border.color: root.accentColor
    }

    Rectangle {
        width: 4
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        color: root.accentColor
    }

    Row {
        id: content
        x: 14
        y: 8
        width: parent.width - 26
        spacing: 10

        Column {
            width: parent.width - cancelButton.width - parent.spacing
            spacing: 2

            Text {
                width: parent.width
                text: root.context.mode === "edit" ? qsTr("Editing message") : qsTr("Replying to %1").arg(root.target.senderDisplayName || qsTr("message"))
                color: root.accentColor
                font.bold: true
                font.pixelSize: 12
                elide: Text.ElideRight
            }

            Text {
                width: parent.width
                text: root.target.plainText || ""
                color: root.colorScheme === "System" ? systemPalette.text
                                                       : (root.darkSurface ? "#e7edf5" : "#303842")
                font.pixelSize: 12
                maximumLineCount: 2
                elide: Text.ElideRight
                wrapMode: Text.Wrap
            }
        }

        Button {
            id: cancelButton
            text: qsTr("Cancel")
            Accessible.name: text
            onClicked: {
                if (root.cancelComposerContext)
                    root.cancelComposerContext()
            }
        }
    }
}
