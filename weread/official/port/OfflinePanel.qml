// input: native offline service; output: per-book download management; pos: Voyage offline library UI.
import QtQuick
Rectangle {
    id: panel
    color: "white"
    property string confirmDelete: ""
    property int page: 0
    // Taps arrive as press/release pairs, so the list pages instead of scrolling.
    readonly property int rows: Math.floor((height - 236 - 92) / 124)
    // Rows are rebuilt only when the grouping changes, not on every progress update; progress text binds per row.
    readonly property string orderKey: appController.downloadingBook + "|" + Object.keys(appController.offlineBooks).sort().join(",")
                                       + "|" + appController.shelfBooks.length
    property var ordered: []
    function reorder() {
        const all = appController.shelfBooks.slice(), info = appController.offlineBooks, active = appController.downloadingBook
        const rank = b => String(b.bookId) === active ? 0 : info[String(b.bookId)] !== undefined ? 1 : 2
        ordered = all.map((b, i) => ({ b: b, i: i })).sort((x, y) => rank(x.b) - rank(y.b) || x.i - y.i).map(x => x.b)
    }
    onOrderKeyChanged: reorder()
    Component.onCompleted: reorder()
    readonly property var visibleBooks: ordered.slice(Math.min(page, pages - 1) * rows, (Math.min(page, pages - 1) + 1) * rows)
    readonly property int pages: Math.max(1, Math.ceil(ordered.length / Math.max(1, rows)))
    signal close()
    Shortcut { sequence: "PgDown"; enabled: panel.visible; autoRepeat: false; onActivated: panel.page = Math.min(panel.pages - 1, panel.page + 1) }
    Shortcut { sequence: "PgUp"; enabled: panel.visible; autoRepeat: false; onActivated: panel.page = Math.max(0, panel.page - 1) }
    component Action: Rectangle {
        property string label
        property bool primary: false
        signal activated()
        width: 168; height: 68; radius: 8
        color: area.pressed ? "#888888" : primary ? "black" : "white"
        border.color: "black"; border.width: primary ? 0 : 2
        Text { anchors.centerIn: parent; text: parent.label; font.pixelSize: 26; font.bold: true; color: parent.primary ? "white" : "black" }
        MouseArea { id: area; anchors.fill: parent; anchors.margins: -10; onClicked: parent.activated() }
    }
    Text { x: 56; y: 34; text: "离线书库"; font.pixelSize: 44; font.bold: true }
    Text { anchors.right: parent.right; anchors.rightMargin: 56; y: 50; text: "本机 " + appController.cacheSize; font.pixelSize: 26; color: "#555555" }
    Rectangle { x: 56; y: 120; width: parent.width - 112; height: 2; color: "black" }
    Text { x: 56; y: 150; text: "‹ 书架"; font.pixelSize: 28; font.bold: true
        MouseArea { anchors.fill: parent; anchors.margins: -24; onClicked: { panel.confirmDelete = ""; panel.close() } } }
    // Offline reading switch: on = never touch the network, read only what is on the device.
    Item {
        anchors.right: parent.right; anchors.rightMargin: 56; y: 140; width: 420; height: 64
        Text { anchors.right: toggle.left; anchors.rightMargin: 20; anchors.verticalCenter: parent.verticalCenter
            text: appController.offlineMode ? "离线阅读 · 只读本机" : "离线阅读"; font.pixelSize: 26; color: "#222222" }
        Rectangle { id: toggle; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
            width: 96; height: 52; radius: 26; border.width: 2; border.color: "black"; color: appController.offlineMode ? "black" : "white"
            Rectangle { width: 40; height: 40; radius: 20; y: 6; x: appController.offlineMode ? 50 : 6
                color: appController.offlineMode ? "white" : "black" } }
        MouseArea { anchors.fill: parent; anchors.margins: -12; onClicked: appController.setOfflineMode(!appController.offlineMode) }
    }
    Column {
        x: 56; y: 236; width: parent.width - 112
        Repeater {
        model: panel.visibleBooks
        delegate: Item {
            required property var modelData
            readonly property string bookId: String(modelData.bookId)
            readonly property var info: appController.offlineBooks[bookId]
            readonly property bool active: appController.downloadingBook === bookId
            readonly property bool complete: info !== undefined && info.total > 0 && info.done + info.denied >= info.total
            width: parent.width; height: 124
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#bbbbbb" }
            Text { x: 0; y: 22; width: parent.width - 220; text: modelData.title; elide: Text.ElideRight; font.pixelSize: 30 }
            Text { x: 0; y: 72; width: parent.width - 220; elide: Text.ElideRight; font.pixelSize: 23; color: active ? "black" : "#777777"
                text: info === undefined ? "未下载"
                    : (active ? "下载中 " : complete ? "已下载 · " : "部分下载 ") + info.done + " / " + info.total + " 章 · " + info.size
                      + (info.denied > 0 && !active ? " · " + info.denied + " 章需购买" : "") }
            // Progress line under the active row: the only moving element while a download runs.
            Rectangle { visible: active && info !== undefined && info.total > 0; anchors.bottom: parent.bottom; height: 4; color: "black"
                width: info && info.total ? parent.width * info.done / info.total : 0 }
            Row {
                anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; spacing: 16
                Action { visible: info !== undefined && !active; width: panel.confirmDelete === bookId ? 176 : 112
                    label: panel.confirmDelete === bookId ? "确认删除" : "删除"; primary: panel.confirmDelete === bookId
                    onActivated: { if (panel.confirmDelete === bookId) { panel.confirmDelete = ""; appController.removeDownload(bookId) } else panel.confirmDelete = bookId } }
                Action { visible: panel.confirmDelete !== bookId && (active || !complete)
                    label: active ? "暂停" : info === undefined ? "下载" : "继续"; primary: !active && !appController.offlineMode
                    opacity: appController.offlineMode && !active ? 0.4 : 1
                    onActivated: { panel.confirmDelete = ""; if (active) appController.pauseDownload(); else if (!appController.offlineMode) appController.downloadBook(bookId) } }
            }
        }
        }
    }
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 92; color: "white"
        Row { anchors.right: parent.right; anchors.rightMargin: 56; anchors.verticalCenter: parent.verticalCenter; spacing: 16
            Action { width: 72; label: "‹"; onActivated: panel.page = Math.max(0, panel.page - 1) }
            Text { width: 96; height: 68; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                text: (Math.min(panel.page, panel.pages - 1) + 1) + " / " + panel.pages; font.pixelSize: 24; color: "#555555" }
            Action { width: 72; label: "›"; onActivated: panel.page = Math.min(panel.pages - 1, panel.page + 1) } }
        Text { x: 56; anchors.verticalCenter: parent.verticalCenter; width: parent.width - 112 - 300; elide: Text.ElideRight; font.pixelSize: 24; color: "#555555"
            text: appController.offlineMode ? "离线阅读已开启：不联网，下载前请先关闭"
                : appController.downloadStatus || "下载后可断网阅读；需购买的章节不会下载" }
    }
}
