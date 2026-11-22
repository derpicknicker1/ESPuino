# Advent calendar

One RFID tag plays a different chapter every day. The date and time decide which chapters are
open; earlier days can be listened to again, and each chapter remembers where it was left off.

## Quick start

1. Put the chapters into a folder on the SD card, named by day number (see [SD card layout](#sd-card-layout)).
2. In the web interface, open the file browser, select the folder and assign the tag with playmode
   **🎄 Advent calendar**.
3. Under **General → Playback → Advent calendar**, check the unlock hour and, if you like, switch on
   the announcements (see [Web interface](#web-interface)).
4. Make sure the box knows the date (see [Where the time comes from](#where-the-time-comes-from)).

## SD card layout

Assign the tag to a folder with playmode **🎄 Advent calendar** (19). Every entry of that folder whose
name starts with a day number is a chapter — either a single file or a folder holding several files.
Both forms can be mixed:

```
/Advent/
├── 01 - Der Wunschzettel.mp3
├── 02.mp3
├── 03 Im Schneesturm/
│   ├── 1.mp3
│   └── 2.mp3
├── ...
└── 24 Heiligabend.mp3
```

- Leading zeros are optional (`7.mp3` and `07.mp3` are both day 7); days 1 to 31 are recognized, so a
  calendar may run up to New Year's Eve. A number above 31 (e.g. `2024 Kalender`) is not a chapter.
- Entries without a leading number (cover pictures, an intro) and hidden files are ignored, as are
  playlist files (`.m3u`, `.pls` …) — a chapter has to be audio.
- The files of a chapter folder are sorted like any other playlist (General → Playlist → sort mode).
  Subfolders inside a chapter folder are not played.
- If two entries carry the same number, the one that sorts first is used and the log says so.
- Days may be missing (e.g. no chapter on Sundays); they are simply skipped.
- The calendar is read anew at every tap, so chapters can be added or replaced at any time.

## Web interface

### Assigning the tag

In the file browser, select the calendar **folder**: the playmode list then offers
**🎄 Advent calendar** (it only appears for folders). Hold the tag to the reader so its ID is filled
in, and save the assignment. The assignment stays as it is forever — the progress is stored
separately, see [Saved progress](#saved-progress).

### Settings: General → Playback → Advent calendar

| Setting | Meaning |
| --- | --- |
| **Time from which the day's chapter is unlocked** (hour 0–23, default 6) | In December, the day's chapter opens at this hour; before that only the past days are open. |
| **Play announcement "time unknown"** | When on, the file below is played in addition to the red blink if the tag is applied while the date is unknown. |
| ↳ **Announcement file** | Path on the SD card. Only editable while the checkbox is on. |
| **Play announcement "door still shut"** | When on, the file below is played in addition to the red blink if no chapter is open yet, or when skipping forward past today's chapter. |
| ↳ **Announcement file** | Path on the SD card. Only editable while the checkbox is on. |

The grey text in the file fields is just an example: the path has to be entered (or pasted) for real.
The settings are saved with the regular *Save* button and take effect at the next tap, no restart
needed. *Reset* restores the defaults (6 o'clock, both announcements off, no files).

## When chapters open

- **December:** chapters 1 up to today. Today's chapter opens at the unlock hour; before that, only
  the past days are open.
- **January to November:** everything is open. On December 1st the calendar starts over with day 1,
  and the saved positions of last season are discarded.

"No chapter open" therefore only happens on December 1st before the unlock hour — or if the SD card
has no chapter for any of the open days yet (e.g. the folder starts with `05` on December 3rd).

## Playback

- Applying the tag starts **today's chapter** (the newest open one) where it was left off. Once a
  chapter has been heard to its end, it starts from the beginning next time.
- The playlist holds all open chapters in order, so **Previous** walks back through the earlier days
  file by file, and playback continues through the following chapters up to today's, where it stops.
- **Next** past today's chapter blinks red and, if switched on, plays the "door still shut"
  announcement; playback then carries on where it was. From January to November nothing is shut any
  more, so there it only blinks red.
- Pause/resume, volume, seeking and the other buttons work as in any audiobook.
- The general RFID options apply as usual: with *Pause when RFID tag is removed* taking the tag off
  pauses, putting it back on resumes; with *Don't accept the same RFID twice* a second tap while it is
  playing is ignored.

## Saved progress

The position is kept **per chapter**, in the NVS namespace `advent` (one entry per tag, holding file
number and seconds for each of the 31 days plus the season year). The tag's assignment is never
rewritten.

It is saved at the same moments as an audiobook's:

| Moment | Condition |
| --- | --- |
| Pause (also by removing the tag with *Pause when RFID tag is removed*) | always |
| Previous, Next, moving on to the next file | always (start of the new file) |
| Periodic checkpoint | only with *Auto-save playback position for long audio books*, and only for files of 5 minutes or more |
| Shutdown | only with *Remember playback position at shutdown* |
| Applying another tag | only with *Remember playback position at RFID tag change* |

- A chapter whose last file played to its end is reset, so it starts from the beginning next time.
- Jumping into another chapter with Previous/Next starts that file from the beginning and overwrites
  that chapter's saved position; the chapter you left keeps its last saved position.
- If the files inside a chapter folder change, the saved file number may point to another file; if
  it is out of range, the chapter simply starts from the beginning.
- After a hard power cut, the last saved position is used — switch on the periodic checkpoint if
  that matters.

## Announcements

In each of these situations the LEDs blink red; the announcement comes on top if switched on:

| Situation | Announcement |
| --- | --- |
| Tag applied, no valid date/time | "Time unknown" |
| Tag applied, no chapter open yet | "Door still shut" |
| Next past today's chapter while something is still shut | "Door still shut" (then playback carries on) |

Ready-made German and English files are in [`announcements/`](../announcements/README.md)
(`advent-time-unknown_de.mp3`, `advent-locked_de.mp3`, …). Copy them to the SD card and enter their
path in the settings. Your own recordings work just as well; any short audio file will do.

## Log messages

Useful when something doesn't play as expected (English wording; the log follows the firmware
language):

| Message | Meaning |
| --- | --- |
| `Advent calendar: N chapters open, starting at day D, file F, position P s` | Playlist built, where it starts |
| `Advent calendar: date/time unknown …` | No valid time, see below |
| `Advent calendar: door N is still shut` | No chapter open yet |
| `Advent calendar: no chapters found in '…'` | No entry in the folder starts with a day number |
| `Advent calendar: chapter N exists twice, using …` | Two entries with the same number |
| `Advent calendar: chapter N holds no playable files: …` | Empty chapter folder |
| `Advent calendar: all chapters on the SD card are open already …` | Next at the end, nothing is shut any more |
| `Advent calendar: announcement 'door still shut' is switched off or has no file` | Next at the end, announcement not set up |
| `Advent calendar: announcement file not configured or not found: '…'` | Announcement switched on, but the path is empty or wrong |
| `Advent calendar: saved progress of day D: file F, position P s` | Position saved (info level) |
| `Advent calendar: chapter N finished` | Chapter reset after playing to its end (info level) |

## For developers

- Code: `src/Advent.cpp` / `src/Advent.h`; hooked into `AudioPlayer_SetPlaylist()` (playlist),
  `AudioPlayer_NvsRfidWriteWrapper()` (progress) and the Next handling in `src/AudioPlayer.cpp`.
- Settings in NVS (`settings` namespace): `advHour`, `advNoTimeSnd`, `advLockedSnd`,
  `advNoTimeFile`, `advLockedFile`.
- `/settings` section `advent`:
  `{ "unlockHour": 6, "soundTimeUnknown": false, "soundLocked": false, "fileTimeUnknown": "", "fileLocked": "" }`.
- The date logic (`Advent_HighestOpenDay()`, `Advent_SeasonYear()`, `Advent_ParseChapterDay()`) has
  no hardware access and can be tested on the host.

## Where the time comes from

By default the time is synced via NTP as soon as WiFi is connected. Under **General → Time** NTP can
be switched off and date and time set by hand instead; the device clock is also shown in the title
bar of the web interface.

The ESP32 keeps its clock running in deep sleep, so a box that only sleeps between uses knows the date
even without WiFi. After a power cut or a flat battery the time is gone until the next NTP sync or
until it is set again; until then the calendar blinks red (and says "time unknown" if switched on).
Shortly after switching on, before WiFi is up, the same happens — applying the tag again a few
seconds later works.

The ESP32's internal clock drifts by a few seconds to minutes per day in deep sleep, so for a box that
goes weeks without WiFi the unlock hour may shift a little. Both problems go away with a DS3231
real-time clock module, see [rtc-ds3231.md](rtc-ds3231.md).
