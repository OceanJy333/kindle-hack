// input: recovered official login protocol; output: QML login state; pos: Voyage source reconstruction.
#pragma once
#include <QObject>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkCookieJar>
#include <QTimer>
#include <functional>

class CookieStore : public QNetworkCookieJar {
public:
    explicit CookieStore(const QString &directory, QObject *parent);
    QByteArray value(const QByteArray &name) const;
    void save();
    bool setCookiesFromUrl(const QList<QNetworkCookie> &cookies, const QUrl &url) override;
private:
    QString path;
};

class Auth : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString qrUrl MEMBER qrUrl NOTIFY changed)
    Q_PROPERTY(QString loginStatusMessage MEMBER status NOTIFY changed)
    Q_PROPERTY(bool qrCodeExpired MEMBER expired NOTIFY changed)
    Q_PROPERTY(bool qrCodeLoading READ loading NOTIFY changed)
    Q_PROPERTY(bool loginHasError READ hasError NOTIFY changed)
    Q_PROPERTY(QString errorMessage MEMBER error NOTIFY changed)
    Q_PROPERTY(bool isLoggedIn READ loggedIn NOTIFY changed)
    Q_PROPERTY(bool policyDocumentLoading MEMBER policyLoading NOTIFY policyDocumentChanged)
    Q_PROPERTY(QString policyDocumentHtml MEMBER policyHtml NOTIFY policyDocumentChanged)
    Q_PROPERTY(QString policyDocumentError MEMBER policyError NOTIFY policyDocumentChanged)
    Q_PROPERTY(QString policyDocumentUrl MEMBER policyUrl NOTIFY policyDocumentChanged)
public:
    enum State { Idle, RequestingUid, WaitingForScan, WaitingForConfirm,
                 InitializingSession, Authenticated, SessionExpired, Error };
    explicit Auth(const QString &directory, QObject *parent = nullptr);
    bool loading() const { return state == RequestingUid; }
    bool hasError() const { return state == Error; }
    bool loggedIn() const { return state == Authenticated; }
    Q_INVOKABLE void startLogin();
    Q_INVOKABLE void cancelLogin();
    Q_INVOKABLE void refreshQrCode();
    Q_INVOKABLE void loadPolicyDocument(const QString &url);
    Q_INVOKABLE void cancelPolicyDocument();
    using Callback = std::function<void(QJsonObject, QString)>;
    // weread.qq.com Web API with the same session as wr_vid/wr_skey cookies: notes, underlines and reviews
    // are refused (-2012) on i.weread.qq.com for this e-ink session but served here.
    void webPost(const QString &path, const QJsonObject &body, Callback done);
    void web(const QString &path, const QList<QPair<QString, QString>> &query, Callback done);
signals:
    void changed();
    void policyDocumentChanged();
    void sessionReady();
protected:
    QNetworkRequest progressRequest(const QString &path) const;
    void progressContext(const QString &book, Callback done);
    void progressPost(const QJsonObject &body, Callback done);
    QNetworkRequest apiRequest(const QUrl &url, int timeout) const;
    void request(const QString &path, const QJsonObject &body, int timeout, Callback done,
                 bool post = false, bool login = false, bool replay = false);
    void requestUid();
    void pollLogin();
    void finishLogin(QJsonObject result);
    void resolveSession(const QJsonObject &result);
    void renewSession(std::function<void(bool)> done);
    void fail(const QString &message);
    void setState(State next, const QString &message);
    QNetworkAccessManager network;
    CookieStore *cookies;
    QTimer pollTimer, expiryTimer, retryTimer;
    QString directory, deviceId, uid, qrUrl, status, error, sessionVid, accessToken, refreshToken;
    QString policyHtml, policyError, policyUrl;
    State state = Idle;
    bool expired = false, policyLoading = false, renewing = false;
    QList<std::function<void(bool)>> renewWaiters;
    int generation = 0, uidRetries = 0, pollRetries = 0, policyGeneration = 0;
};
