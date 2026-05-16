#include "esp_chip_info.h"
#include "includes/console.h"
#include "includes/enow.h"
#include "includes/http.h"
#include "includes/led.h"
#include "includes/wifi.h"
#include "nvs_flash.h"
#include <stdio.h>

void app_main(void) {
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);
  printf("Hello from ESP32-C6!\n");
  printf("Chip: %s, %d core(s), revision v%d.%d\n", CONFIG_IDF_TARGET,
         chip_info.cores, chip_info.revision / 100, chip_info.revision % 100);

  // WiFi calibration data lives in NVS; init it before esp_wifi_init().
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  led_init();
  wifi_init_sta();
  http_server_init();
  enow_init();
  console_start();
}
