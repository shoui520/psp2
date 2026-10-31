/*
 * paf::inputdevice: every device initialised, read and locked together, the
 * input listeners told of each frame, and the ownership of the devices the
 * system shares (the touch panels' regions, the Bluetooth remote, the system
 * impose state) counted per device.
 */
#include <inputdevice/inputdevice.h>
#include <inputdevice/inputlistener.h>
#include <inputdevice/pad/pad.h>
#include <inputdevice/touchscreen/touchscreen.h>
#include <inputdevice/motionsensor/motionsensor.h>
#include <inputdevice/btrm/btrm.h>
#include <inputdevice/hidrm/hidrm.h>
#include <inputdevice/hidkeyboard/hidkeyboard.h>
#include <std/list>
#include <std/string.h>
extern "C" {
#include <appmgr/appmgr.h>
}

using namespace paf;
using namespace paf::inputdevice;

/* the devices whose ownership sets the system impose state */
#define DEVICE_IMPOSE	(DEVICE_PAD | DEVICE_TOUCHSCREEN_FRONT | DEVICE_TOUCHSCREEN_BACK | \
			 DEVICE_HIDRM | DEVICE_HIDKEYBOARD)

static int32_t s_impose_mode = 1;

static int32_t s_ownership_count[DEVICE_NUM];
static int32_t s_release_count[DEVICE_NUM];
static int32_t s_listener_count[DEVICE_TYPE_NUM];
static Data s_data;
static list<common::SharedPtr<InputListener> > s_listener;

/* The devices of the mask owned no more. */
static inline void ClearOwnership(uint32_t device)
{
	for (int32_t i = 0; i < DEVICE_NUM; i++) {
		if (((1 << i) & device) != 0) {
			s_ownership_count[i] = 0;
			s_release_count[i] = 0;
		}
	}
}

/* The devices with an ownership. */
static inline uint32_t GetOwnershipGotten()
{
	uint32_t gotten = 0;

	for (int32_t i = 0; i < DEVICE_NUM; i++) {
		if (s_ownership_count[i] > 0)
			gotten |= 1 << i;
	}
	return gotten;
}

int32_t paf::inputdevice::Init(InitParam &init_param)
{
	pad::InitParam pad_param;
	touchscreen::InitParam touch_param;
	motionsensor::InitParam motion_param;
	btrm::InitParam btrm_param;
	hidrm::InitParam hidrm_param;
	hidkeyboard::InitParam keyboard_param;
	int32_t mode = init_param.m_mode;

	for (int32_t i = 0; i < DEVICE_NUM; i++) {
		s_ownership_count[i] = 0;
		s_release_count[i] = 0;
	}
	for (int32_t i = 0; i < DEVICE_TYPE_NUM; i++)
		s_listener_count[i] = 0;

	switch (mode) {
	case InitParam::Mode_DigitalAnalog:
		pad_param.m_mode = 1;
		touch_param.m_mode = 1;
		motion_param.m_mode = 1;
		btrm_param.m_mode = 1;
		hidrm_param.m_mode = 1;
		keyboard_param.m_mode = 1;
		break;
	case InitParam::Mode_CommonDialog:
		pad_param.m_mode = 2;
		touch_param.m_mode = 2;
		motion_param.m_mode = 2;
		btrm_param.m_mode = 2;
		hidrm_param.m_mode = 2;
		keyboard_param.m_mode = 2;
		break;
	case InitParam::Mode_DigitalOnly:
		pad_param.m_mode = 3;
		touch_param.m_mode = 0;
		motion_param.m_mode = 0;
		btrm_param.m_mode = 0;
		hidrm_param.m_mode = 0;
		keyboard_param.m_mode = 0;
		break;
	default:
		pad_param.m_mode = 0;
		touch_param.m_mode = 0;
		motion_param.m_mode = 0;
		btrm_param.m_mode = 0;
		hidrm_param.m_mode = 0;
		keyboard_param.m_mode = 0;
		break;
	}

	int32_t ret = pad::Init(pad_param);
	ret |= touchscreen::Init(touch_param);
	ret |= motionsensor::Init(motion_param);
	ret |= btrm::Init(btrm_param);
	ret |= hidrm::Init(hidrm_param);
	return hidkeyboard::Init(keyboard_param) | ret;
}

int32_t paf::inputdevice::End()
{
	int32_t ret = hidkeyboard::End();

	ret |= hidrm::End();
	ret |= btrm::End();
	ret |= motionsensor::End();
	ret |= touchscreen::End();
	return pad::End() | ret;
}

