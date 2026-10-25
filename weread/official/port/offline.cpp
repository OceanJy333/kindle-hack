// input: user download commands and authorized API responses; output: resumable local library; pos: offline orchestration.
#include "library.h"
#include "offline-store.h"
#include "archive.h"
#include <QJsonDocument>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QDir>
#include <QImage>
#include <QFileInfo>
#include <QThreadPool>
QString Library::accountCache() const {return qEnvironmentVariable("WEREAD_CACHE_DIR", "/mnt/us/weread-offline")+"/"+OfflineStore::hash(sessionVid);}
QString Library::bookCache(const QString &id,int v) const {return accountCache()+"/"+OfflineStore::hash(id)+"/"+QString::number(v);}
QString Library::chapterCache(int uid) const {return bookCache(bookId,version)+"/"+QString::number(uid);}
// Sum of the per-book sizes already measured by refreshOfflineInfo; QML reads this on every download notification.
QString Library::cacheSize() const {qint64 n=0;for(const auto &v:offlineInfo)n+=v.toMap().value("bytes").toLongLong();return QString::number(n/1048576.0,'f',1)+" MB";}
QUrl Library::chapterUrl(const QVariantMap &book,int uid) const {
    auto type=book.value("format").toString();bool epub=type.startsWith("epub")||type=="pdf";
    QUrl url("https://i.weread.qq.com/book/chapterdownload");QUrlQuery q;
    q.addQueryItem("bookId",book.value("bookId").toString());q.addQueryItem("chapters",(epub?"0,":"")+QString::number(uid));
    q.addQueryItem("pf","wechat_wx-2001-android-100-weread");q.addQueryItem("pfkey","pfKey");q.addQueryItem("zoneId","1");
    q.addQueryItem("bookVersion",book.value("version").toString());q.addQueryItem("bookType",epub?"epub":"txt");
    q.addQueryItem("release","1");q.addQueryItem("stopAutoPayWhenBNE","1");
    for(auto key:{"preload","preview","offline"}) q.addQueryItem(key,"0");url.setQuery(q);return url;
}
// Per-book state for the offline list: chapters on disk, chapters in the TOC, denied chapters and bytes.
// Only the book being downloaded is rescanned per chapter; a full scan happens on shelf changes.
void Library::refreshOfflineInfo(const QString &only) {
    for(const auto &value:books) {
        auto book=value.toMap();auto id=book.value("bookId").toString();
        if(!only.isEmpty()&&id!=only)continue;
        auto root=bookCache(id,book.value("version").toInt());
        int done=0;for(const auto &dir:QDir(root).entryList(QDir::Dirs|QDir::NoDotAndDotDot))if(QFileInfo::exists(root+"/"+dir+"/chapter.json"))++done;
        int total=OfflineStore::read(root+"/toc.json").value("chapters").toArray().size();
        if(!total&&!done){offlineInfo.remove(id);continue;}
        auto size=OfflineStore::bytes(accountCache()+"/"+OfflineStore::hash(id));
        offlineInfo[id]=QVariantMap{{"done",done},{"total",total},{"denied",OfflineStore::read(root+"/state.json").value("denied").toInt()},
            {"bytes",size},{"size",QString::number(size/1048576.0,'f',1)+" MB"}};
    }
}
void Library::applyLocalCovers() {
    for(auto &value:books) {auto book=value.toMap();auto path=bookCache(book.value("bookId").toString(),book.value("version").toInt())+"/cover.img";
        book["coverUrl"]=QFileInfo::exists(path)?QUrl::fromLocalFile(path).toString():(offlineMode?QString():book.value("cover").toString());value=book;}
    refreshOfflineInfo();emit shelfBooksChanged();emit downloadChanged();
}
void Library::setOfflineMode(bool value) {
    if(value) pauseDownload();offlineMode=value;offline=value;applyLocalCovers();
    OfflineStore::write(accountCache()+"/settings.json",QJsonDocument(QJsonObject{{"offline",value}}).toJson());
    emit downloadChanged();emit isOfflineChanged();
}
void Library::pauseDownload() {
    ++downloadGeneration;downloading=false;
    auto reply=downloadReply;downloadReply=nullptr;if(reply)reply->abort();
    downloadStatus="已暂停，已下载内容保留";refreshOfflineInfo(downloadTarget.value("bookId").toString());emit downloadChanged();
}
void Library::removeDownload(QString id) {
    if(downloading&&downloadTarget.value("bookId").toString()==id) pauseDownload();
    if(bookId==id) {++readGeneration;blocks.clear();chapters.clear();page="bookshelf";emit pageChanged();}
    bool ok=QDir(accountCache()+"/"+OfflineStore::hash(id)).removeRecursively();
    applyLocalCovers();downloadStatus=ok?"本机下载已删除":"删除失败";emit downloadChanged();
}
void Library::downloadBook(QString id) {
    if(offlineMode) {downloadStatus="请先关闭离线模式";emit downloadChanged();return;}
    pauseDownload();downloadTarget.clear();
    for(auto b:books) if(b.toMap().value("bookId").toString()==id) downloadTarget=b.toMap();
    if(downloadTarget.isEmpty())return;
    downloading=true;downloadStatus="正在获取目录";downloadIndex=downloadedCount=deniedCount=skippedCount=0;emit downloadChanged();
    int gen=downloadGeneration;auto root=bookCache(id,downloadTarget.value("version").toInt());
    auto begin=[this,gen,root](QVariantList toc) {
        if(gen!=downloadGeneration)return;
        downloadChapters=toc;
        if(toc.isEmpty()||!OfflineStore::write(root+"/toc.json",QJsonDocument::fromVariant(QVariantMap{{"chapters",toc}}).toJson())) {
            downloading=false;downloadStatus="目录为空或存储空间不足";emit downloadChanged();return;
        }
        auto cover=downloadTarget.value("cover").toString();auto file=root+"/cover.img";
        if(!QFileInfo::exists(file)&&QUrl(cover).scheme()=="https") {
            downloadImages({QVariantMap{{"url",cover},{"path",file}}},[this,gen](bool ok){
                if(gen!=downloadGeneration)return;
                if(ok)applyLocalCovers();else qInfo("OFFLINE_COVER_MISSING");
                downloadNext();
            });
        }else downloadNext();
    };
    auto cached=OfflineStore::read(root+"/toc.json").value("chapters").toArray().toVariantList();
    if(!cached.isEmpty()){begin(cached);return;}
    request("/book/chapterInfos",{{"bookIds",QJsonArray{id}},{"synckeys",QJsonArray{0}},{"updateTimes",QJsonArray{0}}},30000,
      [this,gen,begin](QJsonObject data,QString message){
        if(gen!=downloadGeneration)return;
        if(!message.isEmpty()){downloading=false;downloadStatus="目录下载失败，可重试";emit downloadChanged();return;}
        begin(data.value("data").toArray().first().toObject().value("updated").toArray().toVariantList());
      },true);
}
void Library::downloadImages(QVariantList images,std::function<void(bool)> done) {
    if(images.isEmpty()){done(true);return;}
    auto item=images.takeFirst().toMap();QNetworkRequest req(QUrl(item.value("url").toString()));req.setTransferTimeout(30000);
    auto reply=network.get(req);downloadReply=reply;int gen=downloadGeneration;
    connect(reply,&QNetworkReply::downloadProgress,this,[reply](qint64 got,qint64){if(got>20*1024*1024)reply->abort();});
    connect(reply,&QNetworkReply::finished,this,[this,reply,gen,item,images,done]{
        if(downloadReply==reply)downloadReply=nullptr;
        if(gen==downloadGeneration) {
            auto bytes=reply->readAll();QImage image;bool ok=reply->error()==QNetworkReply::NoError&&image.loadFromData(bytes)&&OfflineStore::write(item.value("path").toString(),bytes);
            if(ok)downloadImages(images,done);else done(false);
        }reply->deleteLater();
    });
}
void Library::downloadNext() {
    if(!downloading)return;
    if(downloadIndex>=downloadChapters.size()) {
        auto title=downloadTarget.value("title").toString();
        downloading=false;downloadStatus=deniedCount?QString("%1：%2 章需购买或会员，未下载").arg(title).arg(deniedCount)
            :QString("%1：全部 %2 章已下载").arg(title).arg(downloadChapters.size());
        if(skippedCount)downloadStatus+=QString("；%1 章格式无法解析，已跳过").arg(skippedCount);
        auto id=downloadTarget.value("bookId").toString();
        OfflineStore::write(bookCache(id,downloadTarget.value("version").toInt())+"/state.json",QJsonDocument(QJsonObject{{"denied",deniedCount}}).toJson());
        refreshOfflineInfo(id);emit downloadChanged();qInfo("OFFLINE_DOWNLOAD complete=%d denied=%d",downloadedCount,deniedCount);return;
    }
    auto chapter=downloadChapters[downloadIndex].toMap();int uid=chapter.value("chapterUid").toInt();
    auto root=bookCache(downloadTarget.value("bookId").toString(),downloadTarget.value("version").toInt())+"/"+QString::number(uid);
    auto status=QString("正在下载《%1》").arg(downloadTarget.value("title").toString());
    if(downloadStatus!=status){downloadStatus=status;refreshOfflineInfo(downloadTarget.value("bookId").toString());emit downloadChanged();}
    else notifyProgress();
    int gen=downloadGeneration;
    auto finish=[this,gen,uid](bool ok){if(gen!=downloadGeneration)return;if(!ok){qInfo("OFFLINE_FAIL chapter=%d",uid);downloading=false;downloadStatus="网络中断，点「继续」接着下载；已下载内容保留";refreshOfflineInfo(downloadTarget.value("bookId").toString());emit downloadChanged();return;}++downloadedCount;++downloadIndex;QTimer::singleShot(0,this,&Library::downloadNext);};
    // A missing illustration must not stop the book: text is already saved and the image is retried on the next pass.
    auto images=[finish,uid](bool ok){if(!ok)qInfo("OFFLINE_IMAGE_MISSING chapter=%d",uid);finish(true);};
    auto cached=OfflineStore::read(root+"/chapter.json").value("blocks").toArray().toVariantList();
    if(!cached.isEmpty()){downloadImages(OfflineStore::missingImages(cached),images);return;}
    auto reply=network.get(apiRequest(chapterUrl(downloadTarget,uid),60000));downloadReply=reply;
    connect(reply,&QNetworkReply::downloadProgress,this,[reply](qint64 got,qint64){if(got>50*1024*1024)reply->abort();});
    connect(reply,&QNetworkReply::finished,this,[this,reply,gen,root,chapter,finish,images]{
        if(downloadReply==reply)downloadReply=nullptr;
        if(gen==downloadGeneration) {
            int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if(status==402||status==403){++deniedCount;++downloadIndex;QTimer::singleShot(0,this,&Library::downloadNext);}
            else if(status==401){qInfo("OFFLINE_HTTP status=401");downloading=false;downloadStatus="微信读书登录已过期：重新扫码登录后点「继续」";
                refreshOfflineInfo(downloadTarget.value("bookId").toString());emit downloadChanged();}
            else if(reply->error()!=QNetworkReply::NoError){qInfo("OFFLINE_HTTP status=%d",status);finish(false);}
            else storeDownloaded(gen,reply->readAll(),reply->rawHeader("encryptkey"),root,chapter,images);
        }reply->deleteLater();
    });
}
// Key decoding, unpacking, file writes and image scanning run off the GUI thread so taps and page turns stay
// responsive while a book downloads; the result is applied on the GUI thread only if the download is still current.
void Library::storeDownloaded(int gen,QByteArray bytes,QByteArray header,QString root,QVariantMap chapter,std::function<void(bool)> images) {
    auto vid=sessionVid;
    QThreadPool::globalInstance()->start([this,gen,bytes,header,vid,root,chapter,images]{
        QString error;auto key=decodeChapterKey(header,vid,error);
        auto files=error.isEmpty()?unpackChapter(bytes,key,error):QMap<QString,QByteArray>{};key.fill(0);
        auto data=error.isEmpty()?OfflineStore::storeChapter(root,files,chapter,error):QVariantList{};
        auto missing=error.isEmpty()&&!data.isEmpty()?OfflineStore::missingImages(data):QVariantList{};
        QMetaObject::invokeMethod(this,[this,gen,error,data,missing,chapter,images]{
            if(gen!=downloadGeneration)return;
            // A chapter the archive format cannot yield is skipped; it must not hold back the rest of the book.
            if(!error.isEmpty()||data.isEmpty()){qInfo("OFFLINE_STORE_FAIL chapter=%d error=%s",chapter.value("chapterUid").toInt(),qPrintable(error));
                ++skippedCount;++downloadIndex;QTimer::singleShot(0,this,&Library::downloadNext);return;}
            downloadImages(missing,images);
        },Qt::QueuedConnection);
    });
}
void Library::notifyProgress() {
    if(progressPending)return;
    progressPending=true;
    QTimer::singleShot(2500,this,[this]{progressPending=false;if(downloading){refreshOfflineInfo(downloadTarget.value("bookId").toString());emit downloadChanged();}});
}
