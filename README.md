# esp32

ESP-IDF project targeting the ESP32-C6.

## WiFi credentials

WiFi SSID and password are sourced from Kconfig (`CONFIG_WIFI_SSID`,
`CONFIG_WIFI_PASSWORD`). To keep your creds out of git **and** out of harm's
way when `sdkconfig` is regenerated, this project layers a local defaults
file on top of the shared `sdkconfig.defaults`.

### One-time setup

Create `sdkconfig.defaults.local` in the project root:

```
CONFIG_WIFI_SSID="your-ssid"
CONFIG_WIFI_PASSWORD="your-password"
```

It's already listed in `.gitignore`, so it won't be committed.

`CMakeLists.txt` wires it in via:

```cmake
set(SDKCONFIG_DEFAULTS "sdkconfig.defaults;sdkconfig.defaults.local")
```

Values in `sdkconfig.defaults.local` override anything in
`sdkconfig.defaults`, and `sdkconfig` (which menuconfig writes to) overrides
both at build time.

### Applying changes

If `sdkconfig` already exists, ESP-IDF won't re-read the defaults. To pick
up new values from `sdkconfig.defaults.local`:

```
just fullclean
just reconfigure
```

Or override interactively with `just menuconfig`.
