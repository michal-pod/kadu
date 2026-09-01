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
import QtQuick.Effects

Item {
    id: root

    property var reactions: []
    property var removeOwnReaction: null
    property string stableId: ""
    property url removeReactionIconSource: ""
    property color removeReactionHoverColor: "#d4e7f5"
    property color removeReactionHoverTextColor: "#202020"
    property color textColor: "#202020"
    property color accentColor: "#4f8ecb"
    property color backgroundColor: "#e8edf3"
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
                    onTapped: removeReactionPopup.openFor(reactionChip)
                }

                Popup {
                    id: removeReactionPopup

                    parent: root
                    padding: 4
                    modal: false
                    focus: true
                    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

                    function openFor(sourceItem) {
                        const position = sourceItem.mapToItem(root, sourceItem.width / 2,
                                                              sourceItem.height)
                        x = Math.round(Math.max(0, Math.min(position.x - implicitWidth / 2,
                                                            root.width - implicitWidth)))
                        y = Math.round(Math.max(0, position.y + 4))
                        open()
                    }

                    contentItem: Rectangle {
                        id: removeReactionButton

                        implicitWidth: removeReactionContent.implicitWidth + 16
                        implicitHeight: 30
                        radius: 4
                        color: removeReactionHover.hovered
                               ? root.removeReactionHoverColor : "transparent"
                        border.width: removeReactionHover.hovered ? 1 : 0
                        border.color: root.removeReactionHoverTextColor
                        activeFocusOnTab: true
                        Accessible.role: Accessible.Button
                        Accessible.name: qsTr("Remove reaction")

                        function trigger() {
                            root.removeOwnReaction(root.stableId, reactionChip.modelData.key)
                            removeReactionPopup.close()
                        }

                        Keys.onPressed: function(event) {
                            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter
                                    || event.key === Qt.Key_Space) {
                                trigger()
                                event.accepted = true
                            }
                        }

                        Row {
                            id: removeReactionContent

                            anchors.centerIn: parent
                            spacing: 6

                            Image {
                                id: removeReactionIcon

                                width: 16
                                height: 16
                                visible: false
                                source: root.removeReactionIconSource
                                sourceSize: Qt.size(width, height)
                                fillMode: Image.PreserveAspectFit
                            }

                            MultiEffect {
                                width: 16
                                height: 16
                                visible: removeReactionIcon.status === Image.Ready
                                source: removeReactionIcon
                                colorization: 1.0
                                colorizationColor: removeReactionHover.hovered
                                                   ? root.removeReactionHoverTextColor
                                                   : root.textColor
                            }

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: qsTr("Remove reaction")
                                color: removeReactionHover.hovered
                                       ? root.removeReactionHoverTextColor : root.textColor
                                font.pixelSize: 13
                            }
                        }

                        HoverHandler {
                            id: removeReactionHover
                        }

                        TapHandler {
                            onTapped: removeReactionButton.trigger()
                        }
                    }

                    background: Rectangle {
                        radius: 5
                        color: root.backgroundColor
                        border.width: 1
                        border.color: Qt.rgba(root.textColor.r, root.textColor.g,
                                              root.textColor.b, 0.55)
                    }
                }
            }
        }
    }
}
