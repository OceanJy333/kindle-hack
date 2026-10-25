// input: account-scoped book data; output: atomic offline files; pos: persistent cache boundary.
#pragma once
#include <QJsonObject>
#include <QVariantList>
#include <QMap>
namespace OfflineStore {
QString hash(const QString &value);
QJsonObject read(const QString &path);
bool write(const QString &path, const QByteArray &bytes);
qint64 bytes(const QString &path);
QString imagePath(const QString &base, const QString &url);
QVariantList storeChapter(const QString &root, const QMap<QString,QByteArray> &files, const QVariantMap &chapter, QString &error);
QVariantList missingImages(const QVariantList &blocks);
}
