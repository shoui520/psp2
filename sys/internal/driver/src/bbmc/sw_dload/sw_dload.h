/* The modem software download module (SceBbmcSwdlMod): its thread, heap and client. */
#pragma once
#include <scetypes.h>
#include <stddef.h>
#include <sysmem/cpu.h>
#include "../bbmc_message.h"

/* name unknown: the client the module reports its notification bits to */
typedef struct SwdlClient {
	int (*notify)(SceUInt32 mask, const BbmcNotificationArgs *args);
	void *argument;
} SwdlClient;

/* name unknown: the Intel HEX reader the image download parses its file with */
typedef struct SwdlHexReader {
	SceUID fd;
	SceSize consumed;		/* bytes read from the file */
	int hasUpperAddress;		/* record 4 seen */
	int hasStartAddress;		/* record 5 seen */
	int endOfFile;			/* record 1 seen */
	SceUInt16 upperAddress;
	SceUInt16 address;		/* of the first data record */
	SceUInt32 startAddress;
	SceUInt8 *line;			/* 522 bytes */
	SceSize length;			/* data bytes collected */
	SceUInt8 *data;
	SceSize capacity;
} SwdlHexReader;

_Static_assert(sizeof(SwdlHexReader) == 44, "HEX reader");

/* name unknown: the download module's state (one, at g_swdlContextStorage) */
typedef struct SwdlContext {
	SceKernelSpinlock lock;
	SceUInt32 reserved0;
	SceUID threadId;
	SceUID eventFlagId;
	SceUID heapId;
	SceUInt32 state;		/* the thread's handler */
	SceUInt32 mode;
	int lastError;
	SceUInt32 reserved20;
	void *port;
	SceUInt32 attached;
	SceUInt32 waiting;		/* 1 attach, 2 detach */
	void *clientArgument;
	BbmcNotificationArgs notification;
	int (*notify)(SceUInt32 mask, const BbmcNotificationArgs *args);
	SceUInt8 reserved44[0x68 - 0x44];
	SceUID fd;
	SceUInt8 reserved6c[0xbc - 0x6c];
	SceUInt8 *buffer;
	SceSize bufferLength;
	SceSize bufferCapacity;
	SceUInt8 reservedc8[0x548 - 0xc8];
	SwdlHexReader hexReader;
	SceUInt8 reserved574[0x59c - 0x574];
	SceUInt32 listCount;
	struct SwdlListNode *list;
	SceUInt32 reserved5a4;
} SwdlContext;

/* name unknown: a node of the context's list */
typedef struct SwdlListNode {
	SceUInt32 reserved0;
	void *data;
	SceUInt32 reserved8;
	void *extra;
	struct SwdlListNode *next;
} SwdlListNode;

_Static_assert(offsetof(SwdlContext, notify) == 0x40, "swdl notify");
_Static_assert(offsetof(SwdlContext, hexReader) == 0x548, "swdl HEX reader");
_Static_assert(sizeof(SwdlContext) == 0x5a8, "swdl context");

int swdlInitialize(SceUInt32 version, const SwdlClient *client);
int swdlTerminate(void);
/* name unknown: ask the modem to switch into its download mode (event 0x800000) */
int swdlRequestDownloadMode(void);
/* name unknown: wait for the download device to attach, 35 s when extended, else 5 s */
int swdlWaitAttach(int extended);
/* name unknown: wait 5 s for the download device to detach */
int swdlWaitDetach(void);
