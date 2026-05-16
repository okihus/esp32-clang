#include "includes/console.h"
#include "argtable3/argtable3.h"
#include "esp_chip_info.h"
#include "esp_console.h"
#include "esp_now.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "includes/enow.h"
#include "includes/led.h"
#include <stdio.h>
#include <string.h>

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
  esp_err_t err = enow_broadcast(buf, off);
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

void console_start(void) {
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
