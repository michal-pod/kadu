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

    visible: false
    property string colorScheme: "System"
    property var customColors: ({ "enabled": false })
    property var chatFont: ({ "family": "", "pointSize": 10, "bold": false, "italic": false,
                              "underline": false, "forced": false })
    property var roomInfo: ({})
    property var openUrl: null
    property var openImage: null
    property var openLocation: null
    property var timelineActions: null
    property var executeTimelineAction: null
    property var copyText: null
    property var removeOwnReaction: null
    property var addReaction: null
    property var requestFullReactionSelector: null

    SystemPalette {
        id: systemPalette
        colorGroup: SystemPalette.Active
    }

    readonly property bool darkSurface: colorScheme === "Dark" || colorScheme === "HighContrast" ||
                                       (colorScheme !== "Light" && systemPalette.base.r * 0.2126 +
                                        systemPalette.base.g * 0.7152 +
                                        systemPalette.base.b * 0.0722 < 0.5)
    readonly property bool highContrast: colorScheme === "HighContrast"
    property color backgroundColor: highContrast ? "#000000" : (darkSurface ? "#1e242c" : "#f5f7fa")
    property color textColor: highContrast ? "#ffffff" : (darkSurface ? "#f3f6fa" : "#17202a")
    property color mutedTextColor: highContrast ? "#ffff00" : (darkSurface ? "#b6c4d2" : "#536273")
    property color separatorColor: highContrast ? "#ffffff" : (darkSurface ? "#627181" : "#a9b4c0")
    property color loadingTextColor: textColor
    property color roomHeaderBackgroundColor: highContrast ? "#000000" : (darkSurface ? "#28333e" : "#e9eef4")
    property int timelineMargin: 10
    property int timelineSpacing: 5
    property int groupingIntervalSeconds: 300
    property Component timelineItem: timelineItemComponent
    property Component composerContext: composerContextComponent
    property Component pinnedMessagesPanel: pinnedMessagesPanelComponent

    Component {
        id: timelineItemComponent

        Item {
            id: item

            required property string stableId
            required property string protocolEventType
            required property int kind
            required property var timestamp
            required property bool ownEvent
            property string senderId: ""
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
            property string colorScheme: root.colorScheme
            property var customColors: root.customColors
            property var openUrl: root.openUrl
            property var openImage: null
            property var openLocation: null
            property var timelineActions: root.timelineActions
            property var executeTimelineAction: root.executeTimelineAction
            property var copyText: root.copyText
            property var removeOwnReaction: null
            property var addReaction: null
            property var requestFullReactionSelector: null
            property var jumpToTimelineItem: null
            property var chatFont: root.chatFont

            implicitHeight: card.implicitHeight

            function valueLabel(label, value) {
                return label + ": " + value
            }

            function attachmentSummary() {
                const values = []
                for (let index = 0; index < attachments.length; ++index) {
                    const attachment = attachments[index]
                    values.push("kind=" + attachment.kind +
                                " | fileName=" + attachment.fileName +
                                " | mimeType=" + attachment.mimeType +
                                " | size=" + attachment.size +
                                " | dimensions=" + attachment.dimensions +
                                " | duration=" + attachment.duration +
                                " | sourceUri=" + attachment.sourceUri +
                                " | thumbnailUri=" + attachment.thumbnailUri +
                                " | state=" + attachment.state +
                                " | progress=" + attachment.progress +
                                " | localResourceId=" + attachment.localResourceId +
                                " | errorText=" + attachment.errorText +
                                " | iconSource=" + attachment.iconSource)
                }
                return values.join("\n")
            }

            function reactionsSummary() {
                const values = []
                for (let index = 0; index < reactions.length; ++index) {
                    const reaction = reactions[index]
                    values.push("key=" + reaction.key +
                                " | senderIds=" + reaction.senderIds.join(", ") +
                                " | senderDisplayNames=" + reaction.senderDisplayNames.join(", ") +
                                " | own=" + reaction.own)
                }
                return values.join("\n")
            }

            function replySummary() {
                if (!reply || !reply.id)
                    return ""
                return "id=" + reply.id +
                       " | found=" + reply.found +
                       " | senderDisplayName=" + (reply.senderDisplayName || "") +
                       " | plainText=" + (reply.plainText || "") +
                       " | formattedText=" + (reply.formattedText || "")
            }

            Rectangle {
                id: card
                width: parent.width
                implicitHeight: details.implicitHeight + 16
                radius: 5
                border.width: 1
                border.color: root.separatorColor
                color: item.ownEvent ? (root.darkSurface ? "#263c55" : "#e1effc")
                                     : (root.darkSurface ? "#303842" : "#ffffff")

                Column {
                    id: details
                    x: 8
                    y: 8
                    width: parent.width - 16
                    spacing: 3

                    Text {
                        width: parent.width
                        color: root.textColor
                        font.bold: true
                        wrapMode: Text.Wrap
                        text: item.valueLabel("stableId", item.stableId) + " | " +
                              item.valueLabel("protocolEventType", item.protocolEventType) + " | " +
                              item.valueLabel("kind", item.kind) + " | " +
                              item.valueLabel("timestamp", item.timestamp)
                    }

                    Text {
                        width: parent.width
                        color: root.mutedTextColor
                        wrapMode: Text.Wrap
                        text: item.valueLabel("senderDisplayName", item.senderDisplayName) + " | " +
                              item.valueLabel("senderId", item.senderId) + " | " +
                              item.valueLabel("senderAvatarSource", item.senderAvatarSource) + " | " +
                              item.valueLabel("senderColor", item.senderColor) + " | " +
                              item.valueLabel("ownEvent", item.ownEvent) + " | " +
                              item.valueLabel("showSender", item.showSender) + " | " +
                              item.valueLabel("showAvatar", item.showAvatar) + " | " +
                              item.valueLabel("showTimestamp", item.showTimestamp) + " | " +
                              item.valueLabel("startsNewDay", item.startsNewDay)
                    }

                    TextEdit {
                        width: parent.width
                        height: contentHeight
                        readOnly: true
                        selectByMouse: true
                        textFormat: item.formattedText.length > 0 ? TextEdit.RichText : TextEdit.PlainText
                        color: root.textColor
                        wrapMode: TextEdit.Wrap
                        text: item.formattedText.length > 0 ? item.formattedText : item.plainText
                        onLinkActivated: {
                            if (item.openUrl)
                                item.openUrl(link)
                        }
                    }

                    Text {
                        width: parent.width
                        color: root.mutedTextColor
                        wrapMode: Text.Wrap
                        text: item.valueLabel("plainText", item.plainText) + "\n" +
                              item.valueLabel("replyToId", item.replyToId) + " | " +
                              item.valueLabel("reply", item.replySummary()) + "\n" +
                              item.valueLabel("attachments", item.attachmentSummary()) + "\n" +
                              item.valueLabel("locationUri", item.locationUri) + " | " +
                              item.valueLabel("reactions", item.reactionsSummary()) + "\n" +
                              item.valueLabel("deliveryState", item.deliveryState) + " | " +
                              item.valueLabel("edited", item.edited) + " | " +
                              item.valueLabel("systemEvent", item.systemEvent) + " | " +
                              item.valueLabel("emote", item.emote) + " | " +
                              item.valueLabel("redacted", item.redacted) + " | " +
                              item.valueLabel("encrypted", item.encrypted) + " | " +
                              item.valueLabel("decryptionState", item.decryptionState) + "\n" +
                              item.valueLabel("errorText", item.errorText) + " | " +
                              item.valueLabel("colorScheme", item.colorScheme) + " | " +
                              item.valueLabel("chatFont", JSON.stringify(item.chatFont))
                    }

                    Flow {
                        width: parent.width
                        spacing: 5

                        Repeater {
                            model: item.timelineActions ? item.timelineActions(item.stableId) : []

                            delegate: Button {
                                required property var modelData
                                text: modelData.text || modelData.key || "action"
                                onClicked: {
                                    if (item.executeTimelineAction)
                                        item.executeTimelineAction(item.stableId, modelData.id)
                                }
                            }
                        }

                        Button {
                            text: qsTr("Remove own reaction")
                            visible: item.reactions.some(function(reaction) { return reaction.own })
                            onClicked: {
                                const reaction = item.reactions.find(function(candidate) { return candidate.own })
                                if (reaction && item.removeOwnReaction)
                                    item.removeOwnReaction(item.stableId, reaction.key)
                            }
                        }

                        Button {
                            text: qsTr("Jump to replied message")
                            visible: item.reply && item.reply.found
                            onClicked: {
                                if (item.jumpToTimelineItem)
                                    item.jumpToTimelineItem(item.reply.id)
                            }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: composerContextComponent

        Item {
            id: contextItem

            property var context: ({})
            property var cancelComposerContext: null
            readonly property var target: context && context.target ? context.target : ({})

            implicitHeight: details.implicitHeight + 16

            Rectangle {
                anchors.fill: parent
                color: root.darkSurface ? "#3d2f4d" : "#f4eaff"
                border.width: 1
                border.color: "#a359d1"
            }

            Column {
                id: details
                x: 8
                y: 8
                width: parent.width - 16
                spacing: 3

                Text {
                    width: parent.width
                    color: root.textColor
                    font.bold: true
                    text: "composerContext: mode=" + (contextItem.context.mode || "") +
                          " | target.id=" + (contextItem.target.id || "")
                    wrapMode: Text.Wrap
                }

                Text {
                    width: parent.width
                    color: root.mutedTextColor
                    text: "senderDisplayName=" + (contextItem.target.senderDisplayName || "") +
                          " | protocolEventType=" + (contextItem.target.protocolEventType || "") +
                          " | kind=" + (contextItem.target.kind || "") + "\n" +
                          "plainText=" + (contextItem.target.plainText || "") + "\n" +
                          "formattedText=" + (contextItem.target.formattedText || "") + "\n" +
                          "edited=" + (contextItem.target.edited || false) +
                          " | redacted=" + (contextItem.target.redacted || false)
                    wrapMode: Text.Wrap
                }

                Text {
                    color: "#a359d1"
                    text: qsTr("Cancel")
                    font.bold: true

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (contextItem.cancelComposerContext)
                                contextItem.cancelComposerContext()
                        }
                    }
                }
            }
        }
    }

    Component {
        id: pinnedMessagesPanelComponent

        Item {
            id: panel

            property var pinnedMessages: []
            property var openUrl: null
            property var openImage: null
            property var openLocation: null
            property var timelineActions: null
            property var executeTimelineAction: null
            property var copyText: null
            property var removeOwnReaction: null
            property var addReaction: null
            property var requestFullReactionSelector: null
            readonly property var entries: pinnedMessages || []

            visible: entries.length > 0
            implicitHeight: visible ? summary.implicitHeight + 12 : 0

            onEntriesChanged: {
                if (entries.length === 0)
                    pinnedMessagesPopup.close()
            }

            Rectangle {
                anchors.fill: parent
                color: root.darkSurface ? "#363040" : "#f6edff"
                border.width: 1
                border.color: "#a359d1"
            }

            Text {
                id: summary
                x: 8
                y: 6
                width: parent.width - 16
                color: root.textColor
                wrapMode: Text.Wrap
                text: "pinnedMessages.count=" + panel.entries.length + "\n" +
                      panel.entries.map(function(entry) {
                          return "stableId=" + (entry.stableId || "") +
                                 " | available=" + (entry.available || false) +
                                 " | senderDisplayName=" + (entry.senderDisplayName || "") +
                                 " | plainText=" + (entry.plainText || "") +
                                 " | formattedText=" + (entry.formattedText || "") +
                                 " | protocolEventType=" + (entry.protocolEventType || "") +
                                 " | timestamp=" + (entry.timestamp || "") +
                                 " | encrypted=" + (entry.encrypted || false) +
                                 " | decryptionState=" + (entry.decryptionState || 0) +
                                 " | redacted=" + (entry.redacted || false)
                      }).join("\n")

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: pinnedMessagesPopup.open()
                }
            }

            Popup {
                id: pinnedMessagesPopup
                parent: panel
                x: 8
                y: panel.height + 4
                width: Math.max(280, Math.min(panel.width - 16, 560))
                height: Math.min(440, pinnedMessagesList.contentHeight + 20)
                padding: 10
                z: 10
                closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

                contentItem: ListView {
                    id: pinnedMessagesList
                    width: pinnedMessagesPopup.availableWidth
                    height: Math.min(contentHeight, 400)
                    spacing: 6
                    model: panel.entries

                    delegate: Column {
                        required property var modelData
                        readonly property var entry: modelData

                        width: pinnedMessagesList.width
                        spacing: 3

                        Text {
                            width: parent.width
                            color: root.textColor
                            wrapMode: Text.Wrap
                            text: "stableId=" + (entry.stableId || "") +
                                  " | available=" + (entry.available || false) +
                                  " | senderDisplayName=" + (entry.senderDisplayName || "") +
                                  " | plainText=" + (entry.plainText || "") +
                                  " | formattedText=" + (entry.formattedText || "") +
                                  " | protocolEventType=" + (entry.protocolEventType || "") +
                                  " | timestamp=" + (entry.timestamp || "") +
                                  " | encrypted=" + (entry.encrypted || false) +
                                  " | decryptionState=" + (entry.decryptionState || 0) +
                                  " | redacted=" + (entry.redacted || false)
                        }

                        Row {
                            spacing: 5

                            Repeater {
                                model: panel.timelineActions ? panel.timelineActions(entry.stableId) : []

                                delegate: Button {
                                    required property var modelData
                                    text: modelData.text || ""
                                    onClicked: {
                                        if (panel.executeTimelineAction)
                                            panel.executeTimelineAction(entry.stableId, modelData.id)
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
