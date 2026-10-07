/* Save states for femto8 on esp-box-emu.
 *
 * A state is PICO-8's RAM, the overlay, the frame counters, the audio
 * channels and the cart's Lua heap. The heap is serialized with Eris (Lua
 * 5.2's heavy-duty persistence, the approach fake-08 uses): everything the
 * cart created or replaced in _G is persisted as a graph; the API functions
 * and library tables that existed before the cart ran are "permanent" and
 * referenced by name (p8_state_init builds that table right after the API is
 * registered). Closures, upvalues, metatables and coroutines are persisted
 * with the graph.
 *
 * Both operations must run while no cart code is executing -- between
 * frames, from p8_espbox_frame_boundary() -- never from the Lua hook.
 */
#include "p8_state.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"

#include "lua.h"
#include "lauxlib.h"
#include "eris.h"
#include "p8_emu.h"
#include "p8_audio.h"

#define STATE_MAGIC 0x31533850u /* "P8S1" */
#define PERMS_KEY   "__PICO8_PERMS"
#define UNPERMS_KEY "__PICO8_UNPERMS"
#define SANDBOX_KEY "__PICO8_SANDBOX"

typedef struct {
    uint32_t magic;
    uint32_t frames;
    uint32_t fps;
    uint32_t audio_size;
    uint32_t lua_size;
} state_header_t;

lua_State *p8_lua_state(void); /* p8_lua.c */

/* perms[object] = "name" / "table.name", unperms the reverse; the sandbox
 * (name -> original value) already exists for the _ENV fallback */
