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
    SystemPalette {
        id: systemPalette
        colorGroup: SystemPalette.Active
    }
    readonly property bool darkSurface: colorScheme === "Dark" ||
                                       (colorScheme !== "Light" && systemPalette.base.r * 0.2126 +
                                        systemPalette.base.g * 0.7152 +
                                        systemPalette.base.b * 0.0722 < 0.5)
    readonly property bool useCustomColors: root.value("useCustomColors", false)
    readonly property color fallbackAvatarColor: darkSurface ? "#4b86c5" : "#5a8bbd"
    readonly property color themeForegroundColor: useCustomColors ? root.value("foregroundColor", systemPalette.text)
                                                                   : (colorScheme === "System" ? systemPalette.text
                                                                                               : (darkSurface ? "#f2f4f8" : "#202020"))
    readonly property color themeBackgroundColor: useCustomColors ? root.value("backgroundColor", "transparent")
                                                                   : (colorScheme === "System" ? "transparent"
                                                                                               : (darkSurface ? "#20242b" : "#f7f7f7"))
    readonly property string selectedText: detailsText.selectedText.length > 0
                                          ? detailsText.selectedText
                                          : (statusText.selectedText.length > 0
                                             ? statusText.selectedText
                                             : descriptionText.selectedText)

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
        color: root.themeBackgroundColor
    }

    Flickable {
        id: flickable
        anchors.fill: parent
        clip: true
        contentWidth: width
        contentHeight: content.implicitHeight + 12
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.VerticalFlick

        Rectangle {
            id: scrollHandle
            width: 5
            height: Math.max(20, flickable.height * flickable.height / flickable.contentHeight)
            x: flickable.width - width - 2
            y: 2 + flickable.contentY * (flickable.height - height - 4) /
                   Math.max(1, flickable.contentHeight - flickable.height)
            radius: width / 2
            color: root.colorScheme === "System" ? systemPalette.mid
                                                   : (root.darkSurface ? "#6d7785" : "#858585")
            opacity: 0.7
            visible: root.value("showScrollBar", false) && flickable.contentHeight > flickable.height

            MouseArea {
                anchors.fill: parent
                property real pressY: 0

                onPressed: pressY = mouse.y
                onPositionChanged: {
                    const handleY = Math.max(2, Math.min(flickable.height - scrollHandle.height - 2,
                                                         scrollHandle.y + mouse.y - pressY))
                    flickable.contentY = (handleY - 2) *
                                         (flickable.contentHeight - flickable.height) /
                                         Math.max(1, flickable.height - scrollHandle.height - 4)
                }
            }
        }

        Column {
            id: content
            width: flickable.width - 14
            x: 7
            y: 6
            spacing: 4

            Row {
                width: parent.width
                spacing: 8

                Item {
                    id: avatarContainer
                    width: 40
                    height: width

                    Rectangle {
                        anchors.fill: parent
                        radius: width / 2
                        color: root.fallbackAvatarColor
                        visible: !avatar.visible
                    }

                    Text {
                        anchors.centerIn: parent
                        text: root.value("displayName", "?").length > 0
                              ? root.value("displayName", "?").substring(0, 1).toUpperCase()
                              : "?"
                        color: "#ffffff"
                        font.bold: true
                        font.pixelSize: parent.width / 2
                        visible: !avatar.visible
                    }

                    Image {
                        id: avatar
                        anchors.fill: parent
                        source: root.value("avatarSource", "")
                        fillMode: Image.PreserveAspectCrop
                        visible: status === Image.Ready
                    }
                }

                Column {
                    width: parent.width - avatarContainer.width - parent.spacing
                    spacing: 1

                    Text {
                        width: parent.width
                        text: root.value("displayName", "")
                        color: root.themeForegroundColor
                        elide: Text.ElideRight
                        font.family: root.value("fontFamily", "")
                        font.pointSize: root.value("fontPointSize", 10) + 1
                        font.bold: true
                    }

                    TextEdit {
                        id: detailsText
                        width: parent.width
                        text: root.value("detailsText", "")
                        textFormat: TextEdit.RichText
                        color: root.themeForegroundColor
                        readOnly: true
                        selectByMouse: true
                        wrapMode: TextEdit.Wrap
                        font.family: root.value("fontFamily", "")
                        font.pointSize: root.value("fontPointSize", 10)
                    }
                }
            }

            TextEdit {
                id: statusText
                width: parent.width
                text: root.value("statusText", "")
                textFormat: TextEdit.RichText
                color: root.themeForegroundColor
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.Wrap
                font.family: root.value("fontFamily", "")
                font.pointSize: root.value("fontPointSize", 10)
                font.bold: root.value("fontBold", false)
                font.italic: root.value("fontItalic", false)
                font.underline: root.value("fontUnderline", false)
            }

            TextEdit {
                id: descriptionText
                width: parent.width
                text: root.value("descriptionText", "")
                textFormat: TextEdit.RichText
                color: root.themeForegroundColor
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.Wrap
                font.family: root.value("fontFamily", "")
                font.pointSize: root.value("fontPointSize", 10)
                font.bold: root.value("fontBold", false)
                font.italic: root.value("fontItalic", false)
                font.underline: root.value("fontUnderline", false)
            }
        }
    }
}
