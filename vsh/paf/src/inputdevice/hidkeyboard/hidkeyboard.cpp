/*
 * paf::inputdevice::hidkeyboard: the cursor, enter and escape keys of the
 * connected keyboards, read by SceHid's hint callbacks and handed to the
 * input device each frame as one list of keys held down.
 */
#include <inputdevice/hidkeyboard/hidkeyboard.h>
#include <std/list>
#include <std/string.h>
#include <timer/timer.h>
#include <kernel/processmgr.h>
#include <kernel/threadmgr.h>
extern "C" {
#include <hid/hid.h>
}
#include "../../paf_types.h"

using namespace paf;
using namespace paf::inputdevice;
using namespace paf::inputdevice::hidkeyboard;

#define DEVICE_MAX	8
#define REPORT_MAX	16
#define PORT_MAX	1

/* A connected keyboard and the last report taken from it. */
struct Device {
	SceUID handle;
	uint32_t read_count;
	SceHidKeyboardReport report;
};

/* A keyboard's HID usage and the key it is. */
struct KeyMap {
	uint32_t keycode;
	uint32_t usage;
};

static const KeyMap s_keymap[] = {
	{ Data::Keycode_Right, 0x4f },
	{ Data::Keycode_Left, 0x50 },
	{ Data::Keycode_Down, 0x51 },
	{ Data::Keycode_Up, 0x52 },
	{ Data::Keycode_Return, 0x28 },
	{ Data::Keycode_Escape, 0x29 },
};

static int32_t s_device_num = 0;
static int32_t s_port = 0;
static int32_t s_port_pre = 0;

static InitParam s_init_param;
static void (*s_device_handler)(Data *data);
static SceUID s_enum_hint_cbid;
static SceUID s_read_hint_cbid;
static Device s_device[DEVICE_MAX];
/* the keys of the frame before, the keys the callbacks read */
static Data s_data_last;
static Data s_data_read;
static Data s_port_data_pre[PORT_MAX];
static Data s_port_data[PORT_MAX];
static Data s_data_pre;
static Data s_data;
/* keys released between two reads of a callback, kept until the next frame */
static Data s_data_released;
/* keys held down when the input was locked, ignored until they are released */
static Data s_data_locked;
static int32_t s_lock_count;
static int32_t s_unlock_count;
static list<Timer> s_lock_timer;
static SceUInt64 s_idle_time;

static SceInt32 EnumHintCallback(SceUID notifyId, SceInt32 notifyCount, SceInt32 notifyArg,
				 void *pCommon);
static SceInt32 ReadHintCallback(SceUID notifyId, SceInt32 notifyCount, SceInt32 notifyArg,
				 void *pCommon);

int32_t paf::inputdevice::hidkeyboard::Init(InitParam &init_param)
{
	s_init_param = init_param;
	s_enum_hint_cbid = 0;
	s_read_hint_cbid = 0;
	sce_paf_memset(s_device, 0, sizeof(s_device));
	s_data_last.Clear();
	s_data_read.Clear();
	for (int32_t i = 0; i < PORT_MAX; i++)
		s_port_data_pre[i].Clear();
	for (int32_t i = 0; i < PORT_MAX; i++)
		s_port_data[i].Clear();
	s_data_pre.Clear();
	s_data.Clear();
	s_data_released.Clear();
	s_data_locked.Clear();
	s_lock_count = 0;
	s_unlock_count = 0;
	s_idle_time = 0;
	return 0;
}

int32_t paf::inputdevice::hidkeyboard::RegisterCallback()
{
	if (s_enum_hint_cbid > 0 || s_read_hint_cbid > 0)
		return SCE_PAF_ERROR_INPUTDEVICE_ALREADY_REGISTERED;

	SceUID cbid = sceKernelCreateCallback("ScePafHidkeyboardEnumHint", 0, EnumHintCallback,
					      NULL);
	if (cbid < 0)
		return cbid;
	s_enum_hint_cbid = cbid;
	int32_t ret = sceHidKeyboardRegisterEnumHintCallback(cbid);
	if (ret < 0)
		return ret;

	cbid = sceKernelCreateCallback("ScePafHidkeyboardReadHint", 0, ReadHintCallback, NULL);
	if (cbid < 0)
		return cbid;
	s_read_hint_cbid = cbid;
	return sceHidKeyboardRegisterReadHintCallback(cbid);
}

