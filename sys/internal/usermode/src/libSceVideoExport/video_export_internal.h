/*
 * SceVideoExport's session with the media service (SceAvMediaSrv's IPMI service), the types of its
 * calls and the state of the export it carries between the three SceVideoExportData calls. No SDK
 * header and no debug record names them; the names are ours.
 */
#ifndef LIBSCEVIDEOEXPORT_VIDEO_EXPORT_INTERNAL_H
#define LIBSCEVIDEOEXPORT_VIDEO_EXPORT_INTERNAL_H

#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <libSceVideoExport/video_export.h>
#include "../../../../../vsh/lowlayer/src/ipmi/client.h"

/* the codes the library returns (0x80105400 and up; the SDK has no header for them) */
#define VIDEO_EXPORT_ERROR_PARAM		-2146413567	/* 0x80105401 */
#define VIDEO_EXPORT_ERROR_NO_MEMORY		-2146413564	/* 0x80105404 */
#define VIDEO_EXPORT_ERROR_SERVER_DOWN		-2146413563	/* 0x80105405 */
#define VIDEO_EXPORT_ERROR_MEDIA_FULL		-2146413561	/* 0x80105407 */
#define VIDEO_EXPORT_ERROR_CREATE_FILE		-2146413560	/* 0x80105408 */
#define VIDEO_EXPORT_ERROR_CANCELED		-2146413558	/* 0x8010540A */
#define VIDEO_EXPORT_ERROR_INTERNAL		-2146413554	/* 0x8010540E */
#define VIDEO_EXPORT_ERROR_NOT_STARTED		-2146413553	/* 0x8010540F */
#define VIDEO_EXPORT_ERROR_WRITE_OPEN		-2146413552	/* 0x80105410 */
#define VIDEO_EXPORT_ERROR_NO_SPACE		-2146413551	/* 0x80105411 */
#define VIDEO_EXPORT_ERROR_NOT_SUPPORTED_FORMAT	-2146413550	/* 0x80105412 */
#define VIDEO_EXPORT_ERROR_INVALID_PATH		-2146413549	/* 0x80105413 */

/* the memory the library takes for itself: the IPMI client and the results */
#define VIDEO_EXPORT_HEAP_SIZE			0x10000
#define VIDEO_EXPORT_MAX_ITEMS			10

/* the media service's methods */
#define VIDEO_EXPORT_METHOD_START		0x41560007
#define VIDEO_EXPORT_METHOD_GET_RESULT		0x41560008
#define VIDEO_EXPORT_METHOD_CANCEL		0x41560009
#define VIDEO_EXPORT_METHOD_GET_PROGRESS	0x4156000A

/* SceVideoExportEmpr_8493BF31's input and output */
typedef struct VideoExportEmprInput {
	char path[VIDEO_EXPORT_MAX_FS_PATH];
	SceUInt8 reserved0[64];
	SceInt32 mode;
	VideoExportCancelFunc cancelFunc;
	VideoExportProgressFunc progressFunc;
	void *userdata;
	void *workMemory;
	SceSize workMemorySize;
	SceUInt8 reserved1[64];
} VideoExportEmprInput;

typedef struct VideoExportEmprOutput {
	char path[VIDEO_EXPORT_MAX_FS_PATH];
	SceInt32 reserved0;
	SceInt32 reserved1;
	SceUInt8 reserved2[64];
} VideoExportEmprOutput;

/* one connection to the media service */
typedef struct VideoExportSession {
	IPMI::Client *pClient;
	SceInt32 requestId;		/* -1: no request yet */
	void *mspace;
	SceUID memblockId;		/* the memory block of the heap, 0 where the caller gave the memory */
	void *pClientBuffer;
	void *pWorkMemory;		/* the caller's heap */
	SceInt32 progressMode;		/* 0: percent and count to the callback, 1: the count only */
} VideoExportSession;

/* video_export_ipc_client.cpp */
int videoExportIpcStart(IPMI::Client *client, int *pRequestId,
			const IPMI::EventFlagInfo *pEventFlag, SceUInt32 count,
			const VideoExportInputParam *pItems, SceUInt32 mode);
int videoExportIpcGetResult(IPMI::Client *client, int requestId, SceUInt32 *pCount,
			    VideoExportOutputParam *pResults);
int videoExportIpcCancel(IPMI::Client *client);
int videoExportIpcGetProgress(IPMI::Client *client, SceInt32 *pProgress);

#endif
