#include "includes/led.h"
#include "led_strip.h"

static led_strip_handle_t s_led;

void led_init(void) {
  led_strip_config_t strip_cfg = {
      .strip_gpio_num = LED_GPIO,
      .max_leds = LED_COUNT,
  };
  led_strip_rmt_config_t rmt_cfg = {
      .resolution_hz = 10 * 1000 * 1000,
  };
  ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &s_led));
  led_strip_clear(s_led);
}

void led_set(uint8_t r, uint8_t g, uint8_t b) {
  led_strip_set_pixel(s_led, 0, r, g, b);
  led_strip_refresh(s_led);
}
