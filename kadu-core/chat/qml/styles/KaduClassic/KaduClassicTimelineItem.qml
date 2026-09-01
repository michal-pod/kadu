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
// Bundled styles use the host's shared attachment and location components.
// This is also the resource root embedded by kadu-theme-tester.
import "qrc:/Kadu/Chat/chat/qml" as KaduChat

Item {
    id: root

    required property string stableId
    required property string protocolEventType
    required property int kind
    required property var timestamp
    required property bool ownEvent
    required property string senderDisplayName
    required property url senderAvatarSource
    required property color senderColor
    required property string plainText
    required property string formattedText
    required property string replyToId
    property var reply: ({})
    property var attachments: []
    property string locationUri: ""
    property var reactions: []
    required property bool showSender
    required property bool showAvatar
    required property bool showTimestamp
    required property bool startsNewDay
    required property int deliveryState
    required property bool edited
    required property bool systemEvent
    property bool emote: false
    required property bool redacted
    required property bool encrypted
    required property int decryptionState
    required property string errorText
    property string colorScheme: "System"
    property var customColors: ({ "enabled": false })
    property var chatFont: ({ "family": "", "pointSize": 10, "bold": false, "italic": false, "underline": false })
    property var openUrl: null
    property var openImage: null
    property var openLocation: null
    property var timelineActions: null
    property var executeTimelineAction: null
    property var copyText: null
    property var removeOwnReaction: null
    property var addReaction: null
    property var requestFullReactionSelector: null
    // Auxiliary views can retain the regular entry component while asking the
    // style to place it in the incoming column.
    property bool forceIncomingAlignment: false
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
    readonly property string configuredFontFamily: chatFont && chatFont.family ? chatFont.family : ""
    readonly property bool configuredFontForced: chatFont && chatFont.forced === true
    readonly property real configuredFontPointSize: configuredFontForced && Number(chatFont.pointSize) > 0
                                                  ? Number(chatFont.pointSize) : 10
    readonly property bool configuredFontBold: configuredFontForced && chatFont.bold
    readonly property bool configuredFontItalic: configuredFontForced && chatFont.italic
    readonly property bool configuredFontUnderline: configuredFontForced && chatFont.underline
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
    readonly property color timestampColor: usesCustomColors ? textColor
                                                              : (colorScheme === "System" ? systemPalette.text
                                                                                          : (darkSurface ? "#c5cedd" : "#4e5968"))
    readonly property color separatorColor: colorScheme === "System" ? systemPalette.mid
                                                               : (darkSurface ? "#6d7785" : "#858585")

    function isSystemEvent() { return systemEvent }
    function systemEventDescription() {
        return plainText.length > 0 ? plainText : protocolEventType
    }
    function messageText() {
        if (attachments.length === 1 && plainText.trim() === attachments[0].fileName.trim())
            return ""
        if (locationUri.length > 0 && plainText.trim() === locationUri.trim())
            return ""
        if (formattedText.length > 0)
            return formattedText
        return plainText
    }
    function displayedMessageText() {
        if (!emote)
            return messageText()
        return "* " + (ownEvent ? qsTr("You") : senderDisplayName) + " " + plainText
    }
    function replyText() {
        if (reply && reply.found)
            return qsTr("Reply to %1: %2").arg(reply.senderDisplayName).arg(reply.plainText)
        return qsTr("Reply to: %1").arg(replyToId)
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
    function availableActions() {
        return timelineActions ? timelineActions(stableId) : []
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
                icon.source: modelData.iconName ? "image://kaduicon/" + encodeURIComponent(modelData.iconName) : ""
                onTriggered: root.triggerAction(modelData.id)
            }
        }
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
                anchors.horizontalCenterOffset: -8
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
                    color: root.timestampColor
                    opacity: 0.75
                    font.family: root.configuredFontFamily
                    font.pointSize: Math.max(8, root.configuredFontPointSize - 2)
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

        Item {
            id: systemEventItem
            visible: root.isSystemEvent()
            width: parent.width
            implicitHeight: Math.max(systemSenderAvatar.visible ? systemSenderAvatar.height : 0,
                                     systemEventLabel.implicitHeight) + 6

            HoverHandler {
                id: systemEventHover
            }

            Rectangle {
                id: systemSenderAvatar
                anchors.left: parent.left
                anchors.leftMargin: 11
                anchors.verticalCenter: parent.verticalCenter
                visible: root.senderDisplayName.length > 0
                width: 18
                height: 18
                radius: width / 2
                clip: false
                color: systemSenderAvatarImage.status === Image.Ready
                       ? "transparent"
                       : (root.senderColor.a > 0 ? root.senderColor
                                                : (root.ownEvent ? root.outgoingSenderColor : root.incomingSenderColor))

                Image {
                    id: systemSenderAvatarImage
                    anchors.fill: parent
                    source: root.senderAvatarSource
                    fillMode: Image.PreserveAspectFit
                }

                Text {
                    anchors.centerIn: parent
                    visible: systemSenderAvatarImage.status !== Image.Ready
                    text: root.senderDisplayName.slice(0, 1).toUpperCase()
                    color: "white"
                    font.pixelSize: 10
                }
            }

            Item {
                id: systemText
                x: root.senderDisplayName.length > 0 ? 38 : 8
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - x - 16
                implicitHeight: systemEventLabel.implicitHeight

                Text {
                    id: systemSender
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    visible: root.senderDisplayName.length > 0
                    width: Math.min(implicitWidth, 120)
                    text: root.ownEvent ? qsTr("You") : root.senderDisplayName
                    color: root.ownEvent ? root.outgoingSenderColor : root.incomingSenderColor
                    elide: Text.ElideRight
                    font.bold: true
                    font.family: root.configuredFontFamily
                    font.pointSize: Math.max(8, root.configuredFontPointSize - 1)
                }

                Text {
                    id: systemEventLabel
                    anchors.left: systemSender.visible ? systemSender.right : parent.left
                    anchors.leftMargin: systemSender.visible ? 12 : 0
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.systemEventDescription()
                    color: root.textColor
                    opacity: 0.80
                    elide: Text.ElideRight
                    font.italic: true
                    font.family: root.configuredFontFamily
                    font.pointSize: Math.max(8, root.configuredFontPointSize - 1)
                }
            }

            KaduChat.TimelineActionsBar {
                anchors.right: parent.right
                anchors.top: parent.top
                actions: root.availableActions()
                executeAction: root.triggerAction
                fallbackTextColor: root.mutedTextColor
                backgroundColor: root.darkSurface ? "#39424d" : "#f7f8fa"
                shown: systemEventHover.hovered
                onReactionRequested: function(sourceItem) {
                    if (root.requestFullReactionSelector)
                        root.requestFullReactionSelector(root.stableId, sourceItem)
                }
            }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.RightButton
                onClicked: eventMenu.popup()
            }
        }

        Item {
            visible: !root.isSystemEvent()
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
                    implicitHeight: Math.max(senderAvatar.visible ? senderAvatar.height : 0,
                                             sender.implicitHeight, timestamp.implicitHeight)

                    Rectangle {
                        id: senderAvatar
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        visible: root.showAvatar && !root.emote
                        width: 24
                        height: 24
                        radius: width / 2
                        clip: false
                        color: senderAvatarImage.status === Image.Ready
                               ? "transparent"
                               : (root.senderColor.a > 0 ? root.senderColor
                                                        : (root.ownEvent ? root.outgoingSenderColor : root.incomingSenderColor))

                        Image {
                            id: senderAvatarImage
                            anchors.fill: parent
                            source: root.senderAvatarSource
                            fillMode: Image.PreserveAspectFit
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: senderAvatarImage.status !== Image.Ready
                            text: root.senderDisplayName.slice(0, 1).toUpperCase()
                            color: "white"
                            font.pixelSize: 10
                        }
                    }

                    Text {
                        id: sender
                        anchors.left: senderAvatar.visible ? senderAvatar.right : parent.left
                        anchors.leftMargin: senderAvatar.visible ? 6 : 0
                        anchors.right: timestamp.left
                        anchors.rightMargin: 8
                        visible: !root.emote
                        text: root.ownEvent ? qsTr("You") : root.senderDisplayName
                        elide: Text.ElideRight
                        color: root.ownEvent ? root.outgoingSenderColor : root.incomingSenderColor
                        font.bold: true
                        font.italic: false
                        font.family: root.configuredFontFamily
                        font.pointSize: root.configuredFontPointSize
                    }

                    Text {
                        id: timestamp
                        visible: root.showTimestamp
                        anchors.right: parent.right
                        anchors.rightMargin: 16
                        text: Qt.formatTime(root.timestamp, "HH:mm")
                        color: root.timestampColor
                        opacity: 0.70
                        font.family: root.configuredFontFamily
                        font.pointSize: Math.max(8, root.configuredFontPointSize - 2)
                    }
                }

                Item {
                    id: messageRow
                    x: 30
                    width: parent.width - x
                    implicitHeight: Math.max(message.implicitHeight, trailingTimestamp.implicitHeight)

                    Image {
                        id: redactedIcon
                        anchors.left: parent.left
                        anchors.verticalCenter: message.verticalCenter
                        visible: root.redacted
                        width: 16
                        height: 16
                        source: "image://kaduicon/edit-delete"
                    }

                    TextEdit {
                        id: message
                        x: redactedIcon.visible ? redactedIcon.width + 6 : 0
                        width: trailingTimestamp.visible ? parent.width - x - trailingTimestamp.implicitWidth - 8
                                                      : parent.width - x
                        text: root.redacted ? qsTr("Message removed") : root.displayedMessageText()
                        textFormat: !root.emote && root.formattedText.length > 0
                                    ? TextEdit.RichText : TextEdit.PlainText
                        color: root.textColor
                        wrapMode: TextEdit.Wrap
                        readOnly: true
                        selectByMouse: true
                        font.family: root.configuredFontFamily
                        font.pointSize: root.configuredFontPointSize
                        font.bold: root.configuredFontBold
                        font.italic: root.configuredFontItalic || root.emote
                        font.underline: root.configuredFontUnderline
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

                    Text {
                        id: trailingTimestamp
                        visible: !root.showSender && root.showTimestamp
                        anchors.right: parent.right
                        anchors.rightMargin: 16
                        anchors.bottom: parent.bottom
                        text: Qt.formatTime(root.timestamp, "HH:mm")
                        color: root.timestampColor
                        opacity: 0.70
                        font.family: root.configuredFontFamily
                        font.pointSize: Math.max(8, root.configuredFontPointSize - 2)
                    }
                }

                Text {
                    visible: root.replyToId.length > 0
                    width: parent.width
                    text: root.replyText()
                    color: root.mutedTextColor
                    elide: Text.ElideMiddle
                    font.family: root.configuredFontFamily
                    font.pointSize: Math.max(8, root.configuredFontPointSize - 2)
                }

                Text {
                    visible: root.edited && !root.redacted
                    width: parent.width
                    text: qsTr("edited")
                    color: root.mutedTextColor
                    font.family: root.configuredFontFamily
                    font.pointSize: Math.max(8, root.configuredFontPointSize - 2)
                    font.italic: true
                }

                Repeater {
                    model: root.attachments.filter(function(attachment) {
                        return Number(attachment.kind) !== 0
                    })

                    delegate: KaduChat.ChatFileAttachment {
                        required property var modelData
                        x: 30
                        width: parent ? parent.width - x : 1
                        attachment: modelData
                        textColor: root.textColor
                        linkColor: root.ownEvent ? root.outgoingSenderColor : root.incomingSenderColor
                        backgroundColor: "transparent"
                        borderColor: root.separatorColor
                        saveAttachment: root.actionId("saveAttachment") >= 0
                                        ? function() { root.triggerAction(root.actionId("saveAttachment")) } : null
                    }
                }

                KaduChat.ChatLocation {
                    visible: root.locationUri.length > 0
                    x: 30
                    availableWidth: parent.width - x
                    geoUri: root.locationUri
                    textColor: root.textColor
                    markerColor: root.ownEvent ? root.outgoingSenderColor : root.incomingSenderColor
                    openLocation: root.openLocation
                }

                Repeater {
                    model: root.attachments.filter(function(attachment) {
                        return Number(attachment.kind) === 0
                    })

                    delegate: KaduChat.ChatImageAttachment {
                        required property var modelData
                        x: 30
                        availableWidth: parent.width - x
                        attachment: modelData
                        placeholderColor: root.darkSurface ? "#5a6472" : "#b7c0cc"
                        placeholderTextColor: root.textColor
                        openImage: root.openImage
                    }
                }

                KaduChat.TimelineReactionsBar {
                    width: parent.width
                    leftInset: 30
                    reactions: root.reactions
                    removeOwnReaction: root.removeOwnReaction
                    textColor: root.textColor
                    accentColor: root.ownEvent ? root.outgoingSenderColor : root.incomingSenderColor
                    backgroundColor: root.darkSurface ? "#3d4652" : "#e8edf3"
                }

                Text {
                    visible: root.ownEvent && root.deliveryText().length > 0
                    width: parent.width
                    horizontalAlignment: Text.AlignRight
                    text: root.deliveryText()
                    color: root.deliveryState === 4 ? root.failureColor : root.mutedTextColor
                    opacity: root.deliveryState === 4 ? 1.0 : 0.50
                    font.family: root.configuredFontFamily
                    font.pointSize: Math.max(8, root.configuredFontPointSize - 2)
                }
            }

            KaduChat.TimelineActionsBar {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.rightMargin: 6
                anchors.topMargin: 6
                actions: root.availableActions()
                executeAction: root.triggerAction
                fallbackTextColor: root.mutedTextColor
                backgroundColor: root.darkSurface ? "#39424d" : "#f7f8fa"
                shown: hoverHandler.hovered
                onReactionRequested: function(sourceItem) {
                    if (root.requestFullReactionSelector)
                        root.requestFullReactionSelector(root.stableId, sourceItem)
                }
            }
        }
    }

}
