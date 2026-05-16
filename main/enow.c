#include "includes/enow.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "enow";

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
    ESP_LOGW(TAG, "send to " MACSTR " failed", MAC2STR(info->des_addr));
  }
}

void enow_init(void) {
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
  ESP_LOGI(TAG, "ready, my MAC " MACSTR, MAC2STR(mac));
}

esp_err_t enow_broadcast(const void *data, size_t len) {
  return esp_now_send(s_broadcast_mac, (const uint8_t *)data, len);
}
