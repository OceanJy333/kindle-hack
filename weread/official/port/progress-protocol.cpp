// input: chapter position; output: WeRead Web request fields; pos: native progress protocol adapter.
#include "progress-protocol.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QUrl>
namespace ProgressProtocol {
static QString md5(const QString &s) {return QCryptographicHash::hash(s.toUtf8(),QCryptographicHash::Md5).toHex();}
QString encode(const QString &s) {
    auto hash=md5(s); QStringList chunks;
    bool digits=QRegularExpression("^[0-9]+$").match(s).hasMatch();
    if(digits) for(int i=0;i<s.size();i+=9) chunks<<QString::number(s.mid(i,9).toUInt(),16);
    else {QString hex;for(unsigned char c:s.toUtf8())hex+=QString::number(c,16);chunks<<hex;}
    QString out=hash.left(3)+(digits?"3":"4")+"2"+hash.right(2);
    QStringList parts;for(auto chunk:chunks)parts<<QString::number(chunk.size(),16).rightJustified(2,'0')+chunk;
    out+=parts.join('g');if(out.size()<20)out+=hash.left(20-out.size());return out+md5(out).left(3);
}
QString sign(const QJsonObject &p) {
    QStringList fields,keys=p.keys();keys.sort();
    for(auto k:keys)if(k!="s")fields<<k+"="+QString::fromLatin1(QUrl::toPercentEncoding(p[k].toVariant().toString()));
    auto q=fields.join('&').toLatin1();quint32 a=0x15051505,b=a;
    for(int i=q.size();i>1;i-=2){a=(a^(quint32(quint8(q[i-1]))<<((q.size()-i+1)%30)))&0x7fffffff;b=(b^(quint32(quint8(q[i-2]))<<((i-1)%30)))&0x7fffffff;}
    return QString::number(quint64(a)+b,16);
}
QJsonObject payload(const QString &book,const QJsonObject &p,const QJsonObject &context,bool enter) {
    qint64 now=QDateTime::currentSecsSinceEpoch();QString ua=userAgent,prefix;quint32 hash=0;
    for(auto word:ua.split(' ').mid(0,12))prefix+=QString::number(word.size()%10);
    for(unsigned char c:ua.toUtf8())hash=(131*hash+c)&0x7fffffff;
    auto pc=context["pclts"].toString();if(pc.isEmpty()||pc=="0")pc=encode(QString::number(now));
    QJsonObject out{{"appId","wb"+prefix+"h"+QString::number(hash)}, {"b",encode(book)},
      {"c",encode(QString::number(p["chapterUid"].toInt()))},{"ci",p["chapterIdx"]},
      {"co",p["chapterOffset"]},{"sm",p["summary"].toString().left(20)}, {"pr",p["progress"]},
      {"ct",now},{"ps",context["psvts"]},{"pc",pc}};
    if(!enter){qint64 ts=QDateTime::currentMSecsSinceEpoch();int rn=QRandomGenerator::global()->bounded(1000);
      auto token=context["token"].toString();if(token.isEmpty())token="3c5c8717f3daf09iop3423zafeqoi";
      out["rt"]=0;out["ts"]=ts;out["rn"]=rn;
      out["sg"]=QString::fromLatin1(QCryptographicHash::hash((QString::number(ts)+QString::number(rn)+token).toUtf8(),QCryptographicHash::Sha256).toHex());}
    out["s"]=sign(out);return out;
}
}
