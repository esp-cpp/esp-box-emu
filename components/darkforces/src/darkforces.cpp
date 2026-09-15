// Dark Forces (The Force Engine) glue for esp-box-emu.
//
// This mirrors what TheForceEngine/main.cpp (and the Amiga port's main) do:
// set up paths/settings/system/render/audio, create the game, and then run one
// iteration of the engine loop per call to run_darkforces_rom().
#include "darkforces.hpp"

#include "box-emu.hpp"
#include "statistics.hpp"

#include <TFE_System/types.h>
#include <TFE_System/system.h>
#include <TFE_Memory/memoryRegion.h>
#include <TFE_Game/igame.h>
#include <TFE_Game/saveSystem.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Audio/audioSystem.h>
#include <TFE_Audio/midiPlayer.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_Input/input.h>
#include <TFE_Input/inputMapping.h>
#include <TFE_Settings/settings.h>
#include <TFE_Jedi/Task/task.h>
#include <TFE_Jedi/IMuse/imuse.h>
#include <TFE_Jedi/Renderer/virtualFramebuffer.h>
#include <TFE_Jedi/Renderer/RClassic_Fixed/rclassicFixedSharedState.h>
#include "pool_allocator.h"
#include <TFE_FrontEndUI/frontEndUi.h>

#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <esp_rom_sys.h>
#include <sdkconfig.h>
#if CONFIG_ESP_TASK_WDT_EN
#include <esp_task_wdt.h>
#endif
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstring>
#include <chrono>
#include <thread>

using namespace TFE_Input;

namespace TFE_RenderBackend
{
	const uint16_t* getDisplayPalette();
}
namespace TFE_FrontEndUI
{
	bool exitToMenuRequested();
}
namespace TFE_DarkForces
{
	extern JBool s_palModified;
}

namespace
{
	constexpr int DF_WIDTH = 320;
	constexpr int DF_HEIGHT = 200;

	std::string s_gobPath;
	std::string s_gameDir;			// directory containing the GOB files, with trailing slash.
	IGame* s_curGame = nullptr;
	bool s_initialized = false;
	bool s_quit = false;
	bool s_paused = false;
	uint8_t* s_frameBuffer = nullptr;	// 8-bit 320x200 render target (BoxEmu frame buffer 0).
	const char* s_argv[2] = { "darkforces", nullptr };

	// Virtual mouse used to drive the (mouse based) menus with the d-pad.
	struct VirtualMouse
	{
		float x = DF_WIDTH / 2;
		float y = DF_HEIGHT / 2;
		float speed = 0.0f;
	};
	VirtualMouse s_mouse;

	// Controller bindings for the box gamepad. Unmodified buttons map to
	// controller buttons; SELECT acts as a modifier that re-routes buttons to
	// alternate controller buttons / keyboard keys (see updateInput()).
	InputBinding s_boxControllerBinds[] =
	{
		{ IADF_FORWARD,        ITYPE_CONTROLLER, CONTROLLER_BUTTON_DPAD_UP },
		{ IADF_BACKWARD,       ITYPE_CONTROLLER, CONTROLLER_BUTTON_DPAD_DOWN },
		{ IADF_TURN_LT,        ITYPE_CONTROLLER, CONTROLLER_BUTTON_DPAD_LEFT },
		{ IADF_TURN_RT,        ITYPE_CONTROLLER, CONTROLLER_BUTTON_DPAD_RIGHT },
		{ IADF_PRIMARY_FIRE,   ITYPE_CONTROLLER, CONTROLLER_BUTTON_A },
		{ IADF_JUMP,           ITYPE_CONTROLLER, CONTROLLER_BUTTON_B },
		{ IADF_USE,            ITYPE_CONTROLLER, CONTROLLER_BUTTON_X },
		{ IADF_CYCLEWPN_NEXT,  ITYPE_CONTROLLER, CONTROLLER_BUTTON_Y },
		{ IADF_MENU_TOGGLE,    ITYPE_CONTROLLER, CONTROLLER_BUTTON_START },
		// SELECT + d-pad left/right
		{ IADF_STRAFE_LT,      ITYPE_CONTROLLER, CONTROLLER_BUTTON_LEFTSHOULDER },
		{ IADF_STRAFE_RT,      ITYPE_CONTROLLER, CONTROLLER_BUTTON_RIGHTSHOULDER },
		// SELECT + down (hold)
		{ IADF_CROUCH,         ITYPE_CONTROLLER, CONTROLLER_BUTTON_LEFTSTICK },
		// SELECT + A
		{ IADF_SECONDARY_FIRE, ITYPE_CONTROLLER, CONTROLLER_BUTTON_RIGHTSTICK },
		// SELECT + Y
		{ IADF_CYCLEWPN_PREV,  ITYPE_CONTROLLER, CONTROLLER_BUTTON_GUIDE },
	};

