/*
 * The seven DMA controllers, one 0x1000-byte register page each, in pairs:
 * DMAC0-1 at 0xe3000000, DMAC2-3 at 0xe5000000, DMAC4-5 at 0xe0400000 (DMAC5
 * serves the crypto engine) and DMAC6 at 0xe50c0000. SceDmacmgr maps them as
 * SceDmacmgrDmac<n>Reg; the register layouts follow below. Addresses
 * from those memblocks and the henkaku wiki's Physical Memory page.
 */
#pragma once

#include <stddef.h>
#include <psp2common/types.h>

#define DMAC0_REG_BASE			0xe3000000
#define DMAC1_REG_BASE			0xe3010000
#define DMAC2_REG_BASE			0xe5000000
#define DMAC3_REG_BASE			0xe5010000
#define DMAC4_REG_BASE			0xe0400000
#define DMAC5_REG_BASE			0xe0410000
#define DMAC6_REG_BASE			0xe50c0000
#define DMAC_REG_SIZE			0x1000

/* DMAC5's key ring (SceSblDMAC5DmacKRBase): 32 key slots of 256 bits that DMAC5
 * operations name by number (the henkaku wiki's DMAC page) */
#define DMAC5_KEYRING_REG_BASE		0xe04e0000
#define DMAC5_KEYRING_SLOTS		32

typedef struct SceDmac5KeyringSlot {
	volatile unsigned int words[8];
} SceDmac5KeyringSlot;

typedef struct SceDmac5KeyringRegisters {
	SceDmac5KeyringSlot slot[DMAC5_KEYRING_SLOTS];
	volatile unsigned int nonSecureAccess;	/* bit n: slot n readable in Non-Secure state */
	unsigned int reserved[0x2ff];
} SceDmac5KeyringRegisters;

_Static_assert(sizeof(SceDmac5KeyringSlot) == 0x20, "keyring slot");
_Static_assert(offsetof(SceDmac5KeyringRegisters, nonSecureAccess) == 0x400, "keyring access");
_Static_assert(sizeof(SceDmac5KeyringRegisters) == DMAC_REG_SIZE, "keyring page");

/* one channel's registers, 0x80 bytes */
typedef struct SceDmacChannelReg {
	SceUInt32 src;
	SceUInt32 dst;
	SceSize len;
	SceUInt32 cmd;
	SceUInt32 srcKeyringId;
	SceUInt32 iv;
	SceSize blockSize;
	SceUInt32 ctrl;				/* 0x1c: 0 stops the channel */
	SceUInt32 chainStart;		/* 0x20: the first tag of a chain */
	SceUInt32 stat;				/* 0x24: SCE_KERNEL_DMAC_STAT_* */
	SceUInt32 intrStatus;			/* 0x28: DMAC_INTR_*, written back to clear */
	SceUInt32 coherencyMask;
	SceUInt32 ivCoherencyMask;
	SceUInt32 setValue;			/* 0x34: for SCE_KERNEL_DMAC_CMD_OP_SET */
	SceUInt32 reserved0;
	SceUInt32 trngValue;
	SceUInt32 errorSrc;
	SceUInt32 errorDst;
	SceSize errorLen;			/* 0x48: what the failing tag left undone */
	SceUInt32 currentTag;		/* 0x4c */
	SceUInt32 reserved1[12];
} SceDmacChannelReg;

#define DMAC_INTR_COMPLETE	0x00000001
#define DMAC_INTR_ERROR		0x00000002

/* the command flag that makes a tag raise the completion interrupt */
#define DMAC_CMD_INTERRUPT	0x00001000

/* DMAC5 channel: its external key follows the other channel's registers */
typedef struct SceDmac5ChannelReg {
	SceDmacChannelReg reg;
	SceUInt8 reserved[0x180];
	SceUInt32 key[16];			/* 0x200 */
} SceDmac5ChannelReg;

/* DMAC4's interrupt gates, one per four channels */
typedef struct SceDmac4GateReg {
	SceUInt32 activeIntr;
	SceUInt32 intrMask;
} SceDmac4GateReg;

typedef struct SceDmac4Reg {
	SceDmacChannelReg ch[16];
	SceUInt32 accessControl[2];		/* 0x800: see SceDmacRegisters */
	SceUInt8 reserved1[8];
	SceUInt32 busErrorAddress;
	SceUInt32 busErrorAttribute;
	SceUInt32 secureBusErrorAddress;
	SceUInt32 secureBusErrorAttribute;
	SceUInt8 reserved2[0x900 - 0x820];
	SceDmac4GateReg gate[4];		/* 0x900: interrupts 0x78..0x7b */
	SceDmac4GateReg secureGate[4];		/* 0x920: set up by the TZS IntrMgr */
} SceDmac4Reg;

/* DMAC0-3, 5 and 6: two channels, then the secure-side words. accessControl
 * is written by the TZS IntrMgr's monitor calls 0x168 and 0x169; it starts
 * word 0 with one bit per channel set. */
typedef struct SceDmacRegisters {
	SceDmacChannelReg ch[2];
	SceUInt32 accessControl[2];		/* 0x100 */
	SceUInt8 reserved1[8];
	SceUInt32 busErrorAddress;		/* 0x110 */
	SceUInt32 busErrorAttribute;
	SceUInt32 secureBusErrorAddress;
	SceUInt32 secureBusErrorAttribute;
} SceDmacRegisters;

_Static_assert(sizeof(SceDmacChannelReg) == 0x80, "SceDmacChannelReg size");
_Static_assert(__builtin_offsetof(SceDmacRegisters, accessControl) == 0x100, "DMAC access");
_Static_assert(__builtin_offsetof(SceDmac4Reg, accessControl) == 0x800, "DMAC4 access");
_Static_assert(__builtin_offsetof(SceDmac4Reg, secureGate) == 0x920, "DMAC4 secure gates");
_Static_assert(__builtin_offsetof(SceDmac5ChannelReg, key) == 0x200, "DMAC5 key");
_Static_assert(__builtin_offsetof(SceDmac4Reg, gate) == 0x900, "DMAC4 gates");
