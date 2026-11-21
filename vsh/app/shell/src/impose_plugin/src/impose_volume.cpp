/*
 * impose_volume.cpp: impose_plugin's volume control: the volume keys, repeated while held, the
 * volume bar, the mute on both keys, and the AV configuration's callback (0x811450A4-0x81145B00).
 * The file's and the names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <timer/timer.h>
#include <inputdevice/inputdevice.h>
#include <inputdevice/inputlistener.h>
#include <inputdevice/pad/pad.h>
#include <common/shared_ptr.h>
#include <kernel/threadmgr.h>
#include <av_config/av_config.h>
#include "../../../plugins.h"
#include "impose.h"

using namespace paf;

/* the volume's range, and its limit under AVLS */
#define IMPOSE_VOLUME_MAX	30
#define IMPOSE_VOLUME_AVLS_MAX	21

/* the AV configuration's events the callback takes (PLACEHOLDER names) */
#define IMPOSE_VOLUME_EVENT_BT_VOLUME	0x2
#define IMPOSE_VOLUME_EVENT_AVLS	0x4
#define IMPOSE_VOLUME_EVENT_AVLS_FORCED	0x8
#define IMPOSE_VOLUME_EVENT_HEADSET	0x10
#define IMPOSE_VOLUME_EVENTS		0x1F

/* the notice of the volume limited, and its text (PLACEHOLDER name) */
#define IMPOSE_VOLUME_MSG_AVLS_FORCED	0x81BCB60A

/* PLACEHOLDER: a notice of the shell's (0x811CBDE6, in the shell's own code) */
int Shell_811CBDE6(int type, int arg);

/* (0x81564D3C) */
ImposeVolume *g_imposeVolume;

/* the volume keys' listener (vtable 0x81530C9C) */
class ImposeVolumeListener : public inputdevice::InputListener {
public:
	ImposeVolumeListener() : InputListener(inputdevice::DEVICE_TYPE_PAD)
	{
	}

	virtual void OnUpdate(const inputdevice::Data *data);
};

static int imposeVolumeOnAVConfig(int notifyId, int notifyCount, int notifyArg, void *common);

/* the AV configuration's flags read (0x811450A4) */
ImposeVolume::ImposeVolume(Plugin *plugin)
	: m_repeatTime(0.0f), m_muteTime(0.0f), m_firstRepeat(true), m_external(false), m_volume(0),
	  m_bar(plugin), m_avls(false), m_fixed(false)
{
	g_imposeVolume = this;

	int device = 0;
	int fixed = 0;
	int avls = 0;

	sceAVConfigGetVolCtrlEnable(&device, &fixed, &avls);
	m_avls = avls != 0;
	m_fixed = fixed != 0;

	int volume;

	sceAVConfigGetSystemVol(&volume);
	m_volume = volume;
}

/* (0x8114512C) */
ImposeVolume::~ImposeVolume()
{
	sceAVConfigUnRegisterCallback();
	inputdevice::DelInputListener(m_listener);
	g_imposeVolume = NULL;
}

/* the keys listened to, the AV configuration's callback registered (0x811451A4) */
void ImposeVolume::Start()
{
	m_listener = common::SharedPtr<inputdevice::InputListener>(new ImposeVolumeListener());
	inputdevice::AddInputListener(m_listener);
	sceAVConfigRegisterCallback(sceKernelCreateCallback("SceShellAVConfigCallback", 0,
				    (SceKernelCallbackFunction)imposeVolumeOnAVConfig, NULL),
				    IMPOSE_VOLUME_EVENTS);
}

static inline int imposeVolumeClamp(int volume)
{
	if (volume > IMPOSE_VOLUME_MAX)
		volume = IMPOSE_VOLUME_MAX;
	else if (volume < 0)
		volume = 0;
	return volume;
}

/* the volume kept, and set unless it changed elsewhere */
static inline void imposeVolumeStore(int volume)
{
	g_imposeVolume->m_volume = volume;
	if (!g_imposeVolume->m_external)
		sceAVConfigSetSystemVol(g_imposeVolume->m_volume);
}

static inline void imposeVolumeChange(int volume)
{
	volume = imposeVolumeClamp(volume);
	if (g_imposeVolume->m_volume != volume)
		imposeVolumeStore(volume);
}

