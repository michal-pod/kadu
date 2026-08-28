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

    required property string stableId
    required property int kind
    required property var timestamp
    required property bool ownEvent
    required property string senderDisplayName
    required property string plainText
    required property string formattedText
    property var attachments: []
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
    property var openImage: null
    property var timelineActions: null
    property var executeTimelineAction: null
    property var copyText: null
    property string contextSelectedText: ""
    property string contextLink: ""

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
        if (attachments.length === 1 && plainText.trim() === attachments[0].fileName.trim())
            return ""
        if (formattedText.length > 0)
            return formattedText
        return plainText
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
    function hasImageAttachments() {
        for (let index = 0; index < attachments.length; ++index)
            if (attachments[index].kind === 0)
                return true
        return false
    }
    function availableActions() {
        return timelineActions ? timelineActions(stableId) : []
    }
    function actionSymbol(actionKey) {
        if (actionKey === "reply")
            return "↩"
        if (actionKey === "edit")
            return "✎"
        if (actionKey === "delete")
            return "⌫"
        if (actionKey === "saveAttachment")
            return "⇩"
        return "⋯"
    }
    function triggerAction(action) {
        if (executeTimelineAction)
            executeTimelineAction(stableId, action)
    }
    function actionId(actionKey) {
        const actions = availableActions()
        for (let index = 0; index < actions.length; ++index)
            if (actions[index].key === actionKey)
                return actions[index].id
        return -1
    }

    Menu {
        id: selectedTextMenu

        MenuItem {
            text: qsTr("Copy")
            enabled: root.contextSelectedText.length > 0
            onTriggered: {
                if (root.copyText)
                    root.copyText(root.contextSelectedText)
            }
        }
    }

    Menu {
        id: linkMenu

        MenuItem {
            text: qsTr("Open link")
            enabled: root.contextLink.length > 0
            onTriggered: {
                if (root.openUrl)
                    root.openUrl(root.contextLink)
            }
        }

        MenuItem {
            text: qsTr("Copy link")
            enabled: root.contextLink.length > 0
            onTriggered: {
                if (root.copyText)
                    root.copyText(root.contextLink)
            }
        }
    }

    Menu {
        id: eventMenu

        Repeater {
            model: root.availableActions()

            delegate: MenuItem {
                required property var modelData
                text: modelData.text
                onTriggered: root.triggerAction(modelData.id)
            }
        }
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
                    width: root.hasImageAttachments() ? parent.width : Math.min(parent.width, message.implicitWidth + 22)
                    implicitHeight: bubbleContent.implicitHeight + 14
                    radius: 8
                    color: root.ownEvent ? root.outgoingBubbleColor : root.incomingBubbleColor

                    HoverHandler {
                        id: hoverHandler
                    }

                    Row {
                        anchors.top: parent.top
                        anchors.right: parent.right
                        anchors.margins: 4
                        spacing: 2
                        visible: hoverHandler.hovered && !root.redacted && root.availableActions().length > 0
                        z: 2

                        Repeater {
                            model: root.availableActions()

                            delegate: ToolButton {
                                required property int index
                                required property var modelData
                                visible: index < 3
                                text: root.actionSymbol(modelData.key)
                                font.pixelSize: 14
                                ToolTip.visible: hovered
                                ToolTip.text: modelData.text
                                onClicked: root.triggerAction(modelData.id)
                            }
                        }

                        ToolButton {
                            visible: root.availableActions().length > 3
                            text: "⋯"
                            font.pixelSize: 16
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("More actions")
                            onClicked: eventMenu.popup()
                        }
                    }

                    Column {
                        id: bubbleContent
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 4

                        Item {
                            width: parent.width
                            implicitHeight: message.implicitHeight

                            TextEdit {
                                id: message
                                width: parent.width
                                text: root.redacted ? qsTr("Message removed") : root.messageText()
                                textFormat: root.formattedText.length > 0 ? TextEdit.RichText : TextEdit.PlainText
                                color: root.ownEvent ? root.outgoingTextColor : root.textColor
                                wrapMode: TextEdit.Wrap
                                readOnly: true
                                selectByMouse: true
                                onLinkActivated: {
                                    if (root.openUrl)
                                        root.openUrl(link)
                                }
                            }

                            MouseArea {
                                id: messageContextArea
                                anchors.fill: message
                                acceptedButtons: Qt.RightButton
                                onClicked: function(mouse) {
                                    root.contextSelectedText = message.selectedText
                                    root.contextLink = message.linkAt(mouse.x, mouse.y)
                                    if (root.contextSelectedText.length > 0)
                                        selectedTextMenu.popup()
                                    else if (root.contextLink.length > 0)
                                        linkMenu.popup()
                                    else
                                        eventMenu.popup()
                                }
                            }
                        }

                        Repeater {
                            model: root.attachments

                            delegate: ChatFileAttachment {
                                required property var modelData
                                width: parent ? parent.width : 1
                                attachment: modelData
                                textColor: root.ownEvent ? root.outgoingTextColor : root.textColor
                                linkColor: root.ownEvent ? root.outgoingTextColor : root.incomingAvatarColor
                                backgroundColor: "transparent"
                                borderColor: root.mutedTextColor
                                saveAttachment: root.actionId("saveAttachment") >= 0
                                                ? function() { root.triggerAction(root.actionId("saveAttachment")) } : null
                            }
                        }

                        Repeater {
                            model: root.attachments

                            delegate: ChatImageAttachment {
                                required property var modelData
                                width: parent ? parent.width : 1
                                visible: modelData.kind === 0
                                attachment: modelData
                                placeholderColor: root.ownEvent ? root.outgoingBubbleColor : root.incomingBubbleColor
                                placeholderTextColor: root.ownEvent ? root.outgoingTextColor : root.textColor
                                openImage: root.openImage
                            }
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
