/* Force-included only for NUT translation units.
 * Remap AF_UNIX to lwIP TCP loopback; leave AF_INET alone (upsd:3493).
 */
#ifndef NUT_ESP32_POSIX_WRAP_H
#define NUT_ESP32_POSIX_WRAP_H 1

#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>

int kill(pid_t pid, int sig);

/* AF_UNIX is remapped in af_unix.c via --wrap=lwip_* (ESP-IDF 6 inlines
 * bind/socket to lwip_bind/lwip_socket, so preprocessor macros cannot). */

#endif
