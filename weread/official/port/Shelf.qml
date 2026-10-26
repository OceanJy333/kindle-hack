// input: shelf, offline and session state; output: paged Voyage bookshelf with continue-reading hero; pos: reader-tab home page.
import QtQuick
Rectangle {
    id: shelf
    color: "white"
    signal offlineRequested()
    signal exitRequested()
    property int page: 0
    property int turns: 0
    property bool accountOpen: false
    readonly property int margin: 56
    readonly property int columns: 5
    readonly property int rows: 2
    readonly property int gap: 24
    readonly property real cellWidth: (width - margin * 2 - gap * (columns - 1)) / columns
    readonly property var books: appController.shelfBooks
    // The hero is the book read last: the one just closed, else the newest read time.
    readonly property var hero: {
        if (!books.length) return null
        let best = books[0]
        for (const b of books) {
            if (String(b.bookId) === appController.currentBookId) return b
            if ((b.readUpdateTime || 0) > (best.readUpdateTime || 0)) best = b
        }
        return best
    }
    readonly property var rest: books.filter(b => !hero || b.bookId !== hero.bookId)
    readonly property int perPage: columns * rows
    readonly property int pages: Math.max(1, Math.ceil(rest.length / perPage))
    readonly property int current: Math.min(page, pages - 1)
    function open(b) {
        appController.openBook(b.bookId, b.title, b.format || "txt", b.version || 0, b.soldOut || false, b.payType || 0, b.maxFreeChapter || 0)
    }
    function cover(b) {
        if (!b) return ""
        if (b.coverPath) return "file://" + b.coverPath
        return b.coverUrl ? String(b.coverUrl).replace("/s_", "/t6_") : ""
    }
    function offlineLabel(b) {
        const id = String(b.bookId), info = appController.offlineBooks[id]
        if (appController.downloadingBook === id) return info && info.total ? "下载中 " + info.done + "/" + info.total : "下载中"
        if (info === undefined) return ""
        return info.total > 0 && info.done + info.denied >= info.total ? "已下载" : "部分下载"
    }
    function turn(dir) {
        const next = Math.max(0, Math.min(pages - 1, current + dir))
        if (next === current) return
        page = next
        // Covers leave the most ghosting on e-ink; clean every fourth page turn.
        if (++turns % 4 === 0) eink.clearGhosting()
    }
    Shortcut { sequence: "PgDown"; enabled: shelf.visible; autoRepeat: false; onActivated: shelf.turn(1) }
    Shortcut { sequence: "PgUp"; enabled: shelf.visible; autoRepeat: false; onActivated: shelf.turn(-1) }

    component Cover: Rectangle {
        property var book
        color: "#eeeeee"; clip: true
        Image { id: image; anchors.fill: parent; fillMode: Image.PreserveAspectCrop; asynchronous: true; source: shelf.cover(parent.book) }
        Text { anchors.centerIn: parent; visible: image.status !== Image.Ready; text: parent.book ? String(parent.book.title).substring(0, 2) : ""
            font.family: uiFontFamily; font.pixelSize: 30; color: "#999999" }
        Rectangle { anchors.fill: parent; color: "transparent"; border.color: "#888888"; border.width: 1 }
    }
    component Button: Rectangle {
        property string label
        property bool primary: false
        signal activated()
        height: 72; radius: 8; color: primary ? "black" : "white"; border.color: "black"; border.width: primary ? 0 : 2
        Text { anchors.centerIn: parent; text: parent.label; font.family: uiFontFamily; font.pixelSize: 28; font.weight: Font.Medium; color: parent.primary ? "white" : "black" }
        MouseArea { anchors.fill: parent; anchors.margins: -8; onClicked: parent.activated() }
    }

    // Header: same height and rule as the desk pages.
    Text { x: shelf.margin; y: 32; text: "微信读书"; font.family: uiFontFamily; font.pixelSize: 44; font.bold: true }
    Row {
        anchors.right: parent.right; anchors.rightMargin: shelf.margin; y: 40; spacing: 32
        Text { text: appController.isLoading ? "同步中…" : appController.offlineMode ? "离线阅读中" : appController.isOffline ? "未联网 · 本机书架" : books.length + " 本"
            visible: !appController.loginExpired || appController.offlineMode
            font.family: uiFontFamily; font.pixelSize: 26; color: "#555555"; anchors.verticalCenter: parent.verticalCenter }
        // Expired session: the shelf still shows local books; one tap goes to the QR login.
        Rectangle {
            visible: appController.loginExpired && !appController.offlineMode; anchors.verticalCenter: parent.verticalCenter
            width: expiredLabel.implicitWidth + 32; height: 52; radius: 8; color: "black"
            Text { id: expiredLabel; anchors.centerIn: parent; text: "登录已过期 · 重新扫码"; font.family: uiFontFamily; font.pixelSize: 24; font.weight: Font.Medium; color: "white" }
            MouseArea { anchors.fill: parent; anchors.margins: -12; onClicked: appController.logout() }
        }
        Text { text: "刷新"; font.family: uiFontFamily; font.pixelSize: 28; font.weight: Font.Medium
            color: appController.isLoading || appController.offlineMode ? "#aaaaaa" : "black"; anchors.verticalCenter: parent.verticalCenter
            MouseArea { anchors.fill: parent; anchors.margins: -20; enabled: !appController.isLoading && !appController.offlineMode; onClicked: appController.refreshShelf() } }
        Text { text: "账号"; font.family: uiFontFamily; font.pixelSize: 28; font.weight: Font.Medium; anchors.verticalCenter: parent.verticalCenter
            MouseArea { anchors.fill: parent; anchors.margins: -20; onClicked: shelf.accountOpen = !shelf.accountOpen } }
    }
    Rectangle { x: shelf.margin; y: 122; width: parent.width - shelf.margin * 2; height: 2; color: "black" }

    // Continue reading: one large target for the book most likely to be opened.
    Item {
        id: heroBox
        x: shelf.margin; y: 152; width: parent.width - shelf.margin * 2; height: 304; visible: shelf.hero !== null
        Cover { id: heroCover; width: 210; height: 304; book: shelf.hero
            MouseArea { anchors.fill: parent; onClicked: shelf.open(shelf.hero) } }
        Column {
            x: 246; width: parent.width - 246; spacing: 10
            Text { text: "继续阅读"; font.family: uiFontFamily; font.pixelSize: 24; color: "#555555" }
            Text { width: parent.width; text: shelf.hero ? shelf.hero.title : ""; wrapMode: Text.WrapAnywhere; maximumLineCount: 2; elide: Text.ElideRight
                font.family: uiFontFamily; font.pixelSize: 38; font.bold: true; lineHeight: 1.15 }
            Text { width: parent.width; text: shelf.hero ? (shelf.hero.author || "") : ""; elide: Text.ElideRight; font.family: uiFontFamily; font.pixelSize: 26; color: "#555555" }
            Text { text: shelf.hero ? (shelf.offlineLabel(shelf.hero) || "未下载 · 联网阅读") : ""; font.family: uiFontFamily; font.pixelSize: 24; color: "#777777" }
        }
        Row {
            x: 246; anchors.bottom: parent.bottom; spacing: 20
            Button { width: 240; label: "继续阅读"; primary: true; onActivated: shelf.open(shelf.hero) }
            Button {
                readonly property string offline: shelf.hero ? shelf.offlineLabel(shelf.hero) : ""
                width: 200; label: offline === "已下载" ? "离线书库" : offline.startsWith("下载中") ? "暂停下载" : offline === "部分下载" ? "继续下载" : "下载离线"
                onActivated: {
                    if (offline === "已下载") shelf.offlineRequested()
                    else if (offline.startsWith("下载中")) appController.pauseDownload()
                    else if (!appController.offlineMode) appController.downloadBook(String(shelf.hero.bookId))
                    else shelf.offlineRequested()
                }
            }
        }
    }

    // Section row: whole library on the left, offline library entry on the right.
    Rectangle { x: shelf.margin; y: 488; width: parent.width - shelf.margin * 2; height: 1; color: "#bbbbbb" }
    Text { x: shelf.margin; y: 512; text: "全部书籍"; font.family: uiFontFamily; font.pixelSize: 30; font.bold: true }
    Text {
        anchors.right: parent.right; anchors.rightMargin: shelf.margin; y: 516
        text: "离线书库 · " + Object.keys(appController.offlineBooks).length + " 本 · " + appController.cacheSize + "  ›"
        font.family: uiFontFamily; font.pixelSize: 26; font.weight: Font.Medium
        MouseArea { anchors.fill: parent; anchors.margins: -20; onClicked: shelf.offlineRequested() }
    }

    Grid {
        x: shelf.margin; y: 580; columns: shelf.columns; columnSpacing: shelf.gap; rowSpacing: 26
        Repeater {
            model: shelf.rest.slice(shelf.current * shelf.perPage, (shelf.current + 1) * shelf.perPage)
            Item {
                required property var modelData
                width: shelf.cellWidth; height: cover.height + 46
                Cover { id: cover; width: parent.width; height: Math.round(parent.width * 1.45); book: modelData }
                Rectangle {
                    readonly property string label: shelf.offlineLabel(modelData)
                    visible: label !== ""; x: 0; y: cover.height - height; height: 34; width: tag.implicitWidth + 20; color: "black"
                    Text { id: tag; anchors.centerIn: parent; text: parent.label; font.family: uiFontFamily; font.pixelSize: 20; color: "white" }
                }
                Text { y: cover.height + 8; width: parent.width; text: modelData.title; elide: Text.ElideRight
                    font.family: uiFontFamily; font.pixelSize: 24; font.weight: Font.Medium }
                MouseArea { anchors.fill: parent; onClicked: shelf.open(modelData) }
            }
        }
    }

    // Pager: buttons and PagePress both turn shelf pages.
    Row {
        anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: 24; spacing: 24
        Button { width: 120; label: "‹"; enabled: shelf.current > 0; opacity: enabled ? 1 : .3; onActivated: shelf.turn(-1) }
        Text { width: 160; height: 72; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
            text: (shelf.current + 1) + " / " + shelf.pages; font.family: uiFontFamily; font.pixelSize: 28; color: "#333333" }
        Button { width: 120; label: "›"; enabled: shelf.current < shelf.pages - 1; opacity: enabled ? 1 : .3; onActivated: shelf.turn(1) }
    }

    Text {
        anchors.centerIn: parent; visible: !books.length && !appController.isLoading
        text: appController.isOffline ? "网络异常，书架暂时为空" : "书架为空"; font.family: uiFontFamily; font.pixelSize: 30; color: "#555555"
    }

    // Account sheet: the only rarely used action, kept out of the main layout.
    Rectangle {
        visible: shelf.accountOpen; anchors.right: parent.right; anchors.rightMargin: shelf.margin; y: 112; width: 300; height: 288; z: 10
        color: "white"; border.color: "black"; border.width: 2; radius: 8
        Column {
            anchors.centerIn: parent; spacing: 16
            Button { width: 252; label: "退出登录"; onActivated: { shelf.accountOpen = false; appController.logout() } }
            Button { width: 252; label: "退出阅读器"; onActivated: { shelf.accountOpen = false; shelf.exitRequested() } }
            Button { width: 252; label: "取消"; onActivated: shelf.accountOpen = false }
        }
    }
}
