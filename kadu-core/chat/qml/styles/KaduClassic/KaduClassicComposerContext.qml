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
    readonly property color backgroundColor: colorScheme === "System" ? systemPalette.alternateBase
                                                                        : (darkSurface ? "#2d323a" : "#edf0f4")
    readonly property color textColor: colorScheme === "System" ? systemPalette.text
                                                                  : (darkSurface ? "#f2f4f8" : "#202020")
    readonly property color mutedTextColor: colorScheme === "System" ? systemPalette.mid
                                                                       : (darkSurface ? "#aeb8c7" : "#666666")

    implicitHeight: context && context.mode ? content.implicitHeight + 16 : 0

    SystemPalette {
        id: systemPalette
        colorGroup: SystemPalette.Active
    }

    Rectangle {
        anchors.fill: parent
        color: root.backgroundColor
        border.width: 1
        border.color: root.colorScheme === "System" ? systemPalette.mid
                                                      : (root.darkSurface ? "#4d5662" : "#c7cdd5")
    }

    Row {
        id: content
        x: 12
        y: 8
        width: parent.width - 24
        spacing: 10

        Column {
            width: parent.width - cancelButton.width - parent.spacing
            spacing: 2

            Text {
                width: parent.width
                text: root.context.mode === "edit" ? qsTr("Editing message") : qsTr("Replying to %1").arg(root.target.senderDisplayName || qsTr("message"))
                color: root.textColor
                font.bold: true
                font.pixelSize: 12
                elide: Text.ElideRight
            }

            Text {
                width: parent.width
                text: root.target.plainText || ""
                color: root.mutedTextColor
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
