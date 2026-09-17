// ESP32 (esp-box-emu) stubs for The Force Engine subsystems that are not
// available on the ESP32: the developer console, the ImGui front end, image
// loading, zip/memory archives, the GPU/float renderers, the SF2 and system
// MIDI devices, accessibility captions, and the reticle.
//
// Adapted from the Amiga port's stubs (amiga/stubs_amiga.cpp).
#include <TFE_Game/saveSystem.h>
#include <TFE_Game/reticle.h>
#include <TFE_Archive/zipArchive.h>
#include <TFE_Archive/gobMemoryArchive.h>
#include <TFE_Jedi/Level/level.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/Renderer/screenDraw.h>
#include <TFE_Jedi/Renderer/RClassic_GPU/rsectorGPU.h>
#include <TFE_Jedi/Renderer/RClassic_Float/rsectorFloat.h>
#include <TFE_Jedi/Renderer/RClassic_Float/rclassicFloatSharedState.h>
#include <TFE_RenderShared/texturePacker.h>
#include <TFE_Audio/MidiSynth/soundFontDevice.h>
#include <TFE_Audio/systemMidiDevice.h>
#include <TFE_System/system.h>
#include <TFE_System/tfeMessage.h>
#include <TFE_A11y/accessibility.h>
#include <TFE_FrontEndUI/console.h>
#include <TFE_FrontEndUI/frontEndUi.h>
#include <TFE_Asset/imageAsset.h>
#include <TFE_Input/inputMapping.h>

#include <cstdio>

namespace TFE_DarkForces
{
	extern void pauseLevelSound();
	extern void resumeLevelSound();
	extern JBool s_palModified;
}

/////////////////////////////////////////////
// Front end UI
/////////////////////////////////////////////
namespace TFE_FrontEndUI
{
	static bool s_exitToMenu = false;

	void init() { s_exitToMenu = false; }
	void shutdown() {}
	void initConsole() {}
	bool toggleConsole() { return false; }
	bool isConsoleOpen() { return false; }
	bool isConsoleAnimating() { return false; }
	bool isConfigMenuOpen() { return false; }
	bool uiControlsEnabled() { return false; }
	bool shouldClearScreen() { return false; }
	void setCurrentGame(IGame* game) {}
	void setAppState(AppState state) {}
	AppState update() { return APP_STATE_GAME; }
	void draw(bool drawFrontEnd, bool noGameData, bool setDefaults, bool showFps) {}
	void setCanSave(bool canSave) {}
	bool getCanSave() { return true; }
	void toggleProfilerView() {}

	void logToConsole(const char* str) {}

	void enableConfigMenu()
	{
		// There is no configuration UI on the box; just make sure the palette is restored.
		TFE_DarkForces::s_palModified = JTRUE;
	}

	// "Quit" from the in-game escape menu: leave the game and return to the emulator menu.
	void exitToMenu()
	{
		s_exitToMenu = true;
		TFE_System::postQuitMessage();
	}

	bool exitToMenuRequested()
	{
		return s_exitToMenu;
	}
}

/////////////////////////////////////////////
// Console
/////////////////////////////////////////////
namespace TFE_Console
{
	void registerCVarInt(const char* name, u32 flags, s32* var, const char* helpString) {}
	void registerCVarFloat(const char* name, u32 flags, f32* var, const char* helpString) {}
	void registerCVarBool(const char* name, u32 flags, bool* var, const char* helpString) {}
	void registerCVarString(const char* name, u32 flags, char* var, u32 maxLen, const char* helpString) {}
	void registerCommand(const char* name, ConsoleFunc func, u32 argCount, const char* helpString, bool repeat) {}

	void addSerializedCVarInt(const char* name, s32 value) {}
	void addSerializedCVarFloat(const char* name, f32 value) {}
	void addSerializedCVarBool(const char* name, bool value) {}
	void addSerializedCVarString(const char* name, const char* value) {}

	bool init() { return true; }
	void destroy() {}
	void update() {}
	bool isOpen() { return false; }
	bool isAnimating() { return false; }
	void startOpen() {}
	void startClose() {}

	void addToHistory(const char* str)
	{
		if (str) { printf("[TFE Console] %s\n", str); }
	}
	u32 getCVarCount() { return 0; }
	const CVar* getCVarByIndex(u32 index) { return nullptr; }
}

/////////////////////////////////////////////
// Messages
/////////////////////////////////////////////
namespace TFE_System
{
	const char* getMessage(TFE_Message msg)
	{
		switch (msg)
		{
			case TFE_MSG_SAVE:
				return "Game Saved.";
			case TFE_MSG_SECRET:
				return "You found a secret!";
			case TFE_MSG_FLYMODE:
				return "Fly Mode Toggle.";
			case TFE_MSG_NOCLIP:
				return "No Clip Toggle.";
			case TFE_MSG_TESTER:
				return "Testing Mode Toggle.";
			default:
				break;
		}
		return "";
	}

	bool loadMessages(const char* path) { return true; }
	void freeMessages() {}
}

