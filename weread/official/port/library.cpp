// input: official shelf/TOC responses; output: QML data and persisted cursor; pos: recovered service flow.
#include <unistd.h>
#include "library.h"
#include "offline-store.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QSaveFile>
Library::Library(const QString &directory) : Auth(directory) {
    startProgress();
    connect(this, &Library::pageChanged, this, &Library::currentPageChanged);
    connect(this, &Auth::sessionReady, this, [this] { books = OfflineStore::read(accountCache()+"/shelf.json").value("books").toArray().toVariantList();
        offlineMode = OfflineStore::read(accountCache()+"/settings.json").value("offline").toBool();
        offline = offlineMode; applyLocalCovers(); page = "bookshelf"; emit pageChanged(); emit shelfBooksChanged(); emit downloadChanged(); if (!offlineMode) refreshShelf(); });
}
void Library::savePrivate(const QString &name, const QByteArray &bytes) {
    QSaveFile file(directory + "/" + name);
    if (file.open(QIODevice::WriteOnly)) {
        file.setPermissions(QFile::ReadOwner | QFile::WriteOwner); file.write(bytes); file.commit();
    }
}
void Library::refreshShelf() {
    if (busy || !loggedIn() || offlineMode) return;
    busy = true; error.clear(); emit shelfBooksChanged(); emit errorMessageChanged();
    const auto account=sessionVid;
    request("/shelf/sync?synckey=0&onlyBookid=1", {}, 30000, [this,account](QJsonObject data, QString message) {
        if(account!=sessionVid)return;
        if (!message.isEmpty()) {
            expiredLogin = message == "微信读书登录已过期"; offline = !expiredLogin;
            busy = false; error = message;
            emit isOfflineChanged(); emit errorMessageChanged(); emit shelfBooksChanged(); return;
        }
        QJsonArray ids;
        for (auto item : data.value("bookIds").toArray()) ids.append(item.toObject().value("bookId"));
        pendingBooks.clear(); details(ids);
    });
}
void Library::details(QJsonArray ids, int offset) {
    if(offlineMode){busy=false;emit shelfBooksChanged();return;}
    QJsonArray batch;
    for (int i = offset; i < qMin(offset + 20, int(ids.size())); ++i) batch.append(ids[i]);
    if (batch.isEmpty()) { books=pendingBooks;applyLocalCovers(); OfflineStore::write(accountCache()+"/shelf.json", QJsonDocument::fromVariant(QVariantMap{{"books",books}}).toJson()); busy = false; emit shelfBooksChanged(); return; }
    const auto account=sessionVid;
    request("/shelf/syncbook", {{"bookIds", batch}, {"albumIds", QJsonArray{}}}, 30000,
      [this, ids, offset, account](QJsonObject data, QString message) {
        if(account!=sessionVid)return;
        if (!message.isEmpty()) { busy = false; error = message; emit errorMessageChanged(); emit shelfBooksChanged(); return; }
        for (auto item : data.value("books").toArray()) {
            auto book = item.toObject().toVariantMap();
            book["coverUrl"] = book.value("cover"); book["soldOut"] = book.value("soldout");
            pendingBooks.append(book);
        }
        offline = false; expiredLogin = false; applyLocalCovers(); emit isOfflineChanged(); emit shelfBooksChanged();
        details(ids, offset + 20);
      }, true);
}
void Library::logout() {
    ++syncGeneration; syncBusy=false; pauseDownload(); ++readGeneration; cancelLogin(); sessionVid.clear(); accessToken.clear(); refreshToken.clear();
    QFile::remove(directory + "/session-port.json"); page = "login"; books.clear(); blocks.clear(); emit pageChanged();
}
void Library::exitApp(bool immediately) {
    // The process must end even if a network callback or the event loop stalls.
    if (!immediately) alarm(8);
    if (immediately) { QCoreApplication::quit(); return; }
    if (exitPending) return;
    exitPending = true;
    const bool alreadySyncing = syncBusy;
    if (!alreadySyncing) synchronizeProgress();
    if (!syncBusy) { QCoreApplication::quit(); return; }
    // A running upload may carry an older cursor. Finish it, then send the final pending position.
    auto timer = new QTimer(this);
    QElapsedTimer deadline; deadline.start();
    connect(timer, &QTimer::timeout, this, [this, deadline, finalSync = alreadySyncing]() mutable {
        if (deadline.hasExpired(5000)) { QCoreApplication::quit(); return; }
        if (syncBusy) return;
        if (finalSync) { finalSync = false; synchronizeProgress(); if (syncBusy) return; }
        QCoreApplication::quit();
    });
    timer->start(100);
}
void Library::goBackToShelf() { synchronizeProgress(); ++readGeneration; page = "bookshelf"; reading = false; emit pageChanged(); }
void Library::openBook(QString id, QString title, QString type, int bookVersion, bool, int, int) {
    ++readGeneration; int generation = readGeneration;
    bookId = id; bookTitle = title; format = type; version = bookVersion;
    page = "reader"; blocks.clear(); chapters.clear(); readError.clear(); tocError.clear();
    chapterTitle.clear(); reading = true; purchase = unavailable = false;
    emit pageChanged(); emit chapterListChanged(); emit currentBlocksChanged();
    auto cached = OfflineStore::read(bookCache(bookId,version)+"/toc.json").value("chapters").toArray().toVariantList();
    if(!cached.isEmpty()) {acceptChapters(cached);return;}
    if(offlineMode) {tocError="这本书尚未下载，请联网后下载";reading=false;emit chapterListChanged();emit currentBlocksChanged();return;}
    request("/book/chapterInfos", {{"bookIds", QJsonArray{id}}, {"synckeys", QJsonArray{0}}, {"updateTimes", QJsonArray{0}}},
      30000, [this, generation](QJsonObject data, QString message) {
        if (generation != readGeneration) return;
        if (!message.isEmpty()) { tocError = message; reading = false; emit chapterListChanged(); emit currentBlocksChanged(); return; }
        auto value=data.value("data").toArray().first().toObject().value("updated").toArray().toVariantList();
        OfflineStore::write(bookCache(bookId,version)+"/toc.json",QJsonDocument::fromVariant(QVariantMap{{"chapters",value}}).toJson());
        acceptChapters(value);
      }, true);
}
void Library::acceptChapters(QVariantList value) {
    chapters=value; emit chapterListChanged();
    if(chapters.isEmpty()) {tocError="没有可用章节";reading=false;emit chapterListChanged();emit currentBlocksChanged();return;}
    auto saved=OfflineStore::read(accountCache()+"/cursor-"+OfflineStore::hash(bookId)+".json");
    const auto ownerFile=directory+"/legacy-cursor-owner.json";
    auto owner=OfflineStore::read(ownerFile).value("account").toString();
    if(owner.isEmpty()) {owner=OfflineStore::hash(sessionVid);OfflineStore::write(ownerFile,QJsonDocument(QJsonObject{{"account",owner}}).toJson());}
    if(saved.isEmpty()&&owner==OfflineStore::hash(sessionVid)) saved=OfflineStore::read(directory+"/cursor-"+bookId+".json");
    int selected=chapters.first().toMap().value("chapterUid").toInt();
    for(auto item:chapters) if(item.toMap().value("chapterUid").toInt()==saved.value("chapterUid").toInt()) selected=saved.value("chapterUid").toInt();
    chapterUid=selected;
    resumeBlock=saved.value("block").toInt();resumeOffset=saved.value("offset").toInt();
    openProgress();
}

