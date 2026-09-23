#include "syslog.h"

#include "esp_log.h"

#include <stdarg.h>
#include <stdio.h>

#define TAG "nut"

void closelog(void) {}
void openlog(const char *ident, int option, int facility)
{
    (void)ident;
    (void)option;
    (void)facility;
}
int setlogmask(int mask)
{
    (void)mask;
    return 0;
}
void syslog(int pri, const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    switch (LOG_PRI(pri)) {
    case LOG_EMERG:
    case LOG_ALERT:
    case LOG_CRIT:
    case LOG_ERR:
        ESP_LOGE(TAG, "%s", buf);
        break;
    case LOG_WARNING:
        ESP_LOGW(TAG, "%s", buf);
        break;
    case LOG_DEBUG:
        ESP_LOGD(TAG, "%s", buf);
        break;
    default:
        ESP_LOGI(TAG, "%s", buf);
        break;
    }
}
