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
    property var openUrl: null

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
    property Component timelineItem: timelineItemComponent

    Component {
        id: timelineItemComponent

        Item {
            id: item

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
            property string colorScheme: root.colorScheme
            property var openUrl: root.openUrl

            implicitHeight: card.implicitHeight

            function valueLabel(label, value) {
                return label + ": " + value
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
                              item.valueLabel("kind", item.kind) + " | " +
                              item.valueLabel("timestamp", item.timestamp)
                    }

                    Text {
                        width: parent.width
                        color: root.mutedTextColor
                        wrapMode: Text.Wrap
                        text: item.valueLabel("senderDisplayName", item.senderDisplayName) + " | " +
                              item.valueLabel("ownEvent", item.ownEvent) + " | " +
                              item.valueLabel("showSender", item.showSender) + " | " +
                              item.valueLabel("showTimestamp", item.showTimestamp) + " | " +
                              item.valueLabel("startsNewDay", item.startsNewDay)
                    }

                    TextEdit {
                        width: parent.width
                        height: contentHeight
                        readOnly: true
                        selectByMouse: true
                        textFormat: TextEdit.RichText
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
                              item.valueLabel("deliveryState", item.deliveryState) + " | " +
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
}
