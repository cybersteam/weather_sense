# Architecture

WeatherSense is a single cooperative loop on an ATmega328P clocked at 16 MHz. The source that runs on the chip also runs, unchanged, as a host process. The host supplies the clock, the buses, and a scripted sensor, so the compensation math, the protocol, and the fault policy are tested without a board.

## Why the core stays out

The Arduino core hides the watchdog, the startup code, and the time base behind a framework this image does not need. Keeping the hardware access in `src/hal_avr.c` means a reviewer can read every register write, and the same call sites link against `src/hal_host.c` for the test suite. There is no heap. Strings that must survive in flash use `WS_PSTR`. Formatted output is integer-only, because a printf that can print floats does not fit the memory budget this project is willing to spend.

Link-time optimization is off. The reset-reason capture lives in a naked `.init3` function, and an LTO build is free to drop or reorder that section. The image is built `-Os` with function and data sections garbage-collected, and with `-mrelax`.

## Loop

`main` initializes the HAL and the station, then kicks the watchdog and calls `ws_station_poll` forever. One poll does five things, in order:

1. Drain the UART and, when a line is complete, run one command.
2. Sample the keypad every 10 ms.
3. Finish a BME280 conversion that is already in progress, then start a new one when the sample period has elapsed.
4. Mark the sample stale when no good reading has arrived in time.
5. Redraw the LCD. The panel driver writes only the cells that changed.

The sensor is requested and then polled inside the same call. On the real chip the conversion is still running, so the poll returns busy and a later call collects the result. In the host test the scripted device finishes inside the SPI write, so one poll both starts and completes a sample. Measurement timeout is based on `ws_millis`, which the tests advance by hand. Startup delays that the sensor requires (reset, NVM copy, LCD timing) go through `ws_delay_ms` and `ws_delay_us`. On the host those calls return immediately, so a mistake that waits on the clock instead of on `ws_millis` hangs the suite. The test process sets a ten-second alarm for that reason.

A missing or wrong chip id is a fault bit. The loop keeps serving the UART and the display. The original sketch stopped forever in that case.

## Faults

The station keeps one 16-bit word. Sensor faults clear on the next good sample. The watchdog, brown-out, and configuration faults stay set until something that is allowed to clear them does so.

| Bit | Name | Set when | Cleared when |
| --- | --- | --- | --- |
| 0 | `WS_FAULT_BME_ID` | The SPI id byte is not `0x60` | A later good sample |
| 1 | `WS_FAULT_BME_TIMEOUT` | NVM copy or a conversion exceeds its wait | A later good sample |
| 2 | `WS_FAULT_BME_RANGE` | Temperature outside −40..85 °C or pressure outside 300..1100 hPa | A later good sample |
| 3 | `WS_FAULT_BME_STALE` | No good sample for `max(3 × period, 3000 ms)` | A later good sample |
| 4 | `WS_FAULT_CONFIG` | The EEPROM image fails its check | A successful load or save of a valid image |
| 5 | `WS_FAULT_WDT_RESET` | This boot was a watchdog reset | Stays latched |
| 6 | `WS_FAULT_BROWNOUT` | This boot was a brown-out reset | Stays latched |

Stale is not raised while the id fault is set. A missing sensor is reported as missing, not as a stale reading of a sensor that was never there.

Reset cause comes from `MCUSR`, captured in `.init3` before the C runtime clears memory and before the watchdog can fire again during that startup. The priority is watchdog, then brown-out, then external reset, then power-on. The mirror byte is `.noinit`, so the BSS clear does not wipe it. The early routine is naked and contains no `ret`; it falls into the data-copy code. The watchdog is then enabled for a two-second timeout and kicked from the main loop, from `ws_delay_ms`, and once per byte of an EEPROM save.

## Configuration

Twelve bytes at EEPROM address 0:

| Offset | Contents |
| --- | --- |
| 0..1 | Magic `WS` |
| 2 | Version, currently 1 |
| 3 | Units: 0 metric, 1 imperial |
| 4..5 | Altitude, int16 little-endian, metres |
| 6..7 | Sample period, uint16 little-endian, milliseconds |
| 8 | Telemetry: 0 off, 1 on |
| 9 | Reserved, written as 0 |
| 10..11 | CRC-16/CCITT-FALSE over the first ten bytes |

Accepted ranges are altitude −400..4500 m and period 500..60000 ms. The factory period is 1000 ms and telemetry defaults on. A blank EEPROM (`0xFF` bytes) loads the defaults and is not a fault. A bad magic, version, CRC, or range sets `WS_FAULT_CONFIG`, keeps the EEPROM untouched, and runs on the RAM defaults. `SAVE` rewrites a valid image and clears the fault. Bytes that already match are skipped, so a repeated save does not wear the cell.

