/*
 * fw1 runtime: the debugger on the ARM side. It shares DRAM after the status
 * blocks: registers, debug points, and event bits it is interrupted for.
 *
 * Debug points are numbered 0 to 63 (32 and up: core 1's). The debugger groups
 * them in sets, a list of VipDebugPointSet; a point's record also carries values
 * the core's program reads (point 0: the decoder's rings).
 */
#pragma once

#include "runtime.h"

#define VIP_DEBUG_POINTS	64
#define VIP_DEBUG_POINT_CORE1	0x20

/* the kind of a point, in bits 6-7 of VipDebugPoint.state */
#define VIP_POINT_KIND_SHIFT	6
#define VIP_POINT_ANY		0xff	/* findDebugPoint: any kind */
#define VIP_POINT_ONCE		1	/* becomes VIP_POINT_HIT when hit */
#define VIP_POINT_HIT		2
#define VIP_POINT_VALUES	2	/* findDebugPoint: points with values */

typedef struct VipDebugPoint {
	uint16_t count;
	uint8_t state;			/* the kind, and flags below it */
	uint8_t result;
	uint8_t action;			/* checkDebugPoint's report */
	uint8_t argument;
	uint16_t reserved;
	uint32_t values[];
} VipDebugPoint;

typedef struct VipDebugPointSet {
	uint32_t reserved0;
	VipDebugPoint **points;		/* by point number */
	uint32_t reserved1;
	struct VipDebugPointSet *next;
	uint32_t enabled[2];		/* bit masks, points 0-31 and 32-63 */
	uint32_t hit[2];		/* the points that have been hit */
} VipDebugPointSet;

/* the debugger's registers, from VIP_DEBUG_REGISTERS */
typedef struct VipDebugRegisters {
	uint32_t resetValue[2];			/* VIP_DEBUG_RESET_VALUE, per core */
	volatile uint32_t *semaphore;		/* guards the debug points */
	VipDebugPointSet *pointSets;
	uint32_t reserved0[6];
	uint32_t syncThreshold;			/* PU1: how far off the output resyncs */
	uint32_t reserved1[165];
	volatile uint32_t events;		/* events not yet delivered */
	volatile uint32_t eventMask;		/* events kept back, not interrupted for */
	volatile uint32_t *eventSemaphore;	/* guards events */
} VipDebugRegisters;

#define VIP_DEBUG		((VipDebugRegisters *)VIP_DEBUG_REGISTERS)

/* the debugger's memory areas, by kind (getDebugArea) */
#define VIP_DEBUG_AREA_COMMON	(VIP_DEBUG_REGISTERS + 0x100)
#define VIP_DEBUG_AREA_17	(VIP_DEBUG_REGISTERS + 0x500)
#define VIP_DEBUG_AREA_16	(VIP_DEBUG_REGISTERS + 0x600)
#define VIP_DEBUG_AREA_0	(VIP_DEBUG_REGISTERS + 0x700)

/* bits of g_pauseFlags: debug points that pause the core */
extern uint32_t g_pauseFlags;

VipDebugPoint *findDebugPoint(uint32_t point, uint32_t kind);
int checkDebugPoint(uint32_t point, uint32_t *action, uint32_t *argument,
	uint32_t *count);
int getDebugPointValue(uint32_t point, uint32_t which, uint32_t *value);
int setDebugPointResult(uint32_t point, uint32_t result);
int setDebugPointValue(uint32_t point, uint32_t which, uint32_t value);
int disableDebugPoint(uint32_t point);
int holdSchedule(uint32_t point);
int releaseSchedule(uint32_t point);
int readDebugRegister(uint32_t offset, uint32_t *value);
int writeDebugRegister(uint32_t offset, uint32_t value);
int getDebugArea(uint32_t kind, uint32_t *address);
void debugHook(void);
void stopForDebugger(uint32_t result, uint32_t state);
void setPauseFlag(uint32_t flag, uint32_t set);
int notifyDebugger(uint32_t mode, uint32_t events);
int takeDebugSemaphore(uint32_t mode);
void releaseDebugSemaphore(uint32_t mode);

/* the decoder tasks' debug point commands (pu0/decoder.c, pu1/decoder.c) */
#define COMMAND_NONE		0
#define COMMAND_OPEN		1
#define COMMAND_CLOSE		2
#define COMMAND_CLOSE_COUNT	8	/* the close command's size */

/* their results, and the command task's states */
#define RESULT_DONE		1
#define RESULT_BAD_ACTION	3
#define RESULT_BAD_COUNT	4
#define RESULT_BAD_STATE	5

#define DECODER_CLOSED		0
#define DECODER_OPENING		1
#define DECODER_OPEN		2
#define DECODER_CLOSING		3

/* how long the reset of the hardware (vipSaveHardware) waits for the DMA to stop */
#define RESET_POLLS		0x8000000
