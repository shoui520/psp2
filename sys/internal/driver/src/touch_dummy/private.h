/* SceTouchDummy: the state its source files share.
 * Private names and fields without a recovered SDK name remain unknown.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <sysmem/cpu.h>
#include <sysmem/sysmem.h>
#include <threadmgr/threadmgr.h>
#include <intrmgr/intrmgr.h>
#include <sysmem/procevent.h>
#include <motion_dummy/motion_dummy.h>
#include <processmgr/processmgr.h>
#include <sysmem/sysevent.h>
#include <intrmgr/syscall.h>
#include <touch_dummy/touch_dummy.h>
#include <touch/touch.h>

struct touchOwner {
	int pid;
	unsigned short region, attr;
};
struct touchSample {
	SceTouchData data;
	struct touchOwner owners[8];
};
struct touchRect {
	short x, y, width, height;
};
struct touchRegion {
	int active;
	struct touchRect rect;
	unsigned attr;
};
struct touchRing {
	struct touchSample samples[64];
	int readers[2], writeIndex;
};
struct touchProcessPort {
	unsigned eventBit, reserved;
	struct touchRegion regions[8];
	struct touchRing ring;
};
struct touchProcess {
	int pid;
	unsigned char sampling[2], force[2];
	unsigned privilege;
	int state;
	unsigned timestamp;
	unsigned short generation;
	unsigned char padding[6];
	unsigned sdkVersion;
	struct touchProcessPort ports[2];
};
struct touchNode {
	unsigned char id, reportIndex, padding[2];
	struct touchOwner owner;
	struct touchNode *next;
};
struct touchTracking {
	struct touchNode *added, *active, *free;
	struct touchNode nodes[8];
};
struct touchPort {
	struct touchDeviceInfo deviceInfo;
	unsigned reportMax;
	float densityX, densityY;		/* pixels per mm */
	unsigned char forceMode, forceRequest, forceChange, reserved;
	unsigned allocated, stopped;
	int events[2];
	SceTouchPanelInfo panel;
	struct touchTracking tracking;
	unsigned lastTouchTime, timeout;
	unsigned char forceLevel[2];		/* raised / lowered force setting */
	unsigned char enableState, padding;
};
struct physicalState {
	int state;
	unsigned activeMask, modeMask, idleMask, processCount;
	int pids[16];
	unsigned priorityCount;
	int priorities[16];
	struct touchPort ports[2];
	int timer, lock, priorityEnabled, sysHandler, procHandler;
	unsigned char forceDefault[2];
	unsigned short generation;
	int plsId;
	int (*ctrlCallback)(unsigned, SceTouchData *);
	const TouchVirtualDriver *virtualDriver;
	unsigned reserved;
};
struct virtualRing {
	SceTouchData samples[64];
	int reader, writeIndex;
};
struct virtualProcessPort {
	unsigned eventBit, reserved;
	struct virtualRing ring;
};
struct virtualProcess {
	int pid;
	unsigned reserved;
	int state;
	unsigned timestamp;
	unsigned short generation;
	unsigned char padding[6];
	struct virtualProcessPort ports[4];
};
struct virtualState {
	unsigned processCount;
	int pids[16];
	unsigned priorityCount;
	int priorities[16];
	struct { unsigned allocated, stopped; int event; } ports[4];
	int lock, plsId;
	unsigned short generation, padding;
};
struct touchStorage {
	struct touchSample front __attribute__((aligned(8)));
	SceTouchData emulation[2];
	struct physicalState physical;
	struct virtualState virtual;
	struct touchSample back __attribute__((aligned(8)));
	int readMutex;
	unsigned padding0;
	SceTouchData readBuffer[64];
	int peekMutex;
	unsigned padding1;
	SceTouchData peekBuffer[64];
	SceTouchData virtualSamples[4];
	SceTouchData virtualBuffer[64];
};

extern struct touchStorage storage;
#define touch storage.physical
#define pad storage.virtual

_Static_assert(offsetof(struct touchStorage, back) == 0x560, "back buffer layout");
_Static_assert(offsetof(struct touchStorage, virtualSamples) == 0x4e40, "virtual buffers layout");
_Static_assert(sizeof(SceTouchData) == 0x90, "touch data layout");
_Static_assert(sizeof(struct touchSample) == 0xd0, "sample layout");
_Static_assert(sizeof(struct touchRing) == 0x3410, "ring layout");
_Static_assert(sizeof(struct touchProcessPort) == 0x3498, "process port layout");
_Static_assert(sizeof(struct touchProcess) == 0x6950, "process layout");
_Static_assert(sizeof(struct touchPort) == 0xf4, "port layout");
_Static_assert(sizeof(struct physicalState) == 0x2a8, "physical state layout");
_Static_assert(sizeof(struct virtualState) == 0xc4, "virtual state layout");
_Static_assert(sizeof(struct virtualProcess) == 0x9058, "virtual process layout");

/* ring.c */
int initializeRing(struct touchRing *ring);
int resetRingReader(struct touchRing *ring, int kind);
int readRing(struct touchRing *ring, int kind, SceTouchData *out, unsigned count,
	     int pid, unsigned regions, unsigned flags);
struct touchSample *nextRingSample(struct touchRing *ring);
void advanceRing(struct touchRing *ring);
struct touchSample *lastRingSample(struct touchRing *ring);

/* touch_dummy.c */
int readRegionExt(unsigned port, SceTouchData *out, unsigned count, unsigned regions);
int peekRegionExt(unsigned port, SceTouchData *out, unsigned count, unsigned regions);
int setSamplingStateExt(unsigned port, unsigned state);
int getSamplingStateExt(unsigned port, unsigned *state);
int enableTouchForceExt(unsigned port);
int disableTouchForceExt(unsigned port);
int getPixelDensity(float *front, float *back);
int getPixelDensity2(float *frontX, float *frontY, float *backX, float *backY);
int setProcessPrivilege(unsigned privilege);
int sceTouchGetProcessInfo(unsigned port, void *out);

/* touch_user.c */
int initializeUser(void);

/* tracking.c */
int initializeTracking(struct touchTracking *tracking);
int updateTracking(struct touchTracking *tracking, const SceTouchData *data);
int moveNodes(struct touchNode **from, struct touchNode **to);

/* virtual.c */
int readVirtual(unsigned port, SceTouchData *out, int count, unsigned flags);
int createVirtualProcess(int pid);
int removeVirtualProcess(int pid);
int suspendVirtualProcess(int pid);
int checkVirtualProcess(int pid);
int stopVirtualProcess(int pid);
int resumeVirtualProcess(int pid);
int initializeVirtual(void);
int sampleVirtual(TouchVirtualSampleCallback driver);
int readVirtualWait(unsigned device, unsigned port, SceTouchData *out, unsigned count);
int readVirtualPeek(unsigned device, unsigned port, SceTouchData *out, unsigned count);

/* a process's state of this module, created on the first lookup when create is set */
static inline int lookupProcess(int pid, int pls, void **out, int create) /* name unknown */
{
	int ret;

	*out = NULL;
	ret = sceKernelGetRemoteKPLS(pid, pls, out, create);
	if (ret < 0)
		*out = NULL;
	return ret;
}

/* the lowest bit allocated does not have yet, now allocated; 0 when all are taken */
static inline unsigned allocateBit(unsigned *allocated) /* name unknown */
{
	unsigned i, bit;

	for (i = 0; i < 32; i++) {
		bit = 1U << i;
		if (!(*allocated & bit)) {
			*allocated |= bit;
			return bit;
		}
	}
	return 0;
}
