#include <Arduino.h>
#include "settings.h"

#include "Advent.h"

#include "AudioPlayer.h"
#include "Log.h"
#include "MemX.h"
#include "SdCard.h"
#include "System.h"
#include "logmessages.h"
#include "strnatcmp.h"

#include <Preferences.h>
#include <ctype.h>

namespace {

constexpr const char prefsNamespace[] = "advent";
constexpr time_t minValidEpoch = 1704067200; // 2024-01-01: anything earlier is an unset clock
constexpr uint16_t progressVersion = 1;

struct DayProgress {
	uint16_t track; // file index inside the chapter
	uint16_t reserved;
	uint32_t pos; // seconds into that file
};

struct Progress {
	uint16_t seasonYear;
	uint16_t version;
	DayProgress day[Advent_MaxDay];
};

struct Chapter {
	uint8_t day;
	uint16_t start; // index of its first file in the playlist
	uint16_t count; // number of files
};

// Layout of the playlist built last. There is only ever one playlist playing, so one layout suffices.
Chapter chapters[Advent_MaxDay];
uint8_t chapterCount = 0;
bool lockedChaptersAhead = false;
uint16_t seasonYear = 0;

const Chapter *findChapter(uint16_t playlistTrack) {
	for (uint8_t i = 0; i < chapterCount; i++) {
		if (playlistTrack >= chapters[i].start && playlistTrack < chapters[i].start + chapters[i].count) {
			return &chapters[i];
		}
	}
	return nullptr;
}

void loadProgress(const char *rfidTag, Progress &progress) {
	memset(&progress, 0, sizeof(progress));
	Preferences prefs;
	if (prefs.begin(prefsNamespace, false)) {
		if (prefs.getBytesLength(rfidTag) == sizeof(progress)) {
			prefs.getBytes(rfidTag, &progress, sizeof(progress));
		}
		prefs.end();
	}
	if (progress.version != progressVersion || progress.seasonYear != seasonYear) {
		// Nothing stored yet, or left over from last year's calendar: start fresh.
		memset(&progress, 0, sizeof(progress));
		progress.version = progressVersion;
		progress.seasonYear = seasonYear;
	}
}

bool storeProgress(const char *rfidTag, const Progress &progress) {
	Preferences prefs;
	if (!prefs.begin(prefsNamespace, false)) {
		return false;
	}
	const bool ok = prefs.putBytes(rfidTag, &progress, sizeof(progress)) == sizeof(progress);
	prefs.end();
	return ok;
}

// Audio files only: a playlist file (.m3u etc.) can't be handed to the decoder as a chapter.
bool isPlayableFile(const char *path) {
	if (!fileValid(path)) {
		return false;
	}
	const char *ext = strrchr(path, '.');
	for (const char *playlistExt : {".m3u", ".m3u8", ".pls", ".asx"}) {
		if (strcasecmp(ext, playlistExt) == 0) {
			return false;
		}
	}
	return true;
}

char *duplicateString(const char *s) {
	const size_t len = strlen(s) + 1;
	char *copy = static_cast<char *>(x_malloc(len));
	if (copy) {
		memcpy(copy, s, len);
	}
	return copy;
}

// Appends the files of one chapter to the playlist. False only on out-of-memory.
bool appendChapter(Playlist *list, const String &path, bool isDir) {
	if (!isDir) {
		char *entry = duplicateString(path.c_str());
		if (!entry) {
			return false;
		}
		list->push_back(entry);
		return true;
	}
	std::optional<Playlist *> files = SdCard_ReturnPlaylist(path.c_str(), ALL_TRACKS_OF_DIR_SORTED, 0, false);
	if (!files) {
		return false;
	}
	Playlist *sub = files.value();
	AudioPlayer_SortPlaylist(sub);
	for (char *&entry : *sub) {
		if (isPlayableFile(entry)) {
			list->push_back(entry);
			entry = nullptr; // ownership moved to `list`
		}
	}
	freePlaylist(sub);
	return true;
}

} // namespace

uint8_t Advent_HighestOpenDay(const struct tm &now, uint8_t unlockHour) {
	if (now.tm_mon != 11) {
		return Advent_MaxDay; // January to November: the whole calendar is open
	}
	uint8_t day = now.tm_mday;
	if (now.tm_hour < unlockHour) {
		day--; // today's door opens later
	}
	return day;
}

uint16_t Advent_SeasonYear(const struct tm &now) {
	const uint16_t year = now.tm_year + 1900;
	return (now.tm_mon == 11) ? year : year - 1;
}

uint8_t Advent_ParseChapterDay(const char *path) {
	if (path == nullptr) {
		return 0;
	}
	const char *slash = strrchr(path, '/');
	const char *name = slash ? slash + 1 : path;
	if (!isdigit(static_cast<unsigned char>(name[0]))) {
		return 0;
	}
	const unsigned long day = strtoul(name, nullptr, 10);
	return (day >= 1 && day <= Advent_MaxDay) ? day : 0;
}

bool Advent_GetTime(struct tm &now) {
	// System clock: set by NTP, and kept running by the ESP32's RTC through deep sleep (but not through
	// a power cut or a flat battery). An external RTC module would be read here as a fallback.
	const time_t t = time(nullptr);
	if (t < minValidEpoch) {
		return false;
	}
	localtime_r(&t, &now);
	return true;
}

uint8_t Advent_GetUnlockHour(void) {
	const uint8_t hour = gPrefsSettings.getUChar("advHour", Advent_DefaultUnlockHour);
	return (hour <= 23) ? hour : Advent_DefaultUnlockHour;
}

