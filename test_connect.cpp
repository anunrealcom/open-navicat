// 极简 MySQL 连接测试，直接链接 libmariadb.dll
#include <mysql.h>
#include <stdio.h>
#include <string.h>

int main() {
    const char *host = "47.121.120.165";
    unsigned int port = 13207;
    const char *user = "root";
    const char *pass = "cTisKNx4Ndf4bR25";
    const char *db   = "ainovel";

    MYSQL *m = mysql_init(NULL);
    if (!m) { printf("mysql_init failed\n"); return 1; }

    // 插件目录指向 build/plugin
    mysql_options(m, MYSQL_PLUGIN_DIR, "e:/yihuantech/DataMG/build/plugin");

    // 策略 1: SSL + 自动认证
    printf("=== Strategy 1: SSL enabled, auto auth ===\n");
    mysql_ssl_set(m, NULL, NULL, NULL, NULL, NULL);
    const char *auth = NULL;
    mysql_options(m, MYSQL_DEFAULT_AUTH, auth);
    MYSQL *ok = mysql_real_connect(m, host, user, pass, db, port, NULL, 0);
    if (ok) {
        printf("SUCCESS! Server version: %s\n", mysql_get_server_info(m));
        mysql_close(m);
        return 0;
    }
    printf("FAIL: %s (errno=%d)\n", mysql_error(m), mysql_errno(m));
    mysql_close(m);

    // 策略 2: 无 SSL
    m = mysql_init(NULL);
    mysql_options(m, MYSQL_PLUGIN_DIR, "e:/yihuantech/DataMG/build/plugin");
    printf("\n=== Strategy 2: NO SSL, auto auth ===\n");
    ok = mysql_real_connect(m, host, user, pass, db, port, NULL, 0);
    if (ok) {
        printf("SUCCESS! Server version: %s\n", mysql_get_server_info(m));
        mysql_close(m);
        return 0;
    }
    printf("FAIL: %s (errno=%d)\n", mysql_error(m), mysql_errno(m));
    mysql_close(m);

    // 策略 3: SSL + 强制 mysql_native_password
    m = mysql_init(NULL);
    mysql_options(m, MYSQL_PLUGIN_DIR, "e:/yihuantech/DataMG/build/plugin");
    mysql_ssl_set(m, NULL, NULL, NULL, NULL, NULL);
    printf("\n=== Strategy 3: SSL + mysql_native_password ===\n");
    mysql_options(m, MYSQL_DEFAULT_AUTH, "mysql_native_password");
    ok = mysql_real_connect(m, host, user, pass, db, port, NULL, 0);
    if (ok) {
        printf("SUCCESS! Server version: %s\n", mysql_get_server_info(m));
        mysql_close(m);
        return 0;
    }
    printf("FAIL: %s (errno=%d)\n", mysql_error(m), mysql_errno(m));
    mysql_close(m);

    // 策略 4: 无 SSL + mysql_native_password
    m = mysql_init(NULL);
    mysql_options(m, MYSQL_PLUGIN_DIR, "e:/yihuantech/DataMG/build/plugin");
    printf("\n=== Strategy 4: NO SSL + mysql_native_password ===\n");
    mysql_options(m, MYSQL_DEFAULT_AUTH, "mysql_native_password");
    ok = mysql_real_connect(m, host, user, pass, db, port, NULL, 0);
    if (ok) {
        printf("SUCCESS! Server version: %s\n", mysql_get_server_info(m));
        mysql_close(m);
        return 0;
    }
    printf("FAIL: %s (errno=%d)\n", mysql_error(m), mysql_errno(m));
    mysql_close(m);

    printf("\n=== All strategies failed ===\n");
    return 2;
}
