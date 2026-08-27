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
    readonly property color incomingSenderColor: usesCustomColors ? customColors.buddyNick
                                                                  : (colorScheme === "System" ? systemPalette.text
                                                                                              : (darkSurface ? "#7db8f4" : "#1f5d9c"))
    readonly property color outgoingSenderColor: usesCustomColors ? customColors.myNick
                                                                  : (colorScheme === "System" ? systemPalette.accent
                                                                                              : (darkSurface ? "#d3a8f4" : "#6d3a85"))
    readonly property color failureColor: darkSurface ? "#ff9d97" : "#b32929"
    readonly property color textColor: usesCustomColors
                                           ? (ownEvent ? customColors.myText : customColors.buddyText)
                                           : (colorScheme === "System" ? systemPalette.text
                                                                       : (darkSurface ? "#f2f4f8" : "#202020"))
    readonly property color mutedTextColor: colorScheme === "System" ? systemPalette.mid
                                                                : (darkSurface ? "#aeb8c7" : "#666666")
    readonly property color separatorColor: colorScheme === "System" ? systemPalette.mid
                                                               : (darkSurface ? "#6d7785" : "#858585")

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
        spacing: 0

        Item {
            visible: root.startsNewDay
            width: parent.width
            height: 30

            Row {
                anchors.centerIn: parent
                spacing: 8

                Rectangle {
                    width: 48
                    height: 1
                    anchors.verticalCenter: parent.verticalCenter
                    color: root.separatorColor
                    opacity: 0.45
                }

                Text {
                    text: Qt.formatDate(root.timestamp, "dddd, d MMMM")
                    color: root.mutedTextColor
                    opacity: 0.60
                    font.pixelSize: 11
                }

                Rectangle {
                    width: 48
                    height: 1
                    anchors.verticalCenter: parent.verticalCenter
                    color: root.separatorColor
                    opacity: 0.45
                }
            }
        }

        Text {
            visible: root.systemEvent()
            width: parent.width
            height: implicitHeight + 10
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: "•  " + root.plainText
            color: root.mutedTextColor
            opacity: 0.65
            font.italic: true
        }

        Item {
            visible: !root.systemEvent()
            width: parent.width
            implicitHeight: messageContent.y + messageContent.implicitHeight + 5

            HoverHandler {
                id: hoverHandler
            }

            Rectangle {
                width: parent.width
                height: 1
                visible: root.showSender
                color: root.separatorColor
                opacity: 0.35
            }

            Rectangle {
                anchors.fill: parent
                color: root.usesCustomColors ? (root.ownEvent ? root.customColors.myBackground
                                                               : root.customColors.buddyBackground)
                                             : (root.darkSurface ? "#ffffff" : "#000000")
                opacity: root.usesCustomColors ? 1.0 : (hoverHandler.hovered ? 0.08 : 0.0)
            }

            Column {
                id: messageContent
                x: 8
                y: root.showSender ? 6 : 2
                width: parent.width - 16
                spacing: 3

                Item {
                    visible: root.showSender
                    width: parent.width
                    implicitHeight: Math.max(sender.implicitHeight, timestamp.implicitHeight)

                    Text {
                        id: sender
                        anchors.left: parent.left
                        anchors.right: timestamp.left
                        anchors.rightMargin: 8
                        text: root.ownEvent ? qsTr("You") : root.senderDisplayName
                        elide: Text.ElideRight
                        color: root.ownEvent ? root.outgoingSenderColor : root.incomingSenderColor
                        font.bold: true
                    }

                    Text {
                        id: timestamp
                        visible: root.showTimestamp
                        anchors.right: parent.right
                        text: Qt.formatTime(root.timestamp, "HH:mm")
                        color: root.mutedTextColor
                        opacity: 0.50
                        font.pixelSize: 11
                    }
                }

                Item {
                    id: messageRow
                    width: parent.width
                    implicitHeight: Math.max(message.implicitHeight, trailingTimestamp.implicitHeight)

                    TextEdit {
                        id: message
                        width: trailingTimestamp.visible ? parent.width - trailingTimestamp.implicitWidth - 8 : parent.width
                        text: root.redacted ? qsTr("Message removed") : root.messageText()
                        textFormat: TextEdit.RichText
                        color: root.textColor
                        wrapMode: TextEdit.Wrap
                        readOnly: true
                        selectByMouse: true
                        font.pixelSize: 13
                        onLinkActivated: Qt.openUrlExternally(link)
                    }

                    Text {
                        id: trailingTimestamp
                        visible: !root.showSender && root.showTimestamp
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        text: Qt.formatTime(root.timestamp, "HH:mm")
                        color: root.mutedTextColor
                        opacity: 0.50
                        font.pixelSize: 11
                    }
                }

                Text {
                    visible: root.ownEvent && root.deliveryText().length > 0
                    width: parent.width
                    horizontalAlignment: Text.AlignRight
                    text: root.deliveryText()
                    color: root.deliveryState === 4 ? root.failureColor : root.mutedTextColor
                    opacity: root.deliveryState === 4 ? 1.0 : 0.50
                    font.pixelSize: 11
                }
            }
        }
    }
}