/* The ownerships of the mask released by as many releases as gets end. */
static void ReleaseOwnershipCompleted(uint32_t device)
{
	uint32_t released = 0;
	uint32_t free = 0;

	for (int32_t i = 0; i < DEVICE_NUM; i++) {
		int32_t count = s_ownership_count[i];
		uint32_t bit = 1 << i;

		if ((bit & device) != 0 && s_release_count[i] > 0 &&
		    count == s_release_count[i])
			released |= bit;
		if (count == 0 && s_release_count[i] == 0)
			free |= bit;
	}

	if ((released & DEVICE_TOUCHSCREEN_FRONT) != 0) {
		ClearOwnership(DEVICE_TOUCHSCREEN_FRONT);
		touchscreen::ActivateOwnershipRegion(0, false);
	}
	if ((released & DEVICE_TOUCHSCREEN_BACK) != 0) {
		ClearOwnership(DEVICE_TOUCHSCREEN_BACK);
		touchscreen::ActivateOwnershipRegion(1, false);
	}
	if ((released & DEVICE_MOTIONSENSOR) != 0)
		ClearOwnership(DEVICE_MOTIONSENSOR);
	if ((released & DEVICE_BTRM) != 0) {
		ClearOwnership(DEVICE_BTRM);
		sceAppMgrReleaseBtrm();
	}
	if ((released & DEVICE_IMPOSE) != 0 &&
	    ((released | free) & DEVICE_IMPOSE) == DEVICE_IMPOSE) {
		ClearOwnership(DEVICE_IMPOSE);
		sceAppMgrSetSystemImposeState2(0);
	}
}

bool paf::inputdevice::IsOwnershipReleasable(uint32_t device, int32_t all)
{
	bool ret = false;
	uint32_t releasable = 0;

	for (int32_t i = 0; i < DEVICE_NUM; i++) {
		int32_t bit = 1 << i;
		bool result;

		switch (bit) {
		case DEVICE_PAD:
			result = pad::IsOwnershipReleasable();
			break;
		case DEVICE_TOUCHSCREEN_FRONT:
			result = touchscreen::IsOwnershipReleasable(0);
			break;
		case DEVICE_TOUCHSCREEN_BACK:
			result = touchscreen::IsOwnershipReleasable(1);
			break;
		case DEVICE_MOTIONSENSOR:
			result = motionsensor::IsOwnershipReleasable();
			break;
		case DEVICE_BTRM:
			result = btrm::IsOwnershipReleasable();
			break;
		case DEVICE_HIDRM:
			result = hidrm::IsOwnershipReleasable();
			break;
		case DEVICE_HIDKEYBOARD:
			result = hidkeyboard::IsOwnershipReleasable();
			break;
		default:
			continue;
		}
		if (result)
			releasable |= bit;
	}

	releasable &= device;
	if (all == 0) {
		if (releasable != 0)
			ret = true;
	} else if (releasable == device) {
		ret = true;
	}
	return ret;
}

int32_t paf::inputdevice::Update(float diff_time)
{
	sce_paf_memset(&s_data, 0, sizeof(s_data));

	int32_t ret = pad::Update(diff_time, &s_data.m_pad_data, &s_data.m_pad_data_pre);
	ret |= touchscreen::Update(diff_time, &s_data.m_touch_data, &s_data.m_touch_rear_data,
				   &s_data.m_touch_data_pre, &s_data.m_touch_rear_data_pre);
	ret |= motionsensor::Update(diff_time, &s_data.m_motion_data, &s_data.m_motion_data_pre);
	ret |= btrm::Update(diff_time, &s_data.m_btrm_data, &s_data.m_btrm_data_pre);
	ret |= hidrm::Update(diff_time, &s_data.m_bdrmc_data, &s_data.m_bdrmc_data_pre);
	ret |= hidkeyboard::Update(diff_time, &s_data.m_keyboard_data, &s_data.m_keyboard_data_pre);

	if (IsOwnershipReleasable(DEVICE_IMPOSE | DEVICE_MOTIONSENSOR, 1))
		ReleaseOwnershipCompleted(DEVICE_IMPOSE | DEVICE_MOTIONSENSOR);
	if (IsOwnershipReleasable(DEVICE_BTRM, 0))
		ReleaseOwnershipCompleted(DEVICE_BTRM);
	return ret;
}

int32_t paf::inputdevice::UpdateListener()
{
	for (list<common::SharedPtr<InputListener> >::iterator it = s_listener.begin();
	     it != s_listener.end(); ++it)
		(*it)->OnUpdate(&s_data);
	return 0;
}

int32_t paf::inputdevice::UpdateGesture(bool event)
{
	for (list<common::SharedPtr<InputListener> >::iterator it = s_listener.begin();
	     it != s_listener.end(); ++it)
		(*it)->OnUpdateGesture(&s_data, event);
	return 0;
}

