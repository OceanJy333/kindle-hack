// input: Qt libraries on Kindle; output: runtime checks as JSON; pos: native platform acceptance probe.
#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSslSocket>
#include <QTemporaryFile>
#include <QThread>
#include <QTimer>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/resource.h>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QJsonObject result{{"qt", qVersion()}, {"epochMs", QDateTime::currentMSecsSinceEpoch()}};
    unsigned char first[32], second[32];
    arc4random_buf(first, sizeof(first));
    arc4random_buf(second, sizeof(second));
    result["entropy"] = std::memcmp(first, second, sizeof(first)) != 0;
    QTemporaryFile file;
    const QByteArray sample("native-kindle-file-check");
    bool fileOk = file.open() && file.write(sample) == sample.size() && file.flush();
    fileOk = fileOk && QFileInfo(file.fileName()).size() == sample.size() && file.seek(0);
    result["file"] = fileOk && file.readAll() == sample;
    result["tlsAvailable"] = QSslSocket::supportsSsl();
    result["tlsLibrary"] = QSslSocket::sslLibraryVersionString();
    QTimer::singleShot(20, &app, [&] { result["timer"] = true; });
    auto worker = QThread::create([&] {
        QMetaObject::invokeMethod(&app, [&] { result["thread"] = true; }, Qt::QueuedConnection);
    });
    worker->start();
    QNetworkAccessManager network;
    auto reply = network.get(QNetworkRequest(QUrl("https://ink.qq.com/")));
    QObject::connect(reply, &QNetworkReply::finished, &app, [&] {
        result["httpsStatus"] = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        result["httpsBytes"] = reply->readAll().size();
        result["httpsError"] = reply->errorString();
        result["https"] = reply->error() == QNetworkReply::NoError;
        QTimer::singleShot(50, &app, &QCoreApplication::quit);
    });
    QTimer::singleShot(15000, &app, &QCoreApplication::quit);
    app.exec();
    worker->wait();
    delete worker;
    struct rusage usage{};
    getrusage(RUSAGE_SELF, &usage);
    result["peakRssKB"] = qint64(usage.ru_maxrss);
    auto json = QJsonDocument(result).toJson();
    std::fwrite(json.constData(), 1, json.size(), stdout);
    return result["file"].toBool() && result["timer"].toBool()
        && result["thread"].toBool() && result["https"].toBool() && result["entropy"].toBool() ? 0 : 1;
}
