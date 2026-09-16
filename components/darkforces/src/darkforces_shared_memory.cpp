// Shared memory for the Dark Forces port.
//
// The largest statics of The Force Engine (and of the ESP32 platform layer) are
// pointers into the emulator's shared memory system rather than fixed arrays,
// so they only take up RAM while Dark Forces is running, like the other cores.
// Each owning file provides an espbox_shared_*(bool alloc) hook; see
// tfe/TFE_System/espboxShared.h.
#include "shared_memory.h"

#include <cstdio>

void espbox_shared_streams(bool alloc);
void espbox_shared_parser(bool alloc);
void espbox_shared_fm4(bool alloc);
void espbox_shared_midiPlayer(bool alloc);
void espbox_shared_lmusic(bool alloc);
void espbox_shared_weapon(bool alloc);
void espbox_shared_vfb(bool alloc);
void espbox_shared_settings(bool alloc);
void espbox_shared_saveSystem(bool alloc);
void espbox_shared_rwallFixed(bool alloc);
void espbox_shared_mission(bool alloc);
void espbox_shared_escapeMenu(bool alloc);
void espbox_shared_audio(bool alloc);
void espbox_shared_render(bool alloc);
void espbox_shared_regionStats(bool alloc);

static void (*const s_hooks[])(bool) =
{
	espbox_shared_streams,
	espbox_shared_parser,
	espbox_shared_fm4,
	espbox_shared_midiPlayer,
	espbox_shared_lmusic,
	espbox_shared_weapon,
	espbox_shared_vfb,
	espbox_shared_settings,
	espbox_shared_saveSystem,
	espbox_shared_rwallFixed,
	espbox_shared_mission,
	espbox_shared_escapeMenu,
	espbox_shared_audio,
	espbox_shared_render,
	espbox_shared_regionStats,
};

void darkforces_init_shared_memory()
{
	const size_t before = shared_num_bytes_allocated();
	for (auto hook : s_hooks) { hook(true); }
	printf("[DarkForces] shared memory: %u bytes\n", (unsigned)(shared_num_bytes_allocated() - before));
}

void darkforces_free_shared_memory()
{
	// Clear the pointers first so nothing can use them after the memory is released.
	for (auto hook : s_hooks) { hook(false); }
	shared_mem_clear();
}
