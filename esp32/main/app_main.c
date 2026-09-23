#include "common.h"
#include "nut_version.h"
#include "upsconf.h"

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "nut_portal.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG PACKAGE

extern int main(int argc, char **argv);
extern int drv_main(int argc, char **argv);
extern void hidHostInstall(void);

void mountFS(void)
{
    const esp_vfs_fat_mount_config_t mount_config = {
        .max_files = 8,
        .format_if_mount_failed = true,
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE,
        .use_one_fat = false,
    };

    static wl_handle_t s_var_wl_handle = WL_INVALID_HANDLE;
    static wl_handle_t s_usr_wl_handle = WL_INVALID_HANDLE;

    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl("/var", "var", &mount_config, &s_var_wl_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
        return;
    }

    if (mkdir("/var/db", 0755) < 0 || mkdir("/var/db/nut", 0755) < 0) {
        if (errno != EEXIST) {
            ESP_LOGE(TAG, "Failed to create directory: %s", strerror(errno));
            return;
        }
    }

    err = esp_vfs_fat_spiflash_mount_rw_wl("/usr", "usr", &mount_config, &s_usr_wl_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
        return;
    }

    if (mkdir("/usr/local", 0755) < 0 || mkdir("/usr/local/etc", 0755) < 0 || mkdir("/usr/local/etc/nut", 0755) < 0) {
        if (errno != EEXIST) {
            ESP_LOGE(TAG, "Failed to create directory: %s", strerror(errno));
            return;
        }
    }
}

static void write_if_missing(const char *path, const char *contents)
{
    FILE *f = fopen(path, "r");
    if (f) {
        fclose(f);
        return;
    }
    f = fopen(path, "w");
    if (!f) {
        ESP_LOGE(TAG, "Failed to create %s: %s", path, strerror(errno));
        return;
    }
    fputs(contents, f);
    fclose(f);
}

static void seed_default_config(void)
{
    write_if_missing("/usr/local/etc/nut/nut.conf", "MODE=netserver\n");
    write_if_missing("/usr/local/etc/nut/upsd.conf",
                     "ALLOW_NO_DEVICE true\nLISTEN 0.0.0.0 3493\nMAXCONN 4\n");
    write_if_missing("/usr/local/etc/nut/ups.conf",
                     "[hidapc]\n  driver = usbhid-ups\n  port = auto\n  desc = \"APC\"\n");
    write_if_missing("/usr/local/etc/nut/upsd.users",
                     "[nut]\n  password = espdonut\n  actions = SET\n  instcmds = ALL\n\n"
                     "[monuser]\n  password = pass\n  upsmon primary\n");
}

extern void do_upsconf_args_driver(char *upsname, char *var, char *val);

static void nut_main(void *pvParameter)
{
    (void)pvParameter;
    while (1) {
        optind = 0;
        callback_upsconf_args = do_upsconf_args;
        char *args[2] = {PACKAGE_NAME, "-F"};
        main(2, args);
        vTaskDelay(1);
    }
}

static void drv_task(void *pvParameter)
{
    (void)pvParameter;
    while (1) {
        optind = 0;
        callback_upsconf_args = do_upsconf_args_driver;
        /* -s skips ups.conf (shared parser would otherwise use upsd's callback). */
        char *args[4] = {"usbhid-ups", "-F", "-shidapc", "-xport=auto"};
        drv_main(4, args);
        vTaskDelay(1);
    }
}

void app_main(void)
{
    /* Debug 9 plus missing va_copy made vupslog spin forever and starve upsd.
     * Keep a little verbosity on UART without flooding the driver init path. */
    nut_debug_level = 1;

    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = 60000,
        .idle_core_mask = (1 << CONFIG_FREERTOS_NUMBER_OF_CORES) - 1,
        .trigger_panic = false,
    };
    ESP_ERROR_CHECK(esp_task_wdt_reconfigure(&twdt_config));

    mountFS();
    seed_default_config();

    nut_wifi_config_t wifi_cfg;
    ESP_ERROR_CHECK(nut_wifi_conf_load(&wifi_cfg));
    ESP_ERROR_CHECK(nut_wifi_start(&wifi_cfg));
    if (nut_eth_start() != ESP_OK) {
        ESP_LOGW(TAG, "Ethernet unavailable, continuing on Wi-Fi");
    }
    ESP_ERROR_CHECK(nut_portal_start());

    hidHostInstall();

    ESP_LOGI(TAG, "starting usbhid-ups task");
    BaseType_t task_created = xTaskCreatePinnedToCore(drv_task, "drv_main", 8192 * 2, NULL, 5, NULL, 0);
    assert(task_created == pdTRUE);
    /* Let the driver finish HID init and bind its AF_UNIX stand-in before upsd
     * starts; the two programs share global NUT state in this single firmware. */
    vTaskDelay(pdMS_TO_TICKS(8000));

    ESP_LOGI(TAG, "starting upsd task");
    task_created = xTaskCreatePinnedToCore(nut_main, "nut_main", 8192 * 2, NULL, 5, NULL, 1);
    assert(task_created == pdTRUE);

    while (1) {
        taskYIELD();
    }
}