/* one step down, or the headset's key (0x811452FE) */
void ImposeVolume::VolumeDown(SceUInt32 flags)
{
	if (flags & IMPOSE_VOLUME_HEADSET) {
		sceAVConfigSendVolKey(2);
		m_bar.ShowBluetoothKey(-1);
		return;
	}

	int volume = m_volume;

	if ((flags & IMPOSE_VOLUME_AVLS) && volume >= IMPOSE_VOLUME_AVLS_MAX) {
		m_bar.BlinkAvls(0);
		volume = IMPOSE_VOLUME_AVLS_MAX;
		m_volume = IMPOSE_VOLUME_AVLS_MAX;
	}
	if (flags & IMPOSE_VOLUME_FIXED)
		imposeVolumeStore(imposeVolumeClamp(volume));
	else
		imposeVolumeChange(volume - 1);
	m_bar.SetVolume(m_volume, flags);
}

/* one step up, up to the limit, or the headset's key (0x811453A6) */
void ImposeVolume::VolumeUp(SceUInt32 flags)
{
	if (flags & IMPOSE_VOLUME_HEADSET) {
		sceAVConfigSendVolKey(1);
		m_bar.ShowBluetoothKey(1);
		return;
	}

	int volume = m_volume;

	if ((flags & IMPOSE_VOLUME_AVLS) && volume >= IMPOSE_VOLUME_AVLS_MAX) {
		if (flags & IMPOSE_VOLUME_FIXED) {
			imposeVolumeStore(imposeVolumeClamp(volume));
			volume = m_volume;
		}
		m_bar.SetVolume(volume, flags);
		m_bar.BlinkAvls(1);
		return;
	}
	if (flags & IMPOSE_VOLUME_FIXED)
		imposeVolumeStore(imposeVolumeClamp(volume));
	else
		imposeVolumeChange(volume + 1);
	m_bar.SetVolume(m_volume, flags);
}

/* the indicator told whether the volume is fixed */
static inline void imposeVolumeSetIndicator(int fixed)
{
	Plugin *indicator = Plugin::Find("indicator_plugin");

	if (indicator != NULL) {
		ShellPluginInterface *iface = (ShellPluginInterface *)indicator->GetInterface(1);

		if (iface != NULL)
			((void (*)(int))iface->func[8])(fixed);
	}
}

