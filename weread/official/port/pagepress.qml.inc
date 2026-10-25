    // Voyage adaptation: use the original turnPage path, including chapter boundaries.
    Shortcut {
        sequence: "PgDown"
        autoRepeat: false
        enabled: readerPage.visible && !readerPage.tocVisible && !readerPage.fontPanelVisible
                 && !appController.chapterLoading && !readerPage.replacingContent
        onActivated: pageTapArea.goNext()
    }
    Shortcut {
        sequence: "PgUp"
        autoRepeat: false
        enabled: readerPage.visible && !readerPage.tocVisible && !readerPage.fontPanelVisible
                 && !appController.chapterLoading && !readerPage.replacingContent
        onActivated: pageTapArea.goPrev()
    }
