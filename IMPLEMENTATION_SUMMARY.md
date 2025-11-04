# PICO-8 Emulator Implementation Summary

This document summarizes the proposed implementation for adding PICO-8 emulator support to esp-box-emu using the `esp-cpp/femto8` fork with direct source integration.

## Files Created

### 1. Core Documentation
- **`PICO8_PR_PROPOSAL.md`** - Comprehensive PR proposal with implementation plan
- **`IMPLEMENTATION_SUMMARY.md`** - This summary document

### 2. Component Structure
- **`components/pico8/CMakeLists.txt`** - Build configuration for PICO-8 component
- **`components/pico8/include/pico8.hpp`** - C++ API wrapper for femto8 emulator
- **`components/pico8/src/pico8.cpp`** - Implementation with shared_memory integration
- **`components/pico8/README.md`** - Component documentation

### 3. Integration Files
- **`main/pico8_cart.hpp`** - Cart class implementation for PICO-8
- **`main/carts_integration_example.hpp`** - Integration points for main cart system

## Implementation Strategy

### Approach: Direct Source Integration (Recommended)
Following the pattern of existing emulators (nofrendo, gnuboy, etc.), we will:

1. **Fork femto8 to esp-cpp/femto8**
   - Create our own fork for ESP32-specific modifications
   - Add shared_memory integration hooks
   - Implement ESP32 performance optimizations

2. **Copy source directly into component**
   - No git submodule management needed
   - Easy to modify for ESP32 integration
   - Consistent with existing emulator patterns

3. **Integrate with shared_memory component**
   - Replace static memory allocations
   - Use SharedMemory::get().allocate() for all major memory blocks
   - Enable better memory management and debugging

## Implementation Roadmap

### Phase 1: Foundation (Week 1)
1. **Set up esp-cpp/femto8 fork**
   ```bash
   # Fork benbaker76/femto8 to esp-cpp/femto8
   # Add ESP32 optimizations branch
   # Test compilation with ESP-IDF
   ```

2. **Copy source to component**
   ```bash
   cd components/pico8
   git clone https://github.com/esp-cpp/femto8.git temp_femto8
   cp -r temp_femto8/src femto8/src
   cp -r temp_femto8/include femto8/include
   rm -rf temp_femto8
   ```

3. **Create component structure**
   - Set up CMakeLists.txt with shared_memory dependency
   - Create C++ wrapper API
   - Test basic compilation

### Phase 2: Core Functionality (Week 2)
1. **Shared Memory Integration**
   - Replace femto8's static allocations
   - Implement memory management functions
   - Test memory allocation/deallocation

2. **Video integration**
   - Connect femto8 framebuffer to ESP32 display
   - Implement palette conversion
   - Test video output with scaling

3. **Input system**
   - Map gamepad controls to PICO-8 inputs
   - Test input responsiveness
   - Handle button state management

### Phase 3: Advanced Features (Week 3)
1. **Audio integration**
   - Connect PICO-8 audio synthesis to ESP32 audio pipeline
   - Implement audio buffer management
   - Test audio output quality

2. **Save state system**
   - Implement save/load functionality
   - Integrate with existing save system
   - Test save state reliability

3. **Performance optimization**
   - Profile memory usage
   - Optimize frame rate
   - Test with various cartridges

## Key Integration Points

### 1. Shared Memory Integration
```cpp
// Replace femto8's static allocations
pico8_ram = SharedMemory::get().allocate(PICO8_RAM_SIZE, "pico8_ram");
pico8_vram = SharedMemory::get().allocate(PICO8_VRAM_SIZE, "pico8_vram");
lua_heap = SharedMemory::get().allocate(LUA_HEAP_SIZE, "pico8_lua");

// Initialize femto8 with our memory
femto8_config_t config = {
    .ram = pico8_ram,
    .ram_size = PICO8_RAM_SIZE,
    .vram = pico8_vram,
    .vram_size = PICO8_VRAM_SIZE,
    .lua_heap = lua_heap,
    .lua_heap_size = LUA_HEAP_SIZE
};
femto8_init(&config);
```

### 2. ESP32 Optimizations
```c
// In femto8 source files
#ifdef FEMTO8_ESP32
  #define PICO8_IRAM IRAM_ATTR
  #define PICO8_MEMCPY esp32_dma_memcpy
#endif
```

### 3. Build System Integration
```cmake
# CMakeLists.txt includes shared_memory dependency
REQUIRES "box-emu" "statistics" "shared_memory"

# Compile definitions for ESP32 integration
target_compile_definitions(${COMPONENT_LIB} PRIVATE 
  FEMTO8_EMBEDDED
  FEMTO8_SHARED_MEMORY
  FEMTO8_ESP32
)
```

## Technical Specifications

### Memory Usage (Updated with Shared Memory)
- **Emulator Core**: ~45KB
- **PICO-8 RAM**: 32KB (via SharedMemory)
- **Video RAM**: 16KB (via SharedMemory)
- **Lua Heap**: 64KB (via SharedMemory)
- **Audio Buffers**: 8KB (via SharedMemory)
- **Cartridge Data**: Up to 32KB
- **Total**: ~197KB (well within ESP32-S3 capabilities)

