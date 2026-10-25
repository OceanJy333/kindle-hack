// input: original QML layout commands; output: Qt native page drawing; pos: Voyage rendering adapter.
#include "reader.h"
#include <QPainter>
#include <QAbstractTextDocumentLayout>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextLayout>
Reader::Reader(QQuickItem *parent) : QQuickPaintedItem(parent) {
    setAntialiasing(true); setOpaquePainting(true);
    connect(this, &QQuickItem::widthChanged, this, &Reader::relayout);
    connect(this, &QQuickItem::heightChanged, this, &Reader::relayout);
}
void Reader::paint(QPainter *painter) {
    painter->fillRect(boundingRect(), Qt::white);
    if (!count) return;
    painter->save(); painter->setClipRect(boundingRect());
    painter->translate(margin, -current * height());
    QAbstractTextDocumentLayout::PaintContext context;
    context.clip = QRectF(0, current * height(), width() - 2 * margin, height());
    context.palette.setColor(QPalette::Text, Qt::black);
    document.documentLayout()->draw(painter, context); painter->restore();
}
void Reader::setContent(QVariantList values) {
    ++imageGeneration;
    const auto pending = imageRequests.values(); imageRequests.clear();
    for (auto reply : pending) reply->abort();
    images.clear(); source = values; cursorBlock = cursorOffset = 0; current = 0; relayout();
}
void Reader::relayout() {
    if (width() < 100 || height() < 100) return;
    document.clear(); QFont font(family); font.setPixelSize(fontSize); document.setDefaultFont(font);
    document.setDocumentMargin(0);
    QString html;
    for (auto value : source) {
        auto block = value.toMap();
        if (html.isEmpty()) document.setBaseUrl(QUrl(block.value("baseUrl").toString()));
        html += prepareHtml(block.value("richHtml").toString(), QUrl(block.value("baseUrl").toString()));
    }
    document.setDefaultStyleSheet(QString("body,p { font-family: '%1'; font-size: %2px; color:black; } img { max-width: 800px; }").arg(family).arg(fontSize));
    document.setHtml(html);
    for (auto it = images.begin(); it != images.end(); ++it) document.addResource(QTextDocument::ImageResource, QUrl(it.key()), it.value());
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        QTextCursor cursor(block); auto format = block.blockFormat();
        bool imageBlock = block.text().contains(QChar::ObjectReplacementCharacter);
        format.setLineHeight(imageBlock ? 100 : lineHeight * 100, QTextBlockFormat::ProportionalHeight);
        format.setTopMargin(imageBlock ? 0 : fontSize / 3); format.setBottomMargin(imageBlock ? 0 : fontSize / 3);
        if (indent) format.setTextIndent(fontSize * 2);
        cursor.setBlockFormat(format);
        auto charFormat = block.charFormat(); charFormat.setFont(font); cursor.select(QTextCursor::BlockUnderCursor); cursor.mergeCharFormat(charFormat);
    }
    applyMarks();
    document.setPageSize(QSizeF(width() - 2 * margin, height()));
    count = source.isEmpty() ? 0 : document.pageCount(); current = qBound(0, current, qMax(0, count - 1));
    update(); emit totalPagesChanged(); emit currentPageChanged(); emit layoutSettingsChanged();
}
void Reader::setPage(int value) {
    if (!count) return;
    int next = qBound(0, value, count - 1); if (next == current) return;
    current = next; qInfo("READER_PAGE %d/%d", current + 1, count); update(); emit currentPageChanged(); reportCursor();
}
bool Reader::nextPage() { if (current + 1 >= count) return false; setPage(current + 1); return true; }
bool Reader::prevPage() { if (!current) return false; setPage(current - 1); return true; }
void Reader::reportCursor() {
    for (QTextBlock b = document.begin(); b.isValid(); b = b.next()) {
        auto rect = document.documentLayout()->blockBoundingRect(b);
        if (rect.bottom() < current * height()) continue;
        int offset = 0;
        if (auto layout = b.layout()) for (int i = 0; i < layout->lineCount(); ++i) {
            auto line = layout->lineAt(i);
            if (rect.top() + line.y() >= current * height()) { offset = line.textStart(); break; }
        }
        cursorBlock=b.blockNumber();cursorOffset=offset;
        emit cursorChanged(cursorBlock, cursorOffset); return;
    }
}
void Reader::setCursor(int index, int offset) {
    auto block = document.findBlockByNumber(index); if (!block.isValid()) return;
    auto rect = document.documentLayout()->blockBoundingRect(block); double y = rect.top();
    if (block.layout()) { auto line = block.layout()->lineForTextPosition(offset); if (line.isValid()) y += line.y(); }
    setPage(int(y / height())); reportCursor();
}
QVariantMap Reader::layoutSettings() const { return {{"fontSize", fontSize}, {"lineHeight", lineHeight}, {"pageMargin", margin}, {"fontFamily", family}, {"firstLineIndent", indent}}; }
void Reader::applyLayoutSettings(QVariantMap values) {
    if (values.isEmpty()) return;
    fontSize = qBound(18, values.value("fontSize", 32).toInt(), 80);
    lineHeight = qBound(1.0, values.value("lineHeight", 1.65).toDouble(), 3.0);
    margin = qBound(8, values.value("pageMargin", 48).toInt(), 120);
    family = values.value("fontFamily", "Noto Sans CJK SC").toString(); indent = values.value("firstLineIndent", false).toBool(); relayout();
}
QString Reader::layoutFingerprint() const { return QString("%1:%2:%3:%4:%5:%6").arg(width()).arg(height()).arg(fontSize).arg(lineHeight).arg(margin).arg(family); }
