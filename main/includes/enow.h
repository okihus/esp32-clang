#ifndef ESP32_ENOW_H
#define ESP32_ENOW_H

#include "esp_err.h"
#include <stddef.h>

void enow_init(void);
esp_err_t enow_broadcast(const void *data, size_t len);

#endif