	struct ButtonMap
	{
		GamepadState::Button pad;
		Button ctrl;		// controller button when SELECT is not held.
		Button altCtrl;		// controller button when SELECT is held (CONTROLLER_BUTTON_UNKNOWN = use altKey)
		KeyboardCode altKey;// keyboard key when SELECT is held.
	};
	// In-game mapping.
	const ButtonMap s_gameMap[] =
	{
		{ GamepadState::Button::UP,    CONTROLLER_BUTTON_DPAD_UP,    CONTROLLER_BUTTON_UNKNOWN,       KEY_TAB },	// automap
		{ GamepadState::Button::DOWN,  CONTROLLER_BUTTON_DPAD_DOWN,  CONTROLLER_BUTTON_LEFTSTICK,     KEY_UNKNOWN },	// crouch
		{ GamepadState::Button::LEFT,  CONTROLLER_BUTTON_DPAD_LEFT,  CONTROLLER_BUTTON_LEFTSHOULDER,  KEY_UNKNOWN },	// strafe left
		{ GamepadState::Button::RIGHT, CONTROLLER_BUTTON_DPAD_RIGHT, CONTROLLER_BUTTON_RIGHTSHOULDER, KEY_UNKNOWN },	// strafe right
		{ GamepadState::Button::A,     CONTROLLER_BUTTON_A,          CONTROLLER_BUTTON_RIGHTSTICK,    KEY_UNKNOWN },	// secondary fire
		{ GamepadState::Button::B,     CONTROLLER_BUTTON_B,          CONTROLLER_BUTTON_UNKNOWN,       KEY_F5 },		// headlamp
		{ GamepadState::Button::X,     CONTROLLER_BUTTON_X,          CONTROLLER_BUTTON_UNKNOWN,       KEY_F4 },		// gas mask
		{ GamepadState::Button::Y,     CONTROLLER_BUTTON_Y,          CONTROLLER_BUTTON_GUIDE,         KEY_UNKNOWN },	// previous weapon
		{ GamepadState::Button::START, CONTROLLER_BUTTON_START,      CONTROLLER_BUTTON_UNKNOWN,       KEY_F2 },		// night vision
	};
	// Menu / cutscene mapping (keyboard keys only; the d-pad drives the virtual mouse).
	struct MenuKeyMap
	{
		GamepadState::Button pad;
		KeyboardCode key;
		MouseButton mouse;
	};
	const MenuKeyMap s_menuMap[] =
	{
		{ GamepadState::Button::A,     KEY_UNKNOWN, MBUTTON_LEFT },
		{ GamepadState::Button::START, KEY_RETURN,  MBUTTON_COUNT },
		{ GamepadState::Button::B,     KEY_ESCAPE,  MBUTTON_COUNT },
		{ GamepadState::Button::Y,     KEY_SPACE,   MBUTTON_COUNT },
		{ GamepadState::Button::X,     KEY_N,       MBUTTON_COUNT },
	};

	// Everything that is currently reported as "down" to the engine, so it can
	// be released cleanly when the mapping changes (menu <-> game, modifier).
	bool s_ctrlDown[CONTROLLER_BUTTON_COUNT] = {};
	bool s_keyDown[KEY_COUNT] = {};
	bool s_mouseDown[MBUTTON_COUNT] = {};
	bool s_wasMenuMode = true;

