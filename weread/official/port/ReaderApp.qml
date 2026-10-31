// input: native Library/Notes services; output: standalone WeRead window; pos: public reader entry.
import QtQuick
import QtQuick.Window
Window {
    id: app
    width: panelWidth; height: panelHeight; visible: true; color: "white"
    title: "微信读书"
    property bool offlineOpen: false
    property bool exiting: false
    function exitReader(immediately) {
        if (exiting && immediately !== true) return
        if (appController.currentPage === "reader") reading.savePosition()
        exiting = true
        appController.exitApp(immediately === true)
    }
    // The layout is authored for 1072x1448; the stage scales it to the panel so text is rasterised at final size.
    Item {
        id: stage
        width: 1072; height: 1448; scale: panelScale; transformOrigin: Item.TopLeft
        LoginView { anchors.fill: parent; visible: appController.currentPage === "login"; onExitRequested: app.exitReader() }
        Shelf {
            anchors.fill: parent; visible: appController.currentPage === "bookshelf" && !app.offlineOpen
            onOfflineRequested: app.offlineOpen = true
            onExitRequested: app.exitReader()
        }
        OfflinePanel {
            anchors.fill: parent; visible: appController.currentPage === "bookshelf" && app.offlineOpen
            onClose: app.offlineOpen = false
        }
        ReadingView { id: reading; anchors.fill: parent; visible: appController.currentPage === "reader"; onExitRequested: app.exitReader() }
        Connections { target: appController; function onPageChanged() { app.offlineOpen = false } }
        Rectangle {
            anchors.fill: parent; visible: app.exiting; color: "white"; z: 100
            Text { anchors.centerIn: parent; text: "正在保存进度并退出…"; font.family: uiFontFamily; font.pixelSize: 32 }
        }
    }
    onClosing: function(close) { close.accepted = false; exitReader() }
}
