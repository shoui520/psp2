/* MIPI DSI transmitters: SceDsi0Reg (physical 0xe5050000) drives the built-in
 * OLED or LCD panel, SceDsi1Reg (0xe5060000) the HDMI bridge. Sony names none
 * of the registers; names follow the henkaku wiki's DSI Registers page and
 * what lowio's SceDsiForDriver exports write to each word.
 *
 * The timing words take the video mode in transmitter clocks (clocks per
 * pixel: 6 on two lanes, 4 at 24 bits per pixel, 5 at 30). The transmitter
 * runs two programs of 32-bit words: the video program (fed to videoProgram
 * once per enable, repeated every frame) and command programs (fed to
 * commandProgram for one panel command), both built from SCE_DSI_PROGRAM_*.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

typedef struct SceDsiRegisters {
	volatile uint32_t control;		/* the start mode; 0 stops the output */
	volatile uint32_t videoMode;		/* bits 0-1: 3 interlaced; bit 8: phase adjust */
	volatile uint32_t hTotal;		/* (htotal * clocks) >> 1 */
	volatile uint32_t vTotal;		/* stepped by one line to pace HDMI frames */
	volatile uint32_t hStart;		/* 0x10001 */
	volatile uint32_t hActive;		/* active start << 16 | active end + 1 */
	volatile uint32_t hActiveAdjust;	/* 30-bit three-lane 480p and 720p only */
	volatile uint32_t vActive;		/* sync + back porch << 16 | vtotal + 1 - front porch */
	volatile uint32_t vActiveEven;		/* second field */
	volatile uint32_t hSync;		/* sync width << 16 | 1 */
	volatile uint32_t hSyncAdjust;		/* 30-bit three-lane 480p and 720p only */
	volatile uint32_t vStart;		/* 0x10001 */
	volatile uint32_t vSync;		/* sync width << 16 | hTotal */
	volatile uint32_t vSyncEven;		/* second field */
	volatile uint32_t vSyncEvenEnd;
	volatile uint32_t timingEnable;		/* 1 */
	volatile uint32_t lineInterrupt;	/* the lines the frame interrupt fires on */
	volatile uint32_t reserved0;
	volatile uint32_t pending;		/* bits 0-3: video program words still queued */
	volatile uint32_t position;		/* bits 16-28: current line + 1 */
	volatile uint32_t interruptStatus;	/* write back to acknowledge */
	volatile uint32_t interruptEnable;	/* one bit per sub-interrupt */
	volatile uint32_t reserved1;
	volatile uint32_t fieldStart;
	volatile uint32_t activeStart;
	volatile uint32_t fieldStartEven;
	volatile uint32_t activeStartEven;
	volatile uint32_t fieldControl;
	volatile uint32_t fieldControlEven;
	volatile uint32_t phaseAdjust[2];	/* 30-bit three-lane 480p and 720p only */
	volatile uint32_t reserved2[229];
	volatile uint32_t status;		/* SCE_DSI_STATUS_* */
	volatile uint32_t commandStatus;	/* SCE_DSI_COMMAND_* */
	volatile uint32_t reserved3[6];
	volatile uint32_t rxFifo;		/* read responses, four bytes a word */
	volatile uint32_t reserved4[51];
	volatile uint32_t videoProgram;
	volatile uint32_t programControl;	/* 8 before a command, 12 around blanking */
	volatile uint32_t videoProgramStart;	/* all ones: run the video program */
	volatile uint32_t commandStatusClear;
	volatile uint32_t commandProgram;
	volatile uint32_t programReset;		/* all ones: drop queued program words */
	volatile uint32_t videoProgramEnable;
	volatile uint32_t commandMode;		/* 1 while a command program runs */
	volatile uint32_t reserved5[185];
	volatile uint32_t phyReset;
	volatile uint32_t reserved6[2];
	volatile uint32_t phyTiming[9];		/* one DsiPhyTiming per link rate */
	volatile uint32_t laneEnable;		/* clock lane and data lanes, one bit each */
	volatile uint32_t continuousClock;	/* 1: the HS clock never drops to LP */
	volatile uint32_t clockLaneTiming;
	volatile uint32_t reserved7[48];
	volatile uint32_t laneConfig[4];	/* bits 0-1: 2 on SoC revision 1 and later */
	volatile uint32_t reserved8[12];
	volatile uint32_t lanePower[4];		/* 0x200 on, 1 off */
	volatile uint32_t reserved9[428];
} SceDsiRegisters;