	// Gamepad text entry for the agent name box (SELECT held in a menu):
	//   SELECT+Right  add a new letter (starts at 'A')
	//   SELECT+Up/Dn  change the last letter
	//   SELECT+Left   backspace
	// The engine's edit box applies buffered text before buffered keys within a
	// frame, so "replace the last letter" is queued as two frames: backspace, then
	// the new letter.
	const char c_textChars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ";
	constexpr int TEXT_QUEUE_LEN = 8;
	char s_textQueue[TEXT_QUEUE_LEN];	// 0 = nothing, '\b' = backspace, else the character to type.
	int s_textQueueHead = 0, s_textQueueCount = 0;
	int s_lastTextChar = -1;			// index into c_textChars of the last letter we typed, -1 = unknown.
	uint16_t s_prevPadButtons = 0;

	void queueText(char c)
	{
		if (s_textQueueCount >= TEXT_QUEUE_LEN) { return; }
		s_textQueue[(s_textQueueHead + s_textQueueCount) % TEXT_QUEUE_LEN] = c;
		s_textQueueCount++;
	}

	void flushTextQueue()
	{
		if (s_textQueueCount == 0) { return; }
		const char c = s_textQueue[s_textQueueHead];
		s_textQueueHead = (s_textQueueHead + 1) % TEXT_QUEUE_LEN;
		s_textQueueCount--;
		if (c == '\b')
		{
			TFE_Input::setBufferedKey(KEY_BACKSPACE);
		}
		else
		{
			char text[2] = { c, 0 };
			TFE_Input::setBufferedInput(text);
		}
	}

	void updateTextEntry(const GamepadState& state)
	{
		const uint16_t pressed = state.buttons & ~s_prevPadButtons;
		const int numChars = int(sizeof(c_textChars) - 1);
		if (pressed & (1 << int(GamepadState::Button::RIGHT)))
		{
			s_lastTextChar = 0;
			queueText(c_textChars[0]);
		}
		else if (pressed & (1 << int(GamepadState::Button::LEFT)))
		{
			queueText('\b');
			s_lastTextChar = -1;
		}
		else if ((pressed & (1 << int(GamepadState::Button::UP))) || (pressed & (1 << int(GamepadState::Button::DOWN))))
		{
			const int dir = (pressed & (1 << int(GamepadState::Button::UP))) ? 1 : -1;
			if (s_lastTextChar < 0)
			{
				// Nothing known at the end of the field: start a new letter.
				s_lastTextChar = 0;
				queueText(c_textChars[0]);
			}
			else
			{
				s_lastTextChar = (s_lastTextChar + dir + numChars) % numChars;
				queueText('\b');
				queueText(c_textChars[s_lastTextChar]);
			}
		}
	}

	void setCtrl(Button b, bool down)
	{
		if (b >= CONTROLLER_BUTTON_COUNT) { return; }
		if (down && !s_ctrlDown[b]) { TFE_Input::setButtonDown(b); }
		else if (!down && s_ctrlDown[b]) { TFE_Input::setButtonUp(b); }
		s_ctrlDown[b] = down;
	}

	void setKey(KeyboardCode k, bool down)
	{
		if (k == KEY_UNKNOWN || k >= KEY_COUNT) { return; }
		if (down && !s_keyDown[k])
		{
			TFE_Input::setKeyDown(k);
			TFE_Input::setBufferedKey(k);
		}
		else if (!down && s_keyDown[k]) { TFE_Input::setKeyUp(k); }
		s_keyDown[k] = down;
	}

	void setMouse(MouseButton m, bool down)
	{
		if (m >= MBUTTON_COUNT) { return; }
		if (down && !s_mouseDown[m]) { TFE_Input::setMouseButtonDown(m); }
		else if (!down && s_mouseDown[m]) { TFE_Input::setMouseButtonUp(m); }
		s_mouseDown[m] = down;
	}

