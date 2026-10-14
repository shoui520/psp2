#pragma once

#include <stdint.h>
#include <stddef.h>

/* ScePervasiveReset (physical 0xe3101000) and ScePervasiveGate (0xe3102000):
 * one control word per device, at the same offset in both pages. In the reset
 * page a set bit holds the device in reset; in the gate page it runs the
 * device's clock. Most devices use bit 0; GPU, DSI and USB take a mask. The
 * secure devices (Allegrex, SonyRegbus, CompatRAM, SDIO, VIP register access)
 * are changed through the secure monitor. Device names follow the henkaku
 * wiki's Pervasive page; RAZ/WI words are reserved.
 */
typedef struct ScePervasiveDeviceRegisters {
	volatile uint32_t arm;			/* gate only */
	volatile uint32_t armDebugger;
	volatile uint32_t reserved1[2];
	volatile uint32_t gpu;
	volatile uint32_t reserved2[3];
	volatile uint32_t allegrex;
	volatile uint32_t sonyRegbus;
	volatile uint32_t compatRam;
	volatile uint32_t reserved3;
	volatile uint32_t venezia;
	volatile uint32_t vip;
	volatile uint32_t reserved4[2];
	volatile uint32_t sdio[2];
	volatile uint32_t debugPa;
	volatile uint32_t dbgSdio;
	volatile uint32_t dmac[7];
	volatile uint32_t reserved5;
	volatile uint32_t csi[2];
	volatile uint32_t reserved6[2];
	volatile uint32_t dsi[2];
	volatile uint32_t iftu;
	volatile uint32_t unidentifiedDevice;	/* only its own exports touch it */
	volatile uint32_t usb[3];
	volatile uint32_t reserved7;
	volatile uint32_t sdif[4];
	volatile uint32_t msif;
	volatile uint32_t reserved8[3];
	volatile uint32_t i2s[8];
	volatile uint32_t srcMix[3];
	volatile uint32_t reserved9;
	volatile uint32_t spdif;
	volatile uint32_t reserved10[3];
	volatile uint32_t gpio;
	volatile uint32_t spi[3];
	volatile uint32_t i2c[2];
	volatile uint32_t reserved11[2];
	volatile uint32_t uart[7];
	volatile uint32_t reserved12[6];
	volatile uint32_t debugBus;
	volatile uint32_t reserved13[2];
	volatile uint32_t lpddr2[2];		/* main, sub */
	volatile uint32_t reserved14[2];
	volatile uint32_t timer;
	volatile uint32_t reserved15;
	volatile uint32_t spm;			/* 4: SPM32, 8: SPM128 */
	volatile uint32_t veneziaRegisterAccess; /* needed for bus error dumps */
	volatile uint32_t vipRegisterAccess;
	volatile uint32_t reserved16[3];
	volatile uint32_t bigmac;
	volatile uint32_t reserved17[923];
} ScePervasiveDeviceRegisters;

typedef ScePervasiveDeviceRegisters ScePervasiveResetRegisters;
typedef ScePervasiveDeviceRegisters ScePervasiveGateRegisters;

/* ScePervasiveTas0 through Tas7 occupy eight pages at 0xe3108000.
 * PLACEHOLDER: only the dedicated test-and-set lock register is identified;
 * generic physical register access also exposes the remaining words.
 */
typedef struct ScePervasiveTasChannel {
	volatile uint32_t lock;
	volatile uint32_t unidentifiedRegisters[1023];
} ScePervasiveTasChannel;

typedef struct ScePervasiveTasRegisters {
	ScePervasiveTasChannel channels[8];
} ScePervasiveTasRegisters;

_Static_assert(offsetof(ScePervasiveDeviceRegisters, gpu) == 0x10, "gpu");
_Static_assert(offsetof(ScePervasiveDeviceRegisters, allegrex) == 0x20, "allegrex");
_Static_assert(offsetof(ScePervasiveDeviceRegisters, venezia) == 0x30, "venezia");
_Static_assert(offsetof(ScePervasiveDeviceRegisters, vip) == 0x34, "vip");
_Static_assert(offsetof(ScePervasiveDeviceRegisters, sdio) == 0x40, "sdio");
_Static_assert(offsetof(ScePervasiveDeviceRegisters, dmac) == 0x50, "dmac");
_Static_assert(offsetof(ScePervasiveDeviceRegisters, csi) == 0x70, "csi");
_Static_assert(offsetof(ScePervasiveDeviceRegisters, dsi) == 0x80, "dsi");
_Static_assert(offsetof(ScePervasiveDeviceRegisters, unidentifiedDevice) == 0x8c, "unidentifiedDevice");
_Static_assert(offsetof(ScePervasiveDeviceRegisters, usb) == 0x90, "usb");
_Static_assert(offsetof(ScePervasiveDeviceRegisters, sdif) == 0xa0, "sdif");
_Static_assert(offsetof(ScePervasiveDeviceRegisters, msif) == 0xb0, "msif");
_Static_assert(offsetof(ScePervasiveDeviceRegisters, i2s) == 0xc0, "i2s");
_Static_assert(offsetof(ScePervasiveDeviceRegisters, srcMix) == 0xe0, "srcMix");
_Static_assert(offsetof(ScePervasiveDeviceRegisters, spdif) == 0xf0, "spdif");
_Static_assert(offsetof(ScePervasiveDeviceRegisters, gpio) == 0x100, "gpio");
_Static_assert(offsetof(ScePervasiveDeviceRegisters, spi) == 0x104, "spi");
_Static_assert(offsetof(ScePervasiveDeviceRegisters, i2c) == 0x110, "i2c");
_Static_assert(offsetof(ScePervasiveDeviceRegisters, uart) == 0x120, "uart");
_Static_assert(offsetof(ScePervasiveDeviceRegisters, debugBus) == 0x154, "debugBus");
_Static_assert(offsetof(ScePervasiveDeviceRegisters, lpddr2) == 0x160, "lpddr2");
_Static_assert(offsetof(ScePervasiveDeviceRegisters, timer) == 0x170, "timer");
_Static_assert(offsetof(ScePervasiveDeviceRegisters, spm) == 0x178, "spm");
_Static_assert(offsetof(ScePervasiveDeviceRegisters, vipRegisterAccess) == 0x180, "vipRegisterAccess");
_Static_assert(offsetof(ScePervasiveDeviceRegisters, bigmac) == 0x190, "bigmac");
_Static_assert(sizeof(ScePervasiveDeviceRegisters) == 0x1000, "device register page");
_Static_assert(sizeof(ScePervasiveTasChannel) == 0x1000, "TAS channel page");
_Static_assert(sizeof(ScePervasiveTasRegisters) == 0x8000, "TAS register pages");
