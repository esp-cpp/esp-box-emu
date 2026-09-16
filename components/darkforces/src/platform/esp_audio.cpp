// ESP32 (esp-box-emu) audio system for The Force Engine.
//
// Dark Forces mixes all of its sound effects through the iMuse digital sound
// system, which registers itself with setAudioThreadCallback() and fills 8-bit
// stereo frames at 11025 Hz (the same code path the Amiga port uses). Music is
// rendered by the OPL3 MIDI synthesizer through TFE_MidiPlayer::synthesizeMidi().
//
// A dedicated FreeRTOS task (core 1) mixes the two into 16-bit stereo chunks and
// pushes them to the BoxEmu I2S output at a fixed cadence.
#include <TFE_Audio/audioSystem.h>
#include <TFE_Audio/midiPlayer.h>
#include <TFE_System/system.h>
#include <TFE_Settings/settings.h>
#include <TFE_Jedi/IMuse/imuse.h> // IM_AUDIO_OVERSAMPLE

#include "box-emu.hpp"
#include "task.hpp"
#include <TFE_System/espboxShared.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <chrono>
#include <cstring>
#include <memory>
#include <thread>

static_assert(IM_AUDIO_OVERSAMPLE == 0, "The ESP32 audio path expects the non-oversampled 8-bit iMuse output.");

struct SoundSource
{
	int dummy;
};

namespace TFE_Audio
{
	enum
	{
		AUDIO_FREQ = 11025,
		AUDIO_CHANNEL_COUNT = 2,
		// Stereo frames per mix chunk (limited by the iMuse internal buffer of 512 samples).
		AUDIO_CALLBACK_BUFFER_SIZE = 256,
		BUFFERED_SILENT_FRAME_COUNT = 8,
		AUDIO_BUFFER_SIZE = (AUDIO_CALLBACK_BUFFER_SIZE * AUDIO_CHANNEL_COUNT),
	};

	static f32 s_soundFxVolume = 1.0f;
	static s32 s_soundFxScale = 192;	// 8.8 fixed point (8-bit sfx -> 16-bit)
	static s32 s_prefillChunks = 0;		// chunks of silence to push ahead after (re)start
	static bool s_paused = false;
	static bool s_nullDevice = false;
	static volatile s32 s_silentAudioFrames = 0;
	static SemaphoreHandle_t s_lock = nullptr;

	// AUDIO_BUFFER_SIZE samples each (shared memory, see espbox_shared_audio()).
	static s8*  s_sfxBuffer = nullptr;
	static s16* s_midiBuffer = nullptr;
	static s16* s_mixBuffer = nullptr;
	static constexpr size_t SFX_BYTES = AUDIO_BUFFER_SIZE * sizeof(s8);
	static constexpr size_t PCM_BYTES = AUDIO_BUFFER_SIZE * sizeof(s16);

	static AudioThreadCallback s_audioThreadCallback = nullptr;
	static std::unique_ptr<espp::Task> s_task;

	static bool audioTask(std::mutex& m, std::condition_variable& cv, bool& task_notified)
	{
		using namespace std::chrono;
		static constexpr auto chunkPeriod = microseconds((1000000LL * AUDIO_CALLBACK_BUFFER_SIZE) / AUDIO_FREQ);
		static auto nextChunk = steady_clock::now();

		auto now = steady_clock::now();
		if (nextChunk < now - 4 * chunkPeriod)
		{
			// We fell far behind (e.g. a long load); don't try to catch up, but
			// re-prime the output so the consumer has some slack again.
			nextChunk = now;
			s_prefillChunks = 2;
		}
		if (s_prefillChunks > 0)
		{
			// The box's audio task pulls a fixed slice every 1/60s and zero-fills
			// whatever isn't queued yet, so stay a couple of chunks ahead of it.
			memset(s_mixBuffer, 0, PCM_BYTES);
			for (; s_prefillChunks > 0; s_prefillChunks--)
			{
				BoxEmu::get().play_audio(reinterpret_cast<const uint8_t*>(s_mixBuffer), PCM_BYTES);
			}
		}

		bool haveSfx = false;
		bool haveMidi = false;
		if (!s_paused)
		{
			if (s_audioThreadCallback)
			{
				lock();
				s_audioThreadCallback((f32*)s_sfxBuffer, AUDIO_CALLBACK_BUFFER_SIZE, 1.0f);
				unlock();
				haveSfx = true;
			}
			memset(s_midiBuffer, 0, PCM_BYTES);
			TFE_MidiPlayer::synthesizeMidi(s_midiBuffer, AUDIO_CALLBACK_BUFFER_SIZE, true);
			haveMidi = true;
		}

		if (s_silentAudioFrames > 0)
		{
			s_silentAudioFrames--;
			haveSfx = false;
			haveMidi = false;
		}

		if (!haveSfx && !haveMidi)
		{
			memset(s_mixBuffer, 0, PCM_BYTES);
		}
		else
		{
			const s32 sfxScale = haveSfx ? s_soundFxScale : 0;
			for (s32 i = 0; i < AUDIO_BUFFER_SIZE; i++)
			{
				// 8-bit sfx -> 16-bit with volume, plus the 16-bit music.
				s32 sample = (s32(s_sfxBuffer[i]) * sfxScale) + (haveMidi ? s32(s_midiBuffer[i]) : 0);
				if (sample > 32767) { sample = 32767; }
				else if (sample < -32768) { sample = -32768; }
				s_mixBuffer[i] = s16(sample);
			}
		}

		BoxEmu::get().play_audio(reinterpret_cast<const uint8_t*>(s_mixBuffer), PCM_BYTES);

		nextChunk += chunkPeriod;
		std::this_thread::sleep_until(nextChunk);
		return false;
	}

