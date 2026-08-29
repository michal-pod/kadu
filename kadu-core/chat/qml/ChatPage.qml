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
    readonly property var activeTheme: themeLoader.item
    property bool initialPositioned: false
    property bool followingTail: true
    property bool scrollBarVisible: false
    property string olderAnchorId: ""
    property real olderAnchorOffset: 0

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
        item.plainText = Qt.binding(function() { return delegate.plainText })
        item.formattedText = Qt.binding(function() { return delegate.formattedText })
        if (item.replyToId !== undefined)
            item.replyToId = Qt.binding(function() { return delegate.replyToId })
        if (item.reply !== undefined)
            item.reply = Qt.binding(function() { return delegate.reply })
        if (item.attachments !== undefined)
            item.attachments = Qt.binding(function() { return delegate.attachments })
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
        if (item.timelineActions !== undefined)
            item.timelineActions = root.timelineActions
        if (item.executeTimelineAction !== undefined)
            item.executeTimelineAction = root.executeTimelineAction
        if (item.copyText !== undefined)
            item.copyText = root.copyText
    }

    function atBottom() {
        const maximum = Math.max(timeline.originY, timeline.contentHeight - timeline.height + timeline.originY)
        return timeline.contentY >= maximum - 8
    }

    function scrollToBottom() {
        timeline.positionViewAtEnd()
        followingTail = true
    }

    function scrollPage(direction) {
        const maximum = Math.max(timeline.originY, timeline.contentHeight - timeline.height + timeline.originY)
        const pageSize = Math.max(1, timeline.height - 24)
        timeline.contentY = Math.max(timeline.originY, Math.min(maximum, timeline.contentY + direction * pageSize))
        followingTail = root.atBottom()
        revealScrollBar()
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

    Rectangle {
        anchors.fill: parent
        color: root.themeValue("backgroundColor", root.fallbackBackgroundColor)
    }

    Rectangle {
        id: roomHeader
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: visible ? Math.max(56, roomHeaderContent.implicitHeight + 16) : 0
        visible: root.chatViewModel && root.chatViewModel.roomInfoVisible
        color: root.themeValue("roomHeaderBackgroundColor", root.darkSurface ? "#2d323a" : "#f4f6f8")
        border.width: 1
        border.color: root.themeValue("separatorColor", root.fallbackSeparatorColor)
        clip: true

        Row {
            id: roomHeaderContent
            x: 8
            y: 8
            width: parent.width - 16
            spacing: 10

            Item {
                id: roomAvatar
                width: 40
                height: width
                implicitHeight: height

                Rectangle {
                    anchors.fill: parent
                    color: root.darkSurface ? "#4b86c5" : "#5a8bbd"
                    visible: !roomAvatarImage.visible
                }

                Text {
                    anchors.centerIn: parent
                    text: root.chatViewModel && root.chatViewModel.roomName.length > 0
                          ? root.chatViewModel.roomName.slice(0, 1).toUpperCase()
                          : "#"
                    color: "#ffffff"
                    font.bold: true
                    font.pixelSize: 19
                    visible: !roomAvatarImage.visible
                }

                Image {
                    id: roomAvatarImage
                    anchors.fill: parent
                    source: root.chatViewModel ? root.chatViewModel.roomAvatarSource : ""
                    fillMode: Image.PreserveAspectCrop
                    visible: status === Image.Ready
                }
            }

            Column {
                width: parent.width - roomAvatar.width - parent.spacing
                spacing: 2

                Text {
                    width: parent.width
                    text: root.chatViewModel ? root.chatViewModel.roomName : ""
                    color: root.themeValue("textColor", root.fallbackTextColor)
                    elide: Text.ElideRight
                    font.bold: true
                    font.pixelSize: 14
                }

                Text {
                    visible: text.length > 0
                    width: parent.width
                    text: root.chatViewModel ? root.chatViewModel.roomDescription : ""
                    color: root.themeValue("textColor", root.fallbackTextColor)
                    opacity: 0.70
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    font.pixelSize: 12
                }
            }
        }
    }

    ListView {
        id: timeline
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: roomHeader.bottom
        anchors.bottom: parent.bottom
        anchors.margins: root.themeValue("timelineMargin", 16)
        clip: true
        spacing: root.themeValue("timelineSpacing", 4)
        model: root.chatViewModel ? root.chatViewModel.timeline : null
        reuseItems: true
        boundsBehavior: Flickable.StopAtBounds
        focus: true

        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_PageUp) {
                root.scrollPage(-1)
                event.accepted = true
            } else if (event.key === Qt.Key_PageDown) {
                root.scrollPage(1)
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

            height: rendererItem ? rendererItem.implicitHeight : 0

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
                    "errorText": delegateRoot.errorText
                })
                if (rendererItem)
                    root.bindTimelineItem(rendererItem, delegateRoot)
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

    Text {
        anchors.centerIn: parent
        visible: root.chatViewModel && root.chatViewModel.loadingInitial
        text: qsTr("Loading messages…")
        color: root.themeValue("loadingTextColor", root.fallbackTextColor)
        z: 2
    }

    ImageViewerDialog {
        id: imageViewer
        parentItem: root
    }

    Connections {
        target: root.chatViewModel
        function onTimelineStateChanged() {
            if (!root.chatViewModel)
                return
            if (!root.chatViewModel.loadingInitial && !root.initialPositioned) {
                root.initialPositioned = true
                Qt.callLater(root.scrollToBottom)
            }
            if (!root.chatViewModel.loadingOlder)
                Qt.callLater(root.restoreOlderAnchor)
        }
    }

    Connections {
        target: root.chatViewModel ? root.chatViewModel.timeline : null
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
