/*
 * paf::inputdevice::touchscreen: the front and back touch panels, read through SceTouch.
 *
 * PROVISIONAL: what inputdevice.cpp calls, until touchscreen (0x81044bd8..) is converted.
 */
#ifndef PAF_INPUTDEVICE_TOUCHSCREEN_TOUCHSCREEN_H
#define PAF_INPUTDEVICE_TOUCHSCREEN_TOUCHSCREEN_H

#include <stdint.h>

namespace paf {
namespace inputdevice {
namespace touchscreen {

class Data;

class InitParam {
public:
	int32_t m_mode;
};

int32_t Init(InitParam &init_param);
int32_t End();
int32_t Update(float diff_time, const Data **data, const Data **rear_data,
	       const Data **data_pre, const Data **rear_data_pre);
void Lock(uint32_t port);
void Unlock(uint32_t port);
int32_t Activate(uint32_t port);
int32_t Deactivate(uint32_t port);
int32_t ActivateOwnershipRegion(uint32_t port, bool activate);
bool IsOwnershipReleasable(uint32_t port);

}
}
}

/* A touch panel locked for a time; no Sony name found for the NID. */
extern "C" void ScePafMisc_2CF401D9(uint32_t port, float time);

#endif /* PAF_INPUTDEVICE_TOUCHSCREEN_TOUCHSCREEN_H */
