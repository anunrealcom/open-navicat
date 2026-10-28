#include "MySqlClient.h"

#include <mysql.h>

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QLibrary>
#include <QMetaType>
#include <QMutex>
#include <QSqlError>

namespace {

struct MariaApi
{
    // 初始化/销毁
    int (*server_init)(int, char **, char **) = nullptr;
    void (*server_end)() = nullptr;
    // 连接
    MYSQL *(*init)(MYSQL *) = nullptr;
    int (*options)(MYSQL *, enum mysql_option, const void *) = nullptr;
    MYSQL *(*real_connect)(MYSQL *, const char *, const char *, const char *, const char *,
                          unsigned int, const char *, unsigned long) = nullptr;
    void (*close)(MYSQL *) = nullptr;
    int (*ssl_set)(MYSQL *, const char *, const char *, const char *, const char *,
                   const char *) = nullptr;
    const char *(*error)(MYSQL *) = nullptr;
    unsigned int (*errno_)(MYSQL *) = nullptr;
    const char *(*sqlstate)(MYSQL *) = nullptr;
    // 查询
    int (*real_query)(MYSQL *, const char *, unsigned long) = nullptr;
    MYSQL_RES *(*store_result)(MYSQL *) = nullptr;
    unsigned int (*field_count)(MYSQL *) = nullptr;
    int (*next_result)(MYSQL *) = nullptr;
    unsigned int (*num_fields)(MYSQL_RES *) = nullptr;
    MYSQL_FIELD *(*fetch_fields)(MYSQL_RES *) = nullptr;
    MYSQL_ROW (*fetch_row)(MYSQL_RES *) = nullptr;
    unsigned long *(*fetch_lengths)(MYSQL_RES *) = nullptr;
    void (*free_result)(MYSQL_RES *) = nullptr;
    unsigned long long (*affected_rows)(MYSQL *) = nullptr;
    unsigned long long (*insert_id)(MYSQL *) = nullptr;
    unsigned long (*real_escape_string)(MYSQL *, char *, const char *, unsigned long) = nullptr;

    QLibrary *library = nullptr;
    bool initialized = false;
    QString loadError;
};

Q_GLOBAL_STATIC(QMutex, g_apiMutex)

MariaApi *api()
{
    static MariaApi s_api;
    return &s_api;
}

bool ensureLibrary()
{
    QMutexLocker locker(g_apiMutex());
    MariaApi *a = api();
    if (a->initialized)
        return a->library != nullptr;
    a->initialized = true;

    auto tryLoad = [&](const QString &fileName) -> bool {
        auto *lib = new QLibrary(fileName);
        lib->setLoadHints(QLibrary::PreventUnloadHint);
        if (!lib->load()) {
            a->loadError = lib->errorString();
            delete lib;
            return false;
        }
        auto resolve = [lib](const char *name) { return lib->resolve(name); };
        a->server_init = reinterpret_cast<int (*)(int, char **, char **)>(resolve("mysql_server_init"));
        a->server_end = reinterpret_cast<void (*)()>(resolve("mysql_server_end"));
        a->init = reinterpret_cast<MYSQL *(*)(MYSQL *)>(resolve("mysql_init"));
        a->options = reinterpret_cast<int (*)(MYSQL *, enum mysql_option, const void *)>(
            resolve("mysql_options"));
        a->real_connect = reinterpret_cast<MYSQL *(*)(MYSQL *, const char *, const char *,
                                                      const char *, const char *, unsigned int,
                                                      const char *, unsigned long)>(
            resolve("mysql_real_connect"));
        a->close = reinterpret_cast<void (*)(MYSQL *)>(resolve("mysql_close"));
        a->ssl_set = reinterpret_cast<int (*)(MYSQL *, const char *, const char *,
                                              const char *, const char *, const char *)>(
            resolve("mysql_ssl_set"));
        a->error = reinterpret_cast<const char *(*)(MYSQL *)>(resolve("mysql_error"));
        a->errno_ = reinterpret_cast<unsigned int (*)(MYSQL *)>(resolve("mysql_errno"));
        a->sqlstate = reinterpret_cast<const char *(*)(MYSQL *)>(resolve("mysql_sqlstate"));
        a->real_query = reinterpret_cast<int (*)(MYSQL *, const char *, unsigned long)>(
            resolve("mysql_real_query"));
        a->store_result = reinterpret_cast<MYSQL_RES *(*)(MYSQL *)>(resolve("mysql_store_result"));
        a->field_count = reinterpret_cast<unsigned int (*)(MYSQL *)>(resolve("mysql_field_count"));
        a->next_result = reinterpret_cast<int (*)(MYSQL *)>(resolve("mysql_next_result"));
        a->num_fields = reinterpret_cast<unsigned int (*)(MYSQL_RES *)>(resolve("mysql_num_fields"));
        a->fetch_fields = reinterpret_cast<MYSQL_FIELD *(*)(MYSQL_RES *)>(
            resolve("mysql_fetch_fields"));
        a->fetch_row = reinterpret_cast<MYSQL_ROW (*)(MYSQL_RES *)>(resolve("mysql_fetch_row"));
        a->fetch_lengths = reinterpret_cast<unsigned long *(*)(MYSQL_RES *)>(
            resolve("mysql_fetch_lengths"));
        a->free_result = reinterpret_cast<void (*)(MYSQL_RES *)>(resolve("mysql_free_result"));
        a->affected_rows = reinterpret_cast<unsigned long long (*)(MYSQL *)>(
            resolve("mysql_affected_rows"));
        a->insert_id = reinterpret_cast<unsigned long long (*)(MYSQL *)>(resolve("mysql_insert_id"));
        a->real_escape_string =
            reinterpret_cast<unsigned long (*)(MYSQL *, char *, const char *, unsigned long)>(
                resolve("mysql_real_escape_string"));
        a->library = lib;
        return a->init && a->real_connect && a->real_query && a->store_result &&
               a->fetch_row && a->real_escape_string;
    };

    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir::toNativeSeparators(appDir + QStringLiteral("/libmariadb.dll")),
        QDir::toNativeSeparators(appDir + QStringLiteral("/libmysql.dll")),
        QStringLiteral("libmariadb.dll"),
        QStringLiteral("libmysql.dll"),
    };
    for (const QString &c : candidates) {
        if (tryLoad(c)) {
            if (a->server_init)
                a->server_init(0, nullptr, nullptr);
            return true;
        }
    }
    return false;
}

