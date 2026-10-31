/* paf::sound::Output: the Vita layout and exported C++ interface. */
#ifndef PAF_AVOUT_SOUND_H
#define PAF_AVOUT_SOUND_H

#include <stddef.h>
#include "wave.h"

namespace paf {
namespace sound {

class Output {
public:
	enum Mode {
		Mode_Normal = 0,
		Mode_CommonDialog = 1,
		Mode_None = 2
	};

	Output(int mode);
	~Output();
	__declspec(dllexport) void Update()
	{
		if (!m_is_init)
			return;

		for (int i = 0; i < 8; ++i) {
			if (m_sounds[i].state != Sound::State_Default && !WaveIsPlaying(i))
				m_sounds[i].state = Sound::State_Default;
		}
	}

	bool EnableSound(bool enable);
	void StopSound(const char *buffer);
	void StopSoundAll();
	void WaitSoundStopped();
	void PlayADPCM1ch(const char *buffer, unsigned int size, int samplerate,
			 float vol, bool ignore_init);
	void PlayADPCM2ch(const char *buffer1, unsigned int size1,
			 const char *buffer2, unsigned int size2, int samplerate,
			 float vol, bool ignore_init);
	void PlaySound(const char *buffer, unsigned int size, int samplerate,
		       float vol, bool ignore_init);
	void PlaySound(const char *buffer1, unsigned int size1,
		       const char *buffer2, unsigned int size2, int samplerate,
		       float vol, bool ignore_init);

private:
	struct Sound {
		enum State {
			State_Default = 0,
			State_Playing = 1,
			State_Stopped = 2
		};

		State state;
		const char *buffer;
		unsigned int no;
	};

	void Init(int mode);

	/* Retained common-SE storage; these builds use the shared wave engine. */
	char m_common_se[0x528];
	int m_is_init;
	int m_disable_count;
	bool m_enabled;
	Sound m_sounds[8];
	unsigned int m_se_num;
};

typedef char OutputSizeCheck[sizeof(Output) == 0x598 ? 1 : -1];

}
}

#endif
