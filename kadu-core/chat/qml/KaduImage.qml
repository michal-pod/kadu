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

    required property url sourceUri
    property int resourceState: 0
    property int reloadToken: 0
    property alias fillMode: image.fillMode
    property alias status: image.status
    property alias sourceSize: image.sourceSize

    function providerSource() {
        const uri = root.sourceUri.toString()
        if (!uri.startsWith("kaduimg:"))
            return ""
        return "image://kaduimg/" + encodeURIComponent(uri) + "?state=" + root.resourceState
                + "&reload=" + root.reloadToken
    }

    Image {
        id: image
        anchors.fill: parent
        source: root.providerSource()
        asynchronous: false
    }
}
