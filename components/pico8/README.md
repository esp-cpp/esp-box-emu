# PICO-8 Emulator Component

This component provides PICO-8 fantasy console emulation for the esp-box-emu project.

## Overview

PICO-8 is a fantasy console created by Lexaloffle Games that simulates the limitations and feel of 8-bit era game consoles. This component integrates a PICO-8 emulator based on the femto8 project, specifically adapted for embedded systems like the ESP32.

## Features

- **Native PICO-8 Support**: Runs authentic PICO-8 cartridges (.p8 files)
- **128x128 Display**: Native PICO-8 resolution with proper scaling options
- **16-Color Palette**: Full PICO-8 color palette support
- **4-Channel Audio**: PICO-8's unique sound synthesis
- **Save States**: Load and save game progress
- **Controller Support**: Maps gamepad controls to PICO-8 inputs
- **Memory Efficient**: Optimized for ESP32 memory constraints

## Technical Specifications

### Display
- **Resolution**: 128x128 pixels
- **Color Depth**: 4-bit (16 colors)
- **Scaling Modes**: Original, Fit, Fill
- **Memory Usage**: 16KB framebuffer

### Audio
- **Channels**: 4-channel synthesis
- **Sample Rate**: 22050 Hz
- **Format**: 16-bit stereo output

### Memory Requirements
- **Emulator Core**: ~50KB
- **Cartridge Data**: Up to 32KB
- **Video Buffer**: 16KB
- **Audio Buffers**: ~8KB
- **Total**: ~106KB additional memory usage

## Control Mapping

| ESP32 Box Control | PICO-8 Function | Description |
|------------------|-----------------|-------------|
| D-Pad | Arrow Keys | Movement/Navigation |
| A Button | Z Key | Primary action button |
| B Button | X Key | Secondary action button |
| Start | Enter | Pause/Menu |
| Select | Shift | Alternative action |

## File Format Support

- **Cartridge Files**: `.p8` - PICO-8 cartridge format
- **Save States**: `.p8_save` - Save state files

## API Reference

### Core Functions

```cpp
// Initialize the PICO-8 emulator
void init_pico8(const char* rom_filename, const uint8_t* rom_data, size_t rom_size);

// Clean up emulator resources
void deinit_pico8();

// Reset the emulator state
void reset_pico8();

// Run one frame of emulation
void run_pico8_frame();
```

### Video Functions

```cpp
// Get the current video framebuffer
std::span<uint8_t> get_pico8_video_buffer();

// Get screen dimensions
int get_pico8_screen_width();  // Returns 128
int get_pico8_screen_height(); // Returns 128
```

### Input Functions

```cpp
// Set button state (bitmask of PICO8_BTN_* constants)
void set_pico8_input(uint8_t buttons);

// Individual key events
void pico8_key_down(int key);
void pico8_key_up(int key);
```

### Audio Functions

```cpp
// Get audio samples for playback
void get_pico8_audio_buffer(int16_t* buffer, size_t frames);

// Enable/disable audio
void set_pico8_audio_enabled(bool enabled);
```

### Save State Functions

```cpp
// Load save state from file
bool load_pico8_state(const char* path);

// Save current state to file
bool save_pico8_state(const char* path);
```

## Button Constants

```cpp
#define PICO8_BTN_LEFT   0x01  // D-Pad Left
#define PICO8_BTN_RIGHT  0x02  // D-Pad Right
#define PICO8_BTN_UP     0x04  // D-Pad Up
#define PICO8_BTN_DOWN   0x08  // D-Pad Down
#define PICO8_BTN_Z      0x10  // Primary action (A button)
#define PICO8_BTN_X      0x20  // Secondary action (B button)
#define PICO8_BTN_ENTER  0x40  // Start button
#define PICO8_BTN_SHIFT  0x80  // Select button
```

## Integration

This component is designed to integrate seamlessly with the esp-box-emu cart system:

1. **Cart Class**: `Pico8Cart` extends the base `Cart` class
2. **File Detection**: Automatically detects `.p8` files
3. **Menu Integration**: Appears in the emulator selection menu
4. **Save System**: Uses the standard save state system

## Configuration

Enable PICO-8 support in your project configuration:

```kconfig
CONFIG_ENABLE_PICO8=y
```

## Dependencies

- **femto8**: PICO-8 emulator core (included as submodule)
- **box-emu**: Core emulation framework
- **statistics**: Performance monitoring
- **shared_memory**: Memory management

## Build Configuration

The component uses the following compile-time definitions:

- `FEMTO8_EMBEDDED`: Enables embedded system optimizations
- `FEMTO8_NO_FILESYSTEM`: Disables file system dependencies
- `FEMTO8_ESP32`: ESP32-specific optimizations
- `PICO8_EMULATOR`: Identifies PICO-8 emulator build

## Performance Notes

- **Frame Rate**: Targets 60 FPS on ESP32-S3
- **Memory Usage**: Efficient memory management for embedded systems
- **Audio Latency**: Low-latency audio synthesis
- **CPU Usage**: Optimized for single-core operation

## Limitations

1. **Cartridge Size**: Maximum 32KB per cartridge (PICO-8 standard)
2. **No Editor**: Only runs existing cartridges (no built-in editor)
3. **Limited Keyboard**: Basic keyboard input only
4. **No Network**: No network/multiplayer features

## Troubleshooting

### Common Issues

**Cartridge Won't Load**
- Ensure the file has `.p8` extension
- Check file is not corrupted
- Verify sufficient memory available

**No Audio**
- Check audio is enabled in settings
- Verify audio hardware initialization
- Check for memory allocation issues

**Poor Performance**
- Ensure ESP32-S3 is running at full speed
- Check for memory fragmentation
- Verify other emulators are not running

### Debug Output

Enable debug logging to troubleshoot issues:

```cpp
espp::Logger logger({.tag = "pico8", .level = espp::Logger::Verbosity::DEBUG});
```

## License

This component is licensed under the same terms as the esp-box-emu project. The femto8 emulator core is licensed under the MIT license.

## Contributing

When contributing to this component:

1. Follow the existing code style
2. Add appropriate logging
3. Update this documentation
4. Test with multiple PICO-8 games
5. Verify memory usage stays within limits

## References

- [PICO-8 Official Site](https://www.lexaloffle.com/pico-8.php)
- [femto8 Emulator](https://github.com/benbaker76/femto8)
- [PICO-8 Technical Specification](https://www.lexaloffle.com/dl/docs/pico-8_manual.html)
- [esp-box-emu Project](https://github.com/esp-cpp/esp-box-emu)