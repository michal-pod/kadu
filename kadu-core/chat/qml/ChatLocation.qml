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
import QtLocation
import QtPositioning

Item {
    id: root

    property string geoUri: ""
    property color textColor: "black"
    property color markerColor: "#3b78b4"
    property var openLocation: null
    property real maximumWidth: 360
    property real availableWidth: parent ? parent.width : maximumWidth
    readonly property var coordinateParts: geoUri.replace(/^geo:/, "").split(/[;,]/)
    readonly property real latitude: Number(coordinateParts[0])
    readonly property real longitude: Number(coordinateParts[1])
    readonly property bool validLocation: !isNaN(latitude) && !isNaN(longitude) && latitude >= -90 && latitude <= 90 &&
                                         longitude >= -180 && longitude <= 180

    width: Math.max(1, Math.min(availableWidth, maximumWidth))
    readonly property real mapHeight: Math.min(220, Math.max(140, width * 0.62))
    implicitHeight: validLocation ? mapHeight + locationText.implicitHeight + 5 : locationText.implicitHeight

    Map {
        id: map
        anchors.left: parent.left
        anchors.right: parent.right
        height: root.mapHeight
        visible: root.validLocation
        plugin: Plugin { name: "osm" }
        center: QtPositioning.coordinate(root.latitude, root.longitude)
        zoomLevel: 15

        MapQuickItem {
            coordinate: QtPositioning.coordinate(root.latitude, root.longitude)
            anchorPoint.x: marker.width / 2
            anchorPoint.y: marker.height
            sourceItem: Rectangle {
                id: marker
                width: 18
                height: 18
                radius: width / 2
                color: root.markerColor
                border.width: 2
                border.color: "white"
            }
        }

        TapHandler {
            acceptedButtons: Qt.LeftButton
            onTapped: {
                if (root.openLocation)
                    root.openLocation(root.geoUri)
            }
        }
    }

    Text {
        id: locationText
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        text: root.validLocation ? qsTr("Location: %1, %2").arg(root.latitude.toFixed(6)).arg(root.longitude.toFixed(6))
                                 : root.geoUri
        color: root.textColor
        wrapMode: Text.Wrap
        visible: root.geoUri.length > 0
    }
}
