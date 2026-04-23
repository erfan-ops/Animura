import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import QtQuick.Dialogs
//import Qt.labs.platform


ColumnLayout {
    id: rootGroup
    property var schemaObj: ({})
    property var settingsObj: ({})
    property var path: []
    property bool hasChanged: false
    signal settingsChanged()

    spacing: 16

    function updateSettings(key, value) {
        hasChanged = true;
        settingsChanged();
        
        let current = rootGroup.settingsObj;

        // 1. Navigate through the existing path array (the parent groups)
        for (let p of rootGroup.path) {
            if (current && current[p] !== undefined) {
                current = current[p];
            } else {
                return undefined; // Path broken
            }
        }
        
        current[key] = value;
    }

    function getUpdatedSettings() {
        return settingsObj
    }

    // Helper: Converts RGBA array [R, G, B, A] from 0-1 to a QML Color
    function toColor(arr) {
        if (!arr || arr.length < 3) {
            return Qt.rgba(0.0, 0.0, 0.0, 0.0);
        }
        let a = arr.length > 3 ? arr[3] : 1.0;
        return Qt.rgba(arr[0], arr[1], arr[2], a);
    }

    // Helper: Formats "max-line-distance" into "Max Line Distance"
    function formatName(str) {
        if (!str) return "";
        return str.split('-').map(word => word.charAt(0).toUpperCase() + word.slice(1)).join(' ');
    }

    Repeater {
        // Iterate over the keys of the current schema level
        model: rootGroup.schemaObj ? Object.keys(rootGroup.schemaObj) : []

        delegate: ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            property string key: modelData
            property var fieldSchema: rootGroup.schemaObj[key]
            function getValue() {
                let current = rootGroup.settingsObj;

                // 1. Navigate through the existing path array (the parent groups)
                for (let p of rootGroup.path) {
                    if (current && current[p] !== undefined) {
                        current = current[p];
                    } else {
                        return undefined; // Path broken
                    }
                }

                return (current && current[key] !== undefined) ? current[key] : undefined;
            }
            
            // If there's no "type" defined, treat it as a nested dictionary/group
            property string fieldType: fieldSchema.type !== undefined ? fieldSchema.type : "group"

            // Field Label
            Label {
                text: rootGroup.formatName(key)
                font.pixelSize: fieldType === "group" ? 16 : 14
                font.bold: fieldType === "group"
                color: fieldType === "group" ? Material.accent : Material.foreground
                Layout.topMargin: fieldType === "group" ? 12 : 0
            }

            // Dynamic Control Loader
            Loader {
                Layout.fillWidth: true
                sourceComponent: {
                    switch(fieldType) {
                        case "int":
                        case "float": return sliderComp;
                        case "bool": return switchComp;
                        case "select": return selectComp;
                        case "color": return colorComp;
                        case "color_list": return colorListComp;
                        case "group": return groupComp;
                        default: return null;
                    }
                }
            }

            /* --- UI Component Definitions --- */

            Component {
                id: sliderComp
                RowLayout {
                    spacing: 12
                    Slider {
                        id: slider
                        Layout.fillWidth: true
                        from: fieldSchema.min !== undefined ? fieldSchema.min : 0
                        to: fieldSchema.max !== undefined ? fieldSchema.max : 100
                        stepSize: fieldSchema.step !== undefined ? fieldSchema.step : (fieldType === "int" ? 1 : 0.01)
                        value: getValue() !== undefined ? getValue() : from
                        onValueChanged: {
                            rootGroup.updateSettings(key, value)
                        }
                    }
                    Label {
                        text: Number(slider.value).toFixed(fieldType === "int" ? 0 : 2)
                        Layout.minimumWidth: 40
                        horizontalAlignment: Text.AlignRight
                        color: Material.foreground
                        opacity: 0.8
                    }
                }
            }

            Component {
                id: switchComp
                Switch {
                    id: switchElement
                    checked: getValue() === true
                    onToggled: {
                        rootGroup.updateSettings(key, checked)
                    }
                }
            }

            Component {
                id: selectComp
                ComboBox {
                    id: selectElement
                    Layout.fillWidth: true
                    model: fieldSchema.options || []
                    currentIndex: fieldSchema.options
                        ? fieldSchema.options.indexOf(getValue())
                        : -1
                    onCurrentIndexChanged: {
                        rootGroup.updateSettings(key, fieldSchema.options[currentIndex])
                    }
                }
            }

            Component {
                id: colorComp

                Rectangle {
                    id: swatch
                    width: 48
                    height: 32
                    radius: 4

                    color: rootGroup.toColor(getValue())

                    border.color: Qt.rgba(1, 1, 1, 0.2)
                    border.width: 1

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            var newColor = ColorDialogHelper.openColorDialog(swatch.color)
                            swatch.color = newColor
                            let arr = [newColor.r, newColor.g, newColor.b, newColor.a]
                            rootGroup.updateSettings(key, arr)
                        }
                    }
                }
            }

            Component {
                id: colorListComp

                Item {
                    id: root
                    Layout.fillWidth: true
                    height: 200

                    property int selectedIndex: -1
                    property var colors: getValue() || []

                    function save() {
                        colors = colors.slice()
                        rootGroup.updateSettings(key, colors)
                    }

                    function addColor() {
                        var c = ColorDialogHelper.openColorDialog("white")
                        if (!c) return
                        colors.push([c.r, c.g, c.b, c.a])
                        colors = colors.slice()
                        selectedIndex = colors.length - 1
                        save()
                    }

                    function removeColor() {
                        if (selectedIndex < 0) return
                        colors.splice(selectedIndex,1)
                        selectedIndex = Math.min(selectedIndex, colors.length-1)
                        colors = colors.slice()
                        save()
                    }

                    function updateColor(index, c) {
                        colors[index] = [c.r,c.g,c.b,c.a]
                        colors = colors.slice()
                        save()
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 6

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Button {
                                text: "+"
                                onClicked: root.addColor()
                            }

                            Button {
                                text: "-"
                                enabled: root.selectedIndex >= 0
                                onClicked: root.removeColor()
                            }
                        }

                        ScrollView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true

                            GridView {
                                id: grid
                                anchors.fill: parent

                                cellWidth: 48
                                cellHeight: 48

                                model: root.colors.length + 1

                                move: Transition {
                                    NumberAnimation { properties: "x,y"; duration: 150; easing.type: Easing.OutQuad }
                                }

                                add: Transition {
                                    NumberAnimation { properties: "scale,opacity"; from: 0.6; to: 1; duration: 120 }
                                }

                                remove: Transition {
                                    NumberAnimation { properties: "scale,opacity"; to: 0.6; duration: 120 }
                                }

                                delegate: Item {
                                    id: delegateRoot
                                    width: 40
                                    height: 40

                                    property bool isAddButton: index === root.colors.length
                                    property bool hovered: false

                                    Drag.active: dragArea.drag.active
                                    Drag.source: delegateRoot
                                    Drag.hotSpot.x: width / 2
                                    Drag.hotSpot.y: height / 2

                                    Rectangle {
                                        id: swatch
                                        anchors.fill: parent
                                        radius: 4

                                        color: isAddButton ? "transparent"
                                                           : rootGroup.toColor(root.colors[index])

                                        border.width: isAddButton ? 1 :
                                                      (index === root.selectedIndex ? 2 : 1)

                                        border.color: isAddButton ? "#888"
                                                      : (index === root.selectedIndex
                                                         ? "#4da3ff"
                                                         : Qt.rgba(1,1,1,0.2))

                                        opacity: hovered ? 1 : 0.9
                                        scale: hovered ? 1.05 : 1

                                        Behavior on scale { NumberAnimation { duration: 100 } }
                                        Behavior on opacity { NumberAnimation { duration: 100 } }

                                        Text {
                                            anchors.centerIn: parent
                                            visible: isAddButton
                                            text: "+"
                                            font.pixelSize: 16
                                            color: "#aaa"
                                        }
                                    }

                                    MouseArea {
                                        id: dragArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        drag.target: !isAddButton ? delegateRoot : null

                                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                                        cursorShape: Qt.PointingHandCursor

                                        onEntered: hovered = true
                                        onExited: hovered = false

                                        onClicked: function(mouse) {

                                            if (isAddButton) {
                                                root.addColor()
                                                return
                                            }

                                            if (mouse.button === Qt.LeftButton)
                                                root.selectedIndex = index

                                            if (mouse.button === Qt.RightButton && root.colors.length > 1) {
                                                root.selectedIndex = index
                                                root.removeColor()
                                            }
                                        }

                                        onDoubleClicked: {
                                            if (!isAddButton && mouse.button === Qt.LeftButton) {
                                                root.selectedIndex = index
                                                var qc = ColorDialogHelper.openColorDialog(swatch.color)
                                                if (qc)
                                                    root.updateColor(index, qc)
                                            }
                                        }
                                    }

                                    DropArea {
                                        anchors.fill: parent

                                        onEntered: function(drag) {

                                            if (isAddButton)
                                                return

                                            let from = drag.source.index
                                            let to = index

                                            if (from === to)
                                                return

                                            let item = root.colors[from]
                                            root.colors.splice(from,1)
                                            root.colors.splice(to,0,item)

                                            root.colors = root.colors.slice()
                                            root.selectedIndex = to
                                            root.save()
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }


            Component {
                id: groupComp
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Rectangle {
                        Layout.fillHeight: true
                        width: 2
                        color: Qt.rgba(1, 1, 1, 0.1)
                        radius: 1
                    }
                    Loader {
                        id: groupLoader
                        Layout.fillWidth: true
                        source: "SettingsGroup.qml"


                        onLoaded: {
                            if (groupLoader.item && groupLoader.item.settingsChanged) {
                                groupLoader.item.settingsChanged.connect(function() {
                                    rootGroup.hasChanged = true
                                    rootGroup.settingsChanged()
                                })
                            }
                        }
            
                        Binding { 
                            target: groupLoader.item 
                            property: "path" 
                            value: rootGroup.path.concat([key]) 
                        }
                        Binding { 
                            target: groupLoader.item 
                            property: "settingsObj" 
                            value: rootGroup.settingsObj 
                        }
                        Binding { 
                            target: groupLoader.item 
                            property: "schemaObj" 
                            value: fieldSchema 
                        }
                    }
                }
            }

        }
    }
}
