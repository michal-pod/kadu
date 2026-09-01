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
import QtQuick.Effects

Item {
    id: root

    property var chatViewModel: _chatViewModel
    // A configuration preview supplies this property while the real chat keeps
    // it empty and follows the globally selected theme from ChatViewModel.
    property url themeSourceOverride: ""
    property string themeColorSchemeOverride: ""
    readonly property url activeThemeSource: themeSourceOverride.toString().length > 0
                                            ? themeSourceOverride
                                            : (chatViewModel ? chatViewModel.themeSource : "")
    readonly property string activeThemeColorScheme: themeColorSchemeOverride.length > 0
                                                    ? themeColorSchemeOverride
                                                    : (chatViewModel ? chatViewModel.themeColorScheme : "System")
    readonly property var activeCustomColors: chatViewModel ? chatViewModel.customColors : ({ "enabled": false })
    readonly property var activeChatFont: chatViewModel ? chatViewModel.chatFont
                                                        : ({ "family": "", "pointSize": 10,
                                                             "bold": false, "italic": false,
                                                             "underline": false, "forced": false })
    readonly property var activeTheme: themeLoader.item
    readonly property bool composerActive: chatViewModel && chatViewModel.composerActive
    property bool initialPositioned: false
    property bool followingTail: true
    property bool scrollBarVisible: false
    property int attachmentImageRevision: 0
    property string olderAnchorId: ""
    property real olderAnchorOffset: 0
    property var defaultComposerContextComponent: null
    property var defaultComposerOverlayComponent: null
    property var defaultPinnedMessagesPanelComponent: null
    readonly property int newEventsBelow: chatViewModel ? chatViewModel.newEventsBelow : 0

    // These values also make the fallback renderer readable when a selected
    // external style cannot be loaded.
    SystemPalette {
        id: systemPalette
        colorGroup: SystemPalette.Active
    }
    readonly property bool systemDarkSurface: systemPalette.base.r * 0.2126 +
                                             systemPalette.base.g * 0.7152 +
                                             systemPalette.base.b * 0.0722 < 0.5

    function themeValue(name, fallback) {
        return activeTheme && activeTheme[name] !== undefined ? activeTheme[name] : fallback
    }

    function openUrl(url) {
        if (chatViewModel)
            chatViewModel.openUrl(url)
    }

    function timelineActions(stableId) {
        // Make action models reactive to protocol permission/state changes.
        const revision = chatViewModel ? chatViewModel.timelineActionsRevision : 0
        return chatViewModel ? chatViewModel.timelineActions(stableId) : []
    }

    function executeTimelineAction(stableId, action) {
        if (chatViewModel)
            chatViewModel.executeTimelineAction(stableId, action)
    }

    function copyText(text) {
        if (chatViewModel)
            chatViewModel.copyText(text)
    }

    function removeOwnReaction(stableId, key) {
        if (chatViewModel)
            chatViewModel.removeOwnReaction(stableId, key)
    }

    function frequentReactionEmojis() {
        return chatViewModel ? chatViewModel.frequentReactionEmojis() : []
    }

    function addReaction(stableId, key) {
        if (chatViewModel)
            chatViewModel.addReaction(stableId, key)
    }

    function requestFullReactionSelector() {
        if (chatViewModel)
            chatViewModel.requestFullReactionSelector()
    }

    function cancelComposerContext() {
        if (chatViewModel)
            chatViewModel.cancelComposerContext()
    }

    function togglePinnedMessages() {
        if (!pinnedMessagesContainer.rendererItem)
            return

        if (pinnedMessagesContainer.rendererItem.togglePinnedMessages)
            pinnedMessagesContainer.rendererItem.togglePinnedMessages()
        else if (pinnedMessagesContainer.rendererItem.showPinnedMessages)
            pinnedMessagesContainer.rendererItem.showPinnedMessages()
    }

    function bindComposerContext(item) {
        item.width = Qt.binding(function() {
            return Math.max(1, Math.min(460, composerContextContainer.width - 40))
        })
        if (item.colorScheme !== undefined)
            item.colorScheme = Qt.binding(function() { return root.activeThemeColorScheme })
        if (item.customColors !== undefined)
            item.customColors = Qt.binding(function() { return root.activeCustomColors })
        if (item.chatFont !== undefined)
            item.chatFont = Qt.binding(function() { return root.activeChatFont })
        if (item.context !== undefined)
            item.context = Qt.binding(function() {
                return root.chatViewModel ? root.chatViewModel.composerContext : ({})
            })
        if (item.cancelComposerContext !== undefined)
            item.cancelComposerContext = root.cancelComposerContext
    }

    function bindPinnedMessagesPanel(item) {
        item.width = Qt.binding(function() { return pinnedMessagesContainer.width })
        if (item.colorScheme !== undefined)
            item.colorScheme = Qt.binding(function() { return root.activeThemeColorScheme })
        if (item.customColors !== undefined)
            item.customColors = Qt.binding(function() { return root.activeCustomColors })
        if (item.chatFont !== undefined)
            item.chatFont = Qt.binding(function() { return root.activeChatFont })
        if (item.openUrl !== undefined)
            item.openUrl = root.openUrl
        if (item.openImage !== undefined)
            item.openImage = root.openImage
        if (item.openLocation !== undefined)
            item.openLocation = root.openLocation
        if (item.pinnedMessages !== undefined)
            item.pinnedMessages = Qt.binding(function() {
                return root.chatViewModel ? root.chatViewModel.pinnedMessages : []
            })
        if (item.timelineActions !== undefined)
            item.timelineActions = root.timelineActions
        if (item.executeTimelineAction !== undefined)
            item.executeTimelineAction = root.executeTimelineAction
        if (item.copyText !== undefined)
            item.copyText = root.copyText
        if (item.removeOwnReaction !== undefined)
            item.removeOwnReaction = root.removeOwnReaction
        if (item.frequentReactionEmojis !== undefined)
            item.frequentReactionEmojis = root.frequentReactionEmojis
        if (item.addReaction !== undefined)
            item.addReaction = root.addReaction
        if (item.requestFullReactionSelector !== undefined)
            item.requestFullReactionSelector = root.requestFullReactionSelector
    }

    function defaultComposerContext() {
        if (!defaultComposerContextComponent)
            defaultComposerContextComponent = Qt.createComponent(
                        "qrc:/Kadu/Chat/chat/qml/styles/KaduClassic/KaduClassicComposerContext.qml")
        return defaultComposerContextComponent
    }

    function defaultComposerOverlay() {
        if (!defaultComposerOverlayComponent)
            defaultComposerOverlayComponent = Qt.createComponent(
                        "qrc:/Kadu/Chat/chat/qml/styles/KaduClassic/KaduClassicComposerOverlay.qml")
        return defaultComposerOverlayComponent
    }

    function defaultPinnedMessagesPanel() {
        if (!defaultPinnedMessagesPanelComponent)
            defaultPinnedMessagesPanelComponent = Qt.createComponent(
                        "qrc:/Kadu/Chat/chat/qml/styles/KaduClassic/KaduClassicPinnedMessages.qml")
        return defaultPinnedMessagesPanelComponent
    }

    function applyTimelineStyleProperties() {
        if (!chatViewModel || !chatViewModel.timeline)
            return

        const configuredInterval = Number(themeValue("groupingIntervalSeconds", 300))
        chatViewModel.timeline.groupingIntervalSeconds = isNaN(configuredInterval)
                                                      ? 300 : Math.max(0, Math.round(configuredInterval))
    }

    function openImage(sourceUri, title, width, height, state) {
        imageViewer.openFor(sourceUri, title, width, height, state)
    }

    function openLocation(geoUri) {
        locationViewer.openFor(geoUri)
    }

    readonly property bool darkSurface: themeValue("darkSurface", systemDarkSurface)
    readonly property bool usesSystemColors: activeThemeColorScheme === "System"
    readonly property color fallbackBackgroundColor: usesSystemColors ? systemPalette.base
                                                                  : (darkSurface ? "#20242b" : "#f7f7f7")
    readonly property color fallbackTextColor: usesSystemColors ? systemPalette.text
                                                            : (darkSurface ? "#f2f4f8" : "#202020")
    readonly property color fallbackMutedTextColor: usesSystemColors ? systemPalette.mid
                                                                 : (darkSurface ? "#aeb8c7" : "#666666")
    readonly property color fallbackSeparatorColor: usesSystemColors ? systemPalette.mid
                                                                 : (darkSurface ? "#6d7785" : "#858585")

    function bindTimelineItem(item, delegate) {
        item.width = Qt.binding(function() { return delegate.width })
        item.stableId = Qt.binding(function() { return delegate.stableId })
        if (item.protocolEventType !== undefined)
            item.protocolEventType = Qt.binding(function() { return delegate.protocolEventType })
        item.kind = Qt.binding(function() { return delegate.kind })
        item.timestamp = Qt.binding(function() { return delegate.timestamp })
        item.ownEvent = Qt.binding(function() { return delegate.ownEvent })
        item.senderDisplayName = Qt.binding(function() { return delegate.senderDisplayName })
        if (item.senderAvatarSource !== undefined)
            item.senderAvatarSource = Qt.binding(function() { return delegate.senderAvatarSource })
        if (item.senderColor !== undefined)
            item.senderColor = Qt.binding(function() { return delegate.senderColor })
        if (item.chatFont !== undefined)
            item.chatFont = Qt.binding(function() { return root.activeChatFont })
        item.plainText = Qt.binding(function() { return delegate.plainText })
        item.formattedText = Qt.binding(function() { return delegate.formattedText })
        if (item.replyToId !== undefined)
            item.replyToId = Qt.binding(function() { return delegate.replyToId })
        if (item.reply !== undefined)
            item.reply = Qt.binding(function() { return delegate.reply })
        if (item.attachments !== undefined)
            item.attachments = Qt.binding(function() { return delegate.attachments })
        if (item.locationUri !== undefined)
            item.locationUri = Qt.binding(function() { return delegate.locationUri })
        if (item.reactions !== undefined)
            item.reactions = Qt.binding(function() { return delegate.reactions })
        item.showSender = Qt.binding(function() { return delegate.showSender })
        if (item.showAvatar !== undefined)
            item.showAvatar = Qt.binding(function() { return delegate.showAvatar })
        item.showTimestamp = Qt.binding(function() { return delegate.showTimestamp })
        item.startsNewDay = Qt.binding(function() { return delegate.startsNewDay })
        item.deliveryState = Qt.binding(function() { return delegate.deliveryState })
        if (item.edited !== undefined)
            item.edited = Qt.binding(function() { return delegate.edited })
        if (item.systemEvent !== undefined)
            item.systemEvent = Qt.binding(function() { return delegate.systemEvent })
        if (item.emote !== undefined)
            item.emote = Qt.binding(function() { return delegate.emote })
        item.redacted = Qt.binding(function() { return delegate.redacted })
        item.encrypted = Qt.binding(function() { return delegate.encrypted })
        item.decryptionState = Qt.binding(function() { return delegate.decryptionState })
        item.errorText = Qt.binding(function() { return delegate.errorText })
        if (item.openImage !== undefined)
            item.openImage = root.openImage
        if (item.openLocation !== undefined)
            item.openLocation = root.openLocation
        if (item.timelineActions !== undefined)
            item.timelineActions = root.timelineActions
        if (item.executeTimelineAction !== undefined)
            item.executeTimelineAction = root.executeTimelineAction
        if (item.copyText !== undefined)
            item.copyText = root.copyText
        if (item.removeOwnReaction !== undefined)
            item.removeOwnReaction = root.removeOwnReaction
        if (item.frequentReactionEmojis !== undefined)
            item.frequentReactionEmojis = root.frequentReactionEmojis
        if (item.addReaction !== undefined)
            item.addReaction = root.addReaction
        if (item.requestFullReactionSelector !== undefined)
            item.requestFullReactionSelector = root.requestFullReactionSelector
    }

    function atBottom() {
        const maximum = Math.max(timeline.originY, timeline.contentHeight - timeline.height + timeline.originY)
        return timeline.contentY >= maximum - 8
    }

    function scrollToBottom() {
        timeline.positionViewAtEnd()
        followingTail = true
        if (chatViewModel)
            chatViewModel.setTimelineAtNewest(true)
    }

    function scrollPage(direction) {
        const maximum = Math.max(timeline.originY, timeline.contentHeight - timeline.height + timeline.originY)
        const pageSize = Math.max(1, timeline.height - 24)
        timeline.contentY = Math.max(timeline.originY, Math.min(maximum, timeline.contentY + direction * pageSize))
        followingTail = root.atBottom()
        if (chatViewModel)
            chatViewModel.setTimelineAtNewest(followingTail)
        revealScrollBar()
    }

    function updateVisibleTimelineItem() {
        if (!chatViewModel || timeline.count === 0)
            return

        const row = timeline.indexAt(timeline.width / 2, timeline.contentY + timeline.height - 2)
        const item = timeline.itemAtIndex(Math.max(0, row))
        if (item)
            chatViewModel.markTimelineItemVisible(item.stableId)
    }

    function revealScrollBar() {
        scrollBarVisible = true
        scrollBarHideTimer.restart()
    }

    Timer {
        id: scrollBarHideTimer
        interval: 900
        onTriggered: root.scrollBarVisible = false
    }

    function requestOlder() {
        if (!chatViewModel || !chatViewModel.hasOlder || chatViewModel.loadingInitial || chatViewModel.loadingOlder)
            return

        const row = timeline.indexAt(timeline.width / 2, Math.max(timeline.contentY, timeline.originY) + 2)
        const item = timeline.itemAtIndex(Math.max(0, row))
        olderAnchorId = item ? item.stableId : ""
        olderAnchorOffset = item ? timeline.contentY - item.y : 0
        chatViewModel.loadOlder()
    }

    function restoreOlderAnchor() {
        if (!chatViewModel || olderAnchorId.length === 0)
            return
        const index = chatViewModel.timeline.rowForStableId(olderAnchorId)
        if (index >= 0) {
            timeline.positionViewAtIndex(index, ListView.Beginning)
            Qt.callLater(function() {
                const item = timeline.itemAtIndex(index)
                if (item)
                    timeline.contentY = item.y + olderAnchorOffset
            })
        }
        olderAnchorId = ""
    }

    Loader {
        id: themeLoader
        source: root.activeThemeSource

        onLoaded: {
            item.colorScheme = root.activeThemeColorScheme
            if (item.customColors !== undefined)
                item.customColors = root.activeCustomColors
            if (item.chatFont !== undefined)
                item.chatFont = root.activeChatFont
            if (item.openUrl !== undefined)
                item.openUrl = root.openUrl
            if (item.timelineActions !== undefined)
                item.timelineActions = root.timelineActions
            if (item.executeTimelineAction !== undefined)
                item.executeTimelineAction = root.executeTimelineAction
            if (item.copyText !== undefined)
                item.copyText = root.copyText
            root.applyTimelineStyleProperties()
        }
    }

    onActiveThemeChanged: applyTimelineStyleProperties()

    onActiveThemeColorSchemeChanged: {
        if (themeLoader.item)
            themeLoader.item.colorScheme = activeThemeColorScheme
    }

    onActiveCustomColorsChanged: {
        if (themeLoader.item && themeLoader.item.customColors !== undefined)
            themeLoader.item.customColors = activeCustomColors
    }

    onActiveChatFontChanged: {
        if (themeLoader.item && themeLoader.item.chatFont !== undefined)
            themeLoader.item.chatFont = activeChatFont
    }

    Rectangle {
        anchors.fill: parent
        color: root.themeValue("backgroundColor", root.fallbackBackgroundColor)
    }

    Rectangle {
        id: roomHeader
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: visible ? Math.max(48, roomHeaderContent.implicitHeight + 16) : 0
        visible: root.chatViewModel && root.chatViewModel.chatHeaderVisible
        color: root.themeValue("roomHeaderBackgroundColor", root.darkSurface ? "#2d323a" : "#f4f6f8")
        border.width: 1
        border.color: root.themeValue("separatorColor", root.fallbackSeparatorColor)
        clip: true

        Item {
            id: roomHeaderContent
            x: 8
            y: 8
            width: parent.width - 16
            implicitHeight: Math.max(roomAvatar.height, headerDetails.implicitHeight)

            Item {
                id: roomAvatar
                width: roomAvatarImage.status === Image.Ready ? 40 : 0
                height: width

                Image {
                    id: roomAvatarImage
                    anchors.fill: parent
                    source: root.chatViewModel ? root.chatViewModel.chatHeaderAvatarSource : ""
                    sourceSize.width: 40
                    sourceSize.height: 40
                    fillMode: Image.PreserveAspectFit
                    visible: status === Image.Ready
                }
            }

            Column {
                id: headerDetails
                anchors.left: roomAvatar.right
                anchors.leftMargin: roomAvatar.width > 0 ? 10 : 0
                anchors.right: headerActions.left
                anchors.rightMargin: headerActions.width > 0 ? 8 : 0
                spacing: 2

                Text {
                    width: parent.width
                    text: root.chatViewModel ? root.chatViewModel.chatHeaderTitle : ""
                    color: root.themeValue("textColor", root.fallbackTextColor)
                    elide: Text.ElideRight
                    font.bold: true
                    font.pixelSize: 14
                }

                Text {
                    visible: text.length > 0
                    width: parent.width
                    text: root.chatViewModel ? root.chatViewModel.chatHeaderDescription : ""
                    color: root.themeValue("textColor", root.fallbackTextColor)
                    opacity: 0.70
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    font.pixelSize: 12
                }
            }

            Row {
                id: headerActions
                anchors.top: parent.top
                anchors.right: parent.right
                spacing: 2

                Repeater {
                    model: root.chatViewModel ? root.chatViewModel.chatHeaderActions : []

                    delegate: ToolButton {
                        id: actionButton
                        required property var modelData

                        width: 28
                        height: 28
                        padding: 6
                        Accessible.name: modelData.text || ""
                        ToolTip.visible: hovered
                        ToolTip.text: modelData.text || ""
                        background: Item {}

                        contentItem: Item {
                            Image {
                                id: actionIcon
                                anchors.centerIn: parent
                                width: 16
                                height: 16
                                source: actionButton.modelData.iconName
                                        ? "image://kaduicon/" + encodeURIComponent(actionButton.modelData.iconName)
                                        : ""
                                visible: status === Image.Ready && sourceSize.width > 1
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: !actionIcon.visible
                                text: "•"
                                color: root.themeValue("textColor", root.fallbackTextColor)
                                font.pixelSize: 18
                            }
                        }

                        onClicked: {
                            if (root.chatViewModel)
                                root.chatViewModel.executeChatHeaderAction(modelData.id)
                        }
                    }
                }
            }
        }
    }

    Item {
        id: pinnedMessagesContainer
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: roomHeader.bottom
        height: rendererItem ? rendererItem.implicitHeight : 0
        z: 4

        property var rendererItem: null

        function createRenderer() {
            if (rendererItem) {
                rendererItem.destroy()
                rendererItem = null
            }

            const component = root.activeTheme && root.activeTheme.pinnedMessagesPanel
                              ? root.activeTheme.pinnedMessagesPanel : root.defaultPinnedMessagesPanel()
            if (!component || component.status !== Component.Ready)
                return
            rendererItem = component.createObject(pinnedMessagesContainer, {
                "width": pinnedMessagesContainer.width
            })
            if (rendererItem)
                root.bindPinnedMessagesPanel(rendererItem)
        }

        Component.onCompleted: createRenderer()
        Component.onDestruction: {
            if (rendererItem)
                rendererItem.destroy()
        }

        Connections {
            target: root

            function onActiveThemeChanged() {
                pinnedMessagesContainer.createRenderer()
            }
        }
    }

    ListView {
        id: timeline
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: pinnedMessagesContainer.bottom
        anchors.bottom: parent.bottom
        anchors.margins: root.themeValue("timelineMargin", 16)
        clip: true
        interactive: !root.composerActive
        spacing: root.themeValue("timelineSpacing", 4)
        model: root.chatViewModel ? root.chatViewModel.timeline : null
        reuseItems: true
        boundsBehavior: Flickable.StopAtBounds
        focus: true
        Accessible.role: Accessible.List
        Accessible.name: qsTr("Conversation history")
        Accessible.description: qsTr("Use Page Up and Page Down to browse messages, Home for the beginning and End for the newest message.")

        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_PageUp) {
                root.scrollPage(-1)
                event.accepted = true
            } else if (event.key === Qt.Key_PageDown) {
                root.scrollPage(1)
                event.accepted = true
            } else if (event.key === Qt.Key_Home) {
                timeline.positionViewAtBeginning()
                root.followingTail = false
                if (root.chatViewModel)
                    root.chatViewModel.setTimelineAtNewest(false)
                root.revealScrollBar()
                event.accepted = true
            } else if (event.key === Qt.Key_End) {
                root.scrollToBottom()
                event.accepted = true
            }
        }

        ScrollBar.vertical: ScrollBar {
            id: verticalScrollBar
            policy: ScrollBar.AsNeeded
            opacity: timeline.moving || timeline.dragging || pressed || hovered || root.scrollBarVisible
                     ? 1.0 : 0.0

            Behavior on opacity {
                NumberAnimation { duration: 140 }
            }

            onPressedChanged: if (pressed) root.revealScrollBar()
        }

        onContentYChanged: {
            if (moving || dragging)
                root.revealScrollBar()
            if (moving || dragging || verticalScrollBar.pressed)
                followingTail = root.atBottom()
            if (moving || dragging || verticalScrollBar.pressed) {
                if (root.chatViewModel)
                    root.chatViewModel.setTimelineAtNewest(followingTail)
                root.updateVisibleTimelineItem()
            }
            if (contentY <= originY + 64)
                root.requestOlder()
        }
        onContentHeightChanged: {
            if (root.followingTail && root.initialPositioned && root.chatViewModel &&
                    !root.chatViewModel.loadingInitial && !root.chatViewModel.loadingOlder)
                Qt.callLater(root.scrollToBottom)
        }
        onMovementStarted: root.revealScrollBar()
        onMovementEnded: scrollBarHideTimer.restart()
        onAtYBeginningChanged: if (atYBeginning) root.requestOlder()

        header: Item {
            width: timeline.width
            height: 44

            Row {
                anchors.centerIn: parent
                visible: root.chatViewModel && !root.chatViewModel.hasOlder && !root.chatViewModel.loadingInitial
                spacing: 8
                Rectangle { width: 70; height: 1; color: root.themeValue("separatorColor", root.fallbackSeparatorColor) }
                Text {
                    text: qsTr("Beginning of history")
                    color: root.themeValue("mutedTextColor", root.fallbackMutedTextColor)
                    font.pixelSize: 12
                }
                Rectangle { width: 70; height: 1; color: root.themeValue("separatorColor", root.fallbackSeparatorColor) }
            }
        }

        delegate: Item {
            id: delegateRoot
            required property string stableId
            required property string protocolEventType
            required property int kind
            required property var timestamp
            required property bool ownEvent
            required property string senderDisplayName
            required property url senderAvatarSource
            required property color senderColor
            required property string plainText
            required property string formattedText
            required property string replyToId
            required property var reply
            required property var attachments
            required property string locationUri
            required property var reactions
            required property bool showSender
            required property bool showAvatar
            required property bool showTimestamp
            required property bool startsNewDay
            required property int deliveryState
            required property bool edited
            required property bool systemEvent
            required property bool emote
            required property bool redacted
            required property bool encrypted
            required property int decryptionState
            required property string errorText

            width: timeline.width
            property var rendererItem: null

            readonly property bool isFirstNewEvent: root.newEventsBelow > 0 && index === timeline.count - root.newEventsBelow
            height: (isFirstNewEvent ? newMessagesMarker.implicitHeight : 0) + (rendererItem ? rendererItem.implicitHeight : 0)
            Accessible.role: Accessible.ListItem
            Accessible.name: senderDisplayName.length > 0
                             ? senderDisplayName + ": " + plainText
                             : plainText

            Item {
                id: newMessagesMarker
                width: parent.width
                implicitHeight: visible ? 30 : 0
                visible: delegateRoot.isFirstNewEvent
                Accessible.role: Accessible.Separator
                Accessible.name: qsTr("New messages")

                Row {
                    anchors.centerIn: parent
                    spacing: 8
                    Rectangle { width: 64; height: 1; color: root.themeValue("separatorColor", root.fallbackSeparatorColor) }
                    Text {
                        text: qsTr("New messages")
                        color: root.themeValue("mutedTextColor", root.fallbackMutedTextColor)
                        font.pixelSize: 12
                    }
                    Rectangle { width: 64; height: 1; color: root.themeValue("separatorColor", root.fallbackSeparatorColor) }
                }
            }

            function createRenderer() {
                if (rendererItem) {
                    rendererItem.destroy()
                    rendererItem = null
                }

                const component = root.activeTheme ? root.activeTheme.timelineItem : null
                if (!component)
                    return

                rendererItem = component.createObject(delegateRoot, {
                    "width": delegateRoot.width,
                    "stableId": delegateRoot.stableId,
                    "protocolEventType": delegateRoot.protocolEventType,
                    "kind": delegateRoot.kind,
                    "timestamp": delegateRoot.timestamp,
                    "ownEvent": delegateRoot.ownEvent,
                    "senderDisplayName": delegateRoot.senderDisplayName,
                    "senderAvatarSource": delegateRoot.senderAvatarSource,
                    "senderColor": delegateRoot.senderColor,
                    "plainText": delegateRoot.plainText,
                    "formattedText": delegateRoot.formattedText,
                    "replyToId": delegateRoot.replyToId,
                    "attachments": delegateRoot.attachments,
                    "locationUri": delegateRoot.locationUri,
                    "reactions": delegateRoot.reactions,
                    "showSender": delegateRoot.showSender,
                    "showAvatar": delegateRoot.showAvatar,
                    "showTimestamp": delegateRoot.showTimestamp,
                    "startsNewDay": delegateRoot.startsNewDay,
                    "deliveryState": delegateRoot.deliveryState,
                    "edited": delegateRoot.edited,
                    "systemEvent": delegateRoot.systemEvent,
                    "redacted": delegateRoot.redacted,
                    "encrypted": delegateRoot.encrypted,
                    "decryptionState": delegateRoot.decryptionState,
                    "errorText": delegateRoot.errorText,
                    "chatFont": root.activeChatFont
                })
                if (rendererItem) {
                    rendererItem.y = Qt.binding(function() { return newMessagesMarker.implicitHeight })
                    root.bindTimelineItem(rendererItem, delegateRoot)
                }
            }

            Component.onCompleted: createRenderer()
            Component.onDestruction: {
                if (rendererItem)
                    rendererItem.destroy()
            }

            Connections {
                target: root
                function onActiveThemeChanged() {
                    delegateRoot.createRenderer()
                }
            }
        }

        footer: Item {
            width: timeline.width
            height: root.chatViewModel && root.chatViewModel.loadingOlder ? 40 : 8
            Text {
                anchors.centerIn: parent
                visible: root.chatViewModel && root.chatViewModel.loadingOlder
                text: qsTr("Loading older messages…")
                color: root.themeValue("mutedTextColor", root.fallbackMutedTextColor)
            }
        }
    }

    MultiEffect {
        anchors.fill: timeline
        source: timeline
        visible: root.composerActive
        blurEnabled: visible
        blur: 0.70
        saturation: 0.55
        z: 5
    }

    Text {
        anchors.centerIn: parent
        visible: root.chatViewModel && root.chatViewModel.loadingInitial
        text: qsTr("Loading messages…")
        color: root.themeValue("loadingTextColor", root.fallbackTextColor)
        z: 2
    }

    Button {
        id: newMessagesButton
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 14
        visible: root.newEventsBelow > 0 && !root.composerActive
        text: root.newEventsBelow === 1 ? qsTr("1 new message") : qsTr("%1 new messages").arg(root.newEventsBelow)
        z: 3
        Accessible.name: text
        onClicked: root.scrollToBottom()
    }

    Item {
        id: composerContextContainer
        anchors.left: timeline.left
        anchors.right: timeline.right
        anchors.top: timeline.top
        anchors.bottom: timeline.bottom
        visible: root.composerActive
        z: 10

        property var rendererItem: null

        function createRenderer() {
            if (rendererItem) {
                rendererItem.destroy()
                rendererItem = null
            }
            if (!root.composerActive)
                return

            const component = root.activeTheme && root.activeTheme.composerOverlay
                              ? root.activeTheme.composerOverlay
                              : (root.activeTheme && root.activeTheme.composerContext
                                 ? root.activeTheme.composerContext : root.defaultComposerOverlay())
            if (!component || component.status !== Component.Ready)
                return
            rendererItem = component.createObject(composerContextContainer)
            if (rendererItem) {
                rendererItem.z = 1
                root.bindComposerContext(rendererItem)
            }
        }

        Component.onCompleted: createRenderer()
        Component.onDestruction: {
            if (rendererItem)
                rendererItem.destroy()
        }

        Connections {
            target: root

            function onActiveThemeChanged() {
                composerContextContainer.createRenderer()
            }
            function onComposerActiveChanged() {
                composerContextContainer.createRenderer()
            }
        }

        Rectangle {
            anchors.fill: parent
            color: "black"
            opacity: root.darkSurface ? 0.16 : 0.10
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            hoverEnabled: true
            preventStealing: true
            propagateComposedEvents: false

            onWheel: function(wheel) {
                wheel.accepted = true
            }
        }

        Binding {
            target: composerContextContainer.rendererItem
            property: "width"
            value: Math.max(1, Math.min(460, composerContextContainer.width - 40))
            when: composerContextContainer.rendererItem !== null
        }

        Binding {
            target: composerContextContainer.rendererItem
            property: "x"
            value: {
                const item = composerContextContainer.rendererItem
                return item ? Math.max(0, (composerContextContainer.width - item.width) / 2) : 0
            }
            when: composerContextContainer.rendererItem !== null
        }

        Binding {
            target: composerContextContainer.rendererItem
            property: "y"
            value: {
                const item = composerContextContainer.rendererItem
                return item ? Math.max(0, composerContextContainer.height - item.implicitHeight - 16) : 0
            }
            when: composerContextContainer.rendererItem !== null
        }
    }

    ImageViewerDialog {
        id: imageViewer
        parentItem: root
        reloadToken: root.attachmentImageRevision
    }

    LocationViewerDialog {
        id: locationViewer
        parentItem: root
    }

    ReactionSelectorPopup {
        id: contextReactionSelector
        parent: root
        addReaction: root.addReaction
        requestFullSelector: root.requestFullReactionSelector
        textColor: root.fallbackTextColor
        backgroundColor: root.fallbackBackgroundColor
    }

    Connections {
        target: root.chatViewModel
        function onTimelineStateChanged() {
            ++root.attachmentImageRevision
            if (!root.chatViewModel)
                return
            if (!root.chatViewModel.loadingInitial && !root.initialPositioned) {
                root.initialPositioned = true
                Qt.callLater(root.scrollToBottom)
            }
            if (!root.chatViewModel.loadingOlder)
                Qt.callLater(root.restoreOlderAnchor)
        }
        function onComposerContextChanged() {
            composerContextContainer.createRenderer()
        }
        function onPinnedMessagesChanged() {
            if (pinnedMessagesContainer.rendererItem)
                root.bindPinnedMessagesPanel(pinnedMessagesContainer.rendererItem)
        }
        function onPinnedMessagesRequested() {
            root.togglePinnedMessages()
        }
        function onReactionSelectorRequested(stableId) {
            contextReactionSelector.stableId = stableId
            contextReactionSelector.emojiProvider = root.frequentReactionEmojis
            contextReactionSelector.openFor(timeline)
        }
    }

    Connections {
        target: root.chatViewModel ? root.chatViewModel.timeline : null
        function onDataChanged() {
            if (imageViewer.visible)
                ++root.attachmentImageRevision
        }
        function onRowsInserted(parent, first, last) {
            if (!root.chatViewModel)
                return
            if (!root.initialPositioned && !root.chatViewModel.loadingInitial) {
                root.initialPositioned = true
                Qt.callLater(root.scrollToBottom)
            } else if (root.followingTail && !root.chatViewModel.loadingOlder) {
                Qt.callLater(root.scrollToBottom)
            }
        }
    }
}
