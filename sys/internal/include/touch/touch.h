/* SceTouch (os0:kd/bootimage/touch) imports absent from the public kernel headers. */
#pragma once
#include <stddef.h>
#include <scetypes.h>
#include <syscon/syscon.h>
#include <sysmem/suspend.h>
#include <threadmgr/threadmgr.h>

struct touchDeviceInfo {
	unsigned short vendor;
	unsigned short product;		/* nonzero once the panel answers */
	unsigned short firmware;
	unsigned char detail[2];	/* reported after the firmware version; passed through */
	unsigned reserved;
};

_Static_assert(sizeof(struct touchDeviceInfo) == 12, "touch device info size");

/* vitasdk: ksceTouchGetDeviceInfo */
int SceTouchForDriver_FD2E0278(unsigned port, struct touchDeviceInfo *out);

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

/* vitasdk: ksceTouchRegisterVirtualTouchDriver */
int SceTouchForDriver_03C3AC7C(const TouchVirtualDriver *driver);

/* Front-to-back process order used for touch-region ownership. */
typedef struct TouchProcessPriorities {
	SceUInt32 count;
	SceUID processIds[16];
} TouchProcessPriorities;

_Static_assert(sizeof(TouchProcessPriorities) == 68, "touch process priorities");
_Static_assert(offsetof(TouchProcessPriorities, count) == 0, "touch priority count");
_Static_assert(offsetof(TouchProcessPriorities, processIds) == 4, "touch priority PIDs");

int SceTouchForDriver_3F3FBF69(const TouchProcessPriorities *priorities);


/* vitasdk: ksceTouchSetRegionAttr */
int SceTouchForDriver_10A998DE(unsigned port, unsigned region, unsigned attr);
/* vitasdk: ksceTouchSetSamplingState */
int SceTouchForDriver_1247257A(unsigned port, unsigned state);
/* vitasdk: ksceTouchGetSamplingState */
int SceTouchForDriver_174D0333(unsigned port, unsigned *state);
/* vitasdk: ksceTouchSetEnableFlag */
int SceTouchForDriver_328E38B0(unsigned port, int enable);
int SceTouchForDriver_384F6C68(unsigned port, unsigned timeout);
int SceTouchForDriver_423F26D3(void);
int SceTouchForDriver_4E6C4558(unsigned panels);
int SceTouchForDriver_63DCF2FE(void);
/* vitasdk: ksceTouchRead */
int SceTouchForDriver_70C8AACE(unsigned port, SceTouchData *out, unsigned count);
/* vitasdk: ksceTouchSetRegion */
int SceTouchForDriver_7985B164(unsigned port, unsigned region, const short *rect, unsigned attr);
/* vitasdk: ksceTouchGetPanelInfo */
int SceTouchForDriver_937DB4C0(unsigned port, SceTouchPanelInfo *out);
/* vitasdk: ksceTouchReadRegion */
int SceTouchForDriver_9A91F624(unsigned port, SceTouchData *out, unsigned count, unsigned regions);
/* vitasdk: ksceTouchPeekRegion */
int SceTouchForDriver_9B3F7207(unsigned port, SceTouchData *out, unsigned count, unsigned regions);
/* vitasdk: ksceTouchSetTouchEmulationData */
int SceTouchForDriver_A32529B8(const SceTouchData *front, const SceTouchData *back);
/* vitasdk: ksceTouchPeek */
int SceTouchForDriver_BAD1960B(unsigned port, SceTouchData *out, unsigned count);
/* vitasdk: ksceTouchActivateRegion */
int SceTouchForDriver_C7F0DAA0(unsigned port, unsigned region, unsigned enable);
int SceTouchForDriver_CDE79889(unsigned port);	/* vitasdk: ksceTouchDisableTouchForce */
int SceTouchForDriver_DDCF5145(void);
int SceTouchForDriver_E1C5D95F(unsigned port, unsigned region);
int SceTouchForDriver_E41B4BF8(unsigned port);	/* vitasdk: ksceTouchEnableTouchForce */
/* vitasdk: ksceTouchEnableIdleTimerCancelSetting */
int SceTouchForDriver_EBF4C86A(unsigned port, int enable);
int sceTouchActivateRegion(unsigned port, unsigned region, unsigned enable);
int sceTouchClearRegion(unsigned port, unsigned region);
int sceTouchDisableTouchForce(unsigned port);
int sceTouchDisableTouchForceExt(unsigned port);
int sceTouchEnableIdleTimerCancelSetting(unsigned port, int enable);
int sceTouchEnableTouchForce(unsigned port);
int sceTouchEnableTouchForceExt(unsigned port);
int sceTouchGetDeviceInfo(unsigned port, void *out);
int sceTouchGetPixelDensity(void *front, void *back);
int sceTouchGetPixelDensity2(void *frontX, void *frontY, void *backX, void *backY);
int sceTouchGetSamplingStateExt(unsigned port, void *out);
int sceTouchPeek2(unsigned device, unsigned port, void *out, unsigned count);
int sceTouchPeekRegion(unsigned port, void *out, unsigned count, unsigned regions);
int sceTouchPeekRegionExt(unsigned port, void *out, unsigned count, unsigned regions);
int sceTouchRead2(unsigned device, unsigned port, void *out, unsigned count);
int sceTouchReadRegion(unsigned port, void *out, unsigned count, unsigned regions);
int sceTouchReadRegionExt(unsigned port, void *out, unsigned count, unsigned regions);
int sceTouchSetCtrlpCallback(void (*callback)(unsigned, SceTouchData *));
int sceTouchSetProcessPrivilege(unsigned privilege);
int sceTouchSetRegion(unsigned port, unsigned region, const void *rect, unsigned attr);
int sceTouchSetRegionAttr(unsigned port, unsigned region, unsigned attr);
int sceTouchSetSamplingState(unsigned port, unsigned state);
int sceTouchSetSamplingStateExt(unsigned port, unsigned state);
