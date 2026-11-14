# Emulator Console (web)

The page esp-box-emu and tab5-emu advertise as their WebUSB landing page
(`esp-cpp.github.io/esp-box-emu/`, `esp-cpp.github.io/tab5-emu/`): Chrome or
Edge offers it when the emulator is plugged in. It talks to the device's link
(the vendor interface, the same protocol as `tools/emulink`) and its console
(the CDC port, with Web Serial).

- **Dashboard**: what runs, frame rate, memory, firmware, uptime, last crash,
  and the SD card's USB drive mode.
- **Screen**: the game's (or the launcher's) screen, live, with an on-screen
  controller (or the keyboard), save states and quit.
- **Library**: the games by console with their boxart; play, filter (the
  launcher too), add a game (ROM + boxart + metadata.csv), remove one.
- **Files**: browse the SD card, download, upload (drag and drop), new
  folder, delete.
- **Settings**: volume, mute, brightness, video scaling, resume last save
  (haptics on the box), as in the launcher.
- **System**: chip, partitions, reset reason, the services on the link,
  restart (or into download mode), and the firmware update (the updater with
  the espp OTA console). The generic services open in their espp web apps.
- **Console**: the console log, filtered, saved.

Plain HTML / CSS / ES modules, no build step. `link.js` is the protocol
(stream_frame frames, emu.control, the espp system service), `app.js` the
page. The files are the same in both repositories (like `tools/emulink`).

Try it locally: `python3 -m http.server -d web 8000`, then open
http://localhost:8000 (WebUSB needs https or localhost). Only one program can
use the link at a time: close `emulink` first.

The pages workflow (`.github/workflows/pages.yml`) publishes this folder
on a push to main.
