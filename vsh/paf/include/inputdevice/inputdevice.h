/*
 * paf::inputdevice: the input devices (pad, touch screens, motion sensor, the
 * Bluetooth and HID remotes, keyboards) read once per frame, their locks and
 * the ownership of the devices the system shares with other applications.
 */
#ifndef PAF_INPUTDEVICE_INPUTDEVICE_H
#define PAF_INPUTDEVICE_INPUTDEVICE_H

#include <stdint.h>

namespace paf {
namespace inputdevice {

namespace pad {
class Data;
}
namespace touchscreen {
class Data;
}
namespace motionsensor {
class Data;
}
namespace btrm {
class Data;
}
namespace hidrm {
class Data;
}
namespace hidkeyboard {
class Data;
}

/* The devices as bits of a device mask (Lock, GetOwnership, ...). */
enum {
	DEVICE_PAD = 1 << 0,
	DEVICE_TOUCHSCREEN_FRONT = 1 << 1,
	DEVICE_TOUCHSCREEN_BACK = 1 << 2,
	DEVICE_MOTIONSENSOR = 1 << 3,
	DEVICE_BTRM = 1 << 4,
	DEVICE_HIDRM = 1 << 5,
	DEVICE_HIDKEYBOARD = 1 << 6,
	DEVICE_NUM = 7
};

class InitParam {
public:
	enum Mode {
		Mode_Normal = 0,
		Mode_DigitalAnalog = 1,
		Mode_CommonDialog = 2,
		Mode_DigitalOnly = 3
	};

	int32_t m_mode;
};

/* Every device's data of this frame and the frame before. */
class Data {
public:
	const pad::Data *m_pad_data;
	const pad::Data *m_pad_data_pre;
	const touchscreen::Data *m_touch_data;
	const touchscreen::Data *m_touch_data_pre;
	const touchscreen::Data *m_touch_rear_data;
	const touchscreen::Data *m_touch_rear_data_pre;
	const motionsensor::Data *m_motion_data;
	const motionsensor::Data *m_motion_data_pre;
	const btrm::Data *m_btrm_data;
	const btrm::Data *m_btrm_data_pre;
	const hidrm::Data *m_bdrmc_data;
	const hidrm::Data *m_bdrmc_data_pre;
	const hidkeyboard::Data *m_keyboard_data;
	const hidkeyboard::Data *m_keyboard_data_pre;
};

int32_t Init(InitParam &init_param);
int32_t End();
bool IsOwnershipReleasable(uint32_t device, int32_t all);
int32_t Update(float diff_time);
int32_t UpdateListener();
int32_t UpdateGesture(bool event);
void Lock(uint32_t device);
void Unlock(uint32_t device);
bool IsOwnershipGotten(uint32_t device, int32_t all);
void GetOwnership(uint32_t device);
void GetBtrmOwnership();
void ReleaseOwnership(uint32_t device);
void ReleaseOwnershipImmediate(uint32_t device);
void ReleaseBtrmOwnership();
void SetImposeMode(uint32_t mode);

}
}

/* Each device in the mask locked for a time; no Sony name found for the NID. */
extern "C" void ScePafMisc_CAABF0B0(uint32_t device, float time);

#endif /* PAF_INPUTDEVICE_INPUTDEVICE_H */
