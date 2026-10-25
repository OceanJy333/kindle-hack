// input: original AuthService decompilation; output: QR login/session; pos: ARMv7 reconstruction, not original source text.
#include "auth.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>
#include <QUuid>

static QString textValue(const QJsonValue &value) { return value.toVariant().toString(); }
Auth::Auth(const QString &directory, QObject *parent) : QObject(parent), directory(directory) {
    QDir().mkpath(directory);
    QFile::setPermissions(directory, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    cookies = new CookieStore(directory, &network);
    network.setCookieJar(cookies);
    QFile idFile(directory + "/device_id");
    if (idFile.open(QIODevice::ReadOnly)) deviceId = QString::fromUtf8(idFile.readAll()).trimmed();
    if (deviceId.isEmpty()) {
        deviceId = "eink" + QUuid::createUuid().toString(QUuid::WithoutBraces);
        QSaveFile file(directory + "/device_id");
        if (file.open(QIODevice::WriteOnly)) {
            file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
            file.write(deviceId.toUtf8()); file.commit();
        }
    }
    pollTimer.setSingleShot(true); expiryTimer.setSingleShot(true); retryTimer.setSingleShot(true);
    connect(&pollTimer, &QTimer::timeout, this, &Auth::pollLogin);
    connect(&retryTimer, &QTimer::timeout, this, &Auth::requestUid);
    connect(&expiryTimer, &QTimer::timeout, this, [this] {
        cancelLogin(); expired = true; status = "二维码已过期"; emit changed();
    });
    QFile sessionFile(directory + "/session-port.json");
    if (sessionFile.open(QIODevice::ReadOnly)) {
        auto session = QJsonDocument::fromJson(sessionFile.readAll()).object();
        if (!session.value("accessToken").toString().isEmpty())
            QTimer::singleShot(0, this, [this, session] { resolveSession(session); });
    }
}
void Auth::setState(State next, const QString &message) {
    state = next; status = message; qInfo("AUTH_STATE %d", state); emit changed();
}
void Auth::fail(const QString &message) {
    pollTimer.stop(); expiryTimer.stop(); retryTimer.stop();
    error = message; setState(Error, message);
}
// Evidence: 0x163b50, 0x163d30, 0x162ad0, 0x1647d0.
void Auth::startLogin() {
    if (state >= RequestingUid && state <= Authenticated) return;
    ++generation; expired = false; error.clear(); uidRetries = pollRetries = 0;
    expiryTimer.start(180000); requestUid();
}
void Auth::cancelLogin() {
    ++generation; pollTimer.stop(); expiryTimer.stop(); retryTimer.stop();
    uid.clear(); qrUrl.clear(); error.clear(); expired = false; setState(Idle, {});
}
void Auth::refreshQrCode() { cancelLogin(); startLogin(); }
void Auth::requestUid() {
    int current = generation;
    setState(RequestingUid, "正在生成二维码...");
    request("/web/getuid", {}, 30000, [this, current](QJsonObject result, QString message) {
        if (current != generation) return;
        if (!message.isEmpty()) {
            if (uidRetries < 4) { retryTimer.start(qMin(1000 << uidRetries++, 5000)); return; }
            fail(message); return;
        }
        uid = textValue(result.value("uid"));
        if (uid.isEmpty()) { fail("uid missing"); return; }
        uidRetries = 0; expiryTimer.start(180000);
        qrUrl = "https://weread.qq.com/web/confirm?pf=2&uid=" + uid;
        setState(WaitingForScan, "请使用微信扫码登录");
        pollTimer.start(2000);
    });
}
// Evidence: 0x164510, 0x1657c0. Session values are never printed in diagnostics.
void Auth::pollLogin() {
    if (uid.isEmpty()) return;
    int current = generation;
    request("/web/getlogininfo", {{"uid", uid}}, 70000,
        [this, current](QJsonObject result, QString message) {
        if (current != generation) return;
        QString code = result.value("logicCode").toString();
        if (code == "NEED_OTP") {
            qrUrl.clear(); setState(WaitingForConfirm, "请在手机上完成验证");
            pollTimer.start(2000); return;
        }
        if (QStringList{"OTP_EXPIRED", "OTP_NOT_MATCH", "LOGIN_TIMEOUT", "LOGIN_FAILED",
                        "LOGIN_FAIL", "LOGIN_CANCELLED", "LOGIN_CANCELED"}.contains(code)) {
            cancelLogin(); expired = true; status = code; emit changed(); return;
        }
        if (!message.isEmpty()) {
            if (++pollRetries < 3) pollTimer.start(2000);
            else fail(message);
            return;
        }
        pollRetries = 0;
        QString vid;
        for (const auto &key : {"webLoginVid", "vid", "userVid", "user_vid", "openid"}) {
            vid = textValue(result.value(key)); if (!vid.isEmpty()) break;
        }
        if (vid.isEmpty()) vid = QString::fromUtf8(cookies->value("wr_vid"));
        QString skey = QString::fromUtf8(cookies->value("wr_skey"));
        if (skey.isEmpty()) skey = textValue(result.value("skey"));
        if (skey.isEmpty()) skey = textValue(result.value("accessToken"));
        if (!vid.isEmpty() && !skey.isEmpty() && !textValue(result.value("code")).isEmpty()) {
            result["vid"] = vid; result["skey"] = skey;
            finishLogin(result); return;
        }
        if (code == "LOGIN_SUCCESS" || result.contains("wxHeadImg") || result.contains("wxNickName"))
            setState(WaitingForConfirm, "请在手机上确认登录");
        pollTimer.start(2000);
    }, true);
}
// Evidence: 0x177820; preserve official request fields, use the actual target's name.
void Auth::finishLogin(QJsonObject result) {
    pollTimer.stop(); expiryTimer.stop();
    setState(InitializingSession, "正在完成登录...");
    QJsonObject body{{"vid", result.value("vid")}, {"skey", result.value("skey")},
        {"code", result.value("code")}, {"isAutoLogout", result.value("isAutoLogout").toInt()},
        {"deviceId", deviceId}, {"deviceType", 4}, {"deviceName", "Kindle Voyage"}};
    int current = generation;
    request("/weblogin", body, 30000, [this, current](QJsonObject data, QString message) {
        if (current != generation) return;
        if (!message.isEmpty()) { fail(message); return; }
        resolveSession(data);
    }, true, true);
}
void Auth::resolveSession(const QJsonObject &result) {
    // No successful-login claim until actual credentials have been returned.
    QString token = textValue(result.value("accessToken"));
    if (token.isEmpty()) token = QString::fromUtf8(cookies->value("wr_skey"));
    if (token.isEmpty()) { fail("weblogin response missing accessToken"); return; }
    sessionVid = textValue(result.value("vid"));
    if (sessionVid.isEmpty()) sessionVid = QString::fromUtf8(cookies->value("wr_vid"));
    if (sessionVid.isEmpty()) { fail("weblogin response missing vid"); return; }
    accessToken = token; refreshToken = textValue(result.value("refreshToken"));
    QSaveFile file(directory + "/session-port.json");
    if (!file.open(QIODevice::WriteOnly)) { fail(file.errorString()); return; }
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    file.write(QJsonDocument(result).toJson(QJsonDocument::Compact));
    if (!file.commit()) { fail(file.errorString()); return; }
    cookies->save(); qrUrl.clear();
    setState(Authenticated, "登录会话已建立"); emit sessionReady();
}
