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
        spacing: 5

        Text {
            visible: root.startsNewDay
            anchors.horizontalCenter: parent.horizontalCenter
            text: Qt.formatDate(root.timestamp, "dddd, d MMMM")
            color: "#b6c0cf"
            font.pixelSize: 12
        }

        Text {
            visible: root.systemEvent()
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: root.plainText
            color: "#b6c0cf"
            font.italic: true
        }

        Row {
            visible: !root.systemEvent()
            width: parent.width
            layoutDirection: root.ownEvent ? Qt.RightToLeft : Qt.LeftToRight
            spacing: 7

            Rectangle {
                visible: root.showSender
                width: 28
                height: 28
                radius: width / 2
                color: root.ownEvent ? "#7552a0" : "#4f8ecb"
                Text { anchors.centerIn: parent; text: root.senderDisplayName.slice(0, 1).toUpperCase(); color: "white" }
            }

            Column {
                width: Math.min(parent.width - 42, Math.max(120, bubble.implicitWidth))
                spacing: 2

                Text {
                    visible: root.showSender
                    text: root.ownEvent ? qsTr("You") : root.senderDisplayName
                    color: "#cbd5e1"
                    font.bold: true
                    font.pixelSize: 12
                }

                Rectangle {
                    id: bubble
                    width: Math.min(parent.width, message.implicitWidth + 22)
                    implicitHeight: message.implicitHeight + 14
                    radius: 8
                    color: root.ownEvent ? "#4b3764" : "#303946"

                    TextEdit {
                        id: message
                        anchors.fill: parent
                        anchors.margins: 10
                        text: root.redacted ? qsTr("Message removed") : root.messageText()
                        textFormat: TextEdit.RichText
                        color: "#f2f4f8"
                        wrapMode: TextEdit.Wrap
                        readOnly: true
                        selectByMouse: true
                        onLinkActivated: Qt.openUrlExternally(link)
                    }
                }

                Text {
                    visible: root.showTimestamp
                    text: Qt.formatTime(root.timestamp, "HH:mm")
                    color: "#aeb8c7"
                    font.pixelSize: 11
                }

                Text {
                    visible: root.ownEvent && root.deliveryText().length > 0
                    text: root.deliveryText()
                    color: root.deliveryState === 4 ? "#ff8b8b" : "#aeb8c7"
                    font.pixelSize: 11
                }
            }
        }
    }
}
