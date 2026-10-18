/*
 * videodec.h: the video decoder service's error codes, types, records and functions, under Sony's
 * names and with Sony's definitions (SDK Release 03.570.011, target/include/videodec.h).
 */
#ifndef _SCE_VIDEODEC_VIDEODEC_H
#define _SCE_VIDEODEC_VIDEODEC_H

#include <scetypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SCE_VIDEODEC_ERROR_INVALID_TYPE		(-2141059071)	/* 0x80620801 */
#define SCE_VIDEODEC_ERROR_INVALID_PARAM	(-2141059070)	/* 0x80620802 */
#define SCE_VIDEODEC_ERROR_OUT_OF_MEMORY	(-2141059069)	/* 0x80620803 */
#define SCE_VIDEODEC_ERROR_INVALID_STATE	(-2141059068)	/* 0x80620804 */
#define SCE_VIDEODEC_ERROR_UNSUPPORT_IMAGE_SIZE	(-2141059067)	/* 0x80620805 */
#define SCE_VIDEODEC_ERROR_INVALID_COLOR_FORMAT	(-2141059066)	/* 0x80620806 */
#define SCE_VIDEODEC_ERROR_NOT_PHY_CONTINUOUS_MEMORY	(-2141059065)	/* 0x80620807 */
#define SCE_VIDEODEC_ERROR_ALREADY_USED		(-2141059064)	/* 0x80620808 */
#define SCE_VIDEODEC_ERROR_INVALID_POINTER	(-2141059063)	/* 0x80620809 */
#define SCE_VIDEODEC_ERROR_ES_BUFFER_FULL	(-2141059062)	/* 0x8062080A */
#define SCE_VIDEODEC_ERROR_INITIALIZE		(-2141059061)	/* 0x8062080B */
#define SCE_VIDEODEC_ERROR_NOT_INITIALIZE	(-2141059060)	/* 0x8062080C */
#define SCE_VIDEODEC_ERROR_INVALID_STREAM	(-2141059059)	/* 0x8062080D */
#define SCE_VIDEODEC_ERROR_INVALID_ARGUMENT_SIZE	(-2141059058)	/* 0x8062080E */

/* decoder init type */
#define SCE_VIDEODEC_TYPE_HW_AVCDEC		(0x1001U)

typedef struct SceVideodecQueryInitInfoHwAvcdec {
	SceSize size;
	SceUInt32 horizontal;
	SceUInt32 vertical;
	SceUInt32 numOfRefFrames;
	SceUInt32 numOfStreams;
} SceVideodecQueryInitInfoHwAvcdec;

typedef union SceVideodecQueryInitInfo {
	SceUInt8 reserved[32];
	SceVideodecQueryInitInfoHwAvcdec hwAvc;
} SceVideodecQueryInitInfo;

typedef struct SceVideodecMemInfo {
	SceUInt32 memSize;
} SceVideodecMemInfo;

typedef struct SceVideodecBuf {
	void *pBuf;
	SceUInt32 size;
} SceVideodecBuf;

typedef struct SceVideodecCtrl {
	SceVideodecBuf memBuf;
	SceUID memBufUid;
	SceUIntVAddr vaContext;
	SceUInt32 contextSize;
} SceVideodecCtrl;

extern SceInt32 sceVideodecInitLibrary(SceUInt32 codecType,
				       const SceVideodecQueryInitInfo *pInitInfo);
extern SceInt32 sceVideodecQueryMemSize(SceUInt32 codecType,
					const SceVideodecQueryInitInfo *pInitInfo,
					SceVideodecMemInfo *pMemInfo);
extern SceInt32 sceVideodecInitLibraryWithUnmapMem(SceUInt32 codecType, SceVideodecCtrl *pCtrl,
						   const SceVideodecQueryInitInfo *pInitInfo);
extern SceInt32 sceVideodecTermLibrary(SceUInt32 codecType);

#define SCE_VIDEODEC_VOID_TIMESTAMP		(0xffffffff)
#define SCE_VIDEODEC_PARAM_UNKNOWN		(0xffffffff)

typedef struct SceVideodecTimeStamp {
	SceUInt32 upper;
	SceUInt32 lower;
} SceVideodecTimeStamp;

#ifdef __cplusplus
}
#endif

#endif /* _SCE_VIDEODEC_VIDEODEC_H */
