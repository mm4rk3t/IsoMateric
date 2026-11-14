#ifndef AUDIO_H
#define AUDIO_H

#include <memory>
#include <string>

// Fire-and-forget sound effects through miniaudio. If no audio device is available
// (headless CI, broken PulseAudio) every call becomes a no-op instead of failing.
class Audio
{
public:
	Audio();
	~Audio();
	Audio(const Audio&) = delete;
	Audio& operator=(const Audio&) = delete;

	bool init(const std::string& soundDir);
	void play(const std::string& name, float volume = 1.0f); // plays <soundDir>/<name>.wav
	void setVolume(float volume);
	bool available() const;

	float volume = 0.6f;

private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};

#endif
