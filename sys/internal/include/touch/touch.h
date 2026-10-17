/* SceTouch (os0:kd/bootimage/touch) imports absent from the public kernel headers. */
#pragma once
#include <stddef.h>
#include <psp2kern/types.h>
#include <syscon/syscon.h>
#include <sysmem/suspend.h>

struct touchDeviceInfo {
	unsigned short vendor;
	unsigned short product;		/* nonzero once the panel answers */
	unsigned short firmware;
	unsigned char detail[2];	/* reported after the firmware version; passed through */
	unsigned reserved;
};

_Static_assert(sizeof(struct touchDeviceInfo) == 12, "touch device info size");

int ksceTouchGetDeviceInfo(unsigned port, struct touchDeviceInfo *out);

/* Sony's SDK touch.h types, as the kernel touch drivers keep them */
#define SCE_TOUCH_PORT_FRONT	0
#define SCE_TOUCH_PORT_BACK	1
#define SCE_TOUCH_PORT_MAX_NUM	2
#define SCE_TOUCH_MAX_REPORT	8

typedef struct SceTouchPanelInfo {
	SceInt16 minAaX, minAaY, maxAaX, maxAaY;	/* active area */
	SceInt16 minDispX, minDispY, maxDispX, maxDispY;	/* display area */
	SceUInt8 minForce, maxForce;
	SceUInt8 rsv[30];
} SceTouchPanelInfo;

typedef struct SceTouchReport {
	SceUInt8 id;
	SceUInt8 force;
	SceInt16 x, y;
	SceInt8 rsv[8];
	SceUInt16 info;
} SceTouchReport;

typedef struct SceTouchData {
	/* the drivers' buffers keep it 4-byte aligned */
	SceUInt64 timeStamp __attribute__((packed, aligned(4)));
	SceUInt32 status;
	SceUInt32 reportNum;
	SceTouchReport report[SCE_TOUCH_MAX_REPORT];
} SceTouchData;

typedef struct TouchVirtualSampleRequest {
	SceUInt8 sampleCount;
	SceUInt8 reserved[7];
	SceTouchData *samples[4];
} TouchVirtualSampleRequest;

typedef int (*TouchVirtualSampleCallback)(int port,
					TouchVirtualSampleRequest *request);

typedef struct TouchVirtualDriver {
	TouchVirtualSampleCallback getSample;
} TouchVirtualDriver;

_Static_assert(sizeof(TouchVirtualSampleRequest) == 24, "virtual touch sample request");
_Static_assert(sizeof(TouchVirtualDriver) == 4, "virtual touch driver");

int ksceTouchRegisterVirtualTouchDriver(const TouchVirtualDriver *driver);

/* Front-to-back process order used for touch-region ownership. */
typedef struct TouchProcessPriorities {
	SceUInt32 count;
	SceUID processIds[16];
} TouchProcessPriorities;

_Static_assert(sizeof(TouchProcessPriorities) == 68, "touch process priorities");
_Static_assert(offsetof(TouchProcessPriorities, count) == 0, "touch priority count");
_Static_assert(offsetof(TouchProcessPriorities, processIds) == 4, "touch priority PIDs");

int SceTouchForDriver_3F3FBF69(const TouchProcessPriorities *priorities);

/* names unknown: touch panel queries of the syscon */
int SceSysconForDriver_030D447F(void *info);		/* both panels' device info (newer syscon) */
int SceSysconForDriver_9B6B8BB9(SceUInt32 *version);	/* front panel firmware version */
int SceSysconForDriver_C4A61241(SceUInt32 *version);	/* back panel firmware version */
int SceSysconForDriver_142D5E82(void);
int SceSysconForDriver_9DA2A5AB(void);
int SceSysconForDriver_1546A141(int front, int back);

int sceKernelDelayThread(SceUInt32 usec);
int ksceSysconGetTouchpanelDeviceInfo(void *info);
