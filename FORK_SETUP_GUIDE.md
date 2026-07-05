# esp-cpp/femto8 Fork Setup Guide

This guide explains how to set up and manage the `esp-cpp/femto8` fork for PICO-8 emulator integration.

## Fork Creation

### 1. Create the Fork
```bash
# Fork benbaker76/femto8 to esp-cpp/femto8 via GitHub UI
# Or use GitHub CLI:
gh repo fork benbaker76/femto8 --org esp-cpp --fork-name femto8
```

### 2. Clone and Setup Local Repository
```bash
git clone https://github.com/esp-cpp/femto8.git
cd femto8
git remote add upstream https://github.com/benbaker76/femto8.git
```

### 3. Create ESP32 Integration Branch
```bash
# Create branch for ESP32-specific modifications
git checkout -b esp32-integration
git push -u origin esp32-integration

# Create branch for shared_memory integration
git checkout -b shared-memory-integration
git push -u origin shared-memory-integration

# Create branch specifically for esp-box-emu
git checkout -b esp-box-emu-integration
git push -u origin esp-box-emu-integration
```

## Integration with esp-box-emu

### 1. Copy Source to Component
```bash
# From esp-box-emu root directory
cd components/pico8

# Clone our fork temporarily
git clone https://github.com/esp-cpp/femto8.git temp_femto8
cd temp_femto8
git checkout esp-box-emu-integration  # Use our integration branch

# Copy source files
cd ..
mkdir -p femto8/src femto8/include
cp -r temp_femto8/src/* femto8/src/
cp -r temp_femto8/include/* femto8/include/

# Clean up
rm -rf temp_femto8

# Add to git
git add femto8/
git commit -m "Add femto8 source for PICO-8 emulator"
```

### 2. Apply ESP32-Specific Modifications

#### Memory Management Integration
Create `femto8/src/pico8_esp32_memory.c`:
```c
#ifdef FEMTO8_SHARED_MEMORY
#include "shared_memory.hpp"

static uint8_t* pico8_memory_pool = NULL;
static size_t pico8_memory_pool_size = 0;

// Memory allocation hooks for SharedMemory integration
void* pico8_malloc(size_t size, const char* tag) {
    return SharedMemory::get().allocate(size, tag);
}

void pico8_free(void* ptr, const char* tag) {
    SharedMemory::get().deallocate((uint8_t*)ptr, tag);
}

// Initialize memory system
int pico8_init_memory_system(size_t total_size) {
    pico8_memory_pool = SharedMemory::get().allocate(total_size, "pico8_pool");
    if (!pico8_memory_pool) {
        return -1;
    }
    pico8_memory_pool_size = total_size;
    return 0;
}

void pico8_deinit_memory_system(void) {
    if (pico8_memory_pool) {
        SharedMemory::get().deallocate(pico8_memory_pool, "pico8_pool");
        pico8_memory_pool = NULL;
        pico8_memory_pool_size = 0;
    }
}
#endif
```

#### Performance Optimizations
Modify critical functions in femto8 source files:
```c
// Add to femto8/src/pico8_cpu.c
#ifdef FEMTO8_ESP32
#include "esp_attr.h"
#define PICO8_FAST_FUNC IRAM_ATTR
#else
#define PICO8_FAST_FUNC
#endif

// Apply to performance-critical functions
PICO8_FAST_FUNC void pico8_cpu_step(void) {
    // ... existing implementation
}

PICO8_FAST_FUNC void pico8_render_scanline(int line) {
    // ... existing implementation
}
```

#### Configuration Header
Create `femto8/include/pico8_esp32_config.h`:
```c
#ifndef PICO8_ESP32_CONFIG_H
#define PICO8_ESP32_CONFIG_H

#ifdef FEMTO8_ESP32
// ESP32-specific configuration
#define PICO8_SAMPLE_RATE 22050
#define PICO8_AUDIO_BUFFER_SIZE 512
#define PICO8_MAX_CARTRIDGE_SIZE 32768
#define PICO8_RAM_SIZE 32768
#define PICO8_VRAM_SIZE 16384

// Use ESP32 timer functions
#include "esp_timer.h"
#define PICO8_GET_TIME_US() esp_timer_get_time()

// Use ESP32 memory functions
#ifdef FEMTO8_SHARED_MEMORY
#define PICO8_MALLOC(size, tag) pico8_malloc(size, tag)
#define PICO8_FREE(ptr, tag) pico8_free(ptr, tag)
#else
#define PICO8_MALLOC(size, tag) malloc(size)
#define PICO8_FREE(ptr, tag) free(ptr)
#endif

#else
// Default configuration for other platforms
#define PICO8_SAMPLE_RATE 44100
#define PICO8_AUDIO_BUFFER_SIZE 1024
// ... other defaults
#endif

#endif // PICO8_ESP32_CONFIG_H
```

