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
    property var pinnedMessages: []
    property var timelineActions: null
    property var executeTimelineAction: null

    readonly property bool darkSurface: colorScheme === "Dark" ||
                                       (colorScheme !== "Light" && systemPalette.base.r * 0.2126 +
                                        systemPalette.base.g * 0.7152 +
                                        systemPalette.base.b * 0.0722 < 0.5)
    readonly property color accentColor: darkSurface ? "#5b9bd5" : "#2878b7"
    readonly property color textColor: colorScheme === "System" ? systemPalette.text
                                                                   : (darkSurface ? "#f2f4f8" : "#202020")
    readonly property color mutedTextColor: colorScheme === "System" ? systemPalette.mid
                                                                        : (darkSurface ? "#b6c0cf" : "#5c6470")
    readonly property var entries: pinnedMessages || []

    implicitHeight: visible ? pinButton.implicitHeight + 8 : 0
    visible: entries.length > 0

    function actionsFor(stableId) {
        return timelineActions ? timelineActions(stableId) : []
    }

    SystemPalette {
        id: systemPalette
        colorGroup: SystemPalette.Active
    }

    Rectangle {
        anchors.fill: parent
        color: colorScheme === "System" ? systemPalette.alternateBase
                                         : (darkSurface ? "#28333e" : "#e9eef4")
    }

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 4
        color: root.accentColor
    }

    Button {
        id: pinButton
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        text: root.entries.length === 1 ? qsTr("1 pinned message")
                                        : qsTr("%1 pinned messages").arg(root.entries.length)
        Accessible.name: text
        onClicked: pinnedMessagesPopup.open()
    }

    Popup {
        id: pinnedMessagesPopup
        parent: root
        x: 8
        y: root.height + 4
        width: Math.max(280, Math.min(root.width - 16, 560))
        height: Math.min(440, content.implicitHeight + topPadding + bottomPadding)
        padding: 10
        modal: false
        z: 10
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            color: root.colorScheme === "System" ? systemPalette.base
                                                   : (root.darkSurface ? "#232d36" : "#ffffff")
            border.width: 1
            border.color: root.accentColor
            radius: 5
        }

        contentItem: Column {
            id: content
            width: pinnedMessagesPopup.availableWidth
            spacing: 8

            Text {
                width: parent.width
                text: qsTr("Pinned messages")
                color: root.accentColor
                font.bold: true
            }

            ListView {
                id: pinnedMessagesList
                width: parent.width
                height: Math.min(contentHeight, 360)
                clip: true
                spacing: 6
                model: root.entries

                delegate: Rectangle {
                    id: entryCard
                    required property var modelData
                    readonly property var entry: modelData

                    width: pinnedMessagesList.width
                    implicitHeight: entryContent.implicitHeight + 14
                    color: root.colorScheme === "System" ? systemPalette.alternateBase
                                                           : (root.darkSurface ? "#34424f" : "#eff5fb")
                    radius: 5

                    Column {
                        id: entryContent
                        x: 7
                        y: 7
                        width: parent.width - 14
                        spacing: 3

                        Text {
                            width: parent.width
                            visible: entry.senderDisplayName && entry.senderDisplayName.length > 0
                            text: entry.senderDisplayName || ""
                            color: root.accentColor
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        Text {
                            width: parent.width
                            text: entry.available ? (entry.plainText || qsTr("This message is no longer available."))
                                                  : qsTr("This pinned message is not loaded yet.")
                            textFormat: entry.formattedText && entry.formattedText.length > 0 ? Text.RichText : Text.PlainText
                            color: root.textColor
                            wrapMode: Text.Wrap
                            maximumLineCount: 3
                            elide: Text.ElideRight
                        }

                        Text {
                            width: parent.width
                            visible: !entry.available
                            text: qsTr("Open the history around this message is not available yet.")
                            color: root.mutedTextColor
                            font.italic: true
                            wrapMode: Text.Wrap
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
