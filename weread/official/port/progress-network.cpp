// input: authenticated sync requests; output: validated Web responses; pos: progress transport.
#include "auth.h"
#include "progress-protocol.h"
#include <QJsonDocument>
#include <QNetworkReply>
#include <QRegularExpression>
QNetworkRequest Auth::progressRequest(const QString &path) const {
    QNetworkRequest req(QUrl("https://weread.qq.com"+path));req.setTransferTimeout(15000);
    req.setRawHeader("User-Agent",ProgressProtocol::userAgent);
    req.setRawHeader("Origin","https://weread.qq.com");req.setRawHeader("Referer","https://weread.qq.com/");
    req.setRawHeader("Cookie",("wr_vid="+sessionVid+"; wr_skey="+accessToken).toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");return req;
}
void Auth::progressContext(const QString &book,Callback done) {
    auto reply=network.get(progressRequest("/web/reader/"+ProgressProtocol::encode(book)));
    connect(reply,&QNetworkReply::finished,this,[reply,done]{
        auto html=QString::fromUtf8(reply->readAll());QJsonObject result;QString error;
        if(reply->error()!=QNetworkReply::NoError)error=reply->errorString();
        for(auto k:{"psvts","pclts","token"}){
            auto match=QRegularExpression(QString("\"%1\"\\s*:\\s*\"([^\"]+)\"").arg(k)).match(html);
            if(match.hasMatch())result[k]=match.captured(1);
        }
        if(error.isEmpty()&&result["psvts"].toString().isEmpty())error="无法获取同步会话";
        reply->deleteLater();done(result,error);
    });
}
void Auth::progressPost(const QJsonObject &body,Callback done) {
    auto reply=network.post(progressRequest("/web/book/read"),QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply,&QNetworkReply::finished,this,[reply,done]{
        auto result=QJsonDocument::fromJson(reply->readAll()).object();QString error;
        if(reply->error()!=QNetworkReply::NoError)error=reply->errorString();
        else if(result["succ"].toVariant().toInt()!=1)error=QString("同步被拒绝 %1").arg(result["errCode"].toInt());
        qInfo("PROGRESS_POST HTTP=%d accepted=%d",reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),error.isEmpty());
        reply->deleteLater();done(result,error);
    });
}
void Auth::webPost(const QString &path,const QJsonObject &body,Callback done) {
    auto reply=network.post(progressRequest(path),QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply,&QNetworkReply::finished,this,[reply,path,done]{
        auto doc=QJsonDocument::fromJson(reply->readAll());auto result=doc.object();QString error;
        int code=0;for(auto key:{"errCode","errcode","err"})if(!code)code=result[key].toInt();
        if(reply->error()!=QNetworkReply::NoError)error=reply->errorString();
        else if(!doc.isObject())error="评论响应格式无效";
        else if(code==-2012||code==-2010)error="微信读书登录已过期";
        else if(code)error=QString("评论加载失败 %1").arg(code);
        qInfo("WEB_POST %s HTTP=%d code=%d",qPrintable(path),reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),code);
        reply->deleteLater();done(result,error);
    });
}
