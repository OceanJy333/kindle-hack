// input: official ZIP/TAR reply; output: validated entries; pos: reconstruction of 0x1cd9e0 / 0x1ce990.
#include "archive.h"
#include <QtEndian>
#include <zlib.h>
#include <stdexcept>
QMap<QString, QByteArray> unpackChapter(const QByteArray &bytes, const QByteArray &password, QString &error) {
    QMap<QString, QByteArray> files;
    constexpr int limit = 32 * 1024 * 1024;
    auto fail = [](const char *message) { throw std::runtime_error(message); };
    auto u16 = [&](int p) { if (p < 0 || p + 2 > bytes.size()) fail("truncated archive"); return qFromLittleEndian<quint16>(bytes.constData() + p); };
    auto u32 = [&](int p) { if (p < 0 || p + 4 > bytes.size()) fail("truncated archive"); return qFromLittleEndian<quint32>(bytes.constData() + p); };
    try {
        if (bytes.startsWith("PK")) {
            int p = 0, total = 0;
            while (p + 30 <= bytes.size() && u32(p) == 0x04034b50) {
                int flags = u16(p + 6), method = u16(p + 8);
                quint32 checksum = u32(p + 14), compressed = u32(p + 18), size = u32(p + 22);
                int names = u16(p + 26), extra = u16(p + 28), start = p + 30 + names + extra;
                if ((flags & 8) || size > limit || compressed > limit || start + compressed > quint32(bytes.size())) fail("unsupported archive size or descriptor");
                QString name = QString::fromUtf8(bytes.mid(p + 30, names)).replace('\\', '/');
                QByteArray payload = bytes.mid(start, compressed); p = start + compressed;
                if (flags & 1) {
                    if (password.isEmpty() || payload.size() < 12) fail("missing archive key");
                    quint32 a = 0x12345678, b = 0x23456789, c = 0x34567890;
                    const auto table = get_crc_table();
                    auto step = [&](unsigned char ch) { a = table[(a ^ ch) & 255] ^ (a >> 8); b = (b + (a & 255)) * 134775813 + 1; c = table[(c ^ (b >> 24)) & 255] ^ (c >> 8); };
                    for (unsigned char ch : password) step(ch);
                    for (int i = 0; i < payload.size(); ++i) {
                        quint32 t = c | 2; unsigned char plain = static_cast<unsigned char>(payload[i]) ^ ((t * (t ^ 1)) >> 8);
                        payload[i] = char(plain); step(plain);
                    }
                    // Official 0x1cd9e0 skips the 12-byte prefix; integrity is checked after inflate.
                    payload.remove(0, 12);
                }
                QByteArray plain;
                if (method == 0) plain = payload;
                else if (method == 8) {
                    plain.resize(size); z_stream stream{};
                    stream.next_in = reinterpret_cast<Bytef *>(payload.data()); stream.avail_in = payload.size();
                    stream.next_out = reinterpret_cast<Bytef *>(plain.data()); stream.avail_out = size;
                    if (inflateInit2(&stream, -15) != Z_OK) fail("inflate init failed");
                    int result = inflate(&stream, Z_FINISH); inflateEnd(&stream);
                    if (result != Z_STREAM_END || stream.total_out != size) fail("chapter inflate failed");
                } else fail("unsupported compression");
                if (plain.size() != size || crc32(0, reinterpret_cast<const Bytef *>(plain.constData()), plain.size()) != checksum) fail("chapter CRC mismatch");
                total += plain.size(); if (total > limit) fail("chapter too large");
                while (name.startsWith('/')) name.remove(0, 1);
                files.insert(name, plain);
            }
        } else {
            for (int p = 0; p + 512 <= bytes.size();) {
                if (bytes[p] == 0) break;
                auto name = QString::fromUtf8(bytes.mid(p, 100).split('\0').first());
                bool ok; int size = bytes.mid(p + 124, 12).split('\0').first().trimmed().toInt(&ok, 8);
                if (!ok || size < 0 || size > limit || p + 512 + size > bytes.size()) fail("invalid TAR entry");
                files.insert(name, bytes.mid(p + 512, size)); p += 512 + ((size + 511) / 512) * 512;
            }
        }
        if (files.isEmpty()) fail("empty chapter archive");
    } catch (const std::exception &e) { error = QString::fromUtf8(e.what()); files.clear(); }
    return files;
}
