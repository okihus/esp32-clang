#include "includes/http.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif_types.h"
#include "esp_timer.h"
#include <stdio.h>

static const char *TAG = "http";
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

static void on_got_ip(void *arg, esp_event_base_t base, int32_t id,
                      void *data) {
  start_webserver();
}

void http_server_init(void) {
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip, NULL, NULL));
}
