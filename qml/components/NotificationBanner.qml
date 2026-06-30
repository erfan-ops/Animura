import QtQuick
import QtQuick.Controls

Rectangle {
    id: root

    property string message
    property string type: "success"
    property int duration: 2500

    signal finished()

    width: parent.width
    height: 44
    radius: 14
    opacity: 0
    y: -height

    // ── Gradient based on type ──
    gradient: Gradient {
        orientation: Gradient.Horizontal
        GradientStop { position: 0.0; color: type === "success" ? "#2a1040" :
                                              type === "warning" ? "#3a1a10" :
                                              type === "error"   ? "#3a1020" : "#1a1035" }
        GradientStop { position: 1.0; color: type === "success" ? "#1a1035" :
                                              type === "warning" ? "#1a1035" :
                                              type === "error"   ? "#1a1035" : "#1a1035" }
    }

    // Left accent dot
    Rectangle {
        anchors {
            left: parent.left
            leftMargin: 14
            verticalCenter: parent.verticalCenter
        }
        width: 8; height: 8; radius: 4
        color: type === "success" ? root.paletteAcent :
               type === "warning" ? "#ed6c02" :
               type === "error"   ? "#e04040" : root.paletteAcent
    }

    property color paletteAcent: "#e04090"

    Text {
        anchors.centerIn: parent
        text: root.message
        color: "#ede4f5"
        font.pixelSize: 13
        font.weight: Font.Medium
    }

    function start() {
        slideIn.start()
    }

    SequentialAnimation {
        id: slideIn

        ParallelAnimation {
            NumberAnimation { target: root; property: "y"; to: 8; duration: 240; easing.type: Easing.OutCubic }
            NumberAnimation { target: root; property: "opacity"; to: 1; duration: 160 }
        }

        PauseAnimation { duration: root.duration }

        ParallelAnimation {
            NumberAnimation { target: root; property: "y"; to: -root.height; duration: 200; easing.type: Easing.InCubic }
            NumberAnimation { target: root; property: "opacity"; to: 0; duration: 140 }
        }

        ScriptAction { script: root.finished() }
    }
}
