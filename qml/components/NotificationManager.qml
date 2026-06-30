import QtQuick
import QtQuick.Controls

Item {
    id: root

    anchors.top: parent.top
    anchors.topMargin: 8
    width: parent.width

    readonly property color cAccent: "#e04090"
    property var queue: []

    Column {
        id: stack
        anchors.top: parent.top
        width: parent.width - 16
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 6
    }

    Component {
        id: bannerComponent
        NotificationBanner { }
    }

    function show(message, type) {
        if (!type) type = "success"
        queue.push({message: message, type: type})
        processQueue()
    }

    function processQueue() {
        if (queue.length === 0)
            return
        if (stack.children.length > 4)
            return

        const data = queue.shift()

        const banner = bannerComponent.createObject(stack, {
            message: data.message,
            type: data.type,
            paletteAcent: root.cAccent
        })

        banner.finished.connect(function() {
            banner.destroy()
            processQueue()
        })

        banner.start()
    }
}
