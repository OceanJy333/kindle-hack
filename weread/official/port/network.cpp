// input: official API paths and session cookies; output: parsed responses; pos: recovered WereadApiClient subset.
#include "auth.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QNetworkCookie>
#include <QNetworkReply>
#include <QSaveFile>
#include <QSslConfiguration>
#include <QUrlQuery>

CookieStore::CookieStore(const QString &directory, QObject *parent)
    : QNetworkCookieJar(parent), path(directory + "/cookies-port.dat") {
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        QList<QNetworkCookie> list;
        for (const auto &line : file.readAll().split('\n')) list += QNetworkCookie::parseCookies(line);
        setAllCookies(list);
    }
}
QByteArray CookieStore::value(const QByteArray &name) const {
    for (const auto &cookie : allCookies()) if (cookie.name() == name) return cookie.value();
    return {};
}
void CookieStore::save() {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return;
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    for (const auto &cookie : allCookies()) file.write(cookie.toRawForm() + '\n');
    file.commit();
}
bool CookieStore::setCookiesFromUrl(const QList<QNetworkCookie> &list, const QUrl &url) {
    bool result = QNetworkCookieJar::setCookiesFromUrl(list, url);
    save();
    return result;
}

QNetworkRequest Auth::apiRequest(const QUrl &url, int timeout) const {
    QNetworkRequest req(url);
    req.setSslConfiguration(QSslConfiguration::defaultConfiguration());
    req.setTransferTimeout(timeout);
    req.setRawHeader("User-Agent", "WeRead/1.0.0 WRBrand/remarkable wr_eink");
    req.setRawHeader("Accept", "application/json, text/plain, */*");
    req.setRawHeader("Origin", "https://weread.qq.com");
    req.setRawHeader("Referer", "https://weread.qq.com/");
    if (!sessionVid.isEmpty()) req.setRawHeader("vid", sessionVid.toUtf8());
    if (!accessToken.isEmpty()) req.setRawHeader("accessToken", accessToken.toUtf8());
    return req;
}

// Evidence: 0x1707a0, 0x1721a0, 0x177820, 0x16cc10, 0x16b640.
void Auth::request(const QString &path, const QJsonObject &body, int timeout,
                   Callback done, bool post, bool login, bool replay) {
    QUrl url("https://i.weread.qq.com" + path);
    QUrlQuery query(url);
    bool desktop = path.startsWith("/web/") || path.startsWith("/weblogin");
    if (desktop) query.addQueryItem("platform", "desktop");
    if (login) for (auto it = body.begin(); it != body.end(); ++it)
        query.addQueryItem(it.key(), it.value().toVariant().toString());
    url.setQuery(query);
    auto req = apiRequest(url, timeout);
    if (desktop) {
        req.setRawHeader("CLIENT_TYPE", "1");
        req.setRawHeader("X-Is-Serverside", "1");
    }
    if (login) req.setRawHeader("fingerprint", deviceId.toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    auto reply = post ? network.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact))
                      : network.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, done, path, body, timeout, post, login, replay] {
        QJsonParseError parse;
        auto document = QJsonDocument::fromJson(reply->readAll(), &parse);
        const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QString error;
        int code = 0;
        auto object = document.object();
        QList<QJsonObject> layers{object, object.value("data").toObject(), object.value("error").toObject()};
        for (const auto &layer : layers) for (const auto &key : {"err", "errcode", "errCode"})
            if (!code) code = layer.value(key).toVariant().toInt();
        qInfo("API %s HTTP=%d error=%d code=%d", qPrintable(path), http, reply->error(), code);
        reply->deleteLater();
        // -2012: the access token timed out. The official client renews it with the refresh token
        // (0x17aa60) and replays the request once; only a rejected renewal means a new QR login.
        if (code == -2012 && !login && !replay && path != "/weblogin" && !refreshToken.isEmpty()) {
            renewSession([this, path, body, timeout, done, post](bool ok) {
                if (ok) request(path, body, timeout, done, post, false, true);
                else done({}, "微信读书登录已过期");
            });
            return;
        }
        // 401 means the saved WeRead session is no longer accepted; callers must not mistake it for no network.
        if (http == 401) error = "微信读书登录已过期";
        else if (reply->error() != QNetworkReply::NoError) error = reply->errorString();
        else if (!document.isObject()) error = "invalid login response";
        else if (code != 0) error = QString("WeRead error %1").arg(code);
        if (object.value("data").isObject()) object = object.value("data").toObject();
        done(object, error);
    });
}
// Evidence: 0x17aa60 (request), 0x17b450 (store). Concurrent callers wait for one renewal.
void Auth::renewSession(std::function<void(bool)> done) {
    renewWaiters.append(std::move(done));
    if (renewing) return;
    renewing = true;
    QJsonObject body{{"vid", sessionVid}, {"refreshToken", refreshToken}, {"deviceId", deviceId},
        {"deviceType", 7}, {"deviceName", "Kindle Voyage"}, {"isFromweb", 0}};
    const QString previous = accessToken;
    request("/weblogin", body, 30000, [this, previous](QJsonObject data, QString) {
        QString token = data.value("accessToken").toVariant().toString();
        bool ok = !token.isEmpty() && token != previous;
        qInfo("SESSION_RENEWAL %s", ok ? "ok" : "rejected");
        if (ok) {
            QFile file(directory + "/session-port.json");
            QJsonObject session;
            if (file.open(QIODevice::ReadOnly)) session = QJsonDocument::fromJson(file.readAll()).object();
            file.close();
            session["accessToken"] = token; accessToken = token;
            auto next = data.value("refreshToken").toVariant().toString();
            if (!next.isEmpty()) session["refreshToken"] = refreshToken = next;
            QSaveFile out(directory + "/session-port.json");
            if (out.open(QIODevice::WriteOnly)) {
                out.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
                out.write(QJsonDocument(session).toJson(QJsonDocument::Compact)); out.commit();
            }
        }
        renewing = false;
        const auto waiters = std::exchange(renewWaiters, {});
        for (const auto &waiter : waiters) waiter(ok);
    }, true, true);
}

