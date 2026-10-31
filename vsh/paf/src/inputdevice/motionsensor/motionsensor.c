#include "motionsensor.h"
#include "../../std/string.h"
#include "../../std/stdc++/atexit.h"

static MotionSensorDeviceHandler motionDeviceHandler;
static MotionSensorData motionDataPre;
static MotionSensorData motionData;
static uint32_t motionLockCount;
static InputTimerList motionTimers;

InputTimerList *InputTimerListConstruct(InputTimerList *self,
				       const void *allocator)
{
	InputTimerNode *root;

	self->count = 0;
	root = PafNew(sizeof(*root));
	if (!root)
		ScePafStdc_D608246A();
	self->root_node = root;
	if (root) {
		root->prev = root;
		root->next = root;
	}
	return self;
}

InputTimerList *InputTimerListDestroy(InputTimerList *self)
{
	InputTimerNode *root = self->root_node;

	if (root) {
		InputTimerNode *node = root->next;

		while (node != root) {
			InputTimerNode *next = node->next;

			node->val.vtable->Destroy(&node->val);
			PafDelete(node);
			root = self->root_node;
			node = next;
		}
		PafDelete(root);
	}
	self->root_node = (InputTimerNode *)-1;
	self->count = -1;
	return self;
}

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
