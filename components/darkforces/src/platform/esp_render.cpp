// ESP32 (esp-box-emu) render backend for The Force Engine.
//
// The Jedi classic renderer produces an 8-bit paletted 320x200 image in the
// virtual framebuffer. The BoxEmu display pipeline already knows how to
// palette-convert and scale 8-bit frames, so all this backend does is keep an
// RGB565 copy of the palette and hand the frame pointer to BoxEmu on swap().
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_System/system.h>

#include "box-emu.hpp"
#include "pool_allocator.h"
#include "esp_platform.h"

#include <esp_heap_caps.h>

#include <cstring>
#include <cmath>

namespace TFE_RenderBackend
{
	static WindowState s_windowState;
	static u32 s_virtualWidth = 320;
	static u32 s_virtualHeight = 200;
	static const u8* s_curFrameBuffer = nullptr;
	static u32 s_paletteCpu[256];
	// RGB565 (native order, the display pipeline handles the LCD byte order).
	static uint16_t s_palette565[256];
	static bool s_colorCorrection = false;
	static u8 s_gammaTable[256];
	static u32 s_frameCount = 0;
	// Display buffers (8-bit, virtual resolution). The renderer draws into its own
	// buffer; swap() copies the finished frame here so the display task can convert
	// and scale it while the next frame is being drawn.
	static u8* s_displayBuffers[2] = { nullptr, nullptr };
	static u32 s_displayIndex = 0;

	static void freeDisplayBuffers()
	{
		for (u8*& buf : s_displayBuffers)
		{
			if (!buf) { continue; }
			if (pool_contains(buf)) { TFE_Memory::lockedPoolFree(buf); }
			else { free(buf); }
			buf = nullptr;
		}
	}

