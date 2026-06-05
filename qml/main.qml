import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: window
    width: 1280
    height: 720
    visible: true
    title: "Animura"

    Material.theme: Material.Dark
    Material.accent: WindowsAccent
    Material.background: Qt.rgba(0.09, 0.09, 0.09, 1)

    function showMainWindow() {
        window.show()
        window.raise()
        window.requestActivate()
    }

    onClosing: (closeEvent) => {
        closeEvent.accepted = false
        window.hide()
    }

    header: ToolBar {
        height: 64
        background: Rectangle {
            color: Qt.rgba(1,1,1,0.05)
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Qt.rgba(1,1,1,0.08)
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            spacing: 16

            Label {
                text: "Animura"
                font.pixelSize: 20
                font.bold: true
                color: Material.foreground
            }

            Item { Layout.fillWidth: true }

            Button {
                text: "Stop Wallpaper"
                flat: true
                icon.name: "media-playback-stop"
                onClicked: backend.stopWallpaper()
            }
        }
    }

    GridView {
        id: grid
        anchors.fill: parent
        anchors.margins: 32
        cellWidth: 240
        cellHeight: 200
        model: backend.getModulesList()

        delegate: WallpaperCard {
            moduleData: modelData
            onClicked: {
                const obj = backend.loadSettingsUI(modelData.id)
                settingsPanel.settingsData = {} // why?
                settingsPanel.settingsData = obj.settings
                settingsPanel.schemaData = obj.schema
                settingsPanel.moduleId = modelData.id
                settingsPanel.open()
            }
        }

        ScrollIndicator.vertical: ScrollIndicator { }
    }

    SettingsPanel {
        id: settingsPanel
    }
    
    NotificationManager {
        id: notifications
    }
}
