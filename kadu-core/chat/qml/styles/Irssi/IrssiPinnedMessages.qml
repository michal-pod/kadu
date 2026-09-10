/*
 * %kadu copyright begin%
 * Copyright 2026 Kadu Qt6 port
 * %kadu copyright end%
 */

import QtQuick
import QtQuick.Controls

Item {
    id: root

    property var pinnedMessages: []
    property var timelineActions: null
    property var executeTimelineAction: null
    property var jumpToTimelineItem: null
    property var popupOpened: null
    property var popupClosed: null
    property string terminalFont: "monospace"
    property int actionsRevision: 0
    readonly property var entries: pinnedMessages || []

    visible: entries.length > 0
    implicitHeight: 0

    onEntriesChanged: {
        if (entries.length === 0)
            pinnedMessagesPopup.close()
    }

    function showPinnedMessages() {
        if (entries.length > 0)
            pinnedMessagesPopup.open()
    }

    function togglePinnedMessages() {
        if (pinnedMessagesPopup.opened)
            pinnedMessagesPopup.close()
        else
            showPinnedMessages()
    }

    function actionsFor(stableId) {
        return timelineActions ? timelineActions(stableId) : []
    }

    function actionSymbol(action) {
        switch (action.key) {
        case "copy": return "⧉"
        case "react": return "☺"
        case "reply": return "↩"
        case "edit": return "✎"
        case "saveAttachment": return "↓"
        case "delete": return "×"
        case "showSource": return "?"
        case "pin": return "⌖"
        case "unpin": return "⌫"
        default: return "•"
        }
    }

    function encryptionEmoji(entry) {
        if (!entry.encrypted || Number(entry.decryptionState) === 0)
            return ""
        if (Number(entry.decryptionState) === 1)
            return "⌛"
        if (Number(entry.decryptionState) === 2)
            return "🔒"
        if (Number(entry.decryptionState) === 4)
            return "🔐"
        return "⚠"
    }

    Popup {
        id: pinnedMessagesPopup
        parent: root
        x: Math.max(4, (root.width - width) / 2)
        y: 24
        width: Math.max(360, Math.min(root.width - 8, 720))
        height: Math.min(460, content.implicitHeight + topPadding + bottomPadding)
        padding: 8
        modal: true
        dim: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onOpened: {
            if (root.popupOpened)
                root.popupOpened()
        }
        onClosed: {
            pinnedMessagesList.cancelFlick()
            if (root.popupClosed)
                root.popupClosed()
        }

        background: Rectangle {
            color: "#000000"
            border.width: 1
            border.color: "#0000ff"
        }

        contentItem: Column {
            id: content
            width: pinnedMessagesPopup.availableWidth
            spacing: 5

            Text {
                width: parent.width
                text: "[" + qsTr("pinned messages") + "]"
                color: "#ffffff"
                font.family: root.terminalFont
                font.pointSize: 10
                font.bold: true
            }

            ListView {
                id: pinnedMessagesList
                width: parent.width
                height: Math.min(contentHeight, 390)
                clip: true
                spacing: 3
                model: root.entries

                delegate: Column {
                    id: pinnedMessageDelegate

                    required property var modelData
                    readonly property var entry: modelData
                    property var entryActions: []

                    width: pinnedMessagesList.width
                    spacing: 1

                    function refreshActions() {
                        entryActions = root.actionsFor(entry.stableId)
                    }

                    Component.onCompleted: refreshActions()

                    Connections {
                        target: root

                        function onActionsRevisionChanged() {
                            pinnedMessageDelegate.refreshActions()
                        }
                    }

                    Text {
                        width: parent.width
                        text: "[" + Qt.formatTime(entry.timestamp || new Date(), "HH:mm:ss") + "] " +
                              (root.encryptionEmoji(entry).length > 0 ? root.encryptionEmoji(entry) + " " : "") +
                              "<" + (entry.senderDisplayName || qsTr("unknown")) + "> " +
                              (entry.redacted ? qsTr("message removed") :
                               (entry.plainText || qsTr("This pinned message is not loaded yet.")))
                        color: entry.redacted ? "#8e8e8e" : "#d9d9d9"
                        wrapMode: Text.Wrap
                        font.family: root.terminalFont
                        font.pointSize: 10
                    }

                    Row {
                        visible: (entry.stableId || "").length > 0 || pinnedMessageDelegate.entryActions.length > 0
                        spacing: 7

                        Item {
                            width: 15
                            height: 15
                            visible: (entry.stableId || "").length > 0

                            Text {
                                anchors.centerIn: parent
                                text: "↪"
                                color: "#d9d9d9"
                                font.family: root.terminalFont
                                font.pointSize: 10
                            }

                            MouseArea {
                                id: goToMessageArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    pinnedMessagesPopup.close()
                                    if (root.jumpToTimelineItem)
                                        root.jumpToTimelineItem(entry.stableId)
                                }
                            }

                            ToolTip.visible: goToMessageArea.containsMouse
                            ToolTip.text: qsTr("Go to message")
                            ToolTip.delay: 350
                        }

                        Repeater {
                            model: pinnedMessageDelegate.entryActions

                            delegate: Item {
                                required property var modelData

                                width: 15
                                height: 15
                                visible: modelData.enabled === undefined || modelData.enabled

                                Text {
                                    anchors.centerIn: parent
                                    text: root.actionSymbol(modelData)
                                    color: "#d9d9d9"
                                    font.family: root.terminalFont
                                    font.pointSize: 10
                                }

                                MouseArea {
                                    id: actionArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (root.executeTimelineAction)
                                            root.executeTimelineAction(entry.stableId, modelData.id)
                                        pinnedMessagesPopup.close()
                                    }
                                }

                                ToolTip.visible: actionArea.containsMouse
                                ToolTip.text: modelData.text || ""
                                ToolTip.delay: 350
                            }
                        }
                    }
                }
            }
        }
    }
}
