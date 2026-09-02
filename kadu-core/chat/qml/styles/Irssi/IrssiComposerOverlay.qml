/*
 * %kadu copyright begin%
 * Copyright 2026 Kadu Qt6 port
 * %kadu copyright end%
 */

import QtQuick
import QtQuick.Controls

Item {
    id: root

    property var context: ({})
    property var cancelComposerContext: null
    property string terminalFont: "monospace"
    readonly property var target: context && context.target ? context.target : ({})

    implicitHeight: card.implicitHeight

    Rectangle {
        id: card
        anchors.centerIn: parent
        width: Math.min(parent.width - 32, 560)
        implicitHeight: details.implicitHeight + 18
        color: "#000000"
        border.width: 1
        border.color: "#0000ff"

        Column {
            id: details
            x: 10
            y: 9
            width: parent.width - 20
            spacing: 4

            Text {
                width: parent.width
                text: root.context.mode === "edit" ? "-!- " + qsTr("editing message")
                                                      : "-!- " + qsTr("replying to") + " <" + (root.target.senderDisplayName || qsTr("unknown")) + ">"
                color: "#ffffff"
                font.family: root.terminalFont
                font.pointSize: 10
            }

            Text {
                width: parent.width
                text: root.target.redacted ? qsTr("message removed") : (root.target.plainText || "")
                color: "#b4b4b4"
                wrapMode: Text.Wrap
                maximumLineCount: 3
                elide: Text.ElideRight
                font.family: root.terminalFont
                font.pointSize: 10
            }

            Button {
                text: qsTr("Cancel")
                onClicked: {
                    if (root.cancelComposerContext)
                        root.cancelComposerContext()
                }
            }
        }
    }
}
