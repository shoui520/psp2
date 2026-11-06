/*
 * paf::inputdevice::hidrm: the BD remote. While active, SceCtrl's BD remote
 * callback reports each key event; the key pressed is held until it is
 * released. Update, once per frame, takes it as the remote's key; while the
 * remote is locked the key held is ignored until it is released. The code is
 * hidkeyboard's, for one key and one port.
 */
#include <inputdevice/hidrm/hidrm.h>
#include <std/list>
#include <timer/timer.h>
#include <kernel/processmgr.h>
#include <kernel/threadmgr.h>

/* SceCtrl's BD remote calls, as sys/internal/include/ctrl/ctrl.h declares them */
extern "C" {
int sceCtrlRegisterBdRMCCallback(SceUID callback);
int sceCtrlUnregisterBdRMCCallback(SceUID callback);
}

using namespace paf;
using namespace paf::inputdevice;
using namespace paf::inputdevice::hidrm;

#define PORT_MAX	1

enum {
	/* a BD remote event: its key code and its state */
	EVENT_CODE_MASK = 0xfff,
	EVENT_STATE_MASK = (int32_t)0xf0000000,
	EVENT_STATE_PRESSED = 0x10000000,
	/* the event that releases every key */
	EVENT_CODE_RELEASE_ALL = 0xff,
	ERROR_ALREADY_ACTIVE = -2135994000,	/* 0x80AF5170 */
	ERROR_NOT_ACTIVE = -2135993999		/* 0x80AF5171 */
};

/* paf's key for each BD remote key code */
static const struct {
	uint32_t keycode;
	uint32_t code;
} s_keymap[] = {
	{ 0x01, 0x32 }, { 0x02, 0x38 }, { 0x03, 0x39 }, { 0x04, 0x30 },
	{ 0x05, 0x31 }, { 0x06, 0x76 }, { 0x07, 0x75 }, { 0x08, 0x33 },
	{ 0x09, 0x34 }, { 0x0a, 0x60 }, { 0x0b, 0x61 }, { 0x0c, 0x09 },
	{ 0x0d, 0x00 }, { 0x0e, 0x01 }, { 0x0f, 0x02 }, { 0x10, 0x03 },
	{ 0x11, 0x04 }, { 0x12, 0x05 }, { 0x13, 0x06 }, { 0x14, 0x07 },
	{ 0x15, 0x08 }, { 0x16, 0x16 }, { 0x17, 0x64 }, { 0x18, 0x65 },
	{ 0x19, 0x63 }, { 0x1a, 0x0f }, { 0x1b, 0x28 }, { 0x1c, 0x80 },
	{ 0x1d, 0x81 }, { 0x1e, 0x82 }, { 0x1f, 0x83 }, { 0x20, 0x70 },
	{ 0x21, 0x1a }, { 0x22, 0x40 }, { 0x23, 0x0e }, { 0x2b, 0x0c },
	{ 0x2c, 0x0b }
};

static int32_t s_port = 0;
static int32_t s_port_pre = 0;

/* called with the keys each frame they change or are held; nothing in libpaf sets it, and
   Sony's build reads it each time, so it is not internal to this file */
namespace paf {
namespace inputdevice {
namespace hidrm {
void (*s_device_handler)(Data *data);
}
}
}
static SceUID s_hint_cbid;
/* the key of the frame before, the key the callback holds */
static Data s_data_last;
static Data s_data_read;
static Data s_port_data_pre[PORT_MAX];
static Data s_port_data[PORT_MAX];
static Data s_data_pre;
static Data s_data;
/* keys released by the callback, kept until the next frame */
static Data s_data_released;
/* the key held down when the remote was locked, ignored until it is released */
static Data s_data_locked;
static int32_t s_lock_count;
static int32_t s_unlock_count;
static list<Timer> s_lock_timer;
static SceUInt64 s_idle_time;

static SceInt32 HintCallback(SceUID notifyId, SceInt32 notifyCount, SceInt32 notifyArg,
			     void *pCommon);

