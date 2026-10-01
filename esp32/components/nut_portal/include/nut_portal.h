#pragma once

#include "esp_err.h"

#include <stdbool.h>
#include <stdint.h>

#define NUT_WIFI_CONF_PATH "/usr/local/etc/nut/wifi.conf"

typedef struct {
    bool sta; /* false: softAP, true: station */
    char ssid[33];
    char passphrase[64];
    uint8_t channel; /* 1-13, softAP only */
} nut_wifi_config_t;

/* Defaults, then the file if it exists. Creates the file when it is missing. */
esp_err_t nut_wifi_conf_load(nut_wifi_config_t *cfg);

esp_err_t nut_wifi_conf_save(const nut_wifi_config_t *cfg);

/* Bring up the radio from cfg. Station failure falls back to the default softAP. */
esp_err_t nut_wifi_start(const nut_wifi_config_t *cfg);

/* True when a softAP is up and the captive portal can hijack DNS. */
bool nut_wifi_ap_is_up(void);

/* HTTP page on port 80. DNS and DHCP option 114 run only while the softAP is up. */
esp_err_t nut_portal_start(void);