void Library::nextChapter() {
    for (int i = 0; i + 1 < chapters.size(); ++i) if (chapters[i].toMap().value("chapterUid").toInt() == chapterUid) {
        resumeBlock = resumeOffset = 0; loadChapter(chapters[i + 1].toMap().value("chapterUid").toInt()); return;
    }
}
void Library::prevChapter() {
    for (int i = 1; i < chapters.size(); ++i) if (chapters[i].toMap().value("chapterUid").toInt() == chapterUid) {
        atEnd = true; loadChapter(chapters[i - 1].toMap().value("chapterUid").toInt()); return;
    }
}
void Library::saveReaderCursor(int block, int offset) {
    if (reading || blocks.isEmpty()) return;
    OfflineStore::write(accountCache()+"/cursor-"+OfflineStore::hash(bookId)+".json", QJsonDocument(QJsonObject{{"chapterUid", chapterUid}, {"block", block}, {"offset", offset}}).toJson());
}
QVariantMap Library::readerLayoutSettings() const {
    QFile file(directory + "/layout.json"); if (!file.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(file.readAll()).object().toVariantMap();
}
void Library::saveReaderLayoutSettings(QVariantMap settings) { savePrivate("layout.json", QJsonDocument::fromVariant(settings).toJson()); }
void Library::updateBookPagination(int current, int total, QString) {
    currentPageNumber = current + 1; totalPages = total; emit paginationChanged();
}
