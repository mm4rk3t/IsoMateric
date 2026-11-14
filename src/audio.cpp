#include "audio.h"
#include "log.h"

#include "miniaudio.h"

#include <filesystem>

namespace
{
	constexpr int kVoices = 32;
}

struct Audio::Impl
{
	ma_engine engine;
	ma_sound voices[kVoices];
	bool voiceUsed[kVoices] = {};
	int next = 0;
	bool ready = false;
	std::string dir;
};

Audio::Audio() : impl(std::make_unique<Impl>()) { }

Audio::~Audio()
{
	if (!impl->ready)
		return;
	for (int i = 0; i < kVoices; i++)
		if (impl->voiceUsed[i])
			ma_sound_uninit(&impl->voices[i]);
	ma_engine_uninit(&impl->engine);
}

bool Audio::init(const std::string& soundDir)
{
	impl->dir = soundDir;
	if (ma_engine_init(nullptr, &impl->engine) != MA_SUCCESS)
	{
		LOG_WARN("audio: no playback device, sound disabled");
		return false;
	}
	impl->ready = true;
	setVolume(volume);
	return true;
}

bool Audio::available() const
{
	return impl->ready;
}

void Audio::setVolume(float v)
{
	volume = v;
	if (impl->ready)
		ma_engine_set_volume(&impl->engine, v);
}

void Audio::play(const std::string& name, float gain)
{
	if (!impl->ready || gain <= 0.0f)
		return;
	std::string file = impl->dir + "/" + name + ".wav";
	if (!std::filesystem::exists(file))
	{
		LOG_WARN("audio: no sound '%s'", file.c_str());
		return;
	}
	// round-robin voice pool: the oldest voice is recycled when all are busy.
	// MA_SOUND_FLAG_DECODE + the engine's resource manager decode each file once.
	int slot = impl->next;
	for (int i = 0; i < kVoices; i++)
	{
		int candidate = (impl->next + i) % kVoices;
		if (!impl->voiceUsed[candidate] || !ma_sound_is_playing(&impl->voices[candidate]))
		{
			slot = candidate;
			break;
		}
	}
	impl->next = (slot + 1) % kVoices;

	ma_sound& voice = impl->voices[slot];
	if (impl->voiceUsed[slot])
		ma_sound_uninit(&voice);
	impl->voiceUsed[slot] = false;
	if (ma_sound_init_from_file(&impl->engine, file.c_str(), MA_SOUND_FLAG_DECODE, nullptr, nullptr, &voice) != MA_SUCCESS)
	{
		LOG_WARN("audio: cannot play '%s'", file.c_str());
		return;
	}
	impl->voiceUsed[slot] = true;
	ma_sound_set_volume(&voice, gain);
	ma_sound_start(&voice);
}