/////////////////////////////////////////////
// Images (only used for save game thumbnails, which are disabled).
/////////////////////////////////////////////
namespace TFE_Image
{
	void init() {}
	void shutdown() {}

	Image* get(const char* imagePath)
	{
		return nullptr;
	}

	Image* loadFromMemory(const u8* buffer, size_t size)
	{
		return nullptr;
	}

	void free(Image* image) {}
	void freeAll() {}

	size_t writeImageToMemory(u8*& output, u32 width, u32 height, const u32* pixelData)
	{
		output = (u8*)pixelData;
		return width * height;
	}

	void readImageFromMemory(Image* output, size_t size, const u32* pixelData)
	{
		output->width = TFE_SaveSystem::SAVE_IMAGE_WIDTH;
		output->height = TFE_SaveSystem::SAVE_IMAGE_HEIGHT;
		output->data = (u32*)pixelData;
	}

	void writeImage(const char* path, u32 width, u32 height, u32* pixelData)
	{
	}
}

/////////////////////////////////////////////
// Archives that are not supported (mods).
/////////////////////////////////////////////
ZipArchive::~ZipArchive() {}
bool ZipArchive::create(const char *archivePath) { return false; }
bool ZipArchive::open(const char *archivePath) { return false; }
void ZipArchive::close() {}
bool ZipArchive::openFile(const char *file) { return false; }
bool ZipArchive::openFile(u32 index) { return false; }
void ZipArchive::closeFile() {}
bool ZipArchive::fileExists(const char *file) { return false; }
bool ZipArchive::fileExists(u32 index) { return false; }
u32 ZipArchive::getFileIndex(const char* file) { return INVALID_FILE; }
size_t ZipArchive::getFileLength() { return 0;}
size_t ZipArchive::readFile(void* data, size_t size) { return 0; }
bool ZipArchive::seekFile(s32 offset, s32 origin) { return false; }
size_t ZipArchive::getLocInFile() { return 0; }
u32 ZipArchive::getFileCount() { return 0; }
const char* ZipArchive::getFileName(u32 index) { return nullptr; }
size_t ZipArchive::getFileLength(u32 index) { return 0; }
void ZipArchive::addFile(const char* fileName, const char* filePath) {}

GobMemoryArchive::~GobMemoryArchive() {}
bool GobMemoryArchive::create(const char *archivePath) { return false; }
bool GobMemoryArchive::open(const char *archivePath) { return false; }
bool GobMemoryArchive::open(const u8* buffer, size_t size) { return false; }
void GobMemoryArchive::close() {}
bool GobMemoryArchive::openFile(const char *file) { return false; }
bool GobMemoryArchive::openFile(u32 index) { return false; }
void GobMemoryArchive::closeFile() {}
u32 GobMemoryArchive::getFileIndex(const char* file) { return INVALID_FILE; }
bool GobMemoryArchive::fileExists(const char *file) { return false; }
bool GobMemoryArchive::fileExists(u32 index) { return false; }
size_t GobMemoryArchive::getFileLength() { return 0; }
size_t GobMemoryArchive::readFile(void *data, size_t size) { return 0; }
bool GobMemoryArchive::seekFile(s32 offset, s32 origin) { return false; }
size_t GobMemoryArchive::getLocInFile() { return 0; }
u32 GobMemoryArchive::getFileCount() { return 0; }
const char* GobMemoryArchive::getFileName(u32 index) { return nullptr; }
size_t GobMemoryArchive::getFileLength(u32 index) { return 0; }
void GobMemoryArchive::addFile(const char* fileName, const char* filePath) {}

/////////////////////////////////////////////
// GPU / floating point renderers (only the classic fixed point renderer is built).
/////////////////////////////////////////////
namespace TFE_Jedi
{
	Vec3f s_cameraPos;
	Vec3f s_cameraDir;
	namespace RClassic_GPU
	{
		void resetState() {}
		void setupInitCameraAndLights(s32 width, s32 height) {}
		void changeResolution(s32 width, s32 height) {}
		void computeCameraTransform(RSector* sector, f32 pitch, f32 yaw, f32 camX, f32 camY, f32 camZ) {}
		void transformPointByCamera(vec3_float* worldPoint, vec3_float* viewPoint) {}
		void computeSkyOffsets() {}
	}

	namespace RClassic_Float
	{
		void resetState() {}
		void setupInitCameraAndLights(s32 width, s32 height) {}
		void changeResolution(s32 width, s32 height) {}
		void computeCameraTransform(RSector* sector, f32 pitch, f32 yaw, f32 camX, f32 camY, f32 camZ) {}
		void computeSkyOffsets() {}
	}

	void TFE_Sectors_Float::destroy() {}
	void TFE_Sectors_Float::reset() {}
	void TFE_Sectors_Float::prepare() {}
	void TFE_Sectors_Float::draw(RSector* sector) {}
	void TFE_Sectors_Float::subrendererChanged() {}

