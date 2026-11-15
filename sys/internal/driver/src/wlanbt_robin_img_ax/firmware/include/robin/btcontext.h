/*
 * The Bluetooth link controller's contexts (bt/lccontext.c, 0x42b66 - 0x43030): the clock requests the
 * controller makes, the SoC registers its start-up sets, and the small state the coexistence
 * decisions use. The names of the firmware's placeholders are kept (btLmpLc...Context); the roles are
 * inferred from what the code does.
 */
#ifndef ROBIN_BTCONTEXT_H
#define ROBIN_BTCONTEXT_H

#include <stdint.h>

/* 0x55284: the coexistence state of the controller. */
struct BtLcContext {
	uint8_t node;		/* highest activity count at which the controller counts as idle (5) */
	uint8_t slot;		/* lowest activity count at which it counts as busy (4) */
	uint8_t window;		/* cleared by the dispatch and parse hooks */
	uint8_t activeCount;	/* the link count (3 at reset), 1 when the controller has one */
	uint8_t bitmap;		/* set (1) by event 0xf1, cleared by 0xf2 */
};
extern struct BtLcContext btLmpLcContext;			/* 0x55284 */
#define btLmpLcActiveCount	(btLmpLcContext.activeCount)	/* 0x55287 */

extern uint16_t *btLinkRate;					/* 0x400f934 */
extern uint8_t btLinkMaskContext;				/* 0x400f7ff */
extern int32_t btLinkMaskHeader;				/* 0x400f8c8 */
extern uint32_t btLmpLcEntryCount[8];				/* 0x59ea8 */

unsigned int btLmpLcHandleContext(void);			/* 0x42b66 */
uint8_t *btLmpLcProcessContext(uint8_t *record);		/* 0x42b8a */
int btLmpLcUpdateContext(void);					/* 0x42b98 */
int btLmpLcConfigureContext(void);				/* 0x42ba0 */
unsigned int btLmpLcCheckContext(void);				/* 0x42ce8 */
unsigned int btLmpLcPrepareContext(void);			/* 0x42f30 */
int btLmpLcDispatchContext(void);				/* 0x42f38 */
uint8_t *btLmpLcParseContext(void);				/* 0x42f46 */
int btLmpLcBuildContext(uint32_t *remaining);			/* 0x42f4e */
void btLmpLcComputeContext(void);				/* 0x42f94 */
int btLmpLcQueryContext(int link, const uint8_t *event);	/* 0x42f96 */
int btLmpLcResetContext(int link, const uint8_t *event);	/* 0x42ff2 */
void btLmpLcSyncContext(void);					/* 0x43030 */
void btLmpLcRecordState(void);					/* 0x42b3c */

/* Not converted yet, or in another file. */
unsigned int btLmpLcTrackFrame(void);				/* 0x47fc4 */
unsigned int hostCmd0003Version(void);				/* 0x27de0 */
void *btLmpLcFormatContext(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);	/* 0x432bc */
unsigned int btLmpLcResetEntry(void);				/* 0x438b0 */

#endif
