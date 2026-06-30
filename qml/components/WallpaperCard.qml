import QtQuick
import QtQuick.Controls

Item {
    id: root
    width: 240
    height: 200
    scale: hovered ? 1.04 : 1.0

    Behavior on scale {
        NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
    }

    property var moduleData
    signal clicked()
    property bool hovered: false
    property bool pressed: false

    // ── Shadow layer (offset dark rectangle → neumorphic depth) ──
    Rectangle {
        anchors.fill: parent
        anchors {
            leftMargin: hovered ? 5 : 3
            topMargin: hovered ? 6 : 4
        }
        radius: 18
        color: "#080218"
        opacity: hovered ? 0.7 : 0.5

        Behavior on anchors.leftMargin { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
        Behavior on anchors.topMargin  { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
        Behavior on opacity             { NumberAnimation { duration: 200 } }
    }

    // ── Card Surface ──
    Rectangle {
        id: card
        anchors.fill: parent
        radius: 18
        gradient: Gradient {
            GradientStop { position: 0.0; color: hovered ? "#281d48" : "#1e1438" }
            GradientStop { position: 1.0; color: hovered ? "#1e1438" : "#160d2a" }
        }

        Behavior on gradient {
            ColorAnimation { duration: 180 }
        }

        // Top-left light highlight (neumorphic light edge)
        Rectangle {
            anchors.fill: parent
            radius: card.radius
            color: "transparent"
            border {
                width: 1
                color: Qt.rgba(1, 1, 1, hovered ? 0.06 : 0.03)
            }
        }

        // ── Preview Image ──
        Rectangle {
            anchors {
                fill: parent
                margins: 6
                bottomMargin: 48
            }
            radius: 14
            clip: true
            color: "#0d0620"

            Image {
                anchors.fill: parent
                source: moduleData.previewPath || ""
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                sourceSize.width: 400
                opacity: 0.9
            }

            // Subtle inner border
            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                color: "transparent"
                border { width: 1; color: Qt.rgba(0, 0, 0, 0.3) }
            }
        }

        // ── Hover Glow Border ──
        Rectangle {
            anchors.fill: parent
            radius: card.radius
            color: "transparent"
            border {
                width: hovered ? 1.5 : 0
                color: Qt.rgba(0.88, 0.25, 0.56, hovered ? 0.4 : 0.0)
            }
            Behavior on border.color { ColorAnimation { duration: 180 } }
            Behavior on border.width   { NumberAnimation { duration: 180 } }
        }

        // ── Module Name Pill ──
        Rectangle {
            anchors {
                bottom: parent.bottom
                left: parent.left
                right: parent.right
                margins: 10
            }
            height: 34
            radius: 12
            color: Qt.rgba(0.06, 0.03, 0.12, 0.75)

            Label {
                anchors.centerIn: parent
                text: moduleData.name || "Unknown"
                color: "#ede4f5"
                font.pixelSize: 13
                font.weight: Font.Medium
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
                width: parent.width - 16
            }
        }

        // ── Interaction ──
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            onClicked: root.clicked()
            onEntered: hovered = true
            onExited:  { hovered = false }
        }
    }
}