	void TFE_Sectors_GPU::destroy() {}
	void TFE_Sectors_GPU::reset() {}
	void TFE_Sectors_GPU::prepare() {}
	void TFE_Sectors_GPU::draw(RSector* sector) {}
	void TFE_Sectors_GPU::subrendererChanged() {}
	void TFE_Sectors_GPU::flushCache() {}
	TextureGpu* getColormap() { return nullptr; }

	void screenGPU_init() {}
	void screenGPU_setHudTextureCallbacks(s32 count, TextureListCallback* callbacks) {}
	void screenGPU_beginQuads(u32 width, u32 height) {}
	void screenGPU_endQuads() {}
	void screenGPU_beginLines(u32 width, u32 height) {}
	void screenGPU_endLines() {}
	void screenGPU_beginImageQuads(u32 width, u32 height) {}
	void screenGPU_endImageQuads() {}
	void screenGPU_addImageQuad(s32 x0, s32 z0, s32 x1, s32 z1, TextureGpu* texture) {}
	void screenGPU_addImageQuad(s32 x0, s32 z0, s32 x1, s32 z1, f32 u0, f32 u1, TextureGpu* texture) {}
	void screenGPU_drawPoint(ScreenRect* rect, s32 x, s32 z, u8 color) {}
	void screenGPU_drawLine(ScreenRect* rect, s32 x0, s32 z0, s32 x1, s32 z1, u8 color) {}
	void screenGPU_blitTextureLit(TextureData* texture, DrawRect* rect, s32 x0, s32 y0, u8 lightLevel, JBool forceTransparency) {}
	void screenGPU_drawColoredQuad(fixed16_16 x0, fixed16_16 y0, fixed16_16 x1, fixed16_16 y1, u8 color) {}
	void screenGPU_blitTextureScaled(TextureData* texture, DrawRect* rect, fixed16_16 x0, fixed16_16 y0, fixed16_16 xScale, fixed16_16 yScale, u8 lightLevel, JBool forceTransparency) {}

	void texturepacker_reset() {}
}

/////////////////////////////////////////////
// MIDI devices other than OPL3.
/////////////////////////////////////////////
namespace TFE_Audio
{
	SoundFontDevice::~SoundFontDevice() {}
	u32 SoundFontDevice::getOutputCount() { return 0; }
	void SoundFontDevice::getOutputName(s32 index, char* buffer, u32 maxLength) { *buffer = 0; }
	bool SoundFontDevice::selectOutput(s32 index) { return false; }
	s32 SoundFontDevice::getActiveOutput(void) { return 0; }
	void SoundFontDevice::exit() {}
	const char* SoundFontDevice::getName() { return "SF2 (unavailable)"; }
	bool SoundFontDevice::render(MidiSample* buffer, u32 sampleCount) { return false; }
	bool SoundFontDevice::canRender() { return false; }
	void SoundFontDevice::setVolume(f32 volume) {}
	void SoundFontDevice::message(u8 type, u8 arg1, u8 arg2) {}
	void SoundFontDevice::message(const u8* msg, u32 len) {}
	void SoundFontDevice::noteAllOff() {}

	SystemMidiDevice::SystemMidiDevice() : m_midiout(nullptr), m_outputId(-1) {}
	SystemMidiDevice::~SystemMidiDevice() {}
	void SystemMidiDevice::exit() {}
	const char* SystemMidiDevice::getName() { return "System MIDI (unavailable)"; }
	void SystemMidiDevice::message(u8 type, u8 arg1, u8 arg2) {}
	void SystemMidiDevice::message(const u8* msg, u32 len) {}
	void SystemMidiDevice::noteAllOff() {}
	void SystemMidiDevice::setVolume(f32 volume) {}
	u32 SystemMidiDevice::getOutputCount() { return 0; }
	void SystemMidiDevice::getOutputName(s32 index, char* buffer, u32 maxLength) { *buffer = 0; }
	bool SystemMidiDevice::selectOutput(s32 index) { return false; }
	s32 SystemMidiDevice::getActiveOutput(void) { return 0; }
};

/////////////////////////////////////////////
// Reticle (GPU only feature).
/////////////////////////////////////////////
bool reticle_init() { return false; }
void reticle_destroy() {}
void reticle_enable(bool enable) {}
void reticle_setShape(u32 index) {}
void reticle_setColor(const f32* color) {}
void reticle_setScale(f32 scale) {}
bool reticle_enabled() { return false; }
u32  reticle_getShape() { return 0; }
u32  reticle_getShapeCount() { return 0; }
void reticle_getColor(f32* color) { color[0] = color[1] = color[2] = color[3] = 1.0f; }
f32  reticle_getScale() { return 1.0f; }

/////////////////////////////////////////////
// Accessibility (captions).
/////////////////////////////////////////////
namespace TFE_A11Y
{
	void init() {}
	void clearCaptions() {}
	void drawCaptions() {}
	void drawExampleCaptions() {}
	void focusCaptions() {}
	void addCaption(Caption caption) {}
	void onSoundPlay(char* name, CaptionEnv env) {}
	bool cutsceneCaptionsEnabled() { return false; }
	bool gameplayCaptionsEnabled() { return false; }
}