/// 将带 ? 占位符的 SQL 替换为客户端转义后的字面量（跳过引号/注释中的 ?）
QString expandPlaceholders(const QString &sql, const QVariantList &params)
{
    QString out;
    out.reserve(sql.size() + params.size() * 8);
    int paramIndex = 0;
    const QChar slash = QLatin1Char('/');
    for (int i = 0; i < sql.size(); ++i) {
        const QChar ch = sql[i];
        // 字符串：'...'（'' 转义）、"..."、`...`
        if (ch == QLatin1Char('\'') || ch == QLatin1Char('"') || ch == QLatin1Char('`')) {
            const QChar quote = ch;
            out.append(ch);
            while (++i < sql.size()) {
                const QChar c = sql[i];
                out.append(c);
                if (c == quote) {
                    if (quote == QLatin1Char('\'') && i + 1 < sql.size() &&
                        sql[i + 1] == QLatin1Char('\'')) {
                        out.append(sql[++i]);
                        continue;
                    }
                    break;
                }
                if (c == QLatin1Char('\\') && i + 1 < sql.size())
                    out.append(sql[++i]);
            }
            continue;
        }
        // 行注释 -- 与 #，块注释 / * ... * /
        if (ch == QLatin1Char('-') && i + 1 < sql.size() && sql[i + 1] == QLatin1Char('-')) {
            while (i < sql.size() && sql[i] != QLatin1Char('\n'))
                out.append(sql[i++]);
            if (i < sql.size())
                out.append(sql[i]);
            continue;
        }
        if (ch == QLatin1Char('#')) {
            while (i < sql.size() && sql[i] != QLatin1Char('\n'))
                out.append(sql[i++]);
            if (i < sql.size())
                out.append(sql[i]);
            continue;
        }
        if (ch == slash && i + 1 < sql.size() && sql[i + 1] == QLatin1Char('*')) {
            out.append(sql[i++]);
            out.append(sql[i]);
            while (i + 1 < sql.size() && !(sql[i] == QLatin1Char('*') && sql[i + 1] == slash))
                out.append(sql[i++]);
            if (i + 1 < sql.size())
                out.append(sql[i++]).append(sql[++i]);
            continue;
        }
        if (ch == QLatin1Char('?') && paramIndex < params.size()) {
            // 占位符：由调用方负责转义，这里先放哨兵，稍后替换
            out.append(QStringLiteral("\x01%1\x01").arg(paramIndex));
            ++paramIndex;
            continue;
        }
        out.append(ch);
    }
    return out;
}

} // namespace

MySqlClient::MySqlClient() = default;

MySqlClient::~MySqlClient()
{
    close();
}

