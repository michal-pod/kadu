/*
 * %kadu copyright begin%
 * Copyright 2026 Kadu Qt6 port
 * %kadu copyright end%
 */

import QtQuick

Item {
    id: root

    property string chatTitle: ""
    property string ownDisplayName: ""
    property var roomInfo: ({})
    property var pinnedMessages: []
    property var showPinnedMessages: null
    property string terminalFont: "monospace"
    property date now: new Date()

    implicitHeight: 22

    function roomLabel() {
        let label = root.chatTitle
        if (root.roomInfo && root.roomInfo.ircModes)
            label += " " + root.roomInfo.ircModes
        if (root.roomInfo && root.roomInfo.historyVisibility && root.roomInfo.historyVisibility !== "shared")
            label += " history=" + root.roomInfo.historyVisibility
        return label
    }

    Timer {
        interval: 1000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: root.now = new Date()
    }

    Rectangle {
        anchors.fill: parent
        color: "#0000b8"
    }

    Row {
        anchors.left: parent.left
        anchors.leftMargin: 5
        anchors.right: parent.right
        anchors.rightMargin: 5
        anchors.verticalCenter: parent.verticalCenter
        spacing: 7

        Text {
            id: timestampText
            text: "[" + Qt.formatTime(root.now, "HH:mm:ss") + "]"
            color: "#ffffff"
            font.family: root.terminalFont
            font.pointSize: 10
        }
        Text {
            id: ownNameText
            text: "[" + (root.ownDisplayName || qsTr("You")) +
                  (root.roomInfo && root.roomInfo.memberPrefix ? " " + root.roomInfo.memberPrefix : "") + "]"
            color: "#ffffff"
            font.family: root.terminalFont
            font.pointSize: 10
        }
        Text {
            id: chatTitleText
            width: Math.max(0, root.width - timestampText.implicitWidth - ownNameText.implicitWidth -
                            (pinnedMessagesText.visible ? pinnedMessagesText.implicitWidth : 0) -
                            3 * parent.spacing - 10)
            text: "[" + root.roomLabel() + "]"
            color: "#ffffff"
            elide: Text.ElideRight
            font.family: root.terminalFont
            font.pointSize: 10
        }
        Text {
            id: pinnedMessagesText
            visible: root.pinnedMessages && root.pinnedMessages.length > 0
            text: "[📌 " + qsTr("pinned messages") + "]"
            color: "#ffffff"
            font.family: root.terminalFont
            font.pointSize: 10

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (root.showPinnedMessages)
                        root.showPinnedMessages()
                }
            }
        }
    }
}
