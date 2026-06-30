import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Drawer {
    id: root
    width: 420
    height: parent.height
    edge: Qt.RightEdge
    interactive: true
    modal: false

    // ── Design tokens (inline, consistent across all components) ──
    readonly property color cAccent:      "#e04090"
    readonly property color cBgBase:      "#0d0620"
    readonly property color cBgSurface:   "#1a1035"
    readonly property color cBgElevated:  "#221845"
    readonly property color cTextPrimary: "#ede4f5"
    readonly property color cTextSec:     "#b8a9cc"
    readonly property color cShadow:      "#080218"
    readonly property int   cRadius:      14

    property var schemaData: ({})
    property var settingsData: ({})
    property var moduleId: -1

    // ── Panel Background ──
    background: Rectangle {
        color: "#0f0825"
        radius: 0

        // Left gradient edge
        Rectangle {
            anchors {
                left: parent.left
                top: parent.top
                bottom: parent.bottom
            }
            width: 2
            gradient: Gradient {
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 0.5; color: Qt.rgba(0.88, 0.25, 0.56, 0.3) }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }
    }

    // ── Enter Animation ──
    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "x"; from: root.parent.width; to: root.parent.width - root.width
                              duration: 280; easing.type: Easing.OutCubic }
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 220 }
        }
    }

    exit: Transition {
        ParallelAnimation {
            NumberAnimation { property: "x"; from: root.parent.width - root.width; to: root.parent.width
                              duration: 220; easing.type: Easing.InCubic }
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 180 }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ── Header ──
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            color: "transparent"

            // Bottom separator
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width - 32
                height: 1
                anchors.horizontalCenter: parent.horizontalCenter
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop { position: 0.5; color: Qt.rgba(0.88, 0.25, 0.56, 0.25) }
                    GradientStop { position: 1.0; color: "transparent" }
                }
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 28
                anchors.rightMargin: 20

                Label {
                    text: "Module Settings"
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    color: "#ede4f5"
                    Layout.fillWidth: true
                }
                ToolButton {
                    text: "Close"
                    font.pixelSize: 13
                    contentItem: Text {
                        text: parent.text
                        font: parent.font
                        color: "#b8a9cc"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 10
                        color: parent.hovered ? Qt.rgba(1,1,1,0.05) : "transparent"
                        Behavior on color { ColorAnimation { duration: 120 } }
                    }
                    onClicked: root.close()
                }
            }
        }

        // ── Scrollable Settings ──
        ScrollView {
            id: scrollArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical.policy: ScrollBar.AsNeeded

            ScrollBar.vertical.background: Rectangle { color: "transparent" }

            ScrollBar.vertical.contentItem: Rectangle {
                color: "#e04090"

                HoverHandler { id: hoverH }

                implicitWidth: hoverH.hovered ? 6 : 4
                radius: hoverH.hovered ? 3 : 2
                opacity: hoverH.hovered ? 0.70 : 0.35

                Behavior on implicitWidth { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                Behavior on radius        { NumberAnimation { duration: 150 } }
                Behavior on opacity       { NumberAnimation { duration: 150 } }
            }

            Column {
                width: parent.width
                spacing: 24
                padding: 24

                SettingsGroup {
                    id: settingsGroup
                    width: parent.width - 48
                    anchors.horizontalCenter: parent.horizontalCenter

                    schemaObj: root.schemaData
                    settingsObj: root.settingsData

                    // (palette is defined inline in SettingsGroup)
                }
            }
        }

        // ── Footer Buttons ──
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 140
            color: "#0b0520"

            // Top glow separator
            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: 1
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop { position: 0.5; color: Qt.rgba(0.88, 0.25, 0.56, 0.2) }
                    GradientStop { position: 1.0; color: "transparent" }
                }
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 24
                spacing: 12

                // ── Apply Button (Neumorphic) ──
                Button {
                    id: applyBtn
                    text: "Apply"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    enabled: settingsGroup.hasChanged
                    font.pixelSize: 14
                    font.weight: Font.Medium

                    contentItem: Text {
                        text: applyBtn.text
                        font: applyBtn.font
                        color: applyBtn.enabled ? "#0d0620" : "#564a6e"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        radius: 14
                        gradient: Gradient {
                            GradientStop {
                                position: 0.0
                                color: applyBtn.enabled
                                    ? (applyBtn.down ? "#a02860"
                                       : applyBtn.hovered ? "#f050a0" : "#e04090")
                                    : (applyBtn.down ? "#100820" : "#1a1035")
                            }
                            GradientStop {
                                position: 1.0
                                color: applyBtn.enabled
                                    ? (applyBtn.down ? "#801840"
                                       : applyBtn.hovered ? "#d04088" : "#c03078")
                                    : (applyBtn.down ? "#0a0418" : "#150d2a")
                            }
                        }
                        border {
                            width: 1
                            color: applyBtn.enabled
                                ? (applyBtn.down
                                    ? Qt.rgba(1,0.3,0.5,0.2)
                                    : Qt.rgba(1,0.4,0.7,0.3))
                                : Qt.rgba(1,1,1,0.04)
                        }

                        Behavior on gradient { ColorAnimation { duration: 150 } }
                        Behavior on border.color { ColorAnimation { duration: 150 } }
                    }

                    onClicked: root.applyChanges()
                }

                // ── Start / Stop Button ──
                Button {
                    id: actionBtn
                    property bool isRunning: backend.runningModuleId === root.moduleId
                    text: isRunning ? "Stop" : "Start"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    font.pixelSize: 14
                    font.weight: Font.Medium

                    contentItem: Text {
                        text: actionBtn.text
                        font: actionBtn.font
                        color: actionBtn.isRunning ? "#e04090" : "#ede4f5"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        radius: 14
                        color: actionBtn.down ? Qt.rgba(0.88,0.25,0.56,0.18)
                               : actionBtn.hovered ? "#221845" : "transparent"
                        border {
                            width: 1
                            color: actionBtn.isRunning
                                ? Qt.rgba(0.88, 0.25, 0.56, 0.35)
                                : Qt.rgba(1, 1, 1, 0.08)
                        }
                        Behavior on color { ColorAnimation { duration: 150 } }
                    }

                    onClicked: {
                        if (actionBtn.isRunning) {
                            backend.stopWallpaper()
                        } else {
                            backend.startWallpaper(root.moduleId)
                        }
                    }
                }
            }
        }
    }

    function applyChanges() {
        settingsGroup.hasChanged = false;

        let newSettings = settingsGroup.getUpdatedSettings()

        // Send to backend
        backend.applySettings(root.moduleId, newSettings)

        // Update the local data
        root.settingsData = newSettings

        notifications.show("Settings applied", "success")
    }
}
