/* IFTU registers shared by lowio and crash-screen display code. Names after the
 * henkaku wiki's IFTU Registers page and lowio/iftu.h's SceIftuPlaneState,
 * which lowio's sceIftuSetInputFrameBuffer copies into a bank.
 *
 *   SceIftu0RegA/B 0xe5020000/0xe5021000  OLED/LCD planes A and B
 *   SceIftuc0Reg   0xe5022000             OLED/LCD control
 *   SceIftu1RegA/B 0xe5030000/0xe5031000  HDMI planes A and B
 *   SceIftuc1Reg   0xe5032000             HDMI control
 *   SceIftu2Reg    0xe5040000             off-screen CSC and scaler
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

/* IFTU <n> (0, 1) occupies three pages: plane A, plane B, then its IFTUC */
#define IFTU_REG_BASE(n)		(0xe5020000 + (n) * 0x10000)
#define IFTU_PAGE_BASE(n, page)	(IFTU_REG_BASE(n) + (page) * 0x1000)
#define IFTUC_REG_BASE(n)		(IFTU_REG_BASE(n) + 0x2000)
#define IFTU2_REG_BASE			0xe5040000

/* Source and destination of one configuration bank. */
typedef struct SceIftuBufferRegisters {
	uint32_t address[3];			/* source planes 0..2 */
	uint32_t reserved0[5];
	uint32_t sourceConfig[3];		/* 0x20: copied, not interpreted */
	uint32_t reserved1[5];
	uint32_t format;			/* 0x40: source pixel format */
	uint32_t width;
	uint32_t height;
	uint32_t disabled;
	uint32_t reserved2;
	uint32_t linePadding;			/* 0x54: row padding, plane 0 */
	uint32_t leftoverAlign;			/* row padding, planes 1 and 2 */
	uint32_t reserved3;
	uint32_t vtopPadding;			/* 0x60: rows/pixels cropped */
	uint32_t vbotPadding;
	uint32_t hleftPadding;
	uint32_t hrightPadding;
	uint32_t reserved4[4];
	uint32_t dstAddress[3];			/* 0x80: destination planes */
	uint32_t reserved5[5];
	uint32_t dstFormat;			/* 0xa0 */
	uint32_t dstWidth;
	uint32_t dstHeight;
	uint32_t reserved6[2];
	uint32_t dstLinePadding;		/* 0xb4 */
	uint32_t dstLeftoverAlign;
	uint32_t reserved7;
	uint32_t scaleX;			/* 0xc0: source / destination, 16.16 */
	uint32_t scaleY;
	uint32_t dstX;				/* 0xc8: placement */
	uint32_t dstY;
	uint32_t sampleX;			/* 0xd0: source sampling offset */
	uint32_t sampleY;
	uint32_t reserved8[10];
} SceIftuBufferRegisters;

/* Colour space conversion: two post-multiply adds and a 3x3 S3.9 matrix. */
typedef struct SceIftuCscRegisters {
	uint32_t add[2];			/* plane 0, planes 1 and 2 */
	uint32_t matrix[9];			/* rr rg rb gr gg gb br bg bb */
} SceIftuCscRegisters;

typedef struct SceIftuClampRegisters {
	uint32_t max;
	uint32_t min;
} SceIftuClampRegisters;

typedef struct SceIftuRegisters {
	uint32_t control;			/* bit 0 starts the engine */
	uint32_t crtcMask;			/* 1: OLED/LCD, 2: HDMI */
	uint32_t reserved0[14];
	uint32_t interruptStatus;		/* 0x40: write back to clear */
	uint32_t reserved1[3];
	uint32_t interruptEnable;		/* 0x50: one bit per plane */
	uint32_t reserved2;
	uint32_t pathConfig;			/* 0x58: 0x108 before starting */
	uint32_t reserved3[9];
	uint32_t backgroundColor[3];		/* 0x80: 10-bit components */
	uint32_t alpha;
	uint32_t fieldMode;			/* 0x90: IFTU2 only */
	uint32_t reserved4[3];
	uint32_t alphaControl;			/* 0xa0: bit 0 disables blending */
	uint32_t reserved5[23];
	uint32_t cscControl;			/* 0x100: bit 0 enables CSC */
	SceIftuCscRegisters cscParams2;		/* 0x104 */
	SceIftuCscRegisters cscParams1;		/* 0x130 */
	SceIftuClampRegisters clamp[2];		/* 0x15c: plane 0, planes 1 and 2 */
	uint32_t reserved6[5];
	uint32_t filter;			/* 0x180: 0 nearest, 1 bilinear */
	uint32_t reserved7[31];
	SceIftuBufferRegisters buffer[2];	/* 0x200: configuration banks */
} SceIftuRegisters;

/* SceIftuc0Reg, SceIftuc1Reg: a display path's two planes. */
typedef struct SceIftucPlaneRegisters {
	uint32_t bankSelect;			/* bit 0: buffer[1] instead of [0] */
	uint32_t companion;
} SceIftucPlaneRegisters;

typedef struct SceIftucRegisters {
	uint32_t engineControl;			/* bit 0 enables the engine */
	uint32_t planeControl;			/* 1: alpha, 4: plane A, 0x10: B */
	uint32_t reserved[2];
	SceIftucPlaneRegisters plane[2];	/* 0x10: planes A and B */
	uint32_t blendControl;			/* 0x20: 1 subtractive, 2 additive */
} SceIftucRegisters;

_Static_assert(sizeof(SceIftuBufferRegisters) == 0x100, "IFTU buffer stride");
_Static_assert(offsetof(SceIftuBufferRegisters, disabled) == 0x4c, "IFTU disable");
_Static_assert(offsetof(SceIftuBufferRegisters, linePadding) == 0x54, "IFTU padding");
_Static_assert(offsetof(SceIftuBufferRegisters, dstAddress) == 0x80, "IFTU destination");
_Static_assert(offsetof(SceIftuBufferRegisters, dstFormat) == 0xa0, "IFTU destination format");
_Static_assert(offsetof(SceIftuBufferRegisters, dstLinePadding) == 0xb4, "IFTU destination padding");
_Static_assert(offsetof(SceIftuBufferRegisters, scaleX) == 0xc0, "IFTU X scale");
_Static_assert(offsetof(SceIftuBufferRegisters, sampleY) == 0xd4, "IFTU Y offset");
_Static_assert(offsetof(SceIftuRegisters, interruptStatus) == 0x40, "IFTU interrupts");
_Static_assert(offsetof(SceIftuRegisters, interruptEnable) == 0x50, "IFTU interrupt enable");
_Static_assert(offsetof(SceIftuRegisters, pathConfig) == 0x58, "IFTU path");
_Static_assert(offsetof(SceIftuRegisters, backgroundColor) == 0x80, "IFTU background");
_Static_assert(offsetof(SceIftuRegisters, alphaControl) == 0xa0, "IFTU alpha control");
_Static_assert(offsetof(SceIftuRegisters, cscControl) == 0x100, "IFTU CSC");
_Static_assert(offsetof(SceIftuRegisters, cscParams1) == 0x130, "IFTU CSC 1");
_Static_assert(offsetof(SceIftuRegisters, clamp) == 0x15c, "IFTU clamp");
_Static_assert(offsetof(SceIftuRegisters, filter) == 0x180, "IFTU filter");
_Static_assert(offsetof(SceIftuRegisters, buffer) == 0x200, "IFTU buffers");
_Static_assert(offsetof(SceIftucRegisters, plane) == 0x10, "IFTUC planes");
_Static_assert(offsetof(SceIftucRegisters, blendControl) == 0x20, "IFTUC blend");
