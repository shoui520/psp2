/*
 * The Bluetooth link controller's contexts (bt/lccontext.c, 0x42b66 - 0x43030): the clock requests the
 * controller makes, the SoC registers its start-up sets, and the small state the coexistence
 * decisions use. The names of the firmware's placeholders are kept (btLmpLc...Context); the roles are
 * inferred from what the code does.
 */
#ifndef ROBIN_BTCONTEXT_H
#define ROBIN_BTCONTEXT_H

#include <stdint.h>
#include "robin/osa.h"

/* 0x55284: the coexistence state of the controller. */
struct BtLcContext {
	uint8_t node;		/* highest activity count at which the controller counts as idle (5) */
	uint8_t slot;		/* lowest activity count at which it counts as busy (4) */
	uint8_t window;		/* cleared by the dispatch and parse hooks */
	uint8_t activeCount;	/* the link count (3 at reset), 1 when the controller has one */
	uint8_t bitmap;		/* set (1) by event 0xf1, cleared by 0xf2 */
	uint8_t reserved5;
	uint16_t rate;		/* the profile the coexistence code was told last (btLmpLcApplyContext) */
	struct OsaIdleHook idleHook;	/* 0x5528c: btLmpLcResolveContext, from btLmpLcScheduleContext */
};

/* The state of one of the controller's two targets (0 the Bluetooth device, 1 the AMP), 16 bytes. */
struct BtLcTarget {
	uint8_t enabled;		/* the release is armed (event 1 of btLmpLcReleaseContext) */
	uint8_t flag;			/* the target was asked to release and is not yet */
	uint8_t pending;		/* a release was selected by btLmpLcSelectContext() */
	uint8_t idle;			/* the host transport was told to go idle for it */
	uint8_t mode;			/* 0xff: the target is released without power-save handshake */
	uint8_t delay;			/* milliseconds the target needs, 0xff: none */
	uint16_t timeout;		/* milliseconds after which the release is due, 0: at once */
	uint32_t since;			/* the event clock when the delay began */
	uint8_t phase;			/* 3: released, waiting for btLmpLcInspectContext() */
	uint8_t reserved13[3];
};
extern struct BtLcTarget btLmpLcTarget[2];			/* 0x59ec8 */
extern uint8_t btLinkIndexRecord;				/* 0x400f7fd */
extern uint8_t btLinkMaskState;					/* 0x400f7fe */
extern uint8_t btDeviceEntry;					/* 0x400c7b8 */
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
void btLmpLcApplyContext(void);					/* 0x430d0 */
int btLmpLcResolveContext(void);				/* 0x431ee */
int btLmpLcScheduleContext(void);				/* 0x43208 */
int btLmpLcNotifyContext(int link, int type, const void *data, unsigned int length);	/* 0x4324c */
void btLmpLcInspectContext(int link, int notify);		/* 0x432ea */
int btLmpLcTrackContext(int link);				/* 0x43348 */
int btLmpLcManageContext(int link);				/* 0x433f0 */
void btLmpLcAcquireContext(int link);				/* 0x43436 */
void btLmpLcReleaseContext(int link, int event, int argument);	/* 0x4347c */
int btLmpLcSetupContext(int link, const uint8_t *event);	/* 0x435b0 */
int btLmpLcClearContext(int link, const uint8_t *event);	/* 0x435d0 */
int btLmpLcConvertContext(int link, const uint8_t *event);	/* 0x435e4 */
int btLmpLcVerifyContext(int link);				/* 0x435f8 */
int btLmpLcSelectContext(int link, int kind, const uint8_t *request);	/* 0x43608 */
int btLmpLcAdjustContext(int link);				/* 0x436c8 */
int btLmpLcQueryContext(int link, const uint8_t *event);	/* 0x42f96 */
int btLmpLcResetContext(int link, const uint8_t *event);	/* 0x42ff2 */
void btLmpLcSyncContext(void);					/* 0x43030 */
void btLmpLcRecordState(void);					/* 0x42b3c */

int btLmpLcFormatContext(int link, int kind, int type, int value);	/* 0x432bc */

unsigned int btLmpLcResetEntry(void);				/* 0x438b0 */

/* Not converted yet, or in another file. */
unsigned int btLmpLcTrackFrame(void);				/* 0x47fc4 */
unsigned int hostCmd0003Version(void);				/* 0x27de0 */

#endif
