#ifndef HELLO_LED_H
#define HELLO_LED_H

#include "led_strip_types.h"

#define LED_GPIO 8
#define LED_COUNT 1

void led_init(void);
void led_set(uint8_t r, uint8_t g, uint8_t b);

#endif
