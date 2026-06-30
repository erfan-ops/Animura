import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ColumnLayout {
    id: rootGroup

    property var schemaObj: ({})
    property var settingsObj: ({})
    property var path: []
    property bool hasChanged: false
    signal settingsChanged()

    spacing: 18

    // ── Navigate into nested settings by path ──
    function updateSettings(key, value) {
        hasChanged = true;
        settingsChanged();

        let current = rootGroup.settingsObj;
        for (let p of rootGroup.path) {
            if (current && current[p] !== undefined) {
                current = current[p];
            } else {
                return undefined;
            }
        }
        current[key] = value;
    }

    function getUpdatedSettings() {
        return settingsObj
    }

    // ── Convert RGBA [0–1] array to QML color ──
    function toColor(arr) {
        if (!arr || arr.length < 3) {
            return Qt.rgba(0.5, 0.5, 0.5, 1.0);
        }
        let a = arr.length > 3 ? arr[3] : 1.0;
        return Qt.rgba(arr[0], arr[1], arr[2], a);
    }

    // ── "max-line-distance" → "Max Line Distance" ──
    function formatName(str) {
        if (!str) return "";
        return str.split('-').map(word => word.charAt(0).toUpperCase() + word.slice(1)).join(' ');
    }

    // ── Design tokens (inline, matches all other components) ──
    readonly property color cAccent:      "#e04090"
    readonly property color cTextPrimary: "#ede4f5"
    readonly property color cTextSec:     "#b8a9cc"
    readonly property color cBgSurface:   "#1a1035"
    readonly property color cBgElevated:  "#221845"
    readonly property color cShadow:      "#060210"

    // ── Schema Key Iterator ──
    Repeater {
        model: rootGroup.schemaObj ? Object.keys(rootGroup.schemaObj) : []

        delegate: ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            property string key: modelData
            property var fieldSchema: rootGroup.schemaObj[key]
            property string fieldType: fieldSchema.type !== undefined ? fieldSchema.type : "group"

            function getValue() {
                let current = rootGroup.settingsObj;
                for (let p of rootGroup.path) {
                    if (current && current[p] !== undefined) {
                        current = current[p];
                    } else {
                        return undefined;
                    }
                }
                return (current && current[key] !== undefined) ? current[key] : undefined;
            }

            // ── Field Label ──
            Label {
                text: rootGroup.formatName(key)
                font.pixelSize: fieldType === "group" ? 16 : 13
                font.weight: fieldType === "group" ? Font.DemiBold : Font.Medium
                color: fieldType === "group" ? rootGroup.cAccent : rootGroup.cTextPrimary
                Layout.topMargin: fieldType === "group" ? 16 : 0
            }

            // ── Group separator line ──
            Rectangle {
                visible: fieldType === "group"
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Qt.rgba(0.88, 0.25, 0.56, 0.15)
                Layout.topMargin: -2
            }

            // ── Control Loader ──
            Loader {
                Layout.fillWidth: true
                sourceComponent: {
                    switch (fieldType) {
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

            /* ═══════════════════════════════════════════
               Slider — Neumorphic track + pink handle
               ═══════════════════════════════════════════ */
            Component {
                id: sliderComp
                RowLayout {
                    spacing: 14
                    Slider {
                        id: slider
                        Layout.fillWidth: true
                        from: fieldSchema.min !== undefined ? fieldSchema.min : 0
                        to: fieldSchema.max !== undefined ? fieldSchema.max : 100
                        stepSize: fieldSchema.step !== undefined ? fieldSchema.step
                            : (fieldType === "int" ? 1 : 0.01)
                        value: getValue() !== undefined ? getValue() : from
                        onValueChanged: rootGroup.updateSettings(key, value)

                        background: Rectangle {
                            x: slider.leftPadding
                            y: slider.topPadding + slider.availableHeight / 2 - 3
                            width: slider.availableWidth
                            height: 6
                            radius: 3
                            color: "#1a1035"
                            border.color: Qt.rgba(1,1,1,0.06)

                            Rectangle {
                                width: slider.visualPosition * parent.width
                                height: 6
                                radius: 3
                                gradient: Gradient {
                                    orientation: Gradient.Horizontal
                                    GradientStop { position: 0.0; color: "#c03078" }
                                    GradientStop { position: 1.0; color: rootGroup.cAccent }
                                }
                            }
                        }

                        handle: Rectangle {
                            x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
                            y: slider.topPadding + slider.availableHeight / 2 - height / 2
                            width: 20
                            height: 20
                            radius: 10
                            color: "#ede4f5"
                            border.color: rootGroup.cAccent
                            border.width: 2
                        }
                    }
                    Label {
                        text: Number(slider.value).toFixed(fieldType === "int" ? 0 : 2)
                        Layout.minimumWidth: 42
                        horizontalAlignment: Text.AlignRight
                        color: rootGroup.cTextSec
                        font.pixelSize: 13
                    }
                }
            }

            /* ═══════════════════════════════════════════
               Switch — Pink accent toggle
               ═══════════════════════════════════════════ */
            Component {
                id: switchComp
                Switch {
                    id: switchElement
                    checked: getValue() === true
                    onToggled: rootGroup.updateSettings(key, checked)

                    indicator: Rectangle {
                        implicitWidth: 44
                        implicitHeight: 26
                        radius: 13
                        color: switchElement.checked ? rootGroup.cAccent : "#1a1035"
                        border {
                            width: 1
                            color: switchElement.checked
                                ? Qt.rgba(1,0.4,0.7,0.3)
                                : Qt.rgba(1,1,1,0.08)
                        }
                        Behavior on color { ColorAnimation { duration: 150 } }

                        Rectangle {
                            x: switchElement.checked ? parent.width - width - 3 : 3
                            y: (parent.height - height) / 2
                            width: 20
                            height: 20
                            radius: 10
                            color: switchElement.checked ? "#ede4f5" : "#564a6e"
                            Behavior on x { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                            Behavior on color { ColorAnimation { duration: 150 } }
                        }
                    }
                }
            }

            /* ═══════════════════════════════════════════
               Select — Styled ComboBox
               ═══════════════════════════════════════════ */
            Component {
                id: selectComp
                ComboBox {
                    id: selectElement
                    Layout.fillWidth: true
                    Layout.preferredHeight: 38

                    model: fieldSchema.options || []
                    currentIndex: fieldSchema.options
                        ? fieldSchema.options.indexOf(getValue())
                        : -1
                    onCurrentIndexChanged: rootGroup.updateSettings(key, fieldSchema.options[currentIndex])

                    // Text display
                    contentItem: Text {
                        text: selectElement.displayText
                        font.pixelSize: 13
                        color: "#ede4f5"
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 14
                    }

                    background: Rectangle {
                        radius: 10
                        color: selectElement.hovered ? "#221845" : "#1a1035"
                        border {
                            width: 1
                            color: selectElement.hovered
                                ? Qt.rgba(0.88, 0.25, 0.56, 0.3)
                                : Qt.rgba(1,1,1,0.06)
                        }
                        Behavior on color        { ColorAnimation { duration: 120 } }
                        Behavior on border.color { ColorAnimation { duration: 120 } }
                    }

                    // Dropdown styling
                    delegate: ItemDelegate {
                        width: selectElement.width
                        implicitHeight: 34

                        contentItem: Text {
                            text: modelData
                            font.pixelSize: 13
                            color: highlighted ? rootGroup.cAccent : "#b8a9cc"
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 14
                        }
                        background: Rectangle {
                            radius: 6
                            color: highlighted ? "#221845" : "transparent"
                            Behavior on color { ColorAnimation { duration: 100 } }
                        }
                    }

                    popup: Popup {
                        y: selectElement.height + 4
                        width: selectElement.width
                        implicitHeight: contentItem.implicitHeight
                        padding: 4

                        contentItem: ListView {
                            clip: true
                            implicitHeight: contentHeight
                            model: selectElement.popup.visible ? selectElement.delegateModel : null
                            currentIndex: selectElement.highlightedIndex
                        }

                        background: Rectangle {
                            radius: 12
                            color: "#1a1035"
                            border {
                                width: 1
                                color: Qt.rgba(0.88, 0.25, 0.56, 0.2)
                            }
                        }
                    }

                    // Down arrow indicator
                    indicator: Text {
                        x: selectElement.width - width - 14
                        y: (selectElement.height - height) / 2
                        text: "▾"
                        font.pixelSize: 10
                        color: rootGroup.cAccent
                    }
                }
            }

            /* ═══════════════════════════════════════════
               Color — Neumorphic swatch
               ═══════════════════════════════════════════ */
            Component {
                id: colorComp
                Rectangle {
                    id: swatch
                    width: 48
                    height: 36
                    radius: 10
                    color: rootGroup.toColor(getValue())

                    border {
                        width: 1.5
                        color: Qt.rgba(1,1,1,0.1)
                    }

                    // Neumorphic depth
                    Rectangle {
                        anchors.fill: parent
                        radius: parent.radius
                        color: "transparent"
                        border {
                            width: 1
                            color: Qt.rgba(0,0,0,0.2)
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        hoverEnabled: true
                        onClicked: {
                            var newColor = ColorDialogHelper.openColorDialog(swatch.color)
                            swatch.color = newColor
                            let arr = [newColor.r, newColor.g, newColor.b, newColor.a]
                            rootGroup.updateSettings(key, arr)
                        }
                    }
                }
            }

            /* ═══════════════════════════════════════════
               Color List — Grid with add/remove/reorder
               ═══════════════════════════════════════════ */
            Component {
                id: colorListComp
                Item {
                    id: colorListRoot
                    Layout.fillWidth: true
                    height: 210

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
                        colors.splice(selectedIndex, 1)
                        selectedIndex = Math.min(selectedIndex, colors.length - 1)
                        colors = colors.slice()
                        save()
                    }

                    function updateColor(index, c) {
                        colors[index] = [c.r, c.g, c.b, c.a]
                        colors = colors.slice()
                        save()
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 8

                        // Add / Remove toolbar
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            // Add button
                            Rectangle {
                                width: 32; height: 28; radius: 8
                                color: colorListRootMouse.containsMouse ? rootGroup.cAccent : "#1a1035"
                                border { width: 1; color: Qt.rgba(1,1,1,0.06) }

                                Text {
                                    anchors.centerIn: parent; text: "+"
                                    font.pixelSize: 16; color: "#ede4f5"
                                }

                                MouseArea {
                                    id: colorListRootMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: colorListRoot.addColor()
                                }

                                Behavior on color { ColorAnimation { duration: 120 } }
                            }

                            // Remove button
                            Rectangle {
                                width: 32; height: 28; radius: 8
                                color: colorListRmMouse.containsMouse ? rootGroup.cAccent : "#1a1035"
                                border { width: 1; color: Qt.rgba(1,1,1,0.06) }
                                opacity: colorListRoot.selectedIndex >= 0 ? 1 : 0.4

                                Text {
                                    anchors.centerIn: parent; text: "−"
                                    font.pixelSize: 18; color: "#ede4f5"
                                }

                                MouseArea {
                                    id: colorListRmMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    enabled: colorListRoot.selectedIndex >= 0
                                    onClicked: colorListRoot.removeColor()
                                }

                                Behavior on color { ColorAnimation { duration: 120 } }
                            }

                            Item { Layout.fillWidth: true }
                        }

                        // Color grid
                        ScrollView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true

                            GridView {
                                id: colorGrid
                                anchors.fill: parent

                                cellWidth: 48
                                cellHeight: 48

                                model: colorListRoot.colors.length + 1

                                move: Transition {
                                    NumberAnimation { properties: "x,y"; duration: 160; easing.type: Easing.OutQuad }
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

                                    property bool isAddButton: index === colorListRoot.colors.length
                                    property bool hovered: false

                                    Drag.active: dragArea.drag.active
                                    Drag.source: delegateRoot
                                    Drag.hotSpot.x: width / 2
                                    Drag.hotSpot.y: height / 2

                                    Rectangle {
                                        id: colorSwatch
                                        anchors.fill: parent
                                        radius: 10

                                        color: isAddButton ? "#1a1035"
                                            : rootGroup.toColor(colorListRoot.colors[index])

                                        border {
                                            width: index === colorListRoot.selectedIndex ? 2 : 1
                                            color: index === colorListRoot.selectedIndex
                                                ? rootGroup.cAccent
                                                : isAddButton
                                                    ? Qt.rgba(1,1,1,0.06)
                                                    : Qt.rgba(1,1,1,0.1)
                                        }

                                        opacity: hovered ? 1 : 0.85
                                        scale: hovered ? 1.08 : 1

                                        Behavior on scale { NumberAnimation { duration: 100 } }
                                        Behavior on opacity { NumberAnimation { duration: 100 } }

                                        Text {
                                            anchors.centerIn: parent
                                            visible: isAddButton
                                            text: "+"
                                            font.pixelSize: 16
                                            color: "#564a6e"
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
                                                colorListRoot.addColor()
                                                return
                                            }
                                            if (mouse.button === Qt.LeftButton)
                                                colorListRoot.selectedIndex = index
                                            if (mouse.button === Qt.RightButton && colorListRoot.colors.length > 1) {
                                                colorListRoot.selectedIndex = index
                                                colorListRoot.removeColor()
                                            }
                                        }

                                        onDoubleClicked: {
                                            if (!isAddButton && mouse.button === Qt.LeftButton) {
                                                colorListRoot.selectedIndex = index
                                                var qc = ColorDialogHelper.openColorDialog(colorSwatch.color)
                                                if (qc)
                                                    colorListRoot.updateColor(index, qc)
                                            }
                                        }
                                    }

                                    DropArea {
                                        anchors.fill: parent
                                        onEntered: function(drag) {
                                            if (isAddButton) return
                                            let from = drag.source.index
                                            let to = index
                                            if (from === to) return
                                            let item = colorListRoot.colors[from]
                                            colorListRoot.colors.splice(from, 1)
                                            colorListRoot.colors.splice(to, 0, item)
                                            colorListRoot.colors = colorListRoot.colors.slice()
                                            colorListRoot.selectedIndex = to
                                            colorListRoot.save()
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            /* ═══════════════════════════════════════════
               Group — Recursive nested SettingsGroup
               ═══════════════════════════════════════════ */
            Component {
                id: groupComp
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14

                    // Left accent bar for nesting depth
                    Rectangle {
                        Layout.fillHeight: true
                        Layout.preferredWidth: 2
                        Layout.minimumHeight: 20
                        color: rootGroup.cAccent
                        opacity: 0.4
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
