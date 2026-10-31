// input: chapter content, cursor, progress and notes; output: native paginated reading; pos: public reader book view.
import QtQuick
import remarkable_weread 1.0
Rectangle {
    id: reading
    objectName: "readingView"
    color: "white"
    signal exitRequested()
    property bool replacingContent: true
    property bool toolsOpen: false
    property string panel: ""
    function savePosition() {
        if (replacingContent || appController.chapterLoading || !appController.hasReadableChapter) return
        const cursor = page.localCursor()
        appController.saveReaderCursor(cursor.block, cursor.offset)
        appController.saveProgressPosition(page.progressPosition(cursor.block, cursor.offset))
    }
    function loadContent() {
        // atEnd is cleared immediately after chapterContentLoaded returns, so consume it synchronously.
        replacingContent = true
        const atEnd = appController.openChapterAtEnd(), cloud = appController.cloudProgress
        const moved = appController.takeChapterPositionChange()
        page.setMarks(bookNotes.chapterMarks)
        if (atEnd) page.setContentAtEnd(appController.currentBlocks())
        else page.setContent(appController.currentBlocks())
        if (cloud && Object.keys(cloud).length) {
            const restored = page.restoreProgress(cloud.summary || "", cloud.chapterOffset || 0)
            appController.progressRestored(restored)
        } else if (!atEnd) page.setCursor(appController.resumeBlockIndex(), appController.resumeCharOffset())
        replacingContent = false
        const cursor = page.localCursor()
        appController.saveReaderCursor(cursor.block, cursor.offset)
        if (moved) savePosition()
        pagination()
    }
    function pagination() { appController.updateBookPagination(page.currentPage, page.totalPages, page.layoutFingerprint()) }
    function turn(dir) {
        if (appController.chapterLoading || !appController.hasReadableChapter) return
        toolsOpen = false; panel = ""
        if (dir > 0 ? !page.nextPage() : !page.prevPage()) {
            savePosition()
            if (dir > 0) appController.nextChapter()
            else appController.prevChapter()
        }
    }
    function back() {
        savePosition(); toolsOpen = false; panel = ""
        appController.goBackToShelf()
    }
    function chooseChapter(uid) { savePosition(); panel = ""; toolsOpen = false; appController.loadChapter(uid) }
    function changeLayout(key, value) {
        const cursor = page.localCursor(), settings = page.layoutSettings()
        settings[key] = value; replacingContent = true
        page.applyLayoutSettings(settings); page.setCursor(cursor.block, cursor.offset)
        replacingContent = false
        appController.saveReaderLayoutSettings(page.layoutSettings())
        savePosition()
    }
    function showMark(range) {
        panel = "notes"; toolsOpen = true
        const mark = bookNotes.chapterMarks.find(m => m.range === range)
        Qt.callLater(function() {
            if (reading.panel === "notes") notes.open(mark || {range: range, text: "本段划线", count: 0})
        })
    }
    Component.onCompleted: { page.applyLayoutSettings(appController.readerLayoutSettings()); if (appController.hasReadableChapter) loadContent() }
    Connections {
        target: appController
        function onChapterContentLoaded() { reading.loadContent() }
        function onCurrentBlocksChanged() { if (appController.chapterLoading) reading.replacingContent = true }
        function onPageChanged() { reading.toolsOpen = false; reading.panel = "" }
    }
    Connections { target: bookNotes; function onChanged() { page.setMarks(bookNotes.chapterMarks) } }
    Shortcut { sequence: "PgDown"; enabled: reading.visible && reading.panel === ""; autoRepeat: false; onActivated: reading.turn(1) }
    Shortcut { sequence: "PgUp"; enabled: reading.visible && reading.panel === ""; autoRepeat: false; onActivated: reading.turn(-1) }
    Text {
        x: 48; y: 26; width: parent.width - 96; elide: Text.ElideRight
        text: appController.currentChapterTitle || appController.currentBookTitle
        font.family: uiFontFamily; font.pixelSize: 25; color: "#555555"
    }
    ReaderPageItem {
        textureSize: panelScale < 1 ? Qt.size(Math.round(width * panelScale), Math.round(height * panelScale)) : Qt.size(0, 0)
        id: page; objectName: "readerPage"
        x: 0; y: 86; width: parent.width; height: parent.height - 168
        onCursorChanged: function(blockIndex, charOffset) {
            if (reading.replacingContent || appController.chapterLoading) return
            appController.saveReaderCursor(blockIndex, charOffset)
            appController.saveProgressPosition(progressPosition(blockIndex, charOffset))
        }
        onCurrentPageChanged: reading.pagination()
        onTotalPagesChanged: reading.pagination()
        MouseArea {
            anchors.fill: parent
            onClicked: function(mouse) {
                const hit = page.hitTestAt(mouse.x, mouse.y)
                if (hit.type === ReaderPageItem.NOTE) { reading.showMark(hit.range); return }
                if (reading.toolsOpen) { reading.toolsOpen = false; reading.panel = "" }
                else if (mouse.x < width * .3) reading.turn(-1)
                else if (mouse.x > width * .7) reading.turn(1)
                else reading.toolsOpen = true
            }
        }
    }
    Text { x: 48; y: parent.height - 58; width: parent.width - 300; elide: Text.ElideRight
        text: appController.syncStatus; font.family: uiFontFamily; font.pixelSize: 22; color: "#777777" }
    Text { anchors.right: parent.right; anchors.rightMargin: 48; y: parent.height - 60
        text: (page.totalPages ? page.currentPage + 1 : 0) + " / " + page.totalPages
        font.family: uiFontFamily; font.pixelSize: 24; color: "#555555" }
    Rectangle {
        anchors.fill: page; visible: appController.chapterLoading || !appController.hasReadableChapter; color: "white"
        Column {
            anchors.centerIn: parent; width: parent.width - 120; spacing: 40
            Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap; font.pixelSize: 30
                text: appController.chapterLoading ? "正在加载章节…" : appController.chapterLoadError || appController.chapterListLoadError || "本章暂无可读内容" }
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: "‹ 返回书架"; font.pixelSize: 28
                MouseArea { anchors.fill: parent; anchors.margins: -24; onClicked: reading.back() } }
            Text { anchors.horizontalCenter: parent.horizontalCenter; visible: !appController.chapterLoading && appController.chapterList.length > 0; text: "重试"; font.pixelSize: 28
                MouseArea { anchors.fill: parent; anchors.margins: -24; onClicked: appController.retryReaderLoad() } }
        }
    }
    Rectangle {
        x: 0; y: 0; width: parent.width; height: 108; visible: reading.toolsOpen; color: "white"; border.color: "black"; z: 3
        Text { x: 48; anchors.verticalCenter: parent.verticalCenter; text: "‹ 书架"; font.pixelSize: 30
            MouseArea { anchors.fill: parent; anchors.margins: -24; onClicked: reading.back() } }
        Text { x: 220; anchors.verticalCenter: parent.verticalCenter; width: parent.width - 430; elide: Text.ElideRight
            text: appController.currentBookTitle; font.pixelSize: 30; font.bold: true }
        Text { anchors.right: parent.right; anchors.rightMargin: 48; anchors.verticalCenter: parent.verticalCenter; text: "退出"; font.pixelSize: 28
            MouseArea { anchors.fill: parent; anchors.margins: -24; onClicked: reading.exitRequested() } }
    }
    ReaderControls {
        id: controls; x: 0; y: 380; width: parent.width; height: parent.height - y - 108
        visible: reading.toolsOpen && (reading.panel === "toc" || reading.panel === "layout")
        panel: reading.panel; readerItem: page
        onChapterRequested: function(uid) { reading.chooseChapter(uid) }
        onLayoutRequested: function(key, value) { reading.changeLayout(key, value) }
    }
    NotesPanel { id: notes; x: 0; y: 300; width: parent.width; height: parent.height - y - 108
        visible: reading.toolsOpen && reading.panel === "notes" }
    Rectangle {
        anchors.bottom: parent.bottom; width: parent.width; height: 108; visible: reading.toolsOpen; color: "white"; border.color: "black"
        Row {
            anchors.fill: parent
            Repeater {
                model: [{label: "目录", value: "toc"}, {label: "字号", value: "layout"}, {label: "划线与想法", value: "notes"}, {label: "收起", value: ""}]
                Item {
                    required property var modelData
                    width: reading.width / 4; height: 108
                    Text { anchors.centerIn: parent; text: modelData.label; font.pixelSize: 28; font.bold: reading.panel === modelData.value }
                    MouseArea { anchors.fill: parent; onClicked: { reading.panel = modelData.value; if (!modelData.value) reading.toolsOpen = false } }
                }
            }
        }
    }
}
