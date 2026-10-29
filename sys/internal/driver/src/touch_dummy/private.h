/* SceTouchDummy: the state its source files share.
 * Private names and fields without a recovered SDK name remain unknown.
 */
#pragma once
#include "../touch/private.h"

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
/* the 3.6x builds place the back panel's sample first, the older 4.00 build after the states */
struct touchStorage {
#if !FW_SINCE(0x04000000)
	struct touchSample back __attribute__((aligned(8)));
#endif
	struct touchSample front __attribute__((aligned(8)));
	SceTouchData emulation[2];
	struct physicalState physical;
	struct virtualState virtual;
#if FW_SINCE(0x04000000)
	struct touchSample back __attribute__((aligned(8)));
#endif
};

extern struct touchStorage storage;
#define touch storage.physical
#define pad storage.virtual

_Static_assert(offsetof(struct touchStorage, back) == (FW_SINCE(0x04000000) ? 0x560 : 0), "back buffer layout");
_Static_assert(sizeof(struct touchPort) == 0xf4, "port layout");
_Static_assert(sizeof(struct physicalState) == 0x2a8, "physical state layout");
_Static_assert(sizeof(struct virtualState) == 0xc4, "virtual state layout");
_Static_assert(sizeof(struct virtualProcess) == 0x9058, "virtual process layout");

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
