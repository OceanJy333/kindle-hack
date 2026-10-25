// input: EPUB image references; output: bounded native image resources; pos: Voyage renderer image loader.
#include "reader.h"
#include "offline-store.h"
#include <QFile>
#include <QQmlContext>
#include <QRegularExpression>
QString Reader::prepareHtml(const QString &source, const QUrl &base) {
    static QRegularExpression img("<img\\b[^>]*\\bsrc\\s*=\\s*['\"]([^'\"]+)['\"][^>]*>", QRegularExpression::CaseInsensitiveOption);
    auto iterator = img.globalMatch(source); QList<QRegularExpressionMatch> matches;
    while (iterator.hasNext()) matches.append(iterator.next());
    QString html = source;
    for (auto it = matches.crbegin(); it != matches.crend(); ++it) {
        QUrl url = base.resolved(QUrl(it->captured(1))); QString key = url.toString();
        auto imageFile = OfflineStore::imagePath(base.toString(),key);
        if(!images.contains(key)) {QImage cached(imageFile);if(!cached.isNull())images.insert(key,cached);}
        auto controller=qmlContext(this)->contextProperty("appController").value<QObject*>();
        bool offline=controller&&controller->property("offlineMode").toBool();
        if (!images.contains(key) && url.isLocalFile()) {
            QImage image(url.toLocalFile()); if (!image.isNull()) images.insert(key, image);
        }
        if (!images.contains(key) && !imageRequests.contains(key) && !offline && url.scheme() == "https"
            && (url.host() == "res.weread.qq.com" || url.host().endsWith(".qpic.cn"))) {
            QNetworkRequest req(url); req.setTransferTimeout(30000);
            auto reply = imageNetwork.get(req); imageRequests.insert(key, reply);
            int generation = imageGeneration;
            connect(reply, &QNetworkReply::finished, this, [this, reply, key, generation, imageFile] {
                if (generation == imageGeneration) {
                    imageRequests.remove(key);
                    auto bytes=reply->readAll(); QImage image; image.loadFromData(bytes);
                    if (!image.isNull()) { OfflineStore::write(imageFile,bytes); images.insert(key, image); relayout(); qInfo("READER_IMAGE ready"); }
                }
                reply->deleteLater();
            });
        }
        if (images.contains(key)) {
            const auto size = images.value(key).size().scaled(qMax(1, int(width()) - 2 * margin),
                qMax(1, int(height()) - 2 * fontSize), Qt::KeepAspectRatio);
            html.replace(it->capturedStart(), it->capturedLength(), QString("<img src=\"%1\" width=\"%2\" height=\"%3\"/>")
                .arg(key.toHtmlEscaped()).arg(size.width()).arg(size.height()));
        }
    }
    return html;
}