void p8_state_init(lua_State *L)
{
    int top = lua_gettop(L);
    lua_newtable(L);                       /* perms */
    int perms = lua_gettop(L);
    lua_newtable(L);                       /* unperms */
    int unperms = lua_gettop(L);
    lua_pushglobaltable(L);
    int g = lua_gettop(L);
    lua_pushnil(L);
    while (lua_next(L, g) != 0) {          /* key -2, value -1 */
        if (lua_type(L, -2) == LUA_TSTRING) {
            int vt = lua_type(L, -1);
            if (vt == LUA_TFUNCTION || vt == LUA_TTABLE || vt == LUA_TUSERDATA) {
                /* the global itself */
                lua_pushvalue(L, -1); lua_pushvalue(L, -3); lua_rawset(L, perms);
                lua_pushvalue(L, -2); lua_pushvalue(L, -2); lua_rawset(L, unperms);
            }
            if (vt == LUA_TTABLE) {
                /* one level down: library functions a cart may have captured
                 * (local sub = string.sub) */
                const char *tname = lua_tostring(L, -2);
                lua_pushnil(L);
                while (lua_next(L, -2) != 0) {
                    if (lua_type(L, -2) == LUA_TSTRING && lua_type(L, -1) == LUA_TFUNCTION) {
                        lua_pushfstring(L, "%s.%s", tname, lua_tostring(L, -2));
                        /* perms[func] = name */
                        lua_pushvalue(L, -2); lua_pushvalue(L, -2); lua_rawset(L, perms);
                        /* unperms[name] = func */
                        lua_pushvalue(L, -1); lua_pushvalue(L, -3); lua_rawset(L, unperms);
                        lua_pop(L, 1);
                    }
                    lua_pop(L, 1);
                }
            }
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);                         /* globals */
    lua_setfield(L, LUA_REGISTRYINDEX, UNPERMS_KEY);
    lua_setfield(L, LUA_REGISTRYINDEX, PERMS_KEY);
    lua_settop(L, top);
}

/* --- Lua side, run through pcall so an Eris error is reported, not fatal --- */

static int do_persist(lua_State *L)
{
    lua_getfield(L, LUA_REGISTRYINDEX, PERMS_KEY);
    int perms = lua_gettop(L);
    lua_newtable(L);                       /* what the cart created or replaced */
    int changed = lua_gettop(L);
    lua_getfield(L, LUA_REGISTRYINDEX, SANDBOX_KEY);
    int sandbox = lua_gettop(L);
    lua_pushglobaltable(L);
    int g = lua_gettop(L);
    lua_pushnil(L);
    while (lua_next(L, g) != 0) {
        lua_pushvalue(L, -2);
        lua_rawget(L, sandbox);
        int same = lua_rawequal(L, -1, -2);
        lua_pop(L, 1);
        if (!same) {
            lua_pushvalue(L, -2); lua_pushvalue(L, -2); lua_rawset(L, changed);
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 2);                         /* globals, sandbox */
    eris_persist(L, perms, changed);       /* pushes the blob */
    return 1;
}

static int do_unpersist(lua_State *L)
{
    /* arg 1: the blob */
    lua_getfield(L, LUA_REGISTRYINDEX, UNPERMS_KEY);
    int unperms = lua_gettop(L);
    eris_unpersist(L, unperms, 1);         /* pushes the table of globals */
    int restored = lua_gettop(L);
    lua_getfield(L, LUA_REGISTRYINDEX, SANDBOX_KEY);
    int sandbox = lua_gettop(L);
    lua_pushglobaltable(L);
    int g = lua_gettop(L);
    /* drop every global the cart created since (not in the original set and
     * not in the state): collect the keys first, then clear them */
    lua_newtable(L);
    int stale = lua_gettop(L);
    int n = 0;
    lua_pushnil(L);
    while (lua_next(L, g) != 0) {
        lua_pushvalue(L, -2); lua_rawget(L, sandbox);
        int original = !lua_isnil(L, -1);
        lua_pop(L, 1);
        lua_pushvalue(L, -2); lua_rawget(L, restored);
        int in_state = !lua_isnil(L, -1);
        lua_pop(L, 1);
        if (!original && !in_state) {
            lua_pushvalue(L, -2); lua_rawseti(L, stale, ++n);
        }
        lua_pop(L, 1);
    }
    for (int i = 1; i <= n; i++) {
        lua_rawgeti(L, stale, i); lua_pushnil(L); lua_rawset(L, g);
    }
    /* originals the cart had replaced go back to the saved values; the
     * state table carries both the replaced originals and the new globals */
    lua_pushnil(L);
    while (lua_next(L, restored) != 0) {
        lua_pushvalue(L, -2); lua_pushvalue(L, -2); lua_rawset(L, g);
        lua_pop(L, 1);
    }
    return 0;
}

int p8_state_save(const char *path)
{
    lua_State *L = p8_lua_state();
    if (!L || !m_memory)
        return -1;
    int top = lua_gettop(L);
    lua_pushcfunction(L, do_persist);
    if (lua_pcall(L, 0, 1, 0) != 0) {
        printf("[PICO8] save state: %s\n", lua_tostring(L, -1));
        lua_settop(L, top);
        return -1;
    }
    size_t blob_len = 0;
    const char *blob = lua_tolstring(L, -1, &blob_len);

    int ret = -1;
    FILE *f = fopen(path, "wb");
    if (f) {
        state_header_t h = { STATE_MAGIC, m_frames, m_fps, (uint32_t)audio_state_size(), (uint32_t)blob_len };
        void *audio = malloc(h.audio_size);
        if (audio) audio_state_save(audio);
        ret = (fwrite(&h, sizeof h, 1, f) == 1 &&
               fwrite(m_memory, 1, MEMORY_SIZE, f) == MEMORY_SIZE &&
               fwrite(m_overlay_memory, 1, MEMORY_SCREEN_SIZE, f) == MEMORY_SCREEN_SIZE &&
               audio && fwrite(audio, 1, h.audio_size, f) == h.audio_size &&
               fwrite(blob, 1, blob_len, f) == blob_len) ? 0 : -1;
        free(audio);
        fclose(f);
    }
    printf("[PICO8] save state '%s': %s (lua %u B)\n", path, ret == 0 ? "ok" : "FAILED", (unsigned)blob_len);
    lua_settop(L, top);
    return ret;
}

int p8_state_load(const char *path)
{
    lua_State *L = p8_lua_state();
    if (!L || !m_memory)
        return -1;
    FILE *f = fopen(path, "rb");
    if (!f) {
        printf("[PICO8] load state: cannot open '%s'\n", path);
        return -1;
    }
    state_header_t h;
    int ret = -1;
    char *blob = NULL;
    void *audio = NULL;
    uint8_t *mem = heap_caps_malloc(MEMORY_SIZE + MEMORY_SCREEN_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (fread(&h, sizeof h, 1, f) == 1 && h.magic == STATE_MAGIC && h.audio_size == audio_state_size() && mem &&
        fread(mem, 1, MEMORY_SIZE + MEMORY_SCREEN_SIZE, f) == MEMORY_SIZE + MEMORY_SCREEN_SIZE &&
        (audio = malloc(h.audio_size)) && fread(audio, 1, h.audio_size, f) == h.audio_size &&
        (blob = heap_caps_malloc(h.lua_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)) &&
        fread(blob, 1, h.lua_size, f) == h.lua_size) {
        /* the Lua heap first: if that fails nothing else has changed */
        int top = lua_gettop(L);
        lua_pushcfunction(L, do_unpersist);
        lua_pushlstring(L, blob, h.lua_size);
        if (lua_pcall(L, 1, 0, 0) != 0) {
            printf("[PICO8] load state: %s\n", lua_tostring(L, -1));
        } else {
            memcpy(m_memory, mem, MEMORY_SIZE);
            memcpy(m_overlay_memory, mem + MEMORY_SIZE, MEMORY_SCREEN_SIZE);
            audio_state_load(audio);
            m_frames = h.frames;
            m_fps = h.fps;
            lua_gc(L, LUA_GCCOLLECT, 0);  /* the previous heap is garbage now */
            ret = 0;
        }
        lua_settop(L, top);
    } else {
        printf("[PICO8] load state: bad file '%s'\n", path);
    }
    free(blob);
    free(audio);
    free(mem);
    fclose(f);
    printf("[PICO8] load state '%s': %s\n", path, ret == 0 ? "ok" : "FAILED");
    return ret;
}
