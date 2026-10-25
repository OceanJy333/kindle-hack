// input: Web reader position and session; output: signed progress payload; pos: native sync protocol.
#pragma once
#include <QJsonObject>
#include <QString>
namespace ProgressProtocol {
QString encode(const QString &value);
QString sign(const QJsonObject &value);
QJsonObject payload(const QString &book, const QJsonObject &position, const QJsonObject &context, bool enter);
constexpr auto userAgent = "Mozilla/5.0 WeRead Voyage";
}
