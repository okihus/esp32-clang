#include "argtable3/argtable3.h"
#include "esp_chip_info.h"
#include "esp_console.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"
#include "includes/led.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <string.h>

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static const char *TAG = "wifi";
static EventGroupHandle_t s_wifi_event_group;
static int s_retry = 0;
static httpd_handle_t s_server = NULL;

static esp_err_t root_get_handler(httpd_req_t *req) {
  char body[256];
  int n = snprintf(body, sizeof(body),
                   "<!doctype html><html><body>"
                   "<h1>Hello from ESP32-C6</h1>"
                   "<p>Uptime: %lld s</p>"
                   "</body></html>",
                   esp_timer_get_time() / 1000000);
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, body, n);
}

static void start_webserver(void) {
  if (s_server != NULL)
    return;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  if (httpd_start(&s_server, &config) == ESP_OK) {
    httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_get_handler,
    };
    httpd_register_uri_handler(s_server, &root);
    ESP_LOGI(TAG, "HTTP server listening on :80");
  } else {
    ESP_LOGE(TAG, "Failed to start HTTP server");
  }
}

static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id,
                          void *data) {
  if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
    if (s_retry < CONFIG_WIFI_MAX_RETRY) {
      s_retry++;
      ESP_LOGW(TAG, "Disconnected, retrying (%d/%d)", s_retry,
               CONFIG_WIFI_MAX_RETRY);
      esp_wifi_connect();
    } else {
      ESP_LOGE(TAG, "Connection failed after %d retries",
               CONFIG_WIFI_MAX_RETRY);
      xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }
  } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
    ESP_LOGI(TAG, "Got IP " IPSTR, IP2STR(&event->ip_info.ip));
    s_retry = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    start_webserver();
  }
}

static void wifi_init_sta(void) {
  s_wifi_event_group = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  esp_event_handler_instance_t any_evt, ip_evt;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &on_wifi_event, NULL, &any_evt));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, &on_wifi_event, NULL, &ip_evt));

  wifi_config_t wifi_config = {
      .sta =
          {
              .ssid = CONFIG_WIFI_SSID,
              .password = CONFIG_WIFI_PASSWORD,
              .threshold.authmode = WIFI_AUTH_WPA2_PSK,
          },
  };
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(TAG, "Connecting to SSID \"%s\"...", CONFIG_WIFI_SSID);
}

static const uint8_t s_broadcast_mac[ESP_NOW_ETH_ALEN] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

static void on_enow_recv(const esp_now_recv_info_t *info, const uint8_t *data,
                         int len) {
  printf("\n[enow] " MACSTR " (%d): %.*s\n", MAC2STR(info->src_addr), len, len,
         (const char *)data);
}

static void on_enow_send(const wifi_tx_info_t *info,
                         esp_now_send_status_t status) {
  if (status != ESP_NOW_SEND_SUCCESS) {
    ESP_LOGW("enow", "send to " MACSTR " failed", MAC2STR(info->des_addr));
  }
}

static void enow_init(void) {
  ESP_ERROR_CHECK(esp_now_init());
  ESP_ERROR_CHECK(esp_now_register_recv_cb(on_enow_recv));
  ESP_ERROR_CHECK(esp_now_register_send_cb(on_enow_send));

  esp_now_peer_info_t peer = {
      .channel = 0,
      .ifidx = WIFI_IF_STA,
      .encrypt = false,
  };
  memcpy(peer.peer_addr, s_broadcast_mac, ESP_NOW_ETH_ALEN);
  ESP_ERROR_CHECK(esp_now_add_peer(&peer));

  uint8_t mac[ESP_NOW_ETH_ALEN];
  esp_wifi_get_mac(WIFI_IF_STA, mac);
  ESP_LOGI("enow", "ready, my MAC " MACSTR, MAC2STR(mac));
}

static int cmd_enow(int argc, char **argv) {
  if (argc < 2) {
    printf("usage: enow <text>\n");
    return 1;
  }
  char buf[ESP_NOW_MAX_DATA_LEN];
  size_t off = 0;
  for (int i = 1; i < argc && off < sizeof(buf); i++) {
    if (i > 1 && off < sizeof(buf))
      buf[off++] = ' ';
    size_t n = strlen(argv[i]);
    if (off + n > sizeof(buf))
      n = sizeof(buf) - off;
    memcpy(buf + off, argv[i], n);
    off += n;
  }
  esp_err_t err = esp_now_send(s_broadcast_mac, (const uint8_t *)buf, off);
  if (err != ESP_OK) {
    printf("send failed: %s\n", esp_err_to_name(err));
    return 1;
  }
  return 0;
}

static struct {
  struct arg_int *r;
  struct arg_int *g;
  struct arg_int *b;
  struct arg_end *end;
} led_args;

static int cmd_led(int argc, char **argv) {
  int nerrors = arg_parse(argc, argv, (void **)&led_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, led_args.end, argv[0]);
    return 1;
  }
  led_set(led_args.r->ival[0], led_args.g->ival[0], led_args.b->ival[0]);
  return 0;
}

static int cmd_reboot(int argc, char **argv) {
  printf("rebooting...\n");
  esp_restart();
  return 0;
}

static int cmd_info(int argc, char **argv) {
  esp_chip_info_t info;
  esp_chip_info(&info);
  printf("chip: %s, %d core(s), rev v%d.%d\n", CONFIG_IDF_TARGET, info.cores,
         info.revision / 100, info.revision % 100);
  printf("uptime: %lld s\n", esp_timer_get_time() / 1000000);
  return 0;
}

static void start_repl(void) {
  esp_console_repl_t *repl = NULL;
  esp_console_repl_config_t repl_cfg = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
  repl_cfg.prompt = "esp32> ";
  esp_console_dev_usb_serial_jtag_config_t dev_cfg =
      ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(
      esp_console_new_repl_usb_serial_jtag(&dev_cfg, &repl_cfg, &repl));

  esp_console_register_help_command();

  led_args.r = arg_int1(NULL, NULL, "<r>", "red 0-255");
  led_args.g = arg_int1(NULL, NULL, "<g>", "green 0-255");
  led_args.b = arg_int1(NULL, NULL, "<b>", "blue 0-255");
  led_args.end = arg_end(2);
  const esp_console_cmd_t led_cmd = {
      .command = "led",
      .help = "Set RGB LED color (0-255 per channel)",
      .hint = NULL,
      .func = &cmd_led,
      .argtable = &led_args,
  };
  ESP_ERROR_CHECK(esp_console_cmd_register(&led_cmd));

  const esp_console_cmd_t info_cmd = {
      .command = "info",
      .help = "Show chip info and uptime",
      .func = &cmd_info,
  };
  ESP_ERROR_CHECK(esp_console_cmd_register(&info_cmd));

  const esp_console_cmd_t enow_cmd = {
      .command = "enow",
      .help = "Broadcast a text message via ESP-NOW",
      .func = &cmd_enow,
  };
  ESP_ERROR_CHECK(esp_console_cmd_register(&enow_cmd));

  const esp_console_cmd_t reboot_cmd = {
      .command = "reboot",
      .help = "Software reset the device",
      .func = &cmd_reboot,
  };
  ESP_ERROR_CHECK(esp_console_cmd_register(&reboot_cmd));

  ESP_ERROR_CHECK(esp_console_start_repl(repl));
}

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
  enow_init();
  start_repl();
}
