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

    required property var attachment
    property color textColor: "#202020"
    property color linkColor: "#2167a5"
    property color backgroundColor: "transparent"
    property color borderColor: "transparent"
    property var saveAttachment: null

    readonly property bool canSave: saveAttachment !== null

    width: parent ? parent.width : implicitWidth
    implicitHeight: attachmentRow.implicitHeight + 8

    Rectangle {
        anchors.fill: parent
        radius: 4
        color: root.backgroundColor
        border.color: root.borderColor
        border.width: root.borderColor === "transparent" ? 0 : 1
    }

    Row {
        id: attachmentRow
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.margins: 6
        spacing: 7

        Item {
            width: 24
            height: 24

            Image {
                id: icon
                anchors.fill: parent
                source: root.attachment && root.attachment.iconSource ? root.attachment.iconSource : ""
                fillMode: Image.PreserveAspectFit
            }

            Text {
                anchors.centerIn: parent
                visible: icon.status !== Image.Ready
                text: "📄"
                font.pixelSize: 18
            }
        }

        Text {
            id: fileName
            width: parent.width - 31
            height: 24
            text: root.attachment && root.attachment.fileName ? root.attachment.fileName : qsTr("Attachment")
            color: root.canSave ? root.linkColor : root.textColor
            elide: Text.ElideMiddle
            verticalAlignment: Text.AlignVCenter
            font.underline: fileArea.containsMouse && root.canSave
        }
    }

    MouseArea {
        id: fileArea
        anchors.fill: parent
        enabled: root.canSave
        hoverEnabled: true
        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: root.saveAttachment()
    }
}
