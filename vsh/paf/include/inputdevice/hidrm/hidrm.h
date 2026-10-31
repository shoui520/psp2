/*
 * paf::inputdevice::hidrm: the HID remote (SceCtrl's BD remote callback).
 *
 * PROVISIONAL: what inputdevice.cpp calls, until hidrm (0x81191010..) is converted.
 */
#ifndef PAF_INPUTDEVICE_HIDRM_HIDRM_H
#define PAF_INPUTDEVICE_HIDRM_HIDRM_H

#include <stdint.h>

namespace paf {
namespace inputdevice {
namespace hidrm {

class Data;

class InitParam {
public:
	int32_t m_mode;
};

int32_t Init(InitParam &init_param);
int32_t End();
int32_t Update(float diff_time, const Data **data, const Data **data_pre);
void Lock();
int32_t Unlock();
void LockTimer(float time);
int32_t Activate();
int32_t Deactivate();
bool IsOwnershipReleasable();

}
}
}

#endif /* PAF_INPUTDEVICE_HIDRM_HIDRM_H */
