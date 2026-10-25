// input: server encryptkey and authenticated vid; output: archive key; pos: reconstruction of official 0x1d9ee0.
#include "archive.h"
#include <openssl/evp.h>
#include <algorithm>
QByteArray decodeChapterKey(const QByteArray &header, const QString &vid, QString &error) {
    if (header.isEmpty()) return {};
    if (vid.isEmpty()) { error = "missing authenticated user"; return {}; }
    QByteArray material;
    while (material.size() < 32) material += vid.toUtf8();
    material.truncate(32);
    bool hex = header.size() % 2 == 0 && std::all_of(header.begin(), header.end(), [](unsigned char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    });
    auto encrypted = hex ? QByteArray::fromHex(header) : QByteArray::fromBase64(header);
    QByteArray output(encrypted.size() + 16, 0);
    auto ctx = EVP_CIPHER_CTX_new();
    int size = 0, tail = 0;
    bool ok = ctx && EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), nullptr,
        reinterpret_cast<const unsigned char *>(material.constData()),
        reinterpret_cast<const unsigned char *>(material.constData() + 16)) == 1;
    ok = ok && EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char *>(output.data()), &size,
        reinterpret_cast<const unsigned char *>(encrypted.constData()), encrypted.size()) == 1;
    ok = ok && EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char *>(output.data() + size), &tail) == 1;
    EVP_CIPHER_CTX_free(ctx); material.fill(0);
    if (!ok) { error = "chapter key decode failed"; return {}; }
    output.resize(size + tail); return output;
}
