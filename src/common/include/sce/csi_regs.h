/* Camera input registers: the MIPI CSI-2 receivers (SceCsi0Reg at 0xe3050000,
 * SceCsi1Reg at 0xe3060000) and the camera interfaces that write the frames
 * to memory (SceCif0Reg at 0xe3020000, SceCif1Reg at 0xe3030000).
 * Sony names none of them; the names follow what lowio's SceCsiForDriver
 * exports and SceCamera (the only caller) do with each word.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

#define CSI_REG_BASE(n)			(0xe3050000 + (n) * 0x10000)
#define CIF_REG_BASE(n)			(0xe3020000 + (n) * 0x10000)
#define CSI_REG_SIZE			0x1000
#define CIF_REG_SIZE			0x1000

typedef struct SceCsiRegisters {
	volatile uint32_t enable;
	volatile uint32_t reserved0;
	volatile uint32_t receive;		/* 1 after enable; 0 before it on stop */
	volatile uint32_t reserved1[2];
	volatile uint32_t status;		/* read back after starting */
	volatile uint32_t reserved2[4];
	volatile uint32_t mode;			/* 0x1000 */
	volatile uint32_t reserved3[24];
	volatile uint32_t interruptEnable;	/* 0: lowio never handles CSI interrupts */
	volatile uint32_t reserved4[28];
	volatile uint32_t format;		/* the camera's CSI format word */
} SceCsiRegisters;

_Static_assert(offsetof(SceCsiRegisters, status) == 0x14, "CSI status");
_Static_assert(offsetof(SceCsiRegisters, mode) == 0x28, "CSI mode");
_Static_assert(offsetof(SceCsiRegisters, interruptEnable) == 0x8c, "CSI interrupt enable");
_Static_assert(offsetof(SceCsiRegisters, format) == 0x100, "CSI format");

#define SCE_CIF_STATUS_BUSY		0x1		/* status: a frame is being written */
#define SCE_CIF_INTR_FRAME		0x1000000	/* interruptStatus: a frame is complete */

/* One of the three frame buffers the interface writes in turn. */
typedef struct SceCifFrameRegisters {
	volatile uint32_t iAddress;		/* luma (I) plane */
	volatile uint32_t chromaAddress[2];	/* U and V planes */
} SceCifFrameRegisters;

typedef struct SceCifRegisters {
	volatile uint32_t stop;
	volatile uint32_t start;
	volatile uint32_t status;
	volatile uint32_t inputFormat;		/* mode bits 4-7 | 0x301100 */
	volatile uint32_t reserved0;
	volatile uint32_t dataOrder;		/* 0x100 or 0x200 from mode bits 12-13 */
	volatile uint32_t outputEnable;		/* 1 just before start */
	volatile uint32_t reserved1[2];
	volatile uint32_t outputFormat;		/* mode bits 0-3 | 0xff0000 */
	volatile uint32_t lastFrame;		/* 2: three frame buffers */
	volatile uint32_t reserved2;
	volatile uint32_t windowControl;
	volatile uint32_t width;
	volatile uint32_t left;
	volatile uint32_t height;
	volatile uint32_t top;
	volatile uint32_t reserved3[2];
	volatile uint32_t iAddress;		/* frame 0, laid out apart from frames 1 and 2 */
	volatile uint32_t iPitch;
	volatile uint32_t chromaAddress[2];
	volatile uint32_t chromaPitch;
	volatile uint32_t reserved4[6];
	SceCifFrameRegisters frame[2];		/* frames 1 and 2 */
	volatile uint32_t currentFrame;
	volatile uint32_t reserved5[8];
	volatile uint32_t frameInfo;		/* passed to the sub-interrupt handlers */
	volatile uint32_t reserved6;
	volatile uint32_t interruptStatus;	/* write back to acknowledge */
	volatile uint32_t interruptEnable;
} SceCifRegisters;

_Static_assert(offsetof(SceCifRegisters, outputFormat) == 0x24, "CIF output format");
_Static_assert(offsetof(SceCifRegisters, windowControl) == 0x30, "CIF window");
_Static_assert(offsetof(SceCifRegisters, iAddress) == 0x4c, "CIF frame 0");
_Static_assert(offsetof(SceCifRegisters, chromaPitch) == 0x5c, "CIF chroma pitch");
_Static_assert(offsetof(SceCifRegisters, frame) == 0x78, "CIF frames 1 and 2");
_Static_assert(offsetof(SceCifRegisters, currentFrame) == 0x90, "CIF current frame");
_Static_assert(offsetof(SceCifRegisters, frameInfo) == 0xb4, "CIF frame info");
_Static_assert(offsetof(SceCifRegisters, interruptStatus) == 0xbc, "CIF interrupt status");
_Static_assert(offsetof(SceCifRegisters, interruptEnable) == 0xc0, "CIF interrupt enable");