bool Advent_IsAnnouncementEnabled(AdventAnnouncement which) {
	return gPrefsSettings.getBool((which == AdventAnnouncement::TimeUnknown) ? "advNoTimeSnd" : "advLockedSnd", false);
}

String Advent_GetAnnouncementFile(AdventAnnouncement which) {
	return gPrefsSettings.getString((which == AdventAnnouncement::TimeUnknown) ? "advNoTimeFile" : "advLockedFile", "");
}

AdventResult Advent_BuildPlaylist(const char *folder, const char *rfidTag, Playlist *&list, uint16_t &startTrack, uint32_t &startPos) {
	list = nullptr;
	startTrack = 0;
	startPos = 0;
	chapterCount = 0;
	lockedChaptersAhead = false;

	struct tm now;
	if (!Advent_GetTime(now)) {
		Log_Println(adventTimeUnknown, LOGLEVEL_NOTICE);
		return AdventResult::TimeUnknown;
	}
	const uint8_t openUntil = Advent_HighestOpenDay(now, Advent_GetUnlockHour());
	seasonYear = Advent_SeasonYear(now);

	// Collect the chapters: index = day number.
	String paths[Advent_MaxDay + 1];
	bool isDir[Advent_MaxDay + 1] = {false};
	File dir = gFSystem.open(folder);
	if (!dir || !dir.isDirectory()) {
		Log_Printf(LOGLEVEL_ERROR, dirOrFileDoesNotExist, folder);
		return AdventResult::Error;
	}
	bool anyChapter = false;
	while (true) {
		bool entryIsDir = false;
		const String name = gFSystem.nextFileName(dir, &entryIsDir);
		if (name.isEmpty()) {
			break;
		}
		const uint8_t day = Advent_ParseChapterDay(name.c_str());
		if (day == 0 || (!entryIsDir && !isPlayableFile(name.c_str()))) {
			continue;
		}
		anyChapter = true;
		// Two entries for the same day: take the one that sorts first, independent of the order the
		// file system happens to list them in.
		if (!paths[day].isEmpty()) {
			if (strnatcasecmp(paths[day].c_str(), name.c_str()) <= 0) {
				Log_Printf(LOGLEVEL_ERROR, adventDuplicateChapter, day, paths[day].c_str());
				continue;
			}
			Log_Printf(LOGLEVEL_ERROR, adventDuplicateChapter, day, name.c_str());
		}
		paths[day] = name;
		isDir[day] = entryIsDir;
	}
	dir.close();

	if (!anyChapter) {
		Log_Printf(LOGLEVEL_ERROR, adventNoChapters, folder);
		return AdventResult::Error;
	}

	list = allocatePlaylist();
	for (uint8_t day = 1; day <= openUntil && day <= Advent_MaxDay; day++) {
		if (paths[day].isEmpty()) {
			continue;
		}
		const size_t start = list->size();
		if (!appendChapter(list, paths[day], isDir[day])) {
			Log_Println(unableToAllocateMemForLinearPlaylist, LOGLEVEL_ERROR);
			freePlaylist(list);
			chapterCount = 0;
			return AdventResult::Error;
		}
		const size_t count = list->size() - start;
		if (count == 0) {
			Log_Printf(LOGLEVEL_ERROR, adventEmptyChapter, day, paths[day].c_str());
			continue;
		}
		chapters[chapterCount++] = {day, static_cast<uint16_t>(start), static_cast<uint16_t>(count)};
	}

	for (uint16_t day = openUntil + 1; day <= Advent_MaxDay; day++) {
		lockedChaptersAhead = lockedChaptersAhead || !paths[day].isEmpty();
	}

	if (chapterCount == 0) {
		freePlaylist(list);
		Log_Printf(LOGLEVEL_NOTICE, adventLocked, openUntil + 1);
		return AdventResult::Locked;
	}

	// Start with the newest open chapter, where it was left off.
	const Chapter &today = chapters[chapterCount - 1];
	Progress progress;
	loadProgress(rfidTag, progress);
	const DayProgress &saved = progress.day[today.day - 1];
	if (saved.track < today.count) {
		startTrack = today.start + saved.track;
		startPos = saved.pos;
	} else {
		startTrack = today.start;
	}
	Log_Printf(LOGLEVEL_NOTICE, adventPlaylistReady, chapterCount, today.day, startTrack - today.start + 1, startPos);
	return AdventResult::Ok;
}

bool Advent_HasLockedChapters(void) {
	return lockedChaptersAhead;
}

bool Advent_SaveProgress(const char *rfidTag, uint16_t playlistTrack, uint32_t pos) {
	const Chapter *chapter = findChapter(playlistTrack);
	if (chapter == nullptr) {
		return false;
	}
	Progress progress;
	loadProgress(rfidTag, progress);
	DayProgress &day = progress.day[chapter->day - 1];
	day.track = playlistTrack - chapter->start;
	day.pos = pos;
	Log_Printf(LOGLEVEL_INFO, adventProgressSaved, chapter->day, day.track + 1, day.pos);
	return storeProgress(rfidTag, progress);
}

void Advent_OnTrackFinished(const char *rfidTag, uint16_t playlistTrack) {
	const Chapter *chapter = findChapter(playlistTrack);
	if (chapter == nullptr || playlistTrack != chapter->start + chapter->count - 1) {
		return;
	}
	Progress progress;
	loadProgress(rfidTag, progress);
	progress.day[chapter->day - 1] = {0, 0, 0};
	Log_Printf(LOGLEVEL_INFO, adventChapterFinished, chapter->day);
	storeProgress(rfidTag, progress);
}
