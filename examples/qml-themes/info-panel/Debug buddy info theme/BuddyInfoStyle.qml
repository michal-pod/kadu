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

    property var infoPanel
    property string colorScheme: "System"
    readonly property string selectedText: detailsText.selectedText.length > 0
                                          ? detailsText.selectedText
                                          : (statusText.selectedText.length > 0
                                             ? statusText.selectedText
                                             : descriptionText.selectedText)

    SystemPalette {
        id: systemPalette
        colorGroup: SystemPalette.Active
    }

    readonly property bool darkSurface: colorScheme === "Dark" || colorScheme === "HighContrast" ||
                                       (colorScheme !== "Light" && systemPalette.base.r * 0.2126 +
                                        systemPalette.base.g * 0.7152 +
                                        systemPalette.base.b * 0.0722 < 0.5)
    readonly property bool highContrast: colorScheme === "HighContrast"
    readonly property color background: highContrast ? "#000000" : (darkSurface ? "#1e242c" : "#f5f7fa")
    readonly property color foreground: highContrast ? "#ffffff" : (darkSurface ? "#f3f6fa" : "#17202a")
    readonly property color muted: highContrast ? "#ffff00" : (darkSurface ? "#b6c4d2" : "#536273")

    function value(name, fallback) {
        return infoPanel && infoPanel[name] !== undefined ? infoPanel[name] : fallback
    }

    function copySelection() {
        if (detailsText.selectedText.length > 0)
            detailsText.copy()
        else if (statusText.selectedText.length > 0)
            statusText.copy()
        else if (descriptionText.selectedText.length > 0)
            descriptionText.copy()
    }

    Rectangle {
        anchors.fill: parent
        color: root.background
    }

    Flickable {
        anchors.fill: parent
        anchors.margins: 8
        contentWidth: width
        contentHeight: content.implicitHeight
        clip: true

        Column {
            id: content
            width: parent.width
            spacing: 5

            Text {
                width: parent.width
                color: root.foreground
                font.bold: true
                wrapMode: Text.Wrap
                text: "Debug BuddyInfoStyle | colorScheme=" + root.colorScheme
            }

            Image {
                width: 48
                height: 48
                source: root.value("avatarSource", "")
                fillMode: Image.PreserveAspectCrop
                visible: status === Image.Ready
            }

            Text {
                width: parent.width
                color: root.muted
                wrapMode: Text.Wrap
                text: "displayName: " + root.value("displayName", "") + "\n" +
                      "avatarSource: " + root.value("avatarSource", "") + "\n" +
                      "style: " + root.value("style", "") + "\n" +
                      "styleSource: " + root.value("styleSource", "") + "\n" +
                      "colorScheme: " + root.value("colorScheme", "") + "\n" +
                      "useCustomColors: " + root.value("useCustomColors", false) + "\n" +
                      "foregroundColor: " + root.value("foregroundColor", "") + "\n" +
                      "backgroundColor: " + root.value("backgroundColor", "") + "\n" +
                      "font: " + root.value("fontFamily", "") + " " +
                      root.value("fontPointSize", 0) + " pt; bold=" + root.value("fontBold", false) +
                      ", italic=" + root.value("fontItalic", false) + ", underline=" +
                      root.value("fontUnderline", false) + "\n" +
                      "showScrollBar: " + root.value("showScrollBar", false)
            }

            TextEdit {
                id: detailsText
                width: parent.width
                height: contentHeight
                readOnly: true
                selectByMouse: true
                textFormat: TextEdit.RichText
                color: root.foreground
                wrapMode: TextEdit.Wrap
                text: root.value("detailsText", "")
            }

            TextEdit {
                id: statusText
                width: parent.width
                height: contentHeight
                readOnly: true
                selectByMouse: true
                textFormat: TextEdit.RichText
                color: root.foreground
                wrapMode: TextEdit.Wrap
                text: root.value("statusText", "")
            }

            TextEdit {
                id: descriptionText
                width: parent.width
                height: contentHeight
                readOnly: true
                selectByMouse: true
                textFormat: TextEdit.RichText
                color: root.foreground
                wrapMode: TextEdit.Wrap
                text: root.value("descriptionText", "")
            }
        }
    }
}