bool MySqlClient::open(const ConnectionProfile &profile, QString *err)
{
    close();
    m_lastErrno = 0;
    m_lastSqlstate.clear();
    if (!ensureLibrary()) {
        m_lastError = QObject::tr("无法加载 MySQL 客户端库 libmariadb.dll（%1）。"
                                  "请确认程序目录下存在该文件。")
                          .arg(api()->loadError);
        if (err) *err = formatLastError();
        return false;
    }
    MariaApi *a = api();

    const QByteArray host = profile.host.toUtf8();
    const QByteArray user = profile.username.toUtf8();
    const QByteArray pass = profile.password.toUtf8();
    const QByteArray db = profile.database.toUtf8();
    const unsigned int port =
        static_cast<unsigned int>(profile.port > 0 ? profile.port : 3306);

    // 连接策略：依次尝试，直到成功。覆盖 SSL 开关与认证插件组合，
    // 以兼容 MySQL 5.7 / 8.0、MariaDB、阿里云 RDS 等不同服务端配置。
    struct Strategy {
        bool ssl;           // 是否显式启用 SSL
        const char *auth;   // nullptr = 让连接器自动协商认证插件
    };
    const Strategy strategies[] = {
        { true,  nullptr },               // SSL + 自动认证
        { false, nullptr },               // 无 SSL + 自动认证
        { true,  "mysql_native_password" },   // SSL + 强制旧认证
        { false, "mysql_native_password" },   // 无 SSL + 强制旧认证
    };

    for (const auto &s : strategies) {
        MYSQL *m = a->init(nullptr);
        if (!m) {
            m_lastError = QObject::tr("mysql_init 失败");
            if (err) *err = formatLastError();
            return false;
        }

        const int timeout = 10;
        a->options(m, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
        const bool reconnect = true;
        a->options(m, MYSQL_OPT_RECONNECT, &reconnect);
        const QByteArray charset = QByteArrayLiteral("utf8mb4");
        a->options(m, MYSQL_SET_CHARSET_NAME, charset.constData());
        const QByteArray pluginDir = QDir::toNativeSeparators(
            QCoreApplication::applicationDirPath() + QStringLiteral("/plugin")).toUtf8();
        a->options(m, MYSQL_PLUGIN_DIR, pluginDir.constData());

        // MariaDB Connector/C 3.4+ 默认自动尝试 SSL（MDEV-31857），
        // 并默认开启证书验证。必须关闭验证以兼容自签/内部 CA 的 MySQL。
        unsigned char verifyCert = 0;
        a->options(m, MYSQL_OPT_SSL_VERIFY_SERVER_CERT, &verifyCert);

        if (s.ssl && a->ssl_set)
            a->ssl_set(m, nullptr, nullptr, nullptr, nullptr, nullptr);

        if (s.auth)
            a->options(m, MYSQL_DEFAULT_AUTH, s.auth);

        MYSQL *ok = a->real_connect(m,
                                    host.isEmpty() ? nullptr : host.constData(),
                                    user.constData(),
                                    pass.constData(),
                                    db.isEmpty() ? nullptr : db.constData(),
                                    port, nullptr,
                                    CLIENT_MULTI_STATEMENTS);
        if (ok) {
            m_handle = m;
            m_profile = profile;
            m_currentDb = profile.database;
            m_lastErrno = 0;
            m_lastSqlstate.clear();
            if (m_currentDb.isEmpty()) {
                DbResult r;
                QString e;
                if (execute(QStringLiteral("SELECT DATABASE()"), {}, &r, &e) &&
                    r.rowCount() > 0)
                    m_currentDb = r.rows.first().value(0);
            }
            return true;
        }

        // 捕获本策略的错误信息后关闭句柄
        m_lastError = QString::fromUtf8(a->error(m));
        m_lastErrno = a->errno_ ? static_cast<int>(a->errno_(m)) : 0;
        m_lastSqlstate = a->sqlstate ? QString::fromUtf8(a->sqlstate(m)) : QString();
        a->close(m);
    }

    if (err) *err = formatLastError();
    return false;
}

void MySqlClient::close()
{
    if (m_handle && api()->library) {
        api()->close(static_cast<MYSQL *>(m_handle));
    }
    m_handle = nullptr;
    m_currentDb.clear();
}

bool MySqlClient::isOpen() const
{
    return m_handle != nullptr;
}

bool MySqlClient::execute(const QString &sqlIn, const QVariantList &params,
                          DbResult *result, QString *err)
{
    if (!m_handle) {
        m_lastError = QObject::tr("连接未打开");
        m_lastErrno = 0;
        m_lastSqlstate.clear();
        if (err) *err = formatLastError();
        return false;
    }
    MariaApi *a = api();
    MYSQL *m = static_cast<MYSQL *>(m_handle);

    // 客户端展开占位符
    QString sql;
    if (!params.isEmpty()) {
        sql = expandPlaceholders(sqlIn, params);
        for (int i = 0; i < params.size(); ++i)
            sql.replace(QStringLiteral("\x01%1\x01").arg(i), quoteLiteral(params[i]));
    } else {
        sql = sqlIn;
    }

    QElapsedTimer timer;
    timer.start();
    const QByteArray utf8 = sql.toUtf8();
    if (a->real_query(m, utf8.constData(), static_cast<unsigned long>(utf8.size())) != 0) {
        m_lastError = QString::fromUtf8(a->error(m));
        m_lastErrno = a->errno_ ? static_cast<int>(a->errno_(m)) : 0;
        m_lastSqlstate = a->sqlstate ? QString::fromUtf8(a->sqlstate(m)) : QString();
        if (err) *err = formatLastError();
        return false;
    }

    MYSQL_RES *res = a->store_result(m);
    if (result)
        result->columns.clear(), result->rows.clear(), result->nullFlags.clear();

    if (res) {
        const unsigned int n = a->num_fields(res);
        MYSQL_FIELD *fields = a->fetch_fields(res);
        if (result) {
            for (unsigned int i = 0; i < n; ++i)
                result->columns << QString::fromUtf8(fields[i].name);
        }
        while (MYSQL_ROW row = a->fetch_row(res)) {
            unsigned long *lens = a->fetch_lengths(res);
            QStringList vals;
            QBitArray nulls(static_cast<int>(n));
            for (unsigned int i = 0; i < n; ++i) {
                if (row[i] == nullptr) {
                    nulls.setBit(static_cast<int>(i));
                    vals << QString();
                } else {
                    vals << QString::fromUtf8(row[i], static_cast<int>(lens[i]));
                }
            }
            if (result) {
                result->rows << vals;
                result->nullFlags << nulls;
            }
        }
        a->free_result(res);
        if (result) {
            result->hasResult = true;
            result->affectedRows = -1;
            result->elapsedMs = timer.elapsed();
        }
        // 消费多语句剩余的 results（防止 MYSQL 连接状态错乱）
        while (a->next_result(m) == 0) {
            MYSQL_RES *r2 = a->store_result(m);
            if (r2) a->free_result(r2);
        }
        return true;
    }

    if (a->field_count(m) == 0) {
        if (result) {
            result->hasResult = false;
            result->affectedRows = static_cast<qint64>(a->affected_rows(m));
            result->insertId = static_cast<qint64>(a->insert_id(m));
            result->elapsedMs = timer.elapsed();
        }
        // 消费多语句剩余的 results
        while (a->next_result(m) == 0) {
            MYSQL_RES *r2 = a->store_result(m);
            if (r2) a->free_result(r2);
        }
        return true;
    }

    m_lastError = QString::fromUtf8(a->error(m));
    m_lastErrno = a->errno_ ? static_cast<int>(a->errno_(m)) : 0;
    m_lastSqlstate = a->sqlstate ? QString::fromUtf8(a->sqlstate(m)) : QString();
    if (err) *err = formatLastError();
    return false;
}

QString MySqlClient::quoteLiteral(const QVariant &v) const
{
    if (!v.isValid() || v.isNull())
        return QStringLiteral("NULL");

    const int id = v.userType();
    if (id == QMetaType::Bool)
        return v.toBool() ? QStringLiteral("1") : QStringLiteral("0");
    if (id == QMetaType::Int || id == QMetaType::UInt || id == QMetaType::LongLong ||
        id == QMetaType::ULongLong || id == QMetaType::Double || id == QMetaType::Float) {
        return v.toString();
    }

    const QByteArray raw = (id == QMetaType::QByteArray) ? v.toByteArray() : v.toString().toUtf8();
    if (m_handle) {
        QByteArray escaped(2 * raw.size() + 1, '\0');
        const unsigned long outLen =
            api()->real_escape_string(static_cast<MYSQL *>(m_handle), escaped.data(),
                                      raw.constData(), static_cast<unsigned long>(raw.size()));
        escaped.resize(static_cast<int>(outLen));
        return QLatin1Char('\'') + QString::fromUtf8(escaped) + QLatin1Char('\'');
    }
    // 未连接时的保守转义（常规路径下不会走到）
    QByteArray t = raw;
    t.replace("\\", "\\\\").replace("'", "\\'").replace("\"", "\\\"")
     .replace("\n", "\\n").replace("\r", "\\r");
    return QLatin1Char('\'') + QString::fromUtf8(t) + QLatin1Char('\'');
}

QString MySqlClient::quoteIdent(const QString &ident) const
{
    QStringList parts = ident.split(QLatin1Char('.'));
    for (QString &p : parts) {
        if (p.startsWith(QLatin1Char('`')))
            continue;
        p.replace(QLatin1Char('`'), QStringLiteral("``"));
        p = QLatin1Char('`') + p + QLatin1Char('`');
    }
    return parts.join(QLatin1Char('.'));
}

bool MySqlClient::setCurrentDatabase(const QString &db, QString *err)
{
    if (!db.isEmpty()) {
        if (!execute(QStringLiteral("USE %1").arg(quoteIdent(db)), {}, nullptr, err))
            return false;
    }
    m_currentDb = db;
    return true;
}
