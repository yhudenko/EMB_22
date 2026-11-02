# ESP32-S3 ADC Serial Examples

Two independent Arduino examples use the existing ESP32-S3 R8/N16 PlatformIO
configuration in [platformio.ini](platformio.ini). Select an environment in the
PlatformIO environment switcher, then build and upload it. Only that program's
source folder is compiled. The default environment is `program1`.

| Environment | Source | ADC API |
| --- | --- | --- |
| `program1` | [src/program1/main.cpp](src/program1/main.cpp) | Arduino `analogRead()` |
| `program2` | [src/program2/main.cpp](src/program2/main.cpp) | ESP-IDF `adc1_get_raw()` |

## Wiring (both programs)

Use a potentiometer, for example 10 kOhm:

| Potentiometer connection | ESP32-S3 connection |
| --- | --- |
| One outside pin | 3V3 |
| Middle pin / wiper | GPIO 4 (ADC1 channel 3) |
| Other outside pin | GND |

Alternatively, connect an analog sensor output to GPIO 4 and share its ground.
Never apply 5 V or a voltage outside 0–3.3 V to the input. Do not leave the
input floating. Disconnect any old 7-segment wiring from GPIO 4; these examples
do not use the display.

**ESP32-S3 pin mapping:** ADC1 channels 0–9 correspond to GPIOs 1–10.
GPIO 4 is therefore `ADC1_CHANNEL_3`. GPIO 14 is on ADC2 and cannot be read
with `adc1_get_raw()`.

## Sampling frequency and serial output

- Set `kSampleRateHz` near the top of each program; the default is **10 Hz**
	(one ADC conversion and one output line every 100 ms).
- Open the PlatformIO Serial Monitor at **115200 baud** after uploading.
- Both examples print a startup message, then lines such as `raw:2048`.
- Both configure 12-bit resolution (raw values **0–4095**) and 11 dB attenuation.
	Raw counts are not calibrated millivolts. On ESP32-S3 this attenuation has an
	approximate measurable range up to 3.1 V, so readings can saturate before 3.3 V.
- `micros()` schedules reads without a sampling `delay()`. Unsigned subtraction
	handles timer rollover, and missed slots are skipped rather than replayed.
- This is software timing, not a precision hardware sampling clock. The integer
	microsecond interval is rounded down for frequencies that do not divide
	1,000,000 evenly. ADC execution, serial blocking, and other tasks add jitter;
	the configured rate is a target, not a guarantee. Start with 10–100 Hz.
	The examples accept 1–1000 Hz, but USB/serial throughput may limit actual output.

## Program 1: Arduino API

`analogReadResolution(12)` selects the result width and
`analogSetPinAttenuation()` configures GPIO 4's attenuation. Each scheduled
sample calls `analogRead(kAdcPin)` and prints the result.

## Program 2: ESP-IDF ADC1 API

`adc1_get_raw()` is available in the installed Arduino-ESP32 2.x / ESP-IDF 4.4
framework. This example calls `adc1_config_width()` and
`adc1_config_channel_atten()` during setup, checks configuration errors with
`ESP_ERROR_CHECK()`, then reads `adc1_get_raw(kAdcChannel)` at the same target
frequency as Program 1. No Arduino ADC functions are used in this program.

This is the legacy ESP-IDF ADC driver. Newer ESP-IDF versions deprecate it in
favor of the ADC oneshot driver; availability may change if the project's
PlatformIO platform/framework is upgraded.