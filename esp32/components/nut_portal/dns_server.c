#include "dns_server.h"

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lwip/sockets.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

static const char *TAG = "nut_dns";

#define DNS_PORT 53

static uint32_t s_ip_nbo;
static volatile bool s_run;

/* Skip a DNS name. Returns the first byte after the name, or NULL. */
static const uint8_t *skip_name(const uint8_t *p, const uint8_t *end)
{
    while (p < end) {
        uint8_t len = *p;
        if (len == 0) {
            return p + 1;
        }
        if ((len & 0xC0) == 0xC0) {
            return (p + 2 <= end) ? p + 2 : NULL;
        }
        if (p + 1 + len > end) {
            return NULL;
        }
        p += 1 + len;
    }
    return NULL;
}

/* Build a reply that answers the first A query with s_ip_nbo. Returns length, or 0. */
static int build_reply(const uint8_t *req, int req_len, uint8_t *reply, int reply_max)
{
    if (req_len < 12 || req_len + 16 > reply_max) {
        return 0;
    }

    memcpy(reply, req, req_len);
    reply[2] |= 0x80; /* QR */

    const uint8_t *qname = skip_name(req + 12, req + req_len);
    if (!qname || qname + 4 > req + req_len) {
        return 0;
    }
    uint16_t qtype = (uint16_t)((qname[0] << 8) | qname[1]);
    if (qtype != 1) { /* A */
        return 0;
    }

    reply[6] = 0;
    reply[7] = 1; /* ANCOUNT */

    uint8_t *ans = reply + req_len;
    ans[0] = 0xC0;
    ans[1] = 0x0C; /* pointer to the question name */
    ans[2] = 0x00;
    ans[3] = 0x01; /* A */
    ans[4] = 0x00;
    ans[5] = 0x01; /* IN */
    ans[6] = 0x00;
    ans[7] = 0x00;
    ans[8] = 0x00;
    ans[9] = 60; /* TTL */
    ans[10] = 0x00;
    ans[11] = 0x04;
    memcpy(ans + 12, &s_ip_nbo, 4);
    return req_len + 16;
}

static void dns_task(void *arg)
{
    (void)arg;

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "socket failed errno=%d", errno);
        vTaskDelete(NULL);
        return;
    }

    int yes = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(DNS_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        ESP_LOGE(TAG, "bind port %d failed errno=%d", DNS_PORT, errno);
        close(sock);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "listening on UDP %d", DNS_PORT);

    uint8_t rx[512];
    uint8_t tx[512];
    while (s_run) {
        struct sockaddr_in from;
        socklen_t fromlen = sizeof(from);
        int len = recvfrom(sock, rx, sizeof(rx), 0, (struct sockaddr *)&from, &fromlen);
        if (len < 0) {
            if (!s_run) {
                break;
            }
            continue;
        }
        int reply_len = build_reply(rx, len, tx, sizeof(tx));
        if (reply_len > 0) {
            sendto(sock, tx, reply_len, 0, (struct sockaddr *)&from, fromlen);
        }
    }

    close(sock);
    vTaskDelete(NULL);
}

esp_err_t nut_dns_server_start(uint32_t ip_nbo)
{
    s_ip_nbo = ip_nbo;
    s_run = true;
    if (xTaskCreate(dns_task, "nut_dns", 4096, NULL, 4, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
