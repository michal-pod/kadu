/*
 * %kadu copyright begin%
 * Copyright 2026 Kadu Qt6 port
 * %kadu copyright end%
 */

import QtQuick
import QtQuick.Controls
import "qrc:/Kadu/Chat/chat/qml" as KaduChat

Item {
    id: root

    required property string stableId
    required property string protocolEventType
    required property int kind
    required property var timestamp
    required property bool ownEvent
    property string senderId: ""
    required property string senderDisplayName
    property url senderAvatarSource: ""
    property color senderColor: "transparent"
    required property string plainText
    required property string formattedText
    property string replyToId: ""
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
    property var chatFont: ({})
    property string terminalFont: "monospace"
    property var openUrl: null
    property var openImage: null
    property var openLocation: null
    property var timelineActions: null
    property var executeTimelineAction: null
    property var copyText: null
    property var removeOwnReaction: null
    property var addReaction: null
    property var requestFullReactionSelector: null

    readonly property color timestampColor: "#b4b4b4"
    readonly property color ownNickColor: "#ffffff"
    readonly property color remoteNickColor: "#a8a8a8"
    readonly property color textColor: "#d9d9d9"
    readonly property color mutedColor: "#8e8e8e"
    readonly property color errorColor: "#ff5f5f"
    readonly property color reactionColor: "#79c879"
    readonly property real fontSize: 10

    implicitHeight: content.implicitHeight + 2

    function timestampText() { return Qt.formatTime(timestamp, "HH:mm:ss") }
    function dayChangeText() { return qsTr("Day changed to %1").arg(Qt.formatDate(timestamp, "dd MMM yyyy")) }
    function nickText() { return ownEvent ? (senderDisplayName || qsTr("You")) : senderDisplayName }
    function actorText() {
        const name = nickText() || qsTr("Unknown")
        return senderId.length > 0 ? name + " [" + senderId + "]" : name
    }
    function messageText() {
        if (redacted)
            return qsTr("message removed")
        return plainText + (edited ? " [" + qsTr("edited") + "]" : "")
    }
    function actions() { return timelineActions ? timelineActions(stableId) : [] }
    function triggerAction(id) {
        if (executeTimelineAction)
            executeTimelineAction(stableId, id)
    }
    function attachmentText(attachment) {
        const state = Number(attachment.state)
        const stateText = state === 1 ? qsTr("downloading %1%").arg(Math.round(Number(attachment.progress) * 100))
                        : state === 3 ? qsTr("transfer failed") : qsTr("ready")
        return "[" + (attachment.fileName || qsTr("attachment")) + " — " + stateText + "]"
    }
    function encryptionEmoji() {
        if (!encrypted || decryptionState === 0)
            return ""
        if (decryptionState === 1)
            return "⌛"
        if (decryptionState === 2)
            return "🔒"
        if (decryptionState === 4)
            return "🔐"
        return "⚠"
    }
    function encryptionDescription() {
        if (decryptionState === 1)
            return qsTr("Encrypted event; waiting for a decryption key")
        if (decryptionState === 2)
            return qsTr("Encrypted and successfully decrypted")
        if (decryptionState === 4)
            return qsTr("Encrypted event; decryption key is unavailable")
        return qsTr("Encrypted event could not be decrypted")
    }

    Menu {
        id: eventMenu
        property var entries: []
        onAboutToShow: entries = root.actions()
        onClosed: entries = []

        Repeater {
            model: eventMenu.entries

            delegate: MenuItem {
                required property var modelData
                text: modelData.text || ""
                onTriggered: root.triggerAction(modelData.id)
            }
        }
    }

    Column {
        id: content
        width: parent.width
        spacing: 0

        // Irssi prints its stock daychange format as a standalone line, with
        // no timestamp, before the first entry of the new day.
        Text {
            width: parent.width
            visible: root.startsNewDay
            text: root.dayChangeText()
            color: root.textColor
            font.family: root.terminalFont
            font.pointSize: root.fontSize
        }

        Row {
            width: parent.width
            spacing: 6

            Row {
                width: implicitWidth
                height: implicitHeight
                spacing: 5

                Text {
                    text: root.timestampText()
                    color: root.timestampColor
                    font.family: root.terminalFont
                    font.pointSize: root.fontSize
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: root.encryptionEmoji().length > 0
                    text: root.encryptionEmoji()
                    font.pixelSize: 12

                    HoverHandler {
                        id: encryptionHover
                    }

                    ToolTip.visible: encryptionHover.hovered
                    ToolTip.text: root.decryptionState !== 2 && root.errorText.length > 0
                                  ? root.encryptionDescription() + ": " + root.errorText
                                  : root.encryptionDescription()
                    ToolTip.delay: 350
                }
            }

            Text {
                width: parent.width - x
                visible: root.systemEvent
                text: "-!- " + root.actorText() + " " + (root.plainText || root.protocolEventType)
                color: root.mutedColor
                wrapMode: Text.Wrap
                font.family: root.terminalFont
                font.pointSize: root.fontSize
            }

            Item {
                visible: !root.systemEvent
                width: parent.width - x
                implicitHeight: messageRow.implicitHeight +
                                (replyLine.visible ? replyLine.implicitHeight : 0) +
                                (mediaColumn.visible ? mediaColumn.implicitHeight : 0) +
                                (reactionsLine.visible ? reactionsLine.implicitHeight : 0)
                height: implicitHeight

                Column {
                    width: parent.width
                    spacing: 0

                    Row {
                        id: replyLine
                        visible: root.replyToId.length > 0
                        width: parent.width
                        spacing: 5

                        Text {
                            text: ">"
                            color: root.mutedColor
                            font.family: root.terminalFont
                            font.pointSize: root.fontSize
                        }
                        Text {
                            text: root.reply && root.reply.found
                                  ? "<" + (root.reply.senderDisplayName || qsTr("Unknown")) + ">"
                                  : "<" + qsTr("message unavailable") + ">"
                            color: root.mutedColor
                            font.family: root.terminalFont
                            font.pointSize: root.fontSize
                        }
                        Text {
                            width: parent.width - x
                            text: root.reply && root.reply.found ? (root.reply.plainText || "") : ""
                            color: root.mutedColor
                            elide: Text.ElideRight
                            font.family: root.terminalFont
                            font.pointSize: root.fontSize
                        }
                    }

                    Row {
                        id: messageRow
                        width: parent.width
                        spacing: 5

                        Text {
                            visible: !root.emote
                            text: "<" + root.nickText() + ">"
                            color: root.ownEvent ? root.ownNickColor : root.remoteNickColor
                            font.family: root.terminalFont
                            font.pointSize: root.fontSize
                            font.bold: root.ownEvent
                        }
                        Text {
                            visible: root.emote
                            text: "* " + root.nickText()
                            color: root.ownEvent ? root.ownNickColor : root.remoteNickColor
                            font.family: root.terminalFont
                            font.pointSize: root.fontSize
                            font.bold: root.ownEvent
                        }
                        Text {
                            width: parent.width - x
                            text: root.messageText()
                            color: root.redacted ? root.mutedColor : root.textColor
                            wrapMode: Text.Wrap
                            font.family: root.terminalFont
                            font.pointSize: root.fontSize
                            font.italic: root.redacted
                        }
                    }

                    Column {
                        id: mediaColumn
                        width: parent.width
                        visible: root.attachments.length > 0 || root.locationUri.length > 0
                        spacing: 3

                        Repeater {
                            model: root.attachments

                            delegate: Column {
                                required property var modelData
                                width: parent.width
                                spacing: 2

                                Text {
                                    x: 2
                                    text: root.attachmentText(modelData)
                                    color: Number(modelData.state) === 3 ? root.errorColor : root.mutedColor
                                    font.family: root.terminalFont
                                    font.pointSize: root.fontSize
                                }

                                Rectangle {
                                    visible: Number(modelData.kind) === 0
                                    width: Math.min(parent.width, 420)
                                    height: imagePreview.height + 2
                                    color: "#111111"
                                    border.width: 1
                                    border.color: "#0000b8"

                                    KaduChat.ChatImageAttachment {
                                        id: imagePreview
                                        anchors.centerIn: parent
                                        attachment: modelData
                                        openImage: root.openImage
                                        maximumWidth: Math.min(parent.width - 2, 416)
                                        maximumHeight: 250
                                        placeholderColor: "#202020"
                                        placeholderTextColor: "#d9d9d9"
                                    }
                                }
                            }
                        }

                        Rectangle {
                            visible: root.locationUri.length > 0
                            width: Math.min(parent.width, 420)
                            height: locationPreview.implicitHeight + 2
                            color: "#111111"
                            border.width: 1
                            border.color: "#0000b8"

                            Loader {
                                id: locationPreview
                                anchors.centerIn: parent
                                width: Math.min(parent.width - 2, 416)
                                active: root.locationUri.length > 0

                                sourceComponent: Component {
                                    KaduChat.ChatLocation {
                                        availableWidth: locationPreview.width
                                        maximumWidth: locationPreview.width
                                        geoUri: root.locationUri
                                        openLocation: root.openLocation
                                        textColor: "#d9d9d9"
                                        markerColor: "#0000ff"
                                    }
                                }
                            }
                        }
                    }

                    Row {
                        id: reactionsLine
                        width: parent.width
                        visible: root.reactions.length > 0
                        spacing: 4

                        Repeater {
                            model: root.reactions

                            delegate: Text {
                                required property var modelData

                                text: "[" + modelData.key + " " + modelData.senderIds.length + "]"
                                color: root.reactionColor
                                font.family: root.terminalFont
                                font.pointSize: root.fontSize

                                MouseArea {
                                    id: reactionArea
                                    anchors.fill: parent
                                    enabled: modelData.own === true && root.removeOwnReaction
                                    hoverEnabled: enabled
                                    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                    onClicked: root.removeOwnReaction(root.stableId, modelData.key)
                                }

                                ToolTip.visible: reactionArea.containsMouse
                                ToolTip.text: qsTr("Click to remove your reaction")
                                ToolTip.delay: 350
                            }
                        }
                    }
                }
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onClicked: eventMenu.popup()
    }
}
