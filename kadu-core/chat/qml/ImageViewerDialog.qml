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
import QtQuick.Layouts
import QtQuick.Window

Popup {
    id: root

    parent: parentItem
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    width: Math.max(1, Math.min(1040, parent ? parent.width - 32 : 1040))
    height: Math.max(1, Math.min(820, parent ? parent.height - 32 : 820))
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0
    padding: 0

    required property Item parentItem
    property url sourceUri
    property string imageTitle: qsTr("Image")
    property int originalWidth: 1
    property int originalHeight: 1
    property int resourceState: 0
    property real zoom: 1.0
    property bool fitted: true

    function fittedZoom() {
        const widthScale = imageViewport.width / originalWidth
        const heightScale = imageViewport.height / originalHeight
        return Math.min(1.0, Math.max(0.05, Math.min(widthScale, heightScale)))
    }

    function maximumZoom() {
        const physicalWidthScale = imageViewport.width * Screen.devicePixelRatio / originalWidth
        const physicalHeightScale = imageViewport.height * Screen.devicePixelRatio / originalHeight
        return Math.max(fittedZoom(), 2.0 * Math.max(physicalWidthScale, physicalHeightScale))
    }

    function setZoom(value, focalX, focalY) {
        const previousZoom = zoom
        const imageX = (imageViewport.contentX + focalX - displayedImage.x) / previousZoom
        const imageY = (imageViewport.contentY + focalY - displayedImage.y) / previousZoom
        zoom = Math.max(fittedZoom(), Math.min(maximumZoom(), value))
        fitted = false
        imageViewport.contentX = displayedImage.x + imageX * zoom - focalX
        imageViewport.contentY = displayedImage.y + imageY * zoom - focalY
    }

    function openFor(source, title, width, height, state) {
        sourceUri = source
        imageTitle = title.length > 0 ? title : qsTr("Image")
        originalWidth = Math.max(1, width)
        originalHeight = Math.max(1, height)
        resourceState = state
        open()
        Qt.callLater(fitImage)
    }

    function fitImage() {
        zoom = fittedZoom()
        fitted = true
        imageViewport.contentX = 0
        imageViewport.contentY = 0
    }

    onWidthChanged: if (visible && fitted) Qt.callLater(fitImage)
    onHeightChanged: if (visible && fitted) Qt.callLater(fitImage)

    background: Rectangle {
        color: "#1d222a"
        border.color: "#526070"
        radius: 8
    }

    contentItem: ColumnLayout {
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 46
            color: "#282e38"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 8

                Label {
                    Layout.fillWidth: true
                    text: root.imageTitle + "  ·  " + root.originalWidth + " × " + root.originalHeight
                    color: "#f2f4f8"
                    elide: Text.ElideRight
                    font.bold: true
                }

                ToolButton {
                    text: "×"
                    onClicked: root.close()
                }
            }
        }

        Flickable {
            id: imageViewport
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: Math.max(width, displayedImage.width)
            contentHeight: Math.max(height, displayedImage.height)
            boundsBehavior: Flickable.StopAtBounds
            interactive: contentWidth > width || contentHeight > height

            WheelHandler {
                target: null
                onWheel: function(event) {
                    const factor = event.angleDelta.y > 0 ? 1.2 : 1 / 1.2
                    root.setZoom(root.zoom * factor, imageViewport.width / 2, imageViewport.height / 2)
                    event.accepted = true
                }
            }

            PinchHandler {
                target: null
                property real zoomBeforePinch: 1.0
                onActiveChanged: if (active) zoomBeforePinch = root.zoom
                onScaleChanged: {
                    if (active)
                        root.setZoom(zoomBeforePinch * scale, imageViewport.width / 2, imageViewport.height / 2)
                }
            }

            KaduImage {
                id: displayedImage
                sourceUri: root.sourceUri
                resourceState: root.resourceState
                width: root.originalWidth * root.zoom
                height: root.originalHeight * root.zoom
                x: width < imageViewport.width ? (imageViewport.width - width) / 2 : 0
                y: height < imageViewport.height ? (imageViewport.height - height) / 2 : 0
                fillMode: Image.PreserveAspectFit
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 42
            color: "#282e38"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 10

                Button {
                    text: qsTr("Fit")
                    onClicked: root.fitImage()
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Drag with the left button · wheel: zoom · pinch: zoom")
                    color: "#d8e0ec"
                    elide: Text.ElideRight
                }

                Label {
                    text: Math.round(root.zoom * 100) + "%"
                    color: "#d8e0ec"
                }
            }
        }
    }
}