/* each frame: the keys pressed, repeated or released, both held for the mute (0x8114547A) */
void ImposeVolume::Update(const inputdevice::Data *data)
{
	SceUInt32 buttons = data->m_pad_data->buttons;
	SceUInt32 previous = data->m_pad_data_pre->buttons;
	bool up = (buttons & inputdevice::pad::Data::PAD_VOL_PLUS) != 0;
	bool down = (buttons & inputdevice::pad::Data::PAD_VOL_MINUS) != 0;
	bool upHeld = (previous & inputdevice::pad::Data::PAD_VOL_PLUS) != 0;
	bool downHeld = (previous & inputdevice::pad::Data::PAD_VOL_MINUS) != 0;
	SceUInt32 flags = 0;
	int device = 0;
	int fixed = 0;
	int avls = 0;

	m_repeatTime += Timer::s_tick_unit * Timer::s_tick_interval;
	sceAVConfigGetVolCtrlEnable(&device, &fixed, &avls);
	if (fixed != m_fixed) {
		if (fixed != 0) {
			m_repeatTime = 0.0f;
			m_bar.Open(1);
			m_bar.ShowAvlsLimit(avls);
			imposeVolumeSetIndicator(1);
		} else {
			imposeVolumeSetIndicator(0);
		}
		m_fixed = fixed != 0;
	}
	if (up && down) {
		m_muteTime += Timer::s_tick_unit * Timer::s_tick_interval;
		if (m_muteTime > 0.8f && device == 1 && fixed == 0 && !m_external)
			sceAVConfigMuteOn();
	} else if (m_muteTime <= 0.2f) {
		if (avls != 0)
			flags = IMPOSE_VOLUME_AVLS;
		if (fixed != 0)
			flags |= IMPOSE_VOLUME_FIXED;
		switch (device) {
		case 3:
			flags |= IMPOSE_VOLUME_HEADSET;
		case 2:
			flags |= IMPOSE_VOLUME_BLUETOOTH;
			flags &= ~IMPOSE_VOLUME_AVLS;
		case 1:
			if (avls != m_avls) {
				if ((flags & IMPOSE_VOLUME_BLUETOOTH) == 0)
					m_external = true;
				m_avls = avls != 0;
			}
			if (m_bar.IsOpen(1) != 0 && (m_device == 4 || m_device == 0))
				m_external = true;
			if ((up && !upHeld) || (down && !downHeld) || m_external) {
				m_repeatTime = 0.0f;
				m_firstRepeat = true;
				m_bar.Open(1);
				if (fixed != 0 && m_external) {
					if (m_fixed)
						m_bar.ShowAvlsLimit(avls);
				} else {
					int volume = m_volume;
					int btDevice;

					if (flags & IMPOSE_VOLUME_BLUETOOTH)
						sceAVConfigGetBtVol(&btDevice, &volume);
					else
						sceAVConfigGetSystemVol(&volume);
					imposeVolumeChange(volume);
					m_bar.SetVolume(volume, flags);
				}
				if (up)
					VolumeUp(flags);
				else if (down)
					VolumeDown(flags);
				m_external = false;
			} else if (up && upHeld) {
				if ((m_firstRepeat && m_repeatTime >= 0.6f) ||
				    (!m_firstRepeat && m_repeatTime >= 0.1f)) {
					VolumeUp(flags);
					m_repeatTime = 0.0f;
					m_firstRepeat = false;
				}
			} else if (down && downHeld) {
				if ((m_firstRepeat && m_repeatTime >= 0.6f) ||
				    (!m_firstRepeat && m_repeatTime >= 0.1f)) {
					VolumeDown(flags);
					m_repeatTime = 0.0f;
					m_firstRepeat = false;
				}
			} else if ((!upHeld || up) && (!downHeld || down)) {
				if (m_repeatTime >= 2.0f && m_bar.Close() != 0 &&
				    (flags & IMPOSE_VOLUME_BLUETOOTH) == 0)
					sceAVConfigWriteRegSystemVol(m_volume);
			} else {
				m_repeatTime = 0.0f;
				if (flags & IMPOSE_VOLUME_HEADSET) {
					sceAVConfigSendVolKey(0);
					m_bar.ShowBluetoothKey(0);
				}
			}
			break;
		case 4:
			flags |= IMPOSE_VOLUME_BLUETOOTH;
		case 0:
			if ((up && !upHeld) || (down && !downHeld)) {
				m_repeatTime = 0.0f;
				m_firstRepeat = true;
				m_bar.Open(1);
				m_bar.ShowMute((flags & IMPOSE_VOLUME_BLUETOOTH) != 0);
			} else if ((upHeld && !up) || (downHeld && !down)) {
				m_repeatTime = 0.0f;
				m_firstRepeat = true;
			} else if (!(up && upHeld) && !(down && downHeld) && m_repeatTime >= 2.0f) {
				m_bar.Close();
			}
			if (m_device != 4 && m_device != 0)
				m_bar.ShowMute((flags & IMPOSE_VOLUME_BLUETOOTH) != 0);
			break;
		default:
			break;
		}
	} else if (!up && !down) {
		m_muteTime = 0.0f;
	}
	m_device = device;
}

/* whether the volume is fixed (0x8114592E) */
bool ImposeVolume::IsFixed()
{
	int device = 0;
	int fixed = 0;
	int avls = 0;

	sceAVConfigGetVolCtrlEnable(&device, &fixed, &avls);
	return fixed != 0;
}

/* the AV configuration's changes: the Bluetooth volume, the limit, its notice, the headset
   (0x8114596A) */
static int imposeVolumeOnAVConfig(int notifyId, int notifyCount, int notifyArg, void *common)
{
	if (notifyArg & IMPOSE_VOLUME_EVENT_BT_VOLUME) {
		int device;
		int volume;

		g_imposeVolume->m_external = true;
		sceAVConfigGetBtVol(&device, &volume);
	}
	if (notifyArg & IMPOSE_VOLUME_EVENT_AVLS) {
		int device = 0;
		int fixed = 0;
		int avls = 0;

		sceAVConfigGetVolCtrlEnable(&device, &fixed, &avls);
		if (device == 1 && g_imposeVolume->m_volume >= IMPOSE_VOLUME_AVLS_MAX)
			imposeVolumeChange(IMPOSE_VOLUME_AVLS_MAX);
	}
	if (notifyArg & IMPOSE_VOLUME_EVENT_AVLS_FORCED) {
		ImposeNotice notice;

		notice.name = "SceShellVolumeForceAVLS";
		notice.message = g_imposeInterface->getString(IDParam(IMPOSE_VOLUME_MSG_AVLS_FORCED));
		g_imposeInterface->showNotice(&notice);
	}
	if (notifyArg & IMPOSE_VOLUME_EVENT_HEADSET)
		Shell_811CBDE6(1, 0);
	return 0;
}

/* (0x81145AEC) */
void ImposeVolumeListener::OnUpdate(const inputdevice::Data *data)
{
	g_imposeVolume->Update(data);
}
