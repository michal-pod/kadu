/*
 * %kadu copyright begin%
 * Copyright 2026 Kadu Qt6 port
 * %kadu copyright end%
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

import QtQuick
import QtQuick.Controls

Popup {
    id: root

    property string stableId: ""
    property var emojiProvider: null
    property var emojis: []
    property var addReaction: null
    property var requestFullSelector: null
    property color textColor: "#202020"
    property color backgroundColor: "#f4f6f8"

    padding: 6
    modal: false
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

    function openFor(sourceItem) {
        emojis = emojiProvider ? emojiProvider() : []
        const point = sourceItem.mapToItem(parent, 0, sourceItem.height)
        x = Math.max(0, Math.min(point.x, parent.width - implicitWidth))
        y = Math.max(0, Math.min(point.y + 4, parent.height - implicitHeight))
        open()
    }

    background: Rectangle {
        radius: 6
        color: root.backgroundColor
        border.width: 1
        border.color: Qt.rgba(root.textColor.r, root.textColor.g, root.textColor.b, 0.20)
    }

    contentItem: Row {
        spacing: 3

        Repeater {
            model: root.emojis

            delegate: ToolButton {
                required property string modelData

                implicitWidth: 30
                implicitHeight: 30
                ToolTip.visible: hovered
                ToolTip.text: modelData

                contentItem: Text {
                    text: modelData
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: 17
                }
                onClicked: {
                    if (root.addReaction)
                        root.addReaction(root.stableId, modelData)
                    root.close()
                }
            }
        }

        ToolButton {
            implicitWidth: 30
            implicitHeight: 30
            ToolTip.visible: hovered
            ToolTip.text: qsTr("More reactions")

            contentItem: Text {
                text: "…"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 18
            }
            onClicked: {
                if (root.requestFullSelector)
                    root.requestFullSelector()
                root.close()
            }
        }
    }
}
