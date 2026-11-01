/* paf::sound::Output: the Vita layout and exported C++ interface. */
#ifndef PAF_AVOUT_SOUND_H
#define PAF_AVOUT_SOUND_H

#include <stddef.h>
#include <stdint.h>
#include "wave.h"

namespace paf {

class Framework;

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
	__declspec(dllexport) void Update();

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

	friend class paf::Framework;

	/* a common sound effect as the common resource gives it (Framework::InitCommonSE) */
	struct CommonSE {
		uint32_t type;
		int32_t samplerate;
		const char *buffer1;
		uint32_t size1;
		/* the second channel, or NULL */
		const char *buffer2;
		uint32_t size2;
	};

	enum {
		COMMON_SE_NUM = 55
	};

	CommonSE m_common_se[COMMON_SE_NUM];
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