void Auth::web(const QString &path, const QList<QPair<QString, QString>> &items, Callback done) {
    QUrl url("https://weread.qq.com" + path);
    QUrlQuery query; query.setQueryItems(items); url.setQuery(query);
    QNetworkRequest req(url);
    req.setSslConfiguration(QSslConfiguration::defaultConfiguration());
    req.setTransferTimeout(20000);
    req.setRawHeader("User-Agent", "Mozilla/5.0 WeRead Voyage");
    req.setRawHeader("Origin", "https://weread.qq.com");
    req.setRawHeader("Referer", "https://weread.qq.com/");
    req.setRawHeader("Cookie", ("wr_vid=" + sessionVid + "; wr_skey=" + accessToken).toUtf8());
    auto reply = network.get(req);
    connect(reply, &QNetworkReply::finished, this, [reply, done, path] {
        auto object = QJsonDocument::fromJson(reply->readAll()).object();
        int code = 0;
        for (const auto &key : {"errCode", "errcode", "err"}) if (!code) code = object.value(key).toVariant().toInt();
        QString error;
        if (code == -2012 || code == -2010) error = "微信读书登录已过期";
        else if (reply->error() != QNetworkReply::NoError) error = reply->errorString();
        else if (code) error = QString("WeRead error %1").arg(code);
        qInfo("WEB %s HTTP=%d code=%d", qPrintable(path), reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), code);
        reply->deleteLater();
        done(object, error);
    });
}

void Auth::cancelPolicyDocument() { ++policyGeneration; policyLoading = false; emit policyDocumentChanged(); }
void Auth::loadPolicyDocument(const QString &url) {
    cancelPolicyDocument();
    int current = policyGeneration;
    policyUrl = url; policyHtml.clear(); policyError.clear(); policyLoading = true;
    emit policyDocumentChanged();
    QNetworkRequest req{QUrl(url)};
    req.setSslConfiguration(QSslConfiguration::defaultConfiguration());
    req.setTransferTimeout(30000);
    auto reply = network.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, current] {
        if (current == policyGeneration) {
            policyLoading = false;
            if (reply->error() != QNetworkReply::NoError) policyError = reply->errorString();
            else { policyHtml = QString::fromUtf8(reply->readAll()); policyUrl = reply->url().toString(); }
            emit policyDocumentChanged();
        }
        reply->deleteLater();
    });
}
