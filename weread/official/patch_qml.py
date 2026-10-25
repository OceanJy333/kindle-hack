# input: original QML and Voyage adapter; output: staged QML; pos: explicit packaging transform.
from pathlib import Path

def patch_reader(stage):
    path = Path(stage) / 'original/qml/pages/ReaderPage.qml'
    text = path.read_text()
    anchor = '    id: readerPage\n'
    assert text.count(anchor) == 1
    patch = (Path(__file__).parent / 'port/pagepress.qml.inc').read_text()
    text = text.replace(anchor, anchor + patch)
    error_anchor = '            mode: "error"\n            onRetry:'
    assert text.count(error_anchor) == 1
    text = text.replace(error_anchor, '            mode: "error"\n            customMessage: appController.offlineMode ? readerPage.readerLoadError : ""\n            onRetry:')
    # Immersive reading: the page owns the full screen; the toolbar and its panels rise above the
    # desk tab bar only while they are open, so the text area never relayouts.
    bar_anchor = '        id: bottomBar\n        anchors.left: parent.left\n        anchors.right: parent.right\n        anchors.bottom: parent.bottom\n'
    assert text.count(bar_anchor) == 1
    text = text.replace(bar_anchor, bar_anchor + '        anchors.bottomMargin: readerPage.toolbarInset\n')
    text = text.replace(anchor, anchor + '    property int toolbarInset: 0\n', 1)
    text = patch_progress(patch_notes(text))
    path.write_text(text)


def swap(text, old, new):
    assert text.count(old) == 1, old
    return text.replace(old, new)


# Notes: a third toolbar action opens NotesPanel; marks are drawn in the page and open the note bubble.
NOTES_STATE = '''    property bool notesVisible: false
    property string noteRange: ""
    onNotesVisibleChanged: {
        scheduleRealtimeInkConfig()
        if (notesVisible) { tocVisible = false; fontPanelVisible = false }
        schedulePanelRefresh(notesPanel, notesVisible ? 250 : 500)
    }
    Connections {
        target: bookNotes
        function onChanged() {
            readerItem.setMarks(bookNotes.chapterMarks)
            if (readerPage.noteVisible && readerPage.noteRange !== "")
                readerPage.noteText = bookNotes.noteFor(readerPage.noteRange)
        }
    }
'''
NOTES_BUTTON = '''            Item {
                width: tocVisible ? 0 : bottomBar.secondaryActionWidth
                height: parent.height
                visible: !tocVisible
                Rectangle { anchors.fill: parent; color: "#F2F2F2"; visible: notesArea.pressed }
                Text {
                    anchors.centerIn: parent
                    text: "笔记"
                    font.family: uiFontFamily
                    font.pixelSize: 32
                    font.bold: notesVisible
                    color: "#000000"
                }
                Rectangle { visible: notesVisible; anchors.horizontalCenter: parent.horizontalCenter; y: parent.height - 22; width: 64; height: 5; color: "#000000" }
                MouseArea {
                    id: notesArea
                    anchors.fill: parent
                    onClicked: notesVisible = !notesVisible
                }
            }

'''
NOTES_PANEL = '''    NotesPanel {
        id: notesPanel
        anchors.top: parent.top
        anchors.bottom: bottomBar.top
        anchors.left: parent.left
        anchors.right: parent.right
        z: 30
        visible: notesVisible
    }

'''


def patch_notes(text):
    text = text.replace('    id: readerPage\n', '    id: readerPage\n' + NOTES_STATE, 1)
    text = swap(text, 'toolsVisible || tocVisible || fontPanelVisible || noteVisible\n',
                'toolsVisible || tocVisible || fontPanelVisible || noteVisible || notesVisible\n')
    text = swap(text, '        || readerPage.fontPanelVisible\n\n', '        || readerPage.fontPanelVisible || readerPage.notesVisible\n\n')
    text = swap(text, '            readerPage.fontPanelVisible = false\n        }\n',
                '            readerPage.fontPanelVisible = false\n            readerPage.notesVisible = false\n        }\n')
    text = swap(text, '        tocVisible = false\n        fontPanelVisible = false\n    }\n',
                '        tocVisible = false\n        fontPanelVisible = false\n        notesVisible = false\n    }\n')
    text = swap(text, '(width - backActionWidth - backDivider.width) / 2', '(width - backActionWidth - backDivider.width) / 3')
    text = swap(text, '            // 目录页右侧上下按钮复用', NOTES_BUTTON + '            // 目录页右侧上下按钮复用')
    text = swap(text, '    Rectangle {\n        id: tocPanel\n', NOTES_PANEL + '    Rectangle {\n        id: tocPanel\n')
    text = swap(text, '                readerPage.noteText = hit.content\n',
                '                readerPage.noteRange = hit.range || ""\n'
                '                readerPage.noteText = hit.range ? bookNotes.noteFor(hit.range) : hit.content\n')
    text = swap(text, '                readerPage.noteVisible = true\n', '''                if (hit.range) {
                    readerPage.notesVisible = true
                    Qt.callLater(function() {
                        var mark = bookNotes.chapterMarks.find(function(item) { return item.range === hit.range })
                        if (mark) notesPanel.open(mark)
                    })
                } else readerPage.noteVisible = true
''')
    # Opening the TOC or font panel from the toolbar closes notes.
    text = swap(text, '                        fontPanelVisible = false\n                        if (tocVisible) {',
                '                        fontPanelVisible = false\n                        notesVisible = false\n                        if (tocVisible) {')
    text = swap(text, '                        tocVisible = false\n                        fontPanelVisible = !fontPanelVisible\n',
                '                        tocVisible = false\n                        notesVisible = false\n                        fontPanelVisible = !fontPanelVisible\n')
    return text


def patch_progress(text):
    text = swap(text, '            appController.saveReaderCursor(blockIndex, charOffset)', '''            if (!readerPage.replacingContent) {
                appController.saveReaderCursor(blockIndex, charOffset)
                appController.saveProgressPosition(readerItem.progressPosition(blockIndex, charOffset))
            }''')
    text = swap(text, '                readerItem.setCursor(appController.resumeBlockIndex(), appController.resumeCharOffset())', '''                var cloud = appController.cloudProgress
                if (cloud.chapterUid !== undefined) {
                    appController.progressRestored(readerItem.restoreProgress(cloud.summary || "", cloud.chapterOffset || 0))
                } else {
                    readerItem.setCursor(appController.resumeBlockIndex(), appController.resumeCharOffset())
                }''')
    text = swap(text, '            readerPage.replacingContent = false\n', '''            readerPage.replacingContent = false
            var local = readerItem.localCursor()
            appController.saveReaderCursor(local.block, local.offset)
            if (appController.takeChapterPositionChange())
                appController.saveProgressPosition(readerItem.progressPosition(local.block, local.offset))
''')
    text = swap(text, '    id: readerPage\n', '''    id: readerPage
    Text {
        anchors.top: parent.top
        anchors.topMargin: 70
        anchors.horizontalCenter: parent.horizontalCenter
        text: appController.syncStatus
        font.family: uiFontFamily
        font.pixelSize: 20
        color: "#333333"
        visible: readerPage.toolsVisible && !readerPage.notesVisible && !readerPage.tocVisible && !readerPage.fontPanelVisible
        z: 40
        MouseArea { anchors.fill: parent; anchors.margins: -12; onClicked: appController.synchronizeProgress() }
    }
''')
    return text
