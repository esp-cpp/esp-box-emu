# ESP-BOX-EMU ecad

This folder contains the electronic design of the ESP-BOX-EMU.

The ESP-BOX-EMU contains a single circuit board with the following components:
- USB-C connector
- Conductive / Membrane switches for d-pad, abxy, and start/select buttons which mate with GBC membranes and buttons
- Tactile switches for Volume +/-
- RED LED for charging indication
- DRV2605L haptic driver
- JST-PH connector for the LiPo battery
- AW9523 I/O expander
- MCP73831 LiPo charger
- MAX17048 LiPo state-of-charge monitor (I2C)
- MicoSD card slot
- TPS61070 boost converter

These features are supported in two different versions of the electronics, targeting:
- ESP32-S3-Box
- ESP32-S3-Box-3

## Setup

``` sh
# one time steps on your machine
pipx install atopile
ato configure

# one time steps in this folder
ato install
```

## Build

``` sh
# build everything
ato build -t all

# build for ESP32-S3-BOX
ato build -b box-emu -t all

# build for ESP32-S3-BOX-3
ato build -b box-3-emu -t all
```

Note: if you make changes to the electronics design (.ato files), you'll likely
need to update the `box-emu-base` layout (or other affected layouts). After
updating the affected layouts, make sure you import them into their parent
modules. The tree is listed here:

* box-3-emu
  * box-3-connector
  * box-emu-base
    * gbc-dpad
    * gbc-start-select
    * gbc-a-b-x-y
      * gbc-a-b
* box-emu
  * box-connector
  * box-emu-base
    * gbc-dpad
    * gbc-start-select
    * gbc-a-b-x-y
      * gbc-a-b


## View

``` sh
ato view -b box-emu
# or
ato view -b box-3-emu 
```
