import QtQuick

Item {
    id: root

    property var chatViewModel: _chatViewModel
    // A configuration preview supplies this property while the real chat keeps
    // it empty and follows the globally selected theme from ChatViewModel.
    property string themeOverride: ""
    readonly property string activeTheme: themeOverride.length > 0
                                         ? themeOverride
                                         : (chatViewModel ? chatViewModel.theme : "KaduClassic")
    property bool initialPositioned: false
    property bool followingTail: true
    property string olderAnchorId: ""
    property real olderAnchorOffset: 0

    function atBottom() {
        const maximum = Math.max(timeline.originY, timeline.contentHeight - timeline.height + timeline.originY)
        return timeline.contentY >= maximum - 8
    }

    function scrollToBottom() {
        timeline.positionViewAtEnd()
        followingTail = true
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

    Rectangle {
        anchors.fill: parent
        color: root.activeTheme === "Bubbles" ? "#20242b" : "#f7f7f7"
    }

    ListView {
        id: timeline
        anchors.fill: parent
        anchors.margins: root.activeTheme === "Bubbles" ? 12 : 16
        clip: true
        spacing: 4
        model: root.chatViewModel ? root.chatViewModel.timeline : null
        reuseItems: true
        boundsBehavior: Flickable.StopAtBounds

        onContentYChanged: {
            followingTail = root.atBottom()
            if (contentY <= originY + 64)
                root.requestOlder()
        }
        onAtYBeginningChanged: if (atYBeginning) root.requestOlder()

        header: Item {
            width: timeline.width
            height: 44

            Row {
                anchors.centerIn: parent
                visible: root.chatViewModel && !root.chatViewModel.hasOlder && !root.chatViewModel.loadingInitial
                spacing: 8
                Rectangle { width: 70; height: 1; color: "#858585" }
                Text {
                    text: qsTr("Beginning of history")
                    color: root.activeTheme === "Bubbles" ? "#b6c0cf" : "#666666"
                    font.pixelSize: 12
                }
                Rectangle { width: 70; height: 1; color: "#858585" }
            }
        }

        delegate: Item {
            id: delegateRoot
            required property string stableId
            required property int kind
            required property var timestamp
            required property bool ownEvent
            required property string senderDisplayName
            required property string plainText
            required property string formattedText
            required property bool showSender
            required property bool showTimestamp
            required property bool startsNewDay
            required property int deliveryState
            required property bool redacted
            required property bool encrypted
            required property int decryptionState
            required property string errorText

            width: timeline.width
            height: renderer.item ? renderer.item.implicitHeight : 0

            Loader {
                id: renderer
                anchors.left: parent.left
                anchors.right: parent.right
                sourceComponent: root.activeTheme === "Bubbles" ? bubblesTheme : classicTheme
            }

            Component {
                id: bubblesTheme
                BubblesTimelineItem {
                    width: delegateRoot.width
                    stableId: delegateRoot.stableId
                    kind: delegateRoot.kind
                    timestamp: delegateRoot.timestamp
                    ownEvent: delegateRoot.ownEvent
                    senderDisplayName: delegateRoot.senderDisplayName
                    plainText: delegateRoot.plainText
                    formattedText: delegateRoot.formattedText
                    showSender: delegateRoot.showSender
                    showTimestamp: delegateRoot.showTimestamp
                    startsNewDay: delegateRoot.startsNewDay
                    deliveryState: delegateRoot.deliveryState
                    redacted: delegateRoot.redacted
                    encrypted: delegateRoot.encrypted
                    decryptionState: delegateRoot.decryptionState
                    errorText: delegateRoot.errorText
                }
            }

            Component {
                id: classicTheme
                KaduClassicTimelineItem {
                    width: delegateRoot.width
                    stableId: delegateRoot.stableId
                    kind: delegateRoot.kind
                    timestamp: delegateRoot.timestamp
                    ownEvent: delegateRoot.ownEvent
                    senderDisplayName: delegateRoot.senderDisplayName
                    plainText: delegateRoot.plainText
                    formattedText: delegateRoot.formattedText
                    showSender: delegateRoot.showSender
                    showTimestamp: delegateRoot.showTimestamp
                    startsNewDay: delegateRoot.startsNewDay
                    deliveryState: delegateRoot.deliveryState
                    redacted: delegateRoot.redacted
                    encrypted: delegateRoot.encrypted
                    decryptionState: delegateRoot.decryptionState
                    errorText: delegateRoot.errorText
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
                color: root.activeTheme === "Bubbles" ? "#b6c0cf" : "#666666"
            }
        }
    }

    Text {
        anchors.centerIn: parent
        visible: root.chatViewModel && root.chatViewModel.loadingInitial
        text: qsTr("Loading messages…")
        color: root.activeTheme === "Bubbles" ? "#f2f4f8" : "#202020"
        z: 2
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
            } else if (root.followingTail && first >= timeline.count - (last - first + 1)) {
                Qt.callLater(root.scrollToBottom)
            }
        }
    }
}
