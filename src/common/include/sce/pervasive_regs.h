#pragma once

#include <stdint.h>
#include <stddef.h>

/* The Pervasive pages' physical addresses, one 0x1000-byte page each */
#define PERVASIVE_MISC_BASE		0xe3100000
#define PERVASIVE_RESET_BASE		0xe3101000
#define PERVASIVE_GATE_BASE		0xe3102000
#define PERVASIVE_BASECLK_BASE		0xe3103000
#define PERVASIVE_VID_BASE		0xe3104000
/* 0xe3105000: SceUartClkgenReg, see uart_regs.h */
#define PERVASIVE_MAILBOX_BASE		0xe3106000
#define PERVASIVE_TAS_BASE(n)		(0xe3108000 + (n) * 0x1000)
#define PERVASIVE2_BASE			0xe3110000
#define PERVASIVE_PAGE_SIZE		0x1000

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

/* ScePervasiveMisc (0xe3100000). Names after the henkaku wiki's Pervasive page
 * where it has one, otherwise after the drivers that use the word
 * (usbserv, sdif, motion SPI, bbmc's UART, audio, sdstor).
 */
#define SCE_PERVASIVE_UART5_PULL_DOWN_PINS	0x3	/* TX|CTS, or RX|RFR */

typedef struct ScePervasiveMiscRegisters {
	volatile uint32_t revision;			/* SoC revision */
	volatile uint32_t cacheStatus;			/* bit 1: L2 cache defective */
	volatile uint32_t reserved0[6];
	volatile uint32_t suspendControl;		/* bit 31 set before the CPUs park */
	volatile uint32_t reserved1[24];
	volatile uint32_t usbMode[3];			/* per USB port, bit 0 */
	volatile uint32_t reserved2[32];
	volatile uint32_t sdifConfig[4];		/* bits 8-10 */
	volatile uint32_t reserved3;
	volatile uint32_t sdifVoltage;			/* 1 << port: 1.8 V */
	volatile uint32_t reserved4[2];
	volatile uint32_t secureBusErrorAttribute;	/* PERVASIVE_SYS_SBEATB */
	volatile uint32_t secureBusErrorAddress;	/* PERVASIVE_SYS_SBEADR */
	volatile uint32_t busErrorAttribute;		/* PERVASIVE_SYS_BEATB */
	volatile uint32_t busErrorAddress;		/* PERVASIVE_SYS_BEADR */
	volatile uint32_t dsi0Control;
	volatile uint32_t csiControl;
	volatile uint32_t dsi1Control;
	volatile uint32_t reserved5;
	volatile uint32_t usbBufferAddress[3];
	volatile uint32_t usbBufferSize[3];
	volatile uint32_t reserved6[5];
	volatile uint32_t usbPortControl;		/* 2 bits per USB port */
	volatile uint32_t reserved7[2];
	volatile uint32_t motionSpiDisable;		/* bit 0 */
	volatile uint32_t uart5PullDownTx;		/* bit 0 TX, bit 1 CTS */
	volatile uint32_t audioMode;
	volatile uint32_t gameCard;			/* bit 0 */
	volatile uint32_t spareEnable;			/* bit 0; nothing calls its exports */
	volatile uint32_t reserved8[4];
	volatile uint32_t uart5PullDownRx;		/* bit 0 RX, bit 1 RFR */
	volatile uint32_t reserved9[84];
	volatile uint32_t hardwareInfo[3];
	volatile uint32_t reserved10;
	volatile uint32_t sdifMode;			/* 3 bits per port, port * 8 */
	volatile uint32_t reserved11[827];
} ScePervasiveMiscRegisters;

