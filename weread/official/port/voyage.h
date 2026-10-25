// input: official QML hardware interfaces; output: Voyage capabilities; pos: no-pen hardware adaptation.
#pragma once
#include <QQuickItem>
class VoyagePen : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ unavailable CONSTANT)
    Q_PROPERTY(bool drawing READ unavailable CONSTANT)
    Q_PROPERTY(bool eraser READ unavailable CONSTANT)
    Q_PROPERTY(bool penNear READ unavailable CONSTANT)
public:
    bool unavailable() const { return false; }
    Q_INVOKABLE void configureRealtimeInk(bool, double, int, QRectF, QVariantList) {}
    Q_INVOKABLE bool realtimeInkAccepts(double, double, bool) const { return false; }
signals:
    void penNearChanged();
    void penDown(double x, double y, double pressure, bool eraser);
    void penMoved(double x, double y, double pressure);
    void penUp();
};
class NoPenCanvas : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(QObject* reader MEMBER reader NOTIFY hardwareChanged)
    Q_PROPERTY(QObject* store MEMBER store NOTIFY hardwareChanged)
    Q_PROPERTY(QObject* eink MEMBER eink NOTIFY hardwareChanged)
    Q_PROPERTY(double penWidth MEMBER penWidth)
    Q_PROPERTY(bool pageHasInk READ hasInk CONSTANT)
public:
    using QQuickItem::QQuickItem;
    bool hasInk() const { return false; }
    Q_INVOKABLE void rebuild() {}
    Q_INVOKABLE void cancelStroke() {}
    Q_INVOKABLE void prepareForOverlay() {}
signals:
    void hardwareChanged();
    void inkRemoved(double x, double y, double w, double h);
    void contentChanged();
private:
    QObject *reader = nullptr, *store = nullptr, *eink = nullptr;
    double penWidth = 5;
};
class VoyageFonts : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList choices READ choices CONSTANT)
    Q_PROPERTY(QString status READ status CONSTANT)
public:
    QVariantList choices() const { return {QVariantMap{{"family", "Noto Sans CJK SC"}, {"label", "思源黑体"}, {"installed", true}, {"downloading", false}, {"progress", 100}}}; }
    QString status() const { return {}; }
    Q_INVOKABLE void cancelDownload() {}
    Q_INVOKABLE void requestFont(QString family) { emit fontReady(family, false); }
signals:
    void fontReady(QString family, bool newlyInstalled);
    void statusChanged();
    void choicesChanged();
};
