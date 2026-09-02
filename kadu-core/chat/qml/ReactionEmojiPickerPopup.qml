/*
 * %kadu copyright begin%
 * Copyright 2026 Kadu Qt6 port
 * %kadu copyright end%
 */

import QtQuick
import QtQuick.Controls

Popup {
    id: root

    property string stableId: ""
    property var emojiModel: null
    property var recentEmojis: []
    property var addReaction: null

    padding: 0
    modal: false
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    function openFor(sourceItem) {
        const point = sourceItem.mapToItem(parent, 0, sourceItem.height)
        const preferredY = point.y + 4
        x = Math.max(0, Math.min(point.x, parent.width - implicitWidth))
        y = preferredY + implicitHeight <= parent.height
           ? preferredY : Math.max(0, point.y - implicitHeight - 4)
        open()
    }

    contentItem: EmojiPicker {
        emojiModel: root.emojiModel
        recentEmojis: root.recentEmojis
        theme: "system"

        onEmojiSelected: function(emoji) {
            if (root.addReaction)
                root.addReaction(root.stableId, emoji)
            root.close()
        }
    }

    onClosed: stableId = ""
}