/* The first listener of a type activates its device. */
int32_t paf::inputdevice::AddInputListener(common::SharedPtr<InputListener> &input_listener)
{
	InputListener *listener = input_listener.get();

	for (list<common::SharedPtr<InputListener> >::iterator it = s_listener.begin();
	     it != s_listener.end(); ++it) {
		if (it->get() == listener)
			return -1;
	}

	int32_t type = listener->m_type;
	int32_t ret;

	switch (type) {
	case DEVICE_TYPE_TOUCHSCREEN_FRONT:
		if (s_listener_count[type] == 0) {
			ret = touchscreen::Activate(0);
			if (ret < 0)
				return ret;
		}
		s_listener_count[type]++;
		break;
	case DEVICE_TYPE_TOUCHSCREEN_BACK:
		if (s_listener_count[type] == 0) {
			ret = touchscreen::Activate(1);
			if (ret < 0)
				return ret;
		}
		s_listener_count[type]++;
		break;
	case DEVICE_TYPE_MOTIONSENSOR:
		if (s_listener_count[type] == 0) {
			ret = motionsensor::Activate();
			if (ret < 0)
				return ret;
		}
		s_listener_count[type]++;
		break;
	case DEVICE_TYPE_BTRM:
		if (s_listener_count[type] == 0) {
			ret = btrm::Activate();
			if (ret < 0)
				return ret;
		}
		s_listener_count[type]++;
		break;
	case DEVICE_TYPE_HIDRM:
		if (s_listener_count[type] == 0) {
			ret = hidrm::Activate();
			if (ret < 0)
				return ret;
		}
		s_listener_count[type]++;
		break;
	case DEVICE_TYPE_KEYBOARD:
		if (s_listener_count[type] == 0) {
			ret = hidkeyboard::Activate();
			if (ret < 0)
				return ret;
		}
		s_listener_count[type]++;
		break;
	default:
		break;
	}

	s_listener.push_back(input_listener);
	return 0;
}

/* The last listener of a type deactivates its device. */
int32_t paf::inputdevice::DelInputListener(common::SharedPtr<InputListener> &input_listener)
{
	list<common::SharedPtr<InputListener> >::iterator it = s_listener.begin();

	for (; ; ++it) {
		if (it.node == s_listener.end().node)
			return -1;
		if (it->get() == input_listener.get())
			break;
	}

	s_listener.erase(it);

	int32_t type = input_listener->m_type;
	int32_t ret;

	switch (type) {
	case DEVICE_TYPE_TOUCHSCREEN_FRONT:
		if (s_listener_count[type] == 1) {
			ret = touchscreen::Deactivate(0);
			if (ret < 0)
				return ret;
		}
		s_listener_count[type]--;
		break;
	case DEVICE_TYPE_TOUCHSCREEN_BACK:
		if (s_listener_count[type] == 1) {
			ret = touchscreen::Deactivate(1);
			if (ret < 0)
				return ret;
		}
		s_listener_count[type]--;
		break;
	case DEVICE_TYPE_MOTIONSENSOR:
		if (s_listener_count[type] == 1) {
			ret = motionsensor::Deactivate();
			if (ret < 0)
				return ret;
		}
		s_listener_count[type]--;
		break;
	case DEVICE_TYPE_BTRM:
		if (s_listener_count[type] == 1) {
			ret = btrm::Deactivate();
			if (ret < 0)
				return ret;
		}
		s_listener_count[type]--;
		break;
	case DEVICE_TYPE_HIDRM:
		if (s_listener_count[type] == 1) {
			ret = hidrm::Deactivate();
			if (ret < 0)
				return ret;
		}
		s_listener_count[type]--;
		break;
	case DEVICE_TYPE_KEYBOARD:
		if (s_listener_count[type] == 1) {
			ret = hidkeyboard::Deactivate();
			if (ret < 0)
				return ret;
		}
		s_listener_count[type]--;
		break;
	default:
		break;
	}
	return 0;
}

void paf::inputdevice::Lock(uint32_t device)
{
	if ((device & DEVICE_PAD) != 0)
		pad::Lock();
	if ((device & DEVICE_TOUCHSCREEN_FRONT) != 0)
		touchscreen::Lock(0);
	if ((device & DEVICE_TOUCHSCREEN_BACK) != 0)
		touchscreen::Lock(1);
	if ((device & DEVICE_MOTIONSENSOR) != 0)
		motionsensor::Lock();
	if ((device & DEVICE_BTRM) != 0)
		btrm::Lock();
	if ((device & DEVICE_HIDRM) != 0)
		hidrm::Lock();
	if ((device & DEVICE_HIDKEYBOARD) != 0)
		hidkeyboard::Lock();
}

