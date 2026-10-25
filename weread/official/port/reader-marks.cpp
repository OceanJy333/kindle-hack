// input: chapter marks from Notes and taps from ReaderPage; output: underlined text and NOTE hits; pos: Reader social overlay.
#include "reader.h"
#include <QAbstractTextDocumentLayout>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextLayout>

void Reader::setMarks(QVariantList values) {
    if (values == marks) return;
    marks = values;
    if (count) relayout();
}
// Marks are matched by text, not by WeRead's XHTML offsets: the rendered document is rebuilt HTML
// and its positions do not line up with the server ranges. Multi-paragraph marks match per paragraph.
void Reader::applyMarks() {
    spans.clear();
    for (const auto &value : marks) {
        const auto mark = value.toMap();
        const bool mine = mark.value("mine").toBool();
        QTextCharFormat format;
        if (mine) format.setBackground(QColor("#d6d6d6"));
        else { format.setUnderlineStyle(QTextCharFormat::DashUnderline); format.setUnderlineColor(Qt::black); }
        int from = 0;
        for (auto part : mark.value("text").toString().split('\n', Qt::SkipEmptyParts)) {
            part = part.trimmed();
            if (part.size() < 2) continue;
            auto cursor = document.find(part, from);
            if (cursor.isNull()) cursor = document.find(part);
            if (cursor.isNull()) continue;
            cursor.mergeCharFormat(format);
            spans.append({cursor.selectionStart(), cursor.selectionEnd(), mark.value("range").toString()});
            from = cursor.selectionEnd();
        }
    }
}
QVariantMap Reader::hitTestAt(double x, double y) const {
    if (!count || spans.isEmpty()) return {{"type", NONE}};
    const QPointF point(x - margin, y + current * height());
    const int position = document.documentLayout()->hitTest(point, Qt::ExactHit);
    if (position < 0) return {{"type", NONE}};
    for (const auto &span : spans) {
        if (position < span.start || position >= span.end) continue;
        auto block = document.findBlock(position);
        auto rect = document.documentLayout()->blockBoundingRect(block);
        auto line = block.layout()->lineForTextPosition(position - block.position());
        double top = rect.top() + (line.isValid() ? line.y() : 0) - current * height();
        double bottom = top + (line.isValid() ? line.height() : fontSize * lineHeight);
        return {{"type", NOTE}, {"range", span.range}, {"content", QString()},
                {"anchorLeft", x - 10}, {"anchorRight", x + 10}, {"anchorTop", top}, {"anchorBottom", bottom}};
    }
    return {{"type", NONE}};
}
