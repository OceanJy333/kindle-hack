// input: native services and standalone QML; output: Kindle reader or legacy desk; pos: Qt entry.
#include "desk.h"
#include "native-screen.h"
#include "library.h"
#include "notes.h"
#include "reader.h"
#include "voyage.h"
#include "eink.h"
#include "wifi.h"
#include <QStyleHints>
#include <QGuiApplication>
#include <QFontDatabase>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickImageProvider>
#include <QPainter>
#include <QDir>
#include <QSslConfiguration>
#include <QSslCertificate>
#include <qrencode.h>
#include <memory>
#include <csignal>

static volatile std::sig_atomic_t publicExitRequested = 0;
static void requestPublicExit(int signal) { publicExitRequested = signal; }

class QrProvider final : public QQuickImageProvider {
public:
    QrProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
    QImage requestImage(const QString &id, QSize *size, const QSize &) override {
        QByteArray text = QUrl::fromPercentEncoding(id.toUtf8()).toUtf8();
        auto code = QRcode_encodeString(text.constData(), 0, QR_ECLEVEL_M, QR_MODE_8, 1);
        if (!code) return {};
        const int margin = 4, scale = 8;
        QImage image((code->width + 2 * margin) * scale,
                     (code->width + 2 * margin) * scale, QImage::Format_RGB32);
        image.fill(Qt::white);
        QPainter painter(&image);
        for (int y = 0; y < code->width; ++y) for (int x = 0; x < code->width; ++x)
            if (code->data[y * code->width + x] & 1)
                painter.fillRect((x + margin) * scale, (y + margin) * scale, scale, scale, Qt::black);
        painter.end(); QRcode_free(code); *size = image.size(); return image;
    }
};
int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    const bool publicReader = qEnvironmentVariable("OCEAN_STOCK_APP") == "1";
    // A blinking caret would refresh the physical e-ink panel while typing.
    app.styleHints()->setCursorFlashTime(0);
    QGuiApplication::setApplicationName("Ocean WeRead");
    const auto root = QCoreApplication::applicationDirPath();
    // Qt scans only fixed system cert dirs, which this rootfs lacks; load the bundled roots instead.
    auto ssl = QSslConfiguration::defaultConfiguration();
    ssl.setCaCertificates(QSslCertificate::fromPath(root + "/ca-certificates.crt"));
    QSslConfiguration::setDefaultConfiguration(ssl);
    auto id = QFontDatabase::addApplicationFont(root + "/fonts/NotoSansCJKsc-Regular.otf");
    if (id < 0) qFatal("Bundled CJK font missing");
    auto family = QFontDatabase::applicationFontFamilies(id).value(0);
    app.setFont(QFont(family));
    Library auth(qEnvironmentVariable("WEREAD_STATE_DIR", "/var/local/weread-port"));
    Notes notes(&auth);
    QTimer exitPoll;
    if (publicReader) {
        std::signal(SIGUSR2, requestPublicExit);
        std::signal(SIGTERM, requestPublicExit);
    }
    Eink eink;
    std::unique_ptr<Desk> desk;
    std::unique_ptr<VoyagePen> pen;
    std::unique_ptr<VoyageFonts> fonts;
    std::unique_ptr<Wifi> wifi;
    if (!publicReader) {
        desk = std::make_unique<Desk>(); pen = std::make_unique<VoyagePen>();
        fonts = std::make_unique<VoyageFonts>(); wifi = std::make_unique<Wifi>();
        QObject::connect(desk->power(), &PowerClient::lockedChanged, wifi.get(), &Wifi::setSuspended);
    }
    qmlRegisterType<Reader>("remarkable_weread", 1, 0, "ReaderPageItem");
    if (!publicReader) qmlRegisterType<NoPenCanvas>("remarkable_weread", 1, 0, "InkCanvasItem");
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("eink", &eink);
    if (!publicReader) {
        engine.rootContext()->setContextProperty("desk", desk.get());
        engine.rootContext()->setContextProperty("wifi", wifi.get());
        engine.rootContext()->setContextProperty("pen", pen.get());
        engine.rootContext()->setContextProperty("readerFonts", fonts.get());
    }
    // Panels narrower than the 1072px canvas get a window at panel size; QML scales the logical layout into it.
    int panelWidth = 1072, panelHeight = 1448, probedWidth = 0, probedHeight = 0;
    if (publicReader && display_panel_size(&probedWidth, &probedHeight) == 0 && probedWidth < 1072) {
        panelWidth = probedWidth; panelHeight = probedHeight;
    }
    engine.rootContext()->setContextProperty("panelWidth", panelWidth);
    engine.rootContext()->setContextProperty("panelHeight", panelHeight);
    engine.rootContext()->setContextProperty("panelScale", panelWidth / 1072.0);
    engine.rootContext()->setContextProperty("appVersion", "1.0.0 · Voyage 移植验证");
    engine.addImportPath(root + "/qml-modules");
    engine.addImageProvider("qr", new QrProvider);
    engine.rootContext()->setContextProperty("uiFontFamily", family);
    engine.rootContext()->setContextProperty("appController", &auth);
    engine.rootContext()->setContextProperty("bookNotes", &notes);
    engine.load(QUrl::fromLocalFile(root + (publicReader ? "/ReaderApp.qml" : "/LoginHarness.qml")));
    if (engine.rootObjects().isEmpty()) return 1;
    auto window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    NativeScreen screen(window, desk.get());
    if (publicReader) {
        QObject::connect(&exitPoll, &QTimer::timeout, &auth, [&screen, window] {
            if (publicExitRequested) {
                const bool immediately = publicExitRequested == SIGUSR2;
                publicExitRequested = 0; screen.stopUpdates();
                QMetaObject::invokeMethod(window, "exitReader", Q_ARG(QVariant, QVariant(immediately)));
            }
        });
        exitPoll.start(100);
    }
    QObject::connect(&eink,&Eink::refreshRequested,&screen,[&screen](QRect rect){screen.clean(rect);});
    return app.exec();
}