### Performance Targets
- **Frame Rate**: 60 FPS on ESP32-S3
- **Audio Latency**: <20ms
- **Boot Time**: <2 seconds
- **Memory Efficiency**: Optimal with shared_memory component

### Compatibility
- **Hardware**: All existing esp-box-emu hardware
- **Controls**: Standard gamepad layout
- **Display**: All supported display configurations
- **Audio**: Existing audio pipeline
- **Memory**: Integrates with shared_memory component

## Benefits of This Approach

### 1. Consistency with Existing Emulators
- Matches nofrendo, gnuboy, and other emulator patterns
- No submodule management complexity
- Consistent build and integration process

### 2. ESP32 Optimization Opportunities
- Direct modification of femto8 source for ESP32
- Integration with shared_memory component
- ESP32-specific performance optimizations
- Better debugging and profiling capabilities

### 3. Memory Management Benefits
- Uses shared_memory component for all allocations
- Better memory tracking and debugging
- Optimal memory layout for ESP32
- Easier memory profiling and optimization

### 4. Maintenance Advantages
- Full control over source code
- No submodule synchronization issues
- Direct integration with esp-box-emu architecture
- Easier to apply ESP32-specific patches

## ESP32-Specific Modifications

### 1. Memory Management
```c
// Replace static allocations in femto8
#ifdef FEMTO8_SHARED_MEMORY
extern uint8_t* pico8_get_ram_ptr(void);
extern uint8_t* pico8_get_vram_ptr(void);
extern uint8_t* pico8_get_lua_heap_ptr(void);

#define PICO8_RAM pico8_get_ram_ptr()
#define PICO8_VRAM pico8_get_vram_ptr()
#define LUA_HEAP pico8_get_lua_heap_ptr()
#endif
```

### 2. Performance Optimizations
```c
// Use IRAM for critical functions
#ifdef FEMTO8_ESP32
IRAM_ATTR void pico8_run_scanline(void);
IRAM_ATTR void pico8_update_audio(void);
#endif
```

### 3. ESP-IDF Integration
```c
// Use ESP-IDF specific functions
#ifdef FEMTO8_ESP32
#include "esp_timer.h"
#include "esp_heap_caps.h"

uint64_t pico8_get_time_us(void) {
    return esp_timer_get_time();
}
#endif
```

## Testing Strategy

### Unit Tests
1. **Memory Management**: Test SharedMemory integration
2. **Component Loading**: Test PICO-8 component initialization
3. **Cartridge Parsing**: Test .p8 file loading
4. **Video Output**: Test framebuffer generation
5. **Audio Output**: Test audio synthesis

### Integration Tests
1. **Cart System**: Test integration with main cart system
2. **Menu Integration**: Test PICO-8 appears in emulator selection
3. **Save States**: Test save/load functionality
4. **Video Scaling**: Test all scaling modes
5. **Performance**: Test frame rate consistency

### Game Compatibility Tests
1. **Simple Games**: Basic PICO-8 functionality
2. **Complex Games**: Advanced features (Celeste, etc.)
3. **Audio Games**: Music and sound effects
4. **Memory-Intensive**: Games that use lots of Lua memory
5. **Long Sessions**: Memory stability over time

## Fork Management Strategy

### esp-cpp/femto8 Repository Structure
```
esp-cpp/femto8/
├── main                    # Main branch (sync with upstream)
├── esp32-optimizations    # ESP32-specific modifications
├── shared-memory          # SharedMemory integration
└── esp-box-emu           # Integration branch for esp-box-emu
```

### Modification Areas
1. **Memory Management**: Replace malloc/free with SharedMemory calls
2. **Performance**: Add IRAM attributes for critical functions
3. **Configuration**: Add ESP32-specific configuration options
4. **Integration**: Add hooks for esp-box-emu integration

## Next Steps

### Immediate Actions
1. **Create esp-cpp/femto8 fork**
   - Fork from benbaker76/femto8
   - Create ESP32 optimization branch
   - Test basic compilation with ESP-IDF

2. **Implement shared_memory integration**
   - Modify femto8 memory allocation functions
   - Test memory management
   - Profile memory usage

3. **Create component structure**
   - Set up component files
   - Implement C++ wrapper
   - Test basic integration

### Development Phases
- **Week 1**: Fork setup and basic integration
- **Week 2**: Core functionality and shared_memory integration
- **Week 3**: Performance optimization and testing

### Success Criteria
- [ ] Loads and runs PICO-8 cartridges
- [ ] Integrates with shared_memory component
- [ ] Maintains 60 FPS performance
- [ ] Audio works correctly
- [ ] Save states function properly
- [ ] Memory usage is optimal
- [ ] No memory leaks or crashes

## Conclusion

This updated implementation approach provides the best of both worlds:

1. **Uses our own fork** (esp-cpp/femto8) for full control
2. **Direct source integration** following existing patterns
3. **SharedMemory integration** for optimal memory management
4. **ESP32-specific optimizations** for best performance
5. **No submodule complexity** for easier maintenance

The approach ensures seamless integration with the esp-box-emu architecture while providing a high-quality PICO-8 emulator that takes full advantage of the ESP32-S3's capabilities and the project's shared memory management system.