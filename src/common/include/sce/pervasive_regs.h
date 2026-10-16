#pragma once

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

#ifndef __ASSEMBLER__
#include <stddef.h>
#include <stdint.h>

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
	/* unidentified; the secure boot loader releases it after the DRAM,
	 * with ScePervasiveMisc bootDeviceResetHold as its hold flag */
	volatile uint32_t bootDevice;
	volatile uint32_t reserved13;
	volatile uint32_t lpddr2[2];		/* main, sub */
	volatile uint32_t reserved14[2];
	volatile uint32_t timer;
	/* unidentified; released with the Debug Bus and Debug PA (dipsw 0x9f) */
	volatile uint32_t debugDevice;
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
_Static_assert(offsetof(ScePervasiveDeviceRegisters, bootDevice) == 0x158, "boot device");
_Static_assert(offsetof(ScePervasiveDeviceRegisters, debugDevice) == 0x174, "debug device");
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
	volatile uint32_t reserved1[4];
	/* set while the GPU, or Venezia and VIP, are powered off (SceDriverTzs) */
	volatile uint32_t gpuIsolation;			/* 0x34 */
	volatile uint32_t veneziaIsolation;
	/* cleared by the secure boot loader before it starts the device */
	volatile uint32_t compatRamIsolation;		/* 0x3c */
	volatile uint32_t sonyRegbusIsolation;
	volatile uint32_t lpddr2Isolation[2];		/* main, sub */
	volatile uint32_t reserved1b[2];
	/* bit 0: idle the bus before a power-on; it clears when taken, and
	 * busIdleStatus then reports it done */
	volatile uint32_t gpuBusIdleRequest;		/* 0x54 */
	volatile uint32_t veneziaBusIdleRequest;
	/* the same for CompatRAM (busIdleStatus bit 3) and SonyRegbus (bit 4) */
	volatile uint32_t compatRamBusIdleRequest;	/* 0x5c */
	volatile uint32_t sonyRegbusBusIdleRequest;
	volatile uint32_t reserved1c[8];
	volatile uint32_t usbMode[3];			/* per USB port, bit 0 */
	volatile uint32_t reserved2[8];
	volatile uint32_t pervasiveAccessMode;		/* 0xb0: set by monitor call 0x104 */
	volatile uint32_t tasAccessMode;		/* set by monitor call 0x105 */
	volatile uint32_t reserved2b[2];
	volatile uint32_t busIdleStatus;		/* 0xc0: bit 1 GPU, bit 2 Venezia; write 1 to clear */
	volatile uint32_t reserved2c[19];
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
	volatile uint32_t reserved9[30];
	/* bit 3: set by SKBL's boot() on Kermit 1.0 ES3.0 (wiki: only bits 0xffff R/W) */
	volatile uint32_t es3Config;			/* 0x228 */
	volatile uint32_t reserved9a[49];
	/* set while the device is held in reset (SceDriverTzs, the boot loaders) */
	volatile uint32_t bootDeviceResetHold;		/* 0x2f0 */
	volatile uint32_t reserved9b;
	volatile uint32_t compatRamResetHold;		/* 0x2f8 */
	volatile uint32_t allegrexResetHold;
	volatile uint32_t hardwareInfo[3];
	volatile uint32_t reserved10;
	volatile uint32_t sdifMode;			/* 3 bits per port, port * 8 */
	volatile uint32_t reserved11[827];
} ScePervasiveMiscRegisters;

