// input: Library book/chapter state and weread.qq.com Web note endpoints; output: Notes models with an offline cache; pos: reader social layer.
// Note reads use GET; paragraph reviews use a read-only POST in notes-reviews.cpp. Ranges use original XHTML offsets (verified
// against the cached chapter 27 of book 41598972: offset 19085 starts the marked sentence).
#include "notes.h"
#include "review-data.h"
#include "library.h"
#include "offline-store.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QTextDocumentFragment>
#include <algorithm>

static int rangeStart(const QString &range) { return range.section('-', 0, 0).toInt(); }
static int rangeEnd(const QString &range) { return range.section('-', 1, 1).toInt(); }
static bool overlaps(const QString &a, const QString &b) {
    return rangeStart(a) < rangeEnd(b) && rangeStart(b) < rangeEnd(a);
}
static QString text(const QJsonValue &value) { return value.toVariant().toString().trimmed(); }
static QJsonObject reviewOf(QJsonObject row) {
    while (row.value("review").isObject()) row = row.value("review").toObject();
    return row;
}
// Underlines carry only offsets; the words come from the chapter's own XHTML.
static QString slice(const QString &html, const QString &range) {
    int start = rangeStart(range), end = rangeEnd(range);
    if (end <= start || start >= html.size()) return {};
    QString part = html.mid(start, end - start);
    int close = part.indexOf('>'), open = part.indexOf('<');
    if (close >= 0 && (open < 0 || close < open)) part = part.mid(close + 1);
    if (part.lastIndexOf('<') > part.lastIndexOf('>')) part = part.left(part.lastIndexOf('<'));
    return QTextDocumentFragment::fromHtml(part).toPlainText().simplified();
}

// Keep comment counts and dates for sorting; omit avatar payloads and redundant book copies.
static QJsonObject compact(QJsonObject data) {
    if (!data.contains("reviews")) return data;
    QJsonArray rows;
    for (const auto &value : data.value("reviews").toArray()) {
        rows.append(ReviewData::compactRow(value.toObject()));
    }
    return QJsonObject{{"reviews", rows}};
}

Notes::Notes(Library *owner) : QObject(owner), library(owner) {
    connect(library, &Library::pageChanged, this, &Notes::openBook);
    connect(library, &Library::currentBlocksChanged, this, &Notes::openChapter);
    connect(library, &Library::chapterListChanged, this, &Notes::rebuild);
}
QString Notes::cachePath() const { return library->bookCache(bookId, library->bookVersion()) + "/notes.json"; }
QString Notes::chapterKey() const { return QString::number(library->currentChapterUid()); }
void Notes::save() { OfflineStore::write(cachePath(), QJsonDocument(raw).toJson(QJsonDocument::Compact)); }