void paf::inputdevice::Unlock(uint32_t device)
{
	if ((device & DEVICE_PAD) != 0)
		pad::Unlock();
	if ((device & DEVICE_TOUCHSCREEN_FRONT) != 0)
		touchscreen::Unlock(0);
	if ((device & DEVICE_TOUCHSCREEN_BACK) != 0)
		touchscreen::Unlock(1);
	if ((device & DEVICE_MOTIONSENSOR) != 0)
		motionsensor::Unlock();
	if ((device & DEVICE_BTRM) != 0)
		btrm::Unlock();
	if ((device & DEVICE_HIDRM) != 0)
		hidrm::Unlock();
	if ((device & DEVICE_HIDKEYBOARD) != 0)
		hidkeyboard::Unlock();
}

void ScePafMisc_CAABF0B0(uint32_t device, float time)
{
	if ((device & DEVICE_PAD) != 0)
		ScePafMisc_FA86D1FA(time);
	if ((device & DEVICE_TOUCHSCREEN_FRONT) != 0)
		ScePafMisc_2CF401D9(0, time);
	if ((device & DEVICE_TOUCHSCREEN_BACK) != 0)
		ScePafMisc_2CF401D9(1, time);
	if ((device & DEVICE_MOTIONSENSOR) != 0)
		ScePafMisc_BB74B3B8(time);
	if ((device & DEVICE_BTRM) != 0)
		ScePafMisc_F9C3A155(time);
	if ((device & DEVICE_HIDRM) != 0)
		hidrm::LockTimer(time);
	if ((device & DEVICE_HIDKEYBOARD) != 0)
		hidkeyboard::LockTimer(time);
}

bool paf::inputdevice::IsOwnershipGotten(uint32_t device, int32_t all)
{
	bool ret = false;
	uint32_t gotten = GetOwnershipGotten() & device;

	if (all != 0) {
		if (gotten == device)
			return true;
	} else if (gotten != 0) {
		return true;
	}
	return ret;
}

/*
 * Each device of the mask owned once more; a device owned for the first time
 * takes its touch region or the Bluetooth remote, and the first of the impose
 * devices sets the system impose state.
 */
void paf::inputdevice::GetOwnership(uint32_t device)
{
	uint32_t gotten = 0;
	uint32_t free = 0;

	for (int32_t i = 0; i < DEVICE_NUM; i++) {
		int32_t count = s_ownership_count[i];
		uint32_t bit = 1 << i;

		if ((bit & device) != 0) {
			if (count != 0)
				continue;
			gotten |= bit;
		}
		if (count == 0 && s_release_count[i] == 0)
			free |= bit;
	}

	if ((gotten & DEVICE_TOUCHSCREEN_FRONT) != 0)
		touchscreen::ActivateOwnershipRegion(0, true);
	if ((gotten & DEVICE_TOUCHSCREEN_BACK) != 0)
		touchscreen::ActivateOwnershipRegion(1, true);
	if ((gotten & DEVICE_BTRM) != 0)
		sceAppMgrAcquireBtrm();
	if ((gotten & DEVICE_IMPOSE) != 0 && (free & DEVICE_IMPOSE) == DEVICE_IMPOSE)
		sceAppMgrSetSystemImposeState2(s_impose_mode);

	for (int32_t i = 0; i < DEVICE_NUM; i++) {
		if (((1 << i) & device) != 0)
			s_ownership_count[i]++;
	}
}

void paf::inputdevice::GetBtrmOwnership()
{
	GetOwnership(DEVICE_BTRM);
}

/* Released once more; the ownership ends in Update once the device is idle. */
static inline void CountRelease(uint32_t device)
{
	for (int32_t i = 0; i < DEVICE_NUM; i++) {
		if (((1 << i) & device) != 0 && s_ownership_count[i] > s_release_count[i])
			s_release_count[i]++;
	}
}

void paf::inputdevice::ReleaseOwnership(uint32_t device)
{
	CountRelease(device);
}

void paf::inputdevice::ReleaseOwnershipImmediate(uint32_t device)
{
	CountRelease(device);
	ReleaseOwnershipCompleted(device);
}

void paf::inputdevice::ReleaseBtrmOwnership()
{
	CountRelease(DEVICE_BTRM);
}

void paf::inputdevice::SetImposeMode(uint32_t mode)
{
	int32_t impose_mode = 1;

	if (mode == 1)
		impose_mode = 2;
	if (s_impose_mode != impose_mode) {
		s_impose_mode = impose_mode;
		if ((GetOwnershipGotten() & (DEVICE_IMPOSE | DEVICE_MOTIONSENSOR)) != 0)
			sceAppMgrSetSystemImposeState2(impose_mode);
	}
}
