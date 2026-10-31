/*
 * paf::inputdevice::pad: once per frame every port is sampled through SceCtrl
 * and its buttons mapped to paf's keys. A port that is used becomes the
 * current one, and its sample is the pad's. While the pad is locked the keys
 * held are masked until they are released, and the sticks read as centred
 * until they come back to the centre.
 */
#include <inputdevice/pad/pad.h>
#include <ctrl.h>
#include <std/list>
#include <std/string.h>
#include <std/stdlib.h>
#include <timer/timer.h>
#include <kernel/processmgr.h>

using namespace paf;
using namespace paf::inputdevice;
using namespace paf::inputdevice::pad;

enum {
	PORT_MAX = 5,
	KEYMAP_NUM = 19,
	/* how far a stick must leave the centre to count as used */
	STICK_THRESHOLD = 37
};

/* SceCtrl's controller types (sceCtrlGetControllerPortInfo) */
#define SCE_CTRL_TYPE_DS3	4
#define SCE_CTRL_TYPE_DS4	8

/* the device of each port */
typedef struct SceCtrlPortInfo {
	SceUInt8 port[PORT_MAX];
	SceUInt8 reserved[11];
} SceCtrlPortInfo;

extern "C" {
int sceCtrlSetSamplingMode(SceUInt32 uiMode);
int sceCtrlSetSamplingModeExt(SceUInt32 uiMode);
int sceCtrlPeekBufferPositive2(SceInt32 port, SceCtrlData2 *pData, SceInt32 nBufs);
int sceCtrlPeekBufferPositiveExt2(SceInt32 port, SceCtrlData2 *pData, SceInt32 nBufs);
int sceCtrlGetControllerPortInfo(SceCtrlPortInfo *pInfo);
}

struct KeyMap {
	uint32_t key;
	uint32_t button;
};

/* the port that may take over from the others with any key, 0 for any port */
static int32_t s_main_port = 0;
/* written by Init only */
static bool s_reserved = false;
/* the pad was locked since the last Update: the mask and the sticks stay */
static bool s_lock_requested = false;
static KeyMap s_keymap[KEYMAP_NUM] = {
	{ Data::PAD_UP, SCE_CTRL_UP },
	{ Data::PAD_DOWN, SCE_CTRL_DOWN },
	{ Data::PAD_LEFT, SCE_CTRL_LEFT },
	{ Data::PAD_RIGHT, SCE_CTRL_RIGHT },
	{ Data::PAD_ENTER, SCE_CTRL_CIRCLE },
	{ Data::PAD_ESCAPE, SCE_CTRL_CROSS },
	{ Data::PAD_MENU, SCE_CTRL_TRIANGLE },
	{ Data::PAD_DISPLAY, SCE_CTRL_SQUARE },
	{ Data::PAD_SELECT, SCE_CTRL_SELECT },
	{ Data::PAD_START, SCE_CTRL_START },
	{ Data::PAD_L, SCE_CTRL_L1 },
	{ Data::PAD_R, SCE_CTRL_R1 },
	{ Data::PAD_L2, SCE_CTRL_L2 },
	{ Data::PAD_R2, SCE_CTRL_R2 },
	{ Data::PAD_L3, SCE_CTRL_L3 },
	{ Data::PAD_R3, SCE_CTRL_R3 },
	{ Data::PAD_PS, SCE_CTRL_INTERCEPTED },
	{ Data::PAD_VOL_PLUS, 0x100000 },
	{ Data::PAD_VOL_MINUS, 0x200000 },
};

/* the enter button asked for (0: circle) and the one the keymap has */
static int32_t s_enter_button_req;
static int32_t s_enter_button;
static InitParam s_init_param;
static DeviceHandler s_device_handler;
static Data s_port_data_pre[PORT_MAX];
static Data s_port_data[PORT_MAX];
static int32_t s_port = s_main_port;
static Data s_data_pre;
static Data s_data;
/* keys masked until they are released */
static uint32_t s_mask;
/* the sticks read as centred until they come back to the centre */
static bool s_lock_lstick;
static bool s_lock_rstick;
static int32_t s_lock_count;
static int32_t s_unlock_count;
static list<Timer> s_lock_timer;
static SceUInt64 s_idle_time;

