# ESP32-S3 PlatformIO Arduino Programs

This project uses the following PlatformIO settings:

```ini
board = 4d_systems_esp32s3_gen4_r8n16
framework = arduino
```

There are independent Arduino programs in the `src` folder. Select the environment from the PlatformIO environment switcher, then build or upload that environment.

## Programs

| Environment | Folder | What it does |
| --- | --- | --- |
| `program1` | `src/program1` | Displays a two-digit counter from `00` to `99` on a multiplexed 7-segment display. |
| `program2` | `src/program2` | Displays the same two-digit counter and uses a potentiometer to change the display scan speed. |
| `program3` | `src/program3` | Uses ESP32 hardware timers for counting, display scanning, and potentiometer sampling. |

Each environment uses `build_src_filter` so only its own folder is compiled.

## Shared 7-Segment Wiring

All programs use the same shared pin mapping from `include/seven_segment_config.h`.

| Display signal | ESP32-S3 GPIO |
| --- | --- |
| A | `GPIO 10` |
| B | `GPIO 11` |
| C | `GPIO 6` |
| D | `GPIO 5` |
| E | `GPIO 4` |
| F | `GPIO 9` |
| G | `GPIO 8` |
| DP / dot | `GPIO 7` |
| Digit 1 enable | `GPIO 1` |
| Digit 2 enable | `GPIO 2` |

The segment outputs are configured as active-low and the digit enable lines are active-high. Use one resistor in series with each segment line.

## Program 1 Basic Counter

`program1` is the simplest version.

- Counts from `00` to `99`.
- Increases the value once per second.
- Multiplexes the two display digits in the main loop with a fixed scan interval.

This version is useful as the baseline software-timed implementation.

## Program 2 Potentiometer-Controlled Scan Speed

`program2` adds analog input on `GPIO 14`.

Potentiometer wiring:

| Potentiometer pin | ESP32-S3 connection |
| --- | --- |
| One outside pin | `3V3` |
| Middle pin / wiper | `GPIO 14` |
| Other outside pin | `GND` |

- Counts from `00` to `99`.
- Increases the value once per second.
- Samples the potentiometer and maps it to the digit scan interval.
- Lets you observe how slower or faster multiplex timing changes the visible display behavior.

## Program 3 Hardware Timer Version

`program3` keeps the same display and potentiometer wiring as `program2`, but moves timing onto ESP32 hardware timers.

- Timer 0 queues counter updates once per second.
- Timer 1 queues display scan updates.
- Timer 2 queues potentiometer sampling.
- The main `loop()` consumes queued events and performs the GPIO and ADC work.

This version is more deterministic than the loop-timed examples because periodic tasks are scheduled by hardware timers instead of only by `millis()` and `micros()` checks.