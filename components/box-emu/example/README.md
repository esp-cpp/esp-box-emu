# Box-Emu Example

This example shows how to use the `BoxEmu` component to initialize the hardware
for the BoxEmu, automatically detecting the version of the BoxEmu hardware and
initializing the appropriate hardware (sdcard, memory, gamepad, battery, video).
It uses the `espp::EspBox` component to similarly automatically detect the
version of the EspBox it is running on and intialize its hardware (display,
touch, audio) appropriately.

## How to use example

### Hardware Required

This example is designed to run on the ESP32-S3-BOX or ESP32-S3-BOX-3 in
conjunction with a BoxEmu V0 or BoxEmu V1 gamepad.

Note: Only BoxEmu V1 supports ESP32-S3-BOX-3 (referred to as EspBox-3 here).

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view
serial output:

```
idf.py -p PORT flash monitor
```

(Replace PORT with the name of the serial port to use.)

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

## Example Output

EspBox-3 BoxEmu V1:
![CleanShot 2024-07-03 at 08 07 46](https://github.com/esp-cpp/esp-box-emu/assets/213467/4bfecb69-683c-4cb5-9519-d9f731dd4b0d)

EspBox BoxEmu V1:
![CleanShot 2024-07-03 at 08 10 53](https://github.com/esp-cpp/esp-box-emu/assets/213467/0d089e01-fb73-4f08-a7c7-a6c0e5d1533a)

EspBox BoxEmu V0:
![CleanShot 2024-07-03 at 08 23 05](https://github.com/esp-cpp/esp-box-emu/assets/213467/38f277db-9a99-41e6-b047-adc51c299f30)
