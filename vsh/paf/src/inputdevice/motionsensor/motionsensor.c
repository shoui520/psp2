#include "motionsensor.h"
#include "../inputdevice.h"
#include "../../std/stdc++/atexit.h"

static MotionSensorDeviceHandler motionDeviceHandler;
static MotionSensorData motionDataPre;
static MotionSensorData motionData;
static uint32_t motionLockCount;
static InputTimerList motionTimers;

void MotionSensorUnlock(void)
{
	if ((int32_t)motionLockCount > 0)
		--motionLockCount;
}

MotionSensorDeviceHandler MotionSensorSetDeviceHandler(
	MotionSensorDeviceHandler new_handler)
{
	MotionSensorDeviceHandler old_handler = motionDeviceHandler;

	motionDeviceHandler = new_handler;
	return old_handler;
}

void MotionSensorLock(void)
{
	++motionLockCount;
}

bool MotionSensorIsLocked(void)
{
	return (int32_t)motionLockCount > 0;
}

MotionSensorData *MotionSensorDataConstruct(MotionSensorData *self)
{
	self->device_quat[0] = 0.0f;
	self->device_quat[1] = 0.0f;
	self->device_quat[2] = 0.0f;
	self->device_quat[3] = 1.0f;
	return self;
}

void MotionSensorInitialize(void)
{
	extern void *__dso_handle;
	unsigned char allocator[4];

	MotionSensorDataConstruct(&motionDataPre);
	MotionSensorDataConstruct(&motionData);
	InputTimerListConstruct(&motionTimers, allocator);
	ScePafStdc_C95732BB(&motionTimers,
			  (void (*)(void *))InputTimerListDestroy, &__dso_handle);
}
