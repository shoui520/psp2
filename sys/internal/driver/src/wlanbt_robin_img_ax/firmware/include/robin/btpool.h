/*
 * The Bluetooth controller's buffer pools in SQRAM (bt/lcpool.c, 0x43724 - 0x4381x): three pools of
 * blocks and the memory they are carved from. The objects are not converted yet; they stand at the
 * addresses of the 4.00 image. The roles are inferred.
 */
#ifndef ROBIN_BTPOOL_H
#define ROBIN_BTPOOL_H

#include <stdint.h>
#include "robin/buffer.h"

extern uint8_t btPoolHeaderMemory[0x3c70];		/* SQRAM 0xc00022f4: the memory of the table pool */
extern uint8_t btPoolFrameMemory[0x11a0];		/* SQRAM 0xc0001154: the memory of the event pool */
extern uint8_t btPoolDescriptorMemory[];		/* SQRAM 0xc0003c04: the memory of the packet pool */
extern struct OsaBufferPool btTablePool;		/* SQRAM 0xc0001118 */
extern uint8_t btTablePoolConfig[];			/* SQRAM 0xc0001124 */
extern uint8_t btTablePoolControl[];			/* SQRAM 0xc0005f64 */
extern struct OsaBufferPool btPacketPool;		/* SQRAM 0xc000113c */
extern uint8_t btPacketPoolConfig[];			/* SQRAM 0xc0001108 */
extern uint8_t btPacketPoolControl[];			/* SQRAM 0xc0005fe4 */
extern struct OsaBufferPool btEventPool;		/* SQRAM 0xc0001148 */
extern uint8_t btEventPoolConfig[];			/* SQRAM 0xc0001110 */
extern uint8_t btEventPoolControl[];			/* SQRAM 0xc0006004 */
extern uint32_t btEventPoolFailures;			/* SQRAM 0xc0001100: allocations the event pool refused */
extern uint32_t btPacketPoolFailures;			/* SQRAM 0xc0001104 */
extern uint8_t btRecordBuffers[][752];			/* SQRAM 0xc0006014 */
extern int (*btLimitParam)(void);			/* 0x400f154 */

void btLmpLcFilterContext(void);			/* 0x4377a */
uint8_t *btLmpLcHandleEntry(void);			/* 0x4377c */
int btLmpLcProcessEntry(void);				/* 0x437b2 */
uint8_t *btLmpLcUpdateEntry(void);			/* 0x437b8 */
int btLmpLcConfigureEntry(uint8_t *packet);		/* 0x437e0 */
uint8_t *btLmpLcCheckEntry(int index);			/* 0x437ea */
int btLmpLcRecordContext(void);				/* 0x43724 */

#endif
