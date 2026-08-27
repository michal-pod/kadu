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

Item {
    id: root

    required property string stableId
    required property int kind
    required property var timestamp
    required property bool ownEvent
    required property string senderDisplayName
    required property string plainText
    required property string formattedText
    required property bool showSender
    required property bool showTimestamp
    required property bool startsNewDay
    required property int deliveryState
    required property bool redacted
    required property bool encrypted
    required property int decryptionState
    required property string errorText
    property string colorScheme: "System"
    property var customColors: ({ "enabled": false })
    property var openUrl: null

    implicitHeight: content.implicitHeight

    SystemPalette {
        id: systemPalette
        colorGroup: SystemPalette.Active
    }

    readonly property bool darkSurface: colorScheme === "Dark" ||
                                       (colorScheme !== "Light" && systemPalette.base.r * 0.2126 +
                                        systemPalette.base.g * 0.7152 +
                                        systemPalette.base.b * 0.0722 < 0.5)
    readonly property bool usesCustomColors: customColors && customColors.enabled
    readonly property color textColor: usesCustomColors ? customColors.buddyText
                                                         : (colorScheme === "System" ? systemPalette.text
                                                                                     : (darkSurface ? "#f2f4f8" : "#202020"))
    readonly property color outgoingTextColor: usesCustomColors ? customColors.myText
                                                                 : (colorScheme === "System" ? systemPalette.highlightedText
                                                                                             : textColor)
    readonly property color mutedTextColor: colorScheme === "System" ? systemPalette.mid
                                                                : (darkSurface ? "#b6c0cf" : "#5c6470")
    readonly property color incomingAvatarColor: usesCustomColors ? customColors.buddyNick
                                                                   : (colorScheme === "System" ? systemPalette.accent
                                                                                               : (darkSurface ? "#4f8ecb" : "#4f8ecb"))
    readonly property color outgoingAvatarColor: usesCustomColors ? customColors.myNick
                                                                   : (colorScheme === "System" ? systemPalette.highlight
                                                                                               : (darkSurface ? "#7552a0" : "#7552a0"))
    readonly property color incomingBubbleColor: usesCustomColors ? customColors.buddyBackground
                                                                   : (colorScheme === "System" ? systemPalette.alternateBase
                                                                                               : (darkSurface ? "#303946" : "#e7edf4"))
    readonly property color outgoingBubbleColor: usesCustomColors ? customColors.myBackground
                                                                   : (colorScheme === "System" ? systemPalette.highlight
                                                                                               : (darkSurface ? "#4b3764" : "#eadff5"))

    function systemEvent() { return kind >= 7 }
    function messageText() {
        if (formattedText.length > 0)
            return formattedText
        return plainText.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/\n/g, "<br/>")
    }
    function deliveryText() {
        if (deliveryState === 1)
            return qsTr("sending…")
        if (deliveryState === 3)
            return qsTr("delivered")
        if (deliveryState === 4)
            return qsTr("failed")
        return ""
    }

    Column {
        id: content
        width: parent.width
        spacing: 5

        Text {
            visible: root.startsNewDay
            anchors.horizontalCenter: parent.horizontalCenter
            text: Qt.formatDate(root.timestamp, "dddd, d MMMM")
            color: root.mutedTextColor
            font.pixelSize: 12
        }

        Text {
            visible: root.systemEvent()
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: root.plainText
            color: root.mutedTextColor
            font.italic: true
        }

        Row {
            visible: !root.systemEvent()
            width: parent.width
            layoutDirection: root.ownEvent ? Qt.RightToLeft : Qt.LeftToRight
            spacing: 7

            Rectangle {
                visible: root.showSender
                width: 28
                height: 28
                radius: width / 2
                color: root.ownEvent ? root.outgoingAvatarColor : root.incomingAvatarColor
                Text { anchors.centerIn: parent; text: root.senderDisplayName.slice(0, 1).toUpperCase(); color: "white" }
            }

            Column {
                width: Math.min(parent.width - 42, Math.max(120, bubble.implicitWidth))
                spacing: 2

                Text {
                    visible: root.showSender
                    text: root.ownEvent ? qsTr("You") : root.senderDisplayName
                    color: root.mutedTextColor
                    font.bold: true
                    font.pixelSize: 12
                }

                Rectangle {
                    id: bubble
                    width: Math.min(parent.width, message.implicitWidth + 22)
                    implicitHeight: message.implicitHeight + 14
                    radius: 8
                    color: root.ownEvent ? root.outgoingBubbleColor : root.incomingBubbleColor

                    TextEdit {
                        id: message
                        anchors.fill: parent
                        anchors.margins: 10
                        text: root.redacted ? qsTr("Message removed") : root.messageText()
                        textFormat: TextEdit.RichText
                        color: root.ownEvent ? root.outgoingTextColor : root.textColor
                        wrapMode: TextEdit.Wrap
                        readOnly: true
                        selectByMouse: true
                        onLinkActivated: {
                            if (root.openUrl)
                                root.openUrl(link)
                        }
                    }
                }

                Text {
                    visible: root.showTimestamp
                    text: Qt.formatTime(root.timestamp, "HH:mm")
                    color: root.mutedTextColor
                    font.pixelSize: 11
                }

                Text {
                    visible: root.ownEvent && root.deliveryText().length > 0
                    text: root.deliveryText()
                    color: root.deliveryState === 4 ? "#ff8b8b" : root.mutedTextColor
                    font.pixelSize: 11
                }
            }
        }
    }
}
