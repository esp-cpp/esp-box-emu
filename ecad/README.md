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
