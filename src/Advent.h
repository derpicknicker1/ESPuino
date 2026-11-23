#pragma once

#include <Arduino.h>

#include "Playlist.h"

#include <time.h>

// Advent calendar (playmode ADVENT_CALENDAR): one RFID tag, assigned to a folder, plays a different
// chapter every day. Chapters are the entries of that folder whose name starts with the day number --
// a single file ("01.mp3", "07 - Der Stern.mp3") or a folder ("01/", "12 Schneesturm/") holding one or
// more files. Which form a chapter has is decided per chapter, so both can be mixed freely.
//
// In December, the chapters 1 up to today are open, today's from the configured unlock hour on. The
// rest of the year (January to November) everything is open. The playlist holds all open chapters in
// order and starts at today's chapter, at the position where it was left; Previous/Next walk through
// the files of all open chapters. The play position is kept per chapter (NVS namespace "advent", one
// blob per tag) and is cleared when a new season starts on December 1st.

constexpr uint8_t Advent_MaxDay = 31;
constexpr uint8_t Advent_DefaultUnlockHour = 6;

enum class AdventResult : uint8_t {
	Ok, // playlist built
	TimeUnknown, // no valid date/time available (no NTP sync since power-on, no RTC)
	Locked, // the folder has chapters, but none of them is open yet
	Error, // folder missing, no chapters in it, or out of memory
};

enum class AdventAnnouncement : uint8_t {
	TimeUnknown,
	Locked,
};

// --- Pure date logic (no hardware access) ---

// Highest day whose chapter is open at `now`: 0 = none (December 1st before the unlock hour),
// Advent_MaxDay = all of them (outside December).
uint8_t Advent_HighestOpenDay(const struct tm &now, uint8_t unlockHour);
// Year of the December the current season started in. Progress saved in an older season is discarded.
uint16_t Advent_SeasonYear(const struct tm &now);
// Day number a folder entry stands for (1..31), taken from the leading digits of its name; 0 if the
// entry is no chapter (hidden, no leading number, or out of range).
uint8_t Advent_ParseChapterDay(const char *path);

// --- Runtime ---

// Current local date/time. False if none is known. This is the one place to add an external RTC
// (e.g. DS3231) later: read it here whenever the system clock has not been set.
bool Advent_GetTime(struct tm &now);

uint8_t Advent_GetUnlockHour(void);
// Whether the announcement for the given situation is switched on in the web interface. The LEDs
// blink red in either case; the announcement only comes on top.
bool Advent_IsAnnouncementEnabled(AdventAnnouncement which);
// Path of the file to play for the given situation (also when switched off); empty if none is set.
String Advent_GetAnnouncementFile(AdventAnnouncement which);

// Builds the playlist for the calendar in `folder`. On AdventResult::Ok, `list` holds the open
// chapters' files and startTrack/startPos tell where to begin (today's chapter, saved position).
// Remembers the chapter layout, so the progress functions below can map a track back to its day.
AdventResult Advent_BuildPlaylist(const char *folder, const char *rfidTag, Playlist *&list, uint16_t &startTrack, uint32_t &startPos);

// True if the calendar built last has chapters that are not open yet (Next past the newest open
// chapter then says "still locked" instead of just refusing).
bool Advent_HasLockedChapters(void);

// Saves the position inside the chapter the given playlist track belongs to.
bool Advent_SaveProgress(const char *rfidTag, uint16_t playlistTrack, uint32_t pos);
// Call when a track ran to its end: if it was the chapter's last file, that chapter starts from the
// beginning next time.
void Advent_OnTrackFinished(const char *rfidTag, uint16_t playlistTrack);
