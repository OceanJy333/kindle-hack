// input: restored official session and API; output: official QML models; pos: native service reconstruction.
#pragma once
#include "auth.h"
#include <QVariantList>
#include <QJsonArray>
class Library : public Auth {
    Q_OBJECT
    Q_PROPERTY(QString syncStatus MEMBER syncStatus NOTIFY syncChanged)
    Q_PROPERTY(QVariantMap cloudProgress READ cloudProgress NOTIFY syncChanged)
    Q_PROPERTY(QString downloadStatus MEMBER downloadStatus NOTIFY downloadChanged)
    Q_PROPERTY(bool downloading MEMBER downloading NOTIFY downloadChanged)
    Q_PROPERTY(bool offlineMode MEMBER offlineMode NOTIFY downloadChanged)
    Q_PROPERTY(QString cacheSize READ cacheSize NOTIFY downloadChanged)
    Q_PROPERTY(QVariantMap offlineBooks MEMBER offlineInfo NOTIFY downloadChanged)
    Q_PROPERTY(QString downloadingBook READ downloadingBook NOTIFY downloadChanged)
    Q_PROPERTY(QString currentBookTitle READ title NOTIFY pageChanged)
    Q_PROPERTY(QString currentBookId READ id NOTIFY pageChanged)
    Q_PROPERTY(QString currentPage MEMBER page NOTIFY pageChanged)
    Q_PROPERTY(QVariantList shelfBooks MEMBER books NOTIFY shelfBooksChanged)
    Q_PROPERTY(bool isLoading MEMBER busy NOTIFY shelfBooksChanged)
    Q_PROPERTY(bool isOffline MEMBER offline NOTIFY isOfflineChanged)
    Q_PROPERTY(bool loginExpired MEMBER expiredLogin NOTIFY isOfflineChanged)
    Q_PROPERTY(QVariantList chapterList MEMBER chapters NOTIFY chapterListChanged)
    Q_PROPERTY(QString currentChapterTitle MEMBER chapterTitle NOTIFY currentBlocksChanged)
    Q_PROPERTY(QString chapterListLoadError MEMBER tocError NOTIFY chapterListChanged)
    Q_PROPERTY(QString chapterLoadError MEMBER readError NOTIFY currentBlocksChanged)
    Q_PROPERTY(bool chapterLoading MEMBER reading NOTIFY currentBlocksChanged)
    Q_PROPERTY(bool chapterRequiresPurchase MEMBER purchase NOTIFY currentBlocksChanged)
    Q_PROPERTY(bool chapterUnavailable MEMBER unavailable NOTIFY currentBlocksChanged)
    Q_PROPERTY(bool hasReadableChapter READ readable NOTIFY currentBlocksChanged)
    Q_PROPERTY(int bookTotalPages MEMBER totalPages NOTIFY paginationChanged)
    Q_PROPERTY(int bookCurrentPage MEMBER currentPageNumber NOTIFY paginationChanged)
    Q_PROPERTY(QObject* inkStore READ inkStore CONSTANT)
public:
    explicit Library(const QString &directory);
    bool isEpub() const { return format.startsWith("epub") || format == "pdf"; }
    bool isUpload() const { return bookId.startsWith("CB_"); }
    bool readable() const { return !blocks.isEmpty(); }
    QObject *inkStore() const { return nullptr; }
    Q_INVOKABLE void refreshShelf();
    Q_INVOKABLE void saveProgressPosition(QVariantMap position);
    Q_INVOKABLE bool takeChapterPositionChange() { bool changed=chapterPositionDirty;chapterPositionDirty=false;return changed; }
    Q_INVOKABLE void progressRestored(bool ok);
    Q_INVOKABLE void synchronizeProgress(bool opening = false);
    QVariantMap cloudProgress() const { return cloudAnchor.toVariantMap(); }
    Q_INVOKABLE void downloadBook(QString id);
    Q_INVOKABLE void pauseDownload();
    Q_INVOKABLE void removeDownload(QString id);
    Q_INVOKABLE void setOfflineMode(bool value);
    void applyLocalCovers();
    void refreshOfflineInfo(const QString &id = {});
    QString downloadingBook() const { return downloading ? downloadTarget.value("bookId").toString() : QString(); }
    QString cacheSize() const;
    QString accountCache() const;
    QString bookCache(const QString &id, int version) const;
    QString chapterCache(int uid) const;
    QUrl chapterUrl(const QVariantMap &book, int uid) const;
    void acceptChapters(QVariantList value);
    void downloadNext();
    void downloadImages(QVariantList images, std::function<void(bool)> done);
    void storeDownloaded(int gen, QByteArray bytes, QByteArray header, QString root, QVariantMap chapter, std::function<void(bool)> images);
    void notifyProgress();