_Static_assert(offsetof(SceDsiRegisters, vSync) == 0x30, "DSI vertical sync");
_Static_assert(offsetof(SceDsiRegisters, interruptStatus) == 0x50, "DSI interrupt status");
_Static_assert(offsetof(SceDsiRegisters, fieldStart) == 0x5c, "DSI field start");
_Static_assert(offsetof(SceDsiRegisters, phaseAdjust) == 0x74, "DSI phase adjust");
_Static_assert(offsetof(SceDsiRegisters, status) == 0x410, "DSI status");
_Static_assert(offsetof(SceDsiRegisters, rxFifo) == 0x430, "DSI read FIFO");
_Static_assert(offsetof(SceDsiRegisters, videoProgram) == 0x500, "DSI video program");
_Static_assert(offsetof(SceDsiRegisters, commandMode) == 0x51c, "DSI command mode");
_Static_assert(offsetof(SceDsiRegisters, phyReset) == 0x804, "DSI PHY reset");
_Static_assert(offsetof(SceDsiRegisters, phyTiming) == 0x810, "DSI PHY timing");
_Static_assert(offsetof(SceDsiRegisters, clockLaneTiming) == 0x83c, "DSI clock lane");
_Static_assert(offsetof(SceDsiRegisters, laneConfig) == 0x900, "DSI lane config");
_Static_assert(offsetof(SceDsiRegisters, lanePower) == 0x940, "DSI lane power");
_Static_assert(sizeof(SceDsiRegisters) == 0x1000, "DSI register page");

#define SCE_DSI_STATUS_RX_EMPTY		0x20		/* no read response word left */
#define SCE_DSI_STATUS_READY		0x100000	/* a command program may start */

#define SCE_DSI_COMMAND_BUSY		0x41007		/* commandStatus: a command runs */
#define SCE_DSI_COMMAND_DONE		0x1000
#define SCE_DSI_COMMAND_BLANKED		0x1000000	/* the blanking program has run */

/* Program words. A packet carries a DSI data type (bits 0-5) and virtual
 * channel (bits 6-7): 0x01 vsync start, 0x11 vsync end, 0x21 hsync start,
 * 0x31 hsync end, 0x19 blanking, 0x3e 24-bit pixels, 0x29 30-bit pixels.
 * WAIT holds the lanes until the timing generator reaches the events in its
 * mask; REPEAT runs the next `words` words `count` times (0xffff: forever).
 */
#define SCE_DSI_PROGRAM_REPEAT(words, count) \
	(0x04000000 | (words) << 16 | ((count) & 0xffff))
#define SCE_DSI_PROGRAM_END		0x0c000000
#define SCE_DSI_PROGRAM_SHORT(type)	(0x10000000 | (type))
#define SCE_DSI_PROGRAM_WAIT(events)	(0x28000000 | (events))
#define SCE_DSI_PROGRAM_LONG(type, count) \
	(0x40000000 | ((count) & 0xffff) << 8 | (type))

#define SCE_DSI_WAIT_HSYNC		0x1
#define SCE_DSI_WAIT_HSYNC_END		0x2
#define SCE_DSI_WAIT_VSYNC		0x4
#define SCE_DSI_WAIT_VSYNC_END		0x8
#define SCE_DSI_WAIT_ACTIVE_END		0x10
#define SCE_DSI_WAIT_SECOND_FIELD	0x800000
