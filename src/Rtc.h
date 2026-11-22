#pragma once

#include <Arduino.h>
#include <time.h>

// External real-time clock DS3231 on the second I2C bus (i2cBusTwo, address 0x68). Detected
// automatically whenever that bus is enabled (I2C_2_ENABLE); define RTC_DS3231_DISABLE in settings.h
// to skip it, e.g. if another device uses 0x68.
//
// The DS3231 only feeds and backs up the system clock: it is read once at boot (deep-sleep wakeups
// included, as they reboot) and written whenever the system clock is set, by NTP or by hand. Everything
// that needs the time keeps reading the system clock (System_GetLocalTime()). The DS3231 holds UTC;
// summer/winter time comes from the time zone in settings.h.

enum class RtcStatus : uint8_t {
	Missing, // no DS3231 found (or disabled)
	Lost, // found, but its time is invalid: it stopped (flat backup battery) or was never set
	Ok, // found and holding a valid time
};

// Detects the DS3231 and, if it holds a valid time, sets the system clock from it. Call after the I2C
// bus and the peripheral power are up.
void Rtc_Init(void);
RtcStatus Rtc_GetStatus(void);
const char *Rtc_GetStatusName(void); // "ok", "lost" or "missing" (web interface)
// Writes the current system time into the DS3231. False if there is none or the write failed.
bool Rtc_SetFromSystemClock(void);

// --- Pure conversion helpers (no hardware access) ---

uint8_t Rtc_BcdToBin(uint8_t bcd);
uint8_t Rtc_BinToBcd(uint8_t bin);
// Seconds since 1970-01-01 00:00 UTC for a UTC calendar date (year >= 1970).
time_t Rtc_UtcToEpoch(int year, int month, int day, int hour, int minute, int second);
// Decodes the 7 time registers (0x00..0x06) into seconds since 1970 (UTC). False if out of range.
bool Rtc_DecodeRegisters(const uint8_t regs[7], time_t &epoch);
// Encodes seconds since 1970 (UTC) into the 7 time registers (24-hour mode).
void Rtc_EncodeRegisters(time_t epoch, uint8_t regs[7]);
