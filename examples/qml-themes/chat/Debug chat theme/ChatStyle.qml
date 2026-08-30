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

    visible: false
    property string colorScheme: "System"
    property var customColors: ({ "enabled": false })
    property var openUrl: null
    property var timelineActions: null
    property var executeTimelineAction: null
    property var copyText: null

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

    Component {
        id: timelineItemComponent

        Item {
            id: item

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
            property string colorScheme: root.colorScheme
            property var customColors: root.customColors
            property var openUrl: root.openUrl
            property var openImage: null
            property var timelineActions: root.timelineActions
            property var executeTimelineAction: root.executeTimelineAction
            property var copyText: root.copyText

            implicitHeight: card.implicitHeight

            function valueLabel(label, value) {
                return label + ": " + value
            }

            function attachmentSummary() {
                const values = []
                for (let index = 0; index < attachments.length; ++index) {
                    const attachment = attachments[index]
                    values.push(attachment.fileName + " (" + attachment.mimeType + ", " + attachment.size + " B)")
                }
                return values.join("; ")
            }

            function reactionsSummary() {
                const values = []
                for (let index = 0; index < reactions.length; ++index) {
                    const reaction = reactions[index]
                    values.push(reaction.key + " from " + reaction.senderDisplayNames.join(", "))
                }
                return values.join("; ")
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
                              item.valueLabel("reply", item.reply && item.reply.found ? item.reply.plainText : "") + "\n" +
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
                              item.valueLabel("colorScheme", item.colorScheme)
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
}
