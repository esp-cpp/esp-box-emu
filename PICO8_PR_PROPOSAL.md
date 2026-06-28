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

- **Repository**: https://github.com/esp-cpp/femto8 (our fork)
- **Original**: https://github.com/benbaker76/femto8
- **License**: MIT (compatible with esp-box-emu)
- **Language**: Pure C (optimal for ESP32)
- **Design**: Specifically built for embedded systems with memory constraints
- **Features**: Compact, resource-efficient, based on retro8 but optimized for embedded use
- **Fork Benefits**: Allows us to modify for shared_memory integration and ESP32 optimizations

#### 2. Integration Strategy Options

We have two approaches, following the patterns used by other emulators in the project:

##### Option A: Direct Code Integration (Recommended)
Copy femto8 source directly into the component, similar to how `nofrendo`, `gnuboy`, and other emulators are integrated:

```
components/pico8/
├── CMakeLists.txt
├── include/
│   └── pico8.hpp
├── src/
│   └── pico8.cpp
└── femto8/                    # Direct source copy
    ├── src/
    │   ├── pico8_api.c
    │   ├── pico8_cart.c
    │   ├── pico8_gfx.c
    │   ├── pico8_sound.c
    │   ├── pico8_lua.c
    │   └── ...
    └── include/
        └── pico8.h
```

**Benefits:**
- No submodule management
- Easy to modify for ESP32 optimizations
- Consistent with other emulators (nofrendo, gnuboy, etc.)
- Simplified build process
- Direct integration with shared_memory component

##### Option B: Fork as Submodule
Use our `esp-cpp/femto8` fork as a git submodule:

```
components/pico8/
├── CMakeLists.txt
├── include/
│   └── pico8.hpp
├── src/
│   └── pico8.cpp
└── femto8/                    # Git submodule to esp-cpp/femto8
    └── (femto8 source)
```

**Benefits:**
- Easier to sync with upstream improvements
- Clear separation of our code vs. femto8 code
- Can contribute improvements back to femto8

**Recommendation:** Use Option A (direct integration) to match the existing pattern and allow for easier ESP32-specific modifications.

#### 3. Component Structure (Option A - Recommended)

```
components/pico8/
├── CMakeLists.txt
├── include/
│   └── pico8.hpp              # C++ wrapper API
├── src/
│   └── pico8.cpp              # C++ wrapper implementation
└── femto8/                    # Direct source integration
    ├── src/
    │   ├── pico8_api.c        # Main PICO-8 API
    │   ├── pico8_cart.c       # Cartridge handling
    │   ├── pico8_gfx.c        # Graphics rendering
    │   ├── pico8_sound.c      # Audio synthesis
    │   ├── pico8_lua.c        # Lua interpreter
    │   ├── pico8_memory.c     # Memory management
    │   └── pico8_input.c      # Input handling
    └── include/
        ├── pico8.h            # Main femto8 API
        ├── pico8_types.h      # Type definitions
        └── pico8_config.h     # Configuration
```

#### 4. Integration Points

##### 4.1 Cart System Integration
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

##### 4.2 File Format Support
- Add `.p8` file extension support for PICO-8 cartridges
- Implement cartridge loading from SD card
- Support for save states (following existing pattern)

##### 4.3 Video Integration
- PICO-8 native resolution: 128x128 pixels
- 16-color palette (easily mappable to ESP32 display)
- Scaling options: Original, Fit, Fill (following existing emulators)

##### 4.4 Audio Integration
- PICO-8 has 4-channel sound synthesis
- Integrate with existing audio pipeline
- Support for music and sound effects

##### 4.5 Input Integration
- Map gamepad controls to PICO-8 inputs:
  - D-Pad: Arrow keys/movement
  - A Button: Z key (primary action)
  - B Button: X key (secondary action)
  - Start: Enter key
  - Select: Shift key (alternative action)

#### 5. Memory Management Integration