int32_t paf::inputdevice::hidkeyboard::UnregisterCallback()
{
	if (s_enum_hint_cbid == 0 || s_read_hint_cbid == 0)
		return SCE_PAF_ERROR_INPUTDEVICE_NOT_REGISTERED;

	int32_t ret = sceHidKeyboardUnregisterEnumHintCallback(s_enum_hint_cbid);
	if (ret < 0)
		return ret;
	ret = sceKernelDeleteCallback(s_enum_hint_cbid);
	if (ret < 0)
		return ret;
	s_enum_hint_cbid = 0;
	ret = sceHidKeyboardUnregisterReadHintCallback(s_read_hint_cbid);
	if (ret < 0)
		return ret;
	ret = sceKernelDeleteCallback(s_read_hint_cbid);
	if (ret < 0)
		return ret;
	s_read_hint_cbid = 0;
	return ret;
}

bool paf::inputdevice::hidkeyboard::IsActive()
{
	return s_device_num != 0;
}

int32_t paf::inputdevice::hidkeyboard::End()
{
	return 0;
}

int32_t paf::inputdevice::hidkeyboard::Unlock()
{
	if (s_lock_count > s_unlock_count)
		s_unlock_count++;
	return s_unlock_count;
}

int32_t paf::inputdevice::hidkeyboard::Update(float diff_time, const Data **data,
					      const Data **data_pre)
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

	int32_t active = s_device_num;
	if (data != NULL)
		*data = &s_data;
	if (data_pre != NULL)
		*data_pre = &s_data_pre;
	if (active == 0)
		return 0;

	s_data_pre = s_data_last;
	s_data = s_data_read;
	for (int32_t i = 0; i < PORT_MAX; i++)
		s_port_data[i].Clear();
	s_port = 0;
	s_port_data[s_port] = s_data;

	int32_t ret = sceKernelCheckCallback();
	SceUInt64 idle_time = s_idle_time;
	int32_t port = s_port;
	int32_t unlock = s_unlock_count;
	int32_t port_pre = s_port_pre;

	if (s_data == s_port_data_pre[port < PORT_MAX ? port : 0]) {
	}

	/* the keys held down while locked stay ignored until released */
	int32_t lock = s_lock_count;
	for (int32_t i = 0; i < Data::KEY_MAX; i++) {
		if (lock > 0 && s_data.keycode[i] != 0)
			s_data_locked.keycode[i] = s_data.keycode[i];
	}
	if (unlock > 0) {
		s_lock_count = lock - unlock;
		s_unlock_count = 0;
	}
	if (!s_data_locked.IsEmpty()) {
		Data locked = s_data_locked;

		for (int32_t i = 0; i < Data::KEY_MAX; i++) {
			if (s_port_data_pre[port_pre < PORT_MAX ? port_pre : 0].keycode[i] != 0 &&
			    s_port_data[port < PORT_MAX ? port : 0].keycode[i] == 0)
				s_data_locked.keycode[i] = 0;
			if (locked.keycode[i] != 0)
				s_data.keycode[i] = 0;
		}
	}

	if (s_data == s_port_data_pre[port < PORT_MAX ? port : 0]) {
	}

	if (!s_port_data[port < PORT_MAX ? port : 0].IsEmpty())
		s_idle_time = 0;
	else if (idle_time == 0)
		s_idle_time = sceKernelGetProcessTimeWide();

	if (!(s_data == s_data_pre) || !s_data.IsEmpty()) {
		if (s_device_handler != NULL)
			s_device_handler(&s_data);
	}

	for (int32_t i = 0; i < PORT_MAX; i++)
		s_port_data_pre[i] = s_port_data[i];
	s_port_pre = s_port;
	s_data_last = s_data;

	/* the keys released since the last frame leave the read keys */
	if (!s_data_released.IsEmpty()) {
		for (uint32_t i = 0; i < s_data_released.num; i++)
			s_data_read.Remove(s_data_released.keycode[i]);
	}
	s_data_released.Clear();
	return ret;
}

void paf::inputdevice::hidkeyboard::Lock()
{
	s_lock_count++;
}

/* Sony's build keeps this empty function; nothing calls it. */
void paf::inputdevice::hidkeyboard::Reserved()
{
}

void paf::inputdevice::hidkeyboard::LockTimer(float time)
{
	Timer timer;

	timer.InitTimer(time, 0, 0, -1, NULL);
	s_lock_timer.push_back(timer);
	s_lock_count++;
}

bool paf::inputdevice::hidkeyboard::IsValid()
{
	return true;
}

/* Seconds since the last key was released, 0 while one is held down. */
float paf::inputdevice::hidkeyboard::GetIdleTime()
{
	const Data *port_data = s_port_data;

	if (s_port < PORT_MAX)
		port_data += s_port;
	if (port_data->IsEmpty() && s_idle_time != 0) {
		SceUInt64 now = sceKernelGetProcessTimeWide();

		if (s_idle_time < now)
			return (float)(now - s_idle_time) / 1000.0f;
	}
	return 0.0f;
}