void Notes::openBook() {
    if (library->property("currentPage").toString() != "reader") { bookId.clear(); ++generation; ++reviewGeneration; rangeLoading=false; pending = 0; inflight.clear(); fresh.clear(); return; }
    if (library->id() == bookId) return;
    bookId = library->id(); ++generation; ++reviewGeneration; rangeLoading=false; rangeRows={}; pending = 0; inflight.clear(); fresh.clear(); reviewRange.clear();
    raw = OfflineStore::read(cachePath());
    rebuild(); reload();
}
void Notes::reload() {
    if (bookId.isEmpty()) return;
    if (library->isOfflineMode() || !library->loggedIn()) {
        status = raw.isEmpty() ? "离线中，联网后可看划线和想法" : "离线中 · 显示上次同步的内容";
        emit changed(); return;
    }
    status.clear();
    // Online opens refresh chapter data too; the cached copies stay for offline reading.
    for (const auto &key : raw.keys()) if (key.startsWith("u:") || key.startsWith("t:")) fresh.insert(key);
    fetch("best", "/web/book/bestbookmarks", {{"bookId", bookId}});
    fetch("marks", "/web/book/bookmarklist", {{"bookId", bookId}});
    fetch("mine", "/web/review/list", {{"bookId", bookId}, {"listType", "11"}, {"mine", "1"}, {"synckey", "0"}});
    fetch("reviews", "/web/review/list", {{"bookId", bookId}, {"listType", "3"}, {"synckey", "0"}, {"maxIdx", "0"}, {"count", "10"}});
    fetch("info", "/web/book/info", {{"bookId", bookId}});
    openChapter();
}
// One request pair per chapter, kept in the book cache so returning to a chapter or reading offline needs none.
void Notes::openChapter() {
    if (bookId.isEmpty() || library->currentChapterUid() <= 0) return;
    const QString uid = chapterKey();
    if(reviewChapter!=uid){++reviewGeneration;reviewChapter=uid;reviewRange.clear();rangeLoading=false;rangeRows={};reviewStatus.clear();}
    emit changed();
    if ((raw.contains("u:" + uid) && !fresh.contains("u:" + uid)) || library->isOfflineMode() || !library->loggedIn()) return;
    fetch("u:" + uid, "/web/book/underlines", {{"bookId", bookId}, {"chapterUid", uid}});
    fetch("t:" + uid, "/web/review/list", {{"bookId", bookId}, {"chapterUid", uid}, {"listType", "8"},
                                          {"synckey", "0"}, {"maxIdx", "0"}, {"count", "100"}});
}
void Notes::fetch(const QString &key, const QString &path, const QList<QPair<QString, QString>> &query) {
    if (inflight.contains(key)) return;
    inflight.insert(key); ++pending; const int current = generation;
    library->web(path, query, [this, key, current](QJsonObject data, QString message) {
        if (current != generation) return;
        inflight.remove(key); --pending;
        if (message.isEmpty()) { raw[key] = compact(data); fresh.remove(key); }
        else status = message == "微信读书登录已过期" ? "登录已过期，重新扫码后可看划线" : "部分内容未同步";
        if (!pending) save();
        rebuild();
    });
    emit changed();
}

