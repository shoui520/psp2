/*
 * The Cortex-A9 MPCore private region (PERIPHBASE): the snoop control unit,
 * the GIC CPU interface and distributor, and the PL310 after them (see
 * pl310_regs.h). SceKernelIntrMgr maps it as SceInterruptControllerReg; the
 * TZS IntrMgr in slb2 maps it too. Names after ARM's GIC documentation and
 * the henkaku wiki's Physical Memory and Interrupts pages.
 */
#pragma once

#include <psp2common/types.h>

#define PERIPH_REG_BASE			0x1a000000
#define PERIPH_REG_SIZE			0x2000

/* the GIC in the private region */
#define GIC_CPU_INTERFACE_OFFSET 0x100
#define GIC_DISTRIBUTOR_OFFSET 0x1000

/* ARM GIC distributor (PERIPHBASE + 0x1000) */
typedef struct GicDistributor {
	SceUInt32 control;		/* 0x000 */
	SceUInt32 type;			/* 0x004 */
	SceUInt32 reserved0[30];
	SceUInt32 group[32];		/* 0x080 */
	SceUInt32 setEnable[32];	/* 0x100 */
	SceUInt32 clearEnable[32];	/* 0x180 */
	SceUInt32 setPending[32];	/* 0x200 */
	SceUInt32 clearPending[32];	/* 0x280 */
	SceUInt32 active[32];		/* 0x300 */
	SceUInt32 reserved1[32];
	SceUInt8 priority[1024];	/* 0x400 */
	SceUInt8 target[1024];		/* 0x800 */
	SceUInt32 config[64];		/* 0xc00 two bits per interrupt */
	SceUInt32 reserved2[128];
	SceUInt32 softIntr;		/* 0xf00 */
} GicDistributor;

/* ARM GIC CPU interface (PERIPHBASE + 0x100) */
typedef struct GicCpuInterface {
	SceUInt32 control;		/* 0x00 */
	SceUInt32 priorityMask;		/* 0x04 */
	SceUInt32 binaryPoint;		/* 0x08 */
	SceUInt32 acknowledge;		/* 0x0c */
	SceUInt32 endOfIntr;		/* 0x10 */
} GicCpuInterface;
