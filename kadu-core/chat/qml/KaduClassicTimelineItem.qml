import QtQuick

Item {
    id: root

    required property string stableId
    required property int kind
    required property var timestamp
    required property bool ownEvent
    required property string senderDisplayName
    required property string plainText
    required property string formattedText
    required property bool showSender
    required property bool showTimestamp
    required property bool startsNewDay
    required property int deliveryState
    required property bool redacted
    required property bool encrypted
    required property int decryptionState
    required property string errorText

    implicitHeight: content.implicitHeight

    function systemEvent() { return kind >= 7 }
    function messageText() {
        if (formattedText.length > 0)
            return formattedText
        return plainText.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/\n/g, "<br/>")
    }
    function deliveryText() {
        if (deliveryState === 1)
            return qsTr("sending…")
        if (deliveryState === 3)
            return qsTr("delivered")
        if (deliveryState === 4)
            return qsTr("failed")
        return ""
    }

    Column {
        id: content
        width: parent.width
        spacing: 3

        Text {
            visible: root.startsNewDay
            anchors.horizontalCenter: parent.horizontalCenter
            text: Qt.formatDate(root.timestamp, "dddd, d MMMM")
            color: "#606060"
            font.pixelSize: 12
        }

        Text {
            visible: root.systemEvent()
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: "•  " + root.plainText
            color: "#626262"
            font.italic: true
        }

        Row {
            visible: !root.systemEvent()
            width: parent.width
            spacing: 7

            Text {
                visible: root.showTimestamp
                width: 44
                horizontalAlignment: Text.AlignRight
                text: Qt.formatTime(root.timestamp, "HH:mm")
                color: "#777777"
                font.pixelSize: 11
            }

            Text {
                visible: root.showSender
                text: (root.ownEvent ? qsTr("You") : root.senderDisplayName) + ":"
                color: root.ownEvent ? "#6d3a85" : "#1f5d9c"
                font.bold: true
            }

            TextEdit {
                id: message
                width: Math.max(80, parent.width - 56)
                text: root.redacted ? qsTr("Message removed") : root.messageText()
                textFormat: TextEdit.RichText
                color: "#202020"
                wrapMode: TextEdit.Wrap
                readOnly: true
                selectByMouse: true
                onLinkActivated: Qt.openUrlExternally(link)
            }

            Text {
                visible: root.ownEvent && root.deliveryText().length > 0
                text: root.deliveryText()
                color: root.deliveryState === 4 ? "#b32929" : "#777777"
                font.pixelSize: 11
            }
        }
    }
}
