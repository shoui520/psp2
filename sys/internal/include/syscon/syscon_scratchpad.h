/*
 * Syscon's scratch pad: 0x100 bytes that survive suspend, read and written
 * with SceSysconForDriver_299B1CE7/SceSysconForDriver_E26488B9 (commands 0x90/0x91,
 * sceSysconReadScratchPad/WriteScratchPad) at an offset into this layout.
 * Layout after the henkaku wiki (Ernie, "Syscon Scratch Pad").
 */

#pragma once

#include <scetypes.h>
#include <stddef.h>

typedef struct SceSysconScratchPad {
	SceUInt8 reserved0[8];
	SceUInt32 powerOnTime;
	SceUInt32 resumeContextPA;	/* second_loader resumes from here; -1: none */
	SceUInt8 currentTick[5];	/* tick >> 19 */
	SceUInt8 padding[3];
	SceUInt8 auxTick[8][5];		/* SceRtc's aux clocks, tick >> 19 */
	SceUInt8 reserved1[0xa0];
	SceUInt8 dipsw[0x20];		/* the CP's DIP switches on a DevKit */
} SceSysconScratchPad;

_Static_assert(offsetof(SceSysconScratchPad, resumeContextPA) == 0xc, "resumeContextPA");
_Static_assert(offsetof(SceSysconScratchPad, auxTick) == 0x18, "auxTick");
_Static_assert(offsetof(SceSysconScratchPad, dipsw) == 0xe0, "dipsw");
_Static_assert(sizeof(SceSysconScratchPad) == 0x100, "SceSysconScratchPad size");

#define SYSCON_SCRATCH_PAD(field)	offsetof(SceSysconScratchPad, field)