int32_t paf::inputdevice::hidrm::Init(InitParam &init_param)
{
	s_hint_cbid = 0;
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

int32_t paf::inputdevice::hidrm::Activate()
{
	if (s_hint_cbid > 0)
		return ERROR_ALREADY_ACTIVE;

	SceUID cbid = sceKernelCreateCallback("ScePafHidrmHint", 0, HintCallback, NULL);

	if (cbid < 0)
		return cbid;
	s_hint_cbid = cbid;
	return sceCtrlRegisterBdRMCCallback(cbid);
}

int32_t paf::inputdevice::hidrm::Deactivate()
{
	if (s_hint_cbid == 0)
		return ERROR_NOT_ACTIVE;

	int32_t ret = sceCtrlUnregisterBdRMCCallback(s_hint_cbid);

	if (ret < 0)
		return ret;
	ret = sceKernelDeleteCallback(s_hint_cbid);
	if (ret < 0)
		return ret;
	s_hint_cbid = 0;
	return ret;
}

int32_t paf::inputdevice::hidrm::End()
{
	return 0;
}

int32_t paf::inputdevice::hidrm::Unlock()
{
	if (s_lock_count > s_unlock_count)
		s_unlock_count++;
	return s_unlock_count;
}

int32_t paf::inputdevice::hidrm::Update(float diff_time, const Data **data, const Data **data_pre)
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

	s_port_data[0] = s_data_read;
	s_port = 0;
	s_data = s_data_read;
	s_data_pre = s_data_last;

	int32_t ret = sceKernelCheckCallback();
	SceUInt64 idle_time = s_idle_time;
	int32_t port = s_port;
	int32_t unlock = s_unlock_count;
	int32_t port_pre = s_port_pre;

	/* the key held down while locked stays ignored until released */
	int32_t lock = s_lock_count;
	for (int32_t i = 0; i < Data::KEY_MAX; i++) {
		if (lock > 0 && s_data.keycode[i] != 0)
			s_data_locked.keycode[i] = s_data.keycode[i];
	}
	if (unlock > 0) {
		s_unlock_count = 0;
		s_lock_count = lock - unlock;
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

Data paf::inputdevice::hidrm::GetLockedData()
{
	return s_data_locked;
}

void paf::inputdevice::hidrm::Lock()
{
	s_lock_count++;
}

void paf::inputdevice::hidrm::LockTimer(float time)
{
	Timer timer;

	timer.InitTimer(time, 0, 0, -1, NULL);
	s_lock_timer.push_back(timer);
	s_lock_count++;
}

bool paf::inputdevice::hidrm::IsOwnershipReleasable()
{
	return true;
}

/* A key pressed is added to the keys read; the release-all event lets them go. */
static SceInt32 HintCallback(SceUID notifyId, SceInt32 notifyCount, SceInt32 notifyArg,
			     void *pCommon)
{
	uint32_t code = notifyArg & EVENT_CODE_MASK;
	uint32_t state = notifyArg & EVENT_STATE_MASK;
	Data prev;

	prev.Clear();
	for (uint32_t i = 0; i < sizeof(s_keymap) / sizeof(s_keymap[0]); i++) {
		if (code == s_keymap[i].code && state == EVENT_STATE_PRESSED)
			s_data_read.Add(s_keymap[i].keycode);
	}

	/* the keys read are not released any more */
	prev = s_data_read;
	for (uint32_t k = 0; k < prev.num; k++) {
		if (s_data_released.Find(prev.keycode[k]))
			s_data_released.Remove(prev.keycode[k]);
	}
	if (code == EVENT_CODE_RELEASE_ALL && state == 0) {
		s_data_read.Clear();
		s_data_released = prev;
	}
	/* a key released is held until the next frame takes it */
	if (!s_data_released.IsEmpty()) {
		for (uint32_t k = 0; k < s_data_released.num; k++)
			s_data_read.Add(s_data_released.keycode[k]);
	}
	return 0;
}