##### 5.1 Shared Memory Component Integration
Modify femto8's static memory allocations to use our `shared_memory` component:

```cpp
// In components/pico8/src/pico8.cpp
#include "shared_memory.hpp"

// Replace femto8's static allocations with shared memory
static uint8_t* pico8_ram = nullptr;
static uint8_t* pico8_vram = nullptr;
static uint8_t* lua_heap = nullptr;

void init_pico8_memory() {
  // Use shared_memory component for allocations
  pico8_ram = SharedMemory::get().allocate(PICO8_RAM_SIZE, "pico8_ram");
  pico8_vram = SharedMemory::get().allocate(PICO8_VRAM_SIZE, "pico8_vram");
  lua_heap = SharedMemory::get().allocate(LUA_HEAP_SIZE, "pico8_lua");
}
```

##### 5.2 Memory Layout Optimization
```cpp
// Memory usage breakdown
constexpr size_t PICO8_RAM_SIZE = 32768;      // 32KB - Main RAM
constexpr size_t PICO8_VRAM_SIZE = 16384;     // 16KB - Video RAM  
constexpr size_t LUA_HEAP_SIZE = 65536;       // 64KB - Lua heap
constexpr size_t AUDIO_BUFFER_SIZE = 8192;    // 8KB - Audio buffers
// Total: ~126KB (well within ESP32-S3 capabilities)
```

#### 6. Build System Integration

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
  -Wno-implicit-fallthrough
  -Wno-discarded-qualifiers
  -DFEMTO8_EMBEDDED
  -DFEMTO8_NO_FILESYSTEM
  -DFEMTO8_ESP32
  -DFEMTO8_SHARED_MEMORY
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE 
  PICO8_EMULATOR
  FEMTO8_EMBEDDED
  FEMTO8_SHARED_MEMORY
)
```

##### 6.2 Kconfig Integration
```kconfig
# In main/Kconfig.projbuild
config ENABLE_PICO8
    bool "Enable PICO-8 emulator"
    default y
    help
      Enable PICO-8 fantasy console emulator support

config PICO8_LUA_HEAP_SIZE
    int "PICO-8 Lua heap size (KB)"
    depends on ENABLE_PICO8
    default 64
    range 32 128
    help
      Size of Lua heap for PICO-8 emulator
```

#### 7. ESP32-Specific Optimizations

##### 7.1 Memory Optimizations
```c
// In femto8/src/pico8_memory.c
#ifdef FEMTO8_SHARED_MEMORY
#include "shared_memory.hpp"

// Replace static allocations
static uint8_t* pico8_memory_pool = nullptr;

void pico8_init_memory() {
  pico8_memory_pool = SharedMemory::get().allocate(PICO8_TOTAL_MEMORY, "pico8");
  // Partition memory pool for different uses
}
#endif
```

##### 7.2 Performance Optimizations
```c
// Use ESP32-specific optimizations
#ifdef FEMTO8_ESP32
  // Use IRAM for critical functions
  #define PICO8_IRAM IRAM_ATTR
  
  // Use DMA for large memory operations
  #define PICO8_MEMCPY esp32_dma_memcpy
#else
  #define PICO8_IRAM
  #define PICO8_MEMCPY memcpy
