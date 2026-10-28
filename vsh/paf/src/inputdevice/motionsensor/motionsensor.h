#ifndef PAF_INPUTDEVICE_MOTIONSENSOR_H
#define PAF_INPUTDEVICE_MOTIONSENSOR_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct MotionSensorData {
	uint32_t timestamp;
	float accelerometer[4] __attribute__((aligned(8)));
	float gyroscope[4];
	float device_quat[4];
	float rotation_matrix[4][4];
	float basic_orientation[4];
	unsigned char dataInfo;
} MotionSensorData;

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
