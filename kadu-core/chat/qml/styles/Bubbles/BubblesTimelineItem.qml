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
    property var frequentReactionEmojis: null
    property var addReaction: null
    property var requestFullReactionSelector: null
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
    readonly property color textColor: usesCustomColors ? customColors.buddyText
                                                         : (colorScheme === "System" ? systemPalette.text
                                                                                     : (darkSurface ? "#f2f4f8" : "#202020"))
    readonly property color outgoingTextColor: usesCustomColors ? customColors.myText
                                                                 : (colorScheme === "System" ? systemPalette.highlightedText
                                                                                             : textColor)
    readonly property color mutedTextColor: colorScheme === "System" ? systemPalette.mid
                                                                : (darkSurface ? "#b6c0cf" : "#5c6470")
    readonly property color timestampColor: ownEvent ? outgoingTextColor : textColor
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
    function hasImageAttachments() {
        for (let index = 0; index < attachments.length; ++index)
            if (attachments[index].kind === 0)
                return true
        return false
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
        spacing: 5

        Text {
            visible: root.startsNewDay
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.horizontalCenterOffset: -8
            text: Qt.formatDate(root.timestamp, "dddd, d MMMM")
            color: root.textColor
            opacity: 0.75
            font.family: root.configuredFontFamily
            font.pointSize: Math.max(8, root.configuredFontPointSize - 2)
        }

        Item {
            id: systemEventItem
            visible: root.isSystemEvent()
            x: 12
            width: parent.width - 32
            implicitHeight: Math.max(systemSenderAvatar.visible ? systemSenderAvatar.height : 0,
                                     systemEventLabel.implicitHeight) + 6

            HoverHandler {
                id: systemEventHover
            }

            Rectangle {
                id: systemSenderAvatar
                anchors.left: parent.left
                anchors.leftMargin: 5
                anchors.verticalCenter: parent.verticalCenter
                visible: root.senderDisplayName.length > 0
                width: 18
                height: 18
                radius: width / 2
                clip: false
                color: systemSenderAvatarImage.status === Image.Ready
                       ? "transparent"
                       : (root.senderColor.a > 0 ? root.senderColor
                                                : (root.ownEvent ? root.outgoingAvatarColor : root.incomingAvatarColor))

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
                x: root.senderDisplayName.length > 0 ? 35 : 0
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - x
                implicitHeight: systemEventLabel.implicitHeight

                Text {
                    id: systemSender
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    visible: root.senderDisplayName.length > 0
                    width: Math.min(implicitWidth, 120)
                    text: root.ownEvent ? qsTr("You") : root.senderDisplayName
                    color: root.ownEvent ? root.outgoingAvatarColor : root.incomingAvatarColor
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
                backgroundColor: root.darkSurface ? "#28333e" : "#e9eef4"
                shown: systemEventHover.hovered
                onReactionRequested: function(sourceItem) {
                    reactionSelector.openFor(sourceItem)
                }
            }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.RightButton
                onClicked: eventMenu.popup()
            }
        }

        Item {
            id: messageItem
            visible: !root.isSystemEvent()
            x: 12
            width: parent.width - 32
            implicitHeight: Math.max(messageColumn.implicitHeight,
                                     senderAvatar.visible ? senderAvatar.y + senderAvatar.height : 0)

            Rectangle {
                id: senderAvatar
                visible: root.showAvatar && !root.emote
                x: root.ownEvent ? parent.width - width : 0
                y: messageColumn.y + (senderName.visible
                                      ? senderName.implicitHeight + messageColumn.spacing : 0)
                width: 28
                height: 28
                radius: width / 2
                clip: false
                color: senderAvatarImage.status === Image.Ready
                       ? "transparent"
                       : (root.senderColor.a > 0 ? root.senderColor
                                                : (root.ownEvent ? root.outgoingAvatarColor : root.incomingAvatarColor))

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
                }
            }

            Column {
                id: messageColumn
                x: root.ownEvent ? (senderAvatar.visible ? parent.width - senderAvatar.width - 7 - width
                                                         : parent.width - width)
                                 : (senderAvatar.visible ? senderAvatar.width + 7 : 0)
                width: Math.max(1, Math.min(parent.width - (senderAvatar.visible ? senderAvatar.width + 7 : 0),
                                              Math.max(160, Math.floor(parent.width * 0.70))))
                spacing: 2

                Text {
                    id: senderName
                    visible: root.showSender && !root.emote
                    width: parent.width
                    text: root.ownEvent ? qsTr("You") : root.senderDisplayName
                    color: root.mutedTextColor
                    horizontalAlignment: root.ownEvent ? Text.AlignRight : Text.AlignLeft
                    font.bold: true
                    font.italic: false
                    font.family: root.configuredFontFamily
                    font.pointSize: root.configuredFontPointSize
                }

                Rectangle {
                    id: bubble
                    x: root.ownEvent ? parent.width - width : 0
                    width: root.hasImageAttachments() || root.locationUri.length > 0 || root.attachments.length > 0
                           ? parent.width
                           : Math.min(parent.width, Math.max(112, message.implicitWidth + 22))
                    implicitHeight: bubbleContent.implicitHeight + 14
                    radius: 8
                    color: root.ownEvent ? root.outgoingBubbleColor : root.incomingBubbleColor

                    HoverHandler {
                        id: hoverHandler
                    }

                    KaduChat.TimelineActionsBar {
                        anchors.top: parent.top
                        anchors.right: parent.right
                        anchors.margins: 4
                        actions: root.availableActions()
                        executeAction: root.triggerAction
                        fallbackTextColor: root.ownEvent ? root.outgoingTextColor : root.textColor
                        backgroundColor: root.ownEvent ? root.outgoingBubbleColor : root.incomingBubbleColor
                        shown: hoverHandler.hovered
                        z: 2
                        onReactionRequested: function(sourceItem) {
                            reactionSelector.openFor(sourceItem)
                        }
                    }

                    Column {
                        id: bubbleContent
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 4

                        Text {
                            visible: root.replyToId.length > 0
                            width: parent.width
                            text: root.replyText()
                            color: root.ownEvent ? root.outgoingTextColor : root.textColor
                            opacity: 0.70
                            elide: Text.ElideMiddle
                            font.pixelSize: 11
                        }

                        Item {
                            width: parent.width
                            implicitHeight: message.implicitHeight

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
                                width: parent.width - x
                                text: root.redacted ? qsTr("Message removed") : root.displayedMessageText()
                                textFormat: !root.emote && root.formattedText.length > 0
                                            ? TextEdit.RichText : TextEdit.PlainText
                                color: root.ownEvent ? root.outgoingTextColor : root.textColor
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
                        }

                        Repeater {
                            model: root.attachments.filter(function(attachment) {
                                return Number(attachment.kind) !== 0
                            })

                            delegate: KaduChat.ChatFileAttachment {
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

                        KaduChat.ChatLocation {
                            visible: root.locationUri.length > 0
                            geoUri: root.locationUri
                            textColor: root.ownEvent ? root.outgoingTextColor : root.textColor
                            markerColor: root.ownEvent ? root.outgoingAvatarColor : root.incomingAvatarColor
                            openLocation: root.openLocation
                        }

                        Repeater {
                            model: root.attachments.filter(function(attachment) {
                                return Number(attachment.kind) === 0
                            })

                            delegate: KaduChat.ChatImageAttachment {
                                required property var modelData
                                attachment: modelData
                                placeholderColor: root.ownEvent ? root.outgoingBubbleColor : root.incomingBubbleColor
                                placeholderTextColor: root.ownEvent ? root.outgoingTextColor : root.textColor
                                openImage: root.openImage
                            }
                        }

                        Text {
                            visible: root.edited && !root.redacted
                            width: parent.width
                            text: qsTr("edited")
                            color: root.ownEvent ? root.outgoingTextColor : root.textColor
                            opacity: 0.70
                            font.family: root.configuredFontFamily
                            font.pointSize: Math.max(8, root.configuredFontPointSize - 2)
                            font.italic: true
                        }
                    }
                }

                KaduChat.TimelineReactionsBar {
                    x: bubble.x
                    width: bubble.width
                    alignRight: root.ownEvent
                    reactions: root.reactions
                    removeOwnReaction: root.removeOwnReaction
                    textColor: root.ownEvent ? root.outgoingTextColor : root.textColor
                    accentColor: root.ownEvent ? root.outgoingAvatarColor : root.incomingAvatarColor
                    backgroundColor: root.ownEvent ? root.outgoingBubbleColor : root.incomingBubbleColor
                }

                Text {
                    visible: root.showTimestamp
                    x: bubble.x
                    width: bubble.width
                    text: Qt.formatTime(root.timestamp, "HH:mm")
                    color: root.timestampColor
                    horizontalAlignment: root.ownEvent ? Text.AlignRight : Text.AlignLeft
                    opacity: 0.70
                    font.family: root.configuredFontFamily
                    font.pointSize: Math.max(8, root.configuredFontPointSize - 2)
                }

                Text {
                    visible: root.ownEvent && root.deliveryText().length > 0
                    x: bubble.x
                    width: bubble.width
                    text: root.deliveryText()
                    color: root.deliveryState === 4 ? "#ff8b8b" : root.mutedTextColor
                    horizontalAlignment: Text.AlignRight
                    font.family: root.configuredFontFamily
                    font.pointSize: Math.max(8, root.configuredFontPointSize - 2)
                }
            }
        }
    }

    KaduChat.ReactionSelectorPopup {
        id: reactionSelector
        parent: root
        stableId: root.stableId
        emojiProvider: root.frequentReactionEmojis
        addReaction: root.addReaction
        requestFullSelector: root.requestFullReactionSelector
        textColor: root.ownEvent ? root.outgoingTextColor : root.textColor
        backgroundColor: root.ownEvent ? root.outgoingBubbleColor : root.incomingBubbleColor
    }
}
