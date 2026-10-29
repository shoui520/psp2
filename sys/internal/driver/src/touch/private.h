/* SceTouch: what its units share with each other and with SceTouchDummy, which Sony built
 * from the same ring, tracking and user API units.
 * Private names and fields without a recovered SDK name remain unknown.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <fw.h>
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
	unsigned char compat[2];		/* report without force (old SDK / panel) */
	unsigned reserved;
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

/* the regions of every process with a priority, for one panel */
struct touchProcessInfo {
	unsigned count;
	struct {
		int pid;
		unsigned privilege;
		struct touchRegion regions[8];
	} processes[16];
};

_Static_assert(sizeof(struct touchProcessInfo) == 0x884, "process info layout");
_Static_assert(sizeof(SceTouchData) == 0x90, "touch data layout");
_Static_assert(sizeof(struct touchSample) == 0xd0, "sample layout");
_Static_assert(sizeof(struct touchRing) == 0x3410, "ring layout");
_Static_assert(sizeof(struct touchProcessPort) == 0x3498, "process port layout");
_Static_assert(sizeof(struct touchProcess) == 0x6950, "process layout");

/* ring.c */
int initializeRing(struct touchRing *ring);
int resetRingReader(struct touchRing *ring, int kind);
int readRing(struct touchRing *ring, int kind, SceTouchData *out, unsigned count,
	     int pid, unsigned regions, unsigned flags);
struct touchSample *nextRingSample(struct touchRing *ring);
void advanceRing(struct touchRing *ring);
struct touchSample *lastRingSample(struct touchRing *ring);

/* tracking.c */
int initializeTracking(struct touchTracking *tracking);
int updateTracking(struct touchTracking *tracking, const SceTouchData *data);
int moveNodes(struct touchNode **from, struct touchNode **to);

/* touch_user.c */
int initializeUser(void);

/* the physical panels' unit (touch.c, touch_dummy.c) */
int readRegionExt(unsigned port, SceTouchData *out, unsigned count, unsigned regions);
int peekRegionExt(unsigned port, SceTouchData *out, unsigned count, unsigned regions);
int setSamplingStateExt(unsigned port, unsigned state);
int getSamplingStateExt(unsigned port, unsigned *state);
int enableTouchForceExt(unsigned port);
int disableTouchForceExt(unsigned port);
int getPixelDensity(float *front, float *back);
int getPixelDensity2(float *frontX, float *frontY, float *backX, float *backY);
int setProcessPrivilege(unsigned privilege);
int sceTouchGetProcessInfo(unsigned port, struct touchProcessInfo *out);
int readVirtualWait(unsigned device, unsigned port, SceTouchData *out, unsigned count);
int readVirtualPeek(unsigned device, unsigned port, SceTouchData *out, unsigned count);

/* SceTouch only: touch.c */
int isAltPanel(unsigned port);

/* parse.c */
extern unsigned s_forceReport, s_forceFilterMode;
void setForceReport(unsigned enable);
void setForceFilterMode(unsigned mode);
void setClampCoordinates(unsigned clamp);
int parseSamples(unsigned kinds, const unsigned char *buffer, unsigned size,
		 SceTouchData *front, SceTouchData *back);

/* filter.c */
void initializeFilter(void);
void forceSpread(unsigned port, SceTouchData *data, unsigned flags);
void forceRange(unsigned port, SceTouchData *data, unsigned flags);
void setFilterEnable(unsigned enable);
