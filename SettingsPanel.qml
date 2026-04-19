import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material

Drawer {
    id: root
    width: 400
    height: parent.height
    edge: Qt.RightEdge
    interactive: true

    property var schemaData: ({})
    property var settingsData: ({})
    property var moduleId: -1

    background: Rectangle {
        color: Material.background
        // Left border separator
        Rectangle {
            width: 1
            height: parent.height
            anchors.left: parent.left
            color: Qt.rgba(1, 1, 1, 0.08)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Panel Header
        ToolBar {
            Layout.fillWidth: true
            height: 64
            background: Rectangle {
                color: "transparent"
                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: Qt.rgba(1, 1, 1, 0.08)
                }
            }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 24
                anchors.rightMargin: 16
                
                Label {
                    text: "Module Settings"
                    font.pixelSize: 18
                    font.bold: true
                    color: Material.foreground
                    Layout.fillWidth: true
                }
                ToolButton {
                    text: "Close"
                    onClicked: root.close()
                }
            }
        }

        // Scrollable content area
        ScrollView {
            id: scrollArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

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
                }

                // bottom spacer so last item is fully visible
                Item { height: 0 }
            }
        }

        // BUTTONS SECTION
        Rectangle {
            Layout.fillWidth: true
            height: 128
            color: Material.background

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16

                // APPLY BUTTON
                Button {
                    text: "Apply"
                    Layout.fillWidth: true
                    enabled: settingsGroup.hasChanged
                    onClicked: root.applyChanges()
                }

                // START BUTTON
                Button {
                    text: "Start"
                    Layout.fillWidth: true
                    enabled: true
                    onClicked: backend.startWallpaper(root.moduleId)
                }
            }

            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: 1
                color: Qt.rgba(1,1,1,0.08)
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