_Static_assert(offsetof(ScePervasiveMiscRegisters, suspendControl) == 0x20, "misc suspend");
_Static_assert(offsetof(ScePervasiveMiscRegisters, usbMode) == 0x84, "misc usb mode");
_Static_assert(offsetof(ScePervasiveMiscRegisters, sdifConfig) == 0x110, "misc sdif");
_Static_assert(offsetof(ScePervasiveMiscRegisters, sdifVoltage) == 0x124, "misc sdif voltage");
_Static_assert(offsetof(ScePervasiveMiscRegisters, secureBusErrorAttribute) == 0x130, "misc bus error");
_Static_assert(offsetof(ScePervasiveMiscRegisters, dsi0Control) == 0x140, "misc dsi0");
_Static_assert(offsetof(ScePervasiveMiscRegisters, dsi1Control) == 0x148, "misc dsi1");
_Static_assert(offsetof(ScePervasiveMiscRegisters, usbBufferAddress) == 0x150, "misc usb buffer");
_Static_assert(offsetof(ScePervasiveMiscRegisters, usbBufferSize) == 0x15c, "misc usb buffer size");
_Static_assert(offsetof(ScePervasiveMiscRegisters, usbPortControl) == 0x17c, "misc usb port");
_Static_assert(offsetof(ScePervasiveMiscRegisters, motionSpiDisable) == 0x188, "misc motion spi");
_Static_assert(offsetof(ScePervasiveMiscRegisters, spareEnable) == 0x198, "misc spare");
_Static_assert(offsetof(ScePervasiveMiscRegisters, uart5PullDownRx) == 0x1ac, "misc uart5 rx");
_Static_assert(offsetof(ScePervasiveMiscRegisters, hardwareInfo) == 0x300, "misc hardware info");
_Static_assert(offsetof(ScePervasiveMiscRegisters, sdifMode) == 0x310, "misc sdif mode");
_Static_assert(sizeof(ScePervasiveMiscRegisters) == 0x1000, "misc register page");

/* One DSI pixel clock: a PLL and dividers behind three reset words. Setting a
 * clock holds output, dividers and PLL in reset (8, 4, 0), programs the PLL
 * (pllConfig bit 4 selects it, then its coefficients), releases the PLL, then
 * programs the dividers and source and releases dividers and output in turn.
 */
typedef struct ScePervasivePixelClockRegisters {
	volatile uint32_t pllReset;
	volatile uint32_t dividerReset;
	volatile uint32_t outputReset;
	volatile uint32_t reserved0[5];
	volatile uint32_t source;
	volatile uint32_t pllConfig;			/* bit 4: the PLL is used */
	volatile uint32_t reserved1[2];
	volatile uint32_t pllCoefficient[3];
	volatile uint32_t reserved2;
	volatile uint32_t divider[3];
	volatile uint32_t reserved3;
} ScePervasivePixelClockRegisters;

_Static_assert(offsetof(ScePervasivePixelClockRegisters, source) == 0x20, "pixel clock source");
_Static_assert(offsetof(ScePervasivePixelClockRegisters, pllCoefficient) == 0x30, "pixel clock pll");
_Static_assert(offsetof(ScePervasivePixelClockRegisters, divider) == 0x40, "pixel clock divider");
_Static_assert(sizeof(ScePervasivePixelClockRegisters) == 0x50, "pixel clock block");

/* ScePervasiveBaseClk (physical 0xe3103000): the clocks fed to the SoC's
 * devices, one select word each. Roles follow the henkaku wiki's Pervasive
 * page and the ScePervasiveForDriver exports that write them; the words the
 * wiki marks secure (CMeP, bus, DRAM, compat) are only written by the
 * bootloaders and the secure monitor.
 */
typedef struct ScePervasiveBaseClkRegisters {
	volatile uint32_t armMultiplier;		/* low nibble: the ARM clock step */
	volatile uint32_t armDivisor;			/* 16 - speed */
	volatile uint32_t reserved0[2];
	volatile uint32_t gpuClock;			/* core << 16 | mp */
	volatile uint32_t reserved1[3];
	volatile uint32_t veneziaClock;
	volatile uint32_t reserved2[3];
	volatile uint32_t vipClock;
	volatile uint32_t reserved3[3];
	volatile uint32_t cmepClock;
	volatile uint32_t cameraBusClock;
	volatile uint32_t reserved4[2];
	volatile uint32_t busClock;
	volatile uint32_t reserved5[3];
	volatile uint32_t auxClockDivider;		/* divider - 1 from source 0 */
	volatile uint32_t auxClockSource;
	volatile uint32_t gatedClockSelect;		/* bit 1 enable, bit 0 source */
	volatile uint32_t reserved6;
	volatile uint32_t audioClock;			/* 0x10000 | enable */
	volatile uint32_t reserved7[7];
	volatile uint32_t dramMainClock;
	volatile uint32_t dramSubClock;
	volatile uint32_t reserved8[3];
	volatile uint32_t auxClockEnable;		/* cleared while the aux clock changes */
	volatile uint32_t reserved9[2];
	volatile uint32_t msifClock;			/* bits 0-1, bit 16 */
	volatile uint32_t reserved10[4];
	volatile uint32_t compatClock;
	volatile uint32_t reserved11[14];
	ScePervasivePixelClockRegisters dsi1PixelClock;
	volatile uint32_t reserved12[12];
	ScePervasivePixelClockRegisters dsi0PixelClock;
	volatile uint32_t cecClockOut;			/* HDMI CEC clock output enable */
	volatile uint32_t reserved13[7];
	volatile uint32_t dmac5Clock;
	volatile uint32_t reserved14[7];
	volatile uint32_t srcClock;			/* audio sample-rate converter */
	volatile uint32_t sysClock;
	volatile uint32_t reserved15[890];
} ScePervasiveBaseClkRegisters;

