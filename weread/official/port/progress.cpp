// input: local cursors and cloud progress; output: durable bidirectional sync; pos: native reader coordinator.
#include "library.h"
#include "offline-store.h"
#include "progress-protocol.h"
#include <QDateTime>
#include <QJsonDocument>
static qint64 now(){return QDateTime::currentSecsSinceEpoch();}
QString Library::progressPath() const {return accountCache()+"/progress-"+OfflineStore::hash(bookId)+".json";}
void Library::setSyncStatus(const QString &message){if(syncStatus==message)return;syncStatus=message;emit syncChanged();}
void Library::startProgress(){
    syncTimer.setSingleShot(false);syncTimer.setInterval(30000);
    connect(&syncTimer,&QTimer::timeout,this,[this]{if(page=="reader"||!OfflineStore::read(progressPath())["pending"].toObject().isEmpty())synchronizeProgress();});
    syncTimer.start();
}
void Library::openProgress(){
    suppressChapterPosition=true;
    ++syncGeneration;syncBusy=false;cloudAnchor={};lastMovement=0;
    synchronizeProgress(true);
}
void Library::saveProgressPosition(QVariantMap position){
    if(reading||blocks.isEmpty()||page!="reader")return;
    lastMovement=now();
    if(position.isEmpty()){setSyncStatus("本页位置暂无法同步");return;}
    QJsonObject p=QJsonObject::fromVariantMap(position);
    p["chapterUid"]=chapterUid;
    int index=0;double before=0,total=0,words=0;
    for(int i=0;i<chapters.size();++i){auto c=chapters[i].toMap();double count=c.value("wordCount",c.value("wordcount",1)).toDouble();if(count<=0)count=1;
      if(c.value("chapterUid").toInt()==chapterUid){index=c.value("chapterIdx",i+1).toInt();before=total;words=count;}total+=count;}
    p["chapterIdx"]=index;
    // Percentage is display metadata; chapterOffset and summary are the actual resume anchor.
    double length=0;for(auto b:blocks)length+=b.toMap().value("richHtml").toString().size();
    p["progress"]=qBound(0,int(100*(before+words*qMin(1.0,p["chapterOffset"].toInt()/qMax(1.0,length)))/qMax(1.0,total)),100);
    p["modifiedAt"]=now()+serverClockOffset;
    auto state=OfflineStore::read(progressPath());auto pending=state["pending"].toObject();
    auto same=[](QJsonObject a,QJsonObject b){return a["chapterUid"]==b["chapterUid"]&&a["chapterOffset"]==b["chapterOffset"];};
    if(same(p,pending))return;
    if(same(p,state["cloud"].toObject())){state.remove("pending");OfflineStore::write(progressPath(),QJsonDocument(state).toJson());setSyncStatus("进度已同步");return;}
    p["revision"]=QString::number(QDateTime::currentMSecsSinceEpoch());state["pending"]=p;
    if(!OfflineStore::write(progressPath(),QJsonDocument(state).toJson())){setSyncStatus("阅读进度保存失败");return;}
    setSyncStatus(offlineMode?"离线，进度已保留":"等待同步");
    const int gen=syncGeneration;
    QTimer::singleShot(3000,this,[this,gen]{if(gen==syncGeneration&&now()-lastMovement>=2)synchronizeProgress();});
}
void Library::applyCloudProgress(const QJsonObject &remote,bool opening){
    int uid=remote["chapterUid"].toInt();bool found=false;
    for(auto c:chapters)if(c.toMap().value("chapterUid").toInt()==uid)found=true;
    if(!found){if(opening)loadChapter(chapterUid);return;}
    suppressChapterPosition=true;
    cloudAnchor=remote;resumeBlock=resumeOffset=0;chapterUid=uid;
    qInfo("PROGRESS_PULL chapter=%d offset=%d",uid,remote["chapterOffset"].toInt());
    loadChapter(uid);
}
void Library::progressRestored(bool ok){
    if(cloudAnchor.isEmpty())return;
    setSyncStatus(ok?"已接上云端进度":"云端章节已打开，段落定位未匹配");
    cloudAnchor={};emit syncChanged();
}
void Library::synchronizeProgress(bool opening){
    if(bookId.isEmpty()||!loggedIn()||syncBusy)return;
    if(offlineMode){setSyncStatus("离线，进度已保留");if(opening)loadChapter(chapterUid);return;}
    auto account=sessionVid,id=bookId,path=progressPath();int gen=syncGeneration;
    syncBusy=true;
    auto valid=[this,account,id,gen]{return account==sessionVid&&id==bookId&&gen==syncGeneration;};
    web("/web/book/getProgress",{{"bookId",id}},[this,valid,path,id,opening](QJsonObject response,QString error){
      if(!valid())return;
      auto state=OfflineStore::read(path),pending=state["pending"].toObject(),previous=state["cloud"].toObject();
      auto remote=response["book"].toObject();
      if(!error.isEmpty()||remote.isEmpty()){
        syncBusy=false;setSyncStatus(error.contains("过期")?"同步需重新登录":"暂未联网，进度已保留");
        if(opening)loadChapter(chapterUid);return;
      }
      if(response["timestamp"].toDouble()>0)serverClockOffset=qint64(response["timestamp"].toDouble())-now();
      bool same=!pending.isEmpty()&&pending["chapterUid"]==remote["chapterUid"]&&pending["chapterOffset"]==remote["chapterOffset"];
      bool remoteNewer=!pending.isEmpty()&&remote["updateTime"].toDouble()>pending["modifiedAt"].toDouble();
      bool deferPull=!opening&&pending.isEmpty()&&page=="reader"&&now()-lastMovement<30;
      if(!deferPull)state["cloud"]=remote;
      if(same||remoteNewer){state.remove("pending");pending={};}
      OfflineStore::write(path,QJsonDocument(state).toJson());
      if(!pending.isEmpty()){
        if(opening)loadChapter(chapterUid);
        uploadProgress(id,path,pending,valid);return;
      }
      syncBusy=false;
      bool moved=remote["chapterUid"]!=previous["chapterUid"]||remote["chapterOffset"]!=previous["chapterOffset"];
      bool newer=remote["updateTime"].toDouble()>previous["updateTime"].toDouble();
      if(opening||(page=="reader"&&moved&&newer&&now()-lastMovement>=30))applyCloudProgress(remote,opening);
      else setSyncStatus("进度已同步");
    });
}
void Library::uploadProgress(QString id,QString path,QJsonObject pending,std::function<bool()> valid){
    setSyncStatus("正在同步进度");
    auto fail=[this,valid](QString error){if(!valid())return;syncBusy=false;setSyncStatus("进度已保留，稍后重试");qInfo("PROGRESS_RETRY %s",qPrintable(error));};
    progressContext(id,[this,id,path,pending,valid,fail](QJsonObject context,QString error){
      if(!valid())return;if(!error.isEmpty()){fail(error);return;}
      progressPost(ProgressProtocol::payload(id,pending,context,true),[this,id,path,pending,context,valid,fail](QJsonObject,QString error){
        if(!valid())return;if(!error.isEmpty()){fail(error);return;}
        progressPost(ProgressProtocol::payload(id,pending,context,false),[this,id,path,pending,valid,fail](QJsonObject,QString error){
          if(!valid())return;if(!error.isEmpty()){fail(error);return;}
          web("/web/book/getProgress",{{"bookId",id}},[this,path,pending,valid,fail](QJsonObject response,QString error){
            if(!valid())return;auto remote=response["book"].toObject();
            if(!error.isEmpty()||remote["chapterUid"]!=pending["chapterUid"]||remote["chapterOffset"]!=pending["chapterOffset"]){fail("readback mismatch");return;}
            auto state=OfflineStore::read(path);state["cloud"]=remote;
            if(state["pending"].toObject()["revision"]==pending["revision"])state.remove("pending");
            OfflineStore::write(path,QJsonDocument(state).toJson());syncBusy=false;setSyncStatus("进度已同步");
            qInfo("PROGRESS_VERIFIED chapter=%d offset=%d",remote["chapterUid"].toInt(),remote["chapterOffset"].toInt());
          });
        });
      });
    });
}
