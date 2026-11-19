/*
 * SceMusicExport's session with the media service (SceAvMediaSrv's IPMI service) and what the
 * library sends it. No debug record names the types; the names are ours.
 */
#ifndef LIBSCEMUSICEXPORT_MUSIC_EXPORT_INTERNAL_H
#define LIBSCEMUSICEXPORT_MUSIC_EXPORT_INTERNAL_H

#include <scetypes.h>
#include <music_export.h>
#include <libkernel/libkernel.h>
#include "../../../../../vsh/lowlayer/src/ipmi/client.h"

/* the memory the library takes for itself: the IPMI client and the results */
#define MUSIC_EXPORT_HEAP_SIZE			0x10000
#define MUSIC_EXPORT_MAX_ITEMS			10

/* the media service's methods */
#define MUSIC_EXPORT_METHOD_START		0x4156000B
#define MUSIC_EXPORT_METHOD_GET_RESULT		0x4156000C
#define MUSIC_EXPORT_METHOD_CANCEL		0x4156000D
#define MUSIC_EXPORT_METHOD_GET_PROGRESS	0x4156000E
#define MUSIC_EXPORT_METHOD_SEND_DATA		0x41560013

/* an item to export (SceMusicExport_D1011570's input; sceMusicExportFromFile sends one) */
typedef struct MusicExportItem {
	char path[SCE_MUSIC_EXPORT_MAX_FS_PATH];
	SceInt32 mode;			/* 1: the data goes in pieces (SEND_DATA), else it is read from the path */
	SceUInt8 reserved[64];
} MusicExportItem;

/* what the service answers for an item */
typedef struct MusicExportResult {
	char path[SCE_MUSIC_EXPORT_MAX_FS_PATH];	/* where the content is */
	SceInt32 reserved0;
	SceInt32 reserved1;
} MusicExportResult;

/* the data the media service takes with SEND_DATA, ahead of the data, the extension and the path */
typedef struct MusicExportDataHeader {
	SceUInt32 size;			/* the data's size */
	SceUInt32 index;		/* the number of the piece */
	SceUInt8 last;
	SceOff total;			/* the file's size */
	SceUInt32 reserved;
} MusicExportDataHeader;

/* one connection to the media service */
typedef struct MusicExportSession {
	IPMI::Client *pClient;
	SceInt32 requestId;		/* -1: no request yet */
	void *mspace;
	SceUID memblockId;		/* the memory block of the heap, 0 where the caller gave the memory */
	void *pClientBuffer;
	void *pWorkMemory;		/* the caller's heap */
	const char *pPath;		/* the file being exported */
	SceUInt8 reserved[0x44 - 0x1c];
} MusicExportSession;

/* music_export_ipc_client.cpp */
int musicExportIpcStart(IPMI::Client *client, int *pRequestId,
			const IPMI::EventFlagInfo *pEventFlag, SceUInt32 count,
			const MusicExportItem *pItems, SceUInt32 mode);
int musicExportIpcGetResult(IPMI::Client *client, int requestId, SceUInt32 *pCount,
			    MusicExportResult *pResults);
int musicExportIpcCancel(IPMI::Client *client);
int musicExportIpcGetProgress(IPMI::Client *client, SceInt32 *pProgress);
int musicExportIpcSendData(IPMI::Client *client, const void *pData, SceUInt32 size,
			   SceUInt32 index, SceUInt8 last, SceOff total, const char *pExtension,
			   SceUInt32 reserved, const char *pPath);

#endif