	void releaseAll()
	{
		for (int i = 0; i < CONTROLLER_BUTTON_COUNT; i++) { setCtrl(Button(i), false); }
		for (int i = 0; i < KEY_COUNT; i++) { if (s_keyDown[i]) { setKey(KeyboardCode(i), false); } }
		for (int i = 0; i < MBUTTON_COUNT; i++) { setMouse(MouseButton(i), false); }
	}

	void updateInput(bool menuMode)
	{
		static auto& box = BoxEmu::get();
		const GamepadState state = box.gamepad_state();

		if (menuMode != s_wasMenuMode)
		{
			releaseAll();
			s_wasMenuMode = menuMode;
		}

		if (menuMode)
		{
			flushTextQueue();
			if (state.select)
			{
				// Text entry mode: release the regular menu bindings and drive the edit box.
				for (const auto& m : s_menuMap)
				{
					if (m.key != KEY_UNKNOWN) { setKey(m.key, false); }
					if (m.mouse != MBUTTON_COUNT) { setMouse(m.mouse, false); }
				}
				updateTextEntry(state);
				s_prevPadButtons = state.buttons;
				TFE_Input::setRelativeMousePos(0, 0);
				return;
			}
			s_prevPadButtons = state.buttons;
			s_lastTextChar = -1;

			// Virtual mouse: d-pad moves the cursor with a little acceleration.
			const bool moving = state.up || state.down || state.left || state.right;
			if (moving)
			{
				s_mouse.speed = std::min(s_mouse.speed + 0.25f, 4.0f);
			}
			else
			{
				s_mouse.speed = 1.0f;
			}
			if (state.left)  { s_mouse.x -= s_mouse.speed; }
			if (state.right) { s_mouse.x += s_mouse.speed; }
			if (state.up)    { s_mouse.y -= s_mouse.speed; }
			if (state.down)  { s_mouse.y += s_mouse.speed; }
			s_mouse.x = std::clamp(s_mouse.x, 0.0f, float(DF_WIDTH - 1));
			s_mouse.y = std::clamp(s_mouse.y, 0.0f, float(DF_HEIGHT - 1));
			TFE_Input::setMousePos(int(s_mouse.x), int(s_mouse.y));
			TFE_Input::setRelativeMousePos(0, 0);

			for (const auto& m : s_menuMap)
			{
				const bool down = state.is_pressed(m.pad);
				if (m.key != KEY_UNKNOWN) { setKey(m.key, down); }
				if (m.mouse != MBUTTON_COUNT) { setMouse(m.mouse, down); }
			}
		}
		else
		{
			TFE_Input::setRelativeMousePos(0, 0);
			const bool modifier = state.select;
			for (const auto& m : s_gameMap)
			{
				const bool down = state.is_pressed(m.pad);
				const bool altDown = down && modifier;
				const bool mainDown = down && !modifier;
				setCtrl(m.ctrl, mainDown);
				if (m.altCtrl != CONTROLLER_BUTTON_UNKNOWN) { setCtrl(m.altCtrl, altDown); }
				if (m.altKey != KEY_UNKNOWN) { setKey(m.altKey, altDown); }
			}
		}
	}

	// Hang detector: an esp_timer that fires if the game loop (or init) has not
	// made progress for a while and dumps the task list so we can see who is stuck.
	volatile int64_t s_lastProgressUs = 0;
	const char* volatile s_progressStage = "idle";
	volatile bool s_hangDetectorEnabled = false;
	esp_timer_handle_t s_hangTimer = nullptr;

	void progress(const char* stage)
	{
		s_progressStage = stage;
		s_lastProgressUs = esp_timer_get_time();
	}

