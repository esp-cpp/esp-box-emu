#pragma once

// Test control over the USB link (dispatcher module 0x20, protocol
// "emu.control"): what a test script needs to drive the emulator -- list and
// launch games, quit, hold buttons, save / load states, take screenshots, move
// files to and from the SD card (with the device running), hand the card to
// the PC as a USB drive and reboot into the updater. tools/emulink is the
// host side.
//
// Every request is answered with a reply frame of the same type whose
// payload is one status byte (0 = ok, 1 = error) and then the body: JSON text
// (an error's message as {"error": ...}), or the raw bytes for FILE_READ and
// SCREENSHOT_READ.
// Requests that touch the launcher or the running game are handed to the main
// loop (MainQueue) and answered once it ran them.

#include <algorithm>
#include <cerrno>
#include <condition_variable>
#include <cstring>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <thread>
#include <tuple>
#include <unistd.h>
#include <vector>

#include <esp_heap_caps.h>
#include <esp_app_desc.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <esp_timer.h>

#include "bootloader_common.h"
#include "esp_flash_partitions.h"
#if CONFIG_IDF_TARGET_ESP32S3
#include "soc/rtc_cntl_reg.h"
#endif

#include "dispatcher.hpp"
#include "format.hpp"
#include "stream_frame.hpp"

#include "cart.hpp"
#include "devlink.hpp"
#include "gui.hpp"
#include "settings.hpp"

/// Work for the main loop (the task that owns the launcher and the running
/// cart), posted from other tasks; run() waits for it to be done.
class MainQueue {
public:
  /// Run `fn` on the main loop and wait for it. If the main loop does not
  /// get to it within `timeout` it is dropped (never run later) and run()
  /// returns false; once started it is always waited for.
  bool run(std::function<void()> fn, std::chrono::milliseconds timeout) {
    auto item = std::make_shared<Item>();
    item->fn = std::move(fn);
    std::unique_lock<std::mutex> lk(mutex_);
    queue_.push_back(item);
    if (!cv_.wait_for(lk, timeout, [&] { return item->state != Item::QUEUED; })) {
      item->state = Item::CANCELLED;
      return false;
    }
    cv_.wait(lk, [&] { return item->state == Item::DONE; });
    return true;
  }
  /// Run `fn` on the main loop without waiting for it.
  void post(std::function<void()> fn) {
    auto item = std::make_shared<Item>();
    item->fn = std::move(fn);
    std::lock_guard<std::mutex> lk(mutex_);
    queue_.push_back(item);
  }
  /// Called by the main loop: run what was posted.
  void poll() {
    std::unique_lock<std::mutex> lk(mutex_);
    while (!queue_.empty()) {
      auto item = queue_.front();
      queue_.pop_front();
      if (item->state == Item::CANCELLED) continue;
      item->state = Item::RUNNING;
      cv_.notify_all();
      lk.unlock();
      item->fn();
      lk.lock();
      item->state = Item::DONE;
      cv_.notify_all();
    }
  }

protected:
  struct Item {
    enum State { QUEUED, RUNNING, DONE, CANCELLED } state{QUEUED};
    std::function<void()> fn;
  };
  std::mutex mutex_;
  std::condition_variable cv_;
  std::deque<std::shared_ptr<Item>> queue_;
};

class RemoteControl {
public:
  static constexpr uint8_t kModule = 0x20;
  static constexpr const char *kProtocol = "emu.control";
  static constexpr uint16_t kProtocolVersion = 2; // 2: in-memory screenshots