    Q_INVOKABLE void logout();
    Q_INVOKABLE void exitApp();
    Q_INVOKABLE void openBook(QString id, QString title, QString format, int version, bool soldOut, int payType, int maxFree);
    Q_INVOKABLE void goBackToShelf();
    Q_INVOKABLE void loadChapter(int uid);
    Q_INVOKABLE void nextChapter();
    Q_INVOKABLE void prevChapter();
    Q_INVOKABLE int currentChapterUid() const { return chapterUid; }
    Q_INVOKABLE int chapterStartPage(int) const { return 0; }
    Q_INVOKABLE bool openChapterAtEnd() const { return atEnd; }
    Q_INVOKABLE QVariantList currentBlocks() const { return blocks; }
    Q_INVOKABLE int resumeBlockIndex() const { return resumeBlock; }
    Q_INVOKABLE int resumeCharOffset() const { return resumeOffset; }
    Q_INVOKABLE bool currentBookIsEpub() const { return isEpub(); }
    Q_INVOKABLE bool currentBookIsUpload() const { return isUpload(); }
    Q_INVOKABLE void retryReaderLoad() { loadChapter(chapterUid); }
    Q_INVOKABLE void saveReaderCursor(int block, int offset);
    Q_INVOKABLE QVariantMap readerLayoutSettings() const;
    Q_INVOKABLE void saveReaderLayoutSettings(QVariantMap settings);
    Q_INVOKABLE void updateBookPagination(int current, int total, QString fingerprint = {});
    QString title() const { return bookTitle; }
    int bookVersion() const { return version; }
    bool isOfflineMode() const { return offlineMode; }
    QString id() const { return bookId; }
signals:
    void syncChanged();
    void downloadChanged();
    void pageChanged();
    void currentPageChanged();
    void chapterLoadStateChanged();
    void chapterContentLoaded();
    void shelfBooksChanged();
    void isOfflineChanged();
    void errorMessageChanged();
    void chapterListChanged();
    void currentBlocksChanged();
    void paginationChanged();
    void chapterOpenedAtEnd();
private:
    void startProgress();
    void openProgress();
    QString progressPath() const;
    void setSyncStatus(const QString &message);
    void applyCloudProgress(const QJsonObject &remote, bool opening);
    void uploadProgress(QString id, QString path, QJsonObject pending, std::function<bool()> valid);
    QString syncStatus;
    QTimer syncTimer;
    QJsonObject cloudAnchor;
    bool syncBusy = false, suppressChapterPosition = false, chapterPositionDirty = false;
    int syncGeneration = 0;
    qint64 lastMovement = 0, serverClockOffset = 0;
    void details(QJsonArray ids, int offset = 0);
    void showChapter(const QByteArray &archive, const QByteArray &key, const QVariantMap &chapter);
    void savePrivate(const QString &name, const QByteArray &bytes);
    QString downloadStatus;
    QVariantMap offlineInfo;
    // Coalesces per-chapter progress into one UI update per interval: each update rebuilds bindings and an e-ink region.
    bool progressPending = false;
    bool downloading = false, offlineMode = false;
    int downloadGeneration = 0, downloadIndex = 0, downloadedCount = 0, deniedCount = 0, skippedCount = 0;
    QVariantMap downloadTarget;
    QVariantList downloadChapters;
    QNetworkReply *downloadReply = nullptr;
    QString page = "login", bookId, bookTitle, format, chapterTitle, tocError, readError;
    QVariantList books, pendingBooks, chapters, blocks;
    bool busy = false, offline = false, expiredLogin = false, reading = false, purchase = false, unavailable = false, atEnd = false;
    int version = 0, chapterUid = 0, readGeneration = 0;
    int totalPages = 0, currentPageNumber = 0, chapterStart = 1, resumeBlock = 0, resumeOffset = 0;
};