#endif
```

#### 8. Implementation Steps

##### 8.1 Phase 1: Source Integration (Week 1)
1. **Fork femto8 to esp-cpp/femto8**
   - Create fork with ESP32 optimizations branch
   - Add shared_memory integration hooks
   - Test compilation with ESP-IDF

2. **Copy source to component**
   ```bash
   cd components/pico8
   git clone https://github.com/esp-cpp/femto8.git temp_femto8
   cp -r temp_femto8/src femto8/src
   cp -r temp_femto8/include femto8/include
   rm -rf temp_femto8
   ```

3. **Create component structure**
   - Set up CMakeLists.txt
   - Create C++ wrapper API
   - Test basic compilation

##### 8.2 Phase 2: Core Integration (Week 2)
1. **Implement Pico8Cart class**
   - Basic initialization and cleanup
   - Video buffer management
   - Input handling

2. **Integrate with cart system**
   - Add to emulator enum
   - Add file extension support
   - Test cart loading

3. **Memory integration**
   - Replace static allocations with shared_memory
   - Test memory efficiency
   - Profile memory usage

##### 8.3 Phase 3: Feature Completion (Week 3)
1. **Audio integration**
   - Connect to existing audio pipeline
   - Test audio quality
   - Optimize performance

2. **Save state system**
   - Implement save/load functionality
   - Test with various games
   - Ensure compatibility

3. **Performance optimization**
   - Profile frame rate
   - Optimize critical paths
   - Test with complex games

#### 9. Testing Strategy

##### 9.1 Unit Tests
- Component initialization
- Cartridge loading
- Memory management
- Video output
- Audio synthesis

##### 9.2 Integration Tests  
- Menu integration
- Save states
- Video scaling
- Performance benchmarks

##### 9.3 Game Compatibility Tests
Test with various PICO-8 games:
- Simple demos (Celeste Classic demo)
- Complex games (full Celeste)
- Audio-heavy games
- Graphics-intensive games

#### 10. Benefits of This Approach

##### 10.1 Consistency
- Matches existing emulator integration patterns
- No additional repository management
- Consistent build process

##### 10.2 Customization
- Easy ESP32-specific modifications
- Direct shared_memory integration
- Performance optimizations

##### 10.3 Maintenance
- No submodule synchronization issues
- Direct control over code changes
- Easier debugging and profiling

#### 11. File Extensions and Metadata

##### 11.1 Supported Formats
- `.p8` - PICO-8 cartridge files
- `.p8_save` - PICO-8 save state files

##### 11.2 Metadata Integration
```cpp
// Extract game title from P8 cartridge header
std::string extract_p8_title(const uint8_t* cart_data) {
  // Parse P8 format for title metadata
  // Return extracted title or filename
}
```

#### 12. Memory Usage Estimation (Updated)

```cpp
// Detailed memory breakdown
constexpr size_t PICO8_CORE_SIZE = 45000;        // ~45KB - Emulator core
constexpr size_t PICO8_RAM_SIZE = 32768;         // 32KB - PICO-8 RAM
constexpr size_t PICO8_VRAM_SIZE = 16384;        // 16KB - Video RAM
constexpr size_t LUA_HEAP_SIZE = 65536;          // 64KB - Lua interpreter heap
constexpr size_t AUDIO_BUFFERS_SIZE = 8192;      // 8KB - Audio buffers
constexpr size_t CART_DATA_SIZE = 32768;         // 32KB - Max cartridge size

// Total: ~200KB (acceptable for ESP32-S3 with PSRAM)
// Can be optimized further with shared_memory component
```

### Implementation Timeline

#### Week 1: Foundation
- [ ] Fork femto8 to esp-cpp/femto8
- [ ] Copy source into component
- [ ] Create basic component structure
- [ ] Implement C++ wrapper API
- [ ] Test compilation

#### Week 2: Core Features
- [ ] Implement Pico8Cart class
- [ ] Integrate with cart system
- [ ] Add shared_memory integration
- [ ] Implement video output
- [ ] Add input handling

#### Week 3: Polish & Testing
- [ ] Implement audio integration
- [ ] Add save state support
- [ ] Performance optimization
- [ ] Menu integration
- [ ] Comprehensive testing

### Conclusion

This updated implementation plan provides a robust approach to adding PICO-8 emulator support that:

1. **Follows established patterns** by copying source directly into the repository
2. **Uses our own fork** (esp-cpp/femto8) for easy customization
3. **Integrates with shared_memory** for optimal memory management
4. **Provides ESP32-specific optimizations** for best performance
5. **Maintains consistency** with existing emulator implementations

The approach eliminates submodule complexity while providing full control over the emulator source code, enabling deep integration with the esp-box-emu architecture.