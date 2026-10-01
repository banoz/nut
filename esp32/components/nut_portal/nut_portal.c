#include "nut_portal.h"
#include "dns_server.h"

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lwip/inet.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "nut_portal";

static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static void url_decode(char *s)
{
    char *in = s;
    char *out = s;
    while (*in) {
        if (*in == '+') {
            *out++ = ' ';
            in++;
        } else if (*in == '%' && in[1] && in[2]) {
            int hi = hex_nibble(in[1]);
            int lo = hex_nibble(in[2]);
            if (hi >= 0 && lo >= 0) {
                *out++ = (char)((hi << 4) | lo);
                in += 3;
            } else {
                *out++ = *in++;
            }
        } else {
            *out++ = *in++;
        }
    }
    *out = '\0';
}

static bool form_get(const char *body, const char *key, char *out, size_t outlen)
{
    size_t keylen = strlen(key);
    const char *p = body;
    while (p && *p) {
        bool boundary = (p == body) || (p > body && p[-1] == '&');
        if (boundary && strncmp(p, key, keylen) == 0 && p[keylen] == '=') {
            p += keylen + 1;
            const char *end = strchr(p, '&');
            size_t n = end ? (size_t)(end - p) : strlen(p);
            if (n >= outlen) {
                n = outlen - 1;
            }
            memcpy(out, p, n);
            out[n] = '\0';
            url_decode(out);
            return true;
        }
        p = strchr(p, '&');
        if (p) {
            p++;
        }
    }
    out[0] = '\0';
    return false;
}

static void html_escape(const char *in, char *out, size_t outlen)
{
    size_t used = 0;
    while (*in && used + 6 < outlen) {
        const char *rep = NULL;
        if (*in == '&') {
            rep = "&amp;";
        } else if (*in == '<') {
            rep = "&lt;";
        } else if (*in == '>') {
            rep = "&gt;";
        } else if (*in == '"') {
            rep = "&quot;";
        }
        if (rep) {
            size_t n = strlen(rep);
            memcpy(out + used, rep, n);
            used += n;
            in++;
        } else {
            out[used++] = *in++;
        }
    }
    out[used] = '\0';
}

static esp_err_t send_form(httpd_req_t *req, const nut_wifi_config_t *cfg, const char *error)
{
    char ssid[160];
    char pass[400];
    html_escape(cfg->ssid, ssid, sizeof(ssid));
    html_escape(cfg->passphrase, pass, sizeof(pass));

    char page[2048];
    int n = snprintf(page, sizeof(page),
        "<!DOCTYPE html><html><head>"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>NUT Wi-Fi</title></head><body>"
        "<h1>NUT Wi-Fi</h1>"
        "%s"
        "<form method=\"POST\" action=\"/save\">"
        "<p><label>Mode <select name=\"mode\">"
        "<option value=\"softap\"%s>softap</option>"
        "<option value=\"sta\"%s>sta</option>"
        "</select></label></p>"
        "<p><label>SSID <input name=\"ssid\" maxlength=\"32\" value=\"%s\" required></label></p>"
        "<p><label>Passphrase <input name=\"passphrase\" maxlength=\"63\" value=\"%s\"></label></p>"
        "<p><label>Channel <input name=\"channel\" value=\"%u\"></label></p>"
        "<p><button type=\"submit\">Save and reboot</button></p>"
        "</form>"
        "<p>Stored in /usr/local/etc/nut/wifi.conf. "
        "Channel applies to softAP. An empty passphrase is an open network. "
        "If station join fails, the softAP nut / espdonut comes back.</p>"
        "</body></html>",
        (error && error[0]) ? error : "",
        cfg->sta ? "" : " selected",
        cfg->sta ? " selected" : "",
        ssid,
        pass,
        (unsigned)cfg->channel);
    if (n < 0 || (size_t)n >= sizeof(page)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "page too long");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, page, n);
}

static esp_err_t root_get(httpd_req_t *req)
{
    nut_wifi_config_t cfg;
    nut_wifi_conf_load(&cfg);
    return send_form(req, &cfg, "");
}

