#pragma once

#include <memory>
#include <string>

#include "coredump.hpp"
#include "coredump_service.hpp"
#include "dispatcher_worker.hpp"
#include "monitor_service.hpp"
#include "system_service.hpp"

#include <esp_heap_caps.h>
#include <esp_system.h>

#include "box-emu.hpp"

/// Check the heap around every flash-touching request (debugging aid).
#ifndef DEVLINK_HEAP_CHECK
#define DEVLINK_HEAP_CHECK 0
#endif
#if DEVLINK_HAS_OTA
#include "ota.hpp"
#include "ota_service.hpp"
#endif

/// A module whose requests touch the flash (erase / write / read), run under
/// the launcher's LVGL lock. While the flash is busy the cache is off and only
/// interrupt handlers placed in internal RAM run; the display's SPI "transfer
/// done" interrupt is one and calls into LVGL (flash / PSRAM): a screen update
/// finishing during an OTA erase panics (esp-box-emu). LVGL waits for its
/// transfers before it lets go of the lock, so none is in flight meanwhile.
template <espp::DispatcherModuleConcept Module> class FlashGuarded {
public:
  explicit FlashGuarded(Module &module) : module_(module) {}
  uint8_t module_id() const { return module_.module_id(); }
  espp::DispatcherModuleInfo module_info() const { return module_.module_info(); }
  void handle(const espp::stream_frame::Frame &frame) {
    std::lock_guard<std::recursive_mutex> lk(BoxEmu::lvgl_mutex());
#if DEVLINK_HEAP_CHECK
    if (!heap_caps_check_integrity_all(true)) abort_corrupt("before", frame);
#endif
    module_.handle(frame);
#if DEVLINK_HEAP_CHECK
    if (!heap_caps_check_integrity_all(true)) abort_corrupt("after", frame);
#endif
  }

protected:
#if DEVLINK_HEAP_CHECK
  [[noreturn]] static void abort_corrupt(const char *when, const espp::stream_frame::Frame &frame) {
    static char msg[80];
    const bool internal_ok = heap_caps_check_integrity(MALLOC_CAP_INTERNAL, false);
    const bool psram_ok = heap_caps_check_integrity(MALLOC_CAP_SPIRAM, false);
    snprintf(msg, sizeof(msg), "heap corrupt %s request: internal %s, psram %s (frame @%p)", when,
             internal_ok ? "ok" : "BAD", psram_ok ? "ok" : "BAD", (const void *)&frame);
    esp_system_abort(msg);
  }
#endif
  Module &module_;
};

/// The framed services on the composite USB device's vendor interface, one
/// espp::DispatcherWorker routing them (shared by esp-box-emu and tab5-emu):
///
/// | module | service                                   | host tool                |
/// |--------|-------------------------------------------|--------------------------|
/// | 0      | OTA (DEVLINK_HAS_OTA: a two-slot layout)   | espp_ota, emulink ota    |
/// | 4      | core dump of the last crash               | espp_coredump, emulink   |
/// | 7      | system info, reboot, ROM download mode    | emulink reboot           |
/// | 8      | heap / task monitor                       | the espp Device Hub      |
/// | 0x20   | test control (registered by the app)      | emulink                  |
///
/// The device answers discovery, so the hosted espp web consoles find them.
class DevLink {
public:
  struct Config {
    std::string device_name; ///< advertised through discovery
    std::string firmware;    ///< firmware version, advertised through discovery
  };

  explicit DevLink(const Config &config);
  ~DevLink();

  /// Register an application module (e.g. the test control service).
  template <espp::DispatcherModuleConcept Module> void register_module(Module &module) {
    worker_->register_module(module);
  }

  /// Send one frame to the host (serialized; all or nothing).
  static void send(std::span<const uint8_t> frame);

  /// The running image booted after an update and has not been confirmed
  /// yet: confirm it (call once the app is up). No-op otherwise.
  static void mark_app_valid();

  /// After a panic reset: what it was ("reason (detail) at PC on core N"),
  /// kept in RTC memory (no core dump needed); "" otherwise.
  static std::string last_panic();

protected:
  std::unique_ptr<espp::DispatcherWorker> worker_;
  espp::CoreDump core_dump_;
  std::unique_ptr<espp::CoreDumpService> core_dump_service_;
  std::unique_ptr<FlashGuarded<espp::CoreDumpService>> core_dump_guarded_;
  std::unique_ptr<espp::SystemService> system_service_;
  std::unique_ptr<espp::MonitorService> monitor_service_;
#if DEVLINK_HAS_OTA
  std::unique_ptr<espp::Ota> ota_;
  std::unique_ptr<espp::OtaService> ota_service_;
  std::unique_ptr<FlashGuarded<espp::OtaService>> ota_guarded_;
#endif
};
