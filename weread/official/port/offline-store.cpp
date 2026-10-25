// input: validated archive files; output: private durable chapter cache; pos: offline storage implementation.
#include "offline-store.h"
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QDirIterator>
#include <QFileInfo>
#include <QSaveFile>
#include <QStorageInfo>
#include <QRegularExpression>
#include <QUrl>
namespace OfflineStore {
QString hash(const QString &s) { return QString::fromLatin1(QCryptographicHash::hash(s.toUtf8(),QCryptographicHash::Sha256).toHex()); }
QJsonObject read(const QString &p) { QFile f(p); if (!f.open(QIODevice::ReadOnly)) return {}; return QJsonDocument::fromJson(f.readAll()).object(); }
bool write(const QString &p,const QByteArray &b) {
    QDir().mkpath(QFileInfo(p).absolutePath());
    QStorageInfo disk(QFileInfo(p).absolutePath());
    if (disk.isValid() && disk.bytesAvailable() < b.size()+32*1024*1024) return false;
    QSaveFile f(p); if (!f.open(QIODevice::WriteOnly)) return false;
    f.setPermissions(QFile::ReadOwner|QFile::WriteOwner);
    return f.write(b)==b.size() && f.commit();
}
qint64 bytes(const QString &p) { qint64 n=0; QDirIterator i(p,QDir::Files,QDirIterator::Subdirectories); while(i.hasNext()) { i.next();n+=i.fileInfo().size(); } return n; }
QString imagePath(const QString &base,const QString &url) { return QFileInfo(QUrl(base).toLocalFile()).absolutePath()+"/.remote/"+hash(url); }
QVariantList storeChapter(const QString &root,const QMap<QString,QByteArray> &files,const QVariantMap &chapter,QString &error) {
    QVariantList blocks;
    for(auto it=files.begin();it!=files.end();++it) {
        auto name=QDir::cleanPath(it.key());
        if(name==".."||name.startsWith("../")||QDir::isAbsolutePath(name)) {error="章节文件路径无效";return {};}
        if(!write(root+"/"+name,it.value())) {error="存储空间不足或写入失败";return {};}
    }
    for(const auto &entry:chapter.value("files").toList()) {
        auto name=entry.toString();
        for(auto it=files.begin();it!=files.end();++it) if(it.key()==name||it.key().endsWith('/'+name)) {
            blocks.append(QVariantMap{{"richHtml",QString::fromUtf8(it.value())},{"baseUrl",QUrl::fromLocalFile(root+"/"+it.key()).toString()}});break;
        }
    }
    // TXT-format books carry one plain-text file per chapter (e.g. "<bookId>_<uid>_o") and no file list;
    // turn its lines into paragraphs so the same reader path renders it.
    if(blocks.isEmpty()&&chapter.value("files").toList().isEmpty()) {
        for(auto it=files.begin();it!=files.end();++it) {
            auto name=QFileInfo(it.key()).fileName();
            if(name=="info.txt"||name.contains('.'))continue;
            QString html;
            for(auto line:QString::fromUtf8(it.value()).split('\n')) {
                line=line.trimmed();while(line.startsWith(QChar(0x3000)))line.remove(0,1);
                if(!line.isEmpty())html+="<p>"+line.toHtmlEscaped()+"</p>";
            }
            if(!html.isEmpty())blocks.append(QVariantMap{{"richHtml",html},{"baseUrl",QUrl::fromLocalFile(root+"/"+it.key()).toString()}});
        }
    }
    if(blocks.isEmpty()) error="章节正文文件未匹配";
    if(!blocks.isEmpty()&&!write(root+"/chapter.json",QJsonDocument::fromVariant(QVariantMap{{"blocks",blocks}}).toJson())) {error="章节缓存写入失败";return {};}
    return blocks;
}
QVariantList missingImages(const QVariantList &blocks) {
    QVariantList out; QSet<QString> seen;
    static QRegularExpression re("<img\\b[^>]*\\bsrc\\s*=\\s*['\"]([^'\"]+)['\"][^>]*>",QRegularExpression::CaseInsensitiveOption);
    for(auto b:blocks) {auto m=b.toMap();auto base=m.value("baseUrl").toString();auto it=re.globalMatch(m.value("richHtml").toString());
        while(it.hasNext()) {auto url=QUrl(base).resolved(QUrl(it.next().captured(1)));auto u=url.toString();
            if(url.scheme()!="https"||!(url.host()=="res.weread.qq.com"||url.host().endsWith(".qpic.cn"))) continue;
            auto path=imagePath(base,u);if(!QFileInfo::exists(path)&&!seen.contains(path)) {seen.insert(path);out.append(QVariantMap{{"url",u},{"path",path}});}
        }
    } return out;
}
}
