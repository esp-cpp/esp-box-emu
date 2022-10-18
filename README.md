# esp-box-emu
Emulator(s) running on ESP BOX

This project is a little retro game emulation system running on ESP32-S3-BOX.

This project is designed to have:

 - [ ] LVGL gui for selecting emulators / roms
 - [ ] Emulators to choose from:
   - [x] NES emulator
   - [ ] GB/GBC emulator
   - [ ] SMS / Genesis emulator
 - [ ] LittleFS file system for local storage of roms and metadata
 - [ ] FTP Client for browsing remote FTP server of roms and displaying their
       data in LVGL
 - [ ] Interaction with touchscreen
 - [ ] Interaction with d-pad + buttons
 - [ ] Feedback through BLDC haptic motor (see
       https://github.com/scottbez1/smartknob)
 - [ ] Audio output


## References and Inspiration:

### Other NES Emulators
* https://github.com/nesemu/NESemu
* https://github.com/NiwakaDev/NIWAKA_NES
* https://github.com/kanathan/plainNES
* https://github.com/blagalucianflorin/lbnes
* https://github.com/daniel5151/ANESE
* https://github.com/Grandduchy/YaNES

### Useful Background / Information
* https://yizhang82.dev/nes-emu-overview
* https://www.gridbugs.org/zelda-screen-transitions-are-undefined-behaviour/
* https://bgb.bircd.org/pandocs.htm
