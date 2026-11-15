/*
 * The link controller's host link buffers (bt/lcbuffer.c), and the ROM's host link state they use.
 * The ROM's names are provisional; their addresses are in boot/rom_syms.ld.
 */
#ifndef ROBIN_BTBUFFER_H
#define ROBIN_BTBUFFER_H

#include <stdint.h>

/* The host link's timing in the ROM (0x400c5f0), scaled with the radio's rate. */
struct BtHostTiming {
	uint8_t enabled;			/* 1: the timing follows the rate */
	uint8_t scale;				/* the rate bits of the link controller */
	uint8_t policy;
	uint8_t reserved3[5];
	uint32_t ticks[5];
	uint8_t slots[2];
	uint8_t reserved30[2];
};
_Static_assert(sizeof(struct BtHostTiming) == 32, "BtHostTiming");

extern struct BtHostTiming btHostTiming;			/* 0x400c5f0 */
extern uint8_t btLinkChannelState;				/* 0x400f72f */
extern uint8_t btLinkPendingSlot;				/* 0x400f72c: a slot to give back, 0: none */
extern uint8_t btLinkHostState[];				/* 0x400f740: the ROM's host link lists */
extern uint8_t *btLinkSlotTable;				/* 0x400f914: 8 bytes per slot */
extern uint16_t *btLinkSlotMask;				/* 0x400f904 */
extern void (*btLinkEndHook)(void);				/* 0x400edac */
extern void (*btLinkLimitHook)(uint8_t first, uint8_t second);	/* 0x400f1d0 */
extern void (*btNextWindow)(int link, uint32_t clock, uint32_t interval);	/* 0x400ed24 */
extern void (*btNextControl)(int kind, int link);		/* 0x400ecfc */
extern int (*btLinkSessionCheck)(int link);			/* 0x400f268 */
extern int (*btNextSession)(int link);				/* 0x400ed04: the clock of the link's next slot */
extern uint8_t btLinkSlotFactor;				/* 0x400f617 */
extern uint8_t btLinkSpanEnabled;				/* 0x400f7f2 */
extern uint8_t btLinkPayloadRegister;				/* 0x400f7fc */
extern const uint32_t romQueueLc;				/* ROM 0x03f41a44 */
extern const uint32_t romQueueLsc;				/* ROM 0x03f41a4c */

void btLmpLcHandleBuffer(void);					/* 0x44ca6 */
void btLmpLcProcessBuffer(void);				/* 0x44cb2 */
void btLmpLcUpdateBuffer(void);					/* 0x44db8 */
void btLmpLcConfigureBuffer(void);				/* 0x44ddc */
void btLmpLcCheckBuffer(unsigned int link);			/* 0x44ece */
int btLmpLcValidateBuffer(uint8_t *message);			/* 0x44f2c */
void btLmpLcPrepareBuffer(void);				/* 0x45066 */
int btLmpLcDispatchBuffer(uint8_t *command, const uint16_t *count, uint32_t a2, uint32_t a3, uint32_t a4,
			  uint32_t a5);				/* 0x4507e */
struct BtDeviceProfile;
void btLmpLcParseBuffer(struct BtDeviceProfile *profile, const struct BtDeviceProfile *source);	/* 0x45116 */
void btLmpLcBuildBuffer(int link);				/* 0x4514a */
void btLmpLcComputeBuffer(int link, uint32_t clock, unsigned int next, uint32_t interval);	/* 0x45220 */
void btLmpLcQueryBuffer(int link, uint32_t held);		/* 0x45246 */
void btLmpLcResetBuffer(uint8_t *command);			/* 0x452c2 */
void btLmpLcSyncBuffer(uint16_t *linkOut, uint8_t *slotOut, int clock, const uint8_t *request);	/* 0x45300 */

#endif /* ROBIN_BTBUFFER_H */
