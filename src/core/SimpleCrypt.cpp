#include "SimpleCrypt.h"

#include <QByteArray>

namespace {
// 固定密钥（本机级混淆，不做安全承诺）
const QByteArray kKey = QByteArrayLiteral("DataMG::ConnStore::2026#salt");
}

QByteArray SimpleCrypt::xorCipher(const QByteArray &data)
{
    QByteArray out;
    out.resize(data.size());
    for (int i = 0; i < data.size(); ++i)
        out[i] = static_cast<char>(data[i] ^ kKey[i % kKey.size()]);
    return out;
}

QString SimpleCrypt::encode(const QString &plain)
{
    if (plain.isEmpty())
        return {};
    const QByteArray raw = xorCipher(plain.toUtf8());
    return QString::fromLatin1(PREFIX) + QString::fromLatin1(raw.toBase64());
}

QString SimpleCrypt::decode(const QString &cipher)
{
    const QString prefix = QString::fromLatin1(PREFIX);
    if (!cipher.startsWith(prefix))
        return cipher;   // 兼容历史明文
    const QByteArray raw = QByteArray::fromBase64(cipher.mid(prefix.size()).toLatin1());
    return QString::fromUtf8(xorCipher(raw));
}