_Static_assert(offsetof(ScePervasiveMiscRegisters, suspendControl) == 0x20, "misc suspend");
_Static_assert(offsetof(ScePervasiveMiscRegisters, gpuIsolation) == 0x34, "misc gpu isolation");
_Static_assert(offsetof(ScePervasiveMiscRegisters, gpuBusIdleRequest) == 0x54, "misc gpu bus idle");
_Static_assert(offsetof(ScePervasiveMiscRegisters, compatRamIsolation) == 0x3c, "misc compat isolation");
_Static_assert(offsetof(ScePervasiveMiscRegisters, compatRamBusIdleRequest) == 0x5c, "misc compat bus idle");
_Static_assert(offsetof(ScePervasiveMiscRegisters, es3Config) == 0x228, "misc ES3 config");
_Static_assert(offsetof(ScePervasiveMiscRegisters, bootDeviceResetHold) == 0x2f0, "misc boot device hold");
_Static_assert(offsetof(ScePervasiveMiscRegisters, usbMode) == 0x84, "misc usb mode");
_Static_assert(offsetof(ScePervasiveMiscRegisters, busIdleStatus) == 0xc0, "misc bus idle status");
_Static_assert(offsetof(ScePervasiveMiscRegisters, compatRamResetHold) == 0x2f8, "misc compat ram hold");
_Static_assert(offsetof(ScePervasiveMiscRegisters, pervasiveAccessMode) == 0xb0, "misc access mode");
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
	volatile uint32_t reserved8[2];
	volatile uint32_t allegrexClockEnable;		/* 0xa0, secure */
	volatile uint32_t auxClockEnable;		/* cleared while the aux clock changes */
	volatile uint32_t reserved9[2];
	volatile uint32_t msifClock;			/* bits 0-1, bit 16 */
	volatile uint32_t reserved10[3];
	volatile uint32_t allegrexBusClockEnable;	/* 0xc0, secure */
	volatile uint32_t compatClock;
	volatile uint32_t reserved11[14];
	ScePervasivePixelClockRegisters dsi1PixelClock;
	volatile uint32_t reserved12[12];
	ScePervasivePixelClockRegisters dsi0PixelClock;
	volatile uint32_t cecClockOut;			/* HDMI CEC clock output enable */
	volatile uint32_t reserved13[3];
	/* secure: bits 0-7 the Allegrex clock (0xa or 0xf), bit 26 set while
	 * the GPU powers on */
	volatile uint32_t allegrexClock;		/* 0x1e0 */
	volatile uint32_t reserved13b[3];
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
_Static_assert(offsetof(ScePervasiveBaseClkRegisters, allegrexClockEnable) == 0xa0, "base clock allegrex");
_Static_assert(offsetof(ScePervasiveBaseClkRegisters, auxClockEnable) == 0xa4, "base clock aux enable");
_Static_assert(offsetof(ScePervasiveBaseClkRegisters, allegrexBusClockEnable) == 0xc0, "base clock allegrex bus");
_Static_assert(offsetof(ScePervasiveBaseClkRegisters, allegrexClock) == 0x1e0, "base clock allegrex clock");
_Static_assert(offsetof(ScePervasiveBaseClkRegisters, msifClock) == 0xb0, "base clock msif");
_Static_assert(offsetof(ScePervasiveBaseClkRegisters, compatClock) == 0xc4, "base clock compat");
_Static_assert(offsetof(ScePervasiveBaseClkRegisters, dsi1PixelClock) == 0x100, "base clock dsi1");
_Static_assert(offsetof(ScePervasiveBaseClkRegisters, dsi0PixelClock) == 0x180, "base clock dsi0");
_Static_assert(offsetof(ScePervasiveBaseClkRegisters, cecClockOut) == 0x1d0, "base clock cec");
_Static_assert(offsetof(ScePervasiveBaseClkRegisters, dmac5Clock) == 0x1f0, "base clock dmac5");
_Static_assert(offsetof(ScePervasiveBaseClkRegisters, srcClock) == 0x210, "base clock src");
_Static_assert(sizeof(ScePervasiveBaseClkRegisters) == 0x1000, "base clock page");

/* ScePervasiveVid (physical 0xe3104000): the rail voltage settings, one
 * word per operating point, which ScePower reads into its voltage tables.
 */
typedef struct ScePervasiveVidRegisters {
	uint32_t reserved0;
	uint32_t armVoltage[5];		/* 0x04 */
	uint32_t reserved1[10];
	uint32_t compatVoltage[2];	/* 0x40 */
	uint32_t gpuVoltage[2];		/* 0x48 */
	uint32_t reserved2[4];
	uint32_t mediaVoltage[4];	/* 0x60 */
	uint32_t reserved3[13];
	uint32_t busVoltage[3];		/* 0xa4 */
	uint32_t reserved4[980];
} ScePervasiveVidRegisters;

_Static_assert(offsetof(ScePervasiveVidRegisters, compatVoltage) == 0x40, "vid compat");
_Static_assert(offsetof(ScePervasiveVidRegisters, mediaVoltage) == 0x60, "vid media");
_Static_assert(offsetof(ScePervasiveVidRegisters, busVoltage) == 0xa4, "vid bus");
_Static_assert(sizeof(ScePervasiveVidRegisters) == 0x1000, "vid page");

