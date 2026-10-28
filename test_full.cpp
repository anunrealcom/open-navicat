// 模拟 DataMG 完整流程测试
#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QDebug>

// 复制自 SimpleCrypt.cpp
namespace {
const QByteArray kKey = QByteArrayLiteral("DataMG::ConnStore::2026#salt");
constexpr const char *PREFIX = "DMC1:";
QByteArray xorCipher(const QByteArray &data) {
    QByteArray out(data.size(), Qt::Uninitialized);
    for (int i = 0; i < data.size(); ++i)
        out[i] = static_cast<char>(data[i] ^ kKey[i % kKey.size()]);
    return out;
}
QString sc_decode(const QString &cipher) {
    const QString prefix = QString::fromLatin1(PREFIX);
    if (!cipher.startsWith(prefix)) return cipher;
    const QByteArray raw = QByteArray::fromBase64(cipher.mid(prefix.size()).toLatin1());
    return QString::fromUtf8(xorCipher(raw));
}
QString sc_encode(const QString &plain) {
    if (plain.isEmpty()) return {};
    const QByteArray raw = xorCipher(plain.toUtf8());
    return QString::fromLatin1(PREFIX) + QString::fromLatin1(raw.toBase64());
}
}

int main(int argc, char *argv[]) {
    QCoreApplication a(argc, argv);

    // 直接硬编码参数（模拟手动输入）
    qDebug() << "=== Test 1: Hardcoded params, direct libmariadb ===";
    {
        // 复用之前验证通过的 test_fix.exe 思路
        system("e:\\yihuantech\\DataMG\\build\\test_fix.exe");
    }

    // 读 SQLite 里的保存数据
    qDebug() << "\n=== Test 2: Read from SQLite datamg.db ===";
    QString dbPath = "C:/Users/happy/AppData/Local/YihuanTech/DataMG/datamg.db";
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "cfg");
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        qCritical() << "Cannot open SQLite:" << db.lastError().text();
        return 1;
    }
    QSqlQuery q("SELECT id,name,type,host,port,username,password,database_name FROM connections", db);
    while (q.next()) {
        QString name = q.value(1).toString();
        QString host = q.value(3).toString();
        int port = q.value(4).toInt();
        QString user = q.value(5).toString();
        QString cipher = q.value(6).toString();
        QString pass = sc_decode(cipher);
        QString dbname = q.value(7).toString();
        qDebug() << "Connection:" << name;
        qDebug() << "  host=" << host << ":" << port;
        qDebug() << "  user=" << user;
        qDebug() << "  pass_stored_cipher=" << cipher;
        qDebug() << "  pass_decoded=" << pass << "(len=" << pass.size() << ")";
        qDebug() << "  db=" << dbname;
        qDebug() << "  EXPECTED PASS = cTisKNx4Ndf4bR25";
        qDebug() << "  MATCH =" << (pass == "cTisKNx4Ndf4bR25");
    }
    return 0;
}
