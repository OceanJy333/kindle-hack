// input: recovered official pages and Voyage shelf; output: immersive reading inside the desk tab bar; pos: ARMv7 integration shell.
import QtQuick
import QtQuick.Window
import "original/qml/pages"
Window {
    id: window
    property bool offlinePanelVisible: false
    property string mode: "clock"
    // Same geometry as the desk (tokens.ts space.tab / client.c TAB_BAR_HEIGHT).
    readonly property int tabHeight: 124
    readonly property bool reading: mode === "reader" && appController.currentPage === "reader"
    // Reading is full screen; the tab bar returns with the reader's own toolbar.
    readonly property bool tabsVisible: !reading || reader.panelsOpen
    width: 1072; height: 1448
    visible: true; color: "white"
    Item {
        anchors.fill: parent; anchors.bottomMargin: window.tabHeight
        visible: window.mode === "reader"
        LoginPage { anchors.fill: parent; visible: appController.currentPage === "login" }
        Shelf { anchors.fill: parent; visible: appController.currentPage === "bookshelf" && !window.offlinePanelVisible
            onOfflineRequested: window.offlinePanelVisible = true }
        OfflinePanel { anchors.fill: parent; visible: window.offlinePanelVisible && appController.currentPage === "bookshelf"
            onClose: window.offlinePanelVisible = false }
    }
    NativeDesk { id: nativeDesk; anchors.fill: parent; anchors.bottomMargin: window.tabHeight
        visible: window.mode !== "reader"; mode: window.mode
        onOpenReader: window.mode = "reader"
        onModeRequested: function(next){window.mode=next}
    }
    ReaderPage {
        id: reader
        anchors.fill: parent; visible: window.reading
        bookId: appController.currentBookId; bookTitle: appController.currentBookTitle
        inkEnabled: false; toolbarInset: window.tabHeight
    }
    Rectangle {
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        height: window.tabHeight; color: "white"; visible: window.tabsVisible; z: 100
        Rectangle { width: parent.width; height: 2; color: "black" }
        Row {
            anchors.fill: parent
            Repeater {
                model: ["首页", "AI 日报", "Agent", "微信读书"]
                Item {
                    required property string modelData
                    required property int index
                    width: window.width / 4; height: window.tabHeight
                    Rectangle { visible: index === ["clock","news","agent","reader"].indexOf(window.mode); x: 28; width: parent.width - 56; height: 8; color: "black" }
                    Text { anchors.horizontalCenter: parent.horizontalCenter; y: 44
                        text: modelData; font.family: uiFontFamily; font.pixelSize: 28; font.bold: index === ["clock","news","agent","reader"].indexOf(window.mode); color: index === ["clock","news","agent","reader"].indexOf(window.mode) ? "black" : "#555555" }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            if (index < 3) { window.mode = ["clock","news","agent"][index]; return }
                            window.mode = "reader"
                            // The reading tab returns to the shelf from a book or the offline library.
                            window.offlinePanelVisible = false
                            if (window.reading) { reader.hideTools(); appController.goBackToShelf() }
                        }
                    }
                }
            }
        }
    }
    NativeNotice { anchors.fill: parent; z: 1000 }
}
