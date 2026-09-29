# Hardware

The target is an ATmega328P at 16 MHz with the Arduino Uno pinout and the Uno bootloader (Optiboot, 512 bytes). Flash is 32 KiB and SRAM is 2 KiB. The firmware leaves the factory clock and bootloader fuses alone.

## Sensor voltage

The BME280 die is specified for 1.71–3.6 V and its SPI pins are not 5 V tolerant. Do not wire a bare BME280 to the Uno's 5 V rail or to D11–D13 directly.

The wiring this firmware supports is a breakout that already has a 3.3 V regulator and level shifters, sold as a 5 V-ready module. Power it from the Uno 5 V pin, ground it to Uno ground, and connect chip-select to D10. SCK, MOSI, and MISO use the Uno SPI header (D13, D11, D12). The SPI clock is 1 MHz, mode 0. The MISO pull-up is enabled so an unplugged sensor reads `0xFF` and fails the id check instead of looking like a live device.

## Pin map

| Signal | Uno pin | Port | Notes |
| --- | --- | --- | --- |
| LCD RS | D8 | PB0 | HD44780, 4-bit |
| LCD E | D9 | PB1 | Enable pulse is timed in software |
| LCD D4 | D4 | PD4 | |
| LCD D5 | D5 | PD5 | |
| LCD D6 | D6 | PD6 | |
| LCD D7 | D7 | PD7 | |
| BME280 CS | D10 | PB2 | Active low |
| MOSI | D11 | PB3 | SPI peripheral |
| MISO | D12 | PB4 | Pull-up on |
| SCK | D13 | PB5 | 1 MHz |
| Keypad | A0 | PC0 | Analog ladder, ADC0 |
| Service RX | D0 | PD0 | 9600 8N1 |
| Service TX | D1 | PD1 | 9600 8N1 |

The LCD is the usual DFRobot keypad shield: RS on D8, E on D9, data on D4–D7, R/W tied to ground. The driver is write-only. It waits 2 ms after clear and home, and 50 µs after other instructions, because it cannot read the busy flag. The shield's buttons share A0.

Pins left free on purpose:

| Pin | Reserved for |
| --- | --- |
| D2 | Anemometer, INT0 |
| D3 | Rain gauge, INT1 |
| A1 | Wind vane |
| A2 | Irradiance |
| A3 | Module temperature |

Nothing in the firmware claims those inputs. Assigning them here only keeps a later sensor off the SPI bus and off the LCD bus.

## Front panel

The keypad ladder, read against a 5 V reference, decodes as:

| ADC counts | Key |
| --- | --- |
| 0–49 | Right |
| 50–249 | Up |
| 250–449 | Down |
| 450–649 | Left |
| 650–849 | Select |
| 850–1023 | None |

A key must hold the same code for 30 ms and is then delivered once. There is no auto-repeat. Right and Left change page. Select holds the current page so it stops rotating. Up and Down enter altitude edit and step by one metre; while editing, Left and Right step by ten. Select in the editor saves and leaves edit mode.

Without the shield, A0 floats and will chatter. Tie A0 to 5 V through 10 kΩ so an open input reads as None. Leave the internal pull-up off; it sits in parallel with the ladder and shifts the thresholds.

The display has four pages and rotates every three seconds unless held or editing. The frames are fixed at 16 columns. Page 0 is temperature and humidity over pressure and a tendency mark. Page 1 is dew point and sea-level pressure. Page 2 is heat index (or `n/a`) and uptime. Page 3 is the health line: `System OK` and the reset reason, or `FAULT`, the fault word, and a short name.

## Clock, watchdog, brown-out

Timer 2 runs in CTC with a prescaler of 128 and `OCR2A` of 124, which is a 1.000 ms tick at 16 MHz. `millis` is read with interrupts briefly off. The UART is 9600 8N1 (`UBRR` 103, about 0.2 % error) with a 128-byte transmit ring and a 64-byte receive ring. The ADC uses AVCC, channel 0, prescaler 128, and discards the first conversion after it is enabled.

The watchdog is set to two seconds at the end of HAL init. Early startup, before `.data` and `.bss` are initialized, copies `MCUSR` to a `.noinit` byte, clears `MCUSR`, and disables the watchdog. That ordering is what makes a watchdog reset distinguishable from a power-on reset, and it also stops a watchdog that is still armed from the previous boot from firing while the runtime is copying data.

## Fuses

Ship the Uno defaults:

| Fuse | Value | What it selects |
| --- | --- | --- |
| Low | `0xFF` | External crystal, CKDIV8 off |
| High | `0xDE` | SPI programming enabled, boot size 512 bytes, boot reset vector, EEPROM not preserved across chip erase |
| Extended | `0xFD` | Brown-out detector at 2.7 V |

A bootloader upload does not chip-erase. The avrdude line below passes `-D` for that reason, and EEPROM survives it. Preserving EEPROM across an ISP chip erase is a separate choice: program the EESAVE bit so the high fuse changes from `0xDE` to `0xD6` and nothing else. Read the high fuse first and compare it with `0xDE`. A wrong high or extended fuse can disable SPI programming or move the reset vector off the bootloader, and recovering that needs a high-voltage programmer. This repository intentionally has no fuse-write command.

```sh
avrdude -v -patmega328p -carduino -P/dev/ttyACM0 -b115200 -U hfuse:r:-:h
```

## Programming

Close any serial terminal on the port first. The bootloader and the service port are the same UART.

```sh
avrdude -v -patmega328p -carduino -P/dev/ttyACM0 -b115200 -D \
  -Uflash:w:build/avr/weather_sense.hex:i
```

The hex file is the one the firmware build writes at `build/avr/weather_sense.hex`. Adjust the port to the board that actually enumerated.
