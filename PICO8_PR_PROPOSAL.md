# PICO-8 Emulator Support for esp-box-emu

## Pull Request Proposal

### Overview
This PR proposes adding PICO-8 (fantasy console) emulator support to the esp-box-emu project, following the established pattern of existing emulators (NES, Game Boy, Genesis, etc.).

### Background
PICO-8 is a fantasy console created by Lexaloffle Games that simulates the limitations of 8-bit era game consoles. It features:
- 128x128 pixel display with 16-color palette
- 4-channel sound synthesis
- Lua-based programming environment
- Small cartridge size (32KB limit)
- Built-in sprite, map, and music editors (not needed for emulation)

### Implementation Plan

#### 1. PICO-8 Emulator Selection
After researching available C/C++ PICO-8 emulators suitable for embedded systems, I recommend using **femto8** as the base:

- **Repository**: https://github.com/benbaker76/femto8
- **License**: MIT (compatible with esp-box-emu)
- **Language**: Pure C (optimal for ESP32)
- **Design**: Specifically built for embedded systems with memory constraints
- **Features**: Compact, resource-efficient, based on retro8 but optimized for embedded use
- **Status**: Active development, good PICO-8 compatibility

#### 2. Component Structure
Following the existing pattern, create a new component:

```
components/pico8/
├── CMakeLists.txt
├── include/
│   └── pico8.hpp
├── src/
│   └── pico8.cpp
└── femto8/                    # Git submodule
    ├── src/
    │   ├── pico8_api.c
    │   ├── pico8_cart.c
    │   ├── pico8_gfx.c
    │   ├── pico8_sound.c
    │   └── ...
    └── include/
        └── pico8.h
```

#### 3. Integration Points

##### 3.1 Cart System Integration
Create `main/pico8_cart.hpp` following the existing pattern:

```cpp
class Pico8Cart : public Cart {
public:
  explicit Pico8Cart(const Cart::Config& config);
  virtual ~Pico8Cart() override;
  
  virtual void reset() override;
  virtual void load() override;
  virtual void save() override;
  virtual bool run() override;

protected:
  static constexpr size_t PICO8_WIDTH = 128;
  static constexpr size_t PICO8_HEIGHT = 128;
  
  virtual void set_original_video_setting() override;
  virtual std::pair<size_t, size_t> get_video_size() const override;
  virtual std::span<uint8_t> get_video_buffer() const override;
  virtual void set_fit_video_setting() override;
  virtual void set_fill_video_setting() override;
  virtual std::string get_save_extension() const override;
};
```

##### 3.2 File Format Support
- Add `.p8` file extension support for PICO-8 cartridges
- Implement cartridge loading from SD card
- Support for save states (following existing pattern)

##### 3.3 Video Integration
- PICO-8 native resolution: 128x128 pixels
- 16-color palette (easily mappable to ESP32 display)
- Scaling options: Original, Fit, Fill (following existing emulators)

##### 3.4 Audio Integration
- PICO-8 has 4-channel sound synthesis
- Integrate with existing audio pipeline
- Support for music and sound effects

##### 3.5 Input Integration
- Map gamepad controls to PICO-8 inputs:
  - D-Pad: Arrow keys/movement
  - A Button: Z key (primary action)
  - B Button: X key (secondary action)
  - Start: Enter key
  - Select: Shift key (alternative action)

#### 4. Memory Considerations
PICO-8 is well-suited for ESP32 constraints:
- Small cartridge size (max 32KB)
- Minimal RAM requirements
- 128x128 framebuffer = 16KB (1 byte per pixel)
- Audio buffers are small due to simple synthesis

#### 5. Build System Integration

##### 5.1 CMakeLists.txt Updates
```cmake
# In main/CMakeLists.txt
set(SRCS
    # ... existing sources ...
    pico8_cart.hpp
)

# Add conditional compilation
if(CONFIG_ENABLE_PICO8)
    list(APPEND COMPONENT_REQUIRES pico8)
endif()
```

##### 5.2 Kconfig Integration
```kconfig
# In main/Kconfig.projbuild
config ENABLE_PICO8
    bool "Enable PICO-8 emulator"
    default y
    help
      Enable PICO-8 fantasy console emulator support
```

#### 6. Component Implementation