	// Runs in the esp_timer task: keep the stack use minimal (no fmt).
	void hangCheck(void*)
	{
		if (!s_hangDetectorEnabled || !s_lastProgressUs) { return; }
		const int64_t idle = esp_timer_get_time() - s_lastProgressUs;
		if (idle < 8000000) { return; }
		// esp_rom_printf bypasses the stdio locks, so this works even if a task is stuck inside printf.
		esp_rom_printf("[DarkForces] *** no progress for %d ms, last stage '%s' ***\n", (int)(idle / 1000), (const char*)s_progressStage);
#if (configUSE_TRACE_FACILITY == 1) && (configUSE_STATS_FORMATTING_FUNCTIONS == 1)
		static char taskList[2048];
		vTaskList(taskList);
		esp_rom_printf("Name            State Prio Stack Num Core\n");
		// print line by line (esp_rom_printf has a limited output length)
		char* line = taskList;
		while (*line)
		{
			char* end = strchr(line, '\n');
			if (end) { *end = 0; }
			esp_rom_printf("%s\n", line);
			if (!end) { break; }
			line = end + 1;
		}
#endif
		s_lastProgressUs = esp_timer_get_time();	// report again in 8s if still stuck.
	}

	void startHangDetector()
	{
		if (!s_hangTimer)
		{
			esp_timer_create_args_t args = {};
			args.callback = hangCheck;
			args.name = "df_hang";
			esp_timer_create(&args, &s_hangTimer);
			esp_timer_start_periodic(s_hangTimer, 2000000);
		}
		progress("start");
		s_hangDetectorEnabled = true;
	}

	void stopHangDetector()
	{
		if (s_hangTimer)
		{
			esp_timer_stop(s_hangTimer);
			esp_timer_delete(s_hangTimer);
			s_hangTimer = nullptr;
		}
		s_lastProgressUs = 0;
		s_hangDetectorEnabled = false;
	}

