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

    property var actions: []
    property var executeAction: null
    property color fallbackTextColor: "#202020"
    property bool alignRight: true
    property bool shown: true

    implicitWidth: actionRow.implicitWidth
    implicitHeight: actionRow.implicitHeight
    visible: actions.length > 0
    opacity: shown ? 1.0 : 0.0
    enabled: shown

    Behavior on opacity {
        NumberAnimation { duration: 100 }
    }

    function fallbackSymbol(action) {
        if (action.key === "copy")
            return "⧉"
        if (action.key === "reply")
            return "↩"
        if (action.key === "edit")
            return "✎"
        if (action.key === "delete")
            return "⌫"
        if (action.key === "saveAttachment")
            return "⇩"
        if (action.key === "showSource")
            return "{}"
        if (action.key === "pin")
            return "⌖"
        if (action.key === "unpin")
            return "⊘"
        return "⋯"
    }

    Row {
        id: actionRow
        anchors.right: root.alignRight ? parent.right : undefined
        anchors.left: root.alignRight ? undefined : parent.left
        spacing: 2

        Repeater {
            model: root.actions

            delegate: ToolButton {
                id: actionButton

                required property var modelData
                readonly property string iconName: modelData.iconName || ""

                implicitWidth: 24
                implicitHeight: 24
                padding: 4
                enabled: modelData.enabled === undefined || modelData.enabled
                Accessible.name: modelData.text || ""
                ToolTip.visible: hovered
                ToolTip.text: modelData.text || ""

                contentItem: Item {
                    implicitWidth: 16
                    implicitHeight: 16

                    Image {
                        id: actionIcon
                        anchors.centerIn: parent
                        width: 16
                        height: 16
                        source: actionButton.iconName.length > 0
                                ? "image://kaduicon/" + encodeURIComponent(actionButton.iconName) : ""
                        sourceSize.width: 16
                        sourceSize.height: 16
                        visible: status === Image.Ready && source.length > 0
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: !actionIcon.visible
                        text: root.fallbackSymbol(actionButton.modelData)
                        color: root.fallbackTextColor
                        font.pixelSize: 14
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                background: Rectangle {
                    radius: 3
                    color: actionButton.hovered ? Qt.rgba(root.fallbackTextColor.r, root.fallbackTextColor.g,
                                                          root.fallbackTextColor.b, 0.14) : "transparent"
                }

                onClicked: {
                    if (root.executeAction)
                        root.executeAction(modelData.id)
                }
            }
        }
    }
}
