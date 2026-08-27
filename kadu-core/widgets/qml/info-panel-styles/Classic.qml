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
        color: root.value("backgroundColor", "transparent")
    }

    Flickable {
        id: flickable
        anchors.fill: parent
        clip: true
        contentWidth: width
        contentHeight: content.implicitHeight + 16
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
            color: "#808080"
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
            width: flickable.width - 16
            x: 8
            y: 8
            spacing: 8

            Row {
                width: parent.width
                spacing: 12

                Item {
                    id: avatarContainer
                    width: 64
                    height: width

                    Rectangle {
                        anchors.fill: parent
                        color: "#5a8bbd"
                        visible: !avatar.visible
                    }

                    Text {
                        anchors.centerIn: parent
                        text: root.value("displayName", "?").length > 0
                              ? root.value("displayName", "?").substring(0, 1).toUpperCase()
                              : "?"
                        color: "white"
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
                    spacing: 2

                    Text {
                        width: parent.width
                        text: root.value("displayName", "")
                        color: root.value("foregroundColor", "#202020")
                        elide: Text.ElideRight
                        font.family: root.value("fontFamily", "")
                        font.pointSize: root.value("fontPointSize", 10) + 2
                        font.bold: true
                    }

                    TextEdit {
                        id: detailsText
                        width: parent.width
                        text: root.value("detailsText", "")
                        textFormat: TextEdit.RichText
                        color: root.value("foregroundColor", "#202020")
                        readOnly: true
                        selectByMouse: true
                        wrapMode: TextEdit.Wrap
                        font.family: root.value("fontFamily", "")
                        font.pointSize: root.value("fontPointSize", 10)
                    }

                    TextEdit {
                        id: statusText
                        width: parent.width
                        text: root.value("statusText", "")
                        textFormat: TextEdit.RichText
                        color: root.value("foregroundColor", "#202020")
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

            Rectangle {
                width: parent.width
                height: 1
                color: Qt.rgba(0, 0, 0, 0.2)
                visible: descriptionText.text.length > 0
            }

            TextEdit {
                id: descriptionText
                width: parent.width
                text: root.value("descriptionText", "")
                textFormat: TextEdit.RichText
                color: root.value("foregroundColor", "#202020")
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
