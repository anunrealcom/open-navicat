#include <mysql.h>
#include <stdio.h>
int main() {
    MYSQL *m = mysql_init(NULL);
    mysql_options(m, MYSQL_PLUGIN_DIR, "e:/yihuantech/DataMG/build/plugin");
    // 关键：关掉证书验证 + 开启 SSL（无证书）
    mysql_ssl_set(m, NULL, NULL, NULL, NULL, NULL);
    unsigned char verify = 0;  // MYSQL_OPT_SSL_VERIFY_SERVER_CERT = false
    mysql_options(m, MYSQL_OPT_SSL_VERIFY_SERVER_CERT, &verify);
    // 强制启用 SSL（不降级）— 其实不需要，默认就会尝试
    // 但如果服务端强制 SSL，我们需要用
    const char *host = "47.121.120.165";
    const char *user = "root";
    const char *pass = "cTisKNx4Ndf4bR25";
    const char *db   = "ainovel";
    unsigned int port = 13207;
    printf("Connecting...\n");
    MYSQL *ok = mysql_real_connect(m, host, user, pass, db, port, NULL, 0);
    if (ok) {
        printf("SUCCESS! Server version: %s\n", mysql_get_server_info(m));
        const char *cipher = mysql_get_ssl_cipher(m);
        printf("SSL cipher: %s\n", cipher ? cipher : "(none)");
        mysql_close(m);
        return 0;
    }
    printf("FAIL: %s (errno=%d)\n", mysql_error(m), mysql_errno(m));
    mysql_close(m);
    return 1;
}
