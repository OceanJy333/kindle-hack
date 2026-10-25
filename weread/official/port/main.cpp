// input: recovered official QML and rebuilt login module; output: native Qt screen; pos: Voyage integration entry.
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
    // A blinking caret would refresh the physical e-ink panel while typing.
    app.styleHints()->setCursorFlashTime(0);
    QGuiApplication::setApplicationName("WeRead Voyage recovery");
    const auto root = QCoreApplication::applicationDirPath();
    // Qt scans only fixed system cert dirs, which this rootfs lacks; load the bundled roots instead.
    auto ssl = QSslConfiguration::defaultConfiguration();
    ssl.setCaCertificates(QSslCertificate::fromPath(root + "/ca-certificates.crt"));
    QSslConfiguration::setDefaultConfiguration(ssl);
    auto id = QFontDatabase::addApplicationFont(root + "/fonts/NotoSansCJKsc-Regular.otf");
    if (id < 0) qFatal("Original CJK font missing");
    auto family = QFontDatabase::applicationFontFamilies(id).value(0);
    app.setFont(QFont(family));
    Library auth(qEnvironmentVariable("WEREAD_STATE_DIR", "/var/local/weread-port"));
    Notes notes(&auth);
    Desk desk; Eink eink; VoyagePen pen; VoyageFonts fonts; Wifi wifi;
    QObject::connect(desk.power(),&PowerClient::lockedChanged,&wifi,&Wifi::setSuspended);
    qmlRegisterType<Reader>("remarkable_weread", 1, 0, "ReaderPageItem");
    qmlRegisterType<NoPenCanvas>("remarkable_weread", 1, 0, "InkCanvasItem");
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("eink", &eink);
    engine.rootContext()->setContextProperty("desk", &desk);
    engine.rootContext()->setContextProperty("wifi", &wifi);
    engine.rootContext()->setContextProperty("pen", &pen);
    engine.rootContext()->setContextProperty("readerFonts", &fonts);
    engine.rootContext()->setContextProperty("appVersion", "1.0.0 · Voyage 移植验证");
    engine.addImportPath(root + "/qml-modules");
    engine.addImageProvider("qr", new QrProvider);
    engine.rootContext()->setContextProperty("uiFontFamily", family);
    engine.rootContext()->setContextProperty("appController", &auth);
    engine.rootContext()->setContextProperty("bookNotes", &notes);
    engine.load(QUrl::fromLocalFile(root + "/LoginHarness.qml"));
    if (engine.rootObjects().isEmpty()) return 1;
    auto window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    NativeScreen screen(window,&desk);
    QObject::connect(&eink,&Eink::refreshRequested,&screen,[&screen](QRect rect){screen.clean(rect);});
    return app.exec();
}
