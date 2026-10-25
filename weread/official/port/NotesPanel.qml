// input: bookNotes (chapter marks and thoughts, my notes, book reviews and info); output: paged notes panel above the reader toolbar; pos: ReaderPage notes tab.
import QtQuick
Rectangle {
    id: panel
    color: "white"
    border.color: "#EAEAEA"; border.width: 1
    property int tab: 0
    property int page: 0
    property var detail: null          // a chapter mark opened to read its thoughts
    readonly property int margin: 40
    readonly property int rowHeight: 184
    readonly property int rows: Math.max(1, Math.floor((height - 120 - 96) / rowHeight))
    readonly property var marks: bookNotes.chapterMarks
    readonly property var source: tab === 0 ? marks : tab === 1 ? bookNotes.chapterThoughts : tab === 2 ? bookNotes.myNotes : []
    readonly property bool longText: tab === 3 || detail !== null
    readonly property int pages: longText ? Math.max(1, Math.ceil(scroller.contentHeight / Math.max(1, scroller.height)))
                                                     : Math.max(1, Math.ceil(source.length / rows))
    readonly property int current: Math.min(page, pages - 1)
    function select(next) { bookNotes.closeReviews(); tab = next; page = 0; detail = null; scroller.contentY = 0 }
    function open(mark) { detail = mark; page = 0; scroller.contentY = 0; bookNotes.loadReviews(mark.range) }
    function turn(dir) {
        page = Math.max(0, Math.min(pages - 1, current + dir))
        if (longText) scroller.contentY = Math.min(page * scroller.height, Math.max(0, scroller.contentHeight - scroller.height))
    }
    onVisibleChanged: { if (visible) { detail = null; page = 0 } else bookNotes.closeReviews() }
    Shortcut { sequence: "PgDown"; enabled: panel.visible; autoRepeat: false; onActivated: panel.turn(1) }
    Shortcut { sequence: "PgUp"; enabled: panel.visible; autoRepeat: false; onActivated: panel.turn(-1) }
    MouseArea { anchors.fill: parent }   // taps inside the panel never reach the page underneath

    // Tabs: chapter marks first, because they follow what is on screen.
    Row {
        x: panel.margin; y: 28; spacing: 36
        Repeater {
            model: ["本章划线 " + panel.marks.length, "本章想法 " + bookNotes.chapterThoughts.length,
                    "我的笔记 " + bookNotes.myNotes.length, "简介"]
            Text {
                required property string modelData
                required property int index
                text: modelData; font.family: uiFontFamily; font.pixelSize: 30
                font.bold: panel.tab === index; color: panel.tab === index ? "black" : "#666666"
                Rectangle { visible: panel.tab === index; anchors.top: parent.bottom; anchors.topMargin: 10; width: parent.width; height: 5; color: "black" }
                MouseArea { anchors.fill: parent; anchors.margins: -18; onClicked: panel.select(index) }
            }
        }
    }
    Text {
        anchors.right: parent.right; anchors.rightMargin: panel.margin; y: 36; width: 220; horizontalAlignment: Text.AlignRight
        text: bookNotes.loading ? "同步中…" : bookNotes.status; elide: Text.ElideLeft
        font.family: uiFontFamily; font.pixelSize: 22; color: "#777777"
    }
    Rectangle { x: panel.margin; y: 96; width: parent.width - panel.margin * 2; height: 2; color: "black" }

    Row {
        x: panel.margin; y: 116; spacing: 30
        visible: panel.detail !== null
        Repeater {
            model: [{label: "按赞排序", value: "likes"}, {label: "最新", value: "latest"}]
            Text {
                required property var modelData
                text: modelData.label; font.family: uiFontFamily; font.pixelSize: 26
                font.bold: bookNotes.reviewSort === modelData.value
                color: font.bold ? "black" : "#666666"
                Rectangle { anchors.top: parent.bottom; anchors.topMargin: 8; width: parent.width; height: 3; color: "black"; visible: parent.font.bold }
                MouseArea { anchors.fill: parent; anchors.margins: -14
                    onClicked: { bookNotes.setReviewSort(modelData.value); panel.page = 0; scroller.contentY = 0 } }
            }
        }
    }
    Text {
        anchors.right: parent.right; anchors.rightMargin: panel.margin; y: 118
        visible: panel.detail !== null
        text: bookNotes.reviewsLoading ? "加载评论 " + bookNotes.reviews.length + " / " + bookNotes.reviewTotal : bookNotes.reviewStatus ||
              (bookNotes.reviewsHasMore ? "已加载 " + bookNotes.reviews.length + " / " + bookNotes.reviewTotal + " 条" : bookNotes.reviews.length + " 条评论")
        font.family: uiFontFamily; font.pixelSize: 22; color: "#555555"
        MouseArea { anchors.fill: parent; anchors.margins: -12; enabled: !bookNotes.reviewsLoading
            onClicked: bookNotes.loadReviews(bookNotes.reviewRange) }
    }

    // List pages: chapter marks or my notes.
    Column {
        x: panel.margin; y: 116; width: parent.width - panel.margin * 2
        visible: !panel.longText
        Repeater {
            model: panel.source.slice(panel.current * panel.rows, (panel.current + 1) * panel.rows)
            Item {
                required property var modelData
                width: parent.width; height: panel.rowHeight
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#cccccc" }
                // Small grey line on top: chapter (my notes) or the quoted sentence (thoughts).
                Text {
                    visible: panel.tab !== 0; y: 16; width: parent.width; elide: Text.ElideRight
                    text: panel.tab === 1 ? (modelData.author || "读者") + " ·「" + (modelData.quote || "") + "」" : (modelData.chapter || "")
                    font.family: uiFontFamily; font.pixelSize: 22; color: "#777777"
                }
                Text {
                    y: panel.tab === 0 ? 20 : 50; width: parent.width - (panel.tab === 0 ? 40 : 0)
                    text: panel.tab === 1 ? modelData.content : (modelData.text || "")
                    wrapMode: Text.Wrap; elide: Text.ElideRight
                    maximumLineCount: 2
                    font.family: uiFontFamily; font.pixelSize: 28; lineHeight: 1.25; color: "#111111"
                }
                Text {
                    anchors.bottom: parent.bottom; anchors.bottomMargin: 16; width: parent.width; elide: Text.ElideRight
                    text: panel.tab === 0
                          ? (modelData.count > 0 ? modelData.count + " 人划线" : "我的划线") + (modelData.mine && modelData.count > 0 ? " · 我也划过" : "")
                            + (modelData.thought ? " · 有我的想法" : "")
                          : panel.tab === 1 ? ""
                          : (modelData.thought ? "想法：" + modelData.thought : "")
                    font.family: uiFontFamily; font.pixelSize: 23; color: panel.tab === 2 ? "#222222" : "#666666"
                }
                Text { visible: panel.tab === 0; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                    text: "›"; font.pixelSize: 40; color: "#888888" }
                MouseArea { anchors.fill: parent; enabled: panel.tab === 0; onClicked: panel.open(modelData) }
            }
        }
    }
    Text {
        anchors.centerIn: parent; visible: !panel.longText && !panel.source.length
        text: bookNotes.loading ? "正在同步…" : panel.tab === 0 ? "本章还没有热门划线" : panel.tab === 1 ? "本章还没有公开想法"
              : "这本书还没有你的划线和想法"
        font.family: uiFontFamily; font.pixelSize: 28; color: "#777777"
    }

    // Long text pages: a mark with its thoughts, or the book intro; page buttons move one screen.
    Flickable {
        id: scroller
        x: panel.margin; y: panel.detail ? 176 : 120; width: parent.width - panel.margin * 2; height: parent.height - y - 96
        visible: panel.longText; interactive: false; clip: true
        contentWidth: width; contentHeight: body.implicitHeight
        Column {
            id: body; width: scroller.width; spacing: 22
            Text { visible: panel.detail !== null; text: "‹ 返回本章划线"; font.family: uiFontFamily; font.pixelSize: 26; font.bold: true
                MouseArea { anchors.fill: parent; anchors.margins: -16; onClicked: { bookNotes.closeReviews(); panel.detail = null; panel.page = 0 } } }
            Text {
                width: parent.width; wrapMode: Text.Wrap; font.family: uiFontFamily; lineHeight: 1.35
                font.pixelSize: panel.detail ? 30 : 36; font.bold: !panel.detail
                text: panel.detail ? "「" + panel.detail.text + "」" : (bookNotes.bookInfo.title || appController.currentBookTitle)
            }
            Text {
                width: parent.width; wrapMode: Text.Wrap; font.family: uiFontFamily; font.pixelSize: 24; color: "#555555"
                text: {
                    if (panel.detail) return (panel.detail.count > 0 ? panel.detail.count + " 人划线" : "我的划线")
                                             + (panel.detail.thought ? "\n我的想法：" + panel.detail.thought : "")
                    const b = bookNotes.bookInfo, parts = [b.author, b.category, b.publisher].filter(x => x)
                    if (b.rating > 0) parts.push("推荐值 " + (b.rating / 10).toFixed(1) + "%" + (b.ratingCount ? " · " + b.ratingCount + " 人点评" : ""))
                    return parts.join("  ·  ")
                }
            }
            Rectangle { width: parent.width; height: 1; color: "#cccccc" }
            Text {
                visible: panel.detail === null; width: parent.width; wrapMode: Text.Wrap; lineHeight: 1.45
                text: bookNotes.bookInfo.intro || (bookNotes.loading ? "正在加载简介…" : "暂无简介")
                font.family: uiFontFamily; font.pixelSize: 28; color: "#111111"
            }
            Text {
                visible: panel.detail !== null && !bookNotes.reviews.length
                text: bookNotes.reviewsLoading ? "正在加载这段评论…" : bookNotes.reviewStatus || "这段还没有公开评论"
                font.family: uiFontFamily; font.pixelSize: 26; color: "#777777"
            }
            Text { visible: panel.detail === null && bookNotes.bookReviews.length > 0; text: "读者点评"
                font.family: uiFontFamily; font.pixelSize: 30; font.bold: true; topPadding: 16 }
            Repeater {
                model: panel.detail ? bookNotes.reviews : bookNotes.bookReviews
                Column {
                    required property var modelData
                    width: body.width; spacing: 8
                    Text { text: (modelData.author || "读者") + (panel.detail ? "  ·  " + modelData.likes + " 赞" : "") + (modelData.level ? "  · " + ["", "推荐", "一般", "不行"][modelData.level] : "")
                        font.family: uiFontFamily; font.pixelSize: 24; font.bold: true; color: "#333333" }
                    Text { width: parent.width; text: modelData.content; textFormat: Text.PlainText; wrapMode: Text.Wrap; lineHeight: 1.35
                        font.family: uiFontFamily; font.pixelSize: 28; color: "#111111" }
                    Rectangle { width: parent.width; height: 1; color: "#e0e0e0" }
                }
            }
            Text {
                visible: panel.detail !== null && bookNotes.reviewsHasMore
                text: bookNotes.reviewsLoading ? "加载中…" : "加载更多评论"
                font.family: uiFontFamily; font.pixelSize: 28; font.bold: true
                MouseArea { anchors.fill: parent; anchors.margins: -16; enabled: !bookNotes.reviewsLoading; onClicked: bookNotes.loadMoreReviews() }
            }
        }
    }

    // Pager shared by all views.
    Row {
        anchors.right: parent.right; anchors.rightMargin: panel.margin; anchors.bottom: parent.bottom; anchors.bottomMargin: 14; spacing: 16
        Repeater {
            model: ["‹", "", "›"]
            Rectangle {
                required property string modelData
                required property int index
                width: index === 1 ? 110 : 84; height: 68; radius: 8
                border.width: index === 1 ? 0 : 2; border.color: "black"
                opacity: index === 0 && panel.current === 0 || index === 2 && panel.current >= panel.pages - 1 ? .3 : 1
                Text { anchors.centerIn: parent; text: index === 1 ? (panel.current + 1) + " / " + panel.pages : modelData
                    font.family: uiFontFamily; font.pixelSize: index === 1 ? 24 : 34; color: index === 1 ? "#555555" : "black" }
                MouseArea { anchors.fill: parent; enabled: index !== 1; onClicked: panel.turn(index === 0 ? -1 : 1) }
            }
        }
    }
}
