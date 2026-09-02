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
    readonly property url fallbackThemeSource: "qrc:/Kadu/Chat/chat/qml/styles/KaduClassic/KaduClassicChatStyle.qml"
    property url effectiveThemeSource: activeThemeSource
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
    property int tailScrollPassesRemaining: 0
    property bool scrollBarVisible: false
    property int attachmentImageRevision: 0
    property string olderAnchorId: ""
    property real olderAnchorOffset: 0
    property string highlightedStableId: ""
    property var defaultComposerContextComponent: null
    property var defaultComposerOverlayComponent: null
    property var defaultPinnedMessagesPanelComponent: null
    property var timelineActionsCache: ({})
    readonly property int newEventsBelow: chatViewModel ? chatViewModel.newEventsBelow : 0
    readonly property int latestMessagesHidden: {
        if (!timeline || timeline.count === 0)
            return 0

        const maximum = Math.max(timeline.originY, timeline.contentHeight - timeline.height + timeline.originY)
        if (timeline.contentY >= maximum - 8)
            return 0

        const bottomIndex = timeline.indexAt(timeline.width / 2, timeline.contentY + timeline.height - 2)
        return bottomIndex < 0 ? 0 : Math.max(0, timeline.count - bottomIndex - 1)
    }
    readonly property bool jumpToLatestVisible: (chatViewModel && chatViewModel.hasNewer) || latestMessagesHidden >= 3

    onChatViewModelChanged: {
        timelineActionsCache = ({})
    }

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

    function useFallbackTheme() {
        if (effectiveThemeSource.toString() !== fallbackThemeSource.toString())
            effectiveThemeSource = fallbackThemeSource
    }

    function attachmentEventId(sourceUri) {
        const value = sourceUri ? sourceUri.toString() : ""
        const prefix = "kaduimg:/"
        if (!value.startsWith(prefix))
            return ""
        const queryOffset = value.indexOf("?", prefix.length)
        const encodedId = value.substring(prefix.length, queryOffset < 0 ? value.length : queryOffset)
        try {
            return decodeURIComponent(encodedId)
        } catch (error) {
            return encodedId
        }
    }

    function openUrl(url) {
        if (chatViewModel)
            chatViewModel.openUrl(url)
    }

    function timelineActions(stableId) {
        // Make action models reactive to protocol permission/state changes.
        const revision = chatViewModel ? chatViewModel.timelineActionsRevision : 0
        const cacheKey = String(revision) + ":" + String(stableId)
        if (Object.prototype.hasOwnProperty.call(timelineActionsCache, cacheKey))
            return timelineActionsCache[cacheKey]
        const actions = chatViewModel ? chatViewModel.timelineActions(stableId) : []
        timelineActionsCache[cacheKey] = actions
        return actions
    }

    function executeTimelineAction(stableId, action) {
        if (!chatViewModel)
            return

        const actions = timelineActions(stableId)
        const selectedAction = actions.find(function(candidate) { return candidate.id === action })
        if (selectedAction && selectedAction.confirmationText) {
            timelineMessagePopup.openFor(
                stableId, selectedAction.text, selectedAction.confirmationText, true,
                selectedAction.confirmationActionText,
                function() {
                    if (root.chatViewModel)
                        root.chatViewModel.executeTimelineAction(stableId, action)
                })
            return
        }

        chatViewModel.executeTimelineAction(stableId, action)
    }

    function timelineMessagePopupPosition(stableId, popupWidth, popupHeight) {
        const index = chatViewModel ? chatViewModel.timeline.rowForStableId(stableId) : -1
        const item = index >= 0 ? timeline.itemAtIndex(index) : null
        if (!item)
            return { "x": Math.max(0, (width - popupWidth) / 2),
                     "y": Math.max(0, (height - popupHeight) / 2) }

        const point = item.mapToItem(root, item.width / 2, 0)
        const x = Math.max(0, Math.min(point.x - popupWidth / 2, width - popupWidth))
        const below = point.y + item.height + 6
        const y = below + popupHeight <= height
                  ? below : Math.max(0, point.y - popupHeight - 6)
        return { "x": x, "y": y }
    }

    function copyText(text) {
        if (chatViewModel)
            chatViewModel.copyText(text)
    }

    function removeOwnReaction(stableId, key) {
        if (chatViewModel)
            chatViewModel.removeOwnReaction(stableId, key)
    }

    function addReaction(stableId, key) {
        if (chatViewModel)
            chatViewModel.addReaction(stableId, key)
    }

    function jumpToTimelineItem(stableId) {
        if (!chatViewModel || !stableId)
            return
        followingTail = false
        chatViewModel.setTimelineAtNewest(false)
        chatViewModel.jumpToTimelineItem(stableId)
    }

    function requestFullReactionSelector(stableId, sourceItem) {
        if (!stableId || !reactionEmojiPicker)
            return
        reactionEmojiPicker.stableId = stableId
        reactionEmojiPicker.openFor(sourceItem || timeline)
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

    function pinnedMessagesPopupOpened() {
        timeline.cancelFlick()
    }

    function pinnedMessagesPopupClosed() {
        // A modal popup owns focus and wheel delivery while it is open. Wait
        // until Controls removes its overlay before returning both to the
        // conversation and checking whether pagination should resume.
        Qt.callLater(function() {
            timeline.forceActiveFocus()
            timeline.returnToBounds()
            Qt.callLater(function() {
                if (timeline.contentY <= timeline.originY + 64)
                    root.requestOlder()
                if (timeline.contentY + timeline.height >=
                        timeline.originY + timeline.contentHeight - 64)
                    root.requestNewer()
                root.updateVisibleTimelineItem()
            })
        })
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
        if (item.actionsRevision !== undefined)
            item.actionsRevision = Qt.binding(function() {
                return root.chatViewModel ? root.chatViewModel.timelineActionsRevision : 0
            })
        if (item.executeTimelineAction !== undefined)
            item.executeTimelineAction = root.executeTimelineAction
        if (item.copyText !== undefined)
            item.copyText = root.copyText
        if (item.removeOwnReaction !== undefined)
            item.removeOwnReaction = root.removeOwnReaction
        if (item.addReaction !== undefined)
            item.addReaction = root.addReaction
        if (item.requestFullReactionSelector !== undefined)
            item.requestFullReactionSelector = root.requestFullReactionSelector
        if (item.jumpToTimelineItem !== undefined)
            item.jumpToTimelineItem = root.jumpToTimelineItem
        if (item.popupOpened !== undefined)
            item.popupOpened = root.pinnedMessagesPopupOpened
        if (item.popupClosed !== undefined)
            item.popupClosed = root.pinnedMessagesPopupClosed
    }

    function bindStatusBar(item) {
        item.width = Qt.binding(function() { return statusBarContainer.width })
        item.height = Qt.binding(function() { return item.implicitHeight })
        if (item.colorScheme !== undefined)
            item.colorScheme = Qt.binding(function() { return root.activeThemeColorScheme })
        if (item.chatTitle !== undefined)
            item.chatTitle = Qt.binding(function() { return root.chatViewModel ? root.chatViewModel.title : "" })
        if (item.ownDisplayName !== undefined)
            item.ownDisplayName = Qt.binding(function() { return root.chatViewModel ? root.chatViewModel.ownDisplayName : "" })
        if (item.roomInfo !== undefined)
            item.roomInfo = Qt.binding(function() { return root.chatViewModel ? root.chatViewModel.roomInfo : ({}) })
        if (item.pinnedMessages !== undefined)
            item.pinnedMessages = Qt.binding(function() { return root.chatViewModel ? root.chatViewModel.pinnedMessages : [] })
        if (item.showPinnedMessages !== undefined)
            item.showPinnedMessages = root.togglePinnedMessages
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
        if (item.senderId !== undefined)
            item.senderId = Qt.binding(function() { return delegate.senderId })
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
        if (item.addReaction !== undefined)
            item.addReaction = root.addReaction
        if (item.requestFullReactionSelector !== undefined)
            item.requestFullReactionSelector = root.requestFullReactionSelector
        if (item.jumpToTimelineItem !== undefined)
            item.jumpToTimelineItem = root.jumpToTimelineItem
    }

    function atBottom() {
        const maximum = Math.max(timeline.originY, timeline.contentHeight - timeline.height + timeline.originY)
        return timeline.contentY >= maximum - 8
    }

    function scrollToBottom() {
        timeline.positionViewAtEnd()
        timeline.contentY = Math.max(timeline.originY,
                                     timeline.contentHeight - timeline.height + timeline.originY)
        followingTail = true
        if (chatViewModel)
            chatViewModel.setTimelineAtNewest(true)
    }

    function scheduleScrollToBottom(passes) {
        const requestedPasses = passes === undefined ? 1 : Math.max(1, Number(passes))
        tailScrollPassesRemaining = Math.max(tailScrollPassesRemaining, requestedPasses)
        scrollToBottomTimer.restart()
    }

    function jumpToLatest() {
        if (chatViewModel && chatViewModel.hasNewer) {
            followingTail = true
            scheduleScrollToBottom(4)
            chatViewModel.loadLatest()
        } else {
            followingTail = true
            scheduleScrollToBottom(4)
        }
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

    Timer {
        id: scrollToBottomTimer
        interval: 16
        onTriggered: {
            if (!root.followingTail) {
                root.tailScrollPassesRemaining = 0
                return
            }
            if (!root.initialPositioned || !root.chatViewModel || root.chatViewModel.loadingInitial ||
                    root.chatViewModel.loadingOlder || root.chatViewModel.loadingNewer ||
                    root.chatViewModel.hasNewer)
                return

            root.scrollToBottom()
            root.tailScrollPassesRemaining = Math.max(0, root.tailScrollPassesRemaining - 1)
            if (root.tailScrollPassesRemaining > 0)
                scrollToBottomTimer.restart()
        }
    }

    function requestOlder() {
        if (!chatViewModel || !chatViewModel.hasOlder || chatViewModel.loadingInitial ||
                chatViewModel.loadingOlder || chatViewModel.loadingNewer)
            return

        const row = timeline.indexAt(timeline.width / 2, Math.max(timeline.contentY, timeline.originY) + 2)
        const item = timeline.itemAtIndex(Math.max(0, row))
        olderAnchorId = item ? item.stableId : ""
        olderAnchorOffset = item ? timeline.contentY - item.y : 0
        chatViewModel.loadOlder()
    }

    function requestNewer() {
        if (!chatViewModel || !chatViewModel.hasNewer || chatViewModel.loadingInitial ||
                chatViewModel.loadingOlder || chatViewModel.loadingNewer)
            return

        const row = timeline.indexAt(timeline.width / 2, timeline.contentY + timeline.height - 2)
        const item = timeline.itemAtIndex(Math.max(0, row))
        olderAnchorId = item ? item.stableId : ""
        olderAnchorOffset = item ? timeline.contentY - item.y : 0
        chatViewModel.loadNewer()
    }

    function positionTimelineItem(stableId) {
        if (!chatViewModel || !stableId)
            return
        const index = chatViewModel.timeline.rowForStableId(stableId)
        if (index < 0)
            return
        timeline.positionViewAtIndex(index, ListView.Center)
        highlightedStableId = stableId
        highlightTimer.restart()
        revealScrollBar()
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
        source: root.effectiveThemeSource

        onLoaded: {
            if (!item || item.timelineItem === undefined || !item.timelineItem) {
                root.useFallbackTheme()
                return
            }
            if (item.colorScheme !== undefined)
                item.colorScheme = root.activeThemeColorScheme
            if (item.customColors !== undefined)
                item.customColors = root.activeCustomColors
            if (item.chatFont !== undefined)
                item.chatFont = root.activeChatFont
            if (item.roomInfo !== undefined)
                item.roomInfo = Qt.binding(function() { return root.chatViewModel ? root.chatViewModel.roomInfo : ({}) })
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
        onStatusChanged: {
            if (status === Loader.Error)
                root.useFallbackTheme()
        }
    }

    Timer {
        id: highlightTimer
        interval: 1800
        onTriggered: root.highlightedStableId = ""
    }

    onActiveThemeChanged: applyTimelineStyleProperties()

    onActiveThemeSourceChanged: effectiveThemeSource = activeThemeSource

    onActiveThemeColorSchemeChanged: {
        if (themeLoader.item && themeLoader.item.colorScheme !== undefined)
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
        height: visible ? Math.max(root.themeValue("roomHeaderMinimumHeight", 48),
                                   roomHeaderContent.implicitHeight + root.themeValue("roomHeaderVerticalPadding", 16)) : 0
        visible: root.chatViewModel && root.chatViewModel.chatHeaderVisible &&
                 (!root.themeValue("roomHeaderRequiresDescription", false) ||
                  root.chatViewModel.chatHeaderDescription.length > 0)
        color: root.themeValue("roomHeaderBackgroundColor", root.darkSurface ? "#2d323a" : "#f4f6f8")
        border.width: 1
        border.color: root.themeValue("separatorColor", root.fallbackSeparatorColor)
        clip: true

        Item {
            id: roomHeaderContent
            x: root.themeValue("roomHeaderHorizontalPadding", 8)
            y: root.themeValue("roomHeaderVerticalPadding", 16) / 2
            width: parent.width - 2 * root.themeValue("roomHeaderHorizontalPadding", 8)
            implicitHeight: Math.max(roomAvatar.height, headerDetails.implicitHeight)

            Item {
                id: roomAvatar
                width: root.themeValue("roomHeaderShowAvatar", true) && roomAvatarImage.status === Image.Ready ? 40 : 0
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
                anchors.rightMargin: headerActions.visible && headerActions.width > 0 ? 8 : 0
                spacing: 2

                Text {
                    width: parent.width
                    visible: !root.themeValue("roomHeaderDescriptionOnly", false)
                    text: root.chatViewModel ? root.chatViewModel.chatHeaderTitle : ""
                    color: root.themeValue("textColor", root.fallbackTextColor)
                    elide: Text.ElideRight
                    font.bold: true
                    font.family: root.themeValue("roomHeaderFontFamily", "")
                    font.pixelSize: root.themeValue("roomHeaderTitleFontPixelSize", 14)
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
                    font.family: root.themeValue("roomHeaderFontFamily", "")
                    font.pixelSize: root.themeValue("roomHeaderDescriptionFontPixelSize", 12)
                }
            }

            Row {
                id: headerActions
                visible: root.themeValue("roomHeaderShowActions", true)
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

    Item {
        id: statusBarContainer
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        z: 4

        property var rendererItem: null
        height: rendererItem ? rendererItem.implicitHeight : 0

        function createRenderer() {
            if (rendererItem) {
                rendererItem.destroy()
                rendererItem = null
            }
            const component = root.activeTheme && root.activeTheme.statusBar ? root.activeTheme.statusBar : null
            if (!component || component.status !== Component.Ready)
                return
            rendererItem = component.createObject(statusBarContainer)
            if (rendererItem)
                root.bindStatusBar(rendererItem)
        }

        Component.onCompleted: createRenderer()
        Component.onDestruction: {
            if (rendererItem)
                rendererItem.destroy()
        }

        Connections {
            target: root

            function onActiveThemeChanged() {
                statusBarContainer.createRenderer()
            }
        }
    }

    ListView {
        id: timeline
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: pinnedMessagesContainer.bottom
        anchors.bottom: statusBarContainer.top
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
                root.jumpToLatest()
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
                followingTail = root.atBottom() && (!root.chatViewModel || !root.chatViewModel.hasNewer)
            if (moving || dragging || verticalScrollBar.pressed) {
                if (root.chatViewModel)
                    root.chatViewModel.setTimelineAtNewest(followingTail)
                root.updateVisibleTimelineItem()
            }
            if (contentY <= originY + 64)
                root.requestOlder()
            if (contentY + height >= originY + contentHeight - 64)
                root.requestNewer()
        }
        onContentHeightChanged: {
            if (root.followingTail && root.initialPositioned && root.chatViewModel &&
                    !root.chatViewModel.loadingInitial && !root.chatViewModel.loadingOlder &&
                    !root.chatViewModel.loadingNewer && !root.chatViewModel.hasNewer)
                root.scheduleScrollToBottom()
        }
        onMovementStarted: root.revealScrollBar()
        onMovementEnded: scrollBarHideTimer.restart()
        onAtYBeginningChanged: if (atYBeginning) root.requestOlder()
        onAtYEndChanged: if (atYEnd) root.requestNewer()

        header: Item {
            width: timeline.width
            height: 44

            Text {
                anchors.centerIn: parent
                visible: root.chatViewModel && root.chatViewModel.loadingOlder
                text: qsTr("Loading older messages…")
                color: root.themeValue("mutedTextColor", root.fallbackMutedTextColor)
            }

            Row {
                anchors.centerIn: parent
                visible: root.chatViewModel && root.chatViewModel.historyError.length === 0 &&
                         !root.chatViewModel.hasOlder && !root.chatViewModel.loadingInitial
                spacing: 8
                Rectangle { width: 70; height: 1; color: root.themeValue("separatorColor", root.fallbackSeparatorColor) }
                Text {
                    text: qsTr("Beginning of history")
                    color: root.themeValue("mutedTextColor", root.fallbackMutedTextColor)
                    font.pixelSize: 12
                }
                Rectangle { width: 70; height: 1; color: root.themeValue("separatorColor", root.fallbackSeparatorColor) }
            }

            Row {
                anchors.centerIn: parent
                visible: root.chatViewModel && root.chatViewModel.historyError.length > 0 &&
                         !root.chatViewModel.loadingInitial && !root.chatViewModel.loadingOlder &&
                         !root.chatViewModel.loadingNewer
                spacing: 8

                Text {
                    width: Math.min(implicitWidth, Math.max(80, timeline.width - retryHistoryButton.width - 28))
                    text: root.chatViewModel ? root.chatViewModel.historyError : ""
                    color: root.themeValue("messagePopupWarningColor", root.darkSurface ? "#f1b86a" : "#b45309")
                    elide: Text.ElideRight
                    font.pixelSize: 12
                }

                Button {
                    id: retryHistoryButton
                    text: qsTr("Retry")
                    onClicked: if (root.chatViewModel) root.chatViewModel.retryHistory()
                }
            }
        }

        delegate: Item {
            id: delegateRoot
            required property string stableId
            required property string protocolEventType
            required property int kind
            required property var timestamp
            required property bool ownEvent
            required property string senderId
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

            Rectangle {
                anchors.fill: parent
                anchors.margins: 2
                visible: delegateRoot.stableId === root.highlightedStableId
                color: "transparent"
                border.width: 2
                border.color: root.themeValue("accentColor", root.darkSurface ? "#82c5ff" : "#1675bd")
                radius: 6
            }

            readonly property bool isFirstNewEvent: root.newEventsBelow > 0 &&
                                                    (!root.chatViewModel || !root.chatViewModel.hasNewer) &&
                                                    index === timeline.count - root.newEventsBelow
            height: (isFirstNewEvent ? newMessagesMarker.implicitHeight : 0) +
                    (rendererItem ? rendererItem.implicitHeight : 0)
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
                } else
                    root.useFallbackTheme()
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
            height: root.chatViewModel && root.chatViewModel.loadingNewer ? 40 : 8
            Text {
                anchors.centerIn: parent
                visible: root.chatViewModel && root.chatViewModel.loadingNewer
                text: qsTr("Loading newer messages…")
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

    Rectangle {
        id: newMessagesButton
        anchors.horizontalCenter: timeline.horizontalCenter
        anchors.bottom: timeline.bottom
        anchors.bottomMargin: 18
        visible: root.jumpToLatestVisible && !root.composerActive
        implicitWidth: jumpContent.implicitWidth + 28
        implicitHeight: 40
        radius: height / 2
        color: newMessagesHover.hovered
               ? jumpHoverColor : root.themeValue("jumpToLatestBackgroundColor",
                                                   root.darkSurface ? "#303944" : "#f8fbfe")
        border.width: 1
        border.color: newMessagesHover.hovered
                      ? jumpHoverTextColor : root.themeValue("jumpToLatestBorderColor",
                                                             root.darkSurface ? "#586675" : "#bed4e6")
        activeFocusOnTab: true
        z: 3
        readonly property color jumpAccentColor: root.themeValue(
                                                   "accentColor", root.darkSurface ? "#82c5ff" : "#1675bd")
        readonly property color jumpTextColor: root.themeValue("jumpToLatestTextColor",
                                                                 root.fallbackTextColor)
        readonly property color jumpHoverColor: root.themeValue("jumpToLatestHoverColor",
                                                                  root.darkSurface ? "#344b60" : "#d4e7f5")
        readonly property color jumpHoverTextColor: root.themeValue("jumpToLatestHoverTextColor",
                                                                      root.darkSurface ? "#ffffff" : "#202020")
        Accessible.name: qsTr("Jump to latest messages")
        Accessible.role: Accessible.Button

        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter
                    || event.key === Qt.Key_Space) {
                root.jumpToLatest()
                event.accepted = true
            }
        }

        Row {
            id: jumpContent
            anchors.centerIn: parent
            height: 24
            spacing: 8

            Rectangle {
                width: 24
                height: 24
                radius: width / 2
                color: newMessagesHover.hovered
                       ? Qt.rgba(newMessagesButton.jumpHoverTextColor.r,
                                 newMessagesButton.jumpHoverTextColor.g,
                                 newMessagesButton.jumpHoverTextColor.b, 0.18)
                       : Qt.rgba(newMessagesButton.jumpAccentColor.r,
                                 newMessagesButton.jumpAccentColor.g,
                                 newMessagesButton.jumpAccentColor.b, 0.20)

                Text {
                    anchors.centerIn: parent
                    text: "↓"
                    color: newMessagesHover.hovered
                           ? newMessagesButton.jumpHoverTextColor
                           : newMessagesButton.jumpAccentColor
                    font.pixelSize: 17
                    font.bold: true
                }
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Jump to latest messages")
                color: newMessagesHover.hovered
                       ? newMessagesButton.jumpHoverTextColor : newMessagesButton.jumpTextColor
                font.pixelSize: 13
                font.weight: Font.DemiBold
            }
        }

        HoverHandler {
            id: newMessagesHover
        }

        TapHandler {
            onTapped: root.jumpToLatest()
        }
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

    ReactionEmojiPickerPopup {
        id: reactionEmojiPicker
        parent: root
        emojiModel: root.chatViewModel ? root.chatViewModel.reactionEmojiModel : null
        recentEmojis: root.chatViewModel ? root.chatViewModel.recentReactionEmojis : []
        addReaction: root.addReaction
    }

    TimelineMessagePopup {
        id: timelineMessagePopup
        parent: root
        z: 30
        backgroundColor: root.themeValue("messagePopupBackgroundColor",
                                         root.darkSurface ? "#303944" : "#f8fbfe")
        textColor: root.themeValue("messagePopupTextColor", root.fallbackTextColor)
        mutedTextColor: root.themeValue("messagePopupMutedTextColor", root.fallbackMutedTextColor)
        accentColor: root.themeValue("accentColor", root.darkSurface ? "#82c5ff" : "#1675bd")
        warningColor: root.themeValue("messagePopupWarningColor", root.darkSurface ? "#f1b86a" : "#b45309")
        positionForStableId: root.timelineMessagePopupPosition
    }

    Connections {
        target: root.chatViewModel
        function onTimelineStateChanged() {
            if (!root.chatViewModel)
                return
            if (!root.chatViewModel.loadingInitial && !root.initialPositioned) {
                root.initialPositioned = true
                root.scheduleScrollToBottom(4)
            }
            if (!root.chatViewModel.loadingInitial && root.followingTail && !root.chatViewModel.hasNewer)
                root.scheduleScrollToBottom()
            if (!root.chatViewModel.loadingOlder && !root.chatViewModel.loadingNewer)
                Qt.callLater(root.restoreOlderAnchor)
        }
        function onTimelineActionsRevisionChanged() {
            root.timelineActionsCache = ({})
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
            root.requestFullReactionSelector(stableId, timeline)
        }
        function onTimelinePositionRequested(stableId) {
            Qt.callLater(function() { root.positionTimelineItem(stableId) })
        }
        function onTimelineWarningRequested(stableId, title, message) {
            timelineMessagePopup.openFor(stableId, title, message, false, "", null)
        }
    }

    Connections {
        target: root.chatViewModel ? root.chatViewModel.timeline : null
        function onDataChanged(topLeft, bottomRight, roles) {
            if (!imageViewer.visible || !root.chatViewModel)
                return
            const eventId = root.attachmentEventId(imageViewer.sourceUri)
            const row = eventId.length > 0 ? root.chatViewModel.timeline.rowForStableId(eventId) : -1
            if (row >= topLeft.row && row <= bottomRight.row)
                ++root.attachmentImageRevision
        }
        function onRowsInserted(parent, first, last) {
            if (!root.chatViewModel)
                return
            if (!root.initialPositioned && !root.chatViewModel.loadingInitial) {
                root.initialPositioned = true
                root.scheduleScrollToBottom()
            } else if (root.followingTail && !root.chatViewModel.loadingOlder &&
                       !root.chatViewModel.loadingNewer && !root.chatViewModel.hasNewer) {
                root.scheduleScrollToBottom()
            }
        }
    }
}
