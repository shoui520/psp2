/*
 * display.h: the display's error codes, frame buffer record and constants, under Sony's names and
 * with Sony's definitions (SDK Release 03.570.011, target/include/display.h). The kernel takes
 * these; the user library's functions come with the user-mode SDK headers.
 */
#ifndef _SCE_DISPLAY_DISPLAY_H
#define _SCE_DISPLAY_DISPLAY_H

#include <scetypes.h>

#if !defined(_LANGUAGE_ASSEMBLY)

/* error codes */
#define SCE_DISPLAY_ERROR_INVALID_HEAD		-2144796672	/* 0x80290000 */
#define SCE_DISPLAY_ERROR_INVALID_VALUE		-2144796671	/* 0x80290001 */
#define SCE_DISPLAY_ERROR_INVALID_ADDR		-2144796670	/* 0x80290002 */
#define SCE_DISPLAY_ERROR_INVALID_PIXELFORMAT	-2144796669	/* 0x80290003 */
#define SCE_DISPLAY_ERROR_INVALID_PITCH		-2144796668	/* 0x80290004 */
#define SCE_DISPLAY_ERROR_INVALID_RESOLUTION	-2144796667	/* 0x80290005 */
#define SCE_DISPLAY_ERROR_INVALID_UPDATETIMING	-2144796666	/* 0x80290006 */
#define SCE_DISPLAY_ERROR_NO_FRAME_BUFFER	-2144796665	/* 0x80290007 */
#define SCE_DISPLAY_ERROR_NO_PIXEL_DATA		-2144796664	/* 0x80290008 */
#define SCE_DISPLAY_ERROR_NO_OUTPUT_SIGNAL	-2144796663	/* 0x80290009 */

/* the frame buffer's pixel format */
#define SCE_DISPLAY_PIXELFORMAT_A8B8G8R8	0x00000000U	/* A:B:G:R = 8:8:8:8, 32 bits */
#define SCE_DISPLAY_PIXEL_A8B8G8R8		SCE_DISPLAY_PIXELFORMAT_A8B8G8R8

/* a frame buffer (sceDisplaySetFrameBuf, sceDisplayGetFrameBuf) */
typedef struct SceDisplayFrameBuf {
	SceUInt32	size;		/* sizeof(SceDisplayFrameBuf) */
	void		*base;		/* the base address, 256-byte aligned */
	SceUInt32	pitch;		/* pixels */
	SceUInt32	pixelformat;	/* SCE_DISPLAY_PIXELFORMAT_* */
	SceUInt32	width;
	SceUInt32	height;
} SceDisplayFrameBuf;

/* when sceDisplaySetFrameBuf takes effect */
#define SCE_DISPLAY_UPDATETIMING_NEXTHSYNC	0
#define SCE_DISPLAY_UPDATETIMING_NEXTVSYNC	1

#endif /* !_LANGUAGE_ASSEMBLY */

#endif /* _SCE_DISPLAY_DISPLAY_H */
