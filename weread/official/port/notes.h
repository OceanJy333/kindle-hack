// input: open book and chapter from Library, WeRead Web read-only note APIs; output: marks, thoughts, my notes, reviews and book info for QML; pos: reader social layer.
#pragma once
#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
class Library;
class Notes : public QObject {
    Q_OBJECT
    // Current chapter: popular underlines merged with mine, in text order, each {text, range, count, mine, thought}.
    Q_PROPERTY(QVariantList chapterMarks READ chapterMarks NOTIFY changed)
    // Current chapter: readers' public thoughts, each {author, quote, content, range}.
    Q_PROPERTY(QVariantList chapterThoughts READ chapterThoughts NOTIFY changed)
    // Whole book, my highlights and thoughts in chapter order: {chapter, text, thought}.
    Q_PROPERTY(QVariantList myNotes MEMBER mine NOTIFY changed)
    // Whole book reviews (点评): {author, content, star}.
    Q_PROPERTY(QVariantList bookReviews MEMBER bookReviews NOTIFY changed)
    Q_PROPERTY(QVariantMap bookInfo MEMBER info NOTIFY changed)
    Q_PROPERTY(QString status MEMBER status NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(QString reviewRange MEMBER reviewRange NOTIFY changed)
    // Direct paragraph endpoint; compact cached rows retain counts and timestamps.
    Q_PROPERTY(QVariantList reviews READ reviews NOTIFY changed)
    Q_PROPERTY(bool reviewsLoading MEMBER rangeLoading NOTIFY changed)
    Q_PROPERTY(QString reviewSort MEMBER reviewSort NOTIFY changed)
    Q_PROPERTY(QString reviewStatus MEMBER reviewStatus NOTIFY changed)
    Q_PROPERTY(int reviewTotal MEMBER reviewTotal NOTIFY changed)
    Q_PROPERTY(bool reviewsHasMore MEMBER rangeHasMore NOTIFY changed)
public:
    explicit Notes(Library *library);
    QVariantList chapterMarks() const;
    QVariantList chapterThoughts() const;
    QVariantList reviews() const;
    bool loading() const { return pending > 0; }
    Q_INVOKABLE void reload();
    Q_INVOKABLE void loadReviews(const QString &range);
    Q_INVOKABLE void loadMoreReviews();
    Q_INVOKABLE void closeReviews() { ++reviewGeneration; rangeLoading=false; }
    Q_INVOKABLE void setReviewSort(const QString &sort);
    // Bubble text for a tapped mark: the mark, its reader count, my thought and readers' thoughts.
    Q_INVOKABLE QString noteFor(const QString &range) const;
signals:
    void changed();
private:
    void fetchRange(bool more);
    QString rangeKey() const;
    QString reviewSort="likes",reviewStatus,reviewChapter;
    QJsonArray rangeRows;
    int reviewGeneration=0,reviewTotal=0,reviewOffset=0;
    bool rangeLoading=false,rangeHasMore=false;
    void openBook();
    void openChapter();
    void fetch(const QString &key, const QString &path, const QList<QPair<QString, QString>> &query);
    void rebuild();
    void save();
    QString cachePath() const;
    QString chapterKey() const;
    Library *library;
    QString bookId, status, reviewRange;
    QJsonObject raw;               // last responses by key; chapter keys are "u:<uid>" and "t:<uid>"
    QVariantList own, mine, bookReviews;
    QVariantMap info, chapterNames;
    QSet<QString> inflight, fresh;   // fresh: cached chapter keys to refetch once while online
    int pending = 0, generation = 0;
};
