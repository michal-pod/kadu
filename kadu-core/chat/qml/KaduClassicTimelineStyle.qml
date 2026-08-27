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
    visible: false
    property string colorScheme: "System"
    property var customColors: ({ "enabled": false })

    SystemPalette {
        id: systemPalette
        colorGroup: SystemPalette.Active
    }

    readonly property bool darkSurface: colorScheme === "Dark" ||
                                       (colorScheme !== "Light" && systemPalette.base.r * 0.2126 +
                                        systemPalette.base.g * 0.7152 +
                                        systemPalette.base.b * 0.0722 < 0.5)
    readonly property bool usesCustomColors: customColors && customColors.enabled

    // System uses Qt's current platform palette. Light and Dark are theme
    // variants and therefore use the theme's own palette.
    property color backgroundColor: usesCustomColors && customColors.backgroundEnabled ? customColors.background
                                                                                         : (colorScheme === "System" ? systemPalette.base
                                                                                                                     : (darkSurface ? "#20242b" : "#f7f7f7"))
    property color mutedTextColor: colorScheme === "System" ? systemPalette.mid
                                                       : (darkSurface ? "#aeb8c7" : "#666666")
    property color separatorColor: colorScheme === "System" ? systemPalette.mid
                                                      : (darkSurface ? "#6d7785" : "#858585")
    property color loadingTextColor: colorScheme === "System" ? systemPalette.text
                                                        : (darkSurface ? "#f2f4f8" : "#202020")
    property color textColor: loadingTextColor
    property color roomHeaderBackgroundColor: colorScheme === "System" ? systemPalette.alternateBase
                                                                  : (darkSurface ? "#2d323a" : "#f4f6f8")
    property int timelineMargin: 8
    property int timelineSpacing: 1
    property Component timelineItem: timelineItemComponent

    Component {
        id: timelineItemComponent
        KaduClassicTimelineItem {
            colorScheme: root.colorScheme
            customColors: root.customColors
        }
    }
}
