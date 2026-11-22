# Date, time and real-time clock (DS3231)

ESPuino keeps one system clock. It can be set from three sources, and everything that needs the
date or time reads only that clock (`System_GetLocalTime()` in `src/System.cpp`):

| Source | When | Notes |
| --- | --- | --- |
| DS3231 real-time clock | once at boot (deep-sleep wakeups included) | optional module, keeps the time without power |
| NTP | after every WiFi connect, then every 3 hours | can be switched off |
| By hand | in the web interface | only while NTP is off |

A clock counts as set from 1 January 2024 on; anything earlier means "unset" (after a power cut the
ESP32 starts at 1970). The time zone is `timeZone` in `settings.h` (Europe/Berlin by default), so
summer and winter time switch automatically.

Without any of these sources the time is unknown until the next one kicks in. The ESP32 keeps its
own clock running through deep sleep, but loses it on a power cut or a flat battery and drifts by
seconds to minutes per day. That is what the DS3231 is for.

## Web interface

### Clock in the title bar

The device's clock is shown in the title bar, left of the connection indicator: date and time on
wider screens, only the time on phones. The page asks the device once a minute and ticks on by
itself in between. It always shows the **device's** time, not the browser's, even if the browser is
in another time zone. `--:--:--` means the clock is not set.

### General → Time

| Item | What it does |
| --- | --- |
| **Current device time** | Same clock as in the title bar. |
| **Real-time clock (DS3231)** | *found, time valid*, *time invalid – please set it again* (red) or *not present*; see [below](#real-time-clock-ds3231). |
| **Sync automatically via NTP** | On by default. Saved with the regular *Save* button and applied right away: switching it off stops NTP immediately, switching it on starts NTP immediately if WiFi is connected. |
| **Date and time** | Only editable while NTP is off. Prefilled with the current device time. |
| **Use this device's time** | Fills in the time of the phone or computer the page is open on. |
| **Set time** | Sets the clock immediately, no *Save* needed. It switches NTP off along with it, so unticking the checkbox doesn't have to be saved first. The DS3231 is updated as well. |

A hand-set time survives deep sleep. Without a DS3231 it is lost on a power cut and has to be set
again.

## NTP

- Servers: `de.pool.ntp.org`, `0.pool.ntp.org`, `ptbtime1.ptb.de`.
- Started after every successful WiFi connect, including reconnects after a dropout. The first
  request goes out within 5 seconds, then the time is synced again every 3 hours
  (`CONFIG_LWIP_SNTP_UPDATE_DELAY`) for as long as NTP runs.
- The received time is applied at once (no slow adjustment), logged as
  "Received date/time from NTP server", and written to the DS3231 if there is one.
- Switched off (NVS key `ntpEnable`), no NTP request is made at all and the log says
  "NTP: automatic time sync is switched off".

## Real-time clock DS3231

A DS3231 module keeps the time on its own coin cell and drifts by only about a minute per year.

### Wiring

Connect the module to the second I2C bus, the one the port expander uses (`ext_IIC_DATA` /
`ext_IIC_CLK` in the board's settings file; GPIO 13 and 4 on the "complete" board):

| Module | ESPuino |
| --- | --- |
| VCC | 3.3 V |
| GND | GND |
| SDA | `ext_IIC_DATA` |
| SCL | `ext_IIC_CLK` |

The DS3231 answers on address `0x68`, which doesn't clash with the PCA9555 port expander
(`0x20`–`0x27`) or an MFRC522 on I2C (`0x28`). Small modules usually bring their own pull-up
resistors; together with the ones already on the bus that is fine.

Modules with a charging circuit for the coin cell (e.g. the larger ZS-042) charge a plain CR2032,
which isn't meant to be charged: either remove the charging diode/resistor or use an LIR2032.

### Behaviour

Nothing needs to be configured: the module is detected automatically at boot.

- **At boot** (deep-sleep wakeups included) the system clock is set from the DS3231, right after
  the peripherals are powered and before RFID and WiFi start. It takes precedence over the clock the
  ESP32 kept running itself, as it is far more accurate.
- **Whenever the clock is set**, by an NTP sync or by hand in the web interface, the new time is
  written to the DS3231.
- The DS3231 holds UTC; summer and winter time come from `timeZone` in `settings.h`.

The status is shown under **General → Time**: *found, time valid*, *time invalid* (the module's
oscillator stopped, i.e. the coin cell was empty or removed, or it was never set) or *not present*.
An invalid time is fixed by the next NTP sync or by setting the clock by hand. The status is read
once at boot, so changes while ESPuino is running only show after the next start.

Log lines: "DS3231: date/time loaded" at boot, "DS3231: time invalid" if it stopped, and
"DS3231: date/time saved" (info level) whenever it is updated.

### Detecting a missing or flat coin cell

The DS3231 has no way to report its battery voltage. All it offers is the *oscillator stop flag*,
which it sets once its oscillator actually stopped. As long as the module is powered from ESPuino
(VCC), the oscillator keeps running without a coin cell, so a missing or flat cell only shows after
ESPuino was **without power** and the clock stopped as a result; the status then says *time invalid*
at the next start.

To test it: remove the coin cell, cut ESPuino's power completely (USB and battery; deep sleep is
only enough if the module is fed from the switched peripheral supply) and switch it on again.
Warning about a weak cell before the time is lost would need its voltage measured separately, e.g.
through a voltage divider on an ADC pin.

### Disabling the detection

If address `0x68` is taken by something else, define `RTC_DS3231_DISABLE` in `settings.h` (or
`settings-override.h`) to skip the detection.

## REST API

`GET /time` returns the device clock:

```json
{ "valid": true, "ntpEnabled": false, "rtc": "ok", "local": "2026-12-01T06:00:00" }
```

- `local` is the local time (time zone from `settings.h`) and only present while `valid` is true.
- `rtc` is `ok`, `lost` or `missing` (see above).

`POST /time` sets the clock by hand, only while NTP is off:

```json
{ "local": "2026-12-01T06:00:00", "ntpEnabled": false }
```

- `local` is `YYYY-MM-DDTHH:MM` with optional `:SS`.
- `"ntpEnabled": false` switches NTP off in the same request.
- Answers `200` with the same body as `GET /time`, `400` for an invalid date/time, `409` if NTP is
  still on.

The NTP switch is also part of `/settings` as `"time": { "ntpEnabled": true }` (section `time`).

## What uses the time

- **Spoken time** (command "tell current time"), which needs internet for the speech output anyway.
- **"First start"** date in the info dialog, stored at the very first NTP sync.
- **File timestamps** on the SD card for files written by ESPuino (web/FTP uploads, MediaHub
  downloads), and the dates the FTP server lists.
- The boot log line with the current date/time.

Log timestamps, sleep timer, inactivity shutdown and night mode use the uptime instead and don't
depend on the clock. HTTPS connections don't check certificate dates, so they work without a valid
time.
