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

Item {
    id: root

    property var reactions: []
    property var removeOwnReaction: null
    property var addReaction: null
    property bool showAddButton: true
    property color textColor: "#202020"
    property color accentColor: "#4f8ecb"
    property color backgroundColor: "#e8edf3"
    property bool shown: true

    implicitWidth: reactionRow.implicitWidth
    implicitHeight: reactionRow.implicitHeight
    // The row stays in the layout even when it is faded out. Otherwise moving
    // the pointer over an entry would change the ListView's scroll position.
    visible: reactions.length > 0 || showAddButton
    opacity: shown ? 1.0 : 0.0
    enabled: shown

    Behavior on opacity {
        NumberAnimation { duration: 100 }
    }

    function participants(reaction) {
        return reaction.senderDisplayNames && reaction.senderDisplayNames.length > 0
                ? reaction.senderDisplayNames.join(", ") : qsTr("Unknown user")
    }

    Row {
        id: reactionRow
        anchors.right: parent.right
        spacing: 4

        Repeater {
            model: root.reactions

            delegate: Control {
                id: reactionChip

                required property var modelData
                readonly property bool own: modelData.own === true
                readonly property int count: modelData.senderIds ? modelData.senderIds.length : 0

                implicitWidth: reactionLabel.implicitWidth + 14
                implicitHeight: 24
                Accessible.role: Accessible.Button
                Accessible.name: root.participants(modelData)
                ToolTip.visible: hoverHandler.hovered
                ToolTip.text: own
                              ? qsTr("Reacted by: %1\nHold to remove your reaction.").arg(root.participants(modelData))
                              : qsTr("Reacted by: %1").arg(root.participants(modelData))

                background: Rectangle {
                    radius: 10
                    color: root.backgroundColor
                    border.width: reactionChip.own ? 1 : 0
                    border.color: root.accentColor
                    opacity: reactionChip.own ? 1.0 : 0.82
                }

                contentItem: Text {
                    id: reactionLabel
                    leftPadding: 7
                    rightPadding: 7
                    text: reactionChip.modelData.key + " " + reactionChip.count
                    color: root.textColor
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: 13
                }

                HoverHandler {
                    id: hoverHandler
                }

                TapHandler {
                    enabled: reactionChip.own && root.removeOwnReaction !== null
                    longPressThreshold: 650
                    onLongPressed: root.removeOwnReaction(reactionChip.modelData.key)
                }
            }
        }

        ToolButton {
            id: addReactionButton

            visible: root.showAddButton
            implicitWidth: 24
            implicitHeight: 24
            text: "+"
            font.pixelSize: 18
            Accessible.name: qsTr("Add reaction")
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Add reaction")
            onClicked: {
                if (root.addReaction)
                    root.addReaction()
            }
        }
    }
}