  enum class Type : uint8_t {
    STATUS = 0x01,     ///< () -> {state, rom, platform, console, heap, uptime, ...}
    LIST_ROMS = 0x02,  ///< (offset: text) -> {total, roms: [{name, path, platform, console}]}
    LAUNCH = 0x03,     ///< (rom path or part of a name / path) -> {rom}
    QUIT = 0x04,       ///< ()
    BUTTONS = 0x05,    ///< (u16 buttons, u16 hold ms; 0 = until the next BUTTONS)
    SAVE = 0x06,       ///< (u8 slot)
    LOAD = 0x07,       ///< (u8 slot)
    /// (path: text, optional). With a path: saved there on the card as
    /// [u16be w][u16be h][RGB565] -> {path}. Without: -> {width, height, size,
    /// format: rgb565le}, and the game holds still (its frame buffer with it)
    /// while SCREENSHOT_READ reads the pixels, until the last of them is read.
    SCREENSHOT = 0x08,
    USB_DRIVE = 0x09,  ///< (u8 1 = the PC gets the SD card, 0 = the device takes it back)
    RELOAD_ROMS = 0x0A, ///< () re-read metadata.csv
    ENTER_UPDATER = 0x0B, ///< () reboot into the updater app (ota_1) for an update
    SCREENSHOT_READ = 0x0C, ///< (u32 offset, u16 length) -> raw bytes of the held SCREENSHOT (short at the end)
    FILE_LIST = 0x10,  ///< (path[\0offset]) -> {entries: [{name, dir, size}], truncated?, next?}
    FILE_READ = 0x11,  ///< (u32 offset, u16 length, path) -> raw bytes (short at the end)
    FILE_WRITE = 0x12, ///< (u32 offset, path_len u16, path, data); offset 0 truncates
    FILE_DELETE = 0x13, ///< (path: text)
    MKDIR = 0x14,      ///< (path: text) (with parents)
    FILE_STAT = 0x15,  ///< (path: text) -> {size, dir}
    FILE_CLOSE = 0x16, ///< () flush and close the file being written
  };

  /// Where the main loop is, for STATUS.
  enum class State { MENU, GAME };

  RemoteControl(Gui &gui, MainQueue &main_queue)
      : gui_(gui)
      , main_(main_queue) {
    // the launcher's drive switch can hand the card over between two FILE_WRITEs
    BoxEmu::get().set_before_sd_handover([this] { close_file(); });
  }
  ~RemoteControl() { BoxEmu::get().set_before_sd_handover(nullptr); }

  uint8_t module_id() const { return kModule; }
  espp::DispatcherModuleInfo module_info() const {
    return {.name = "Emulator control",
            .app = "",
            .description = "Launch games, input, save states, screenshots, SD card files",
            .protocol = kProtocol,
            .protocol_version = kProtocolVersion};
  }

  /// The main loop tells us what is running (nullptr: the launcher).
  void set_cart(Cart *cart) {
    std::lock_guard<std::mutex> lk(state_mutex_);
    cart_ = cart;
  }

