// input: recovered chapter HTML; output: native paginated text; pos: ARMv7 renderer adapted to official QML interface.
#pragma once
#include <QQuickPaintedItem>
#include <QTextDocument>
#include <QVariantMap>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QImage>
#include <QHash>
class Reader : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(int currentPage READ page WRITE setPage NOTIFY currentPageChanged)
    Q_PROPERTY(int totalPages READ pages NOTIFY totalPagesChanged)
    Q_PROPERTY(QString chapterTitle MEMBER title NOTIFY titleChanged)
    Q_PROPERTY(int fontSize MEMBER fontSize NOTIFY layoutSettingsChanged)
    Q_PROPERTY(double lineHeight MEMBER lineHeight NOTIFY layoutSettingsChanged)
    Q_PROPERTY(int pageMargin MEMBER margin NOTIFY layoutSettingsChanged)
    Q_PROPERTY(QString fontFamily MEMBER family NOTIFY layoutSettingsChanged)
    Q_PROPERTY(bool firstLineIndent MEMBER indent NOTIFY layoutSettingsChanged)
public:
    enum HitType { NONE = 0, NOTE = 1 };
    Q_ENUM(HitType)
    explicit Reader(QQuickItem *parent = nullptr);
    void paint(QPainter *painter) override;
    int page() const { return current; }
    int pages() const { return count; }
    void setPage(int value);
    Q_INVOKABLE void setContent(QVariantList values);
    Q_INVOKABLE void setContentAtEnd(QVariantList values) { setContent(values); setPage(count - 1); }
    Q_INVOKABLE bool nextPage();
    Q_INVOKABLE bool prevPage();
    Q_INVOKABLE QVariantMap localCursor() const { return {{"block", cursorBlock}, {"offset", cursorOffset}}; }
    Q_INVOKABLE bool restoreProgress(QString summary, int chapterOffset);
    Q_INVOKABLE QVariantMap progressPosition(int block, int offset) const;
    Q_INVOKABLE void setCursor(int block, int offset);
    Q_INVOKABLE void setBookLayoutPolicy(bool, bool) {}
    Q_INVOKABLE void setFontSize(int value) { fontSize = qBound(18, value, 80); relayout(); }
    Q_INVOKABLE void setLineHeight(double value) { lineHeight = qBound(1.0, value, 3.0); relayout(); }
    Q_INVOKABLE void setPageMargin(int value) { margin = qBound(8, value, 120); relayout(); }
    Q_INVOKABLE void setFontFamily(QString value, bool = false) { family = value; relayout(); }
    Q_INVOKABLE void setFirstLineIndent(bool value) { indent = value; relayout(); }
    Q_INVOKABLE QVariantMap layoutSettings() const;
    Q_INVOKABLE void applyLayoutSettings(QVariantMap values);
    Q_INVOKABLE QString layoutFingerprint() const;
    Q_INVOKABLE QVariantList currentPageImageRects() const { return {}; }
    // WeRead marks for this chapter, {text, range, mine}: popular ones dashed, mine shaded; taps return NOTE hits.
    Q_INVOKABLE void setMarks(QVariantList values);
    Q_INVOKABLE QVariantMap hitTestAt(double x, double y) const;
signals:
    void currentPageChanged();
    void totalPagesChanged();
    void titleChanged();
    void layoutSettingsChanged();
    void cursorChanged(int blockIndex, int charOffset);
private:
    void relayout();
    void reportCursor();
    void applyMarks();
    struct Span { int start, end; QString range; };
    QVariantList marks;
    QVector<Span> spans;
    QString prepareHtml(const QString &html, const QUrl &base);
    QNetworkAccessManager imageNetwork;
    QHash<QString, QImage> images;
    QHash<QString, QNetworkReply *> imageRequests;
    int imageGeneration = 0, cursorBlock = 0, cursorOffset = 0;
    QTextDocument document;
    QVariantList source;
    QString title, family = "Noto Sans CJK SC";
    int current = 0, count = 0, fontSize = 32, margin = 48;
    double lineHeight = 1.65;
    bool indent = false;
};
