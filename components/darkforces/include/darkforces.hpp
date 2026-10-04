#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

/// Start Dark Forces. \p gob_filename is the full path to DARK.GOB; the game
/// data (DARK.GOB, SOUNDS.GOB, SPRITES.GOB, TEXTURES.GOB and the LFD/ folder)
/// must live in the same directory, which is also used for settings and saves.
void init_darkforces(const std::string& gob_filename, uint8_t *romdata, size_t rom_data_size);
void reset_darkforces();
void run_darkforces_rom();
void deinit_darkforces();
/// True once the player has quit the game from its own menu.
bool darkforces_quit_requested();
void pause_darkforces_tasks();
void resume_darkforces_tasks();
void load_darkforces(std::string_view save_path, int save_slot);
void save_darkforces(std::string_view save_path, int save_slot);
std::span<uint8_t> get_darkforces_video_buffer();
