/*
 * %kadu copyright begin%
 * Copyright 2026 Kadu Qt6 port
 * %kadu copyright end%
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    signal emojiSelected(string emoji)
    signal emojiClicked(string emoji)

    // The host owns persistence. Keeping it outside the component lets the
    // picker be used by both the chat editor and the reaction popup.
    property var emojiModel: null
    property var recentEmojis: []
    property string theme: "light"
    property string defaultSkinTone: "neutral"
    property string selectedSkinTone: defaultSkinTone
    property bool skinTonesDisabled: false
    property bool searchDisabled: false
    property string searchPlaceholder: qsTr("Search")
    property bool showPreview: true
    property string selectedEmoji: ""
    property string selectedEmojiName: ""
    property int recentLimit: 24

    readonly property int contentMargin: 10
    readonly property int headerHeight: 40
    readonly property int navigationHeight: 40
    readonly property int previewHeight: 70
    readonly property int emojiCellSize: 40
    readonly property bool isDark: theme === "dark" || (theme === "system" && luminance(backgroundColor) < 0.5)
    readonly property color backgroundColor: theme === "light" ? "#ffffff" : theme === "dark" ? "#292929" : systemPalette.base
    readonly property color foregroundColor: theme === "light" ? "#202020" : theme === "dark" ? "#f2f2f2" : systemPalette.text
    readonly property color borderColor: theme === "light" ? "#e4e4e4" : theme === "dark" ? "#454545" : Qt.rgba(foregroundColor.r, foregroundColor.g, foregroundColor.b, 0.18)
    readonly property color searchColor: theme === "light" ? "#f7f7f8" : theme === "dark" ? "#373737" : systemPalette.alternateBase
    readonly property color highlightColor: theme === "light" ? "#d9ecff" : theme === "dark" ? "#40576d" : systemPalette.highlight
    readonly property color hoverColor: Qt.rgba(highlightColor.r, highlightColor.g, highlightColor.b, isDark ? 0.55 : 0.75)
    readonly property var skinToneOptions: [
        { "id": "neutral", "color": "#ffd225", "emoji": "👍" },
        { "id": "1f3fb", "color": "#ffdfbd", "emoji": "👍🏻" },
        { "id": "1f3fc", "color": "#e9c197", "emoji": "👍🏼" },
        { "id": "1f3fd", "color": "#c88e62", "emoji": "👍🏽" },
        { "id": "1f3fe", "color": "#a86637", "emoji": "👍🏾" },
        { "id": "1f3ff", "color": "#60463a", "emoji": "👍🏿" }
    ]

    property string activeCategory: "smileys"
    property string previewEmoji: "😊"
    property string previewEmojiName: qsTr("What's your mood?")

    implicitWidth: 350
    implicitHeight: 450
    color: backgroundColor
    border.width: 1
    border.color: borderColor
    radius: 8
    clip: true

    SystemPalette {
        id: systemPalette
        colorGroup: SystemPalette.Active
    }

    function luminance(color) {
        return 0.2126 * color.r + 0.7152 * color.g + 0.0722 * color.b
    }

    function translatedCategoryName(categoryId) {
        switch (categoryId) {
        case "recent": return qsTr("Recently used")
        case "smileys": return qsTr("Smileys & Emotion")
        case "people": return qsTr("People & Body")
        case "animals": return qsTr("Animals & Nature")
        case "food": return qsTr("Food & Drink")
        case "travel": return qsTr("Travel & Places")
        case "activities": return qsTr("Activities")
        case "objects": return qsTr("Objects")
        case "symbols": return qsTr("Symbols")
        case "flags": return qsTr("Flags")
        }
        return categoryId
    }

    function categorySymbol(categoryId) {
        switch (categoryId) {
        case "recent": return "◷"
        case "smileys": return "☺"
        case "people": return "♟"
        case "animals": return "♞"
        case "food": return "⌑"
        case "travel": return "⌁"
        case "activities": return "★"
        case "objects": return "⌘"
        case "symbols": return "♥"
        case "flags": return "⚑"
        }
        return "•"
    }

    function modelSkinTone(tone) {
        switch (tone) {
        case "neutral": return ""
        case "1f3fb": return "light"
        case "1f3fc": return "mediumLight"
        case "1f3fd": return "medium"
        case "1f3fe": return "mediumDark"
        case "1f3ff": return "dark"
        }
        return tone
    }

    function selectSkinTone(tone) {
        selectedSkinTone = tone
        if (emojiModel)
            emojiModel.skinTone = modelSkinTone(tone)
    }

    function showCategory(categoryId) {
        if (!emojiModel)
            return

        searchField.clear()
        emojiModel.searchText = ""
        activeCategory = categoryId

        Qt.callLater(function() {
            const index = categoryIndex(categoryId)
            if (index >= 0)
                categoryList.positionViewAtIndex(index, ListView.Beginning)
        })
    }

    function categoryIndex(categoryId) {
        if (!emojiModel)
            return -1
        const categories = emojiModel.categories
        for (let index = 0; index < categories.length; ++index) {
            if (categories[index].id === categoryId)
                return index
        }
        return -1
    }

    function updatePreview(emoji, name) {
        previewEmoji = emoji
        previewEmojiName = name
    }

    onRecentEmojisChanged: {
        if (emojiModel)
            emojiModel.recentEmojis = recentEmojis
    }
    onDefaultSkinToneChanged: selectSkinTone(defaultSkinTone)
    onSkinTonesDisabledChanged: {
        if (emojiModel)
            emojiModel.skinTonesDisabled = skinTonesDisabled
    }
    onEmojiModelChanged: {
        if (!emojiModel)
            return
        emojiModel.recentEmojis = recentEmojis
        emojiModel.skinTonesDisabled = skinTonesDisabled
        selectSkinTone(defaultSkinTone)
    }

    Column {
        anchors.fill: parent
        anchors.margins: root.contentMargin
        spacing: 0

        Item {
            id: header

            width: parent.width
            height: root.searchDisabled ? 0 : root.headerHeight
            visible: height > 0

            TextField {
                id: searchField

                width: root.skinTonesDisabled ? parent.width : parent.width - tonePicker.width - 10
                height: parent.height
                leftPadding: 32
                rightPadding: 32
                placeholderText: root.searchPlaceholder
                horizontalAlignment: Text.AlignHCenter
                color: root.foregroundColor
                selectByMouse: true
                Accessible.name: root.searchPlaceholder

                background: Rectangle {
                    color: root.searchColor
                    radius: 12
                    border.width: searchField.activeFocus ? 1 : 0
                    border.color: root.highlightColor
                }

                Label {
                    text: "⌕"
                    font.pixelSize: 23
                    color: root.foregroundColor
                    opacity: 0.55
                    anchors.left: parent.left
                    anchors.leftMargin: 9
                    anchors.verticalCenter: parent.verticalCenter
                }

                ToolButton {
                    id: clearSearch

                    visible: searchField.text.length > 0
                    width: 28
                    height: 28
                    text: "×"
                    font.pixelSize: 22
                    anchors.right: parent.right
                    anchors.rightMargin: 3
                    anchors.verticalCenter: parent.verticalCenter
                    onClicked: searchField.clear()
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Clear search")
                }

                onTextChanged: {
                    if (!root.emojiModel)
                        return
                    root.emojiModel.searchText = text
                    if (text.length > 0)
                        categoryList.positionViewAtBeginning()
                }
            }

            Item {
                id: tonePicker

                property bool open: false
                property int activeIndex: {
                    for (let index = 0; index < root.skinToneOptions.length; ++index) {
                        if (root.skinToneOptions[index].id === root.selectedSkinTone)
                            return index
                    }
                    return 0
                }

                width: open ? root.skinToneOptions.length * 24 + (root.skinToneOptions.length - 1) * 6 : 20
                height: 20
                visible: !root.skinTonesDisabled
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                z: 2

                Behavior on width {
                    NumberAnimation { duration: 160 }
                }

                Repeater {
                    model: root.skinToneOptions

                    delegate: ToolButton {
                        required property int index
                        required property var modelData

                        width: 20
                        height: 20
                        x: tonePicker.open ? index * 30 : 0
                        visible: tonePicker.open || index === tonePicker.activeIndex
                        z: tonePicker.open ? root.skinToneOptions.length - index : 1
                        padding: 2
                        Accessible.name: modelData.emoji

                        background: Rectangle {
                            color: "transparent"
                            radius: 5
                            border.width: tonePicker.open && index === tonePicker.activeIndex ? 1 : 0
                            border.color: root.highlightColor
                        }

                        contentItem: Rectangle {
                            color: modelData.color
                            radius: 4
                        }

                        onClicked: {
                            root.selectSkinTone(modelData.id)
                            tonePicker.open = !tonePicker.open
                        }
                    }
                }
            }
        }

        Item {
            id: navigation

            width: parent.width
            height: root.navigationHeight

            Row {
                anchors.centerIn: parent
                spacing: Math.max(1, (navigation.width - navigationRepeater.count * 30) / Math.max(1, navigationRepeater.count - 1))

                Repeater {
                    id: navigationRepeater
                    model: root.emojiModel ? root.emojiModel.categories : []

                    delegate: ToolButton {
                        id: categoryButton

                        required property var modelData

                        readonly property string categoryId: modelData.id
                        width: 30
                        height: 30
                        enabled: searchField.text.length === 0
                        text: root.categorySymbol(categoryId)
                        font.pixelSize: 18
                        opacity: enabled ? 1.0 : 0.4
                        Accessible.name: root.translatedCategoryName(categoryId)

                        background: Rectangle {
                            color: categoryButton.hovered || root.activeCategory === categoryButton.categoryId ? root.hoverColor : "transparent"
                            radius: width / 2
                        }

                        ToolTip.visible: hovered
                        ToolTip.text: root.translatedCategoryName(categoryId)
                        onClicked: root.showCategory(categoryId)
                    }
                }
            }
        }

        ListView {
            id: categoryList

            width: parent.width
            height: parent.height - header.height - navigation.height - footer.height
            clip: true
            model: root.emojiModel ? root.emojiModel.categories : []
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }

            delegate: Item {
                id: categorySection

                required property int index
                required property var modelData

                readonly property string categoryId: modelData.id
                readonly property var entries: {
                    if (!root.emojiModel)
                        return []

                    // Keep all observable model state in this binding. A search,
                    // a tone change or an updated recent list recreates this
                    // category's small presentation model.
                    const searchText = root.emojiModel.searchText
                    const skinTone = root.emojiModel.skinTone
                    const skinTonesDisabled = root.emojiModel.skinTonesDisabled
                    const recentEmojis = root.emojiModel.recentEmojis
                    return root.emojiModel.entriesForCategory(categoryId)
                }

                width: categoryList.width
                height: entries.length > 0 ? categoryContent.height : 0
                visible: entries.length > 0

                Column {
                    id: categoryContent

                    width: parent.width
                    height: categoryTitle.height + emojiGrid.height

                    Item {
                        id: categoryTitle

                        width: parent.width
                        height: 34

                        Label {
                            text: root.translatedCategoryName(categorySection.categoryId)
                            color: root.foregroundColor
                            font.bold: true
                            font.pixelSize: 15
                            anchors.left: parent.left
                            anchors.leftMargin: 2
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    Grid {
                        id: emojiGrid

                        width: parent.width
                        columns: Math.max(1, Math.floor(width / root.emojiCellSize))
                        height: childrenRect.height
                        spacing: 0

                        Repeater {
                            model: categorySection.entries

                            delegate: Item {
                                id: emojiCell

                                required property var modelData

                                readonly property string emoji: modelData.emoji
                                readonly property string name: modelData.name

                                width: root.emojiCellSize
                                height: root.emojiCellSize
                                Accessible.name: name

                                Rectangle {
                                    anchors.fill: parent
                                    color: emojiMouseArea.containsMouse ? root.hoverColor : "transparent"
                                    radius: 5
                                }

                                Text {
                                    text: emojiCell.emoji
                                    font.pixelSize: 24
                                    font.preferShaping: true
                                    elide: Text.ElideNone
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    anchors.fill: parent
                                }

                                MouseArea {
                                    id: emojiMouseArea

                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onEntered: {
                                        root.updatePreview(emoji, name)
                                    }
                                    onClicked: {
                                        root.selectedEmoji = emoji
                                        root.selectedEmojiName = name
                                        root.updatePreview(emoji, name)
                                        root.emojiSelected(emoji)
                                        root.emojiClicked(emoji)
                                    }
                                }

                                ToolTip.visible: emojiMouseArea.containsMouse
                                ToolTip.text: name
                            }
                        }
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                visible: categoryList.count > 0 && categoryList.contentHeight === 0
                text: searchField.text.length > 0 ? qsTr("No emoji found") : qsTr("No recently used emoji")
                color: root.foregroundColor
                opacity: 0.7
            }

            onContentYChanged: {
                if (searchField.text.length > 0)
                    return
                const index = indexAt(1, contentY + 1)
                if (index >= 0 && root.emojiModel)
                    root.activeCategory = root.emojiModel.categories[index].id
            }
        }

        Rectangle {
            id: footer

            width: parent.width
            height: root.showPreview ? root.previewHeight : 0
            visible: height > 0
            color: "transparent"

            Rectangle {
                width: parent.width
                height: 1
                color: root.borderColor
                anchors.top: parent.top
            }

            Behavior on height {
                NumberAnimation { duration: 180 }
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: root.contentMargin
                anchors.rightMargin: root.contentMargin
                spacing: 10

                Label {
                    text: root.previewEmoji
                    font.pixelSize: 38
                    font.preferShaping: true
                    Layout.alignment: Qt.AlignVCenter
                }

                Label {
                    text: root.previewEmojiName
                    color: root.foregroundColor
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                }
            }
        }
    }
}