/* a port's sample; an invalid port gives port 0's */
static Data *GetPortData(Data *port_data, int32_t port)
{
	if (port < PORT_MAX)
		return &port_data[port];
	return &port_data[0];
}

int32_t paf::inputdevice::pad::Init(InitParam &init_param)
{
	int32_t ret;

	s_init_param = init_param;
	if (s_init_param.m_mode == InitParam::Mode_CommonDialog) {
		ret = sceCtrlSetSamplingModeExt(SCE_CTRL_MODE_DIGITALANALOG);
		if (ret != 0)
			return ret;
	} else if (s_init_param.m_mode == InitParam::Mode_DigitalOnly) {
		ret = sceCtrlSetSamplingMode(SCE_CTRL_MODE_DIGITALONLY);
		if (ret != 0)
			return ret;
	} else {
		ret = sceCtrlSetSamplingMode(SCE_CTRL_MODE_DIGITALANALOG);
		if (ret != 0)
			return ret;
	}

	for (int32_t i = 0; i < PORT_MAX; i++)
		s_port_data_pre[i].Clear();
	for (int32_t i = 0; i < PORT_MAX; i++)
		s_port_data[i].Clear();
	s_data_pre.Clear();
	s_data.Clear();
	s_mask = 0;
	s_lock_lstick = false;
	s_lock_rstick = false;
	s_lock_count = 0;
	s_unlock_count = 0;
	s_enter_button_req = 0;
	s_enter_button = 0;
	s_reserved = false;
	s_idle_time = 0;
	s_lock_requested = false;
	return 0;
}

int32_t paf::inputdevice::pad::End()
{
	int32_t ret;

	if (s_init_param.m_mode == InitParam::Mode_CommonDialog) {
		ret = sceCtrlSetSamplingModeExt(SCE_CTRL_MODE_DIGITALONLY);
		if (ret != 0)
			return ret;
	} else {
		ret = sceCtrlSetSamplingMode(SCE_CTRL_MODE_DIGITALONLY);
		if (ret != 0)
			return ret;
	}
	return 0;
}

void paf::inputdevice::pad::Unlock()
{
	if (s_lock_count > s_unlock_count)
		s_unlock_count++;
}

void paf::inputdevice::pad::SetMask(uint32_t mask)
{
	s_mask |= mask;
}

