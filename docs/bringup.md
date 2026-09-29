# Bring-up

## Tools

Host tests need CMake 3.16 or newer, a C compiler, and Python 3. The firmware needs avr-gcc, avr-libc, binutils-avr, and avrdude to flash. Ubuntu's packages are `gcc-avr`, `avr-libc`, and `binutils-avr`. A newer toolchain that is not on `PATH` is selected by setting `AVR_PREFIX` to the directory that contains `bin/avr-gcc`:

```sh
export AVR_PREFIX=$HOME/.local/avr
make firmware
```

Continuous integration installs the Ubuntu packages and builds with them. The source stays inside what avr-gcc 5 and avr-gcc 16 both accept: C11, no link-time optimization, no binary literals.

## Tests, then the image

```sh
make test
make firmware
```

`make test` prints `passed=` and `failed=` from the suite and a ctest summary. The binary is `build/host/ws_tests`. Sanitizer output on a failure is the stack of the bad shift or the bad access; the suite is built so an undefined shift aborts rather than printing and continuing.

`make firmware` writes three artifacts under `build/avr/`:

| File | Contents |
| --- | --- |
| `weather_sense` | ELF, with a link map beside it |
| `weather_sense.hex` | Intel hex, EEPROM section removed |
| `weather_sense.map` | Linker map |

The last lines of a successful firmware build are the size gate, in the form:

```text
flash_bytes 18836 (limit 30720)
sram_bytes 798 (limit 1536)
```

Those two counts are from avr-gcc 16.1.0. The limits are what fail the build. Berkeley `avr-size` on Binutils 2.46 prints a data column of zero for this image; ignore that column and use the gate.

## Flash

Unplug anything else on the UART, including a serial monitor. Optiboot and the service port share D0/D1.

```sh
avrdude -v -patmega328p -carduino -P/dev/ttyACM0 -b115200 -D \
  -Uflash:w:build/avr/weather_sense.hex:i
```

`-D` skips the chip erase. Optiboot has no full-chip erase, and skipping it leaves the EEPROM image in place across a firmware update. Change the port to match the board.

Leave the fuses at the Uno defaults (`LFUSE 0xFF`, `HFUSE 0xDE`, `EFUSE 0xFD`) unless you have a reason to program EESAVE for ISP. That procedure, and the reason this tree does not ship a fuse-write command, is in [hardware.md](hardware.md).

## First session

Open the port at 9600 8N1. The station announces itself as soon as it starts:

```text
WeatherSense 1.0.0
reset=POR
```

The first sample is requested on the first pass of the loop and published when the conversion finishes. At ×1 oversampling that is inside the 30 ms timeout. Later samples follow the configured period, which is 1000 ms until you change it. A healthy board with a 5 V-ready BME280 breakout and a blank EEPROM answers in this shape:

```text
VER
OK WeatherSense 1.0.0 atmega328p reset=POR
STATUS
OK up=2 samples=1 faults=0000 dirty=0 drops=0
CFG
OK unit=M alt=0 period=1000 telem=1
```

The LCD shows temperature and humidity on the first line and pressure on the second, and it steps through the other pages every three seconds. Select holds the page. Up enters the altitude editor; Select there writes EEPROM.

To put the station 120 m above sea level and keep it:

```text
SET ALT 120
OK
SAVE
OK saved
```

`STATUS` then reports `dirty=0`. Power-cycle the board and `CFG` still shows `alt=120`. `RESET` answers `OK reset`, arms the watchdog for 15 ms, and waits. The next banner reports `reset=WDT`.

## When the sensor is not there

Pull the breakout, or power the board with nothing on SPI. The banner still appears, `VER` still answers, and the fault word has bit 0 set (`faults=0001`). The LCD shows `FAULT` / `BME missing` instead of waiting forever. That is the regression the original sketch failed: a missing id used to sit in an infinite loop with the watchdog unfed.

A sensor that answers the id and then never completes a conversion sets bit 1 after 30 ms and bit 3 once the stale window expires. `STATUS` and `READ` still answer. `READ` returns the last good numbers with those bits set, when a sample exists; the periodic stream stops until a conversion succeeds again.

## Configuration faults

Erase the EEPROM, or leave a new chip untouched, and the boot is clean: defaults, `dirty=0`, `faults=0000`. Change one byte of a saved image without updating the CRC and the boot reports `faults=0010`, runs on the defaults, and sets `dirty=1`. `SAVE` writes a coherent image and clears the bit. `LOAD` on a bad image answers `ERR crc` and does the same.

`SET PERIOD 100` answers `ERR range` and changes nothing. The shortest accepted period is 500 ms.
