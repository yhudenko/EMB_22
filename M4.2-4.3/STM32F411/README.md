# STM32F411CEU6 (Black Pill)

PlatformIO project. Each program is an env with its own folder in `src/`.

Wiring ST-Link (SWD, not I2C): SWDIO -> DIO, SWCLK -> SCK, 3V3 -> 3V3, GND -> GND.

Build/upload: `pio run -e program1 -t upload`