/* ScePervasive2 (physical 0xe3110000). Venezia's boot and capture windows
 * (vnz_wrapper; names ours, descriptive), the VIP and Venezia protection and
 * bus error words (named in sce/buserror_es3.h by address), and the insert and
 * interrupt states of the memory card and the USB ports, per the henkaku
 * wiki's Pervasive page. Interrupt state words are acknowledged by writing
 * back what was read. The Venezia fields keep the volatility vnz_wrapper's
 * accesses have: its debugger reaches the page through a plain pointer.
 */

/* a Venezia bus address window: region -> translated */
typedef struct VeneziaAddressWindow {
	unsigned int regionStart;
	unsigned int regionLimit;
	unsigned int translatedStart;
	unsigned int translatedLimit;
} VeneziaAddressWindow;

/* a set of OCP bus error target status words (the bus error decoder's
 * BET0, BET1, BEBT) */
typedef struct ScePervasive2BusErrorStatus {
	volatile uint32_t bet0;
	uint32_t reserved0[3];
	volatile uint32_t bet1;				/* 0x10 */
	volatile uint32_t bebt;
	uint32_t reserved1[10];
} ScePervasive2BusErrorStatus;

/* one OCP target's bus error words in Pervasive2 */
typedef struct ScePervasive2OcpErrorLog {
	uint32_t reserved0;
	volatile uint32_t address;
	volatile uint32_t attribute;
	uint32_t reserved1;
} ScePervasive2OcpErrorLog;

/* ScePervasive2Registers.ocpErrorLog[world][target] */
#define PERVASIVE2_OCP_SPM128		0
#define PERVASIVE2_OCP_SPM32		1
#define PERVASIVE2_OCP_COMPAT_RAM	2
#define PERVASIVE2_OCP_PERVASIVE2	3
#define PERVASIVE2_OCP_TARGETS		4

typedef struct ScePervasive2Registers {
	unsigned char reserved[0x100];
	unsigned int debuggerPhysicalBase;		/* 0x100 */
	unsigned char reservedDebugger[0xc];
	volatile unsigned int captureImagePhysicalAddress[2];	/* 0x110 */
	/* PLACEHOLDER: per-memory-class image control semantics remain unknown. */
	volatile unsigned int unidentifiedCaptureImageControl[2];
	volatile unsigned int captureAperturePhysicalBase;	/* 0x120 */
	unsigned char reservedBeforeCaptureWindow[0xdc];
	volatile unsigned int captureWindowControl;	/* 0x200 */
	volatile unsigned int lowerRangeBase;
	volatile unsigned int upperRangeBase;
	volatile unsigned int upperRangeEnd;
	volatile unsigned int lowerRangeEnd;		/* 0x210 */
	unsigned char reservedCaptureWindow[0x34];
	/* 0x248: VIP protection errors (BAP, VDPD, VDPM): ERRV, ERRC, ERRA */
	volatile uint32_t vipProtBap[3];
	volatile uint32_t vipProtVdpd[3];
	volatile uint32_t vipProtVdpm[3];
	unsigned char reservedVipProt[0x94];
	unsigned int mappingControl;			/* 0x300 */
	unsigned int kernelStart;
	unsigned int imageStart;
	unsigned int imageEnd;
	unsigned int kernelEnd;				/* 0x310 */
	unsigned char reservedMapping[0x34];
	volatile uint32_t veneProtReg[3];		/* 0x348: ERRV, ERRC, ERRA */
	unsigned char reservedVeneProt[0x2c];
	unsigned int imagePhysicalBase;			/* 0x380 */
	unsigned char reservedImage[0xc];
	VeneziaAddressWindow windows[3];		/* 0x390 */
	unsigned char reservedWindows[0x40];
	unsigned int bootPhysicalBase;			/* 0x400 */
	unsigned char reservedBoot[0x8fc];
	/* 0xd00: the OCP targets' error logs, non-secure then secure (0xd40) */
	ScePervasive2OcpErrorLog ocpErrorLog[2][PERVASIVE2_OCP_TARGETS];
	ScePervasive2BusErrorStatus busErrorStatus[2];	/* 0xd80 */
	unsigned char reservedBusError[0x8];
	unsigned int interruptControl;			/* 0xe08 */
	unsigned char reservedInterrupt[0x38];
	unsigned int bootControl;			/* 0xe44 */
	unsigned char reservedBeforeUsb[0xe8];
	volatile uint32_t usbPhyReady;			/* 0xf30: bit per port */
	volatile uint32_t usbPhyInterrupt;
	volatile uint32_t reservedUsbPhy[2];
	volatile uint32_t memoryCardInsert;		/* 0xf40: bit 0 */
	volatile uint32_t memoryCardInterrupt;		/* bit 0 removed, bit 1 inserted */
	volatile uint32_t reservedMemoryCard[2];
	volatile uint32_t usbVbus;			/* 0xf50: bit per port */
	volatile uint32_t usbVbusInterrupt;		/* port n: bit 4n rise, 4n + 1 fall */
	volatile uint32_t usbVbusInterruptEnable;
	volatile uint32_t reservedUsbVbus;
	volatile unsigned int mainMemoryPageBase;	/* 0xf60 */
	volatile unsigned int externalMemoryPageBase;
	/* PLACEHOLDER: page controls are cleared during capture activation. */
	volatile unsigned int unidentifiedCapturePageControl[2];
	volatile unsigned int capturePagePhysicalBase;	/* 0xf70 */
	unsigned char reservedBeforeAccessMode[0x4c];
	volatile uint32_t accessMode;			/* 0xfc0: set by monitor call 0x106 */
	unsigned char reservedEnd[0x3c];
} ScePervasive2Registers;

