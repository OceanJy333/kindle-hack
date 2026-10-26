// input: chapter list and Reader layout; output: tap-friendly directory/settings; pos: standalone reading controls.
import QtQuick
Rectangle {
    id: controls
    color: "white"; border.width: 2; border.color: "black"
    property string panel: ""
    property var readerItem
    property int tocPage: 0
    readonly property int tocRows: Math.max(1, Math.floor((height - 180) / 86))
    readonly property int tocPages: Math.max(1, Math.ceil(appController.chapterList.length / tocRows))
    signal chapterRequested(int uid)
    signal layoutRequested(string key, var value)
    onPanelChanged: {
        if (panel !== "toc") return
        const index = appController.chapterList.findIndex(c => c.chapterUid === appController.currentChapterUid())
        tocPage = Math.max(0, Math.floor(index / tocRows))
    }
    function turn(dir) { tocPage = Math.max(0, Math.min(tocPages - 1, tocPage + dir)) }
    Shortcut { sequence: "PgDown"; enabled: controls.visible && controls.panel === "toc"; autoRepeat: false; onActivated: controls.turn(1) }
    Shortcut { sequence: "PgUp"; enabled: controls.visible && controls.panel === "toc"; autoRepeat: false; onActivated: controls.turn(-1) }
    MouseArea { anchors.fill: parent }
    component Action: Rectangle {
        property string label
        signal activated()
        width: 140; height: 76; border.width: 2; border.color: "black"; radius: 8; color: "white"
        Text { anchors.centerIn: parent; text: parent.label; font.family: uiFontFamily; font.pixelSize: 28 }
        MouseArea { anchors.fill: parent; onClicked: parent.activated() }
    }
    Text { x: 48; y: 28; text: controls.panel === "toc" ? "目录" : "阅读排版"; font.family: uiFontFamily; font.pixelSize: 34; font.bold: true }
    Column {
        x: 48; y: 100; width: parent.width - 96; visible: controls.panel === "toc"
        Repeater {
            model: appController.chapterList.slice(controls.tocPage * controls.tocRows, (controls.tocPage + 1) * controls.tocRows)
            Rectangle {
                required property var modelData
                width: parent.width; height: 86; color: "white"
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#bbbbbb" }
                Text { anchors.verticalCenter: parent.verticalCenter; width: parent.width; text: modelData.title; elide: Text.ElideRight
                    font.family: uiFontFamily; font.pixelSize: 28; font.bold: modelData.chapterUid === appController.currentChapterUid() }
                MouseArea { anchors.fill: parent; onClicked: controls.chapterRequested(modelData.chapterUid) }
            }
        }
    }
    Row {
        visible: controls.panel === "toc"; anchors.bottom: parent.bottom; anchors.bottomMargin: 14; anchors.horizontalCenter: parent.horizontalCenter; spacing: 32
        Action { width: 120; label: "‹"; onActivated: controls.turn(-1) }
        Text { width: 160; height: 76; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
            text: (controls.tocPage + 1) + " / " + controls.tocPages; font.pixelSize: 26 }
        Action { width: 120; label: "›"; onActivated: controls.turn(1) }
    }
    Column {
        x: 48; y: 130; width: parent.width - 96; spacing: 56; visible: controls.panel === "layout"
        Row { spacing: 32
            Text { width: 220; height: 76; verticalAlignment: Text.AlignVCenter; text: "字号 " + controls.readerItem.fontSize; font.pixelSize: 28 }
            Action { label: "A −"; onActivated: controls.layoutRequested("fontSize", controls.readerItem.fontSize - 2) }
            Action { label: "A +"; onActivated: controls.layoutRequested("fontSize", controls.readerItem.fontSize + 2) }
        }
        Row { spacing: 32
            Text { width: 220; height: 76; verticalAlignment: Text.AlignVCenter; text: "行距 " + controls.readerItem.lineHeight.toFixed(2); font.pixelSize: 28 }
            Action { label: "−"; onActivated: controls.layoutRequested("lineHeight", controls.readerItem.lineHeight - .1) }
            Action { label: "+"; onActivated: controls.layoutRequested("lineHeight", controls.readerItem.lineHeight + .1) }
        }
        Row { spacing: 32
            Text { width: 220; height: 76; verticalAlignment: Text.AlignVCenter; text: "页边距 " + controls.readerItem.pageMargin; font.pixelSize: 28 }
            Action { label: "−"; onActivated: controls.layoutRequested("pageMargin", controls.readerItem.pageMargin - 8) }
            Action { label: "+"; onActivated: controls.layoutRequested("pageMargin", controls.readerItem.pageMargin + 8) }
        }
        Row { spacing: 32
            Text { width: 220; height: 76; verticalAlignment: Text.AlignVCenter; text: "首行缩进"; font.pixelSize: 28 }
            Action { width: 312; label: controls.readerItem.firstLineIndent ? "已开启" : "已关闭"
                onActivated: controls.layoutRequested("firstLineIndent", !controls.readerItem.firstLineIndent) }
        }
    }
}