static const char *validate(const nut_wifi_config_t *cfg)
{
    size_t ssid_len = strlen(cfg->ssid);
    size_t pass_len = strlen(cfg->passphrase);
    if (ssid_len == 0 || ssid_len > 32) {
        return "<p>SSID must be 1 to 32 characters.</p>";
    }
    if (pass_len > 0 && (pass_len < 8 || pass_len > 63)) {
        return "<p>Passphrase must be empty, or 8 to 63 characters.</p>";
    }
    if (cfg->channel < 1 || cfg->channel > 13) {
        return "<p>Channel must be 1 to 13.</p>";
    }
    return NULL;
}

static esp_err_t save_post(httpd_req_t *req)
{
    if (req->content_len <= 0 || req->content_len > 512) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad form");
        return ESP_FAIL;
    }

    char body[513];
    int got = 0;
    while (got < req->content_len) {
        int n = httpd_req_recv(req, body + got, req->content_len - got);
        if (n == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        }
        if (n <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "incomplete form");
            return ESP_FAIL;
        }
        got += n;
    }
    body[got] = '\0';

    nut_wifi_config_t cfg;
    nut_wifi_conf_load(&cfg);

    char mode[16];
    char channel[8];
    form_get(body, "mode", mode, sizeof(mode));
    form_get(body, "ssid", cfg.ssid, sizeof(cfg.ssid));
    form_get(body, "passphrase", cfg.passphrase, sizeof(cfg.passphrase));
    form_get(body, "channel", channel, sizeof(channel));
    cfg.sta = (strcmp(mode, "sta") == 0);
    int ch = atoi(channel);
    cfg.channel = (ch >= 1 && ch <= 13) ? (uint8_t)ch : 0;

    const char *error = validate(&cfg);
    if (error) {
        if (cfg.channel == 0) {
            cfg.channel = 9;
        }
        return send_form(req, &cfg, error);
    }

    if (nut_wifi_conf_save(&cfg) != ESP_OK) {
        return send_form(req, &cfg, "<p>Could not write wifi.conf.</p>");
    }

    const char *done = "<!DOCTYPE html><html><body><p>Saved. Rebooting.</p></body></html>";
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, done, HTTPD_RESP_USE_STRLEN);
    vTaskDelay(pdMS_TO_TICKS(400));
    esp_restart();
    return ESP_OK;
}

static esp_err_t redirect_to_root(httpd_req_t *req, httpd_err_code_t err)
{
    (void)err;
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static void dhcp_captive_portal(void)
{
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (!netif) {
        ESP_LOGW(TAG, "no softAP netif");
        return;
    }

    esp_netif_ip_info_t ip;
    esp_netif_get_ip_info(netif, &ip);

    static char uri[48];
    char ip_addr[16];
    inet_ntoa_r(ip.ip.addr, ip_addr, sizeof(ip_addr));
    snprintf(uri, sizeof(uri), "http://%s/", ip_addr);

    esp_netif_dhcps_stop(netif);
    esp_err_t err = esp_netif_dhcps_option(netif, ESP_NETIF_OP_SET,
                                           ESP_NETIF_CAPTIVEPORTAL_URI,
                                           uri, strlen(uri));
    esp_netif_dhcps_start(netif);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "DHCP option 114 failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "captive portal %s", uri);
    }

    if (nut_dns_server_start(ip.ip.addr) != ESP_OK) {
        ESP_LOGE(TAG, "DNS server did not start");
    }
}

esp_err_t nut_portal_start(void)
{
    esp_log_level_set("httpd_uri", ESP_LOG_ERROR);
    esp_log_level_set("httpd_txrx", ESP_LOG_ERROR);
    esp_log_level_set("httpd_parse", ESP_LOG_ERROR);

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 4;
    config.lru_purge_enable = true;
    config.stack_size = 8192;

    httpd_handle_t server = NULL;
    esp_err_t err = httpd_start(&server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(err));
        return err;
    }

    const httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_get,
    };
    const httpd_uri_t save = {
        .uri = "/save",
        .method = HTTP_POST,
        .handler = save_post,
    };
    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &save);
    httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, redirect_to_root);

    ESP_LOGI(TAG, "config page on port %d", config.server_port);
    if (nut_wifi_ap_is_up()) {
        dhcp_captive_portal();
    }
    return ESP_OK;
}