/* handle is one of the keyboards already open */
static bool IsDevice(SceUID handle)
{
	for (int32_t i = 0; i < DEVICE_MAX; i++) {
		if (s_device[i].handle != 0 && handle == s_device[i].handle)
			return true;
	}
	return false;
}

/* handle is in the list of num handles */
static bool IsHandle(const SceUID *list, int32_t num, SceUID handle)
{
	for (int32_t i = 0; i < num; i++) {
		if (handle == list[i])
			return true;
	}
	return false;
}

static SceInt32 EnumHintCallback(SceUID notifyId, SceInt32 notifyCount, SceInt32 notifyArg,
				 void *pCommon)
{
	int32_t added_num = 0;
	int32_t removed_num = 0;
	SceUID handle[DEVICE_MAX];
	SceUID added[DEVICE_MAX];
	SceUID removed[DEVICE_MAX];

	sce_paf_memset(handle, 0, sizeof(handle));
	sce_paf_memset(added, 0, sizeof(added));
	sce_paf_memset(removed, 0, sizeof(removed));
	int32_t num = sceHidKeyboardEnumerate(handle, DEVICE_MAX);
	int32_t zero = 0;
	int32_t ret = 0;
	if (num >= 0) {
		for (int32_t i = 0; i < num; i++) {
			if (!IsDevice(handle[i]))
				added[added_num++] = handle[i];
		}
		for (int32_t j = 0; j < DEVICE_MAX; j++) {
			if (s_device[j].handle == 0)
				continue;
			if (!IsHandle(handle, num, s_device[j].handle))
				removed[removed_num++] = s_device[j].handle;
		}
	}

	for (int32_t i = 0; i < added_num; i++) {
		for (int32_t j = 0; j < DEVICE_MAX; j++) {
			if (s_device[j].handle == 0) {
				s_device[j].handle = added[i];
				s_device_num++;
				break;
			}
		}
	}
	for (int32_t i = 0; i < removed_num; i++) {
		for (int32_t j = 0; j < DEVICE_MAX; j++) {
			if (s_device[j].handle == removed[i]) {
				sce_paf_memset(&s_device[j], 0, sizeof(s_device[j]));
				s_device_num--;
				break;
			}
		}
	}

	s_data_last.Clear();
	s_data_read.Clear();
	for (int32_t i = zero; i < PORT_MAX; i++)
		s_port_data_pre[i].Clear();
	for (int32_t i = zero; i < PORT_MAX; i++)
		s_port_data[i].Clear();
	s_data_pre.Clear();
	s_data.Clear();
	return ret;
}

static SceInt32 ReadHintCallback(SceUID notifyId, SceInt32 notifyCount, SceInt32 notifyArg,
				 void *pCommon)
{
	Data prev;

	prev.Clear();
	for (int32_t i = 0; i < DEVICE_MAX; i++) {
		Device *device = &s_device[i];

		if (device->handle == 0)
			continue;

		SceHidKeyboardReport report[REPORT_MAX];

		device->read_count = 0;
		sce_paf_memset(report, 0, sizeof(report));
		int32_t num = sceHidKeyboardPeek(device->handle, report, REPORT_MAX);
		bool first = true;

		for (int32_t j = 0; j < num; j++) {
			bool newer = true;

			if (first)
				newer = report[j].timestamp > device->report.timestamp;
			if (report[j].reserved != 0 || !newer)
				continue;

			s_data_read.Clear();
			s_data_released = prev;
			device->report.reserved = report[j].reserved;
			device->report.modifiers[0] = report[j].modifiers[0];
			device->report.modifiers[1] = report[j].modifiers[1];
			device->report.timestamp = report[j].timestamp;
			sce_paf_memcpy(device->report.keycodes, report[j].keycodes,
				       sizeof(device->report.keycodes));
			first = false;

			uint32_t read_count = device->read_count;
			for (uint32_t k = 0; k < sizeof(s_keymap) / sizeof(s_keymap[0]); k++) {
				for (int32_t m = 0; m < 13; m++) {
					if (device->report.keycodes[m] == s_keymap[k].usage)
						s_data_read.Add(s_keymap[k].keycode);
				}
			}
			device->read_count = read_count + 1;

			/* the keys of the report before that are not held any more */
			prev = s_data_read;
			for (uint32_t k = 0; k < prev.num; k++) {
				if (s_data_released.Find(prev.keycode[k]))
					s_data_released.Remove(prev.keycode[k]);
			}
			if (!s_data_released.IsEmpty()) {
				for (uint32_t k = 0; k < s_data_released.num; k++)
					s_data_read.Add(s_data_released.keycode[k]);
			}
		}
	}
	return 0;
}
