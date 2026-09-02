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
import "qrc:/Kadu/Chat/chat/qml" as KaduChat

Item {
    id: root

    property string colorScheme: "System"
    property var customColors: ({ "enabled": false })
    property var pinnedMessages: []
    property var timelineActions: null
    property var executeTimelineAction: null
    property var chatFont: ({})
    property var openUrl: null
    property var openImage: null
    property var openLocation: null
    property var copyText: null
    property var removeOwnReaction: null
    property var addReaction: null
    property var requestFullReactionSelector: null
    property var jumpToTimelineItem: null
    property bool showTrigger: false

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
    readonly property var entries: pinnedMessages || []

    implicitHeight: showTrigger && visible ? pinButton.implicitHeight + 8 : 0
    visible: entries.length > 0

    onEntriesChanged: {
        if (entries.length === 0)
            pinnedMessagesPopup.close()
    }

    function actionsFor(stableId) {
        return timelineActions ? timelineActions(stableId) : []
    }

    function showPinnedMessages() {
        if (entries.length > 0)
            pinnedMessagesPopup.open()
    }

    function togglePinnedMessages() {
        if (pinnedMessagesPopup.opened)
            pinnedMessagesPopup.close()
        else
            showPinnedMessages()
    }

    SystemPalette {
        id: systemPalette
        colorGroup: SystemPalette.Active
    }

    Rectangle {
        anchors.fill: parent
        visible: root.showTrigger
        color: root.backgroundColor
        border.width: 1
        border.color: root.colorScheme === "System" ? systemPalette.mid
                                                      : (root.darkSurface ? "#4d5662" : "#c7cdd5")
    }

    Button {
        id: pinButton
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        visible: root.showTrigger
        text: root.entries.length === 1 ? qsTr("1 pinned message")
                                        : qsTr("%1 pinned messages").arg(root.entries.length)
        Accessible.name: text
        onClicked: root.showPinnedMessages()
    }

    Popup {
        id: pinnedMessagesPopup
        parent: root
        x: 8
        y: root.height + 4
        width: Math.max(280, Math.min(root.width - 16, 560))
        height: Math.min(440, content.implicitHeight + topPadding + bottomPadding)
        padding: 10
        modal: true
        dim: true
        z: 10
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            color: root.colorScheme === "System" ? systemPalette.base
                                                   : (root.darkSurface ? "#252a31" : "#ffffff")
            border.width: 1
            border.color: root.colorScheme === "System" ? systemPalette.mid
                                                          : (root.darkSurface ? "#596472" : "#b8c0ca")
            radius: 4
        }

        contentItem: Column {
            id: content
            width: pinnedMessagesPopup.availableWidth
            spacing: 8

            Text {
                width: parent.width
                text: qsTr("Pinned messages")
                color: root.textColor
                font.bold: true
            }

            ListView {
                id: pinnedMessagesList
                width: parent.width
                height: Math.min(contentHeight, 360)
                clip: true
                spacing: 6
                model: root.entries

                delegate: Item {
                    id: entryCard
                    required property var modelData
                    readonly property var entry: modelData

                    width: pinnedMessagesList.width
                    implicitHeight: timelineEntry.item ? timelineEntry.item.implicitHeight : entryContent.implicitHeight + 14

                    MouseArea {
                        anchors.fill: timelineEntry
                        enabled: entry.available === true
                        z: 2
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            pinnedMessagesPopup.close()
                            if (root.jumpToTimelineItem)
                                root.jumpToTimelineItem(entry.stableId)
                        }
                    }

                    Component {
                        id: timelineItemComponent

                        KaduClassicTimelineItem {
                            width: parent ? parent.width : 1
                            stableId: entry.stableId || ""
                            protocolEventType: entry.protocolEventType || ""
                            kind: Number(entry.kind)
                            timestamp: entry.timestamp
                            ownEvent: entry.ownEvent === true
                            senderDisplayName: entry.senderDisplayName || ""
                            senderAvatarSource: entry.senderAvatarSource || ""
                            senderColor: entry.senderColor
                            plainText: entry.plainText || ""
                            formattedText: entry.formattedText || ""
                            replyToId: entry.replyToId || ""
                            reply: entry.reply || ({})
                            attachments: entry.attachments || []
                            locationUri: entry.locationUri || ""
                            reactions: entry.reactions || []
                            showSender: entry.showSender !== false
                            showAvatar: entry.showAvatar !== false
                            showTimestamp: entry.showTimestamp !== false
                            startsNewDay: false
                            deliveryState: Number(entry.deliveryState)
                            edited: entry.edited === true
                            systemEvent: entry.systemEvent === true
                            emote: entry.emote === true
                            redacted: entry.redacted === true
                            encrypted: entry.encrypted === true
                            decryptionState: Number(entry.decryptionState)
                            errorText: entry.errorText || ""
                            forceIncomingAlignment: true
                            colorScheme: root.colorScheme
                            customColors: root.customColors
                            chatFont: root.chatFont
                            openUrl: root.openUrl
                            openImage: root.openImage
                            openLocation: root.openLocation
                            timelineActions: root.timelineActions
                            executeTimelineAction: root.executeTimelineAction
                            copyText: root.copyText
                            removeOwnReaction: root.removeOwnReaction
                            addReaction: root.addReaction
                            requestFullReactionSelector: root.requestFullReactionSelector
                            jumpToTimelineItem: root.jumpToTimelineItem
                        }
                    }

                    Loader {
                        id: timelineEntry
                        anchors.left: parent.left
                        anchors.right: parent.right
                        active: entry.available === true
                        sourceComponent: timelineItemComponent
                    }

                    Column {
                        id: entryContent
                        visible: !entry.available
                        x: 7
                        y: 7
                        width: parent.width - 14
                        spacing: 3

                        Text {
                            width: parent.width
                            visible: entry.senderDisplayName && entry.senderDisplayName.length > 0
                            text: entry.senderDisplayName || ""
                            color: root.textColor
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        Text {
                            width: parent.width
                            text: entry.available ? (entry.plainText || qsTr("This message is no longer available."))
                                                  : qsTr("This pinned message is not loaded yet.")
                            textFormat: entry.formattedText && entry.formattedText.length > 0 ? Text.RichText : Text.PlainText
                            color: root.mutedTextColor
                            wrapMode: Text.Wrap
                            maximumLineCount: 3
                            elide: Text.ElideRight
                        }

                        Button {
                            width: parent.width
                            visible: !entry.available
                            text: qsTr("Open message")
                            Accessible.name: text
                            onClicked: {
                                pinnedMessagesPopup.close()
                                if (root.jumpToTimelineItem)
                                    root.jumpToTimelineItem(entry.stableId)
                            }
                        }

                        Row {
                            width: parent.width
                            spacing: 6

                            Repeater {
                                model: root.actionsFor(entry.stableId)

                                delegate: Button {
                                    required property var modelData
                                    text: modelData.text || ""
                                    enabled: modelData.enabled === undefined || modelData.enabled
                                    Accessible.name: text
                                    onClicked: {
                                        if (root.executeTimelineAction)
                                            root.executeTimelineAction(entry.stableId, modelData.id)
                                        pinnedMessagesPopup.close()
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