`SET` on the service port changes RAM only. `SAVE` commits it. The keypad has no separate save key: confirming an altitude edit writes the whole RAM image, including any service-port edits that were still staged. `DEFAULTS` dirties RAM and does not touch EEPROM until `SAVE`.

## Sensor

The driver follows the BME280 datasheet (BST-BME280-DS002), not a vendored Bosch library. SPI is mode 0 at 1 MHz. The id must be `0x60`; a BMP280 (`0x58`) is rejected. After the soft reset the driver waits for the NVM-copy bit to clear, reads both calibration blocks, and programs the weather-monitoring profile: oversampling ×1, IIR filter off, forced mode. Humidity control is written before the measurement control register, which is the order the chip requires for the humidity setting to latch.

Compensation is the published integer formula, including the 64-bit pressure path. Temperature is produced in 0.01 °C, pressure is rounded to pascals from the Q24.8 result, and humidity is rounded to 0.01 %RH from the Q22.10 result and clamped to 0..100 %. The host tests compare that integer path with a double-precision transcription of the same section of the datasheet. The double code lives only in the test file. Left shifts of values that can be negative are done on the unsigned bit pattern, because a signed left shift of a negative value is undefined in C and the datasheet expression depends on those bits. Right shifts stay arithmetic, which is what GCC and avr-gcc both do.

## Derived values

All of these are integer. The firmware does not link libm.

- Dew point is Magnus with a = 17.625 and b = 243.04, evaluated in Q16.16. The logarithm reduces its argument into [1, 2) and uses an atanh series.
- Heat index is the NOAA simple formula, and the Rothfusz regression only when that result is inside the published range. The constant term is −42.38 °F in hundredths (−4238), matching the published hundredths-of-a-degree form.
- Sea-level pressure is `P * exp(0.0341632 * h / T)` with `T` in kelvin, in Q16.16. Altitude zero returns station pressure unchanged.
- Tendency stores 16 samples of pressure in 0.1 hPa. A new point is recorded at most once per 60 s. Until the window is full the code is `9` (unknown). After that, a rise or fall of at least 0.5 hPa from the oldest sample to the newest is `1` or `-1`; anything smaller is `0`.

## Display

Four pages, sixteen columns, rotated every three seconds unless the operator is holding the page or editing the altitude. Page 0 is temperature, humidity, station pressure, and a tendency mark (`^`, `v`, `-`, or `?`). Page 1 is dew point and sea-level pressure. Page 2 is heat index and uptime. Page 3 is either `System OK` plus the reset reason, or `FAULT` plus the four-digit fault word and the highest-priority name. With no sample yet, page 0 reads `WeatherSense` / `waiting`, unless a fault is already latched, in which case the fault page is shown.

Imperial mode converts temperature to °F at 0.1°, pressure to inHg at 0.01 inHg (`(Pa × 1000 + 16932) / 33864`), and altitude to feet. The conversion is display-only.

## Service port

Command parsing and the telemetry line are pure functions in `src/protocol.c`. The station owns the UART buffer (47 characters, then the line is discarded through the next newline and answered `ERR syntax`), the staged configuration, and the decision to emit a periodic `WS` line. Periodic telemetry is suppressed until the first good sample and whenever telemetry is configured off. `READ` still returns the last sample while telemetry is off. Details are in [protocol.md](protocol.md).

## Tests

`tests/test_main.c` is one binary. It covers the CRC vector, formatting, the keypad thresholds, the derived-value anchors, EEPROM round-trips (including a negative altitude and a second save that must write zero bytes), the telemetry line, the LCD init sequence and its dirty-cell shadow, the BME280 calibration decode and compensation, and the station. The station cases include a sensor that does not answer (the UART still responds), a sample that survives reboot, a corrupt EEPROM image, telemetry being switched off, keypad hold, and a watchdog reset reason.

Sanitizers are address and undefined behavior, and undefined behavior does not recover: a bad shift fails the test. The scripted BME280 watches the host GPIO stamp so each chip-select edge starts a new SPI transaction, and it models the id, the NVM-copy bit, and a conversion that never finishes.

## Memory

The size gate reads `avr-size -A` and adds the sections itself. Berkeley format on Binutils 2.46 folds `.data` into the text column and prints a data size of zero, which would under-count SRAM. Flash is the program image plus the initializer for `.data`. SRAM is `.data` + `.bss` + `.noinit`.

On avr-gcc 16.1.0 that sum is 18836 bytes of flash and 798 bytes of SRAM. The ceilings checked after every firmware link are 30720 and 1536. UI text is ordinary C strings and accounts for most of the 368-byte `.data` section. Moving it to program memory is available if a later feature presses the SRAM ceiling; the service-port constants are already in program memory.