  void handle(const espp::stream_frame::Frame &frame) {
    if (frame.is_reply() || frame.module != kModule) {
      return;
    }
    std::vector<uint8_t> body;
    bool ok = true;
    const std::string_view text(reinterpret_cast<const char *>(frame.payload.data()), frame.payload.size());
    // the first error is the one reported
    auto fail = [&](std::string_view message) {
      if (!ok) return;
      ok = false;
      const auto j = fmt::format("{{\"error\":{}}}", quote(message));
      body.assign(j.begin(), j.end());
    };
    auto json = [&](const std::string &j) {
      if (ok) body.assign(j.begin(), j.end());
    };
    // the main loop does not take work while it waits in the pause menu
    auto on_main = [&](std::function<void()> fn, std::chrono::milliseconds timeout) {
      if (main_.run(std::move(fn), timeout)) return true;
      fail("the device is busy (is the pause menu open?)");
      return false;
    };
    const auto &p = frame.payload;
    // Held for the card's file requests: the card is handed over (by USB_DRIVE
    // or the launcher's drive switch) under this lock, so it is not unmounted
    // during one, and while the PC has it they are refused.
    std::unique_lock<std::recursive_mutex> card_lock(BoxEmu::lvgl_mutex(), std::defer_lock);
    switch (static_cast<Type>(frame.type)) {
    case Type::FILE_LIST:
    case Type::FILE_READ:
    case Type::FILE_WRITE:
    case Type::FILE_DELETE:
    case Type::MKDIR:
    case Type::FILE_STAT:
    case Type::FILE_CLOSE:
      card_lock.lock();
      if (BoxEmu::get().usb_msc_host_has_card()) {
        fail("the PC has the SD card (emulink drive off)");
        reply(frame, ok, body);
        return;
      }
      break;
    default:
      break;
    }
    switch (static_cast<Type>(frame.type)) {
    case Type::STATUS:
      json(status());
      break;
    case Type::LIST_ROMS: {
      const auto roms = gui_.rom_infos();
      size_t offset = 0;
      if (!text.empty()) offset = std::strtoul(std::string(text).c_str(), nullptr, 10);
      std::string j = fmt::format("{{\"total\":{},\"offset\":{},\"roms\":[", roms.size(), offset);
      size_t i = offset;
      for (; i < roms.size(); i++) {
        const auto &r = roms[i];
        auto e = fmt::format("{}{{\"name\":{},\"path\":{},\"platform\":\"{}\",\"console\":{}}}",
                             i > offset ? "," : "", quote(r.name), quote(r.rom_path), r.platform,
                             quote(console_info(console_of(r.platform)).name));
        // (room for the trailer, `],"next":N}`, and reply()'s status byte)
        if (j.size() + e.size() + 24 > espp::stream_frame::kMaxPayloadSize) break; // the host asks for the rest
        j += e;
      }
      j += fmt::format("],\"next\":{}}}", i);
      json(j);
      break;
    }
    case Type::LAUNCH: {
      if (current_cart()) {
        fail("a game is running; QUIT first");
        break;
      }
      const std::string match(text);
      bool found = false, drive = false;
      on_main([&] {
        // (the PC has the card: the ROM is not there to load)
        if ((drive = BoxEmu::get().usb_msc_host_has_card())) return;
        found = select(match) && (gui_.ready_to_play(true), true);
      }, std::chrono::seconds(2));
      if (drive) fail("the PC has the SD card (emulink drive off)");
      else if (!found) fail(fmt::format("no game matches '{}'", match));
      else json(fmt::format("{{\"rom\":{}}}", quote(gui_.get_selected_rom()->rom_path)));
      break;
    }
    case Type::QUIT: {
      bool quit = false;
      on_main([&] {
        if (auto *cart = current_cart()) {
          cart->request_quit();
          quit = true;
        }
      }, std::chrono::seconds(5));
      if (!quit) fail("no game is running");
      break;
    }
    case Type::BUTTONS: {
      if (p.size() < 4) {
        fail("payload: u16 buttons, u16 hold_ms");
        break;
      }
      const uint16_t buttons = p[0] | (p[1] << 8);
      const uint16_t hold_ms = p[2] | (p[3] << 8);
      BoxEmu::get().set_remote_buttons(buttons);
      if (hold_ms) {
        // held for hold_ms, then released (this task answers afterwards)
        std::this_thread::sleep_for(std::chrono::milliseconds(hold_ms));
        BoxEmu::get().set_remote_buttons(0);
      }
      break;
    }
    case Type::SAVE:
    case Type::LOAD: {
      if (p.empty()) {
        fail("payload: u8 slot");
        break;
      }
      const int slot = p[0];
      // (the menu wraps an out-of-range slot round to 0)
      if (slot > (int)Menu::MAX_SLOT) {
        fail(fmt::format("slot {} out of range (0-{})", slot, Menu::MAX_SLOT));
        break;
      }
      const bool save = static_cast<Type>(frame.type) == Type::SAVE;
      bool done = false, has_cart = false;
      on_main([&] {
        if (auto *cart = current_cart()) {
          has_cart = true;
          if (save) {
            cart->save_from_host(slot);
            done = true;
          } else {
            done = cart->load_from_host(slot);
          }
        }
      }, std::chrono::seconds(30));
      if (!has_cart) fail("no game is running");
      else if (!done) fail(fmt::format("slot {} has no state", slot));
      break;
    }
    case Type::SCREENSHOT: {
      std::string path(text);
      bool done = false, has_cart = false;
      if (!path.empty()) {
        // the old way: a file on the card
        on_main([&] {
          if (auto *cart = current_cart()) {
            has_cart = true;
            std::error_code ec;
            std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
            done = cart->screenshot(path);
          }
        }, std::chrono::seconds(10));
        if (!has_cart) fail("no game is running");
        else if (!done) fail("screenshot failed");
        else json(fmt::format("{{\"path\":{}}}", quote(path)));
        break;
      }
      // The frame is read where the game drew it (for most cores one of the
      // BSP's two frame buffers): the main loop takes the pointer between two
      // frames and then holds, so the game draws nothing over it, until the
      // last SCREENSHOT_READ (or a pause in the reading) lets it go on.
      release_shot();
      size_t width = 0, height = 0;
      {
        std::unique_lock<std::mutex> lk(shot_mutex_);
        shot_state_ = ShotState::CAPTURING;
      }
      main_.post([this] {
        std::unique_lock<std::mutex> lk(shot_mutex_);
        if (shot_state_ != ShotState::CAPTURING) return; // the request gave up
        auto *cart = current_cart();
        if (cart) {
          cart->capture_hold(true);
          auto [w, h] = cart->frame_size();
          const auto pixels = cart->frame_pixels();
          // a core may report more lines than its frame buffer holds (the
          // box's NES: 240 for a 224-line frame): keep what is there
          if (w) h = std::min(h, pixels.size() / (w * 2));
          shot_width_ = w;
          shot_height_ = h;
          shot_pixels_ = pixels.data();
          shot_size_ = w * h * 2;
        }
        shot_state_ = (cart && shot_size_) ? ShotState::HELD : ShotState::IDLE;
        has_shot_cart_ = cart != nullptr;
        shot_cv_.notify_all();
        // hold the game while the frame is read
        shot_deadline_ = std::chrono::steady_clock::now() + kShotIdle;
        while (shot_state_ == ShotState::HELD && std::chrono::steady_clock::now() < shot_deadline_) {
          shot_cv_.wait_until(lk, shot_deadline_);
        }
        shot_state_ = ShotState::IDLE;
        shot_pixels_ = nullptr;
        shot_cv_.notify_all();
        lk.unlock();
        if (cart) cart->capture_hold(false);
      });
      std::unique_lock<std::mutex> lk(shot_mutex_);
      if (!shot_cv_.wait_for(lk, std::chrono::seconds(10), [&] { return shot_state_ != ShotState::CAPTURING; })) {
        shot_state_ = ShotState::IDLE; // the posted job skips it
        fail("the device is busy (is the pause menu open?)");
        break;
      }
      if (!has_shot_cart_) {
        fail("no game is running");
        break;
      }
      if (shot_state_ != ShotState::HELD) {
        fail("this game has no frame to capture");
        break;
      }
      width = shot_width_;
      height = shot_height_;
      json(fmt::format("{{\"width\":{},\"height\":{},\"size\":{},\"format\":\"{}\"}}", width, height,
                       shot_size_, kFrameFormat));
      break;
    }
    case Type::SCREENSHOT_READ: {
      if (p.size() < 6) {
        fail("payload: u32 offset, u16 length");
        break;
      }
      const uint32_t offset = p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
      const size_t length = std::min<size_t>(p[4] | (p[5] << 8), espp::stream_frame::kMaxPayloadSize - 1);
      std::lock_guard<std::mutex> lk(shot_mutex_);
      if (shot_state_ != ShotState::HELD || !shot_pixels_) {
        fail("no screenshot is being read (SCREENSHOT first)");
        break;
      }
      if (offset < shot_size_) {
        const size_t n = std::min<size_t>(length, shot_size_ - offset);
        body.assign(shot_pixels_ + offset, shot_pixels_ + offset + n);
      }
      if (offset + length >= shot_size_) {
        shot_state_ = ShotState::RELEASED; // all read: the game goes on
      } else {
        shot_deadline_ = std::chrono::steady_clock::now() + kShotIdle;
      }
      shot_cv_.notify_all();
      break;
    }
    case Type::USB_DRIVE: {
      const bool host = !p.empty() && p[0];
      const bool closed = close_file();
      bool done = false;
      on_main([&] {
        if (current_cart() && host) return; // a running game uses the card
        if (host) {
          done = BoxEmu::get().initialize_usb_msc();
        } else {
          BoxEmu::get().deinitialize_usb_msc();
          done = !BoxEmu::get().usb_msc_host_has_card();
          if (done) gui_.reload_rom_list();
        }
      }, std::chrono::seconds(10));
      if (!closed) fail("the file being written could not be closed (card full?)");
      if (!done) fail(host ? "could not hand the SD card over (a game running?)" : "could not take the SD card back");
      break;
    }
    case Type::RELOAD_ROMS:
      close_file();
      on_main([&] { gui_.reload_rom_list(); }, std::chrono::seconds(5));
      json(fmt::format("{{\"total\":{}}}", gui_.rom_infos().size()));
      break;
    case Type::ENTER_UPDATER: {
#if DEVLINK_HAS_OTA
      fail("this device updates in place (the OTA service, module 0)");
      break;
#endif
      if (!boot_updater_next()) {
        fail("no updater app (ota_1), or otadata could not be written");
        break;
      }
      close_file();
      // answer first, then restart
      reply(frame, true, body);
      std::this_thread::sleep_for(std::chrono::milliseconds(300));
      esp_restart();
      return;
    }
    case Type::FILE_LIST: {
      // path, optionally followed by '\0' and the index to start at (the "next" of a truncated
      // reply)
      std::string_view path = text;
      size_t offset = 0;
      if (const auto nul = text.find('\0'); nul != std::string_view::npos) {
        path = text.substr(0, nul);
        offset = std::strtoul(std::string(text.substr(nul + 1)).c_str(), nullptr, 10);
      }
      std::error_code ec;
      std::string j = "{\"entries\":[";
      size_t i = 0;
      for (const auto &e : std::filesystem::directory_iterator(std::string(path), ec)) {
        if (i < offset) {
          i++;
          continue;
        }
        std::error_code ec2;
        const bool dir = e.is_directory(ec2);
        const uintmax_t size = dir ? 0 : e.file_size(ec2);
        auto item = fmt::format("{}{{\"name\":{},\"dir\":{},\"size\":{}}}", i > offset ? "," : "",
                                quote(e.path().filename().string()), dir, size);
        if (j.size() + item.size() + 48 > espp::stream_frame::kMaxPayloadSize) {
          j += fmt::format("],\"truncated\":true,\"next\":{}}}", i);
          json(j);
          goto done;
        }
        j += item;
        i++;
      }
      if (ec) {
        fail(fmt::format("{}: {}", path, ec.message()));
        break;
      }
      j += "]}";
      json(j);
    done:
      break;
    }
    case Type::FILE_READ: {
      if (p.size() < 7) {
        fail("payload: u32 offset, u16 length, path");
        break;
      }
      const uint32_t offset = p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
      const size_t length = std::min<size_t>(p[4] | (p[5] << 8), espp::stream_frame::kMaxPayloadSize - 1);
      const std::string path(reinterpret_cast<const char *>(p.data() + 6), p.size() - 6);
      close_file(path);
      FILE *f = fopen(path.c_str(), "rb");
      if (!f) {
        fail(fmt::format("cannot open {}", path));
        break;
      }
      if (fseek(f, offset, SEEK_SET) != 0) {
        fclose(f);
        fail(fmt::format("cannot seek {} to {}", path, offset));
        break;
      }
      body.resize(length);
      body.resize(fread(body.data(), 1, length, f));
      if (ferror(f)) fail(fmt::format("read error in {} at {}", path, offset));
      fclose(f);
      break;
    }
    case Type::FILE_WRITE: {
      if (p.size() < 6) {
        fail("payload: u32 offset, u16 path_len, path, data");
        break;
      }
      const uint32_t offset = p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
      const size_t path_len = p[4] | (p[5] << 8);
      if (p.size() < 6 + path_len) {
        fail("path_len past the end");
        break;
      }
      const std::string path(reinterpret_cast<const char *>(p.data() + 6), path_len);
      const uint8_t *data = p.data() + 6 + path_len;
      const size_t n = p.size() - 6 - path_len;
      if (!open_for_write(path, offset)) {
        fail(fmt::format("cannot write {} at {}", path, offset));
        break;
      }
      if (fwrite(data, 1, n, write_file_) != n) {
        close_file();
        fail(fmt::format("write to {} failed (card full?)", path));
        break;
      }
      write_pos_ = offset + n;
      break;
    }
    case Type::FILE_CLOSE:
      // (a buffered write that fails shows up here)
      if (!close_file()) fail("closing the file failed (card full?)");
      break;
    case Type::FILE_DELETE: {
      const std::string path(text);
      close_file(path);
      struct stat st;
      if (stat(path.c_str(), &st) != 0) {
        fail(fmt::format("{} does not exist", path));
      } else if (S_ISDIR(st.st_mode)) {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
        if (ec) fail(fmt::format("cannot delete {}: {}", path, ec.message()));
      } else if (unlink(path.c_str()) != 0) {
        fail(fmt::format("cannot delete {}: {}", path, strerror(errno)));
      }
      break;
    }
    case Type::MKDIR: {
      std::error_code ec;
      std::filesystem::create_directories(std::string(text), ec);
      if (ec) fail(fmt::format("{}: {}", text, ec.message()));
      break;
    }
    case Type::FILE_STAT: {
      struct stat st;
      if (stat(std::string(text).c_str(), &st) != 0) {
        fail(fmt::format("{} does not exist", text));
        break;
      }
      json(fmt::format("{{\"size\":{},\"dir\":{}}}", (uintmax_t)st.st_size, S_ISDIR(st.st_mode)));
      break;
    }
    default:
      fail(fmt::format("unknown request type {:#x}", frame.type));
      break;
    }
    if (card_lock.owns_lock()) card_lock.unlock();
    reply(frame, ok, body);
  }

protected:
  static std::string quote(std::string_view s) {
    std::string out = "\"";
    for (char c : s) {
      switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if ((unsigned char)c < 0x20) out += fmt::format("\\u{:04x}", (unsigned char)c);
        else out += c;
      }
    }
    return out + "\"";
  }

  static void reply(const espp::stream_frame::Frame &request, bool ok, const std::vector<uint8_t> &body) {
    std::vector<uint8_t> payload;
    payload.reserve(body.size() + 1);
    payload.push_back(ok ? 0 : 1);
    payload.insert(payload.end(), body.begin(), body.end());
    const auto frame = espp::stream_frame::build_frame(true, kModule, request.type, payload, request.correlation);
    DevLink::send(frame);
  }

  Cart *current_cart() {
    std::lock_guard<std::mutex> lk(state_mutex_);
    return cart_;
  }

  bool select(const std::string &match) {
    // an exact path first, then a part of a name / path
    const auto roms = gui_.rom_infos();
    for (const auto &r : roms) {
      if (r.rom_path == match || r.rom_path == std::string(BoxEmu::mount_point) + "/" + match) {
        return gui_.select_rom_matching(r.rom_path);
      }
    }
    return gui_.select_rom_matching(match);
  }

  std::string status() {
    std::string rom, platform, console;
    bool in_game = false;
    uint32_t frames = 0;
    {
      std::lock_guard<std::mutex> lk(state_mutex_);
      if (cart_) {
        in_game = true;
        const auto &info = cart_->rom_info();
        rom = info.rom_path;
        platform = fmt::format("{}", info.platform);
        console = console_info(console_of(info.platform)).name;
        frames = cart_->game_frames();
      }
    }
    const auto *app = esp_app_get_description();
    const auto *running = esp_ota_get_running_partition();
    return fmt::format("{{\"state\":\"{}\",\"rom\":{},\"platform\":\"{}\",\"console\":{},\"game_frames\":{},"
                       "\"frames_presented\":{},\"frames_dropped\":{},\"usb_drive\":{},"
                       "\"uptime_ms\":{},\"internal_free\":{},\"internal_largest\":{},\"internal_min_free\":{},"
                       "\"psram_free\":{},\"psram_largest\":{},\"psram_min_free\":{},\"dma_free\":{},"
                       "\"dma_largest\":{},\"firmware\":{},\"built\":{},"
                       "\"partition\":{},\"elf_sha256\":{},\"reset_reason\":{},\"last_panic\":{}}}",
                       in_game ? "game" : "menu", quote(rom), platform, quote(console), frames,
                       BoxEmu::get().frames_blitted(), BoxEmu::get().frames_dropped(),
                       BoxEmu::get().usb_msc_host_has_card(), esp_timer_get_time() / 1000,
                       heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                       heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
                       heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
                       heap_caps_get_free_size(MALLOC_CAP_SPIRAM), heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM),
                       heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM),
                       heap_caps_get_free_size(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL),
                       heap_caps_get_largest_free_block(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL), quote(app->version),
                       quote(fmt::format("{} {}", app->date, app->time)), quote(running ? running->label : ""),
                       quote(elf_sha256()), (int)esp_reset_reason(), quote(DevLink::last_panic()));
  }

  /// Make the bootloader start the updater (ota_1) next. esp_ota_set_boot_partition()
  /// first verifies the image through a flash mapping, which is unreliable in an
  /// app that runs its code from PSRAM (esp-box-emu: the mapping can land on the
  /// PSRAM heap's addresses), so the otadata entry is written here directly, with
  /// plain SPI flash reads / writes: the same entry esp_ota_ops would write
  /// (boot slot = (seq - 1) % slots). The updater, which has no PSRAM, verifies
  /// and sets the boot partition the usual way.
  static bool boot_updater_next() {
    const esp_partition_t *updater =
        esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, nullptr);
    const esp_partition_t *otadata =
        esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, nullptr);
    if (!updater || !otadata) return false;
    const uint32_t slots = esp_ota_get_app_partition_count();
    const uint32_t sector = otadata->erase_size;
    esp_ota_select_entry_t e[2];
    for (int i = 0; i < 2; i++) {
      if (esp_partition_read(otadata, i * sector, &e[i], sizeof(e[i])) != ESP_OK) return false;
    }
    const bool v0 = bootloader_common_ota_select_valid(&e[0]), v1 = bootloader_common_ota_select_valid(&e[1]);
    const uint32_t seq = std::max(v0 ? e[0].ota_seq : 0, v1 ? e[1].ota_seq : 0);
    uint32_t next = seq + 1;
    while ((next - 1) % slots != 1) next++; // slot 1: the updater
    // overwrite the older (or an invalid) entry, so the current one survives a failure
    const int target = !v0 ? 0 : !v1 ? 1 : (e[0].ota_seq <= e[1].ota_seq ? 0 : 1);
    esp_ota_select_entry_t entry;
    memset(&entry, 0xFF, sizeof(entry));
    entry.ota_seq = next;
    entry.ota_state = ESP_OTA_IMG_VALID;
    entry.crc = bootloader_common_ota_select_crc(&entry);
    esp_ota_select_entry_t check;
    if (esp_partition_erase_range(otadata, target * sector, sector) == ESP_OK &&
        esp_partition_write(otadata, target * sector, &entry, sizeof(entry)) == ESP_OK &&
        esp_partition_read(otadata, target * sector, &check, sizeof(check)) == ESP_OK &&
        memcmp(&check, &entry, sizeof(entry)) == 0) {
      return true;
    }
