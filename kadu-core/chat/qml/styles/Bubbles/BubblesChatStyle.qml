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
    property var chatFont: ({ "family": "", "pointSize": 10, "bold": false, "italic": false, "underline": false })
    property var openUrl: null
    property var openImage: null
    property var openLocation: null
    property var timelineActions: null
    property var executeTimelineAction: null
    property var copyText: null
    property var removeOwnReaction: null
    property var addReaction: null
    property var requestFullReactionSelector: null

    SystemPalette {
        id: systemPalette
        colorGroup: SystemPalette.Active
    }

    readonly property bool darkSurface: colorScheme === "Dark" ||
                                       (colorScheme !== "Light" && systemPalette.base.r * 0.2126 +
                                        systemPalette.base.g * 0.7152 +
                                        systemPalette.base.b * 0.0722 < 0.5)
    readonly property bool usesCustomColors: customColors && customColors.enabled

    property color backgroundColor: usesCustomColors && customColors.backgroundEnabled ? customColors.background
                                                                                         : (colorScheme === "System" ? systemPalette.base
                                                                                                                     : (darkSurface ? "#20242b" : "#f7f7f7"))
    property color mutedTextColor: colorScheme === "System" ? systemPalette.mid
                                                       : (darkSurface ? "#b6c0cf" : "#5c6470")
    property color separatorColor: colorScheme === "System" ? systemPalette.mid
                                                      : (darkSurface ? "#858585" : "#a8afb8")
    property color loadingTextColor: colorScheme === "System" ? systemPalette.text
                                                        : (darkSurface ? "#f2f4f8" : "#202020")
    property color textColor: loadingTextColor
    property color jumpToLatestTextColor: darkSurface ? "#f2f4f8" : "#202020"
    property color jumpToLatestBackgroundColor: darkSurface ? "#303944" : "#f8fbfe"
    property color jumpToLatestBorderColor: darkSurface ? "#586675" : "#8fb9d9"
    property color jumpToLatestHoverColor: darkSurface ? "#344b60" : "#d4e7f5"
    property color jumpToLatestHoverTextColor: darkSurface ? "#ffffff" : "#202020"
    property color roomHeaderBackgroundColor: colorScheme === "System" ? systemPalette.alternateBase
                                                                  : (darkSurface ? "#28333e" : "#e9eef4")
    property int timelineMargin: 12
    property int timelineSpacing: 4
    property int groupingIntervalSeconds: 300
    property Component timelineItem: timelineItemComponent
    property Component composerContext: composerContextComponent
    property Component composerOverlay: composerOverlayComponent
    property Component pinnedMessagesPanel: pinnedMessagesPanelComponent

    Component {
        id: timelineItemComponent
        BubblesTimelineItem {
            colorScheme: root.colorScheme
            customColors: root.customColors
            chatFont: root.chatFont
            openUrl: root.openUrl
            openLocation: root.openLocation
            timelineActions: root.timelineActions
            executeTimelineAction: root.executeTimelineAction
            copyText: root.copyText
            removeOwnReaction: root.removeOwnReaction
            addReaction: root.addReaction
            requestFullReactionSelector: root.requestFullReactionSelector
        }
    }

    Component {
        id: composerContextComponent

        BubblesComposerContext {
            colorScheme: root.colorScheme
            customColors: root.customColors
        }
    }

    Component {
        id: composerOverlayComponent

        BubblesComposerOverlay {
            colorScheme: root.colorScheme
            customColors: root.customColors
            chatFont: root.chatFont
            backgroundColor: root.backgroundColor
            textColor: root.textColor
        }
    }

    Component {
        id: pinnedMessagesPanelComponent

        BubblesPinnedMessages {
            colorScheme: root.colorScheme
            customColors: root.customColors
            chatFont: root.chatFont
            openUrl: root.openUrl
            openImage: root.openImage
            openLocation: root.openLocation
            timelineActions: root.timelineActions
            executeTimelineAction: root.executeTimelineAction
            copyText: root.copyText
            removeOwnReaction: root.removeOwnReaction
            addReaction: root.addReaction
            requestFullReactionSelector: root.requestFullReactionSelector
        }
    }
}
