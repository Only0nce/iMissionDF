import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtGraphicalEffects 1.15

Item {
    width: 1920
    height: 1080

    property var tempDataArray: []
    property var fullDataArray: []
    property var pageList: []
    property var paginationModel: []
    property int currentIndex: 0
    property int itemsPerPage: 15
    property int totalPages: 0
    property int currentPage: 1
    property int pendingPage: -1
    property bool isPopupVisible: false

    DragHandler {
        target: null
        onActiveChanged: {
            if (!active && !isPopupVisible) {
                if (translation.x > 60 && currentPage > 1) {
                    goToPage(currentPage - 1)
                } else if (translation.x < -50 && currentPage < totalPages) {
                    goToPage(currentPage + 1)
                }
            }
        }
    }



    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#292e49" }
            GradientStop { position: 1.0; color: "#536976" }
        }
        z: -1
    }
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#292e49" }
            GradientStop { position: 1.0; color: "#536976" }
        }
        z: -1
    }
    ListModel {
        id: logModel
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            Layout.preferredHeight: 70
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 20
            spacing: 10

            Item { Layout.preferredWidth: 70 }

            TextField {
                id: searchField
                placeholderText: "Search by frequency or date..."
                Layout.preferredWidth: 300
                background: Rectangle {
                    color: "#8e9eab"
                    border.color: "#eef2f3"
                    radius: 4
                    opacity: 0.5
                }
                onTextChanged: filterModel()
            }

            Button {
                text: "Newest First"
                background: Rectangle {
                    color: "#8e9eab"
                    border.color: "#eef2f3"
                    radius: 4
                    opacity: 0.5
                }
                onClicked: {
                    tempDataArray.sort((a, b) => new Date(b.log_datetime) - new Date(a.log_datetime))
                    totalPages = Math.ceil(tempDataArray.length / itemsPerPage)
                    updatePagination()
                    goToPage(1)
                }
            }

            Button {
                text: "Oldest First"
                background: Rectangle {
                    color: "#8e9eab"
                    border.color: "#eef2f3"
                    radius: 4
                    opacity: 0.5
                }
                onClicked: {
                    tempDataArray.sort((a, b) => new Date(a.log_datetime) - new Date(b.log_datetime))
                    totalPages = Math.ceil(tempDataArray.length / itemsPerPage)
                    updatePagination()
                    goToPage(1)
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Item { Layout.preferredWidth: 100 }

            GridView {
                id: grid
                Layout.fillWidth: true
                Layout.fillHeight: true
                cellWidth: 350
                cellHeight: 300
                model: logModel

                delegate: Item {
                    width: 300
                    height: 250
                    opacity: 0.0
                    Component.onDestruction: {
                        previewImage.source = ""
                    }

                    Behavior on opacity {
                        NumberAnimation { duration: 300 }
                    }

                    Component.onCompleted: opacity = 1.0

                    Rectangle {
                        anchors.fill: parent
                        radius: 10
                        color: "#1e2a38"
                        border.color: "#2f3e4e"
                        border.width: 1
                        opacity: 0.5

                        Column {
                            width: parent.width - 20
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: 6
                            padding: 10

                            Text {
                                text: `${frequency} MHz | ${log_datetime}`
                                color: "white"
                                font.pixelSize: 14
                                wrapMode: Text.Wrap
                            }

                            Text {
                                text: `${lat}, ${lon} | Alt: ${altitude} m`
                                color: "#ccc"
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                            }

                            Text {
                                text: `Heading: ${heading.toFixed(2)}°`
                                color: "#ccc"
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                            }

                            Text {
                                text: `Direction: ${direction.toFixed(2)}°`
                                color: "white"
                            }

                            Text {
                                text: `${mgrs}`
                                color: "#aaa"
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                            }

                            Image {
                                id: previewImage
                                source: pic.startsWith("/")
                                    ? "http://" + Krakenmapval.serverKraken + "/pic/" + pic.split("/").pop()
                                    : pic
                                width: 250
                                height: 110
                                fillMode: Image.PreserveAspectCrop
                                anchors.horizontalCenter: parent.horizontalCenter
                                cache: false
                                sourceSize.width: 1000
                                sourceSize.height: 600
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: popupImageViewer.open()
                                    cursorShape: Qt.PointingHandCursor
                                }
                            }

                            Popup {
                                id: popupImageViewer
                                modal: true
                                focus: true
                                width: 1500
                                height: 900
                                dim: true
                                parent: Overlay.overlay
                                anchors.centerIn: parent

                                onVisibleChanged: {
                                    isPopupVisible = visible
                                    if (visible) {
                                        fullImage.source = previewImage.source
                                    } else {
                                        fullImage.source = ""
                                    }
                                }


                                function limitImageToBounds() {
                                    if (!imageContainer) return;

                                    if (imageContainer.scale < 1.0)
                                        imageContainer.scale = 1.0

                                    let halfWidth = (imageContainer.width * imageContainer.scale - imageContainer.width) / 2
                                    let halfHeight = (imageContainer.height * imageContainer.scale - imageContainer.height) / 2

                                    imageContainer.x = Math.max(Math.min(imageContainer.x, halfWidth), -halfWidth)
                                    imageContainer.y = Math.max(Math.min(imageContainer.y, halfHeight), -halfHeight)
                                }

                                background: Rectangle {
                                    color: "#000000cc"
                                    radius: 10
                                    border.color: "#888"
                                    border.width: 1
                                }

                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    radius: 10
                                    color: "#000000cc"
                                    border.color: "#4a5a6a"
                                    border.width: 1

                                    Item {
                                        id: pinchArea
                                        anchors.fill: parent
                                        clip: true

                                        Item {
                                            id: imageContainer
                                            scale: 1.0
                                            x: 0
                                            y: 0
                                            width: pinchArea.width
                                            height: pinchArea.height

                                            Image {
                                                id: fullImage
                                                anchors.fill: parent
                                                source: previewImage.source
                                                fillMode: Image.PreserveAspectFit
                                                cache: false
                                            }
                                        }

                                        PinchArea {
                                            anchors.fill: parent
                                            pinch.target: imageContainer
                                            pinch.minimumScale: 1.0
                                            pinch.maximumScale: 4.0
                                            pinch.dragAxis: Pinch.XAndYAxis
                                        }

                                        WheelHandler {
                                            target: imageContainer
                                            onWheel: {
                                                const delta = wheel.angleDelta.y > 0 ? 0.1 : -0.1
                                                const newScale = Math.max(1.0, Math.min(4.0, imageContainer.scale + delta))
                                                imageContainer.scale = newScale
                                                limitImageToBounds()
                                            }
                                        }
                                    }

                                    Item {
                                        width: 25
                                        height: 25
                                        anchors.top: parent.top
                                        anchors.right: parent.right
                                        anchors.topMargin: -30
                                        anchors.rightMargin: -30
                                        visible: popupImageViewer.visible
                                        z: 999

                                        Rectangle {
                                            id: closeBtn
                                            width: 25
                                            height: 25
                                            radius: 18
                                            color: "red"
                                            border.color: "red"
                                            border.width: 1
                                            anchors.centerIn: parent

                                            Text {
                                                anchors.centerIn: parent
                                                text: "✕"
                                                color: "white"
                                                font.bold: true
                                                font.pixelSize: 18
                                            }

                                            MouseArea {
                                                anchors.fill: parent
                                                onClicked: popupImageViewer.close()
                                                cursorShape: Qt.PointingHandCursor
                                            }
                                        }

                                        Glow {
                                            anchors.fill: closeBtn
                                            source: closeBtn
                                            radius: 12
                                            samples: 32
                                            color: "#ff5555"
                                            spread: 0.3
                                            transparentBorder: true
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // Pagination Control
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 6
            Layout.margins: 20

            Button {
                text: "⟨"
                enabled: currentPage > 1
                onClicked: goToPage(currentPage - 1)
                font.pixelSize: 16
                width: 36
                height: 36
                background: Rectangle {
                    color: enabled ? "#3c4c5a" : "#2a2f38"
                    radius: 8
                    border.color: "#5c7080"
                    opacity: enabled ? 1.0 : 0.4
                }
                contentItem: Text {
                    text: parent.text
                    anchors.centerIn: parent
                    color: "white"
                }
            }

            Repeater {
                model: paginationModel
                delegate: Button {
                    text: modelData.text
                    enabled: modelData.enabled
                    width: 36
                    height: 36
                    font.pixelSize: 14

                    background: Rectangle {
                        color: (modelData.page === currentPage) ? "#2196f3" : "#3c4c5a"
                        radius: 8
                        border.color: (modelData.page === currentPage) ? "#64b5f6" : "#5c7080"
                        border.width: 1
                        opacity: 0.9
                    }
                    contentItem: Text {
                        text: parent.text
                        anchors.centerIn: parent
                        color: (modelData.page === currentPage) ? "white" : "#d0d0d0"
                        font.bold: (modelData.page === currentPage)
                    }

                    onClicked: {
                        if (modelData.page > 0)
                            goToPage(modelData.page)
                    }
                }
            }

            Button {
                text: "⟩"
                enabled: currentPage < totalPages
                onClicked: goToPage(currentPage + 1)
                font.pixelSize: 16
                width: 36
                height: 36
                background: Rectangle {
                    color: enabled ? "#3c4c5a" : "#2a2f38"
                    radius: 8
                    border.color: "#5c7080"
                    opacity: enabled ? 1.0 : 0.4
                }
                contentItem: Text {
                    text: parent.text
                    anchors.centerIn: parent
                    color: "white"
                }
            }

            // Optional: แสดงหน้าปัจจุบัน / จำนวนหน้า
            Label {
                text: ` Page ${currentPage} / ${totalPages} `
                color: "#dddddd"
                font.pixelSize: 14
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                padding: 4
            }
        }

    }

    Component.onCompleted: {
        startLoadTimer.start()
    }

    Timer {
        id: startLoadTimer
        interval: 1000
        repeat: false
        onTriggered: mainWindow.requestDataLog()
    }

    Timer {
        id: loadItemTimer
        interval: 100
        repeat: true
        running: false
        onTriggered: {
            if (currentIndex < Math.min(currentPage * itemsPerPage, tempDataArray.length)) {
                logModel.append(tempDataArray[currentIndex])
                currentIndex++
            } else {
                loadItemTimer.stop()
            }
        }
    }

    Connections {
        id: dataLogConnection
        target: mainWindow
        function onDataLogReady(dataArray) {
            fullDataArray = dataArray.slice(0, 2000)
            tempDataArray = fullDataArray.slice(0)
            tempDataArray.sort((a, b) => new Date(b.log_datetime) - new Date(a.log_datetime))
            totalPages = Math.ceil(tempDataArray.length / itemsPerPage)
            updatePagination()
            goToPage(1)
        }
    }


    Timer {
        id: pageDelayTimer
        interval: 50
        repeat: false
        onTriggered: {
            currentPage = pendingPage
            currentIndex = (pendingPage - 1) * itemsPerPage
            loadItemTimer.start()
            grid.opacity = 1.0
        }
    }

    function goToPage(p) {
        for (let i = 0; i < logModel.count; i++) {
            logModel.setProperty(i, "pic", "")
        }
        logModel.clear()

        var items = grid.contentItem.children
        for (var i = 0; i < items.length; i++) {
            if (items[i].previewImage) {
                items[i].previewImage.source = ""
            }
        }

        grid.opacity = 0.0

        pendingPage = p
        const startIndex = (p - 1) * itemsPerPage
        const endIndex = Math.min(startIndex + itemsPerPage, tempDataArray.length)
        pageList = tempDataArray.slice(startIndex, endIndex)

        currentIndex = 0
        pageDelayTimer.start()

        updatePagination()
    }



    function filterModel() {
        const keyword = searchField.text.toLowerCase()

        if (keyword === "") {
            tempDataArray = fullDataArray.slice(0)
        } else {
            const filtered = []
            for (let i = 0; i < fullDataArray.length; ++i) {
                const item = fullDataArray[i]
                const text = `${item.frequency} ${item.lat} ${item.lon} ${item.log_datetime} ${item.mgrs}`.toLowerCase()
                if (text.indexOf(keyword) !== -1)
                    filtered.push(item)
            }
            tempDataArray = filtered
        }

        totalPages = Math.ceil(tempDataArray.length / itemsPerPage)
        goToPage(1)
    }

    function updatePagination() {
        let pages = [];

        if (totalPages <= 9) {
            // แสดงทุกหน้าเพราะน้อยกว่า 9
            for (let i = 1; i <= totalPages; i++) {
                pages.push({
                    text: i.toString(),
                    page: i,
                    enabled: true,
                    active: i === currentPage
                });
            }
        } else {
            if (currentPage <= 5) {
                // อยู่ช่วงต้น → แสดง 1 ถึง 7 แล้ว ... และ หน้าสุดท้าย
                for (let i = 1; i <= 7; i++) {
                    pages.push({
                        text: i.toString(),
                        page: i,
                        enabled: true,
                        active: i === currentPage
                    });
                }
                pages.push({ text: "...", page: 0, enabled: false });
                pages.push({
                    text: totalPages.toString(),
                    page: totalPages,
                    enabled: true,
                    active: totalPages === currentPage
                });
            } else if (currentPage >= totalPages - 4) {
                // อยู่ช่วงท้าย → แสดงหน้า 1, ... แล้ว totalPages -6 ถึง totalPages
                pages.push({
                    text: "1",
                    page: 1,
                    enabled: true,
                    active: currentPage === 1
                });
                pages.push({ text: "...", page: 0, enabled: false });
                for (let i = totalPages - 6; i <= totalPages; i++) {
                    pages.push({
                        text: i.toString(),
                        page: i,
                        enabled: true,
                        active: i === currentPage
                    });
                }
            } else {
                // อยู่ช่วงกลาง → แสดง 1, ..., currentPage-2 ถึง currentPage+2, ..., totalPages
                pages.push({
                    text: "1",
                    page: 1,
                    enabled: true,
                    active: currentPage === 1
                });
                pages.push({ text: "...", page: 0, enabled: false });

                for (let i = currentPage - 2; i <= currentPage + 2; i++) {
                    pages.push({
                        text: i.toString(),
                        page: i,
                        enabled: true,
                        active: i === currentPage
                    });
                }

                pages.push({ text: "...", page: 0, enabled: false });
                pages.push({
                    text: totalPages.toString(),
                    page: totalPages,
                    enabled: true,
                    active: totalPages === currentPage
                });
            }
        }

        paginationModel = pages;
    }


    Component.onDestruction: {
        logModel.clear()
        pageList = []
        tempDataArray = []
        fullDataArray = []
        paginationModel = []
        currentIndex = 0
        currentPage = 1
        totalPages = 0
    }



}
