#pragma once

#include "esp_err.h"

#include <stdint.h>

/* Answer every DNS A query with ip_nbo (IPv4 address in network byte order). */
esp_err_t nut_dns_server_start(uint32_t ip_nbo);
