/* IFTU input-buffer registers shared by lowio and crash-screen display code. */
#pragma once
#include <stdint.h>
#include <stddef.h>

/*
 * One input buffer; the fields are loaded from an SceIftuPlaneState (vitasdk's
 * names, from lowio's sceIftuSetInputFrameBuffer).
 */
typedef struct SceIftuBufferRegisters {
	uint32_t address[3];			/* paddr0..2 */
	uint32_t reserved0[5];
	uint32_t planeMode;			/* SceIftuPlaneState word 0x20 */
	uint32_t srcX;				/* 0x24 */
	uint32_t srcY;
	uint32_t reserved1[5];
	uint32_t format;			/* 0x40 */
	uint32_t width;
	uint32_t height;
	uint32_t disabled;
	uint32_t reserved2;
	uint32_t linePadding;			/* 0x54: leftover_stride */
	uint32_t leftoverAlign;
	uint32_t reserved3;
	uint32_t vtopPadding;			/* 0x60 */
	uint32_t vbotPadding;
	uint32_t hleftPadding;
	uint32_t hrightPadding;
	uint32_t reserved4[20];
	uint32_t scaleX;			/* 0xc0: src_w */
	uint32_t scaleY;			/* src_h */
	uint32_t dstX;				/* 0xc8 */
	uint32_t dstY;
	uint32_t dstW;
	uint32_t dstH;
	uint32_t reserved5[10];
} SceIftuBufferRegisters;

typedef struct SceIftuRegisters {
	uint32_t control[0x80];			/* the unit's own (lowio) */
	SceIftuBufferRegisters buffer[2];
} SceIftuRegisters;

_Static_assert(sizeof(SceIftuBufferRegisters) == 0x100, "IFTU buffer stride");
_Static_assert(offsetof(SceIftuBufferRegisters, disabled) == 0x4c, "IFTU disable");
_Static_assert(offsetof(SceIftuBufferRegisters, linePadding) == 0x54, "IFTU padding");
_Static_assert(offsetof(SceIftuBufferRegisters, scaleX) == 0xc0, "IFTU X scale");
_Static_assert(offsetof(SceIftuRegisters, buffer) == 0x200, "IFTU buffers");
