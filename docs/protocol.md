# Service port

The service port is the hardware UART at 9600 8N1. Commands are one line terminated by LF. CR is ignored. Matching is case-insensitive. A line longer than 47 characters is discarded through the next newline and answered with `ERR syntax`. An empty line is ignored. Numbers are parsed by hand: trailing junk and overflow are `ERR syntax`, not a partial value.

Every successful reply starts with `OK`. Every rejection starts with `ERR`.

## Commands

| Command | Reply | Effect |
| --- | --- | --- |
| `HELP` or `?` | `OK` and the command list | None |
| `VER` | `OK WeatherSense 1.0.0 atmega328p reset=POR` | Reset name is `POR`, `EXT`, `BOR`, `WDT`, or `UNK` |
| `READ` | One telemetry line, or `ERR no-sample` | Last good sample. Works while periodic telemetry is off |
| `STATUS` | `OK up=<s> samples=<n> faults=<hhhh> dirty=<0\|1> drops=<n>` | Uptime in seconds, sample count, fault word, unsaved config, UART overflow drops |
| `CFG` | `OK unit=M\|I alt=<m> period=<ms> telem=<0\|1>` | RAM configuration, in metres and milliseconds |
| `SET UNIT M` or `SET UNIT I` | `OK` | Staged in RAM. Anything other than `M` or `I` is `ERR syntax` |
| `SET ALT <m>` | `OK` or `ERR range` | Metres, −400..4500, staged in RAM. A non-integer is `ERR syntax` |
| `SET PERIOD <ms>` | `OK` or `ERR range` | 500..60000, staged in RAM. A non-integer is `ERR syntax` |
| `SET TELEM ON` or `OFF` | `OK` | Staged in RAM. Anything other than `ON` or `OFF` is `ERR syntax` |
| `SAVE` | `OK saved` | Writes the RAM image. Unchanged EEPROM bytes are skipped |
| `LOAD` | `OK loaded` or `ERR crc` | Reloads EEPROM. A bad image sets the config fault and marks RAM dirty so a later `SAVE` can repair it |
| `DEFAULTS` | `OK defaults` | RAM only: metric, 0 m, 1000 ms, telemetry on |
| `PAGE` | `OK page=<n>` | Advances the display page and leaves altitude edit |
| `HOLD` | `OK hold=<0\|1>` | Toggles page hold |
| `RESET` | `OK reset` | Then resets the chip. On the host build this sets a flag instead |

`SET` does not write EEPROM. The keypad's altitude confirmation does: the shield has no separate save control, and that confirmation writes the whole RAM image, including service-port edits that have not been saved on their own.

## Telemetry

Periodic lines are emitted only after a good sample, and only while telemetry is enabled. They use the same layout as `READ`. Scales are SI regardless of the LCD units. `u` reports the display unit so a logger can still tell what the operator is looking at.

```text
WS t=<0.01 °C> h=<0.01 %> p=<Pa> sl=<Pa> td=<0.01 °C> hi=<0.01 °C|na> tr=<-1|0|1|9> f=<4 hex digits> u=<0|1>
```

| Field | Meaning |
| --- | --- |
| `t` | Temperature, hundredths of a degree Celsius. `2508` is 25.08 °C |
| `h` | Relative humidity, hundredths of a percent. `4439` is 44.39 %RH |
| `p` | Station pressure, pascals |
| `sl` | Sea-level pressure, pascals, from the current altitude |
| `td` | Dew point, hundredths of a degree Celsius |
| `hi` | Heat index, hundredths of a degree Celsius, or `na` when the NOAA regression does not apply |
| `tr` | Tendency: `-1` falling, `0` steady, `1` rising, `9` window not full |
| `f` | Fault word, four uppercase hex digits. See the table below |
| `u` | Display units: `0` metric, `1` imperial |

A locked example from the host suite, for the datasheet calibration vector at the factory altitude, is:

```text
WS t=2508 h=5000 p=101325 sl=101325 td=1384 hi=na tr=9 f=0010 u=0
```

`f=0010` in that vector is the stale bit, used so the formatter is checked with a non-zero fault word. A healthy first sample from the scripted sensor is `f=0000`.

A periodic line is sent only when a sample is accepted. Sensor fault bits are cleared before that line is formatted, so the periodic stream does not keep repeating a stale reading, and it does not carry the id, timeout, range, or stale bits. Those bits are on the LCD, in `STATUS`, and in `READ` for as long as they stay set. `READ` formats the fault word as it is at the moment of the command. Configuration, watchdog, and brown-out bits survive a good sample, so they do appear on periodic lines. Sea-level pressure is recomputed from the last sample and the altitude currently in effect, including an altitude edit that has not been saved yet.

## Fault word

| Mask | Name |
| --- | --- |
| `0001` | BME280 id was not `0x60` |
| `0002` | Conversion or NVM copy timed out |
| `0004` | Compensated temperature or pressure outside the operating range |
| `0008` | No good sample within the stale window |
| `0010` | EEPROM image failed its check |
| `0020` | This boot was a watchdog reset |
| `0040` | This boot was a brown-out reset |

Sensor bits clear when a good sample arrives. `0010` clears when a valid image is loaded or saved. `0020` and `0040` stay set for the life of the boot.

## EEPROM image

`SAVE` writes twelve bytes at address 0. The CRC is CRC-16/CCITT-FALSE (polynomial `0x1021`, init `0xFFFF`, no reflection) over the first ten bytes. The check value for the ASCII string `123456789` is `0x29B1`, and the host suite locks that vector.

A virgin part reads as `0xFF`. That is the factory state: defaults are loaded and no config fault is raised. Any other image that fails the magic, the version, the CRC, or the range check raises the config fault and leaves the EEPROM as it was.
