# Real-time clock DS3231

Without WiFi, ESPuino only knows the time while it has power: the ESP32 keeps its clock running in
deep sleep, but loses it on a power cut or a flat battery, and drifts by seconds to minutes per day.
A DS3231 module keeps the time on its own coin cell and drifts by only about a minute per year.

## Wiring

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

## Behaviour

Nothing needs to be configured: the module is detected automatically at boot.

- **At boot** (deep-sleep wakeups included) the system clock is set from the DS3231, right after
  the peripherals are powered and before RFID and WiFi start. It takes precedence over the clock the
  ESP32 kept running itself, as it is far more accurate.
- **Whenever the clock is set**, by an NTP sync or by hand in the web interface, the new time is
  written to the DS3231.
- The DS3231 holds UTC; summer and winter time come from `timeZone` in `settings.h`.

The status is shown under **General → Time**: *found, time valid*, *time invalid* (the module's
oscillator stopped, i.e. the coin cell was empty or removed, or it was never set) or *not present*.
An invalid time is fixed by the next NTP sync or by setting the clock by hand.

If address `0x68` is taken by something else, define `RTC_DS3231_DISABLE` in `settings.h` (or
`settings-override.h`) to skip the detection.
