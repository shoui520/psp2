/*
 * paf::inputdevice::btrm: the Bluetooth remote, read through SceBt's AVRCP
 * events. A callback collects the keys pressed and released since the last
 * frame; Update hands them out once per frame.
 */
#ifndef PAF_INPUTDEVICE_BTRM_BTRM_H
#define PAF_INPUTDEVICE_BTRM_BTRM_H

#include <stdint.h>

namespace paf {
namespace inputdevice {
namespace btrm {

class InitParam {
public:
	enum Mode {
		Mode_Normal = 0,
		/* the keys held are masked once the system UI comes and goes */
		Mode_DigitalAnalog = 1,
		Mode_CommonDialog = 2
	};

	int32_t m_mode;
};

/* The remote's keys held, in paf's keys. */
class Data {
public:
	enum Keycode {
		BTRM_PLAY = 0x1,
		BTRM_STOP = 0x2,
		BTRM_PAUSE = 0x4,
		BTRM_FORWARD = 0x8,
		BTRM_BACKWARD = 0x10,
		BTRM_FAST_FORWARD = 0x20,
		BTRM_REWIND = 0x40
	};

	uint32_t buttons;
};

typedef void (*DeviceHandler)(Data *data);

int32_t Init(InitParam &init_param);
int32_t Activate();
int32_t Deactivate();
int32_t End();
void Unlock();
void SetMask(uint32_t mask);
int32_t Update(float diff_time, const Data **data, const Data **data_pre);
DeviceHandler SetDeviceHandler(DeviceHandler new_handler);
uint32_t GetMask();
void Lock();
bool IsLocked();
bool IsOwnershipReleasable();

}
}
}

/* The remote locked for a time; no Sony name found for the NID. */
extern "C" void ScePafMisc_F9C3A155(float time);

#endif /* PAF_INPUTDEVICE_BTRM_BTRM_H */
