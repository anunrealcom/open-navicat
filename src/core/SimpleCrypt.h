#pragma once

#include <QString>

/// 轻量对称混淆（XOR + Base64）。
/// 注意：这不是强加密，仅避免密码明文落盘；
/// 生产环境建议替换为 Windows Credential Manager / QtKeychain。
class SimpleCrypt
{
public:
    static QString encode(const QString &plain);
    static QString decode(const QString &cipher);

private:
    static QByteArray xorCipher(const QByteArray &data);
    static constexpr char PREFIX[] = "enc.v1.";
};
