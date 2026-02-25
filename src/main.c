#include "nut_common.h"
#include "nut_version.h"
#include <pwd.h>
#include <grp.h>

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "esp_task_wdt.h"

#define TAG PACKAGE

extern int main(int, char **);

extern int drivers_main(int, char **);

void mountFS(void)
{
    const esp_vfs_fat_mount_config_t mount_config = {
        .max_files = 4,                                // Number of files that can be open at a time
        .format_if_mount_failed = true,                // If true, try to format the partition if mount fails
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE, // Size of allocation unit, cluster size.
        .use_one_fat = false,                          // Use only one FAT table (reduce memory usage), but decrease reliability of file system in case of power failure.
    };

    // Handle of the wear levelling library instance
    static wl_handle_t s_var_wl_handle = WL_INVALID_HANDLE;
    static wl_handle_t s_usr_wl_handle = WL_INVALID_HANDLE;

    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl("/var", "var", &mount_config, &s_var_wl_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
        return;
    }

    if (mkdir("/var/db", 0755) < 0 || mkdir("/var/db/nut", 0755) < 0)
    {
        if (errno != EEXIST)
        {
            ESP_LOGE(TAG, "Failed to create directory: %s", strerror(errno));
            return;
        }
    }

    FILE *f = fopen("/var/db/nut/nut.pid", "wb");
    if (f == NULL)
    {
        perror("fopen"); // Print reason why fopen failed
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }

    fseek(f, 0, SEEK_END);
    if (0 == ftell(f))
    {
        fprintf(f, "empty");
        fflush(f);
    }
    fclose(f);

    f = fopen("/var/db/nut/apc-hid-hidapc.pid", "wb");
    if (f == NULL)
    {
        perror("fopen"); // Print reason why fopen failed
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }

    fseek(f, 0, SEEK_END);
    if (0 == ftell(f))
    {
        fprintf(f, "empty");
        fflush(f);
    }
    fclose(f);

    f = fopen("/var/db/nut/apc-hid-hidapc", "wb");
    if (f == NULL)
    {
        perror("fopen"); // Print reason why fopen failed
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }
    fclose(f);

    err = esp_vfs_fat_spiflash_mount_rw_wl("/usr", "usr", &mount_config, &s_usr_wl_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
        return;
    }

    if (mkdir("/usr/local", 0755) < 0 || mkdir("/usr/local/etc", 0755) < 0 || mkdir("/usr/local/etc/nut", 0755) < 0)
    {
        if (errno != EEXIST)
        {
            ESP_LOGE(TAG, "Failed to create directory: %s", strerror(errno));
            return;
        }
    }

    f = fopen("/usr/local/etc/nut/upsd.users", "wb");
    if (f == NULL)
    {
        perror("fopen"); // Print reason why fopen failed
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }

    fseek(f, 0, SEEK_END);
    if (0 == ftell(f))
    {
        // WARNING: Using default credentials - CHANGE THESE IN PRODUCTION!
        // Default password "espdonut" should be changed immediately after deployment
        // TODO: Implement secure credential storage using ESP32 NVS with encryption
        fprintf(f, "[nut]\n");
        fprintf(f, "  password = espdonut\n");
        fprintf(f, "  actions = SET\n");
        fprintf(f, "  instcmds = ALL\n");
        fprintf(f, "\n");
        fprintf(f, "[monuser]\n");
        fprintf(f, "  password = pass\n");
        fprintf(f, "  upsmon primary\n");
        fprintf(f, "\n");
        fflush(f);
    }
    fclose(f);

    f = fopen("/usr/local/etc/nut/upsd.conf", "wb");
    if (f == NULL)
    {
        perror("fopen"); // Print reason why fopen failed
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }

    fseek(f, 0, SEEK_END);
    if (0 == ftell(f))
    {
        fprintf(f, "ALLOW_NO_DEVICE true\n");
        fprintf(f, "LISTEN 0.0.0.0 3493\n"); // INADDR_ANY
        fprintf(f, "MAXCONN 4\n");
        fflush(f);
    }
    fclose(f);

    f = fopen("/usr/local/etc/nut/ups.conf", "wb");
    if (f == NULL)
    {
        perror("fopen"); // Print reason why fopen failed
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }

    fseek(f, 0, SEEK_END);
    if (0 == ftell(f))
    {
        fprintf(f, "[hidapc]\n");
        fprintf(f, "  driver = apc-hid\n");
        fprintf(f, "  port = auto\n");
        fprintf(f, "  desc = \"APC\"\n");
        fprintf(f, "\n");
        fflush(f);
    }
    fclose(f);

    f = fopen("/usr/local/etc/nut/nut.conf", "wb");
    if (f == NULL)
    {
        perror("fopen"); // Print reason why fopen failed
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }

    fseek(f, 0, SEEK_END);
    if (0 == ftell(f))
    {
        fprintf(f, "MODE=netserver\n");
        fflush(f);
    }
    fclose(f);
}

extern void hidHostInstall(void);

extern void wifi_init_sta(void);
extern void wifi_init_softap(void);
extern void usb_host_lib_task(void *);
extern void class_driver_task(void *);

extern void esp_vfs_af_unix_register(void);

