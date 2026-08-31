/*
 * %kadu copyright begin%
 * Copyright 2026 Kadu Qt6 port
 * %kadu copyright end%
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

import QtQuick
import QtQuick.Controls
import QtLocation
import QtPositioning

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
    property string geoUri: ""
    readonly property var coordinateParts: geoUri.replace(/^geo:/, "").split(/[;,]/)
    readonly property real latitude: Number(coordinateParts[0])
    readonly property real longitude: Number(coordinateParts[1])
    readonly property bool validLocation: !isNaN(latitude) && !isNaN(longitude) && latitude >= -90 && latitude <= 90 &&
                                         longitude >= -180 && longitude <= 180

    function openFor(uri) {
        geoUri = uri
        if (validLocation) {
            map.center = QtPositioning.coordinate(latitude, longitude)
            map.zoomLevel = 15
        }
        open()
    }

    background: Rectangle {
        color: "#1d222a"
        border.color: "#526070"
        radius: 8
    }

    contentItem: Item {
        Rectangle {
            id: titleBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 46
            color: "#282e38"

            Label {
                anchors.left: parent.left
                anchors.right: closeButton.left
                anchors.leftMargin: 14
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Location")
                color: "#f2f4f8"
                font.bold: true
            }

            ToolButton {
                id: closeButton
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                text: "×"
                onClicked: root.close()
            }
        }

        Map {
            id: map
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: titleBar.bottom
            anchors.bottom: footer.top
            plugin: Plugin { name: "osm" }
            center: root.validLocation ? QtPositioning.coordinate(root.latitude, root.longitude)
                                       : QtPositioning.coordinate(0, 0)
            zoomLevel: 15

            DragHandler {
                id: mapDrag
                target: null
                enabled: map.mapReady
                acceptedButtons: Qt.LeftButton
                onTranslationChanged: function(delta) {
                    map.pan(-delta.x, -delta.y)
                }
            }

            WheelHandler {
                id: mapWheel
                enabled: map.mapReady
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                onWheel: function(event) {
                    if (event.modifiers !== Qt.NoModifier)
                        return

                    const coordinate = map.toCoordinate(mapWheel.point.position, false)
                    const zoomDelta = event.angleDelta.y / 120
                    if (zoomDelta === 0)
                        return

                    map.zoomLevel = Math.max(map.minimumZoomLevel,
                                             Math.min(map.maximumZoomLevel, map.zoomLevel + zoomDelta))
                    map.alignCoordinateToPoint(coordinate, mapWheel.point.position)
                }
            }

            PinchHandler {
                target: null
                property real zoomBeforePinch: 1.0
                onActiveChanged: if (active) zoomBeforePinch = map.zoomLevel
                onScaleChanged: if (active)
                                    map.zoomLevel = Math.max(map.minimumZoomLevel,
                                                             Math.min(map.maximumZoomLevel,
                                                                      zoomBeforePinch + Math.log2(scale)))
            }

            MapQuickItem {
                visible: root.validLocation
                coordinate: QtPositioning.coordinate(root.latitude, root.longitude)
                anchorPoint.x: marker.width / 2
                anchorPoint.y: marker.height
                sourceItem: Item {
                    id: marker
                    width: 30
                    height: 42

                    Rectangle {
                        width: 24
                        height: 24
                        anchors.horizontalCenter: parent.horizontalCenter
                        radius: width / 2
                        color: "#4f8ecb"
                        border.width: 2
                        border.color: "white"
                    }

                    Rectangle {
                        width: 4
                        height: 22
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: 18
                        rotation: 45
                        transformOrigin: Item.Top
                        color: "#4f8ecb"
                    }
                }
            }
        }

        Rectangle {
            id: footer
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 38
            color: "#282e38"

            Label {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Drag with the left button · wheel: zoom · pinch: zoom")
                color: "#d8e0ec"
                elide: Text.ElideRight
            }
        }
    }
}
