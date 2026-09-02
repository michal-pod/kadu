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
import QtLocation
import QtPositioning

Item {
    id: root

    signal locationSelected(real latitude, real longitude)
    signal cancelled()

    property real latitude: NaN
    property real longitude: NaN
    property bool locating: false
    property string positionError: ""
    readonly property bool hasLocation: validCoordinates(latitude, longitude)
    readonly property var defaultCoordinate: QtPositioning.coordinate(52.2297, 21.0122)
    readonly property var selectedCoordinate: hasLocation
                                           ? QtPositioning.coordinate(latitude, longitude)
                                           : defaultCoordinate
    readonly property bool mapLoading: !map.mapReady && map.error === 0
    readonly property bool mapUnavailable: map.error !== 0

    function validCoordinates(candidateLatitude, candidateLongitude) {
        return !isNaN(candidateLatitude) && !isNaN(candidateLongitude) && candidateLatitude >= -90 &&
                candidateLatitude <= 90 && candidateLongitude >= -180 && candidateLongitude <= 180
    }

    function selectCoordinate(coordinate, recenter) {
        if (!coordinate || !validCoordinates(coordinate.latitude, coordinate.longitude))
            return false

        latitude = coordinate.latitude
        longitude = coordinate.longitude
        positionError = ""
        if (recenter) {
            map.center = coordinate
            map.zoomLevel = 15
        }
        return true
    }

    function selectManualCoordinates() {
        const manualLatitude = Number(latitudeField.text.replace(",", "."))
        const manualLongitude = Number(longitudeField.text.replace(",", "."))
        if (!selectCoordinate(QtPositioning.coordinate(manualLatitude, manualLongitude), true))
            positionError = qsTr("Enter a latitude from -90 to 90 and a longitude from -180 to 180.")
    }

    function requestCurrentLocation() {
        positionError = ""
        locating = true
        locationTimeout.restart()
        positionSource.active = true
    }

    Component.onCompleted: map.center = selectedCoordinate

    SystemPalette {
        id: systemPalette
        colorGroup: SystemPalette.Active
    }

    PositionSource {
        id: positionSource

        onPositionChanged: {
            const position = positionSource.position
            if (!position || !position.coordinate)
                return

            if (root.selectCoordinate(position.coordinate, true)) {
                root.locating = false
                locationTimeout.stop()
                active = false
            }
        }

        onSourceErrorChanged: {
            if (!root.locating || sourceError === PositionSource.NoError)
                return

            root.locating = false
            locationTimeout.stop()
            active = false
            root.positionError = sourceError === PositionSource.AccessError
                    ? qsTr("Access to the current location was denied. Choose a point on the map or enter coordinates.")
                    : qsTr("Current location is unavailable. Choose a point on the map or enter coordinates.")
        }
    }

    Timer {
        id: locationTimeout
        interval: 12000
        repeat: false
        onTriggered: {
            if (!root.locating)
                return

            root.locating = false
            positionSource.active = false
            root.positionError = qsTr("Timed out while looking up the current location. Choose a point on the map or enter coordinates.")
        }
    }

    Column {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        Map {
            id: map
            width: parent.width
            height: Math.max(190, parent.height - coordinates.implicitHeight - actions.implicitHeight - status.implicitHeight - 24)
            plugin: Plugin {
                name: "osm"
            }
            zoomLevel: root.hasLocation ? 15 : 5

            TapHandler {
                acceptedButtons: Qt.LeftButton
                gesturePolicy: TapHandler.DragThreshold
                onTapped: function(eventPoint) {
                    if (!map.mapReady || markerDrag.active)
                        return
                    root.selectCoordinate(map.toCoordinate(eventPoint.position), false)
                }
            }

            DragHandler {
                id: mapDrag
                target: null
                enabled: map.mapReady && !markerDrag.active
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

            MapQuickItem {
                visible: root.hasLocation
                coordinate: root.selectedCoordinate
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
                        color: systemPalette.highlight
                        border.width: 2
                        border.color: systemPalette.highlightedText
                    }

                    Rectangle {
                        width: 4
                        height: 22
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: 18
                        rotation: 45
                        transformOrigin: Item.Top
                        color: systemPalette.highlight
                    }

                    DragHandler {
                        id: markerDrag
                        target: marker
                        property bool wasActive: false
                        onActiveChanged: {
                            if (active) {
                                wasActive = true
                                return
                            }
                            if (!wasActive)
                                return

                            const point = marker.mapToItem(map, marker.width / 2, marker.height)
                            root.selectCoordinate(map.toCoordinate(point), false)
                            marker.x = 0
                            marker.y = 0
                            wasActive = false
                        }
                    }
                }
            }

            Column {
                anchors.centerIn: parent
                spacing: 6
                visible: root.mapLoading || root.mapUnavailable

                BusyIndicator {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: root.mapLoading
                    running: visible
                }

                Text {
                    width: Math.min(map.width - 32, 420)
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    color: systemPalette.text
                    text: root.mapUnavailable
                          ? qsTr("The OpenStreetMap map is unavailable: %1").arg(map.errorString || qsTr("unknown error"))
                          : qsTr("Loading OpenStreetMap…")
                }
            }

            Column {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 8
                spacing: 4
                visible: map.mapReady

                ToolButton {
                    text: "+"
                    onClicked: map.zoomLevel = Math.min(map.maximumZoomLevel, map.zoomLevel + 1)
                }

                ToolButton {
                    text: "−"
                    onClicked: map.zoomLevel = Math.max(map.minimumZoomLevel, map.zoomLevel - 1)
                }
            }
        }

        Text {
            id: status
            width: parent.width
            text: root.locating
                  ? qsTr("Looking up the current location…")
                  : root.positionError
                    ? root.positionError
                    : root.hasLocation
                      ? qsTr("Selected location: %1, %2").arg(root.latitude.toFixed(6)).arg(root.longitude.toFixed(6))
                      : qsTr("Click on the map, drag the pin or enter coordinates below.")
            color: root.positionError || root.mapUnavailable ? systemPalette.brightText : systemPalette.text
            wrapMode: Text.WordWrap
        }

        Row {
            id: coordinates
            width: parent.width
            spacing: 8

            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Latitude")
            }

            TextField {
                id: latitudeField
                width: 120
                placeholderText: "52.229700"
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                text: root.hasLocation ? root.latitude.toFixed(6) : ""
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Longitude")
            }

            TextField {
                id: longitudeField
                width: 120
                placeholderText: "21.012200"
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                text: root.hasLocation ? root.longitude.toFixed(6) : ""
            }

            Button {
                text: qsTr("Set coordinates")
                onClicked: root.selectManualCoordinates()
            }
        }

        Row {
            id: actions
            width: parent.width
            spacing: 8

            Button {
                id: currentLocationButton
                text: root.locating ? qsTr("Looking up location…") : qsTr("Use current location")
                enabled: !root.locating
                onClicked: root.requestCurrentLocation()
            }

            Item {
                width: Math.max(0, actions.width - currentLocationButton.implicitWidth - cancelButton.implicitWidth -
                                sendButton.implicitWidth - actions.spacing * 3)
                height: 1
            }

            Button {
                id: cancelButton
                text: qsTr("Cancel")
                onClicked: root.cancelled()
            }

            Button {
                id: sendButton
                text: qsTr("Send")
                enabled: root.hasLocation
                onClicked: root.locationSelected(root.latitude, root.longitude)
            }
        }
    }
}
