// input: authorized chapter archive and server key; output: memory-only entries; pos: recovered archive reader.
#pragma once
#include <QMap>
#include <QByteArray>
#include <QString>
QMap<QString, QByteArray> unpackChapter(const QByteArray &bytes, const QByteArray &key, QString &error);

QByteArray decodeChapterKey(const QByteArray &header, const QString &vid, QString &error);
