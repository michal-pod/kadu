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
    property var chatFont: ({ "family": "", "pointSize": 10, "forced": false })
    property var context: ({})
    property var cancelComposerContext: null

    readonly property var target: context && context.target ? context.target : ({})
    readonly property bool darkSurface: colorScheme === "Dark" ||
                                       (colorScheme !== "Light" && systemPalette.base.r * 0.2126 +
                                        systemPalette.base.g * 0.7152 +
                                        systemPalette.base.b * 0.0722 < 0.5)
    readonly property string fontFamily: chatFont && chatFont.family ? chatFont.family : ""
    readonly property real fontPointSize: chatFont && chatFont.forced === true && Number(chatFont.pointSize) > 0
                                         ? Number(chatFont.pointSize) : 10
    readonly property color backgroundColor: colorScheme === "System" ? systemPalette.alternateBase
                                                                        : (darkSurface ? "#303946" : "#e7edf4")
    readonly property color textColor: colorScheme === "System" ? systemPalette.text
                                                                  : (darkSurface ? "#f2f4f8" : "#202020")
    readonly property color mutedTextColor: colorScheme === "System" ? systemPalette.mid
                                                                       : (darkSurface ? "#b6c0cf" : "#5c6470")
    readonly property color accentColor: customColors && customColors.enabled
                                         ? (target.senderColor || systemPalette.accent)
                                         : (colorScheme === "System" ? systemPalette.accent
                                                                      : (darkSurface ? "#7db8f4" : "#4f8ecb"))

    implicitHeight: card.implicitHeight

    SystemPalette {
        id: systemPalette
        colorGroup: SystemPalette.Active
    }

    Rectangle {
        id: card
        width: parent.width
        implicitHeight: content.implicitHeight + 28
        radius: 12
        color: root.backgroundColor
        border.width: 1
        border.color: Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.55)

        Row {
            id: content
            x: 14
            y: 14
            width: parent.width - 28
            spacing: 10

            Item {
                width: 38
                height: 38
                y: modeLabel.height + details.spacing

                Rectangle {
                    anchors.fill: parent
                    radius: width / 2
                    color: root.target.senderColor || root.accentColor
                    visible: avatar.status !== Image.Ready
                }

                Image {
                    id: avatar
                    anchors.fill: parent
                    source: root.target.senderAvatarSource || ""
                    fillMode: Image.PreserveAspectFit
                }

                Text {
                    anchors.centerIn: parent
                    visible: avatar.status !== Image.Ready
                    text: (root.target.senderDisplayName || "?").slice(0, 1).toUpperCase()
                    color: "white"
                    font.family: root.fontFamily
                    font.pointSize: root.fontPointSize
                }
            }

            Column {
                id: details
                width: parent.width - 38 - cancelButton.width - parent.spacing * 2
                spacing: 2

                Text {
                    id: modeLabel
                    width: parent.width
                    text: root.context.mode === "edit" ? qsTr("Editing message") : qsTr("Replying to")
                    color: root.mutedTextColor
                    elide: Text.ElideRight
                    font.family: root.fontFamily
                    font.pointSize: Math.max(8, root.fontPointSize - 2)
                }

                Text {
                    width: parent.width
                    text: root.target.senderDisplayName || qsTr("Unknown user")
                    color: root.accentColor
                    elide: Text.ElideRight
                    font.bold: true
                    font.family: root.fontFamily
                    font.pointSize: root.fontPointSize
                }

                Text {
                    width: parent.width
                    text: root.target.redacted ? qsTr("Message removed") : (root.target.plainText || "")
                    color: root.textColor
                    maximumLineCount: 3
                    elide: Text.ElideRight
                    wrapMode: Text.Wrap
                    font.family: root.fontFamily
                    font.pointSize: root.fontPointSize
                }
            }

            ToolButton {
                id: cancelButton
                width: 32
                height: 32
                display: AbstractButton.IconOnly
                Accessible.name: qsTr("Cancel")
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Cancel")
                background: Item {}
                onClicked: {
                    if (root.cancelComposerContext)
                        root.cancelComposerContext()
                }

                contentItem: Item {
                    Image {
                        id: cancelIcon
                        anchors.centerIn: parent
                        width: 16
                        height: 16
                        source: "image://kaduicon/application-exit"
                        visible: status === Image.Ready && sourceSize.width > 1
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: !cancelIcon.visible
                        text: "×"
                        color: root.textColor
                        font.pixelSize: 20
                    }
                }
            }
        }
    }
}
