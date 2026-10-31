/*
 * paf::inputdevice::btrm: the Bluetooth remote. While active, a callback on
 * SceBt's AVRCP events records the keys pressed and released; Update, once
 * per frame, takes them as the remote's keys. While the remote is locked the
 * keys held are masked until they are released.
 */
#include <inputdevice/btrm/btrm.h>
#include <appmgr.h>
#include <kernel/threadmgr.h>
#include <kernel/processmgr.h>
#include <std/list>
#include <timer/timer.h>

/*
 * SceBt's user calls and event record, as sys/internal/include/bt/ declares
 * them; its headers' C11 assertions do not compile as C++03.
 */
typedef struct SceBtEvent {
	uint8_t id;
	uint8_t status;
	uint16_t reserved;
	uint32_t argument;
	uint32_t mac0;
	uint32_t mac1;
} SceBtEvent;

extern "C" {
int sceBtRegisterCallback(SceUID callback, uint64_t eventMask);
int sceBtUnregisterCallback(SceUID callback);
int sceBtReadEvent(SceBtEvent *events, int maxCount);
int sceAppMgrGetAppMgrState(SceAppMgrAppState *state);
}

using namespace paf;
using namespace paf::inputdevice;
using namespace paf::inputdevice::btrm;

enum {
	/* the events read from SceBt at a time */
	EVENT_NUM = 2,
	/* SceBt's AVRCP pass-through event */
	BT_EVENT_AVRCP = 13,
	/* an AVRCP operation id, its state bit (released) left out */
	AVRCP_OPERATION_MASK = 0x17f,
	AVRCP_RELEASED = 0x80,
	/* SceBt's event mask for the AVRCP events */
	BT_EVENT_MASK_AVRCP = 0x2000,
	SCE_BT_ERROR_BUSY = -2144400380,		/* 0x802F0C04 */
	ERROR_ALREADY_ACTIVE = -2135994016,	/* 0x80AF5160 */
	ERROR_NOT_ACTIVE = -2135994015		/* 0x80AF5161 */
};

/* paf's key for each AVRCP operation */
static const struct {
	uint32_t key;
	uint32_t operation;
} s_keymap[] = {
	{ Data::BTRM_PLAY, 0x44 },
	{ Data::BTRM_STOP, 0x45 },
	{ Data::BTRM_PAUSE, 0x46 },
	{ Data::BTRM_FORWARD, 0x4b },
	{ Data::BTRM_BACKWARD, 0x4c },
	{ Data::BTRM_FAST_FORWARD, 0x49 },
	{ Data::BTRM_REWIND, 0x48 }
};

static InitParam s_init_param;
static DeviceHandler s_device_handler;
static SceUID s_callback_id;
static SceBool s_system_ui_overlaid;
/* the keys of the frame before, and those the callback holds now */
static uint32_t s_buttons_pre;
static uint32_t s_buttons;
/* the keys read this frame and the frame before, unmasked */
static uint32_t s_raw_pre;
static uint32_t s_raw;
static Data s_data_pre;
static Data s_data;
/* the keys the callback saw pressed and released since the last frame */
static uint32_t s_pressed;
static uint32_t s_released;
static uint32_t s_mask;
static int32_t s_lock_count;
static int32_t s_unlock_count;
static list<Timer> s_lock_timer;
static bool s_system_ui_changed;
static SceUInt64 s_idle_time;

static SceInt32 ReadCallback(SceUID notifyId, SceInt32 notifyCount, SceInt32 notifyArg,
			     void *pCommon);

int32_t paf::inputdevice::btrm::Init(InitParam &init_param)
{
	s_init_param = init_param;
	s_callback_id = 0;
	s_system_ui_overlaid = 0;
	s_buttons_pre = 0;
	s_buttons = 0;
	s_raw_pre = 0;
	s_raw = 0;
	s_data_pre.buttons = 0;
	s_data.buttons = 0;
	s_pressed = 0;
	s_released = 0;
	s_mask = 0;
	s_lock_count = 0;
	s_unlock_count = 0;
	s_system_ui_changed = false;
	s_idle_time = 0;
	return 0;
}

int32_t paf::inputdevice::btrm::Activate()
{
	SceUID id;

	if (s_callback_id > 0)
		return ERROR_ALREADY_ACTIVE;
	id = sceKernelCreateCallback("ScePafBtrmRead", 0, ReadCallback, NULL);
	if (id < 0)
		return id;
	s_callback_id = id;
	return sceBtRegisterCallback(id, BT_EVENT_MASK_AVRCP);
}

int32_t paf::inputdevice::btrm::Deactivate()
{
	int32_t ret;

	if (s_callback_id == 0)
		return ERROR_NOT_ACTIVE;
	ret = sceBtUnregisterCallback(s_callback_id);
	if (ret < 0)
		return ret;
	ret = sceKernelDeleteCallback(s_callback_id);
	if (ret < 0)
		return ret;
	s_callback_id = 0;
	s_system_ui_overlaid = 0;
	return ret;
}

