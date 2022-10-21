# esp-box-emu
Emulator(s) running on ESP BOX

## Description

This project is a little retro game emulation system running on ESP32-S3-BOX.

## Images

![zelda](./images/zelda.jpeg)
![zelda dangerous](./images/zelda_its_dangerous.jpeg)
![zelda sword](./images/zelda_sword.jpeg)

## Planned Features

This project is designed to have:

 - [x] LVGL gui for selecting emulators / roms
 - [x] BT Gamepad input (see note below) 
 - [x] Loading of gui data (rom titles and boxart) from metadata file
 - [ ] Emulators to choose from:
   - [x] NES emulator
   - [ ] GB/GBC emulator
   - [ ] SNES emulator
   - [ ] SMS / Genesis emulator
 - [x] LittleFS file system for local storage of roms and metadata
 - [x] Memory mapping of selected rom data from littlefs partition into raw data
       partition
 - [ ] FTP Client for browsing remote FTP server of roms and displaying their
       data in LVGL
 - [ ] Interaction with touchscreen
 - [ ] Interaction with d-pad + buttons
 - [ ] Feedback through BLDC haptic motor (see
       https://github.com/scottbez1/smartknob)
 - [ ] Audio output

 NOTE: For gamepad input I'm currently using the associated
 [controller.py](./controller.py) script which will send the input reports from
 a BT gamepad (in this case an 8BitDo Pro 2) over UDP to the ESP-BOX-EMU which
 converts those input reports and sets the state of the input device for the NES
 emulator appropriately.
    
 Down the line I'd like to add the ability to load the emulator cores from the
 FTP server (which would be pre-compiled ESP32 libraries) so that they wouldn't
 take up as much space on the ESP32 itself. Right now that's not a huge concern
 though because the ESP (S3 module in the ESP BOX especially) has more than
 enough flash for the emulators so assuming their RAM can be managed it won't be
 much of an issue.
 
 Info for loading elf files at runtime:
 https://github.com/niicoooo/esp32-elfloader.
 [Here](https://github.com/joltwallet/jolt_wallet/tree/master/jolt_os/jelf_loader)
 is another implementation.

## TODO

- [ ] I2C configuration (using esp-idf-cxx) for touch driver
- [ ] LVGL touch driver
- [ ] ~Bluetooth HID support for gamepads~ Cannot do this on ESP32s3 since it
      doesn't support BT Classic and I'm not aware of any BLE gamepads (or at
      least I don't have any).
- [ ] Emulator selection UI

## References and Inspiration:

### Other NES Emulators
* https://github.com/nesemu/NESemu
* https://github.com/NiwakaDev/NIWAKA_NES
* https://github.com/kanathan/plainNES
* https://github.com/blagalucianflorin/lbnes
* https://github.com/daniel5151/ANESE
* https://github.com/Grandduchy/YaNES

### Useful Background / Information
* https://www.zophar.net/nes.html
* https://yizhang82.dev/nes-emu-overview
* https://www.gridbugs.org/zelda-screen-transitions-are-undefined-behaviour/
* https://bgb.bircd.org/pandocs.htm
