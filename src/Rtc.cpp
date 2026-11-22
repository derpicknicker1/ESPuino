#include <Arduino.h>
#include "settings.h"

#include "Rtc.h"

#include "Log.h"
#include "System.h"
#include "logmessages.h"

#include <Wire.h>
#include <sys/time.h>

uint8_t Rtc_BcdToBin(uint8_t bcd) {
	return (bcd >> 4) * 10 + (bcd & 0x0F);
}

uint8_t Rtc_BinToBcd(uint8_t bin) {
	return ((bin / 10) << 4) | (bin % 10);
}

time_t Rtc_UtcToEpoch(int year, int month, int day, int hour, int minute, int second) {
	// Days since 1970-01-01 for a proleptic Gregorian date (Howard Hinnant's days_from_civil). newlib
	// has no timegm(), and mktime() would apply the local time zone.
	year -= (month <= 2) ? 1 : 0;
	const int era = (year >= 0 ? year : year - 399) / 400;
	const int yoe = year - era * 400;
	const int doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
	const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	const int64_t days = static_cast<int64_t>(era) * 146097 + doe - 719468;
	return static_cast<time_t>(days * 86400 + hour * 3600 + minute * 60 + second);
}

bool Rtc_DecodeRegisters(const uint8_t regs[7], time_t &epoch) {
	const int second = Rtc_BcdToBin(regs[0] & 0x7F);
	const int minute = Rtc_BcdToBin(regs[1] & 0x7F);
	int hour;
	if (regs[2] & 0x40) { // 12-hour mode (only if something else set it that way)
		hour = Rtc_BcdToBin(regs[2] & 0x1F) % 12 + ((regs[2] & 0x20) ? 12 : 0);
	} else {
		hour = Rtc_BcdToBin(regs[2] & 0x3F);
	}
	const int day = Rtc_BcdToBin(regs[4] & 0x3F);
	const int month = Rtc_BcdToBin(regs[5] & 0x1F);
	const int year = 2000 + Rtc_BcdToBin(regs[6]); // century bit ignored: only matters after 2099
	if (second > 59 || minute > 59 || hour > 23 || day < 1 || day > 31 || month < 1 || month > 12) {
		return false;
	}
	epoch = Rtc_UtcToEpoch(year, month, day, hour, minute, second);
	return true;
}

void Rtc_EncodeRegisters(time_t epoch, uint8_t regs[7]) {
	struct tm utc;
	gmtime_r(&epoch, &utc);
	regs[0] = Rtc_BinToBcd(utc.tm_sec);
	regs[1] = Rtc_BinToBcd(utc.tm_min);
	regs[2] = Rtc_BinToBcd(utc.tm_hour); // bit 6 clear: 24-hour mode
	regs[3] = utc.tm_wday + 1; // 1..7, not used by us
	regs[4] = Rtc_BinToBcd(utc.tm_mday);
	regs[5] = Rtc_BinToBcd(utc.tm_mon + 1);
	regs[6] = Rtc_BinToBcd(utc.tm_year % 100);
}

#if defined(I2C_2_ENABLE) && !defined(RTC_DS3231_DISABLE)

extern TwoWire i2cBusTwo;

namespace {

constexpr uint8_t rtcAddress = 0x68;
constexpr uint8_t regTime = 0x00; // 7 registers: seconds .. year
constexpr uint8_t regControl = 0x0E;
constexpr uint8_t regStatus = 0x0F;
constexpr uint8_t controlEosc = 0x80; // set = oscillator stops on battery power
constexpr uint8_t statusOsf = 0x80; // set = oscillator stopped at some point, time invalid

RtcStatus status = RtcStatus::Missing;

bool readRegisters(uint8_t reg, uint8_t *buf, uint8_t len) {
	i2cBusTwo.beginTransmission(rtcAddress);
	i2cBusTwo.write(reg);
	if (i2cBusTwo.endTransmission(false) != 0) {
		i2cBusTwo.endTransmission(true); // release the bus, as Port.cpp does
		return false;
	}
	if (i2cBusTwo.requestFrom(rtcAddress, len) != len) {
		return false;
	}
	for (uint8_t i = 0; i < len; i++) {
		buf[i] = i2cBusTwo.read();
	}
	return true;
}

bool writeRegisters(uint8_t reg, const uint8_t *buf, uint8_t len) {
	i2cBusTwo.beginTransmission(rtcAddress);
	i2cBusTwo.write(reg);
	i2cBusTwo.write(buf, len);
	return i2cBusTwo.endTransmission() == 0;
}

void logLocalTime(uint8_t logLevel, const char *format, time_t epoch) {
	struct tm local;
	localtime_r(&epoch, &local);
	Log_Printf(logLevel, format, local.tm_mday, local.tm_mon + 1, local.tm_year + 1900, local.tm_hour, local.tm_min, local.tm_sec);
}

} // namespace

void Rtc_Init(void) {
	uint8_t statusReg;
	if (!readRegisters(regStatus, &statusReg, 1)) {
		status = RtcStatus::Missing;
		Log_Println(rtcNotFound, LOGLEVEL_DEBUG);
		return;
	}
	uint8_t regs[7];
	time_t epoch;
	if ((statusReg & statusOsf) || !readRegisters(regTime, regs, sizeof(regs)) || !Rtc_DecodeRegisters(regs, epoch) || epoch < System_MinValidEpoch) {
		status = RtcStatus::Lost;
		Log_Println(rtcTimeLost, LOGLEVEL_ERROR);
		return;
	}
	status = RtcStatus::Ok;
	const struct timeval tv = {epoch, 0};
	settimeofday(&tv, nullptr);
	logLocalTime(LOGLEVEL_NOTICE, rtcTimeLoaded, epoch);
}

RtcStatus Rtc_GetStatus(void) {
	return status;
}

const char *Rtc_GetStatusName(void) {
	switch (status) {
		case RtcStatus::Ok:
			return "ok";
		case RtcStatus::Lost:
			return "lost";
		default:
			return "missing";
	}
}

bool Rtc_SetFromSystemClock(void) {
	if (status == RtcStatus::Missing) {
		return false;
	}
	const time_t now = time(nullptr);
	if (now < System_MinValidEpoch) {
		return false;
	}
	uint8_t regs[7];
	Rtc_EncodeRegisters(now, regs);
	uint8_t control, statusReg;
	bool ok = writeRegisters(regTime, regs, sizeof(regs));
	// Keep the oscillator running on battery power, and mark the time as valid again.
	ok = ok && readRegisters(regControl, &control, 1) && readRegisters(regStatus, &statusReg, 1);
	if (ok && (control & controlEosc)) {
		control &= ~controlEosc;
		ok = writeRegisters(regControl, &control, 1);
	}
	if (ok) {
		statusReg &= ~statusOsf;
		ok = writeRegisters(regStatus, &statusReg, 1);
	}
	if (!ok) {
		Log_Println(rtcWriteFailed, LOGLEVEL_ERROR);
		return false;
	}
	status = RtcStatus::Ok;
	logLocalTime(LOGLEVEL_INFO, rtcTimeSaved, now); // after every NTP sync, so not at notice level
	return true;
}

#else

void Rtc_Init(void) {
}

RtcStatus Rtc_GetStatus(void) {
	return RtcStatus::Missing;
}

const char *Rtc_GetStatusName(void) {
	return "missing";
}

bool Rtc_SetFromSystemClock(void) {
	return false;
}

#endif
