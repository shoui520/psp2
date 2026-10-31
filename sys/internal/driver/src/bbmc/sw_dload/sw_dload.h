/* The modem software download module (SceBbmcSwdlMod): its thread, heap and client. */
#pragma once
#include <scetypes.h>
#include <stddef.h>
#include <sysmem/cpu.h>
#include <threadmgr/fast_mutex_object.h>
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

/* name unknown: the port the download runs over (SceBbmcSwdlPort), as its users see it */
typedef struct SwdlPortInterface {
	int (*open)(struct SwdlPortInterface *port, int mode);
	int (*close)(struct SwdlPortInterface *port);
	int (*send)(struct SwdlPortInterface *port, const void **data, SceSize *size,
		    int flags);		/* passed 0, not read */
	int (*control)(struct SwdlPortInterface *port, int command, void *data, SceSize size);
	int (*receive)(struct SwdlPortInterface *port, void *data, SceSize *size, int wait);
	int (*getFrameLength)(struct SwdlPortInterface *port, SceSize *length);
	int (*stopReceive)(struct SwdlPortInterface *port);
	struct SwdlPort *owner;
	int (*eventHandler)(struct SwdlPortInterface *port, SceUInt32 events, void *data,
			    SceSize size);
} SwdlPortInterface;

/* name unknown: an HDLC escaping (send) or unescaping (receive) in progress */
typedef struct SwdlPortEscape {
	SceUInt8 *dest;
	SceSize capacity;
	SceSize length;
	const SceUInt8 *source;
	SceSize sourceLength;
	SceSize sourcePos;
	int escaped;			/* 0x7d seen (receive) or written (send) */
} SwdlPortEscape;

/* name unknown: what each of the port's USB requests carries (request->context) */
typedef struct SwdlPortRequestContext {
	int result;
	int last;			/* the last packet of a send */
	struct BbmcUsbSerialRequest *next;	/* in the transmit queue */
	struct SwdlPort *port;
} SwdlPortRequestContext;

/* name unknown: the port (one, swdlPortStorage) */
typedef struct SwdlPort {
	int initialized;
	SwdlPortInterface interface;
	int channel;			/* the USB serial channel */
	SceUInt32 reserved2c;
	SceUID eventFlagId;
	SceUID heapId;
	SceUID threadId;
	SceUInt32 reserved3c;
	int connected;
	struct ThreadmgrFastMutex errorLock;
	int error;
	int rawMode;			/* 1: no HDLC framing */
	SwdlPortEscape transmit;
	SwdlPortEscape receive;
	struct ThreadmgrFastMutex rxLock;
	SceSize frameScan;		/* bytes of the frame being scanned */
	SceSize frameLength;		/* the complete frame's, 0 while there is none */
	SceUInt32 scanPos;
	SceSize rxCount;		/* bytes in ring */
	SceUInt32 readPos;
	SceUInt32 writePos;
	SceUInt8 ring[1024];
	struct BbmcUsbSerialRequest *rxRequest;
	SceUInt32 reserved520;
	int rxActive;
	struct ThreadmgrFastMutex txLock;
	SceUInt32 txCount;
	struct BbmcUsbSerialRequest *txHead;
	struct BbmcUsbSerialRequest *txTail;
	struct BbmcUsbSerialRequest *txCurrent;
	int rxPending;			/* a read is submitted */
	int txPending;			/* a write is submitted */
	int stopping;
	int terminating;
	SceUID abortTimerId;
	SceUInt32 abortEvent;
	int mode;			/* 1 QCOM, 2 QEDL, 3 QDL */
} SwdlPort;

_Static_assert(offsetof(SwdlPort, errorLock) == 0x44, "swdl port error lock");
_Static_assert(offsetof(SwdlPort, rxLock) == 0xc4, "swdl port rx lock");
_Static_assert(offsetof(SwdlPort, ring) == 0x11c, "swdl port ring");
_Static_assert(offsetof(SwdlPort, txLock) == 0x528, "swdl port tx lock");
_Static_assert(sizeof(SwdlPort) == 0x594, "swdl port");

struct SwdlContext;

/* name unknown: an NV item the backup saves, with the instances after it */
typedef struct SwdlNvItem {
	SceUInt16 id;
	SceUInt16 instances;
} SwdlNvItem;

/* name unknown: the backup file: swdlBackupHeader, the magic, the table header and
 * then one record per item */
typedef struct SwdlNvRecordHeader {
	SceUInt32 magic;		/* "NVTL" for the table, "NVIM" for an item */
	SceSize size;			/* the table's item count, or the item's bytes */
} SwdlNvRecordHeader;

/* name unknown: what each request hands the thread (copied into the context) */
typedef struct SwdlBackupRequest {
	SceUInt32 reserved0;
	SceUID fd;			/* the backup file written */
} SwdlBackupRequest;

