import QtQuick
import QtQuick.Controls

Item {
    id: root

    anchors.top: parent.top
    width: parent.width

    property var queue: []

    Column {
        id: stack
        anchors.top: parent.top
        width: parent.width
        spacing: 6
    }

    Component {
        id: bannerComponent
        NotificationBanner { }
    }

    function show(message, type="success") {

        queue.push({message: message, type: type})
        processQueue()
    }

    function processQueue() {

        if (queue.length === 0)
            return

        if (stack.children.length > 4)
            return   // prevent infinite stacking

        const data = queue.shift()

        const banner = bannerComponent.createObject(stack, {
            message: data.message,
            type: data.type
        })

        banner.finished.connect(function() {
            banner.destroy()
            processQueue()
        })

        banner.start()
    }
}
