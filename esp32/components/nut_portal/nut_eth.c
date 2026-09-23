#include "nut_portal.h"

#include "esp_check.h"
#include "esp_eth.h"
#include "esp_eth_driver.h"
#include "esp_eth_mac_w5500.h"
#include "esp_eth_netif_glue.h"
#include "esp_eth_phy_w5500.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"

#include "driver/spi_master.h"

/* Waveshare ESP32-S3-ETH, W5500 wired as in the ETH_DHCP example. */
#define ETH_SPI_HOST   SPI2_HOST
#define ETH_SPI_SCLK   13
#define ETH_SPI_MOSI   11
#define ETH_SPI_MISO   12
#define ETH_SPI_CS     14
#define ETH_SPI_INT    10
#define ETH_SPI_RST    9
#define ETH_SPI_MHZ    20

static const char *TAG = "nut_eth";

static void eth_event(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    if (event_base == ETH_EVENT && event_id == ETHERNET_EVENT_CONNECTED) {
        ESP_LOGI(TAG, "link up");
    } else if (event_base == ETH_EVENT && event_id == ETHERNET_EVENT_DISCONNECTED) {
        ESP_LOGW(TAG, "link down");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_ETH_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "ethernet ip " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

esp_err_t nut_eth_start(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = ETH_SPI_MOSI,
        .miso_io_num = ETH_SPI_MISO,
        .sclk_io_num = ETH_SPI_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .data4_io_num = -1,
        .data5_io_num = -1,
        .data6_io_num = -1,
        .data7_io_num = -1,
        .max_transfer_sz = 4096,
    };
    esp_err_t err = spi_bus_initialize(ETH_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "SPI bus init failed: %s", esp_err_to_name(err));
        return err;
    }

    spi_device_interface_config_t devcfg = {
        .mode = 0,
        .clock_speed_hz = ETH_SPI_MHZ * 1000 * 1000,
        .spics_io_num = ETH_SPI_CS,
        .queue_size = 20,
    };
    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(ETH_SPI_HOST, &devcfg);
    w5500_config.base.int_gpio_num = ETH_SPI_INT;

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    mac_config.rx_task_stack_size = 4096;
    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    if (!mac) {
        ESP_LOGW(TAG, "W5500 MAC init failed");
        return ESP_FAIL;
    }

    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.reset_gpio_num = ETH_SPI_RST;
    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_config);
    if (!phy) {
        ESP_LOGW(TAG, "W5500 PHY init failed");
        return ESP_FAIL;
    }

    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;
    ESP_RETURN_ON_ERROR(esp_eth_driver_install(&eth_config, &eth_handle), TAG, "driver install");

    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *netif = esp_netif_new(&netif_cfg);
    if (!netif) {
        return ESP_ERR_NO_MEM;
    }
    esp_eth_netif_glue_handle_t glue = esp_eth_new_netif_glue(eth_handle);
    if (!glue) {
        return ESP_ERR_NO_MEM;
    }
    ESP_RETURN_ON_ERROR(esp_netif_attach(netif, glue), TAG, "netif attach");

    ESP_RETURN_ON_ERROR(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, eth_event, NULL),
                        TAG, "eth events");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, eth_event, NULL),
                        TAG, "ip events");
    ESP_RETURN_ON_ERROR(esp_eth_start(eth_handle), TAG, "eth start");

    ESP_LOGI(TAG, "W5500 started, waiting for link");
    return ESP_OK;
}
