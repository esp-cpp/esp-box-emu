#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces Cutscene Player
// This gets initialized by the cutscene system and handles the actual
// playback.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "cutscene.h"

namespace TFE_DarkForces
{
	void cutscenePlayer_start(s32 scene);
	void cutscenePlayer_stop();
#ifdef TFE_ESPBOX
	// Stop a running cutscene sequence right away (as if it had reached SCENE_EXIT):
	// frees the current film and its actors and clears the cutscene memory.
	void cutscenePlayer_abort();
#endif

	// Returns JTRUE if we want to continue playing.
	// Note: this is a little different than the original code, which ran in a while loop until finished.
	JBool cutscenePlayer_update();
}  // TFE_DarkForces