int32_t paf::inputdevice::pad::Update(float diff_time, const Data **data, const Data **data_pre)
{
	if (s_lock_timer.size() != 0) {
		list<Timer>::iterator it = s_lock_timer.begin();

		while (it != s_lock_timer.end()) {
			if (it->m_state == Timer::State_Finished) {
				it = s_lock_timer.erase(it);
				Unlock();
			} else {
				it->DoTick();
				++it;
			}
		}
	}

	if (data != NULL)
		*data = &s_data;
	if (data_pre != NULL)
		*data_pre = &s_data_pre;
	s_data_pre = s_data;
	for (int32_t i = 0; i < PORT_MAX; i++)
		s_port_data[i].Clear();

	Data port_data;

	for (int32_t port = 0; port < PORT_MAX; port++) {
		SceCtrlData2 ctrl;

		sce_paf_memset(&ctrl, 0, sizeof(ctrl));
		if (s_init_param.m_mode == InitParam::Mode_CommonDialog) {
			if (sceCtrlPeekBufferPositiveExt2(port, &ctrl, 1) < 0)
				break;
		} else {
			if (sceCtrlPeekBufferPositive2(port, &ctrl, 1) < 0)
				break;
		}

		int32_t main_port = s_main_port;
		uint32_t buttons = 0;

		for (uint32_t i = 0; i < KEYMAP_NUM; i++) {
			if ((s_keymap[i].button & ctrl.buttons) != 0)
				buttons |= s_keymap[i].key;
		}
		if (s_init_param.m_mode != InitParam::Mode_DigitalAnalog &&
		    (buttons & Data::PAD_PS) != 0)
			buttons &= ~Data::PAD_PS;

		uint16_t lx = ctrl.lx;
		uint16_t ly = ctrl.ly;
		uint16_t rx = ctrl.rx;
		uint16_t ry = ctrl.ry;

		if ((ctrl.buttons & SCE_CTRL_INTERCEPTED) != 0) {
			lx = Data::STICK_CENTER;
			ly = Data::STICK_CENTER;
			rx = Data::STICK_CENTER;
			ry = Data::STICK_CENTER;
		}
		port_data.buttons = buttons;
		port_data.lx = lx;
		port_data.ly = ly;
		port_data.rx = rx;
		port_data.ry = ry;

		*GetPortData(s_port_data, port) = port_data;
		if (main_port <= 0 || main_port >= PORT_MAX || main_port == port ||
		    buttons == Data::PAD_ESCAPE) {
			if (GetPortData(s_port_data, port)->buttons != 0 ||
			    sce_paf_abs(GetPortData(s_port_data, port)->lx - Data::STICK_CENTER) > STICK_THRESHOLD ||
			    sce_paf_abs(GetPortData(s_port_data, port)->ly - Data::STICK_CENTER) > STICK_THRESHOLD ||
			    sce_paf_abs(GetPortData(s_port_data, port)->rx - Data::STICK_CENTER) > STICK_THRESHOLD ||
			    sce_paf_abs(GetPortData(s_port_data, port)->ry - Data::STICK_CENTER) > STICK_THRESHOLD)
				s_port = port;
		}
	}

	s_data = *GetPortData(s_port_data, s_port);

	uint32_t mask = s_mask;

	if (s_lock_count > 0) {
		mask |= s_data.buttons != 0 ? s_data.buttons : 0;
		s_mask = mask;
		s_lock_rstick = true;
		s_lock_lstick = true;
	}

	bool lock_lstick = s_lock_lstick;
	bool lock_rstick = s_lock_rstick;

	if (s_unlock_count > 0) {
		s_lock_count -= s_unlock_count;
		s_unlock_count = 0;
		if (s_lock_count <= 0 && mask != 0 && !s_lock_requested) {
			mask &= ~Data::PAD_DIRECTION;
			s_mask = mask;
		}
		if (s_lock_count <= 0 && lock_lstick && !s_lock_requested) {
			lock_lstick = false;
			s_lock_lstick = false;
		}
	}
	s_lock_requested = false;

	if (mask != 0) {
		uint32_t pre = GetPortData(s_port_data_pre, s_port)->buttons;
		uint32_t released = pre & (GetPortData(s_port_data, s_port)->buttons ^
					   GetPortData(s_port_data_pre, s_port)->buttons);

		if (released != 0)
			s_mask = mask & ~released;
		s_data.buttons &= ~mask;
	}

	if (lock_lstick || lock_rstick) {
		bool lstick_centred = false;
		bool rstick_centred = false;

		if (sce_paf_abs(GetPortData(s_port_data, s_port)->lx - Data::STICK_CENTER) < STICK_THRESHOLD)
			lstick_centred = sce_paf_abs(GetPortData(s_port_data, s_port)->ly -
						     Data::STICK_CENTER) < STICK_THRESHOLD;
		if (sce_paf_abs(GetPortData(s_port_data, s_port)->rx - Data::STICK_CENTER) < STICK_THRESHOLD)
			rstick_centred = sce_paf_abs(GetPortData(s_port_data, s_port)->ry -
						     Data::STICK_CENTER) < STICK_THRESHOLD;
		if (lstick_centred)
			s_lock_lstick = false;
		if (rstick_centred)
			s_lock_rstick = false;
		if (lock_lstick) {
			s_data.lx = Data::STICK_CENTER;
			s_data.ly = Data::STICK_CENTER;
		}
		if (lock_rstick) {
			s_data.rx = Data::STICK_CENTER;
			s_data.ry = Data::STICK_CENTER;
		}
	}

	if (s_enter_button != s_enter_button_req && (s_mask & Data::PAD_ENTER) == 0 &&
	    (s_mask & Data::PAD_ESCAPE) == 0) {
		s_enter_button = s_enter_button_req;

		uint32_t escape = SCE_CTRL_CIRCLE;
		uint32_t enter = SCE_CTRL_CROSS;

		if (s_enter_button_req == 0) {
			escape = SCE_CTRL_CROSS;
			enter = SCE_CTRL_CIRCLE;
		}
		for (uint32_t i = 0; i < KEYMAP_NUM; i++) {
			if (s_keymap[i].button == enter)
				s_keymap[i].key = Data::PAD_ENTER;
			else if (s_keymap[i].button == escape)
				s_keymap[i].key = Data::PAD_ESCAPE;
		}
	}

	if (GetPortData(s_port_data, s_port)->buttons != 0)
		s_idle_time = 0;
	else if (s_idle_time == 0)
		s_idle_time = sceKernelGetProcessTimeWide();

	/* keys held, or keys changed since the frame before */
	uint32_t changed = s_data_pre.buttons ^ s_data.buttons;

	if ((s_data.buttons | changed) != 0 && s_device_handler != NULL)
		s_device_handler(&s_data);

	for (int32_t i = 0; i < PORT_MAX; i++)
		s_port_data_pre[i] = s_port_data[i];
	return 0;
}