_Static_assert(offsetof(ScePervasive2Registers, captureImagePhysicalAddress) == 0x110,
	"Pervasive2 capture image");
_Static_assert(offsetof(ScePervasive2Registers, mappingControl) == 0x300, "Pervasive2 mapping");
_Static_assert(offsetof(ScePervasive2Registers, accessMode) == 0xfc0, "Pervasive2 access mode");
_Static_assert(sizeof(ScePervasive2Registers) == PERVASIVE_PAGE_SIZE, "Pervasive2 page");
_Static_assert(offsetof(ScePervasive2Registers, windows) == 0x390, "Pervasive2 windows");
_Static_assert(offsetof(ScePervasive2Registers, bootControl) == 0xe44, "Pervasive2 boot control");
_Static_assert(offsetof(ScePervasive2Registers, usbPhyReady) == 0xf30, "Pervasive2 USB PHY");
_Static_assert(offsetof(ScePervasive2Registers, vipProtBap) == 0x248, "Pervasive2 VIP protection");
_Static_assert(offsetof(ScePervasive2Registers, veneProtReg) == 0x348, "Pervasive2 Venezia protection");
_Static_assert(offsetof(ScePervasive2Registers, busErrorStatus) == 0xd80, "Pervasive2 bus error status");
_Static_assert(offsetof(ScePervasive2Registers, ocpErrorLog[1][PERVASIVE2_OCP_SPM32].address) == 0xd54,
	"Pervasive2 OCP error logs");
_Static_assert(offsetof(ScePervasive2Registers, interruptControl) == 0xe08, "Pervasive2 interrupt control");
_Static_assert(offsetof(ScePervasiveDeviceRegisters, debugBus) == 0x154, "Pervasive debug bus");
_Static_assert(offsetof(ScePervasiveDeviceRegisters, spm) == 0x178, "Pervasive SPM");
_Static_assert(offsetof(ScePervasiveDeviceRegisters, vipRegisterAccess) == 0x180, "Pervasive VIP access");
_Static_assert(offsetof(ScePervasive2Registers, memoryCardInsert) == 0xf40, "Pervasive2 memory card");
_Static_assert(offsetof(ScePervasive2Registers, usbVbusInterruptEnable) == 0xf58, "Pervasive2 VBUS");
_Static_assert(offsetof(ScePervasive2Registers, mainMemoryPageBase) == 0xf60, "Pervasive2 page base");

_Static_assert(offsetof(ScePervasive2Registers, usbVbus) == 0xf50, "pervasive2 vbus");
_Static_assert(sizeof(ScePervasive2Registers) == 0x1000, "pervasive2 page");

#endif /* __ASSEMBLER__ */