void Notes::rebuild() {
    chapterNames.clear();
    for (const auto &item : library->property("chapterList").toList()) {
        auto c = item.toMap(); chapterNames[c.value("chapterUid").toString()] = c.value("title");
    }
    QHash<QString, QString> thoughts;   // chapterUid:range -> my thought
    QVariantList loose;
    for (const auto &value : raw.value("mine").toObject().value("reviews").toArray()) {
        auto r = reviewOf(value.toObject()); auto content = text(r.value("content"));
        if (content.isEmpty()) continue;
        auto uid = text(r.value("chapterUid")), range = text(r.value("range"));
        if (!range.isEmpty()) thoughts[uid + ":" + range] = content;
        loose.append(QVariantMap{{"chapterUid", uid}, {"range", range}, {"text", text(r.value("abstract"))}, {"thought", content}});
    }
    own.clear();
    for (const auto &value : raw.value("marks").toObject().value("updated").toArray()) {
        auto m = value.toObject(); auto uid = text(m.value("chapterUid")), range = text(m.value("range"));
        if (text(m.value("markText")).isEmpty()) continue;
        own.append(QVariantMap{{"chapterUid", uid}, {"range", range}, {"text", text(m.value("markText"))}, {"thought", thoughts.take(uid + ":" + range)}});
    }
    mine = own;
    for (const auto &row : loose) {
        auto r = row.toMap(); auto key = r.value("chapterUid").toString() + ":" + r.value("range").toString();
        if (thoughts.contains(key) || r.value("range").toString().isEmpty()) mine.append(r);
    }
    QHash<QString, int> order; int index = 0;
    for (const auto &item : library->property("chapterList").toList()) order[item.toMap().value("chapterUid").toString()] = index++;
    std::stable_sort(mine.begin(), mine.end(), [&](const QVariant &a, const QVariant &b) {
        auto x = a.toMap(), y = b.toMap();
        int cx = order.value(x.value("chapterUid").toString(), 1 << 20), cy = order.value(y.value("chapterUid").toString(), 1 << 20);
        return cx != cy ? cx < cy : rangeStart(x.value("range").toString()) < rangeStart(y.value("range").toString());
    });
    for (auto &row : mine) { auto r = row.toMap(); r["chapter"] = chapterNames.value(r.value("chapterUid").toString()); row = r; }
    bookReviews.clear();
    for (const auto &value : raw.value("reviews").toObject().value("reviews").toArray()) {
        auto r = reviewOf(value.toObject()); auto content = text(r.value("content"));
        if (!content.isEmpty()) bookReviews.append(QVariantMap{{"author", text(r.value("author").toObject().value("name"))},
                                                               {"content", content}, {"level", r.value("newRatingLevel").toInt()}});
    }
    auto b = raw.value("info").toObject();
    info = b.isEmpty() ? QVariantMap{} : QVariantMap{{"title", text(b.value("title"))}, {"author", text(b.value("author"))},
        {"translator", text(b.value("translator"))}, {"intro", text(b.value("intro"))}, {"category", text(b.value("category"))},
        {"publisher", text(b.value("publisher"))}, {"rating", b.value("newRating").toInt()}, {"ratingCount", b.value("newRatingCount").toInt()}};
    emit changed();
}
QVariantList Notes::chapterMarks() const {
    const QString uid = chapterKey();
    QString html;
    for (const auto &block : library->currentBlocks()) html += block.toMap().value("richHtml").toString();
    QVariantList out; QHash<QString, int> at;
    // Popular underlines: the 40 most marked ones, so the page shows what most readers marked, not every overlap.
    auto lines = raw.value("u:" + uid).toObject().value("underlines").toArray().toVariantList();
    std::stable_sort(lines.begin(), lines.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value("count").toInt() > b.toMap().value("count").toInt(); });
    for (const auto &item : lines.mid(0, 40)) {
        auto u = item.toMap(); auto range = u.value("range").toString(), words = slice(html, range);
        if (words.size() < 2) continue;
        at[range] = out.size();
        out.append(QVariantMap{{"range", range}, {"text", words}, {"count", u.value("count").toInt()}, {"mine", false}});
    }
    for (const auto &item : own) {
        auto m = item.toMap(); if (m.value("chapterUid").toString() != uid) continue;
        auto range = m.value("range").toString();
        if (at.contains(range)) { auto p = out[at[range]].toMap(); p["mine"] = true; p["thought"] = m.value("thought"); out[at[range]] = p; }
        else { m["mine"] = true; m["count"] = 0; out.append(m); }
    }
    std::stable_sort(out.begin(), out.end(), [](const QVariant &a, const QVariant &b) {
        return rangeStart(a.toMap().value("range").toString()) < rangeStart(b.toMap().value("range").toString()); });
    return out;
}
QVariantList Notes::chapterThoughts() const {
    QVariantList out;
    for (const auto &value : raw.value("t:" + chapterKey()).toObject().value("reviews").toArray()) {
        auto r = reviewOf(value.toObject()); auto content = text(r.value("content"));
        if (content.isEmpty()) continue;
        out.append(QVariantMap{{"author", text(r.value("author").toObject().value("name"))}, {"quote", text(r.value("abstract"))},
                               {"content", content}, {"range", text(r.value("range"))}});
    }
    return out;
}
QString Notes::noteFor(const QString &range) const {
    QVariantMap mark;
    for (const auto &item : chapterMarks()) if (item.toMap().value("range").toString() == range) mark = item.toMap();
    if (mark.isEmpty()) return {};
    int count = mark.value("count").toInt();
    QString out = count > 0 ? QString("%1 人划线").arg(count) : QString("我的划线");
    if (!mark.value("thought").toString().isEmpty()) out += "\n我的想法：" + mark.value("thought").toString();
    for (const auto &item : chapterThoughts()) {
        auto r = item.toMap(); if (!overlaps(r.value("range").toString(), range)) continue;
        out += "\n\n" + (r.value("author").toString().isEmpty() ? QString() : r.value("author").toString() + "：") + r.value("content").toString();
    }
    return out;
}