## Maintenance Strategy

### 1. Sync with Upstream
```bash
# Periodically sync with original femto8
git checkout main
git fetch upstream
git merge upstream/main
git push origin main

# Rebase our integration branches
git checkout esp32-integration
git rebase main
git push --force-with-lease origin esp32-integration
```

### 2. Update esp-box-emu Integration
```bash
# When femto8 changes, update the copied source
cd components/pico8

# Get latest from our integration branch
git clone https://github.com/esp-cpp/femto8.git temp_femto8
cd temp_femto8
git checkout esp-box-emu-integration

# Update source files
cd ..
rm -rf femto8/src/* femto8/include/*
cp -r temp_femto8/src/* femto8/src/
cp -r temp_femto8/include/* femto8/include/
rm -rf temp_femto8

# Commit changes
git add femto8/
git commit -m "Update femto8 source to latest version"
```

### 3. Branch Management
- **main**: Sync with upstream benbaker76/femto8
- **esp32-integration**: ESP32-specific optimizations
- **shared-memory-integration**: SharedMemory component integration
- **esp-box-emu-integration**: Final integration branch for esp-box-emu

## Modification Tracking

### Key Files to Modify
1. **Memory Management**:
   - `src/pico8_memory.c` - Replace malloc/free calls
   - `src/pico8_lua.c` - Lua heap management
   - `include/pico8_config.h` - Memory configuration

2. **Performance**:
   - `src/pico8_cpu.c` - Add IRAM attributes
   - `src/pico8_gfx.c` - Graphics rendering optimizations
   - `src/pico8_sound.c` - Audio processing optimizations

3. **Integration**:
   - `include/pico8.h` - Main API header
   - `src/pico8_api.c` - Public API implementation
   - `include/pico8_esp32_config.h` - ESP32 configuration

### Documentation of Changes
Maintain a `MODIFICATIONS.md` file in the fork:
```markdown
# ESP32 Modifications to femto8

## Memory Management
- Replaced malloc/free with SharedMemory integration
- Added configurable memory pool sizes
- Implemented memory tracking and debugging

## Performance Optimizations
- Added IRAM attributes to critical functions
- Optimized memory copy operations for ESP32
- Reduced dynamic allocations in hot paths

## ESP-IDF Integration
- Added ESP32 timer support
- Integrated with ESP-IDF build system
- Added ESP32-specific configuration options
```

## Testing the Fork

### 1. Basic Compilation Test
```bash
# Test compilation with ESP-IDF
cd components/pico8
idf.py build
```

### 2. Memory Integration Test
```cpp
// Test SharedMemory integration
#include "pico8.hpp"
#include "shared_memory.hpp"

void test_pico8_memory() {
    // Initialize PICO-8 with test cartridge
    const uint8_t test_cart[] = { /* minimal PICO-8 cart */ };
    init_pico8("test.p8", test_cart, sizeof(test_cart));
    
    // Verify memory allocations
    auto stats = SharedMemory::get().get_stats();
    // Check that pico8_ram, pico8_vram, etc. are allocated
    
    deinit_pico8();
    
    // Verify cleanup
    auto stats_after = SharedMemory::get().get_stats();
    // Check that memory was properly deallocated
}
```

### 3. Performance Test
```cpp
// Test frame rate performance
void test_pico8_performance() {
    init_pico8("test.p8", test_cart, sizeof(test_cart));
    
    auto start_time = esp_timer_get_time();
    for (int i = 0; i < 60; i++) {
        run_pico8_frame();
    }
    auto end_time = esp_timer_get_time();
    
    uint32_t frame_time_us = (end_time - start_time) / 60;
    printf("Average frame time: %d us\n", frame_time_us);
    // Should be close to 16667 us (60 FPS)
    
    deinit_pico8();
}
```

## Contributing Back to Upstream

### 1. Identify Generic Improvements
Some modifications may be useful for the upstream project:
- Bug fixes
- Performance improvements that don't depend on ESP32
- API enhancements

### 2. Create Upstream PRs
```bash
# Create branch for upstream contribution
git checkout main
git checkout -b feature/performance-improvements

# Cherry-pick generic improvements
git cherry-pick <commit-hash>

# Push and create PR to benbaker76/femto8
git push origin feature/performance-improvements
```

## Conclusion

This fork management strategy provides:
- **Full control** over femto8 source for ESP32 integration
- **Easy maintenance** with clear branching strategy
- **Upstream compatibility** for contributing improvements back
- **Integration simplicity** with direct source copying
- **Performance optimization** opportunities

The approach ensures we can make necessary modifications while maintaining the ability to sync with upstream improvements and contribute back to the femto8 project.