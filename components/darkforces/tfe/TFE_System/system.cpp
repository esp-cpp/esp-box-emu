// ESP32 (esp-box-emu) version of the TFE system layer.
// Timing is based on esp_timer (microsecond resolution); everything else that
// the desktop version does (SDL, shell execution, vsync tracking) is stubbed.
#include <TFE_System/system.h>
#include <TFE_System/profiler.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_Settings/settings.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <algorithm>

#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace TFE_System
{
	f64 c_gameTimeScale = 1.02;	// Adjust game time to match DosBox.

	static u64 s_time;
	static u64 s_startTime;
	static f64 s_freq;
	static f64 s_refreshRate;

	static f64 s_dt = 1.0 / 60.0;		// This is just to handle the first frame, so any reasonable value will work.
	static f64 s_dtRaw = 1.0 / 60.0;
	static const f64 c_maxDt = 0.05;	// 20 fps

	static bool s_synced = false;
	static bool s_resetStartTime = false;
	static bool s_quitMessagePosted = false;
	static bool s_systemUiRequestPosted = false;

	static char s_versionString[64];

	static inline u64 perfCounter()
	{
		return (u64)esp_timer_get_time();
	}

	void init(f32 refreshRate, bool synced, const char* versionString)
	{
		TFE_System::logWrite(LOG_MSG, "Startup", "TFE_System::init");
		s_time = perfCounter();
		s_startTime = s_time;
		s_freq = 1.0 / 1000000.0;	// esp_timer runs in microseconds.

		s_refreshRate = f64(refreshRate);
		s_synced = synced;
		s_resetStartTime = false;
		s_quitMessagePosted = false;
		s_systemUiRequestPosted = false;

		strncpy(s_versionString, versionString ? versionString : "", sizeof(s_versionString) - 1);
		s_versionString[sizeof(s_versionString) - 1] = 0;
	}

	void shutdown()
	{
	}

	void setVsync(bool sync)
	{
		s_synced = sync;
	}

	bool getVSync()
	{
		return s_synced;
	}

	const char* getVersionString()
	{
		return s_versionString;
	}

	void resetStartTime()
	{
		s_resetStartTime = true;
	}

	f64 updateThreadLocal(u64* localTime)
	{
		const u64 curTime = perfCounter();
		const u64 uDt = (*localTime) > 0u ? curTime - (*localTime) : 0u;

		const f64 dt = f64(uDt) * s_freq;
		*localTime = curTime;

		return dt;
	}

	void update()
	{
		const u64 curTime = perfCounter();
		const u64 uDt = (curTime > s_time) ? (curTime - s_time) : 1;	// Make sure time is monotonic.
		s_time = curTime;
		if (s_resetStartTime)
		{
			s_startTime = s_time;
			s_resetStartTime = false;
		}

		// Delta time since the previous frame.
		f64 dt = f64(uDt) * s_freq;
		s_dtRaw = dt;
		// Make sure that if the current fps is too low, the game just slows down.
		// This avoids the "spiral of death" when using fixed time steps and avoids issues
		// during loading spikes.
		s_dt = std::min(dt, c_maxDt);
	}

	// Timing
	// Return the delta time.
	f64 getDeltaTime()
	{
		return s_dt;
	}

	f64 getDeltaTimeRaw()
	{
		return s_dtRaw;
	}

	// Get time since "start time"
	f64 getTime()
	{
		const u64 uDt = s_time - s_startTime;
		return f64(uDt) * s_freq;
	}

	u64 getCurrentTimeInTicks()
	{
		return perfCounter() - s_startTime;
	}

	f64 convertFromTicksToSeconds(u64 ticks)
	{
		return f64(ticks) * s_freq;
	}

	f64 microsecondsToSeconds(f64 mu)
	{
		return mu / 1000000.0;
	}

	void getDateTimeString(char* output)
	{
		time_t _tm = time(NULL);
		struct tm* curtime = localtime(&_tm);
		const char* str = curtime ? asctime(curtime) : nullptr;
		if (str)
		{
			strcpy(output, str);
			// asctime() appends a newline, remove it.
			size_t len = strlen(output);
			if (len && output[len - 1] == '\n') { output[len - 1] = 0; }
		}
		else
		{
			strcpy(output, "unknown");
		}
	}

	bool osShellExecute(const char* pathToExe, const char* exeDir, const char* param, bool waitForCompletion)
	{
		return false;
	}

	void sleep(u32 sleepDeltaMS)
	{
		if (sleepDeltaMS == 0)
		{
			taskYIELD();
			return;
		}
		vTaskDelay(pdMS_TO_TICKS(sleepDeltaMS) ? pdMS_TO_TICKS(sleepDeltaMS) : 1);
	}

	void postQuitMessage()
	{
		s_quitMessagePosted = true;
	}

	void postSystemUiRequest()
	{
		s_systemUiRequestPosted = true;
	}

	bool quitMessagePosted()
	{
		return s_quitMessagePosted;
	}

	bool systemUiRequestPosted()
	{
		bool systemUiPostReq = s_systemUiRequestPosted;
		s_systemUiRequestPosted = false;
		return systemUiPostReq;
	}
}
