// input: synthetic public-domain book; output: deterministic Library/Notes substitute; pos: reader UI integration fixture.
import QtQuick
QtObject {
    property string currentPage: "login"
    property string currentBookId: "test-book"
    property string currentBookTitle: "海边的一天"
    property string currentChapterTitle: "第一章 · 出发"
    property bool chapterLoading: false
    property bool hasReadableChapter: false
    property string chapterLoadError: ""
    property string chapterListLoadError: ""
    property bool isLoading: false
    property bool offlineMode: false
    property bool isOffline: false
    property bool loginExpired: false
    property var shelfBooks: [{bookId:"test-book",title:"海边的一天",author:"公开测试材料",format:"epub",version:1}]
    property var offlineBooks: ({"test-book": {done:2,total:2,denied:0,size:"24 KB"}})
    property string downloadingBook: ""
    property string cacheSize: "24 KB"
    property string downloadStatus: "离线章节已保存"
    property string qrUrl: ""
    property string loginStatusMessage: ""
    property bool qrCodeExpired: false
    property bool qrCodeLoading: false
    property bool loginHasError: false
    property string errorMessage: ""
    property bool policyDocumentLoading: false
    property string policyDocumentError: ""
    property string policyDocumentHtml: "<h1>测试协议</h1><p>公开测试材料。</p>"
    property var chapterList: [{chapterUid:1,title:"第一章 · 出发"},{chapterUid:2,title:"第二章 · 归来"}]
    property var cloudProgress: ({})
    property string syncStatus: "离线，进度已保留"
    property int resumeBlock: 14
    property int uid: 1
    property bool atEnd: false
    property bool positionDirty: false
    property int savedCursorCount: 0
    property int savedPositionCount: 0
    property int savedLayoutCount: 0
    property bool restored: false
    property bool exited: false
    property var lastPosition: ({})
    property var layout: ({})
    property var blocks: []
    property var chapterMarks: [{text:"海风吹过长长的堤岸",range:"12-24",count:8,mine:true,thought:"记住海的声音"}]
    property var chapterThoughts: [{author:"读者",quote:"海风吹过长长的堤岸",content:"想去海边走走",range:"12-24"}]
    property var myNotes: [{chapter:"第一章",text:"海风吹过长长的堤岸",thought:"记住海的声音"}]
    property var bookReviews: []
    property var bookInfo: ({title:"海边的一天",author:"公开测试材料",intro:"用于验证自主阅读界面的虚构短篇。"})
    property bool loading: false
    property string status: "离线笔记"
    property var reviews: [{author:"读者",content:"看到这一句，耳边仿佛响起了海浪。",likes:12}]
    property string reviewRange: ""
    property string reviewSort: "likes"
    property string reviewStatus: ""
    property int reviewTotal: 1
    property bool reviewsLoading: false
    property bool reviewsHasMore: false
    signal pageChanged()
    signal currentBlocksChanged()
    signal chapterContentLoaded()
    signal changed()
    function refreshQrCode() { loginStatusMessage = "请使用微信扫码登录" }
    function cancelPolicyDocument() {}
    function loadPolicyDocument(url) {}
    function refreshShelf() {}
    function logout() { currentPage = "login"; pageChanged() }
    function exitApp() { exited = true }
    function setOfflineMode(value) { offlineMode = value }
    function downloadBook(id) {}
    function pauseDownload() {}
    function removeDownload(id) {}
    function synchronizeProgress() {}
    function openBook(id,title,format,version,soldOut,payType,maxFree) { beginReading() }
    function beginReading() { currentPage = "reader"; pageChanged(); loadChapter(1) }
    function goBackToShelf() { currentPage = "bookshelf"; pageChanged() }
    function makeContent() {
        let html = "<html><body><p>海风吹过长长的堤岸</p>"
        for (let i=0; i<100; ++i) html += "<p>" + (i === 35 ? "可恢复的云端位置。" : "") + "第" + i + "段。清晨" + i + "的海岸铺满阳光" + i + "，沿着石阶" + i + "向远处走去" + i + "，潮水退去后" + i + "留下细细的纹路" + i + "。把脚步放慢" + i + "，听海浪一次次" + i + "拍打堤岸。</p>"
        return [{richHtml: html + "</body></html>",baseUrl:"file:///tmp/reader-ui-fixture.xhtml"}]
    }
    function loadChapter(value) {
        chapterLoading = true; currentBlocksChanged(); if (value !== uid) resumeBlock = 0; uid = value
        currentChapterTitle = value === 1 ? "第一章 · 出发" : "第二章 · 归来"
        blocks = makeContent(); hasReadableChapter = true; chapterLoading = false
        currentBlocksChanged(); chapterContentLoaded(); atEnd = false
    }
    function currentChapterUid() { return uid }
    function currentBlocks() { return blocks }
    function openChapterAtEnd() { return atEnd }
    function takeChapterPositionChange() { const value=positionDirty; positionDirty=false; return value }
    function resumeBlockIndex() { return resumeBlock }
    function resumeCharOffset() { return 0 }
    function progressRestored(ok) { restored=ok; cloudProgress=({}) }
    function saveReaderCursor(block,offset) { savedCursorCount++ }
    function saveProgressPosition(position) { savedPositionCount++; lastPosition=position }
    function readerLayoutSettings() { return layout }
    function saveReaderLayoutSettings(value) { layout=value; savedLayoutCount++ }
    function updateBookPagination(current,total,fingerprint) {}
    function nextChapter() { if (uid<2) { positionDirty=true; loadChapter(uid+1) } }
    function prevChapter() { if (uid>1) { positionDirty=true; atEnd=true; loadChapter(uid-1) } }
    function retryReaderLoad() { loadChapter(uid) }
    function reload() {}
    function loadReviews(range) { reviewRange=range }
    function loadMoreReviews() {}
    function closeReviews() {}
    function setReviewSort(value) { reviewSort=value }
}
