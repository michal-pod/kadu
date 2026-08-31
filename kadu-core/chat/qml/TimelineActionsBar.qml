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
    property color backgroundColor: "#f4f6f8"
    property bool alignRight: true
    property bool shown: true

    signal reactionRequested(var sourceItem)

    // The host places this zero-sized anchor over an entry. The actual bar is
    // deliberately painted above it, so showing actions never changes a
    // timeline delegate's height or moves the ListView viewport.
    implicitWidth: 0
    implicitHeight: 0
    width: 0
    height: 0
    visible: actions.length > 0
    z: 20

    function fallbackSymbol(action) {
        if (action.key === "copy")
            return "⧉"
        if (action.key === "react")
            return "☺"
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

    Rectangle {
        id: actionPopup

        x: root.alignRight ? -width : 0
        y: 0
        implicitWidth: actionRow.implicitWidth + 8
        implicitHeight: actionRow.implicitHeight + 8
        radius: 5
        color: root.backgroundColor
        border.width: 1
        border.color: Qt.rgba(root.fallbackTextColor.r, root.fallbackTextColor.g,
                              root.fallbackTextColor.b, 0.20)
        visible: root.shown || popupHover.hovered
        opacity: visible ? 1.0 : 0.0

        Behavior on opacity {
            NumberAnimation { duration: 100 }
        }

        HoverHandler {
            id: popupHover
        }

        Row {
            id: actionRow
            anchors.centerIn: parent
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
                            // A missing icon is represented by the provider as a transparent
                            // 1x1 image, which still has Ready status. Keep the textual fallback
                            // visible in that case.
                            visible: status === Image.Ready && source.toString().length > 0 && sourceSize.width > 1
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
                        if (modelData.key === "react") {
                            root.reactionRequested(actionButton)
                            return
                        }
                        if (root.executeAction)
                            root.executeAction(modelData.id)
                    }
                }
            }
        }
    }
}