	bool init(bool useNullDevice/*=false*/, s32 outputId/*=-1*/)
	{
		TFE_System::logWrite(LOG_MSG, "Startup", "TFE_AudioSystem::init");
		s_nullDevice = useNullDevice;
		s_paused = false;
		s_silentAudioFrames = 0;
		s_audioThreadCallback = nullptr;

		TFE_Settings_Sound* soundSettings = TFE_Settings::getSoundSettings();
		setVolume(soundSettings->soundFxVolume);

		if (!s_lock)
		{
			s_lock = xSemaphoreCreateRecursiveMutex();
		}

		memset(s_sfxBuffer, 0, SFX_BYTES);
		memset(s_midiBuffer, 0, PCM_BYTES);
		memset(s_mixBuffer, 0, PCM_BYTES);

		if (s_nullDevice)
		{
			return false;
		}

		BoxEmu::get().audio_sample_rate(AUDIO_FREQ);
		s_prefillChunks = 2;

		s_task.reset();
		s_task = espp::Task::make_unique(espp::Task::Config{
				.callback = audioTask,
				.task_config = {
					.name = "df_audio",
					.stack_size_bytes = 6 * 1024,
					.priority = 10,
					.core_id = 1,
				}
			});
		s_task->start();
		TFE_System::logWrite(LOG_MSG, "Audio", "Audio initialized at %d Hz, %d frames per chunk.", AUDIO_FREQ, AUDIO_CALLBACK_BUFFER_SIZE);
		return true;
	}

	void shutdown()
	{
		TFE_System::logWrite(LOG_MSG, "Audio", "Shutdown");
		s_task.reset();
		s_audioThreadCallback = nullptr;
	}

	void stopAllSounds() {}
	void selectDevice(s32 id) {}
	void setUpsampleFilter(AudioUpsampleFilter filter) {}
	AudioUpsampleFilter getUpsampleFilter() { return AUF_DEFAULT; }

	void setVolume(f32 volume)
	{
		if (volume < 0.0f) { volume = 0.0f; }
		if (volume > 1.0f) { volume = 1.0f; }
		s_soundFxVolume = volume;
		s_soundFxScale = s32(volume * 192.0f);
	}

	f32 getVolume()
	{
		return s_soundFxVolume;
	}

	void pause()
	{
		s_paused = true;
	}

	void resume()
	{
		s_paused = false;
	}

	// Really the buffered audio will continue to process so time advances properly.
	// But for 'BUFFERED_SILENT_FRAME_COUNT' audio will be silent.
	// This allows the buffered data to be consumed without audio hitches.
	void bufferedAudioClear()
	{
		s_silentAudioFrames = BUFFERED_SILENT_FRAME_COUNT;
	}

	void setAudioThreadCallback(AudioThreadCallback callback)
	{
		lock();
		s_audioThreadCallback = callback;
		unlock();
	}

	const OutputDeviceInfo* getOutputDeviceList(s32& count, s32& curOutput)
	{
		count = 0;
		curOutput = 0;
		return nullptr;
	}

	void lock()
	{
		if (s_lock) { xSemaphoreTakeRecursive(s_lock, portMAX_DELAY); }
	}

	void unlock()
	{
		if (s_lock) { xSemaphoreGiveRecursive(s_lock); }
	}

	// Sound sources are not used by Dark Forces (everything goes through iMuse).
	bool playOneShot(SoundType type, f32 volume, const SoundBuffer* buffer, bool looping, SoundFinishedCallback finishedCallback, void* cbUserData, s32 cbArg) { return false; }
	SoundSource* createSoundSource(SoundType type, f32 volume, const SoundBuffer* buffer, SoundFinishedCallback callback, void* userData) { return nullptr; }
	s32 getSourceSlot(SoundSource* source) { return -1; }
	SoundSource* getSourceFromSlot(s32 slot) { return nullptr; }
	void playSource(SoundSource* source, bool looping) {}
	void stopSource(SoundSource* source) {}
	void freeSource(SoundSource* source) {}
	void setSourceVolume(SoundSource* source, f32 volume) {}
	void setSourceBuffer(SoundSource* source, const SoundBuffer* buffer) {}
	bool isSourcePlaying(SoundSource* source) { return false; }
	f32 getSourceVolume(SoundSource* source) { return 0.0f; }
}

void espbox_shared_audio(bool alloc)
{
	ESPBOX_SHARED_ALLOC(TFE_Audio::s_sfxBuffer, TFE_Audio::AUDIO_BUFFER_SIZE);
	ESPBOX_SHARED_ALLOC(TFE_Audio::s_midiBuffer, TFE_Audio::AUDIO_BUFFER_SIZE);
	ESPBOX_SHARED_ALLOC(TFE_Audio::s_mixBuffer, TFE_Audio::AUDIO_BUFFER_SIZE);
}