_Static_assert(offsetof(ScePervasiveBaseClkRegisters, gpuClock) == 0x10, "base clock gpu");
_Static_assert(offsetof(ScePervasiveBaseClkRegisters, cameraBusClock) == 0x44, "base clock camera bus");
_Static_assert(offsetof(ScePervasiveBaseClkRegisters, auxClockDivider) == 0x60, "base clock aux");
_Static_assert(offsetof(ScePervasiveBaseClkRegisters, audioClock) == 0x70, "base clock audio");
_Static_assert(offsetof(ScePervasiveBaseClkRegisters, dramMainClock) == 0x90, "base clock dram");
_Static_assert(offsetof(ScePervasiveBaseClkRegisters, auxClockEnable) == 0xa4, "base clock aux enable");
_Static_assert(offsetof(ScePervasiveBaseClkRegisters, msifClock) == 0xb0, "base clock msif");
_Static_assert(offsetof(ScePervasiveBaseClkRegisters, compatClock) == 0xc4, "base clock compat");
_Static_assert(offsetof(ScePervasiveBaseClkRegisters, dsi1PixelClock) == 0x100, "base clock dsi1");
_Static_assert(offsetof(ScePervasiveBaseClkRegisters, dsi0PixelClock) == 0x180, "base clock dsi0");
_Static_assert(offsetof(ScePervasiveBaseClkRegisters, cecClockOut) == 0x1d0, "base clock cec");
_Static_assert(offsetof(ScePervasiveBaseClkRegisters, dmac5Clock) == 0x1f0, "base clock dmac5");
_Static_assert(offsetof(ScePervasiveBaseClkRegisters, srcClock) == 0x210, "base clock src");
_Static_assert(sizeof(ScePervasiveBaseClkRegisters) == 0x1000, "base clock page");

/* ScePervasive2 (physical 0xe3110000): bus protection and bus error words
 * (named in sce/buserror_es3.h by address) and the insert and interrupt states
 * of the memory card and the USB ports, per the henkaku wiki's Pervasive page.
 * Interrupt state words are acknowledged by writing back what was read.
 */
typedef struct ScePervasive2Registers {
	volatile uint32_t reserved0[972];
	volatile uint32_t usbPhyReady;			/* bit per port */
	volatile uint32_t usbPhyInterrupt;
	volatile uint32_t reserved1[2];
	volatile uint32_t memoryCardInsert;		/* bit 0 */
	volatile uint32_t memoryCardInterrupt;		/* bit 0 removed, bit 1 inserted */
	volatile uint32_t reserved2[2];
	volatile uint32_t usbVbus;			/* bit per port */
	volatile uint32_t usbVbusInterrupt;
	volatile uint32_t reserved3[42];
} ScePervasive2Registers;

_Static_assert(offsetof(ScePervasive2Registers, usbPhyReady) == 0xf30, "pervasive2 usb phy");
_Static_assert(offsetof(ScePervasive2Registers, memoryCardInsert) == 0xf40, "pervasive2 card");
_Static_assert(offsetof(ScePervasive2Registers, usbVbus) == 0xf50, "pervasive2 vbus");
_Static_assert(sizeof(ScePervasive2Registers) == 0x1000, "pervasive2 page");
