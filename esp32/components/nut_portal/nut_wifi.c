#include "nut_portal.h"

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include <string.h>

static const char *TAG = "nut_wifi";

#define STA_CONNECTED_BIT BIT0
#define STA_FAIL_BIT      BIT1
#define STA_MAX_RETRY     5

static EventGroupHandle_t s_sta_events;
static int s_sta_retries;
static bool s_sta_give_up;
static bool s_ap_up;

static void fill_ap(wifi_config_t *dst, const char *ssid, const char *pass, uint8_t channel)
{
    memset(dst, 0, sizeof(*dst));
    size_t ssid_len = strnlen(ssid, sizeof(dst->ap.ssid));
    memcpy(dst->ap.ssid, ssid, ssid_len);
    dst->ap.ssid_len = ssid_len;
    dst->ap.channel = channel;
    size_t pass_len = strnlen(pass, sizeof(dst->ap.password) - 1);
    memcpy(dst->ap.password, pass, pass_len);
    dst->ap.max_connection = 4;
    dst->ap.authmode = (pass_len >= 8) ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    dst->ap.pmf_cfg.capable = true;
    dst->ap.pmf_cfg.required = false;
}

static void fill_sta(wifi_config_t *dst, const char *ssid, const char *pass)
{
    memset(dst, 0, sizeof(*dst));
    size_t ssid_len = strnlen(ssid, sizeof(dst->sta.ssid));
    memcpy(dst->sta.ssid, ssid, ssid_len);
    size_t pass_len = strnlen(pass, sizeof(dst->sta.password) - 1);
    memcpy(dst->sta.password, pass, pass_len);
    dst->sta.threshold.authmode = (pass_len >= 8) ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    (void)arg;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (!s_sta_give_up && s_sta_retries < STA_MAX_RETRY) {
            s_sta_retries++;
            ESP_LOGW(TAG, "station retry %d", s_sta_retries);
            esp_wifi_connect();
        } else if (!s_sta_give_up) {
            s_sta_give_up = true;
            xEventGroupSetBits(s_sta_events, STA_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "station ip " IPSTR, IP2STR(&event->ip_info.ip));
        s_sta_retries = 0;
        xEventGroupSetBits(s_sta_events, STA_CONNECTED_BIT);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
    }
}

static esp_err_t nvs_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "nvs erase");
        err = nvs_flash_init();
    }
    return err;
}

static esp_err_t start_softap(const char *ssid, const char *pass, uint8_t channel)
{
    wifi_config_t ap;
    fill_ap(&ap, ssid, pass, channel);
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_AP), TAG, "set mode ap");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &ap), TAG, "set ap config");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi start");
    s_ap_up = true;

    esp_netif_ip_info_t ip;
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip);
    ESP_LOGI(TAG, "softAP ssid=%s channel=%u ip=" IPSTR, ssid, (unsigned)channel, IP2STR(&ip.ip));
    return ESP_OK;
}

bool nut_wifi_ap_is_up(void)
{
    return s_ap_up;
}

esp_err_t nut_wifi_start(const nut_wifi_config_t *cfg)
{
    ESP_RETURN_ON_ERROR(nvs_init(), TAG, "nvs");
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "event loop");

    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&wcfg), TAG, "wifi init");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                            wifi_event_handler, NULL, NULL),
                        TAG, "wifi handler");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                            wifi_event_handler, NULL, NULL),
                        TAG, "ip handler");

    if (!cfg->sta) {
        return start_softap(cfg->ssid, cfg->passphrase, cfg->channel);
    }

    s_sta_events = xEventGroupCreate();
    if (!s_sta_events) {
        return ESP_ERR_NO_MEM;
    }
    s_sta_retries = 0;
    s_sta_give_up = false;

    wifi_config_t sta;
    fill_sta(&sta, cfg->ssid, cfg->passphrase);
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "set mode sta");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &sta), TAG, "set sta config");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi start");
    ESP_LOGI(TAG, "joining ssid=%s", cfg->ssid);

    EventBits_t bits = xEventGroupWaitBits(s_sta_events,
                                            STA_CONNECTED_BIT | STA_FAIL_BIT,
                                            pdFALSE, pdFALSE, pdMS_TO_TICKS(20000));
    if (bits & STA_CONNECTED_BIT) {
        s_ap_up = false;
        return ESP_OK;
    }

    ESP_LOGW(TAG, "station join failed, starting default softAP");
    s_sta_give_up = true;
    esp_wifi_stop();
    return start_softap("nut", "espdonut", 9);
}
