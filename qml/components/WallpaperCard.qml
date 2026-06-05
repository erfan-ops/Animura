import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material


Item {
    id: root
    width: 220
    height: 170
    scale: hovered ? 1.03 : 1.0

    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

    property var moduleData
    signal clicked()
    property bool hovered: false

    Rectangle {
        anchors.fill: parent
        radius: 0
        clip: true
        
        color: hovered ? Qt.rgba(0.18,0.18,0.18,1) : Qt.rgba(0.14,0.14,0.14,1)
        border.color: hovered ? Material.accentColor : Qt.rgba(1,1,1,0.06)
        border.width: 1
        Behavior on color { ColorAnimation { duration: 120 } }
        Behavior on border.color { ColorAnimation { duration: 120 } }


        Image {
            anchors.fill: parent
            anchors.margins: 1
            source: moduleData.previewPath || ""
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            sourceSize.width: 400
        }
        

        // Text overlay
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 12
            height: nameLabel.implicitHeight + 12
            
            color: Qt.rgba(0, 0, 0, 0.65)
            radius: height / 2 

            Label {
                id: nameLabel
                anchors.centerIn: parent
                text: moduleData.name || "Unknown"
                color: "white"
                font.pixelSize: 14
                font.bold: true
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
                width: parent.width - 16
            }
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            onClicked: root.clicked()
            onEntered: hovered = true
            onExited:  hovered = false
        }
    }
}