	static void allocDisplayBuffers(size_t size)
	{
		freeDisplayBuffers();
		for (u8*& buf : s_displayBuffers)
		{
			buf = (u8*)TFE_Memory::lockedPoolAlloc(size);
			if (!buf) { buf = (u8*)heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
			if (buf) { memset(buf, 0, size); }
		}
	}

	static inline uint16_t toDisplayColor(u8 r, u8 g, u8 b)
	{
		return uint16_t(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
	}

	bool init(const WindowState& state)
	{
		s_windowState = state;
		s_curFrameBuffer = nullptr;
		s_frameCount = 0;
		memset(s_paletteCpu, 0, sizeof(s_paletteCpu));
		memset(s_palette565, 0, sizeof(s_palette565));
		BoxEmu::get().palette(s_palette565, 256);
		return true;
	}

	void destroy()
	{
		s_curFrameBuffer = nullptr;
		BoxEmu::get().palette(nullptr);
		freeDisplayBuffers();
	}

	bool getVsyncEnabled() { return false; }
	void enableVsync(bool enable) {}
	void setClearColor(const f32* color) {}

	void swap(bool blitVirtualDisplay)
	{
		if (s_curFrameBuffer && blitVirtualDisplay)
		{
			u8* dst = s_displayBuffers[s_displayIndex];
			if (dst)
			{
				memcpy(dst, s_curFrameBuffer, s_virtualWidth * s_virtualHeight);
				s_displayIndex ^= 1;
			}
			BoxEmu::get().push_frame(dst ? dst : s_curFrameBuffer);
			s_frameCount++;
		}
		s_curFrameBuffer = nullptr;
	}

	u32 getFrameCount()
	{
		return s_frameCount;
	}

	void captureScreenToMemory(u32* mem) {}
	void queueScreenshot(const char* screenshotPath) {}
	void startGifRecording(const char* path) {}
	void stopGifRecording() {}
	void updateSettings() {}
	void resize(s32 width, s32 height) {}
	void enumerateDisplays() {}
	s32 getDisplayCount() { return 1; }
	s32 getDisplayIndex(s32 x, s32 y) { return 0; }

	bool getDisplayMonitorInfo(s32 displayIndex, MonitorInfo* monitorInfo)
	{
		monitorInfo->x = 0;
		monitorInfo->y = 0;
		monitorInfo->w = s_virtualWidth;
		monitorInfo->h = s_virtualHeight;
		return true;
	}

	f32 getDisplayRefreshRate() { return 0.0f; }
	void getCurrentMonitorInfo(MonitorInfo* monitorInfo) { getDisplayMonitorInfo(0, monitorInfo); }
	void enableFullscreen(bool enable) {}
	void clearWindow() {}

	void getDisplayInfo(DisplayInfo* displayInfo)
	{
		// Report the virtual resolution so that menu cursor math maps 1:1.
		displayInfo->width = s_virtualWidth;
		displayInfo->height = s_virtualHeight;
		displayInfo->refreshRate = 0.0f;
	}

	bool createVirtualDisplay(const VirtualDisplayInfo& vdispInfo)
	{
		s_virtualWidth = vdispInfo.width;
		s_virtualHeight = vdispInfo.height;
		BoxEmu::get().native_size(s_virtualWidth, s_virtualHeight);
		allocDisplayBuffers(s_virtualWidth * s_virtualHeight);
		return true;
	}

	u32 getVirtualDisplayWidth2D() { return s_virtualWidth; }
	u32 getVirtualDisplayWidth3D() { return s_virtualWidth; }
	u32 getVirtualDisplayHeight() { return s_virtualHeight; }
	u32 getVirtualDisplayOffset2D() { return 0; }
	u32 getVirtualDisplayOffset3D() { return 0; }
	void* getVirtualDisplayGpuPtr() { return nullptr; }
	bool getWidescreen() { return false; }
	bool getFrameBufferAsync() { return false; }
	bool getGPUColorConvert() { return false; }

	void updateVirtualDisplay(const void* buffer, size_t size)
	{
		s_curFrameBuffer = (const u8*)buffer;
	}

	void bindVirtualDisplay() {}
	void clearVirtualDisplay(f32* color, bool clearColor) {}
	void copyToVirtualDisplay(RenderTargetHandle src) {}
	void copyBackbufferToRenderTarget(RenderTargetHandle dst) {}

	void setPalette(const u32* palette)
	{
		// TFE palette entries are 0xAABBGGRR (R in the low byte).
		memcpy(s_paletteCpu, palette, sizeof(s_paletteCpu));
		for (s32 i = 0; i < 256; i++)
		{
			const u32 c = palette[i];
			u8 r = u8(c & 0xff);
			u8 g = u8((c >> 8) & 0xff);
			u8 b = u8((c >> 16) & 0xff);
			if (s_colorCorrection)
			{
				r = s_gammaTable[r];
				g = s_gammaTable[g];
				b = s_gammaTable[b];
			}
			s_palette565[i] = toDisplayColor(r, g, b);
		}
	}

	const u32* getPalette()
	{
		return s_paletteCpu;
	}

	const uint16_t* getDisplayPalette()
	{
		return s_palette565;
	}

	const TextureGpu* getPaletteTexture() { return nullptr; }

	void setColorCorrection(bool enabled, const ColorCorrection* color, bool bloomChanged)
	{
		s_colorCorrection = enabled && color && color->gamma != 1.0f;
		if (s_colorCorrection)
		{
			for (s32 i = 0; i < 256; i++)
			{
				f32 v = powf(f32(i) / 255.0f, 2.0f - color->gamma) * 255.0f;
				s_gammaTable[i] = u8(v < 0.0f ? 0 : (v > 255.0f ? 255 : v));
			}
		}
	}

	void drawVirtualDisplay() {}

	// GPU functionality is not available; everything below is a no-op.
	RenderTargetHandle createRenderTarget(u32 width, u32 height, bool hasDepthBuffer) { return RenderTargetHandle(nullptr); }
	void freeRenderTarget(RenderTargetHandle handle) {}
	void bindRenderTarget(RenderTargetHandle handle) {}
	void clearRenderTarget(RenderTargetHandle handle, const f32* clearColor, f32 clearDepth) {}
	void clearRenderTargetDepth(RenderTargetHandle handle, f32 clearDepth) {}
	void copyRenderTarget(RenderTargetHandle dst, RenderTargetHandle src) {}
	void unbindRenderTarget() {}
	const TextureGpu* getRenderTargetTexture(RenderTargetHandle rtHandle) { return nullptr; }
	void getRenderTargetDim(RenderTargetHandle rtHandle, u32* width, u32* height) { *width = 0; *height = 0; }
	TextureGpu* createTexture(u32 width, u32 height, TexFormat format) { return nullptr; }
	TextureGpu* createTextureArray(u32 width, u32 height, u32 layers, u32 channels) { return nullptr; }
	TextureGpu* createTexture(u32 width, u32 height, const u32* data, MagFilter magFilter) { return nullptr; }
	void freeTexture(TextureGpu* texture) {}
	void getTextureDim(TextureGpu* texture, u32* width, u32* height) { *width = 0; *height = 0; }
	void* getGpuPtr(const TextureGpu* texture) { return nullptr; }
	void drawIndexedTriangles(u32 triCount, u32 indexStride, u32 indexStart) {}
	void drawLines(u32 lineCount) {}
	void bloomPostEnable(bool enable) {}
	void setupPostEffectChain(bool useDynamicTexture) {}
}  // namespace
