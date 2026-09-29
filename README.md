# WeatherSense

WeatherSense is a bare-metal environmental station for the ATmega328P on an Arduino Uno. It reads a Bosch BME280 over SPI, shows the result on a 16×2 keypad shield, and speaks a small text protocol at 9600 8N1 on the hardware UART. The microcontroller is the same part the original sketch targeted. The firmware in this tree is the whole program: no Arduino core, no dynamic allocation, and no busy-wait if the sensor is missing.

Temperature, humidity, and pressure come from the sensor. Dew point, heat index, sea-level pressure, and a short-term pressure tendency are computed on the chip in integer arithmetic. The LCD can show metric or imperial units. The service port always reports SI, so a logger does not have to track the display mode.

## Scope

The firmware drives the two devices the original wiring actually used.

| Function | Hardware |
| --- | --- |
| Temperature, humidity, pressure | BME280, SPI, chip-select D10 |
| Local display and buttons | DFRobot-style 16×2 keypad shield |
| Logging and configuration | UART on D0/D1 |

D2, D3, A1, A2, and A3 are left free. A later anemometer, rain gauge, wind vane, irradiance sensor, or module-temperature input can take them without moving the sensor or the display. Those inputs are not implemented.

A few derived values have a defined meaning, and that meaning is narrower than a meteorological report:

- Dew point uses the Magnus formula with over-water constants. Relative humidity of zero is reported as −80.00 °C, which is the floor of the function, not a measured dew point.
- Sea-level pressure is an isothermal reduction from the configured altitude. It is not the WMO lapse-rate formula.
- Pressure tendency compares the oldest and newest sample in a 16-minute window (one sample per minute) and moves only when the change reaches 0.5 hPa. It is not a three-hour pressure characteristic.
- Heat index follows the NOAA Steadman/Rothfusz rules. Below the range where the full regression applies, the display shows `n/a` and telemetry sends `hi=na`.

The BME280 die itself is specified for 1.71–3.6 V. The supported build uses a 5 V breakout that carries its own regulator and level shifters. See [docs/hardware.md](docs/hardware.md).

## Build

Host tests need CMake, a host C compiler, and Python 3. The firmware image needs avr-gcc, avr-libc, and binutils-avr. When those tools are not on `PATH`, point `AVR_PREFIX` at the toolchain root (the directory that contains `bin/avr-gcc`).

```sh
make test
make firmware
```

`make test` configures `build/host`, builds one test binary with address and undefined-behavior sanitizers, and runs it. A test that busy-waits on the clock is killed after ten seconds. `make firmware` configures `build/avr` with [cmake/avr-gcc.cmake](cmake/avr-gcc.cmake) and links `build/avr/weather_sense.hex`. The link fails if the image exceeds the budgets below.

An image built with avr-gcc 16.1.0 occupies **18836 bytes of flash** and **798 bytes of SRAM**. The gate rejects a build above 30720 bytes of flash (32 KiB part, 512-byte Optiboot, with margin) or 1536 bytes of SRAM (2 KiB part, with stack margin). Another compiler version can move the counts. The gate is the limit continuous integration enforces.

## Service port

9600 8N1, one command per line. A session looks like this:

```text
HELP
OK
VER READ STATUS CFG
SET UNIT M|I
SET ALT <m>
SET PERIOD <ms>
SET TELEM ON|OFF
SAVE LOAD DEFAULTS
PAGE HOLD RESET
```

`READ` returns one `WS` line in SI units. The field scales, a line the tests lock, and the fault bits are in [docs/protocol.md](docs/protocol.md).

## Layout

| Path | Role |
| --- | --- |
| [docs/architecture.md](docs/architecture.md) | Loop, faults, configuration, math, tests |
| [docs/hardware.md](docs/hardware.md) | Wiring, front panel, fuses, programming |
| [docs/protocol.md](docs/protocol.md) | Service-port commands and telemetry |
| [docs/bringup.md](docs/bringup.md) | Build, flash, and the first session |
| `include/ws/` | Headers shared by firmware and tests |
| `src/` | Firmware. `hal_avr.c` is the chip; `hal_host.c` is the test stand-in |
| `tests/` | Host suite, including a scripted BME280 |
| `scripts/size_gate.py` | Flash and SRAM check run after the AVR link |

Continuous integration on GitHub Actions runs the host suite and the firmware build on every push and pull request.
