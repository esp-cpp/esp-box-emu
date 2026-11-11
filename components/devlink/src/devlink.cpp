#include "devlink.hpp"

#include <cstring>

#include <esp_attr.h>
#include <esp_ota_ops.h>
#include <sdkconfig.h>
#if CONFIG_IDF_TARGET_ARCH_XTENSA
#include <esp_cpu_utils.h>
#endif
#include <esp_debug_helpers.h>
#include <esp_private/panic_internal.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "box-emu.hpp"

void DevLink::send(std::span<const uint8_t> frame) { BoxEmu::get().vendor_write(frame); }

DevLink::DevLink(const Config &config) {
  auto &emu = BoxEmu::get();
  worker_ = std::make_unique<espp::DispatcherWorker>(espp::DispatcherWorker::Config{
      .send = &DevLink::send,
      .on_overflow =
          [this]() {
#if DEVLINK_HAS_OTA
            if (ota_service_) ota_service_->on_rx_overflow();
#endif
          },
      // ~3.5 KB used at the high-water mark (file transfers, core dump reads);
      // internal RAM, since the core dump service reads the flash
      .task_config = {.name = "devlink", .stack_size_bytes = 6144},
  });
  core_dump_service_ = std::make_unique<espp::CoreDumpService>(core_dump_, espp::CoreDumpService::Config{
                                                                               .send = &DevLink::send,
                                                                           });
  system_service_ = std::make_unique<espp::SystemService>(espp::SystemService::Config{
      .send = &DevLink::send,
      .on_reboot_request =
          [](espp::SystemService::RebootKind kind) {
            if (kind == espp::SystemService::RebootKind::Bootloader) {
              BoxEmu::get().usb_prepare_for_rom_download();
            }
            return true;
          },
  });
  monitor_service_ = std::make_unique<espp::MonitorService>(espp::MonitorService::Config{
      .send = &DevLink::send,
  });
  core_dump_guarded_ = std::make_unique<FlashGuarded<espp::CoreDumpService>>(*core_dump_service_);
  worker_->register_module(*core_dump_guarded_);
  worker_->register_module(*system_service_);
  worker_->register_module(*monitor_service_);
#if DEVLINK_HAS_OTA
  ota_ = std::make_unique<espp::Ota>(espp::Ota::Config{});
  ota_service_ = std::make_unique<espp::OtaService>(*ota_, espp::OtaService::Config{
                                                               .send = &DevLink::send,
                                                           });
  ota_guarded_ = std::make_unique<FlashGuarded<espp::OtaService>>(*ota_service_);
  worker_->register_module(*ota_guarded_);
#endif
  worker_->serve_discovery(config.device_name, config.firmware);
  emu.set_vendor_rx([this](std::span<const uint8_t> data) { worker_->push(data); });
}

DevLink::~DevLink() { BoxEmu::get().set_vendor_rx(nullptr); }

void DevLink::mark_app_valid() {
  esp_ota_img_states_t state;
  const esp_partition_t *running = esp_ota_get_running_partition();
  if (running && esp_ota_get_state_partition(running, &state) == ESP_OK && state == ESP_OTA_IMG_PENDING_VERIFY) {
    esp_ota_mark_app_valid_cancel_rollback();
    fmt::print("devlink: the updated firmware is up; marked valid\n");
  }
}

// The last panic, kept across the reset in RTC memory: a crash in the middle of
// a flash operation (an OTA erase) cannot write its core dump, and the console
// is the USB CDC port, which dies with the chip. The wrappers run in the panic
// path (possibly with the cache off): IRAM, and only plain byte copies.
namespace {
struct LastPanic {
  uint32_t magic;
  uint32_t addr;
  int32_t core;
  int32_t exception;
  char reason[64];
  char detail[96];
  char task[16];
  uint32_t backtrace[20]; // Xtensa: the abort()'s caller chain
};
constexpr uint32_t kPanicMagic = 0x50414e43; // "PANC"
RTC_NOINIT_ATTR LastPanic s_last_panic;
DRAM_ATTR volatile bool s_aborting = false;
IRAM_ATTR void copy_str(char *dst, size_t n, const char *src) {
  size_t i = 0;
  if (src) {
    for (; i + 1 < n && src[i]; ++i) dst[i] = src[i];
  }
  dst[i] = 0;
}
} // namespace

extern "C" void __real_esp_panic_handler(panic_info_t *info);
extern "C" IRAM_ATTR void __wrap_esp_panic_handler(panic_info_t *info) {
  // an abort() gets here too (panic_abort() traps): keep its message
  if (s_aborting) {
    __real_esp_panic_handler(info);
    return;
  }
  s_last_panic.magic = kPanicMagic;
  s_last_panic.addr = (uint32_t)info->addr;
  s_last_panic.core = info->core;
  s_last_panic.exception = (int32_t)info->exception;
  copy_str(s_last_panic.reason, sizeof(s_last_panic.reason), info->reason);
  copy_str(s_last_panic.detail, sizeof(s_last_panic.detail), info->description);
  TaskHandle_t t = xTaskGetCurrentTaskHandle();
  copy_str(s_last_panic.task, sizeof(s_last_panic.task), t ? pcTaskGetName(t) : "?");
  for (auto &pc : s_last_panic.backtrace) pc = 0;
  __real_esp_panic_handler(info);
}

extern "C" void __real_panic_abort(const char *details);
IRAM_ATTR void note_task_and_backtrace() {
  TaskHandle_t t = xTaskGetCurrentTaskHandle();
  copy_str(s_last_panic.task, sizeof(s_last_panic.task), t ? pcTaskGetName(t) : "?");
  for (auto &pc : s_last_panic.backtrace) pc = 0;
#if CONFIG_IDF_TARGET_ARCH_XTENSA
  esp_backtrace_frame_t frame{};
  esp_backtrace_get_start(&frame.pc, &frame.sp, &frame.next_pc);
  for (int i = 0; i < 20; ++i) {
    s_last_panic.backtrace[i] = esp_cpu_process_stack_pc(frame.pc);
    if (!esp_backtrace_get_next_frame(&frame)) break;
  }
#endif
}

extern "C" IRAM_ATTR void __wrap_panic_abort(const char *details) {
  s_aborting = true;
  note_task_and_backtrace();
  s_last_panic.magic = kPanicMagic;
  s_last_panic.addr = 0;
  s_last_panic.core = -1;
  s_last_panic.exception = -1;
  copy_str(s_last_panic.reason, sizeof(s_last_panic.reason), "abort");
  copy_str(s_last_panic.detail, sizeof(s_last_panic.detail), details);
  __real_panic_abort(details);
}

std::string DevLink::last_panic() {
  if (esp_reset_reason() != ESP_RST_PANIC || s_last_panic.magic != kPanicMagic) {
    return "";
  }
  s_last_panic.reason[sizeof(s_last_panic.reason) - 1] = 0;
  s_last_panic.detail[sizeof(s_last_panic.detail) - 1] = 0;
  s_last_panic.task[sizeof(s_last_panic.task) - 1] = 0;
  std::string bt;
  for (auto pc : s_last_panic.backtrace) {
    if (pc) bt += fmt::format(" {:#010x}", pc);
  }
  return fmt::format("{} ({}) at {:#010x} on core {} in task '{}'{}{}", s_last_panic.reason, s_last_panic.detail,
                     s_last_panic.addr, s_last_panic.core, s_last_panic.task, bt.empty() ? "" : "; backtrace", bt);
}
