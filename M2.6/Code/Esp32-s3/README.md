# ESP32-S3 PlatformIO Arduino Programs

This project targets the same board setting as `D:\VS_projects\esp32-s3-usb-repeater`:

```ini
board = 4d_systems_esp32s3_gen4_r8n16
framework = arduino
```

There are independent Arduino programs. Select the environment from the PlatformIO environment switcher, then build or upload that environment.

## Programs

| Environment | Folder | What it does |
| --- | --- | --- |
| `program1` | `src/program1` | Blinks the onboard WS2812 RGB LED on IO48_RGB with potentiometer color and speed control. |
| `program2` | `src/program2` | Multiplexes two common-anode 7-segment digits and uses a potentiometer to vary scan frequency so flicker/collision becomes visible. |
| `program3` | `src/program3` | Uses ESP32 hardware timers for the two-digit multiplex tick and the 1 Hz number update, with potentiometer control of scan frequency. |
| `program4` | `src/program4` | Demonstrates real-second display skips when the potentiometer slows the loop. |
| `program5` | `src/program5` | Uses two non-blocking timers: display once per second, dot/RGB period from the potentiometer. |
| `program6` | `src/program6` | Uses ESP32 hardware timers for the display and dot/RGB tasks. |
| `program7` | `src/program7` | Generates servo PWM with an ESP32 hardware timer interrupt and potentiometer angle control. |

Each environment uses `build_src_filter` so only its own folder is compiled.

## Program 1 Onboard RGB LED

`program1` uses `neopixelWrite()` on raw GPIO 48 for the onboard WS2812 LED marked `IO48_RGB`.

Connect a 10 kOhm potentiometer like this:

| Potentiometer pin | ESP32-S3 connection |
| --- | --- |
| One outside pin | `3V3` |
| Middle pin / wiper | `GPIO 1` |
| Other outside pin | `GND` |

The potentiometer changes both blink speed and color. If the direction feels backwards, swap the two outside potentiometer wires. Do not connect the potentiometer to `5V`; use `3V3` only.

## Program 2 Two-Digit Multiplex Collision Demo

`program2` drives two common-anode 7-segment digits that share the same segment lines. Each digit anode is switched separately through a transistor so the code can multiplex them in the main `loop()` without RTOS. A potentiometer changes the scan interval. At fast settings the display looks stable, and at slow settings flicker and digit-to-digit collision become visible.

Shared segment wiring:

| Display pin | ESP32-S3 GPIO |
| --- | --- |
| E | `GPIO 4` |
| D | `GPIO 5` |
| C | `GPIO 6` |
| DP / dot | `GPIO 7` |
| G | `GPIO 8` |
| F | `GPIO 9` |
| A | `GPIO 10` |
| B | `GPIO 11` |

Digit enable wiring through transistor stage:

| Digit | ESP32-S3 GPIO |
| --- | --- |
| Left digit anode enable | `GPIO 1` |
| Right digit anode enable | `GPIO 2` |

Potentiometer wiring for scan-frequency control:

| Potentiometer pin | ESP32-S3 connection |
| --- | --- |
| One outside pin | `3V3` |
| Middle pin / wiper | `GPIO 14` |
| Other outside pin | `GND` |

Use one resistor in series with each segment and the dot. For common anode, the code turns a segment on with `LOW` and off with `HIGH`. If your transistor stage is active-low, change the digit-enable levels in `include/seven_segment_config.h`.

## Program 3 Hardware Timer Multiplex Demo

`program3` keeps the same two-digit common-anode wiring as `program2`, but replaces the `millis()` and `micros()` scheduling with ESP32 hardware timers. Timer 0 raises a 1 Hz event for the displayed value. Timer 1 raises the multiplex scan event. The normal `loop()` only reads the potentiometer on `GPIO 14`, updates the timer-1 alarm period when needed, and performs the actual GPIO writes for the active digit.

Shared segment wiring:

| Display pin | ESP32-S3 GPIO |
| --- | --- |
| E | `GPIO 4` |
| D | `GPIO 5` |
| C | `GPIO 6` |
| DP / dot | `GPIO 7` |
| G | `GPIO 8` |
| F | `GPIO 9` |
| A | `GPIO 10` |
| B | `GPIO 11` |

Digit enable wiring through transistor stage:

| Digit | ESP32-S3 GPIO |
| --- | --- |
| Left digit anode enable | `GPIO 1` |
| Right digit anode enable | `GPIO 2` |

Potentiometer wiring for scan-frequency control:

| Potentiometer pin | ESP32-S3 connection |
| --- | --- |
| One outside pin | `3V3` |
| Middle pin / wiper | `GPIO 14` |
| Other outside pin | `GND` |

Use one resistor in series with each segment and the dot. For common anode, the code turns a segment on with `LOW` and off with `HIGH`. If your transistor stage is active-low, change the digit-enable levels in `include/seven_segment_config.h`.

## Program 4 Blocking Loop Timing Test

`program4` uses the same common-anode 7-segment, decimal point, onboard RGB LED, and potentiometer wiring as `program3`.

The display shows the current real second from `millis()` modulo 10. The potentiometer controls a blocking loop delay from about 100 ms to 3000 ms. When the delay is high, the displayed value can skip seconds, and the decimal point plus onboard RGB LED toggle only when the displayed number changes.

## Program 5 Two-Timer Test

`program5` uses the same wiring as `program3` and `program4`, but has no blocking `delay()`. One software timer updates the 7-segment display once per second. A second software timer uses the potentiometer to set the decimal point and onboard RGB LED blink period from about 100 ms to 3000 ms.

The display timing stays steady even when the potentiometer makes the dot and onboard RGB blink slowly.

## Program 6 Hardware Timer Test

`program6` uses the same wiring and behavior as `program5`, but schedules the two tasks with ESP32 hardware timers. Timer 0 raises a display update event once per second. Timer 1 raises a dot/RGB blink event with its period set by the potentiometer from about 100 ms to 3000 ms.

The timer interrupts only set counters. The normal `loop()` still performs the ADC read, 7-segment GPIO writes, and WS2812 update.

## Program 7 Servo PWM Hardware Timer Test

`program7` generates a 50 Hz hobby-servo PWM signal on `GPIO 12` with Timer 0. The timer interrupt runs every 20 us, starts each 20 ms servo frame, and turns the servo signal off after the selected pulse width. The normal `loop()` reads the potentiometer and updates the pulse width from about 500 us to 2500 us, corresponding to about 0 to 180 degrees.

Servo wiring:

| Servo wire | ESP32-S3 connection |
| --- | --- |
| Signal | `GPIO 2` |
| Power | External servo supply matching the servo rating |
| Ground | External supply ground and ESP32-S3 `GND` |

Use the same potentiometer wiring as `program1`. Do not power a servo from the ESP32-S3 `3V3` pin; use a separate servo supply and connect the grounds together.