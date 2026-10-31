/* paf::sound::Output, backed by the Vita wave engine. */
#include "sound.h"
#include "wave.h"
#include <sysmem/memblock.h>

namespace paf {
namespace sound {

void Output::Init(int mode)
{
	if (m_is_init)
		return;

	if (mode > Mode_CommonDialog) {
		if (mode <= Mode_None)
			return;
	} else if (mode > Mode_Normal) {
		WaveInitExt();
		WaveStart(0x100000e7, 0x02000000, 0x00080000,
			  SCE_KERNEL_MEMBLOCK_TYPE_USER_CDIALOG_RW);
		m_is_init = 1;
		return;
	}
	WaveInit();
	WaveStart(0x100000e7, 0x02000000, 0, SCE_KERNEL_MEMBLOCK_TYPE_USER_RW);
	m_is_init = 1;
}

Output::Output(int mode)
{
	for (int i = 0; i < 8; ++i) {
		m_sounds[i].state = Sound::State_Default;
		m_sounds[i].buffer = NULL;
		m_sounds[i].no = 0;
	}
	m_is_init = 0;
	m_disable_count = 0;
	m_se_num = 0;
	m_enabled = true;
	Init(mode);
}

Output::~Output()
{
	if (m_is_init) {
		m_is_init = 0;
		WaveKeyOffAll();
		WaveStop();
		WaveExit();
	}
}

bool Output::EnableSound(bool enable)
{
	int count = m_disable_count;
	bool previous = m_enabled;

	if (enable == true) {
		if (count > 0)
			m_disable_count = --count;
	} else {
		m_disable_count = ++count;
	}
	if (count == 0)
		m_enabled = true;
	else
		m_enabled = false;
	return previous;
}

void Output::StopSound(const char *buffer)
{
	if (!m_is_init)
		return;

	for (int i = 0; i < 8; ++i) {
		if (m_sounds[i].state == Sound::State_Playing &&
		    m_sounds[i].buffer == buffer) {
			m_sounds[i].state = Sound::State_Stopped;
			if (WaveKeyOff(i))
				break;
		}
	}
}

void Output::StopSoundAll()
{
	if (!m_is_init)
		return;

	for (int i = 0; i < 8; ++i) {
		if (m_sounds[i].state == Sound::State_Playing) {
			m_sounds[i].state = Sound::State_Stopped;
			if (WaveKeyOff(i))
				break;
		}
	}
}

void Output::WaitSoundStopped()
{
	if (!m_is_init)
		return;

	for (int retry = 0; retry < 125; ++retry) {
		ScePafThread_651F4F81(40);
		Update();
		bool playing = false;
		for (int i = 0; i < 8; ++i) {
			if (m_sounds[i].state != Sound::State_Default)
				playing = true;
		}
		if (!playing)
			break;
	}
	WaveKeyOffAll();
}

void Output::PlayADPCM1ch(const char *buffer, unsigned int size, int samplerate,
			  float vol, bool ignore_init)
{
	if ((!ignore_init && !m_enabled) || !buffer)
		return;

	int count = 0;
	int oldest = -1;
	unsigned int no = ~0U;
	for (int i = 0; i < 8; ++i) {
		if (m_sounds[i].state == Sound::State_Playing &&
		    m_sounds[i].buffer == buffer) {
			++count;
			if (no >= m_sounds[i].no) {
				no = m_sounds[i].no;
				oldest = i;
			}
		}
	}
	if (count >= 2) {
		m_sounds[oldest].state = Sound::State_Stopped;
		if (WaveKeyOff(oldest))
			return;
	}

	for (int i = 0; i < 8; ++i) {
		if (m_sounds[i].state != Sound::State_Default)
			continue;
		m_sounds[i].state = Sound::State_Playing;
		m_sounds[i].buffer = buffer;
		m_sounds[i].no = m_se_num++;
		if (!WaveSetVoice(i, buffer, size, samplerate, 2)) {
			int volume = (int)(vol * 127.0f);
			WaveKeyOn(i, volume, volume);
		}
		return;
	}
}

void Output::PlaySound(const char *buffer, unsigned int size, int samplerate,
		      float vol, bool ignore_init)
{
	if (m_is_init)
		PlayADPCM1ch(buffer, size, samplerate, vol, ignore_init);
}

void Output::PlayADPCM2ch(const char *buffer1, unsigned int size1,
			  const char *buffer2, unsigned int size2, int samplerate,
			  float vol, bool ignore_init)
{
	if ((!ignore_init && !m_enabled) || !buffer1 || !buffer2)
		return;

	int voices[2] = { -1, -1 };
	int stopped = 0;
	for (int i = 0; i < 8; ++i) {
		if (m_sounds[i].state == Sound::State_Playing &&
		    m_sounds[i].buffer == buffer1) {
			m_sounds[i].state = Sound::State_Stopped;
			if (WaveKeyOff(i))
				return;
			if (++stopped >= 2)
				break;
		}
	}

	int count = 0;
	for (int i = 0; ; ) {
		if (m_sounds[i].state == Sound::State_Default) {
			voices[count++] = i;
			if (count >= 2)
				break;
		}
		if (++i >= 8)
			return;
	}

	int voice = voices[0];
	m_sounds[voice].state = Sound::State_Playing;
	m_sounds[voice].buffer = buffer1;
	m_sounds[voice].no = m_se_num++;
	if (WaveSetVoice(voice, buffer1, size1, samplerate, 2))
		return;
	if (WaveKeyOn(voice, (int)(vol * 127.0f), 0))
		return;

	voice = voices[1];
	m_sounds[voice].state = Sound::State_Playing;
	m_sounds[voice].buffer = buffer1;
	m_sounds[voice].no = m_se_num++;
	if (!WaveSetVoice(voice, buffer2, size2, samplerate, 2))
		WaveKeyOn(voice, 0, (int)(vol * 127.0f));
}

void Output::PlaySound(const char *buffer1, unsigned int size1,
		      const char *buffer2, unsigned int size2, int samplerate,
		      float vol, bool ignore_init)
{
	if (m_is_init)
		PlayADPCM2ch(buffer1, size1, buffer2, size2, samplerate, vol, ignore_init);
}

}
}
