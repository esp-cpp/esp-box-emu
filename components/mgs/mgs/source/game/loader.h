#ifndef __MGS_GAME_LOADER_H__
#define __MGS_GAME_LOADER_H__

void *NewLoader(const char *dir);

#ifdef __psyz
/* game/loader.c -- the stage a requested name resolves to on this board, or
 * NULL when it is not packed. Both the script command that starts a stage
 * change and the loader itself must agree, so they share this one answer. */
const char *Mgs_ResolveStage(const char *dir);
#endif

#endif // __MGS_GAME_LOADER_H__
