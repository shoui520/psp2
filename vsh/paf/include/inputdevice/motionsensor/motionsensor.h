/*
 * paf::inputdevice::motionsensor: the accelerometer and gyroscope, read
 * through SceMotion once per frame, with the basic orientation of the device.
 */
#ifndef PAF_INPUTDEVICE_MOTIONSENSOR_MOTIONSENSOR_H
#define PAF_INPUTDEVICE_MOTIONSENSOR_MOTIONSENSOR_H

#include <stdint.h>
#include <motion.h>
#include <math/math.h>

namespace paf {
namespace inputdevice {
namespace motionsensor {

class InitParam {
public:
	int32_t m_mode;
};

/* One sample of SceMotion's state. */
class Data {
public:
	Data() : device_quat(math::quaternion::Identity())
	{
	}

	uint32_t timestamp;
	math::v3 accelerometer;
	math::v3 gyroscope;
	math::quaternion device_quat;
	math::matrix rotation_matrix;
	math::v3 basic_orientation;
	/* SceMotionState's dataInfo: bit 0, the sampling was forcibly stopped */
	uint8_t data_info;
};

typedef void (*DeviceHandler)(Data *data);

int32_t Init(InitParam &init_param);
int32_t Activate();
int32_t Deactivate();
int32_t End();
void Unlock();
int32_t Update(float diff_time, const Data **data, const Data **data_pre);
DeviceHandler SetDeviceHandler(DeviceHandler new_handler);
void Lock();
bool IsLocked();
bool IsOwnershipReleasable();

}
}
}

/* The motion sensor locked for a time; no Sony name found for the NID. */
extern "C" void ScePafMisc_BB74B3B8(float time);
/*
 * The tilt angles (degrees per axis) the basic orientation must pass before
 * it changes, set and read; no Sony names found for the NIDs.
 */
extern "C" void ScePafMisc_DC13D542(const SceFVector3 *threshold);
extern "C" void ScePafMisc_B4E6CA68(SceFVector3 *threshold);

#endif /* PAF_INPUTDEVICE_MOTIONSENSOR_MOTIONSENSOR_H */
