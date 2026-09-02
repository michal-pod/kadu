/*
 * %kadu copyright begin%
 * Copyright 2026 Kadu Qt6 port
 * %kadu copyright end%
 */

import QtQuick

Item {
    id: root

    visible: false
    property string colorScheme: "Irssi"
    property var customColors: ({ "enabled": false })
    property var chatFont: ({})
    property var roomInfo: ({})
    property var openUrl: null
    property var openImage: null
    property var openLocation: null
    property var timelineActions: null
    property var executeTimelineAction: null
    property var copyText: null
    property var removeOwnReaction: null
    property var addReaction: null
    property var requestFullReactionSelector: null

    readonly property string terminalFont: "monospace"
    property color backgroundColor: "#000000"
    property color textColor: "#d9d9d9"
    property color mutedTextColor: "#aaaaaa"
    property color separatorColor: "#555555"
    property color loadingTextColor: "#d9d9d9"
    property color roomHeaderBackgroundColor: "#0000b8"
    property color accentColor: "#0000ff"
    property bool roomHeaderRequiresDescription: true
    property bool roomHeaderDescriptionOnly: true
    property bool roomHeaderShowAvatar: false
    property bool roomHeaderShowActions: false
    property int roomHeaderMinimumHeight: 22
    property int roomHeaderVerticalPadding: 4
    property int roomHeaderHorizontalPadding: 5
    property string roomHeaderFontFamily: terminalFont
    property int roomHeaderDescriptionFontPixelSize: 12
    property int timelineMargin: 4
    property int timelineSpacing: 0
    property int groupingIntervalSeconds: 0
    property Component timelineItem: timelineItemComponent
    property Component composerOverlay: composerOverlayComponent
    property Component pinnedMessagesPanel: pinnedMessagesPanelComponent
    property Component statusBar: statusBarComponent

    Component {
        id: timelineItemComponent

        IrssiTimelineItem {
            terminalFont: root.terminalFont
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

    Component {
        id: composerOverlayComponent

        IrssiComposerOverlay {
            terminalFont: root.terminalFont
        }
    }

    Component {
        id: pinnedMessagesPanelComponent

        IrssiPinnedMessages {
            terminalFont: root.terminalFont
            timelineActions: root.timelineActions
            executeTimelineAction: root.executeTimelineAction
        }
    }

    Component {
        id: statusBarComponent

        IrssiStatusBar {
            terminalFont: root.terminalFont
        }
    }
}
