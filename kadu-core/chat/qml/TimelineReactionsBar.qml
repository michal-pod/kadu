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
    property string stableId: ""
    property color textColor: "#202020"
    property color accentColor: "#4f8ecb"
    property real leftInset: 0
    property bool alignRight: false

    implicitWidth: reactionRow.implicitWidth
    implicitHeight: reactions.length > 0 ? reactionRow.implicitHeight : 0
    visible: reactions.length > 0

    function participants(reaction) {
        return reaction.senderDisplayNames && reaction.senderDisplayNames.length > 0
                ? reaction.senderDisplayNames.join(", ") : qsTr("Unknown user")
    }

    Row {
        id: reactionRow
        anchors.left: root.alignRight ? undefined : parent.left
        anchors.leftMargin: root.alignRight ? 0 : root.leftInset
        anchors.right: root.alignRight ? parent.right : undefined
        anchors.rightMargin: root.alignRight ? root.leftInset : 0
        spacing: 4

        Repeater {
            model: root.reactions

            delegate: Control {
                id: reactionChip

                required property var modelData
                readonly property bool own: modelData.own === true
                readonly property int count: modelData.senderIds ? modelData.senderIds.length : 0

                implicitWidth: reactionMetrics.advanceWidth + 14
                implicitHeight: 22
                Accessible.role: Accessible.Button
                Accessible.name: root.participants(modelData)
                ToolTip.visible: chipHover.hovered
                ToolTip.text: reactionChip.own
                              ? qsTr("Reacted by: %1\nClick to remove your reaction")
                                    .arg(root.participants(modelData))
                              : qsTr("Reacted by: %1").arg(root.participants(modelData))

                TextMetrics {
                    id: reactionMetrics
                    text: reactionChip.modelData.key
                          + (reactionChip.count > 1 ? "x" + reactionChip.count : "")
                    font.pixelSize: 13
                }

                background: Rectangle {
                    radius: height / 2
                    color: reactionChip.own
                           ? Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.18)
                           : Qt.rgba(root.textColor.r, root.textColor.g, root.textColor.b, 0.10)
                    border.width: 1
                    border.color: reactionChip.own
                                  ? Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.72)
                                  : Qt.rgba(root.textColor.r, root.textColor.g, root.textColor.b, 0.18)
                }

                contentItem: Text {
                    id: reactionLabel
                    anchors.fill: parent
                    text: reactionChip.modelData.key
                          + (reactionChip.count > 1 ? "x" + reactionChip.count : "")
                    color: root.textColor
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: 13
                }

                HoverHandler {
                    id: chipHover
                }

                TapHandler {
                    enabled: reactionChip.own && root.stableId.length > 0
                             && root.removeOwnReaction
                    onTapped: root.removeOwnReaction(root.stableId, reactionChip.modelData.key)
                }
            }
        }
    }
}