int32_t paf::inputdevice::btrm::End()
{
	return 0;
}

void paf::inputdevice::btrm::Unlock()
{
	if (s_lock_count > s_unlock_count)
		s_unlock_count++;
}

void paf::inputdevice::btrm::SetMask(uint32_t mask)
{
	s_mask |= mask;
}

int32_t paf::inputdevice::btrm::Update(float diff_time, const Data **data, const Data **data_pre)
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

	uint32_t buttons_pre = s_buttons_pre;
	uint32_t buttons = s_buttons;

	if (data != NULL)
		*data = &s_data;
	if (data_pre != NULL)
		*data_pre = &s_data_pre;
	s_data_pre.buttons = buttons_pre;
	s_data.buttons = buttons;
	s_raw = buttons;
	sceKernelCheckCallback();

	if (s_init_param.m_mode == InitParam::Mode_DigitalAnalog) {
		SceAppMgrAppState state;

		if (sceAppMgrGetAppMgrState(&state) >= 0) {
			if (s_system_ui_overlaid != state.isSystemUiOverlaid)
				s_system_ui_changed = state.isSystemUiOverlaid >= 0;
			s_system_ui_overlaid = state.isSystemUiOverlaid;
		}
	}

	int32_t lock_count = s_lock_count;
	int32_t unlock_count = s_unlock_count;
	uint32_t mask = s_mask;

	/* a lock, or the system UI coming or going, masks the keys held */
	if (lock_count > 0 || s_system_ui_changed) {
		uint32_t held = 0;

		if (s_data.buttons != 0)
			held = s_data.buttons;
		mask |= held;
		s_mask = mask;
	}
	if (unlock_count > 0) {
		s_unlock_count = 0;
		s_lock_count = lock_count - unlock_count;
	}
	if (mask != 0) {
		/* a masked key is unmasked once released */
		uint32_t released = s_raw_pre & (s_raw ^ s_raw_pre);

		if (released != 0)
			s_mask = mask & ~released;
		s_data.buttons &= ~mask;
	}

	if (s_idle_time == 0)
		s_idle_time = sceKernelGetProcessTimeWide();
	if (((s_data.buttons ^ s_data_pre.buttons) | s_data.buttons) != 0 &&
	    s_device_handler != NULL)
		s_device_handler(&s_data);
	s_buttons_pre = s_data.buttons;
	s_raw_pre = s_raw;

	/* a key pressed and released within the frame was held for it: release it now */
	if ((s_pressed & s_released) != 0)
		s_buttons ^= s_pressed & s_released;
	s_pressed = 0;
	s_released = 0;
	return 0;
}

DeviceHandler paf::inputdevice::btrm::SetDeviceHandler(DeviceHandler new_handler)
{
	DeviceHandler old_handler = s_device_handler;

	s_device_handler = new_handler;
	return old_handler;
}

uint32_t paf::inputdevice::btrm::GetMask()
{
	return s_mask;
}

void paf::inputdevice::btrm::Lock()
{
	s_lock_count++;
}

bool paf::inputdevice::btrm::IsLocked()
{
	return s_lock_count > s_unlock_count;
}

void ScePafMisc_F9C3A155(float time)
{
	Timer timer;

	timer.InitTimer(time, 0, 0, -1, NULL);
	s_lock_timer.push_back(timer);
	Lock();
}

bool paf::inputdevice::btrm::IsOwnershipReleasable()
{
	return true;
}

/* SceBt's AVRCP events: the keys pressed and released */
static SceInt32 ReadCallback(SceUID notifyId, SceInt32 notifyCount, SceInt32 notifyArg,
			     void *pCommon)
{
	SceBtEvent events[EVENT_NUM];
	int num;

	do {
		num = sceBtReadEvent(events, EVENT_NUM);
		if (num < 0) {
			if (num != SCE_BT_ERROR_BUSY)
				break;
			continue;
		}

		uint32_t buttons = s_buttons;
		uint32_t released = s_released;
		uint32_t pressed = s_pressed;

		for (const SceBtEvent *event = events; event < &events[num]; event++) {
			if (event->id != BT_EVENT_AVRCP)
				continue;

			uint32_t operation = event->argument & AVRCP_OPERATION_MASK;
			uint32_t release = event->argument & AVRCP_RELEASED;

			for (uint32_t i = 0; i < sizeof(s_keymap) / sizeof(s_keymap[0]); i++) {
				if (operation != s_keymap[i].operation)
					continue;

				uint32_t key = s_keymap[i].key;

				if (release != 0) {
					buttons ^= key;
					s_buttons = buttons;
					released |= key;
					s_released = released;
				} else {
					buttons |= key;
					s_buttons = buttons;
					pressed |= key;
					s_pressed = pressed;
				}
			}
		}
	} while (num > 1 || num == SCE_BT_ERROR_BUSY);

	/* pressed and released within the frame: held until Update's next frame */
	if ((s_pressed & s_released) != 0)
		s_buttons |= s_pressed & s_released;
	return 0;
}