#if CONFIG_IDF_TARGET_ESP32S3
    // esp-box-emu: with the flash and PSRAM at 120 MHz the app's flash writes
    // do not land; the bootloader (bootloader_components/updater_switch)
    // writes the entry when it finds this, at the next reset
    REG_WRITE(RTC_CNTL_STORE0_REG, 0x55504454u); // "UPDT"
    return true;
#else
    return false;
#endif
  }

  static std::string elf_sha256() {
    char sha[17] = {};
    esp_app_get_elf_sha256(sha, sizeof(sha));
    return sha;
  }

  // FILE_WRITE keeps the file open between chunks
  bool open_for_write(const std::string &path, uint32_t offset) {
    if (write_file_ && path == write_path_ && offset == write_pos_) {
      return true;
    }
    if (!close_file()) return false; // (the previous file's last data did not land)
    if (offset == 0) {
      std::error_code ec;
      std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
      write_file_ = fopen(path.c_str(), "wb");
    } else {
      write_file_ = fopen(path.c_str(), "r+b");
      if (write_file_ && fseek(write_file_, offset, SEEK_SET) != 0) {
        close_file();
      }
    }
    if (!write_file_) return false;
    write_path_ = path;
    write_pos_ = offset;
    return true;
  }
  /// false: the file's buffered data could not be written
  bool close_file(const std::string &only_if_path = "") {
    // (the handover hook calls this from the task that hands the card over)
    std::lock_guard<std::recursive_mutex> lk(BoxEmu::lvgl_mutex());
    if (write_file_ && (only_if_path.empty() || only_if_path == write_path_)) {
      const bool ok = fclose(write_file_) == 0;
      write_file_ = nullptr;
      write_path_.clear();
      return ok;
    }
    return true;
  }

  // the pixels' byte order in the cores' frame buffers (both devices)
  static constexpr const char *kFrameFormat = "rgb565le";
  // how long the game is held for a screenshot without a SCREENSHOT_READ
  static constexpr std::chrono::milliseconds kShotIdle{2000};
  enum class ShotState { IDLE, CAPTURING, HELD, RELEASED };

  /// Let the game go on if a screenshot is held (a new one replaces it).
  void release_shot() {
    std::unique_lock<std::mutex> lk(shot_mutex_);
    if (shot_state_ == ShotState::HELD) shot_state_ = ShotState::RELEASED;
    shot_cv_.notify_all();
    shot_cv_.wait_for(lk, std::chrono::seconds(1), [&] { return shot_state_ == ShotState::IDLE; });
  }

  Gui &gui_;
  MainQueue &main_;
  std::mutex state_mutex_;
  Cart *cart_{nullptr};
  // the screenshot being read: the held game's frame buffer
  std::mutex shot_mutex_;
  std::condition_variable shot_cv_;
  ShotState shot_state_{ShotState::IDLE};
  std::chrono::steady_clock::time_point shot_deadline_;
  const uint8_t *shot_pixels_{nullptr};
  size_t shot_width_{0}, shot_height_{0}, shot_size_{0};
  bool has_shot_cart_{false};
  FILE *write_file_{nullptr};
  std::string write_path_;
  uint32_t write_pos_{0};
};
