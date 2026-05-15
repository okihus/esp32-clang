# Override on the command line: `just port=/dev/ttyACM1 flash`
port := "/dev/ttyACM0"

# Show available recipes
default:
    @just --list

reconfigure:
    idf.py reconfigure

# Compile the firmware
build:
    idf.py build

# Flash to the chip (also builds if needed)
flash:
    idf.py -p {{port}} flash

# Open serial monitor with tio (Ctrl+T q to quit)
monitor:
    tio {{port}}

# Full dev loop: build, flash, then monitor
dev: flash monitor

# Open the Kconfig menu (set WiFi creds here)
# TERM override: Ghostty's terminfo isn't in the curses DB kconfig uses.
menuconfig:
    TERM=xterm-256color idf.py menuconfig

# Remove build artifacts
clean:
    idf.py clean

# Nuke build dir + cmake cache (use when sdkconfig changes break things)
fullclean:
    idf.py fullclean

# Show binary size breakdown by component
size:
    idf.py size-components

# Print detected ESP serial device(s)
which-port:
    @ls -l /dev/serial/by-id/ 2>/dev/null | grep -i esp || echo "no ESP device found"

# Erase the entire flash (factory reset)
erase:
    idf.py -p {{port}} erase-flash
