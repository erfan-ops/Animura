import QtQuick
import QtQuick.Controls

Rectangle {
    id: root

    property string message
    property string type: "success"   // success | warning | error
    property int duration: 2500

    signal finished()

    width: parent.width
    height: 42
    radius: 8
    opacity: 0
    y: -height

    color: {
        if (type === "success") return "#2e7d32"
        if (type === "warning") return "#ed6c02"
        if (type === "error") return "#d32f2f"
        return "#444"
    }

    Text {
        anchors.centerIn: parent
        text: root.message
        color: "white"
        font.pixelSize: 14
    }

    function start() {
        slideIn.start()
    }

    SequentialAnimation {
        id: slideIn

        ParallelAnimation {
            NumberAnimation { target: root; property: "y"; to: 0; duration: 220; easing.type: Easing.OutCubic }
            NumberAnimation { target: root; property: "opacity"; to: 1; duration: 150 }
        }

        PauseAnimation { duration: root.duration }

        ParallelAnimation {
            NumberAnimation { target: root; property: "y"; to: -root.height; duration: 200; easing.type: Easing.InCubic }
            NumberAnimation { target: root; property: "opacity"; to: 0; duration: 150 }
        }

        ScriptAction { script: root.finished() }
    }
}
