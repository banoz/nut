#include "nut_portal.h"

#include "esp_log.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *TAG = "nut_wifi_conf";

static void set_defaults(nut_wifi_config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->sta = false;
    memcpy(cfg->ssid, "nut", 4);
    memcpy(cfg->passphrase, "espdonut", 9);
    cfg->channel = 9;
}

static void trim(char *s)
{
    char *start = s;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }
    if (start != s) {
        memmove(s, start, strlen(start) + 1);
    }
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) {
        s[--n] = '\0';
    }
}

static void copy_field(char *dst, size_t dstlen, const char *src)
{
    size_t n = strlen(src);
    if (n >= dstlen) {
        n = dstlen - 1;
    }
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static void apply_key(nut_wifi_config_t *cfg, const char *key, const char *val)
{
    if (strcasecmp(key, "MODE") == 0) {
        cfg->sta = (strcasecmp(val, "sta") == 0);
    } else if (strcasecmp(key, "SSID") == 0) {
        copy_field(cfg->ssid, sizeof(cfg->ssid), val);
    } else if (strcasecmp(key, "PASSPHRASE") == 0 || strcasecmp(key, "PASSWORD") == 0) {
        copy_field(cfg->passphrase, sizeof(cfg->passphrase), val);
    } else if (strcasecmp(key, "CHANNEL") == 0) {
        int channel = atoi(val);
        if (channel >= 1 && channel <= 13) {
            cfg->channel = (uint8_t)channel;
        }
    }
}

static void parse_line(nut_wifi_config_t *cfg, char *line)
{
    char *hash = strchr(line, '#');
    if (hash) {
        *hash = '\0';
    }
    trim(line);
    if (line[0] == '\0') {
        return;
    }

    char *sep = strchr(line, '=');
    if (!sep) {
        sep = strchr(line, ' ');
    }
    if (!sep) {
        return;
    }
    *sep = '\0';
    trim(line);
    char *val = sep + 1;
    trim(val);
    apply_key(cfg, line, val);
}

esp_err_t nut_wifi_conf_save(const nut_wifi_config_t *cfg)
{
    FILE *f = fopen(NUT_WIFI_CONF_PATH, "w");
    if (!f) {
        ESP_LOGE(TAG, "cannot write %s", NUT_WIFI_CONF_PATH);
        return ESP_FAIL;
    }
    fprintf(f, "MODE=%s\nSSID=%s\nPASSPHRASE=%s\nCHANNEL=%u\n",
            cfg->sta ? "sta" : "softap",
            cfg->ssid,
            cfg->passphrase,
            (unsigned)cfg->channel);
    if (fclose(f) != 0) {
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "wrote %s mode=%s ssid=%s channel=%u",
             NUT_WIFI_CONF_PATH,
             cfg->sta ? "sta" : "softap",
             cfg->ssid,
             (unsigned)cfg->channel);
    return ESP_OK;
}

esp_err_t nut_wifi_conf_load(nut_wifi_config_t *cfg)
{
    set_defaults(cfg);

    if (access(NUT_WIFI_CONF_PATH, F_OK) != 0) {
        ESP_LOGW(TAG, "%s missing, writing defaults", NUT_WIFI_CONF_PATH);
        return nut_wifi_conf_save(cfg);
    }

    FILE *f = fopen(NUT_WIFI_CONF_PATH, "r");
    if (!f) {
        ESP_LOGW(TAG, "cannot read %s, using defaults", NUT_WIFI_CONF_PATH);
        return ESP_OK;
    }

    char line[128];
    while (fgets(line, sizeof(line), f)) {
        parse_line(cfg, line);
    }
    fclose(f);

    if (cfg->ssid[0] == '\0') {
        copy_field(cfg->ssid, sizeof(cfg->ssid), "nut");
    }
    if (cfg->channel < 1 || cfg->channel > 13) {
        cfg->channel = 9;
    }

    ESP_LOGI(TAG, "loaded %s mode=%s ssid=%s channel=%u",
             NUT_WIFI_CONF_PATH,
             cfg->sta ? "sta" : "softap",
             cfg->ssid,
             (unsigned)cfg->channel);
    return ESP_OK;
}
