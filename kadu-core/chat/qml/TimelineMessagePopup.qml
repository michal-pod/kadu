/*
 * %kadu copyright begin%
 * Copyright 2026 Kadu Qt6 port
 * %kadu copyright end%
 */

import QtQuick
import QtQuick.Controls

Popup {
    id: root

    property string title: ""
    property string message: ""
    property string actionText: ""
    property bool confirmation: false
    property color backgroundColor: "#f8fbfe"
    property color textColor: "#202020"
    property color mutedTextColor: "#666666"
    property color accentColor: "#1675bd"
    property color warningColor: "#b45309"
    property var acceptedAction: null
    property var positionForStableId: null
    property string stableId: ""

    padding: 0
    modal: false
    focus: confirmation
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    function openFor(eventId, popupTitle, popupMessage, confirm, confirmText, action) {
        stableId = eventId
        title = popupTitle
        message = popupMessage
        confirmation = confirm
        actionText = confirmText
        acceptedAction = action
        open()
        Qt.callLater(reposition)
    }

    function reposition() {
        if (!visible)
            return

        const position = positionForStableId ? positionForStableId(stableId, implicitWidth, implicitHeight) : null
        if (!position)
            return

        x = position.x
        y = position.y
    }

    onOpened: {
        if (confirmation)
            warningTimer.stop()
        else
            warningTimer.restart()
    }

    onClosed: {
        warningTimer.stop()
        acceptedAction = null
        stableId = ""
    }

    Timer {
        id: warningTimer
        interval: 5000
        onTriggered: root.close()
    }

    background: Rectangle {
        color: root.backgroundColor
        radius: 7
        border.width: 1
        border.color: Qt.rgba(root.confirmation ? root.accentColor.r : root.warningColor.r,
                             root.confirmation ? root.accentColor.g : root.warningColor.g,
                             root.confirmation ? root.accentColor.b : root.warningColor.b, 0.52)
    }

    contentItem: Item {
        implicitWidth: 300
        implicitHeight: contentColumn.implicitHeight + 24

        Column {
            id: contentColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 12
            spacing: 5

            Text {
                width: parent.width
                text: root.title
                color: root.textColor
                font.pixelSize: 13
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
            }

            Text {
                width: parent.width
                text: root.message
                color: root.mutedTextColor
                font.pixelSize: 12
                wrapMode: Text.Wrap
            }

            Row {
                visible: root.confirmation
                width: parent.width
                height: visible ? 30 : 0
                spacing: 8

                Item { width: Math.max(0, parent.width - cancelButton.implicitWidth - confirmButton.implicitWidth - parent.spacing) }

                Button {
                    id: cancelButton
                    text: qsTr("Cancel")
                    flat: true
                    onClicked: root.close()
                }

                Button {
                    id: confirmButton
                    text: root.actionText
                    onClicked: {
                        const action = root.acceptedAction
                        root.close()
                        if (action)
                            action()
                    }
                }
            }
        }
    }
}
