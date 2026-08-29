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
    readonly property bool hasLocation: !isNaN(latitude) && !isNaN(longitude)
    readonly property var selectedCoordinate: hasLocation
                                           ? QtPositioning.coordinate(latitude, longitude)
                                           : QtPositioning.coordinate(52.2297, 21.0122)

    SystemPalette {
        id: systemPalette
        colorGroup: SystemPalette.Active
    }

    PositionSource {
        id: positionSource

        onPositionChanged: {
            const latitude = positionSource.position.coordinate.latitude
            const longitude = positionSource.position.coordinate.longitude
            if (isNaN(latitude) || isNaN(longitude) || latitude < -90 || latitude > 90 || longitude < -180 || longitude > 180)
                return
            root.latitude = latitude
            root.longitude = longitude
            active = false
        }
    }

    Column {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        Map {
            id: map
            width: parent.width
            height: parent.height - actions.implicitHeight - status.implicitHeight - 16
            plugin: Plugin { name: "osm" }
            center: root.selectedCoordinate
            zoomLevel: root.hasLocation ? 15 : 5

            TapHandler {
                acceptedButtons: Qt.LeftButton
                onTapped: function(eventPoint) {
                    const coordinate = map.toCoordinate(eventPoint.position)
                    if (isNaN(coordinate.latitude) || isNaN(coordinate.longitude) || coordinate.latitude < -90 ||
                            coordinate.latitude > 90 || coordinate.longitude < -180 || coordinate.longitude > 180)
                        return
                    root.latitude = coordinate.latitude
                    root.longitude = coordinate.longitude
                }
            }

            MapQuickItem {
                visible: root.hasLocation
                coordinate: root.selectedCoordinate
                anchorPoint.x: marker.width / 2
                anchorPoint.y: marker.height
                sourceItem: Rectangle {
                    id: marker
                    width: 20
                    height: 20
                    radius: width / 2
                    color: systemPalette.highlight
                    border.width: 2
                    border.color: systemPalette.highlightedText
                }
            }
        }

        Text {
            id: status
            width: parent.width
            text: root.hasLocation
                  ? qsTr("Selected location: %1, %2").arg(root.latitude.toFixed(6)).arg(root.longitude.toFixed(6))
                  : qsTr("Choose current location to show it on the map.")
            color: systemPalette.text
            elide: Text.ElideRight
        }

        Row {
            id: actions
            width: parent.width
            spacing: 8

            Button {
                id: currentLocationButton
                text: qsTr("Use current location")
                onClicked: positionSource.active = true
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
