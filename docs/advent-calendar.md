# Advent calendar

One RFID tag plays a different chapter every day. The date and time decide which chapters are
open; earlier days can be listened to again.

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
  calendar may run up to New Year's Eve.
- Entries without a leading number (cover pictures, an intro) and hidden files are ignored.
- The files of a chapter folder are sorted like any other playlist (General → Playlist → sort mode).
  Subfolders inside a chapter folder are not played.
- If two entries carry the same number, the one that sorts first is used and the log says so.

## When chapters open

- **December:** chapters 1 up to today. Today's chapter opens at the configured hour (General →
  Playback → Advent calendar, default 6 o'clock); before that, only the past days are open.
- **January to November:** everything is open. On December 1st the calendar starts over with day 1,
  and the saved positions of last season are discarded.

## Playback

- Applying the tag starts today's chapter where it was left off. Once a chapter has been heard to its
  end, it starts from the beginning next time.
- The playlist holds all open chapters in order, so **Previous** walks back through the earlier days
  file by file, and playback continues through the following chapters up to today's.
- **Next** past today's chapter blinks red and, if switched on, plays the "door still shut"
  announcement; playback then carries on where it was.
- The position is saved per chapter (pause, track change, shutdown and the periodic checkpoint, just
  like an audiobook) in the NVS namespace `advent`. The tag's assignment itself is never rewritten.

## Announcements

In both of these situations the LEDs blink red:

| Situation | Announcement |
| --- | --- |
| No valid date/time | "Time unknown" |
| No chapter open yet (e.g. December 1st before the unlock hour), or Next past today | "Door still shut" |

On top of that, each announcement can be switched on in the web interface (General → Playback →
Advent calendar), the same way as the low-battery warning, together with the file to play.
Ready-made files are in [`announcements/`](../announcements/README.md).

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
goes weeks without WiFi the unlock hour may shift a little. An external RTC module (e.g. DS3231) can
be added later in `System_GetLocalTime()` (`src/System.cpp`) without touching anything else.
