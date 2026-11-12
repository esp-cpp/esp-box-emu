#include <sdkconfig.h>
#if CONFIG_ESP_TASK_WDT_EN
#include <esp_task_wdt.h>
#endif

#include <chrono>
#include <memory>
#include <thread>
#include <vector>
#include <stdio.h>

#include "logger.hpp"
#include "task_monitor.hpp"
#include "timer.hpp"

#include "box-emu.hpp"
#include "carts.hpp"
#include "devlink.hpp"
#include "gui.hpp"
#include "remote_control.hpp"
#include "settings.hpp"
#include "memory_census.hpp"
#include "rom_info.hpp"
#include "statistics.hpp"
#include "system_info.hpp"

#if CONFIG_HEAP_TRACING_STANDALONE
// Leak hunting (a local sdkconfig with CONFIG_HEAP_TRACING_STANDALONE=y): every
// allocation a cart's lifetime leaves behind, with its callers, is dumped after
// the quit census. The records live in PSRAM, allocated once at boot.
#include <esp_heap_trace.h>
static constexpr size_t kHeapTraceRecords = 1000;
#endif

using namespace std::chrono_literals;

extern "C" void app_main(void) {
  espp::Logger logger({.tag = "esp-box-emu", .level = espp::Logger::Verbosity::INFO});
  logger.info("Bootup");
  // Why did we boot? A hang that ends in a silent hard reset (no panic output)
  // is a watchdog, a brownout or a power glitch, and this is the only place
  // that distinguishes them from a crash.
  fmt::print("[boot] reset reason: {}\n",
             espp::SystemInfo::reset_reason_name(espp::SystemInfo::reset_reason()));
#if CONFIG_ESP_TASK_WDT_EN
  // Diagnostic (local sdkconfig only): watch the main task so a silent hang
  // prints its backtrace after the timeout. Fed from Cart::run() and the menu
  // wait loop, never from the cores' own tasks.
  esp_task_wdt_add(nullptr);
#endif

  // initialize the hardware abstraction layer
  BoxEmu &emu = BoxEmu::get();
  logger.info("Box Emu version: {}", emu.version());

  // initialize
  if (!emu.initialize_box()) {
    logger.error("Failed to initialize box!");
    return;
  }

  if (!emu.initialize_sdcard()) {
    logger.warn("Failed to initialize SD card!");
    logger.warn("This may happen if the SD card is not inserted.");
  }
  // the composite USB device on the USB-C port: the console moves to its CDC
  // port, the link services get its vendor interface, the uSD card its drive
  // function (the card stays ours until handed over)
  if (!emu.initialize_usb_device()) {
    logger.warn("USB composite device not available (console stays on USB-Serial-JTAG)");
  }


  if (!emu.initialize_memory()) {
    logger.error("Failed to initialize memory!");
    return;
  }

  if (!emu.initialize_gamepad()) {
    logger.warn("Failed to initialize gamepad!");
    logger.warn("This may happen if the gamepad is not connected.");
  }

  if (!emu.initialize_battery()) {
    logger.warn("Failed to initialize battery!");
    logger.warn("This may happen if the battery is not connected.");
  }

  if (!emu.initialize_video()) {
    logger.error("Failed to initialize video!");
    return;
  }

  if (!emu.initialize_haptics()) {
    logger.warn("Failed to initialize haptics!");
    logger.warn("This may happen if the gamepad is not connected.");
  }

  // the settings kept on the SD card (volume, brightness, video scaling,
  // haptics, the last game and its save slots); the first boot writes the
  // defaults
  {
    Settings defaults;
    defaults.volume = static_cast<uint8_t>(emu.volume());
    defaults.muted = emu.is_muted();
    defaults.brightness = static_cast<uint8_t>(emu.brightness());
    defaults.video_setting = static_cast<uint8_t>(emu.video_setting());
    auto &settings = AppSettings::get();
    settings.init(std::string(BoxEmu::mount_point) + "/settings.bin", defaults);
    const Settings s = settings.get_settings();
    emu.volume(s.volume);
    emu.mute(s.muted);
    emu.brightness(std::max<int>(10, s.brightness));
    emu.video_setting(static_cast<VideoSetting>(std::min<int>(s.video_setting, 2)));
    logger.info("settings: volume {}{}, brightness {}, video {}, resume {}, haptic {}, last game '{}'", s.volume,
                s.muted ? " (muted)" : "", s.brightness, s.video_setting, s.resume_last_save, s.haptic_waveform,
                s.last_rom);
  }

  logger.info("initializing gui...");

  auto display = BoxEmu::Bsp::get().display();

  // initialize the gui
  Gui gui({
      .play_haptic = [&emu]() { emu.play_haptic_effect(); },
      .set_waveform = [&emu](uint8_t waveform) { emu.set_haptic_effect(waveform); },
      .log_level = espp::Logger::Verbosity::WARN
    });

  // the USB link's services (OTA, core dump, system, monitor) and the test
  // control (see tools/emulink); its requests for the launcher / the game run
  // here, on the main loop, through main_queue
  MainQueue main_queue;
  RemoteControl control(gui, main_queue);
  DevLink devlink({.device_name = "ESP-Box-Emu", .firmware = esp_app_get_description()->version});
  devlink.register_module(control);
  // up and running: an update that got this far is good (else the bootloader
  // rolls back to the previous firmware on the next reset)
  DevLink::mark_app_valid();

  gui.set_haptic_waveform(AppSettings::get().get_settings().haptic_waveform);
  AppSettings::get().save(); // set_haptic_waveform() above marks it changed

  print_mem_census("boot");
  print_mem_blocks("boot", 2048);
#if CONFIG_HEAP_TRACING_STANDALONE
  if (auto *records = static_cast<heap_trace_record_t *>(
          heap_caps_calloc(kHeapTraceRecords, sizeof(heap_trace_record_t), MALLOC_CAP_SPIRAM))) {
    heap_trace_init_standalone(records, kHeapTraceRecords);
  }
#endif

  // set the task priority (for main) to high
  vTaskPrioritySet(nullptr, 20);

  // main loop
  while (true) {
    // reset gui ready to play and user_quit
    gui.ready_to_play(false);
    while (!gui.ready_to_play()) {
      std::this_thread::sleep_for(50ms);
      // settings changed in the GUI are written once the changes stop
      AppSettings::get().save_if_idle();
      main_queue.poll();
#if CONFIG_ESP_TASK_WDT_EN
      esp_task_wdt_reset();
#endif
    }

    // have broken out of the loop, let the user know we're processing...
    emu.set_haptic_effect(gui.get_haptic_waveform());
    emu.play_haptic_effect();

    gui.pause();

    auto maybe_selected_rom = gui.get_selected_rom();
    if (maybe_selected_rom.has_value()) {
      auto selected_rom = maybe_selected_rom.value();
      logger.info("Selected rom:\n\t{}", selected_rom);
      AppSettings::get().modify([&](Settings &s) { s.last_rom = selected_rom.rom_path; });
      AppSettings::get().save();

      // Memory census around the cart's lifetime: "menu" is the baseline,
      // "launch" shows what the core took (and where its big buffers landed,
      // see the per-core [.. mem] lines), and "quit" is diffed against the
      // baseline so a leak or fragmentation across a play cycle shows up as a
      // non-zero delta.
      const MemSnapshot menu_mem = print_mem_census("menu");
#if CONFIG_HEAP_TRACING_STANDALONE
      heap_trace_start(HEAP_TRACE_LEAKS);
#endif

      // Cart handles platform specific code, state management, etc.
      {
        std::unique_ptr<Cart> cart(make_cart(selected_rom, display));
        if (cart) {
          control.set_cart(cart.get());
          // start from the save state this game was last saved to / loaded from
          if (AppSettings::get().get_settings().resume_last_save) {
            if (const auto slot = AppSettings::get().last_slot(selected_rom.rom_path)) {
              cart->resume_from_slot(*slot);
            }
          }
          print_mem_census("launch", &menu_mem);
          print_mem_blocks("launch", 4096);
          while (cart->run())
            main_queue.poll();
          control.set_cart(nullptr);
        } else {
          logger.error("Failed to create cart!");
        }
      }
      // Only the task lines matter here (stack high-water marks after a full
      // play session); the block threshold is set so no block line prints.
      // The census goes last so its `[mem] delta phase=quit` line marks the
      // end of a run for tools/serial_capture.py --until.
      print_mem_blocks("quit", SIZE_MAX);
#if CONFIG_HEAP_TRACING_STANDALONE
      heap_trace_stop();
      {
        // (heap_trace_dump() prints to the UART only)
        size_t leaked = 0;
        const size_t count = heap_trace_get_count();
        for (size_t i = 0; i < count; i++) {
          heap_trace_record_t r;
          if (heap_trace_get(i, &r) != ESP_OK || !r.address) continue;
          leaked += r.size;
          std::string callers;
          for (auto *pc : r.alloced_by) {
            if (pc) callers += fmt::format(" {}", pc);
          }
          fmt::print("[leak] {} B at {}{}:{}\n", r.size, r.address,
                     esp_ptr_external_ram(r.address) ? " (PSRAM)" : "", callers);
        }
        fmt::print("[leak] total {} B in {} blocks\n", leaked, count);
      }
#endif
      print_mem_census("quit", &menu_mem);
    } else {
      logger.error("Invalid rom selected!");
    }

    // volume / brightness / video scaling may have changed in the pause menu
    // or with the volume buttons
    AppSettings::get().modify([&](Settings &s) {
      s.volume = static_cast<uint8_t>(emu.volume());
      s.muted = emu.is_muted();
      s.brightness = static_cast<uint8_t>(emu.brightness());
      s.video_setting = static_cast<uint8_t>(emu.video_setting());
    });
    AppSettings::get().save();

    // print the frame statistics from the previous run
    print_statistics();

    logger.info("Done playing, resuming gui...");

    logger.debug("Task table:\n{}", espp::TaskMonitor::get_latest_info_table());

    gui.resume();
    display->force_refresh();
  }
}