	void logMemory(const char* when)
	{
		fmt::print("[DarkForces] {}: free internal {} B (largest {} B), free PSRAM {} B (largest {} B)\n", when,
			heap_caps_get_free_size(MALLOC_CAP_INTERNAL), heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
			heap_caps_get_free_size(MALLOC_CAP_SPIRAM), heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
	}

}

void init_darkforces(const std::string& gob_filename, uint8_t *romdata, size_t rom_data_size)
{
	s_gobPath = gob_filename;
	s_quit = false;
	s_paused = false;
	s_wasMenuMode = true;
	memset(s_ctrlDown, 0, sizeof(s_ctrlDown));
	memset(s_keyDown, 0, sizeof(s_keyDown));
	memset(s_mouseDown, 0, sizeof(s_mouseDown));

	// The game directory is the directory containing DARK.GOB.
	size_t slash = s_gobPath.find_last_of('/');
	s_gameDir = (slash == std::string::npos) ? std::string("/sdcard/") : s_gobPath.substr(0, slash + 1);
	fmt::print("[DarkForces] game directory: {}\n", s_gameDir);
	logMemory("before init");
	startHangDetector();
#if CONFIG_ESP_TASK_WDT_EN
	esp_task_wdt_add(NULL);
#endif

	auto& box = BoxEmu::get();
	// Use the (otherwise unused) 4MB ROM block as the engine's memory pool.
	static constexpr size_t ROM_POOL_SIZE = 4 * 1024 * 1024;
	pool_create(box.romdata(), ROM_POOL_SIZE);
	// The classic renderer's shared state is large (~320KB); keep it out of static RAM.
	TFE_Jedi::RClassicFixedState* rcfState = (TFE_Jedi::RClassicFixedState*)pool_alloc(sizeof(TFE_Jedi::RClassicFixedState));
	if (!rcfState) { rcfState = (TFE_Jedi::RClassicFixedState*)heap_caps_malloc(sizeof(TFE_Jedi::RClassicFixedState), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
	memset(rcfState, 0, sizeof(TFE_Jedi::RClassicFixedState));
	TFE_Jedi::rcf_setStatePtr(rcfState);

	s_frameBuffer = box.frame_buffer0();
	memset(s_frameBuffer, 0, DF_WIDTH * DF_HEIGHT);
	box.native_size(DF_WIDTH, DF_HEIGHT);

	// Paths: everything (game data, settings, saves) lives in the game directory.
	TFE_Paths::setPath(PATH_PROGRAM, s_gameDir.c_str());
	TFE_Paths::setPath(PATH_PROGRAM_DATA, s_gameDir.c_str());
	TFE_Paths::setPath(PATH_USER_DOCUMENTS, s_gameDir.c_str());
	TFE_Paths::setPath(PATH_SOURCE_DATA, s_gameDir.c_str());
	TFE_System::logOpen("the_force_engine_log.txt");
	progress("settings");

	bool firstRun = false;
	if (!TFE_Settings::init(firstRun))
	{
		TFE_System::logWrite(LOG_ERROR, "Main", "Cannot load settings.");
	}
	// Force the settings that make sense on this hardware.
	TFE_Settings_Graphics* graphics = TFE_Settings::getGraphicsSettings();
	graphics->gameResolution.x = DF_WIDTH;
	graphics->gameResolution.z = DF_HEIGHT;
	graphics->widescreen = false;
	graphics->rendererIndex = 0;
	graphics->asyncFramebuffer = false;
	graphics->gpuColorConvert = false;
	graphics->colorCorrection = false;
	graphics->extendAjoinLimits = false;
	graphics->reticleEnable = false;
	graphics->vsync = false;
	TFE_Settings_Sound* sound = TFE_Settings::getSoundSettings();
	sound->midiType = MIDI_TYPE_OPL3;
	sound->midiOutput = 0;
	sound->audioDevice = -1;
	sound->use16Channels = false;
	TFE_Settings_Game* game = TFE_Settings::getGameSettings();
	game->df_autorun = true;			// no run button on the pad.
	game->df_enableAutoaim = true;
	game->df_smoothVUEs = false;
	TFE_Settings::getSystemSettings()->gameQuitExitsToMenu = true;
	// Make sure the game source path points at our directory.
	const TFE_Game* gameInfo = TFE_Settings::getGame();
	TFE_GameHeader* gameHeader = TFE_Settings::getGameHeader(gameInfo->game);
	strcpy(gameHeader->sourcePath, s_gameDir.c_str());
	TFE_Paths::setPath(PATH_SOURCE_DATA, gameHeader->sourcePath);

	progress("system");
	TFE_System::init(0.0f, false, "esp-box-emu");

	WindowState windowState;
	memset(&windowState, 0, sizeof(windowState));
	windowState.width = DF_WIDTH;
	windowState.height = DF_HEIGHT;
	windowState.baseWindowWidth = DF_WIDTH;
	windowState.baseWindowHeight = DF_HEIGHT;
	windowState.monitorWidth = DF_WIDTH;
	windowState.monitorHeight = DF_HEIGHT;
	strcpy(windowState.name, "The Force Engine");
	TFE_RenderBackend::init(windowState);
	TFE_Jedi::vfb_resetState();
	TFE_Jedi::vfb_setPlatformBuffer(s_frameBuffer);

	TFE_FrontEndUI::initConsole();
	progress("midi");
	TFE_MidiPlayer::init(sound->midiOutput, (MidiDeviceType)sound->midiType);
	progress("audio");
	TFE_Audio::init(false, sound->audioDevice);
	TFE_FrontEndUI::init();
	progress("game_init");
	game_init();

	// Input: default keyboard binds (used for the SELECT combos) plus our controller binds.
	inputMapping_resetToDefaults();
	for (size_t i = 0; i < sizeof(s_boxControllerBinds) / sizeof(s_boxControllerBinds[0]); i++)
	{
		inputMapping_addBinding(&s_boxControllerBinds[i]);
	}
	InputConfig* inputConfig = inputMapping_get();
	inputConfig->controllerFlags |= CFLAG_ENABLE;
	inputConfig->mouseMode = MMODE_NONE;

	TFE_SaveSystem::init();

	progress("runGame");
	s_curGame = createGame(Game_Dark_Forces);
	TFE_SaveSystem::setCurrentGame(s_curGame);
	if (!s_curGame)
	{
		TFE_System::logWrite(LOG_ERROR, "Main", "Cannot create game.");
		s_quit = true;
	}
	else if (!s_curGame->runGame(1, s_argv, nullptr))
	{
		TFE_System::logWrite(LOG_ERROR, "Main", "Cannot run game (is the game data in %s?).", s_gameDir.c_str());
		freeGame(s_curGame);
		s_curGame = nullptr;
		s_quit = true;
	}

	s_initialized = true;
	logMemory("after init");
	reset_frame_time();
}

void reset_darkforces()
{
	deinit_darkforces();
	init_darkforces(s_gobPath, nullptr, 0);
}

void run_darkforces_rom()
{
	if (!s_initialized || !s_curGame || s_quit)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		return;
	}
	auto start = esp_timer_get_time();
	progress("frame");
#if CONFIG_ESP_TASK_WDT_EN
	esp_task_wdt_reset();
#endif

	// Menus, cutscenes and the briefing are mouse driven; in-mission we use the pad directly.
	const bool inMission = s_curGame->canSave();
	const bool menuMode = !inMission || s_curGame->isPaused();
	TFE_Input::enableRelativeMode(!menuMode);
	updateInput(menuMode);
	inputMapping_updateInput();

	// Load requests (from the cart's load slot menu).
	const char* loadRequest = TFE_SaveSystem::loadRequestFilename();
	if (loadRequest)
	{
		progress("load");
		fmt::print("[DarkForces] loading '{}'\n", loadRequest);
		TFE_Jedi::ImStopAllSounds();
		if (!TFE_SaveSystem::loadGame(loadRequest))
		{
			TFE_System::logWrite(LOG_ERROR, "Main", "Cannot load '%s'.", loadRequest);
		}
		// Leaving the previous level blanked the palette, and the save restores the
		// "palette modified" flag as it was at save time; force it to be re-applied.
		TFE_DarkForces::s_palModified = JTRUE;
		releaseAll();
	}

	TFE_System::update();
	if (TFE_System::systemUiRequestPosted())
	{
		TFE_FrontEndUI::enableConfigMenu();
	}

	TFE_SaveSystem::update();
	progress("loopGame");
	s_curGame->loopGame();
	progress("task_run");
	const bool endInputFrame = TFE_Jedi::task_run() != 0;
	progress("swap");

	// Push the frame to the display.
	TFE_RenderBackend::swap(true);

	if (endInputFrame)
	{
		TFE_Input::endFrame();
		inputMapping_endFrame();
	}

	if (TFE_System::quitMessagePosted())
	{
		s_quit = true;
	}

	const int64_t frameTime = esp_timer_get_time() - start;
	update_frame_time(frameTime);
	{
		static int64_t accum = 0, maxFrame = 0, lastReport = 0;
		static int frames = 0;
		accum += frameTime; frames++;
		if (frameTime > maxFrame) { maxFrame = frameTime; }
		if (esp_timer_get_time() - lastReport > 5000000)
		{
			if (lastReport)
			{
				fmt::print("[DarkForces] {:.1f} fps (avg {:.1f} ms, max {:.1f} ms), free internal {} B, PSRAM {} B\n",
					frames * 1000000.0 / double(esp_timer_get_time() - lastReport), accum / 1000.0 / frames, maxFrame / 1000.0,
					heap_caps_get_free_size(MALLOC_CAP_INTERNAL), heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
			}
			lastReport = esp_timer_get_time(); accum = 0; frames = 0; maxFrame = 0;
		}
	}
	// Cap the frame rate (the LCD tops out around 50 fps anyway) and always block
	// for at least one tick so that lower priority tasks (touch/gamepad polling,
	// LVGL, the emulator menu) get CPU time; game logic runs on real time ticks,
	// so this does not change the game speed.
	{
		using namespace std::chrono;
		static constexpr auto framePeriod = microseconds(20000);
		static auto nextFrame = steady_clock::now();
		auto now = steady_clock::now();
		if (nextFrame < now - 2 * framePeriod) { nextFrame = now; }
		nextFrame += framePeriod;
		if (nextFrame > now + milliseconds(1))
		{
			std::this_thread::sleep_until(nextFrame);
		}
		else
		{
			vTaskDelay(1);
		}
	}
}

bool darkforces_quit_requested()
{
	return s_quit;
}

void pause_darkforces_tasks()
{
	if (!s_initialized || s_paused) { return; }
	s_paused = true;
	s_hangDetectorEnabled = false;
#if CONFIG_ESP_TASK_WDT_EN
	esp_task_wdt_delete(NULL);
#endif
	TFE_MidiPlayer::pause();
	TFE_Audio::pause();
	if (s_curGame && s_curGame->canSave())
	{
		s_curGame->pauseGame(true);
	}
}

void resume_darkforces_tasks()
{
	if (!s_initialized || !s_paused) { return; }
	s_paused = false;
	progress("resume");
	s_hangDetectorEnabled = true;
#if CONFIG_ESP_TASK_WDT_EN
	esp_task_wdt_add(NULL);
#endif
	if (s_curGame && s_curGame->canSave())
	{
		s_curGame->pauseGame(false);
	}
	TFE_MidiPlayer::resume();
	TFE_Audio::resume();
	// The emulator menu overwrote the frame buffer and the display palette; force a redraw.
	BoxEmu::get().palette(TFE_RenderBackend::getDisplayPalette(), 256);
	releaseAll();
}

// Saves use the emulator's slot files (an absolute path from the cart); the
// save system accepts absolute paths on this platform.
void load_darkforces(std::string_view save_path, int save_slot)
{
	if (!s_initialized || !s_curGame || save_slot < 0 || save_path.empty()) { return; }
	// The load is processed on the next run_darkforces_rom() call (after the emulator menu closes).
	std::string path(save_path);
	TFE_SaveSystem::postLoadRequest(path.c_str());
}

void save_darkforces(std::string_view save_path, int save_slot)
{
	if (!s_initialized || !s_curGame || save_slot < 0 || save_path.empty()) { return; }
	if (!s_curGame->canSave())
	{
		fmt::print("[DarkForces] cannot save outside of a mission\n");
		return;
	}
	// Save right away (while the emulator menu is open) so the slot shows up as
	// used immediately. The game is paused by the menu; unpause around the save
	// so the paused flag isn't captured in the save file.
	std::string path(save_path);
	auto description = fmt::format("Box Slot {}", save_slot);
	const bool wasPaused = s_paused;
	if (wasPaused) { s_curGame->pauseGame(false); }
	progress("save");
	const bool ok = TFE_SaveSystem::saveGame(path.c_str(), description.c_str());
	if (wasPaused) { s_curGame->pauseGame(true); }
	fmt::print("[DarkForces] save '{}' {}\n", path, ok ? "ok" : "FAILED");
}

std::span<uint8_t> get_darkforces_video_buffer()
{
	// Convert the 8-bit frame to RGB565 (for screenshots) into frame buffer 1.
	uint8_t* dst = BoxEmu::get().frame_buffer1();
	const size_t num_pixels = DF_WIDTH * DF_HEIGHT;
	std::span<uint8_t> frame(dst, num_pixels * sizeof(uint16_t));
	const uint16_t* palette = TFE_RenderBackend::getDisplayPalette();
	const uint8_t* src = s_frameBuffer;
	if (palette && src)
	{
		for (size_t i = 0; i < num_pixels; i++)
		{
			uint16_t color = palette[src[i]];
			dst[i * 2] = color & 0xFF;
			dst[i * 2 + 1] = (color >> 8) & 0xFF;
		}
	}
	return frame;
}

void deinit_darkforces()
{
	if (!s_initialized) { return; }
	s_initialized = false;
	stopHangDetector();
#if CONFIG_ESP_TASK_WDT_EN
	esp_task_wdt_delete(NULL);
#endif

	releaseAll();
	if (s_curGame)
	{
		freeGame(s_curGame);
		s_curGame = nullptr;
	}
	game_destroy();
	inputMapping_shutdown();

	TFE_FrontEndUI::shutdown();
	TFE_Audio::shutdown();
	TFE_MidiPlayer::destroy();
	TFE_Settings::shutdown();
	TFE_RenderBackend::destroy();
	TFE_SaveSystem::destroy();
	TFE_Jedi::vfb_resetState();
	TFE_System::logClose();

	BoxEmu::get().audio_sample_rate(48000);
	TFE_Jedi::rcf_setStatePtr(nullptr);
	pool_destroy();
	logMemory("after deinit");
}
