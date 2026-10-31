/*
 * paf::inputdevice::motionsensor: the accelerometer and gyroscope, read through SceMotion.
 *
 * PROVISIONAL: what inputdevice.cpp calls, until motionsensor.c (0x81043a74..) is converted.
 */
#ifndef PAF_INPUTDEVICE_MOTIONSENSOR_MOTIONSENSOR_H
#define PAF_INPUTDEVICE_MOTIONSENSOR_MOTIONSENSOR_H

#include <stdint.h>

namespace paf {
namespace inputdevice {
namespace motionsensor {

class Data;

class InitParam {
public:
	int32_t m_mode;
};

int32_t Init(InitParam &init_param);
int32_t End();
int32_t Update(float diff_time, const Data **data, const Data **data_pre);
void Lock();
void Unlock();
int32_t Activate();
int32_t Deactivate();
bool IsOwnershipReleasable();

}
}
}

/* The motion sensor locked for a time; no Sony name found for the NID. */
extern "C" void ScePafMisc_BB74B3B8(float time);

#endif /* PAF_INPUTDEVICE_MOTIONSENSOR_MOTIONSENSOR_H */
