#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_chip_info.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "wifi";
static EventGroupHandle_t s_wifi_event_group;
static int s_retry = 0;
static httpd_handle_t s_server = NULL;
static volatile int s_tick = 0;

static esp_err_t root_get_handler(httpd_req_t *req)
{
    char body[256];
    int n = snprintf(body, sizeof(body),
        "<!doctype html><html><body>"
        "<h1>Hello from ESP32-C6</h1>"
        "<p>Tick: %d</p>"
        "<p>Uptime: %lld s</p>"
        "</body></html>",
        s_tick, esp_timer_get_time() / 1000000);
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, body, n);
}

static void start_webserver(void)
{
    if (s_server != NULL) return;
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

static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry < CONFIG_WIFI_MAX_RETRY) {
            s_retry++;
            ESP_LOGW(TAG, "Disconnected, retrying (%d/%d)", s_retry, CONFIG_WIFI_MAX_RETRY);
            esp_wifi_connect();
        } else {
            ESP_LOGE(TAG, "Connection failed after %d retries", CONFIG_WIFI_MAX_RETRY);
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

static void wifi_init_sta(void)
{
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
        .sta = {
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

void app_main(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("Hello from ESP32-C6!\n");
    printf("Chip: %s, %d core(s), revision v%d.%d\n",
           CONFIG_IDF_TARGET, chip_info.cores,
           chip_info.revision / 100, chip_info.revision % 100);

    // WiFi calibration data lives in NVS; init it before esp_wifi_init().
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_sta();

    for (s_tick = 0; ; s_tick++) {
        printf("Tick %d\n", s_tick);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
