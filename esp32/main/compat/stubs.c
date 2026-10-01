#include "config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <dirent.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define TAG PACKAGE

void rtos_yield(void)
{
    vTaskDelay(1);
}

void exit(int status)
{
    ESP_LOGW(TAG, "exit(%d) in task %s — parking task (no reboot)",
             status, pcTaskGetName(NULL) ? pcTaskGetName(NULL) : "?");
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

void __wrap_abort(void)
{
    exit(1);
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

int sigaction(int sig, const struct sigaction *act, struct sigaction *oldact)
{
    (void)sig;
    (void)act;
    (void)oldact;
    return 0;
}

void (*signal(int sig, void (*handler)(int)))(int)
{
    (void)sig;
    (void)handler;
    return 0;
}

int kill(pid_t pid, int sig)
{
    (void)pid;
    (void)sig;
    return 0;
}

int pipe(int pipefd[2])
{
    if (pipefd) {
        pipefd[0] = -1;
        pipefd[1] = -1;
    }
    errno = ENOSYS;
    return -1;
}

struct passwd *getpwuid(uid_t uid)
{
    static struct passwd p = {
        "nut",
        "espdonut",
        0,
        0,
        "",
        "NUT User",
        "/var/lib/nut",
        "/bin/false",
    };
    (void)uid;
    return &p;
}

struct passwd *getpwnam(const char *name)
{
    (void)name;
    return getpwuid(0);
}

struct group *getgrnam(const char *name)
{
    static struct group g = {
        "nut",
        "espdonut",
        0,
        NULL,
    };
    (void)name;
    return &g;
}

int fchmod(int fd, mode_t mode)
{
    (void)fd;
    (void)mode;
    return 0;
}

int fchown(int fd, uid_t owner, gid_t group)
{
    (void)fd;
    (void)owner;
    (void)group;
    return 0;
}

int chown(const char *path, uid_t owner, gid_t group)
{
    (void)path;
    (void)owner;
    (void)group;
    return 0;
}

int chroot(const char *path)
{
    (void)path;
    return 0;
}

int setuid(uid_t uid)
{
    (void)uid;
    return 0;
}

int setgid(gid_t gid)
{
    (void)gid;
    return 0;
}

int initgroups(const char *user, gid_t group)
{
    (void)user;
    (void)group;
    return 0;
}

int seteuid(uid_t uid)
{
    (void)uid;
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

int dup(int fd)
{
    (void)fd;
    return 0;
}

mode_t umask(mode_t mask)
{
    (void)mask;
    return 0;
}
