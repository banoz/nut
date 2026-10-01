#include "lwip/sockets.h"

#include "esp_log.h"

#include <string.h>
#include <sys/un.h>

#define TAG "nut-unix"
#define NUT_ESP_UNIX_PORT 3333

static int is_unix_family(int family)
{
    return family == AF_UNIX;
}

static int is_unix_addr(const struct sockaddr *name)
{
    if (!name) {
        return 0;
    }
    /* ESP sockaddr_un is { short sun_family; char sun_path[108] } with no sa_len.
     * lwIP struct sockaddr is { u8 sa_len; sa_family_t sa_family; ... }. */
    if (name->sa_family == AF_UNIX) {
        return 1;
    }
    return ((const struct sockaddr_un *)name)->sun_family == AF_UNIX;
}

int __real_lwip_socket(int domain, int type, int protocol);
int __real_lwip_bind(int s, const struct sockaddr *name, socklen_t namelen);
int __real_lwip_connect(int s, const struct sockaddr *name, socklen_t namelen);
int __real_lwip_accept(int s, struct sockaddr *addr, socklen_t *addrlen);
int __real_lwip_listen(int s, int backlog);

int __wrap_lwip_socket(int domain, int type, int protocol)
{
    if (is_unix_family(domain)) {
        ESP_LOGD(TAG, "AF_UNIX socket -> AF_INET loopback");
        return __real_lwip_socket(AF_INET, type, protocol);
    }
    return __real_lwip_socket(domain, type, protocol);
}

int __wrap_lwip_bind(int s, const struct sockaddr *name, socklen_t namelen)
{
    if (is_unix_addr(name)) {
        struct sockaddr_in server_addr;
        int yes = 1;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_len = sizeof(server_addr);
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        server_addr.sin_port = htons(NUT_ESP_UNIX_PORT);
        lwip_setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
        ESP_LOGI(TAG, "AF_UNIX bind %s -> 127.0.0.1:%d",
                 ((const struct sockaddr_un *)name)->sun_path, NUT_ESP_UNIX_PORT);
        return __real_lwip_bind(s, (struct sockaddr *)&server_addr, sizeof(server_addr));
    }
    return __real_lwip_bind(s, name, namelen);
}

int __wrap_lwip_connect(int s, const struct sockaddr *name, socklen_t namelen)
{
    if (is_unix_addr(name)) {
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_len = sizeof(server_addr);
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        server_addr.sin_port = htons(NUT_ESP_UNIX_PORT);
        return __real_lwip_connect(s, (struct sockaddr *)&server_addr, sizeof(server_addr));
    }
    return __real_lwip_connect(s, name, namelen);
}

int __wrap_lwip_accept(int s, struct sockaddr *addr, socklen_t *addrlen)
{
    return __real_lwip_accept(s, addr, addrlen);
}

int __wrap_lwip_listen(int s, int backlog)
{
    return __real_lwip_listen(s, backlog);
}