DeviceHandler paf::inputdevice::pad::SetDeviceHandler(DeviceHandler new_handler)
{
	DeviceHandler old_handler = s_device_handler;

	s_device_handler = new_handler;
	return old_handler;
}

uint32_t paf::inputdevice::pad::GetMask()
{
	return s_mask;
}

void ScePafMisc_C4E8CBE6(uint32_t mask)
{
	s_mask |= mask & GetPortData(s_port_data, s_port)->buttons;
}

void paf::inputdevice::pad::Lock()
{
	s_lock_count++;
	s_lock_requested = true;
}

bool paf::inputdevice::pad::IsLocked()
{
	return s_lock_count > s_unlock_count;
}

void ScePafMisc_FA86D1FA(float time)
{
	Timer timer;

	timer.InitTimer(time, 0, 0, -1, NULL);
	s_lock_timer.push_back(timer);
	Lock();
}

int32_t ScePafMisc_0D1B028E(int32_t enter_button)
{
	Lock();
	s_enter_button_req = enter_button;
	Unlock();
	return s_enter_button;
}

int32_t ScePafMisc_9CB5D72E(void)
{
	return s_enter_button;
}

int32_t paf::inputdevice::pad::GetCurrPort()
{
	return s_port;
}

int32_t ScePafMisc_57080F75(int32_t port)
{
	Lock();
	if (port >= 0 && port < PORT_MAX)
		s_main_port = port;
	Unlock();
	return s_main_port;
}

int32_t ScePafMisc_333C86DC(void)
{
	return s_main_port;
}

int32_t paf::inputdevice::pad::SetCurrPort(int32_t port)
{
	Lock();
	if (port >= 0 && port < PORT_MAX)
		s_port = port;
	Unlock();
	return s_port;
}

int32_t paf::inputdevice::pad::GetPortType(int32_t port)
{
	SceCtrlPortInfo info;
	int32_t type = 0;

	if (port >= 0 && port < PORT_MAX && sceCtrlGetControllerPortInfo(&info) >= 0) {
		if (info.port[port] == SCE_CTRL_TYPE_DS3)
			type = 1;
		else if (info.port[port] == SCE_CTRL_TYPE_DS4)
			type = 2;
	}
	return type;
}

bool paf::inputdevice::pad::IsOwnershipReleasable()
{
	return true;
}

float paf::inputdevice::pad::GetIdleTime()
{
	if (GetPortData(s_port_data, s_port)->buttons == 0 && s_idle_time != 0) {
		SceUInt64 now = sceKernelGetProcessTimeWide();

		if (s_idle_time < now)
			return (float)(now - s_idle_time) / 1000.0f;
	}
	return 0.0f;
}

void paf::inputdevice::pad::ResetIdleTime()
{
	s_idle_time = 0;
}
