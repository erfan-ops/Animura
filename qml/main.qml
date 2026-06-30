import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: window
    width: 1280
    height: 720
    visible: true
    title: "Animura"

    // ── Neumorphic Purple/Pink Design Tokens ──
    readonly property color bgBase:        "#0d0620"
    readonly property color bgSurface:     "#1a1035"
    readonly property color bgSurfaceHover:"#221845"
    readonly property color bgElevated:    "#261d42"
    readonly property color accent:         "#e04090"
    readonly property color accentGlow:    "#ff60b0"
    readonly property color textPrimary:   "#ede4f5"
    readonly property color textSecondary: "#b8a9cc"
    readonly property color shadowDark:    "#060210"
    readonly property color shadowLight:   "#2a1a50"
    readonly property color borderSubtle:  "#ffffff0d"
    readonly property int   radiusCard:    18
    readonly property int   radiusButton:  14
    readonly property int   radiusPanel:   24

    // Global background
    background: Rectangle {
        color: window.bgBase
    }

    function showMainWindow() {
        window.show()
        window.raise()
        window.requestActivate()
    }

    onClosing: (closeEvent) => {
        closeEvent.accepted = false
        window.hide()
    }

    // ── Header ──
    header: Rectangle {
        height: 56
        color: "transparent"

        // Subtle bottom gradient edge
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 0.3; color: Qt.rgba(0.88, 0.25, 0.56, 0.25) }
                GradientStop { position: 0.7; color: Qt.rgba(0.88, 0.25, 0.56, 0.25) }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 28
            anchors.rightMargin: 20
            spacing: 12

            // Logo / Title
            Label {
                text: "Animura"
                font.pixelSize: 22
                font.weight: Font.DemiBold
                color: window.textPrimary
                opacity: 0.95
            }

            Item { Layout.fillWidth: true }

            // Stop button — only visible when a module is running
            Button {
                id: stopBtn
                visible: backend.runningModuleId >= 0
                text: "Stop Wallpaper"
                font.pixelSize: 13
                flat: true

                contentItem: Text {
                    text: stopBtn.text
                    font: stopBtn.font
                    color: window.accent
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    radius: window.radiusButton
                    color: stopBtn.hovered ? Qt.rgba(0.88, 0.25, 0.56, 0.12) : "transparent"
                    border.color: stopBtn.hovered ? Qt.rgba(0.88, 0.25, 0.56, 0.3) : "transparent"
                    border.width: 1
                    Behavior on color { ColorAnimation { duration: 150 } }
                    Behavior on border.color { ColorAnimation { duration: 150 } }
                }

                onClicked: backend.stopWallpaper()
            }
        }
    }

    // ── Module Grid ──
    GridView {
        id: grid
        anchors.fill: parent
        anchors.margins: 36
        anchors.topMargin: 16
        cellWidth: 260
        cellHeight: 220
        model: backend.getModulesList()

        delegate: WallpaperCard {
            moduleData: modelData
            onClicked: {
                const obj = backend.loadSettingsUI(modelData.id)
                settingsPanel.settingsData = {}
                settingsPanel.settingsData = obj.settings
                settingsPanel.schemaData = obj.schema
                settingsPanel.moduleId = modelData.id
                settingsPanel.open()
            }
        }

        ScrollIndicator.vertical: ScrollIndicator {
            active: true
            contentItem: Rectangle {
                implicitWidth: 4
                radius: 2
                color: window.accent
                opacity: 0.5
            }
        }
    }

    // ── Settings Panel ──
    SettingsPanel {
        id: settingsPanel
    }

    // ── Notifications ──
    NotificationManager {
        id: notifications
    }
}
