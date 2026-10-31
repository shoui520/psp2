#ifndef PAF_INPUTDEVICE_MOTIONSENSOR_H
#define PAF_INPUTDEVICE_MOTIONSENSOR_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "../../timer/timer.h"

typedef struct MotionSensorData {
	uint32_t timestamp;
	float accelerometer[4] __attribute__((aligned(8)));
	float gyroscope[4];
	float device_quat[4];
	float rotation_matrix[4][4];
	float basic_orientation[4];
	unsigned char dataInfo;
} MotionSensorData;

/* paf::list<Timer> (its C++ COMDATs are hidkeyboard.cpp's). */
typedef struct InputTimerNode {
	struct InputTimerNode *prev;
	struct InputTimerNode *next;
	Timer val;
} InputTimerNode;

typedef struct InputTimerList {
	InputTimerNode *root_node;
	int32_t count;
	unsigned char alloc_value;
	unsigned char alloc_node;
} InputTimerList;

typedef char InputTimerNodeSizeCheck[sizeof(InputTimerNode) == 36 ? 1 : -1];
typedef char InputTimerListSizeCheck[sizeof(InputTimerList) == 12 ? 1 : -1];

InputTimerList *InputTimerListConstruct(InputTimerList *self,
				       const void *allocator);
InputTimerList *InputTimerListDestroy(InputTimerList *self);

typedef void (*MotionSensorDeviceHandler)(MotionSensorData *data);

typedef char MotionSensorDataSizeCheck[
	sizeof(MotionSensorData) == 144 ? 1 : -1];
typedef char MotionSensorDataQuaternionOffsetCheck[
	offsetof(MotionSensorData, device_quat) == 40 ? 1 : -1];
typedef char MotionSensorDataInfoOffsetCheck[
	offsetof(MotionSensorData, dataInfo) == 136 ? 1 : -1];

MotionSensorData *MotionSensorDataConstruct(MotionSensorData *self);
void MotionSensorInitialize(void);

#define MotionSensorUnlock ScePafMisc_219CFEED
#define MotionSensorSetDeviceHandler ScePafMisc_BEA9803E
#define MotionSensorLock ScePafMisc_DEAA750C
#define MotionSensorIsLocked ScePafMisc_D350BADF
void MotionSensorUnlock(void);
MotionSensorDeviceHandler MotionSensorSetDeviceHandler(
	MotionSensorDeviceHandler new_handler);
void MotionSensorLock(void);
bool MotionSensorIsLocked(void);

#endif