static void nut_main(void *pvParameter)
{
    while (1)
    {
        optind = 0;
        char *args[2] = {PACKAGE_NAME, "-F"};
        main(2, args);
        vTaskDelay(1);
    }
}

static void drv_main(void *pvParameter)
{
    while (1)
    {
        optind = 0;
        char *args[3] = {"apc-hid", "-F", "-ahidapc"};
        drivers_main(3, args);
        vTaskDelay(1);
    }
}

void rtos_yield(void)
{
    // Yield to allow other tasks to run
    vTaskDelay(1);
}

void app_main()
{
    nut_debug_level = 9;

    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = 60000,
        .idle_core_mask = (1 << CONFIG_FREERTOS_NUMBER_OF_CORES) - 1, // Bitmask of all cores
        .trigger_panic = false,
    };

    ESP_ERROR_CHECK(esp_task_wdt_reconfigure(&twdt_config));

    wifi_init_softap();

    mountFS();

    BaseType_t task_created;

    hidHostInstall();

    task_created = xTaskCreatePinnedToCore(drv_main, "drv_main", 8192 * 2, NULL, 5, NULL, 0);

    assert(task_created == pdTRUE);

    ulTaskNotifyTake(false, 1000);

    task_created = xTaskCreatePinnedToCore(nut_main, "nut_main", 8192 * 2, NULL, 5, NULL, 0);

    assert(task_created == pdTRUE);

    ulTaskNotifyTake(false, 1000);

    while (1)
    {
        taskYIELD();
    }
}

const char *gai_strerror(int ecode)
{
    static const char gai_strerror_msgs[] =
        "Invalid flags\0"
        "Name does not resolve\0"
        "Try again\0"
        "Non-recoverable error\0"
        "Name has no usable address\0"
        "Unrecognized address family or invalid length\0"
        "Unrecognized socket type\0"
        "Unrecognized service\0"
        "Unknown error\0"
        "Out of memory\0"
        "System error\0"
        "Overflow\0"
        "\0Unknown error";

    const char *s;
    for (s = gai_strerror_msgs, ecode++; ecode && *s; ecode++, s++)
        for (; *s; s++)
            ;
    if (!*s)
        s++;
    return s;
}

/*
 * ESP32 Platform Stubs
 * 
 * The following functions are stubs for POSIX functions that are not
 * applicable or implemented on the ESP32 platform. They return success
 * to allow the NUT codebase to compile and run, but do not perform
 * actual operations.
 * 
 * WARNING: Code expecting these functions to enforce security or modify
 * system state will not work as expected on ESP32.
 */

int sigaction(int, const struct sigaction *, struct sigaction *)
{
    // ESP32 stub: Signal handling not implemented
    return 0;
}

_sig_func_ptr signal(int, _sig_func_ptr)
{
    // ESP32 stub: Signal handling not implemented
    return 0;
}

int upsconf_driver = 0;

struct passwd *getpwuid(uid_t)
{
    static struct passwd p = {
        "nut",
        "espdonut",
        0,
        0,
        "NUT User",
        "",
        "/var/lib/nut",
        "/bin/false"};
    return &p;
}

struct passwd *getpwnam(const char *name)
{
    static struct passwd p = {
        "nut",
        "espdonut",
        0,
        0,
        "NUT User",
        "",
        "/var/lib/nut",
        "/bin/false"};
    return &p;
}

struct group *getgrnam(const char *)
{
    static struct group g = {
        "nut",
        "espdonut",
        0,
        NULL};
    return &g;
}

int fchmod(int __fd, mode_t __mode)
{
    return 0;
}

int fchown(int __fildes, uid_t __owner, gid_t __group)
{
    return 0;
}

int chown(const char *__path, uid_t __owner, gid_t __group)
{
    return 0;
}

int chroot(const char *__path)
{
    return 0;
}

int setuid(uid_t __uid)
{
    return 0;
}

int setgid(gid_t __gid)
{
    return 0;
}

int initgroups(const char *, gid_t)
{
    return 0;
}

int seteuid(uid_t __uid)
{
    return 0;
}

uid_t getuid(void)
{
    return 0;
}

uid_t geteuid(void)
{
    return 0;
}

gid_t getgid(void)
{
    return 0;
}

pid_t setsid(void)
{
    return 0;
}

int dup(int __fildes)
{
    // ESP32 stub: fd duplication not implemented
    errno = ENOSYS;
    return -1;
}

mode_t umask(mode_t __mask)
{
    return 0;
}

void clean_dir(const char *path)
{
    ESP_LOGI(TAG, "Deleting everything in %s:", path);

    DIR *dir = opendir(path);
    if (!dir)
    {
        ESP_LOGE(TAG, "Failed to open directory: %s", strerror(errno));
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        char full_path[256] = {0};
        int written = snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        if (written < 0 || written >= sizeof(full_path))
        {
            ESP_LOGE(TAG, "Path too long: %s/%s", path, entry->d_name);
            continue;
        }
        if (entry->d_type == DT_DIR)
            clean_dir(full_path);
        if (remove(full_path) != 0)
        {
            ESP_LOGE(TAG, "Failed to remove %s: %s", full_path, strerror(errno));
        }
    }

    closedir(dir);
}