##### 6.1 components/pico8/CMakeLists.txt
```cmake
idf_component_register(
  INCLUDE_DIRS "include"
  SRC_DIRS "src" "femto8/src"
  PRIV_INCLUDE_DIRS "femto8/include"
  REQUIRES "box-emu" "statistics" "shared_memory"
)

target_compile_options(${COMPONENT_LIB} PRIVATE 
  -Wno-unused-function 
  -Wno-unused-variable
  -DFEMTO8_EMBEDDED
  -DFEMTO8_NO_FILESYSTEM
)
```

##### 6.2 PICO-8 API Wrapper
Create a C++ wrapper around femto8's C API:

```cpp
// components/pico8/include/pico8.hpp
#pragma once

#include <span>
#include <cstdint>

void init_pico8(const char* rom_filename, const uint8_t* rom_data, size_t rom_size);
void deinit_pico8();
void reset_pico8();
void run_pico8_frame();
std::span<uint8_t> get_pico8_video_buffer();
void set_pico8_input(uint8_t buttons);
void load_pico8_state(const char* path);
void save_pico8_state(const char* path);
```

#### 7. ROM Format Support
- Support `.p8` cartridge files
- Handle PICO-8 cartridge format parsing
- Implement cartridge metadata extraction for menu display

#### 8. Menu Integration
- Add PICO-8 to emulator selection menu
- Support boxart display for PICO-8 games
- Implement game metadata parsing from cartridge headers

#### 9. Testing Strategy
1. **Unit Tests**: Test cartridge loading and basic emulation functions
2. **Integration Tests**: Test with known PICO-8 games
3. **Performance Tests**: Verify acceptable frame rates on ESP32-S3
4. **Memory Tests**: Ensure stable operation within memory constraints

#### 10. Documentation Updates
- Update README.md with PICO-8 support information
- Add PICO-8 specific configuration options
- Document control mapping
- Add troubleshooting section for PICO-8 games

#### 11. Example Games
Include a few open-source PICO-8 demos/games for testing:
- Simple demos that showcase different features
- Games with different complexity levels
- Audio/visual test cartridges

### Implementation Phases

#### Phase 1: Core Integration (Week 1)
- [ ] Add femto8 as git submodule
- [ ] Create basic component structure
- [ ] Implement Pico8Cart class
- [ ] Basic video output (no scaling)

#### Phase 2: Full Feature Implementation (Week 2)
- [ ] Audio integration
- [ ] Input mapping
- [ ] Save state support
- [ ] Video scaling options

#### Phase 3: Polish and Testing (Week 3)
- [ ] Menu integration
- [ ] Metadata support
- [ ] Performance optimization
- [ ] Documentation
- [ ] Testing with various games

### Benefits
1. **Expanded Game Library**: Access to hundreds of PICO-8 games
2. **Modern Indie Games**: Many contemporary indie developers use PICO-8
3. **Educational Value**: PICO-8 is popular in game development education
4. **Small Footprint**: Minimal impact on existing codebase
5. **Community Interest**: Large and active PICO-8 community

### Potential Challenges
1. **Lua Integration**: femto8 includes Lua interpreter (may need memory optimization)
2. **Audio Synthesis**: PICO-8's unique audio system may need adaptation
3. **Cartridge Format**: Need to handle PICO-8's specific cartridge structure
4. **Performance**: Ensure 60 FPS on ESP32-S3 hardware

### Compatibility
- **Hardware**: Compatible with all existing esp-box-emu hardware
- **Controls**: Uses standard gamepad layout
- **Storage**: SD card support for cartridges
- **Display**: Scales well to existing display resolutions

### File Extensions
- `.p8` - PICO-8 cartridge files
- `.p8_save` - PICO-8 save state files (following naming convention)

### Memory Usage Estimation
- Emulator core: ~50KB
- Cartridge data: ~32KB max
- Video buffer: ~16KB (128x128)
- Audio buffers: ~8KB
- **Total additional**: ~106KB (well within ESP32-S3 capabilities)

### Next Steps
1. Fork femto8 and adapt for ESP-IDF compatibility
2. Create initial component structure
3. Implement basic Pico8Cart class
4. Test with simple PICO-8 cartridge
5. Iterate based on testing results

This implementation follows the established patterns in esp-box-emu and should integrate seamlessly with the existing codebase while providing a significant new feature for users.