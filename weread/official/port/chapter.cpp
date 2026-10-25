// input: chapter selection; output: authorized native content; pos: recovered download / archive flow.
#include "library.h"
#include "archive.h"
#include "offline-store.h"
#include <QNetworkReply>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QFileInfo>
#include <QDir>
void Library::loadChapter(int uid) {
    QVariantMap selected;
    for (const auto &item : chapters) if (item.toMap().value("chapterUid").toInt() == uid) selected = item.toMap();
    if (selected.isEmpty()) return;
    chapterPositionDirty=!suppressChapterPosition && cloudAnchor.isEmpty();suppressChapterPosition=false;
    if(uid!=chapterUid) resumeBlock=resumeOffset=0;
    int generation = ++readGeneration; chapterUid = uid; chapterTitle = selected.value("title").toString();
    reading = true; readError.clear(); purchase = unavailable = false; emit currentBlocksChanged(); emit chapterLoadStateChanged();
    auto cached=OfflineStore::read(chapterCache(uid)+"/chapter.json").value("blocks").toArray().toVariantList();
    if(!cached.isEmpty()) {blocks=cached;reading=false;emit currentBlocksChanged();emit chapterLoadStateChanged();emit chapterContentLoaded();atEnd=false;qInfo("OFFLINE_CHAPTER %d",uid);return;}
    if(offlineMode) {blocks.clear();reading=false;readError="本章尚未下载，请联网下载后阅读";emit currentBlocksChanged();emit chapterLoadStateChanged();return;}
    auto req = apiRequest(chapterUrl({{"bookId",bookId},{"format",format},{"version",version}},uid),60000);
    auto reply = network.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation, selected] {
        if (generation == readGeneration) {
            int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (reply->error() != QNetworkReply::NoError) {
                readError = reply->errorString(); purchase = status == 402; reading = false; blocks.clear();
                emit currentBlocksChanged(); emit chapterLoadStateChanged();
            } else showChapter(reply->readAll(), reply->rawHeader("encryptkey"), selected);
            qInfo("CHAPTER HTTP=%d blocks=%d error=%s", status, int(blocks.size()), qPrintable(readError));
        }
        reply->deleteLater();
    });
}
void Library::showChapter(const QByteArray &archive, const QByteArray &key, const QVariantMap &chapter) {
    auto decodedKey = decodeChapterKey(key, sessionVid, readError);
    auto files = readError.isEmpty() ? unpackChapter(archive, decodedKey, readError) : QMap<QString, QByteArray>{};
    decodedKey.fill(0); blocks.clear();
    if(readError.isEmpty()) blocks=OfflineStore::storeChapter(chapterCache(chapterUid),files,chapter,readError);
    reading = false; emit currentBlocksChanged(); emit chapterLoadStateChanged();
    if (!blocks.isEmpty()) emit chapterContentLoaded(); atEnd = false;
}
