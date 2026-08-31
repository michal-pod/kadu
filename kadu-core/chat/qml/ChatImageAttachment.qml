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
    property color placeholderColor: "#808080"
    property color placeholderTextColor: "#ffffff"
    property var openImage: null
    property real maximumWidth: 360
    property real maximumHeight: 260
    property real availableWidth: parent ? parent.width : maximumWidth

    readonly property size dimensions: attachment && attachment.dimensions ? attachment.dimensions : Qt.size(0, 0)
    readonly property real imageWidth: Number(dimensions.width) > 0 ? Number(dimensions.width) : 4
    readonly property real imageHeight: Number(dimensions.height) > 0 ? Number(dimensions.height) : 3
    readonly property string thumbnailSource: attachment && attachment.thumbnailUri
                                             ? attachment.thumbnailUri.toString() : ""
    readonly property url displaySourceUri: thumbnailSource.length > 0
                                           ? attachment.thumbnailUri
                                           : (attachment ? attachment.sourceUri : "")
    readonly property bool downloading: attachment && attachment.state === 1
    readonly property bool failed: attachment && attachment.state === 3
    readonly property real progress: attachment && attachment.progress !== undefined ? attachment.progress : 0.0

    width: Math.max(1, Math.min(availableWidth, maximumWidth))
    height: Math.min(maximumHeight, width * imageHeight / imageWidth)
    clip: true

    Rectangle {
        anchors.fill: parent
        color: root.placeholderColor
        opacity: 0.35
    }

    KaduImage {
        id: image
        anchors.fill: parent
        // A missing thumbnail is represented as an empty QUrl in the model;
        // test its string value, otherwise QML can select an empty source.
        // The full resource is still passed to the image viewer after a click.
        sourceUri: root.displaySourceUri
        resourceState: root.attachment ? root.attachment.state : 0
        fillMode: Image.PreserveAspectFit
        sourceSize.width: Math.max(1, Math.round(root.width * 2))
        sourceSize.height: Math.max(1, Math.round(root.height * 2))
    }

    Text {
        anchors.centerIn: parent
        visible: root.downloading || root.failed || image.status === Image.Error
        text: root.failed ? (root.attachment.errorText || qsTr("Could not load image"))
                          : (image.status === Image.Error ? qsTr("Could not load image")
                                                          : qsTr("Loading image… %1%").arg(Math.round(root.progress * 100)))
        color: root.placeholderTextColor
        font.pixelSize: 12
        width: parent.width - 20
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 3
        visible: root.downloading
        color: root.placeholderTextColor
        opacity: 0.25

        Rectangle {
            width: parent.width * root.progress
            height: parent.height
            color: root.placeholderTextColor
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: image.status === Image.Ready
        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: {
            if (root.openImage)
                root.openImage(root.attachment.sourceUri, root.attachment.fileName,
                               root.imageWidth, root.imageHeight,
                               root.attachment.state)
        }
    }
}