typedef struct SwdlRestoreRequest {
	SceUInt32 reserved0;
	SceUID fd;			/* the backup file read */
	SceUInt32 mode;			/* 1: one item more than the file says */
	SceUInt32 reserved12;
} SwdlRestoreRequest;

typedef struct SwdlUpdateRequest {
	SceUInt32 reserved0[3];
	SceUID fd;			/* the Intel HEX image */
	SceUInt32 reserved16[9];
} SwdlUpdateRequest;

typedef struct SwdlDownloadRequest {
	SceUInt32 reserved0[8];
} SwdlDownloadRequest;

_Static_assert(sizeof(SwdlUpdateRequest) == 52, "swdl update request");

/* name unknown: one step of a download sequence: what it sends, and what it makes of
 * the reply (1 next step, 2 previous step) */
typedef struct SwdlStep {
	int (*send)(struct SwdlContext *context);
	int (*receive)(struct SwdlContext *context);
} SwdlStep;

/* name unknown: the download module's state (one, swdlContextStorage) */
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
	SwdlPortInterface *port;
	SceUInt32 attached;
	SceUInt32 waiting;		/* 1 attach, 2 detach */
	void *clientArgument;
	BbmcNotificationArgs notification;
	int (*notify)(SceUInt32 mask, const BbmcNotificationArgs *args);
	SwdlBackupRequest backup;
	SwdlRestoreRequest restore;
	SwdlUpdateRequest update;
	SwdlDownloadRequest download;
	SceUInt32 stepIndex;
	const SwdlStep *steps;
	SceUInt32 receiveEvent;		/* 512 frame, 1024 timeout, 2048 cancelled */
	SceUInt8 *buffer;		/* the frame to send */
	SceSize bufferLength;
	SceSize bufferCapacity;
	const void *transmitData;
	SceSize transmitLength;
	int crcRetries;
	int sendRetries;
	SceUInt32 transmitState;
	SceUInt32 savedTransmitState;
	SceUInt32 timeouts;
	SceUInt32 responseTimeout;	/* ms */
	SceUInt8 reservede8[0x10c - 0xe8];
	SceSize receiveLength;
	SceUInt8 receiveBuffer[1024];
	SceUInt8 reserved510[0x518 - 0x510];
	SceUInt16 nvItem;		/* the NV item in work (0xfffe: none yet) */
	SceUInt16 nvItemsWritten;
	SceSize backupSize;		/* bytes written to the backup file */
	SceUInt32 reserved520;
	SceUInt32 nvItemsDone;
	SceUInt32 reserved528;
	SceUInt32 nvItemTotal;		/* 0xffff: no file */
	SceUInt32 idCount;
	const SceUInt16 *ids;		/* the IDs the target reports */
	SceUInt16 nvTableIndex;		/* in swdlNvItems */
	SceUInt16 nvTableInstance;	/* of that entry's instances */
	SceUInt32 progress;		/* percent */
	SceUInt32 reportedProgress;
	SceUInt32 reserved544;
	SwdlHexReader hexReader;
	SceUInt8 reserved574[0x584 - 0x574];
	SceUInt32 timerFlags;		/* 0x400 frame timer, 0x40000/0x80000 attach/detach */
	SceUInt32 attachTime;
	SceUInt32 frameTime;		/* ms */
	SceUInt32 attachTimeout;
	SceUInt8 reserved594[0x59c - 0x594];
	SceUInt32 listCount;
	struct SwdlListNode *list;
	SceUInt32 reserved5a4;
} SwdlContext;

/* name unknown: a node of the context's list */
typedef struct SwdlListNode {
	SceSize dataSize;
	void *data;
	SceSize extraSize;
	void *extra;
	struct SwdlListNode *next;
} SwdlListNode;

_Static_assert(offsetof(SwdlContext, notify) == 0x40, "swdl notify");
_Static_assert(offsetof(SwdlContext, stepIndex) == 0xb0, "swdl steps");
_Static_assert(offsetof(SwdlContext, receiveBuffer) == 0x110, "swdl receive buffer");
_Static_assert(offsetof(SwdlContext, nvItem) == 0x518, "swdl NV item");
_Static_assert(offsetof(SwdlContext, hexReader) == 0x548, "swdl HEX reader");
_Static_assert(offsetof(SwdlContext, timerFlags) == 0x584, "swdl timer flags");
_Static_assert(sizeof(SwdlContext) == 0x5a8, "swdl context");

/* name unknown: the port the download runs over */
SwdlPortInterface *swdlCreatePort(void);
void swdlDestroyPort(SwdlPortInterface *interface);

int swdlInitialize(SceUInt32 version, const SwdlClient *client);
int swdlTerminate(void);
/* name unknown: ask the modem to switch into its download mode (event 0x800000) */
int swdlRequestDownloadMode(void);
/* name unknown: wait for the download device to attach, 35 s when extended, else 5 s */
int swdlWaitAttach(int extended);
/* name unknown: wait 5 s for the download device to detach */
int swdlWaitDetach(void);
