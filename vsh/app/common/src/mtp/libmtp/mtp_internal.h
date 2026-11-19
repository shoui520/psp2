/*
 * SceLibMtp's internal interface: the MTP responder core, its device, driver and
 * command layers. Sony's function names come from the module's own messages (each
 * names the function it reports from); the file split follows the components the
 * messages name ("CORE:", "DEVICE:", "DRIV:", ...). File, type and field names are ours.
 */
#ifndef _LIBMTP_MTP_INTERNAL_H
#define _LIBMTP_MTP_INTERNAL_H

#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <usbmtp/usbmtp.h>		/* SceMtpIf's errors, MtpIfTransfer */
#include <rtc.h>
#include <fsutil/fs_util.h>		/* sceFsUtilGetDevInfo */
#include <console_info/console_info.h>
#include <host_collabo/util/wake_on_lan.h>
#include "libmtp.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Every message carries its component, the reporting function and Sony's source line
 * (the line is the value Sony's build passes, kept as data).
 */
#define MTP_ERR(tag, line, fmt, ...) \
	sceClibPrintf("[MTP %s%s(%d)err] " fmt, tag, __FUNCTION__, line, ##__VA_ARGS__)
#define MTP_WARN(tag, line, fmt, ...) \
	sceClibPrintf("[MTP %s%s(%d)warning] " fmt, tag, __FUNCTION__, line, ##__VA_ARGS__)

/* the tags of the messages of the files several files share */
#define MTP_TAG_CMD	"CMD :"
#define MTP_TAG_CTXT	"CTXT:"

/* SceLibMtp's errors (the names are ours) */
#define SCE_MTP_ERROR_NOT_INITIALIZED	((int)0x80540001)
#define SCE_MTP_ERROR_ALREADY		((int)0x80540002)
#define SCE_MTP_ERROR_INVALID_ARGUMENT	((int)0x80540003)
#define SCE_MTP_ERROR_NOT_STOPPED	((int)0x80540004)
/* a logical storage holds its most objects already */
#define SCE_MTP_ERROR_STORAGE_FULL	((int)0x80540005)
/* no registered storage has the mount point a path names */
#define SCE_MTP_ERROR_STORAGE_NOT_FOUND	((int)0x80540006)
#define SCE_MTP_ERROR_NO_MEMORY		((int)0x80540007)
#define SCE_MTP_ERROR_DEVICE		((int)0x80540008)
#define SCE_MTP_ERROR_INVALID_PARENT	((int)0x80540009)
/* an object property the device does not let the host set */
#define SCE_MTP_ERROR_READ_ONLY_PROPERTY	((int)0x8054000A)
/* mtpObjAddRegularObject: a file of that name is already there */
#define SCE_MTP_ERROR_OBJECT_EXISTS	((int)0x80540035)
#define SCE_MTP_ERROR_NOT_FOUND		((int)0x80540011)
/* the host's data phase ended before a value's bytes */
#define SCE_MTP_ERROR_RECEIVE		((int)0x80540012)
#define SCE_MTP_ERROR_DATA_PHASE_OVER	((int)0x80540013)
#define SCE_MTP_ERROR_TERMINATED	((int)0x80540014)
#define SCE_MTP_ERROR_HOST_TIMEOUT	((int)0x8054001C)
#define SCE_MTP_ERROR_PC_TIMEOUT	((int)0x80540028)
#define SCE_MTP_ERROR_PS_TIMEOUT	((int)0x80540029)
#define SCE_MTP_ERROR_NOT_PERMITTED	((int)0x80540023)
#define SCE_MTP_ERROR_CANCELED		((int)0x8054001A)
#define SCE_MTP_ERROR_NO_HOST		((int)0x8054001B)
#define SCE_MTP_ERROR_NOT_SUPPORTED	((int)0x805400FF)
#define SCE_MTP_ERROR_GENERAL		((int)0x80540015)
#define SCE_MTP_ERROR_UNSUPPORTED_FORMAT	((int)0x8054000B)
#define SCE_MTP_ERROR_UNSUPPORTED_PROPERTY	((int)0x8054000C)
/* the cache and the database have no such object */
#define SCE_MTP_ERROR_NO_SUCH_OBJECT	((int)0x8054000E)
/* a database file SQLite cannot read (mtpSqlConvertError) */
#define SCE_MTP_ERROR_DATABASE_BROKEN	((int)0x80540032)
/* any other SQLite error, its primary result code in the low byte */
#define SCE_MTP_ERROR_SQLITE(err)	((int)(0x80541000 | (SceUInt8)(err)))

/* the network device's errors (the names are ours) */
#define SCE_MTP_ERROR_NET_UNKNOWN_HOST_TYPE	((int)0x8054001E)
#define SCE_MTP_ERROR_NET			((int)0x805400D0)
#define SCE_MTP_ERROR_NET_DISCONNECTED		((int)0x805400D1)
#define SCE_MTP_ERROR_NET_NOT_ASSIGNED		((int)0x805400D2)
#define SCE_MTP_ERROR_NET_BAD_TIMING		((int)0x805400D3)
#define SCE_MTP_ERROR_NET_HOST_UNAVAILABLE	((int)0x805400D4)
#define SCE_MTP_ERROR_NET_INVALID_RESPONSE	((int)0x805400D5)
#define SCE_MTP_ERROR_NET_MAX_HOSTS		((int)0x805400D7)
#define SCE_MTP_ERROR_NET_REGISTER_COUNT_MAX	((int)0x805400D8)
#define SCE_MTP_ERROR_NET_HOST_CANCELED		((int)0x805400D9)
#define SCE_MTP_ERROR_NET_INVALID_PIN		((int)0x805400DA)
#define SCE_MTP_ERROR_NET_HOST_TIMEOUT		((int)0x805400DB)
#define SCE_MTP_ERROR_NET_REJECTED		((int)0x805400DC)
#define SCE_MTP_ERROR_NET_UPDATE_DEVICE		((int)0x805400E0)
#define SCE_MTP_ERROR_NET_UPDATE_PC_HOST	((int)0x805400E1)
#define SCE_MTP_ERROR_NET_UPDATE_PS3		((int)0x805400E2)
#define SCE_MTP_ERROR_NET_UPDATE_NASNE		((int)0x805400E3)
#define SCE_MTP_ERROR_NET_INVALID_METHOD	((int)0x805400E4)

/*
 * The errors the host's responses map to (mtpUtilResponseCodeToError). The standard responses
 * are named after the response; Sony's vendor responses (0xA0xx) after the first response that
 * gives the error.
 */
#define SCE_MTP_ERROR_ACCESS_DENIED		((int)0x80540021)
#define SCE_MTP_ERROR_OBJECT_WRITE_PROTECTED	((int)0x8054002C)
#define SCE_MTP_ERROR_STORE_NOT_AVAILABLE	((int)0x8054002E)
#define SCE_MTP_ERROR_STORE_FULL		((int)0x8054002F)
#define SCE_MTP_ERROR_NO_THUMBNAIL_PRESENT	((int)0x8054003F)
#define SCE_MTP_ERROR_INCOMPLETE_TRANSFER	((int)0x80540040)
#define SCE_MTP_ERROR_RESPONSE_A00A		((int)0x80540011)
#define SCE_MTP_ERROR_RESPONSE_A00D		((int)0x80540016)
#define SCE_MTP_ERROR_RESPONSE_A008		((int)0x80540017)
#define SCE_MTP_ERROR_RESPONSE_A010		((int)0x80540018)
#define SCE_MTP_ERROR_RESPONSE_A017		((int)0x80540019)
#define SCE_MTP_ERROR_RESPONSE_A01C		((int)0x80540022)
#define SCE_MTP_ERROR_RESPONSE_A021		((int)0x80540023)
#define SCE_MTP_ERROR_RESPONSE_A022		((int)0x80540024)
#define SCE_MTP_ERROR_RESPONSE_A023		((int)0x80540025)
#define SCE_MTP_ERROR_RESPONSE_A027		((int)0x8054002D)
#define SCE_MTP_ERROR_RESPONSE_A028		((int)0x80540030)
#define SCE_MTP_ERROR_RESPONSE_A018		((int)0x80540031)
#define SCE_MTP_ERROR_RESPONSE_A029		((int)0x80540033)
#define SCE_MTP_ERROR_RESPONSE_A001		((int)0x80540036)
#define SCE_MTP_ERROR_RESPONSE_A002		((int)0x80540037)
#define SCE_MTP_ERROR_RESPONSE_A007		((int)0x80540038)
#define SCE_MTP_ERROR_RESPONSE_A012		((int)0x80540039)
#define SCE_MTP_ERROR_RESPONSE_A013		((int)0x8054003A)
#define SCE_MTP_ERROR_RESPONSE_A014		((int)0x8054003B)
#define SCE_MTP_ERROR_RESPONSE_A01A		((int)0x8054003C)
#define SCE_MTP_ERROR_RESPONSE_A01B		((int)0x8054003D)
#define SCE_MTP_ERROR_RESPONSE_A01F		((int)0x8054003E)
#define SCE_MTP_ERROR_RESPONSE_A020		((int)0x80540041)
#define SCE_MTP_ERROR_RESPONSE_A02E		((int)0x80540045)
#define SCE_MTP_ERROR_RESPONSE_A02C		((int)0x80540046)
#define SCE_MTP_ERROR_RESPONSE_A02A		((int)0x80540081)

/* the interface a device runs on */
#define MTP_IF_TYPE_USB			1
#define MTP_IF_TYPE_NET			2

/* sceRegMgrSrvGetRegion's region where a PS host may not connect */
#define MTP_REGION_CHINA		13

/* the core's state */
#define MTP_CORE_STATE_NONE		0
#define MTP_CORE_STATE_INITIALIZED	1
#define MTP_CORE_STATE_STARTED		2

/* the device thread's event flag bits */
#define MTP_CORE_EVF_RUN		0x1
#define MTP_CORE_EVF_CONNECTED		0x2
#define MTP_CORE_EVF_STOP		0x4

/*
 * The large buffers the caller lends the transfers (1 MiB each, against the utilities' own
 * 64 KiB ones): start() tells the caller to make them available and passes the function it
 * calls with the buffers' new state.
 */
typedef struct MtpLargeBufferFuncs {
	int (*start)(void (*stateCallback)(int state));
	int (*end)(void);
	void *reserved;			/* libmtp does not call it */
	void *(*memalign)(SceSize align, SceSize size);
	void (*free)(void *ptr);
} MtpLargeBufferFuncs;

/* sceMtpInit's parameter */
typedef struct SceMtpInitParam {
	int devThreadPriority;
	int devThreadCpuAffinityMask;
	int xferThreadPriority;
	int xferThreadCpuAffinityMask;
	int reserved;
	const MtpLargeBufferFuncs *largeBufferFuncs;
} SceMtpInitParam;

/* the codes several files use */
#define MTP_RESPONSE_TRANSACTION_CANCELLED	0x201F
#define MTP_EVENT_CANCEL_TRANSACTION	0x4001
#define MTP_OPERATION_CANCEL		0xC108
#define MTP_OPERATION_TERMINATE		0xC126
#define MTP_OPERATION_PAUSE_EXPORT_OBJECT	0xC12B
#define MTP_FORMAT_ASSOCIATION		0x3001	/* a folder */
#define MTP_RESPONSE_UNDEFINED		0x2000
#define MTP_RESPONSE_OK			0x2001
#define MTP_RESPONSE_GENERAL_ERROR	0x2002
#define MTP_RESPONSE_SESSION_NOT_OPEN	0x2003
#define MTP_RESPONSE_OPERATION_NOT_SUPPORTED	0x2005
#define MTP_RESPONSE_PARAMETER_NOT_SUPPORTED	0x2006
#define MTP_RESPONSE_INCOMPLETE_TRANSFER	0x2007
#define MTP_RESPONSE_INVALID_STORAGE_ID	0x2008
#define MTP_RESPONSE_INVALID_OBJECT_HANDLE	0x2009
#define MTP_RESPONSE_DEVICE_PROP_NOT_SUPPORTED	0x200A
#define MTP_RESPONSE_INVALID_OBJECT_FORMAT_CODE	0x200B
#define MTP_RESPONSE_STORE_FULL		0x200C
#define MTP_RESPONSE_STORE_READ_ONLY	0x200E
#define MTP_RESPONSE_ACCESS_DENIED	0x200F
#define MTP_RESPONSE_STORE_NOT_AVAILABLE	0x2013
#define MTP_RESPONSE_SPECIFICATION_BY_FORMAT_UNSUPPORTED	0x2014
#define MTP_RESPONSE_NO_VALID_OBJECT_INFO	0x2015
#define MTP_RESPONSE_INVALID_PARENT_OBJECT	0x201A
#define MTP_RESPONSE_DESTINATION_UNSUPPORTED	0x2020
#define MTP_RESPONSE_INVALID_PARAMETER	0x201D
#define MTP_RESPONSE_SESSION_ALREADY_OPEN	0x201E
/* the vendor responses (the names are ours) */
#define MTP_RESPONSE_VENDOR_INVALID_CONTEXT	0xA001
#define MTP_RESPONSE_VENDOR_UNKNOWN_STATUS	0xA004
#define MTP_RESPONSE_VENDOR_DATA_TOO_LARGE	0xA005
#define MTP_RESPONSE_VENDOR_NO_MEMORY	0xA007
#define MTP_RESPONSE_VENDOR_INVALID_IMPORT_HANDLE	0xA00A
#define MTP_RESPONSE_VENDOR_NO_IMPORT_HANDLE	0xA011
#define MTP_RESPONSE_VENDOR_OBJECT_EXISTENCE	0xA02C
#define MTP_RESPONSE_INVALID_OBJECT_PROP_CODE	0xA801
#define MTP_RESPONSE_INVALID_OBJECT_PROP_FORMAT	0xA802
#define MTP_RESPONSE_INVALID_OBJECT_PROP_VALUE	0xA803
#define MTP_RESPONSE_INVALID_OBJECT_REFERENCE	0xA804
#define MTP_RESPONSE_INVALID_DATASET	0xA806
#define MTP_RESPONSE_VENDOR_OBJECT_TOO_LARGE	0xA809
#define MTP_RESPONSE_VENDOR_CANCELED	0xA020

/* an MTP operation request as the device thread receives it */
typedef struct MtpOperation {
	SceUInt16 code;
	SceUInt16 reserved;
	SceUInt32 sessionId;
	SceUInt32 transactionId;
	SceUInt32 param[5];
	SceUInt32 numParams;
} MtpOperation;

/* an MTP event as it is sent to the host (28 bytes) */
typedef struct MtpEvent {
	SceUInt16 code;
	SceUInt16 reserved;
	SceUInt32 sessionId;
	SceUInt32 transactionId;
	SceUInt32 param[3];
	SceUInt32 numParams;
} MtpEvent;

/* the command handler the task queue's jobs run each operation through */
typedef int (*MtpCommandHandler)(MtpOperation *command, MtpOperation *response);

/* the object a SendObjectInfo announced, until its SendObject */
typedef struct MtpObjectTransfer {
	int isFile;
	SceUInt32 handle;
	SceUInt32 size;
	char path[1024];
	int importing;
} MtpObjectTransfer;

/* the object an import works on */
typedef struct MtpObjectImport {
	int reserved;
	SceUInt32 handle;
	char data[1032];
} MtpObjectImport;

/* the connection's state and the connected host, as its initiatorInfo describes it */
typedef struct MtpStatus {
	int state;			/* MTP_STATUS_STATE_* */
	int platformType;		/* the host's platformType, MTP_PLATFORM_TYPE_* */
	int protocolVersion;		/* the host's protocolVersion, 1900010: 01.90.010 */
	int applicationType;		/* the host's applicationType, below 8 */
	char name[64];			/* the host's name */
	SceUInt32 flags;
	int platformClass;		/* MTP_PLATFORM_CLASS_* */
	char osVersion[8];		/* the host's osVersion */
	int reason;			/* why the last connection ended, -1: none */
	int arg;
} MtpStatus;

/* the host's version strings and how its protocol version compares to ours */
typedef struct MtpHostVersion {
	int check;			/* mtpStatusCheckProtocolVersion's result */
	char platformSubtype[16];	/* "Win", "Mac", ... */
	char version[16];
} MtpHostVersion;

/* the protocol version this responder speaks, and the oldest each host may use */
#define MTP_PROTOCOL_VERSION		1900010
#define MTP_PROTOCOL_VERSION_PS		1700010
#define MTP_PROTOCOL_VERSION_PC		1800010

/* the host's platformType (the names the initiatorInfo uses) */
#define MTP_PLATFORM_TYPE_PS3		1
#define MTP_PLATFORM_TYPE_WINXP32	2
#define MTP_PLATFORM_TYPE_WINXP64	3
#define MTP_PLATFORM_TYPE_WINVISTA32	4
#define MTP_PLATFORM_TYPE_WINVISTA64	5
#define MTP_PLATFORM_TYPE_WIN7_32	6
#define MTP_PLATFORM_TYPE_WIN7_64	7
#define MTP_PLATFORM_TYPE_MAC		8
#define MTP_PLATFORM_TYPE_PC		9
#define MTP_PLATFORM_TYPE_PS		10
#define MTP_PLATFORM_TYPE_NASNE		11

/* what kind of host a platformType is */
#define MTP_PLATFORM_CLASS_NONE		0
#define MTP_PLATFORM_CLASS_PC		1
#define MTP_PLATFORM_CLASS_PS		2

static inline int mtpPlatformClass(int platformType)
{
	switch (platformType) {
	case 1:
	case 10:
		return MTP_PLATFORM_CLASS_PS;
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
	case 8:
	case 9:
	case 11:
		return MTP_PLATFORM_CLASS_PC;
	default:
		return MTP_PLATFORM_CLASS_NONE;
	}
}

/* a context: one request of the shell's in flight */
/* a data phase between the device thread and the request's caller (40 bytes) */
typedef struct MtpContextSendRecvData {
	SceUID eventFlag;		/* MTP_CONTEXT_SR_* */
	void *buffer;			/* the caller's buffer; NULL once the caller canceled */
	SceSize size;			/* of the buffer */
	SceSize transferred;		/* by the last pass */
	SceUInt64 remaining;		/* of the data phase */
	SceUInt32 response;		/* the host's response code */
	int done;			/* the last pass is over */
	int error;			/* why the data phase stopped */
	SceUInt32 reserved;
} MtpContextSendRecvData;

/* how far a request's data phase is (24 bytes) */
typedef struct MtpContextProgress {
	SceUInt64 done;
	SceUInt64 total;
	SceUInt32 count;		/* the data phases started */
	SceUInt32 reserved;
} MtpContextProgress;

#ifdef __cplusplus
}

/* one request of the API's callers, from its Begin to its End (296 bytes) */
class MtpContext {
public:
	MtpContext();
	virtual ~MtpContext();

	SceUInt32 id;			/* what the API's callers name it by */
	SceUID eventFlag;		/* MTP_CONTEXT_EVF_* */
	void *data;			/* what the request returns, freed with freeData */
	SceSize dataSize;
	SceUInt32 result;		/* what the host reported for the request */
	int state;			/* MTP_CONTEXT_STATE_* */
	SceUInt32 resultParam;
	SceUInt32 useCount;
	MtpContextSendRecvData *sendRecv;	/* the data phase, for a request that has one */
	void (*freeData)(void *data, SceSize size);
	void (*onResult)(MtpContext *context, void *result);	/* when the caller waits */
	int opcode;			/* the MTP operation it runs */
	SceUInt32 reserved;
	SceUInt64 startTick;		/* when its task started to run */
	SceUInt32 timeout;		/* how long it may wait unused, in microseconds */
	MtpContextProgress *progress;
	void (*cancelHandler)(void *arg);	/* once, when it is done or canceled */
	void *cancelHandlerArg;
	SceUInt32 param;
	SceUInt8 work[0xC0];
	SceUInt32 objectHandle;		/* the object a resumed import works on */
	void *pauseData[2];		/* what a paused export resumes from */
	int checkVersion;		/* the host's protocol version is checked */
	SceUInt32 reserved2;
};

extern "C" {
#else
typedef struct MtpContext MtpContext;
#endif

/* a content manager host on the network, as host discovery reports it (200 bytes) */
/* host-type */
#define MTP_NET_HOST_TYPE_WIN		1
#define MTP_NET_HOST_TYPE_MAC		2
#define MTP_NET_HOST_TYPE_NASNE		3
#define MTP_NET_HOST_TYPE_PS3		4

/* host-supported-device */
#define MTP_NET_HOST_SUPPORTS_VITA	0x1
#define MTP_NET_HOST_SUPPORTS_DOLCE	0x2

/* a host as its discovery response describes it (the fields are its header lines) */
typedef struct MtpNetHost {
	char name[0x7E];			/* host-name */
	SceUInt8 id[16];			/* host-id; a nasne's MAC address is its last 6 */
	SceUInt8 padding[2];
	int type;				/* host-type, MTP_NET_HOST_TYPE_* */
	SceUInt16 requestPort;			/* host-request-port */
	SceUInt16 padding2;
	int mtpProtocolVersion;			/* host-mtp-protocol-version */
	int wirelessProtocolVersion;		/* host-wireless-protocol-version */
	char address[16];			/* where the response came from */
	SceUInt64 lastConnectedTime;		/* the registry's: when it was last connected */
	SceUInt8 macAddress[6];			/* host-mac-address */
	SceUInt8 padding3[2];
	SceUInt32 supportedDevice;		/* host-supported-device, MTP_NET_HOST_SUPPORTS_* */
	SceUInt32 reserved2;
} MtpNetHost;

/* mtp_core.c */
int mtpCoreSetPublicStatus(int status, int progress);
int mtpCoreWake(int wait);
int mtpCoreHasPendingOperation(void);
int mtpCoreStart(int ifType, const MtpNetHost *host, MtpCommandHandler handler, int run);
int mtpCoreStop(void);
int mtpCoreIsStarted(int *started);
int mtpCoreGetIfType(void);
int mtpCoreGetHostInfo(MtpNetHost *host);
int sceMtpInit(const SceMtpInitParam *param);
int sceMtpEnd(void);
int mtpCoreWaitSema(void);
int mtpCoreSignalSema(void);
void mtpCoreSetPendingOperation(int pending);
int mtpCoreGetPendingOperation(void);
void mtpCoreClearPendingOperation(void);
int mtpCoreGetPublicStatus(int *progress);
void mtpCoreClearObjectTransfer(void);
int mtpCoreGetObjectTransferIsFile(void);
void mtpCoreSetObjectTransferIsFile(int isFile);
SceUInt32 mtpCoreGetObjectTransferSize(void);
void mtpCoreSetObjectTransferSize(SceUInt32 size);
SceUInt32 mtpCoreGetObjectTransferHandle(void);
SceUInt32 mtpCoreGetObjectTransferHandle2(void);
void mtpCoreSetObjectTransferHandle(SceUInt32 handle);
void mtpCoreClearObjectImport(void);
void mtpCoreSetObjectImportHandle(SceUInt32 handle);
SceUInt32 mtpCoreGetObjectImportHandle(void);
void mtpCoreSetObjectTransferPath(const char *path);
void mtpCoreObjectTransferNop(void);
void mtpCoreSetObjectTransferImporting(int importing);
void mtpCoreSetImportRunning(int running);
int mtpCoreGetImportRunning(void);
void mtpCoreSetImportContextId(int contextId);
void mtpCoreImportNop(void);
int mtpCoreSleep(void);
int mtpCoreWakeCancel(void);
int setConnected(void);
void mtpCoreSetCancelRequested(int requested);
int mtpCoreSendData(const void *data, SceSize size, SceUInt64 totalSize);
int mtpCoreRecvData(void *data, SceSize size, SceUInt64 *totalSize);
int mtpCoreDriverSendData(const void *data, SceSize size, SceUInt64 totalSize);
int mtpCoreDriverRecvData(void *data, SceSize size, SceUInt64 *totalSize);
int mtpCoreSendResponse(const MtpEvent *event, SceUInt32 contextId);
int mtpCoreSendEvent(const MtpEvent *event);
void mtpCoreSetConnectTimeout(int seconds);
SceUInt16 mtpCoreGetOperationCode(void);

/* mtp_dev.c: the device, on the interface it was started on */
void mtpDevInitialize(void);
void mtpDevTerminate(void);
int mtpDevStart(int ifType, const MtpNetHost *host);
int mtpDevGetHostInfo(MtpNetHost *host);
int mtpDevGetIfType(void);
int mtpDevStop(void);
void mtpDevSetConnectTimeout(int timeout);
int mtpDevWaitConnect(int *found, int *reason);
int mtpDevIfWaitConnect(SceUInt32 timeout);
int mtpDevIfIsConnected(void);
int mtpDevIfChangePhase(int phase);
int mtpDevIfRecvCommand(MtpOperation *operation);
int mtpDevIfSendResponse(const void *response);
int mtpDevIfSendData(SceUInt32 transactionId, const void *data, SceSize size,
	SceUInt64 totalSize);
int mtpDevIfRecvData(SceUInt32 transactionId, void *data, SceSize size, SceUInt64 *totalSize);
int mtpDevDriverSendData(SceUInt32 transactionId, const void *data, SceSize size,
	SceUInt64 totalSize);
int mtpDevDriverRecvData(SceUInt32 transactionId, void *data, SceSize size,
	SceUInt64 *totalSize);
int mtpDevIfSendEvent(const MtpOperation *command, const void *event);
int mtpDevDriverHandleErrorResponse(const MtpOperation *operation, MtpOperation *response,
	SceUInt32 value, int code, int send);
int mtpDevIfReset(void);
int mtpDevStopStartPort(void);
int mtpDevIfCancelTransfer(void);
const void *mtpDevGetPort(void);

/* mtp_driver.c: data phases over SceMtpIf, in pieces the interface takes */
int MtpIfSendData(const void *port, SceUInt32 transactionId, const void *data, SceSize size,
	SceUInt64 totalSize);
int MtpIfRecvData(const void *port, SceUInt32 transactionId, void *data, SceSize size,
	SceUInt64 *totalSize);
int mtpDriverSendData(const void *port, int ifType, SceUInt32 transactionId, const void *data,
	SceSize size, SceUInt64 totalSize);
int mtpDriverRecvData(const void *port, int ifType, SceUInt32 transactionId, void *data,
	SceSize size, SceUInt64 *totalSize);
int mtpDriverSendEvent(const void *port, const MtpOperation *command, const void *event);
int mtpDriverHandleErrorResponse(const void *port, const MtpOperation *operation,
	MtpOperation *response, SceUInt32 value, int code, int send);
void mtpDriverClearLastError(void);
int mtpDriverPostError(int error);

/* the interfaces' ports */
#define MTP_IF_PORT_USB		1
#define MTP_IF_PORT_NET		2

/* mtp_if.c: SceMtpIf's operations on the interface in use */
int MtpIfReset(const void *port);
int MtpIfRecvCommand(const void *port, MtpOperation *operation);
int MtpIfSendResponse(const void *port, const void *response);
int MtpIfSendDataWithParam(const void *port, MtpIfTransfer *transfer);
int MtpIfRecvDataWithParam(const void *port, MtpIfTransfer *transfer);
int MtpIfSendEvent(const void *port, const MtpOperation *command, const void *event);
int MtpIfCancelTransfer(const void *port);
int MtpIfWaitConnect(const void *port, SceUInt32 timeout);
int MtpIfIsConnected(const void *port);
int MtpIfChangePhase(const void *port, int phase);
int MtpIfStartPort(const void *port);
int MtpIfStopPort(const void *port);
const void *MtpIfGetPort(SceUInt32 type);
int MtpIfStartDriver(SceUInt32 type);
int MtpIfStopDriver(SceUInt32 type);

/* the same operations over the network (the content manager's MTP over IP) */
int netMtpIfReset(const void *port);
int netMtpIfRecvCommand(const void *port, void *command);
int netMtpIfSendResponse(const void *port, const void *response);
int netMtpIfSendDataWithParam(const void *port, MtpIfTransfer *transfer);
int netMtpIfRecvDataWithParam(const void *port, MtpIfTransfer *transfer);
int netMtpIfSendEvent(const void *port, const void *command, const void *event);
int netMtpIfCancelTransfer(const void *port);
int netMtpIfWaitConnect(const void *port, SceUInt32 timeout);
int netMtpIfIsConnected(const void *port);
int netMtpIfChangePhase(const void *port, int phase);
int netMtpIfStartPort(const void *port);
int netMtpIfStopPort(const void *port);
const void *netMtpIfGetPort(int type);
int netMtpIfStartDriver(int type);
int netMtpIfStopDriver(int type);

/* the USB side */
int mtpUsbMonitorIsCableConnected(void);
void usbChargeInit(void);
void usbChargeEnd(void);
void usbChargeStart(void);
void usbChargeStop(void);

/* the network side */
int netMtpDeviceInitialize(void);
int netMtpDeviceTerminate(void);
int netMtpDeviceGetDeviceId(SceUInt8 *id);
int netMtpIsWlanAvailable(void);
void netMtpSendWakeOnLanPacket(const MtpNetHost *host);
int startHostDiscovery(void);
int stopHostDiscovery(void);
int netMtpDeviceGetHostDiscoveryState(int *state);
int netMtpDeviceGetNumDiscoveredHosts(SceUInt32 *num);
int netMtpDeviceGetDiscoveredHost(SceUInt32 index, MtpNetHost *host);
/* the registration's outcome, passed to its callback: 1 the host timed out */
typedef void (*NetMtpRegisterCallback)(int *timedOut);
int netMtpDeviceRegisterStart(const MtpNetHost *host, NetMtpRegisterCallback callback);
int netMtpDeviceRegisterSendPin(const char *pin);
int netMtpDeviceRegisterEnd(void);
int netMtpDeviceCheckOnline(const MtpNetHost *host, int *online);
int netMtpDeviceCheckRegistered(const MtpNetHost *host, int *registered);
int netMtpDeviceConnectToHost(const MtpNetHost *host);
int netMtpDeviceTakeFound(void);

/* the network side's threads and mutexes (mtp_netmtp_thread.c) */
typedef struct NetMtpThread {
	SceUID thread;
	int (*entry)(void *arg);
	void *arg;
	int reserved;
} NetMtpThread;
NetMtpThread *netMtpThreadCreate(int (*entry)(void *arg), void *arg, int reserved);
int netMtpThreadDelete(NetMtpThread *thread);
int netMtpThreadStart(NetMtpThread *thread);
int netMtpThreadJoin(NetMtpThread *thread, int *exitStatus);
int createMutex(SceUID *mutex);
int deleteMutex(SceUID mutex);
int mutexLock(SceUID mutex);
int mutexUnlock(SceUID mutex);

/* the network side's helpers (mtp_netmtp_util.c) */
int netMtpIdToString(char *dst, SceSize size, const SceUInt8 *id);
int netMtpMacAddressToString(char *dst, SceSize size, const SceUInt8 *mac);
int netMtpNameToString(char *dst, SceSize size, const char *name);

/* paf's strtok (ScePafStdc 766D2A69), which paf's std/string.h does not declare yet */
char *sce_paf_strtok(char *str, const char *delimiters);

/*
 * SceThreadmgrCoredumpTime's sceKernelExitThread, the one the USB monitor imports (as libc
 * does; its link name has the library's prefix, the SDK's threadmgr.h has the plain name)
 */
int SceThreadmgrCoredumpTime_sceKernelExitThread(int exitStatus);
int netMtpPinToString(char *dst, SceSize size, const char *pin);
int netMtpUuidToString(char *dst, SceSize size, const SceUInt8 *id);
int netMtpStringToMacAddress(SceUInt8 *mac, const char *str);
int netMtpStringToUuid(SceUInt8 *id, const char *str);
int netMtpStringToHostType(int *type, const char *str);
SceUInt32 netMtpGetTick(void);
int getMacAddr(SceUInt8 *mac);
void netMtpMakeDeviceId(SceUInt8 *id);
int netMtpHostTypeToString(char *dst, SceSize size, int type);
int netMtpIsPhoneConnection(void);
int getSystemParamUserName(char *name, int withModel);
/* mtp_http_parser.c */
int parse_response(int *status, int *result, const char *buf);
int parse_register_cancel_request(int *errorCode, const char *buf);
int parse_host_discovery_responce(MtpNetHost *host, const char *buf);
/* ScePafStdc's strtok (name unknown) */
/* mtp_registry.c: the registered hosts, kept in the registry */
#define NETMTP_MAX_REGISTERED_HOSTS	8	/* of each kind */
int loadAllHostsData(void);
int loadSettingsDataFromRegistry(void);
int saveAllHostsData(void);
int saveSettingsDataToRegistry(void);
int mtpRegistryFinalize(void);
int mtpRegistryGetInterfaceType(int *type);
int mtpRegistrySetInterfaceType(int type);
int mtpRegistryIsNetMtpHostAssigned(void);
int mtpRegistryGetAssignedNetMtpHost(MtpNetHost *host);
int mtpRegistryAssignNetMtpHost(const MtpNetHost *host, const SceUInt64 *time);
int mtpRegistryUnassignNetMtpHosts(void);
int mtpRegistryGetNumRegisteredNetMtpHosts(SceUInt32 *num);
int mtpRegistryGetNumRegisteredPcHosts(SceUInt32 *num);
int mtpRegistryGetNumRegisteredNasneHosts(SceUInt32 *num);
int mtpRegistryGetNumRegisteredPs3Hosts(SceUInt32 *num);
int mtpRegistryGetRegisteredNetMtpHost(SceUInt32 index, MtpNetHost *host);
int mtpRegistryUpdateNetMtpHost(const MtpNetHost *host);
int mtpRegistryRegisterNetMtpHost(const MtpNetHost *host);
int mtpRegistryUnregisterNetMtpHost(const MtpNetHost *host);

/* the rest of SceLibMtp the device and status call */
int mtpMarlinIsWaiting(void);
int mtpMarlinCancelWaitResult(void);
int mtpBgCopyCancelAll(void);
int mtpQueueKick(void);

/*
 * The device's description ("SPEC:"): the strings the shell's ini file sets, the DeviceInfo
 * dataset and the property descriptions the responder reports.
 */
#define MTP_SPEC_NUM_STORAGES	3

/* a storage the responder exports */
typedef struct MtpSpecStorage {
	char path[256];			/* "ux0:" */
	int attribute;			/* 1 for ux0: */
} MtpSpecStorage;

/* what the shell's ini file sets (the callback loads and saves it whole) */
typedef struct MtpSpecIniData {
	char manufacturer[256];
	char model[256];
	char deviceVersion[256];
	char friendlyName[256];
	char syncPartner[256];
	char deviceIconPath[256];
	MtpSpecStorage storages[MTP_SPEC_NUM_STORAGES];
	char storageFile[256];
} MtpSpecIniData;

/* load (1) or save (0) the ini data */
typedef int (*MtpSpecIniCallback)(MtpSpecIniData *data, int load);

/* a list of MTP codes (operations, events, formats, properties) */
typedef struct MtpCodeList {
	SceUInt32 count;
	const SceUInt16 *codes;
} MtpCodeList;

/* the DeviceInfo dataset */
typedef struct MtpDeviceInfo {
	SceUInt16 standardVersion;
	SceUInt32 vendorExtensionId;
	SceUInt16 vendorExtensionVersion;
	const char *vendorExtensionDesc;
	SceUInt16 functionalMode;
	MtpCodeList operations;
	MtpCodeList events;
	MtpCodeList deviceProperties;
	MtpCodeList captureFormats;
	MtpCodeList playbackFormats;
	const char *manufacturer;
	const char *model;
	const char *deviceVersion;
	const char *serialNumber;
} MtpDeviceInfo;

/* the MTP data types */
#define MTP_TYPE_UNDEF		0x0000
#define MTP_TYPE_INT8		0x0001
#define MTP_TYPE_UINT8		0x0002
#define MTP_TYPE_INT16		0x0003
#define MTP_TYPE_UINT16		0x0004
#define MTP_TYPE_INT32		0x0005
#define MTP_TYPE_UINT32		0x0006
#define MTP_TYPE_INT64		0x0007
#define MTP_TYPE_UINT64		0x0008
#define MTP_TYPE_INT128		0x0009
#define MTP_TYPE_UINT128	0x000A
#define MTP_TYPE_AINT8		0x4001
#define MTP_TYPE_AUINT8		0x4002
#define MTP_TYPE_AINT16		0x4003
#define MTP_TYPE_AUINT16	0x4004
#define MTP_TYPE_AINT32		0x4005
#define MTP_TYPE_AUINT32	0x4006
#define MTP_TYPE_AINT64		0x4007
#define MTP_TYPE_AUINT64	0x4008
#define MTP_TYPE_STR		0xFFFF

/* a property's value, as wide as its data type */
typedef union MtpPropValue {
	SceUInt8 u8;
	SceUInt16 u16;
	SceUInt32 u32;
	SceUInt64 u64;
	SceUInt32 u128[4];
	const char *str;
} MtpPropValue;

/* the forms of a property's description (formFlag) */
#define MTP_FORM_NONE		0
#define MTP_FORM_RANGE		1
#define MTP_FORM_ENUM		2
#define MTP_FORM_DATETIME	3
#define MTP_FORM_FIXED_ARRAY	4
#define MTP_FORM_REGEX		5
#define MTP_FORM_BYTE_ARRAY	6

/* a range form for each width: minimum, maximum and step, packed */
typedef struct MtpPropRange8 {
	SceUInt8 min, max, step;
} MtpPropRange8;

typedef struct MtpPropRange16 {
	SceUInt16 min, max, step;
} MtpPropRange16;

typedef struct MtpPropRange32 {
	SceUInt32 min, max, step;
} MtpPropRange32;

typedef struct MtpPropEnum {
	SceUInt16 count;
	SceUInt16 reserved;
	const void *values;		/* count values of the data type */
} MtpPropEnum;

typedef union MtpPropForm {
	MtpPropRange8 range8;
	MtpPropRange16 range16;
	MtpPropRange32 range32;
	SceUInt64 range64[3];
	MtpPropEnum enumeration;
	const char *str;
	SceUInt32 maxLength;		/* MTP_FORM_BYTE_ARRAY */
} MtpPropForm;

/* a device or object property's description */
/* MtpPropDesc.getSet: whether the host may set the property */
#define MTP_GET		0
#define MTP_GET_SET	1

typedef struct MtpPropDesc {
	SceUInt16 code;
	SceUInt16 dataType;
	SceUInt8 getSet;
	MtpPropValue defaultValue;
	SceUInt32 groupCode;
	SceUInt8 formFlag;
	MtpPropForm form;
} MtpPropDesc;

/* a form one object format has instead of its property's own */
typedef struct MtpPropFormatForm {
	SceUInt16 format;
	union {
		MtpPropRange32 range32;
		MtpPropEnum enumeration;
	} form;
} MtpPropFormatForm;

void mtpSpecLoadIniData(MtpSpecIniCallback callback);
void mtpSpecTerminate(void);
char *mtpSpecGetSerialNumber(void);
char *mtpSpecGetSyncPartner(void);
char *mtpSpecGetDeviceIconPath(void);
char *mtpSpecGetFriendlyName(void);
void mtpSpecSetFriendlyName(const char *name);
char *mtpSpecFindStorage(const char *name);
int mtpSpecGetStorageAttribute(const char *name);
const MtpDeviceInfo *mtpSpecGetDeviceInfo(void);
const MtpPropDesc *mtpSpecGetDevicePropDesc(SceUInt16 code);
const MtpCodeList *mtpSpecGetObjectPropsSupported(SceUInt16 format);
int mtpSpecIsSupportedFormat(SceUInt16 format);
int mtpSpecIsReferenceFormat(SceUInt16 format);
const MtpPropDesc *mtpSpecGetObjectPropDesc(SceUInt16 code);
const void *mtpSpecGetObjectPropFormatForm(SceUInt16 code, SceUInt16 format);
int mtpSpecGetVitaInformation(char *buf, SceSize size, int withOnlineId);
void mtpSpecGetCapabilityInfo(const char **xml, int *length);

/* utilities */
int mtpUtilStrcmp(const char *a, const char *b);
SceSize mtpUtilStrlen(const char *s);
char *mtpUtilStrcpy(char *dst, const char *src);
char *mtpUtilStrcat(char *dst, const char *src);
int mtpUtilGetSerialNumberFromIDStorage(char *serial);
void *mtpUtilMalloc(SceSize size);
void mtpUtilFree(void *ptr);

/* an MTP string being packed: its length in characters and where its UTF-16 goes */
typedef struct __attribute__((packed)) MtpString {
	SceUInt8 numChars;		/* in: the room, 0xFF; out: the characters with the NUL */
	void *chars;
} MtpString;

/* the bytes packed, or an error */
int mtpUtilPackMtpString(MtpString *string, const char *src);
int mtpUtilPackCodeList(void *dst, const MtpCodeList *list);
int mtpUtilStrncmp(const char *a, const char *b, int size);
char *mtpUtilStrncpy(char *dst, const char *src, int size);
int mtpUtilIsUsbAvailable(void);
int mtpUtilMemcmp(const void *a, const void *b, int size);
void *mtpUtilMemcpy(void *dst, const void *src, int size);
int mtpUtilSetMtpResponse(SceUInt32 code, const SceUInt32 *params, SceUInt32 numParams,
	const MtpOperation *operation, MtpOperation *response);
SceUInt8 mtpUtilUInt2HexChar(SceUInt8 value);
void *mtpUtilMallocAlign(SceSize align, SceSize size);
char *mtpUtilStrchr(const char *s, int c);
char *mtpUtilStrrchr(const char *s, int c);
int mtpUtilUnpackMtpString(char *dst, int size, const MtpString *string);
int mtpUtilGetUnpackMtpStringSize(MtpString *string);

/* an array as MTP packs it: its count, then the elements */
typedef struct MtpArray {
	int count;
	const void *values;
} MtpArray;

int mtpUtilPackUInt8Array(void *dst, const MtpArray *array);
int mtpUtilPackUInt32Array(void *dst, const MtpArray *array);
int mtpUtilPackUInt64Array(void *dst, const MtpArray *array);
/* "YYYYMMDDThhmmss.0" */
int mtpUtilDateTimeToString(char *str, const SceDateTime *time);
int mtpUtilStringToDateTime(const char *str, SceDateTime *time);
int mtpUtilStringToUtcDateTime(const char *str, SceDateTime *time);
int mtpUtilMd5(const char *str, const void *salt, unsigned char *digest);
int mtpUtilResponseCodeToError(SceUInt32 code);
int mtpUtilTrimFileName(const char *src, char *dst, SceUInt32 dstSize, SceUInt32 srcSize);
void mtpUtilReplaceInvalidChars(char *name, SceSize length);

/* the transfer buffers: the utilities' heap or the caller's large buffers */
#define MTP_UTIL_BUFFER_LARGE		0
#define MTP_UTIL_BUFFER_HEAP		1
#define MTP_UTIL_BUFFER_STATE_NONE	0
#define MTP_UTIL_BUFFER_STATE_TO_HEAP	1
#define MTP_UTIL_BUFFER_STATE_LARGE	2
#define MTP_UTIL_BUFFER_STATE_TO_LARGE	3
#define MTP_UTIL_BUFFER_STATE_HEAP	4

int mtpUtilIsLargeBufferEnabled(void);
void mtpUtilEnableLargeBuffer(void);
int mtpUtilGetBufferState(void);
void mtpUtilSetBufferState(int state);
int mtpUtilLargeBufferStart(void);
int mtpUtilLargeBufferEnd(void);
void *mtpUtilLargeBufferMemalign(SceSize align, SceSize size);
void mtpUtilLargeBufferFree(void *ptr);
int mtpUtilChangeBufferAllocator(void **buffer0, void **buffer1, SceSize *size,
	void (**freeFunc)(void *ptr), int allocator);


/* mtp_status.c: the connection's state */
int mtpStatusCheckProtocolVersion(void);
int mtpStatusIsSupportedHost(void);
int mtpStatusGetPlatformClass(void);
void mtpStatusSetState(int state, int reason, int quiet);
void mtpStatusReset(void);
void mtpStatusSetFlags(SceUInt32 value, SceUInt32 mask);
void mtpStatusGet(MtpStatus *status);
void mtpStatusGetHostVersion(MtpHostVersion *version);
int mtpStatusGetState(void);
int mtpStatusIsApplicationType1(void);
int mtpStatusCheck(int checkVersion);
int mtpStatusSetArg(int arg);

/* the other components the core starts and stops */
int sceMtpTerminateExecution(void);
int mtpQueueInit(MtpCommandHandler handler);
int mtpQueueTerminate(void);
int mtpQueuePushCommand(MtpOperation *operation, int flags);
int mtpBgCopyIdleTimerInitialize(void);
void mtpBgCopyIdleTimerTerminate(void);
int mtpUsbMonitorCreate(void);
int mtpUsbMonitorDelete(void);
int mtpUsbMonitorStart(void);
int mtpUsbMonitorStop(void);
int mtpUsbMonitorIsStarted(void);
int mtpUsbMonitorHasBeenDisconnected(void);
int mtpCapabilityInfoInitialize(void);
void mtpCapabilityInfoFinalize(void);
int mtpUtilInitialize(SceSize size, const MtpLargeBufferFuncs *largeBufferFuncs);
int mtpUtilUninitialize(void);
void *mtpUtilMemset(void *dst, int ch, int size);
int mtpXferInitialize(int priority, int cpuAffinityMask);
int mtpXferUninitialize(void);
/* the transfer thread's requests and their completions */
#define MTP_XFER_EVF_SEND		0x1
#define MTP_XFER_EVF_RECV		0x2
#define MTP_XFER_EVF_SEND_DONE		0x10
#define MTP_XFER_EVF_RECV_DONE		0x20
#define MTP_XFER_EVF_EXIT		0x8000
/* the transfer thread's requests, and the waits for their ends */
#define MTP_XFER_REQUEST_SEND		1
#define MTP_XFER_REQUEST_RECV		2
#define MTP_XFER_DONE_SEND		0x10
#define MTP_XFER_DONE			0x20
int mtpXferStart(SceUInt32 request, void *data, SceSize size, SceUInt64 totalSize);
int mtpXferWait(SceUInt32 done);
/* the network responder's socket timeouts around a transfer */
/* MTP/IP ("MTPIP:"): the packets of PTP/IP over the command and event connections */
#define MTP_IP_OPERATION_REQUEST	6
#define MTP_IP_START_DATA		9
#define MTP_IP_DATA			10
#define MTP_IP_CANCEL			11
#define MTP_IP_END_DATA			12

/* MTP/IP's errors */
#define MTP_IP_ERROR_SOCKET		((int)0x80543601)
#define MTP_IP_ERROR_INVALID_ARGUMENT	((int)0x80543603)
#define MTP_IP_ERROR_ABORTED		((int)0x80543606)
#define MTP_IP_ERROR_DISCONNECTED	((int)0x80543607)
#define MTP_IP_ERROR_CONNECTION		((int)0x8054360C)

typedef struct MtpIpHeader {
	SceUInt32 length;		/* of the whole packet, this header included */
	SceUInt32 type;
} MtpIpHeader;

int mtp_ip_wait_for_recv(int s, int timeout);
int mtp_ip_establish_connection(SceUInt16 port, int *eventSocket, int *commandSocket);
int mtp_ip_close_connection(int eventSocket, int commandSocket);
int mtp_ip_is_data_received(int s, int *received);
int mtp_ip_get_header(int s, MtpIpHeader *header);
int mtp_ip_read_cancel(int s, SceUInt32 *transactionId);
int mtp_ip_read_operation_request(int s, MtpOperation *operation, SceUInt32 length);
int mtp_ip_send_operation_response(int s, const MtpOperation *response);
int mtp_ip_send_event(int s, const MtpEvent *event);
int mtp_ip_read_start_data(int s, SceUInt32 *transactionId, SceUInt64 *totalSize);
int mtp_ip_send_start_data(int s, SceUInt32 transactionId, SceUInt64 totalSize);
int mtp_ip_read_transaction_id(int s, SceUInt32 *transactionId);
int mtp_ip_read_data(int s, void *buffer, SceUInt32 size);
int mtp_ip_send_data(int s, SceUInt32 transactionId, const void *buffer, SceUInt32 size,
		     int last);
int mtp_ip_ignore_recv_data(int s, SceUInt32 length);
int mtp_ip_abort(int s);

/* the network interface's responder (MTP/IP, "NET MTP RESPONDER:") */
int netMtpResponderStart(void);
int netMtpResponderStop(void);
int netMtpResponderWaitConnect(SceUInt16 port);
int netMtpResponderReset(void);
int netMtpResponderRecvCommand(MtpOperation *operation);
int netMtpResponderSendResponse(const MtpOperation *response);
int netMtpResponderSendData(MtpIfTransfer *transfer);
int netMtpResponderRecvData(MtpIfTransfer *transfer);
int netMtpResponderSendEvent(const MtpEvent *event);
int netMtpResponderCancelTransfer(void);
SceUInt16 netMtpResponderGetState(void);
int netMtpResponderSetPhase(void);
void netMtpResponderDisconnect(void);
void NetMtpExpandSocketSendTimeout(void);
void NetMtpResetSocketSendTimeout(void);
void NetMtpExpandSocketReceiveTimeout(void);
void NetMtpResetSocketReceiveTimeout(void);
int mtpStrgInitialize(void);
int mtpStrgUninitialize(void);
/* the storages whose databases are open */
int mtpStrgGetNumActiveStorages(void);
/* mtp_file.c */
int mtpFileExists(const char *path);
int mtpFileGetDates(const char *path, SceDateTime *ctime, SceDateTime *mtime);
int mtpFileSetDates(const char *path, const SceDateTime *ctime, const SceDateTime *mtime);
int mtpFileGetSize(const char *path, SceOff *size);
int mtpFileCreate(const char *path, SceUInt16 format);
int mtpFileDelete(const char *path, SceUInt16 format);
int mtpFileGetData(const char *path, void *buf, SceSize size);
SceUID mtpFileOpen(const char *path, int flags, SceMode mode);
int mtpFileClose(SceUID fd);
SceOff mtpFileSeek(SceUID fd, SceOff offset, int whence);
int mtpFileRead(SceUID fd, void *buf, SceSize size);
int mtpFileWrite(SceUID fd, const void *buf, SceSize size);
int mtpFileGetStorageInfo(const char *path, SceUInt64 *maxCapacity, SceUInt64 *freeSpace);

/* mtp_context.cpp */
int mtpContextInitialize(void);
int mtpContextFinalize(void);
int mtpContextCreate(int kind, MtpContext **context, int opcode);
int mtpContextIsIdle(int *idle);
int mtpContextCreateCancel(MtpContext **context, int opcode);
int mtpContextCreateTerminate(MtpContext **context, int opcode);
int mtpContextCreatePause(MtpContext **context, int opcode);
void mtpContextDelete(MtpContext *context);
int mtpContextGet(SceUInt32 contextId, int use, MtpContext **context);
int mtpContextGetContextByEvent(int opcode, MtpContext **context);
int mtpContextPutContext(MtpContext *context, int restart);
int mtpContextSetDone(MtpContext *context);
int mtpContextCancelById(SceUInt32 contextId);
int mtpContextCancelAll(void);
int mtpContextCreateProgress(MtpContext *context);
int mtpContextSetProgress(MtpContext *context, SceUInt64 done, SceUInt64 total, int newPhase);
int mtpContextGetProgress(SceUInt32 contextId, MtpContextProgress *progress);
int mtpContextInvalidateContext(MtpContext *context, int notify);
int mtpContextWaitBuffer(MtpContextSendRecvData *data, void **buffer, SceSize *size,
	SceUInt32 *response);
int mtpContextPassRecvData(MtpContext *context, SceUInt64 size, SceUInt32 *response,
	int last);
int mtpContextPassSendData(MtpContextSendRecvData *data, SceUInt64 size, SceUInt64 totalSize,
	SceUInt32 *response, int last);
int mtpContextWaitResult(MtpContext *context, void *result);
int mtpContextWaitSendRecv(MtpContext *context, MtpContextSendRecvData *data, int send);
int mtpContextRecvData(MtpContext *context, void *data, SceSize size, SceSize *received,
	SceUInt64 *totalSize);
int mtpContextSendData(MtpContext *context, const void *data, SceSize size, SceSize *sent,
	SceUInt64 *totalSize);
int mtpContextNotifyResultResponse(MtpContext *context, SceUInt32 response);
int mtpContextSetCancelHandler(MtpContext *context, void (*handler)(void *arg), void *arg);
int mtpContextCancel(MtpContext *context, SceUInt32 contextId);
int mtpContextSetPauseContextData(SceUInt32 contextId);
SceUInt32 mtpContextGetPauseContextId(void);
int mtpContextIsDeleted(MtpContext *context);
int mtpContextPauseSource(void *data);
int mtpContextPauseTarget(void *data);
void mtpContextSetCanceling(int canceling);
int mtpContextIsCanceling(void);
int mtpExportPauseSource(void *data);
int mtpExportPauseTarget(void *data);
/* mtp_http.c */
int mtpContextHttpGetData(MtpContext *context, const char *url, SceSize urlLength,
	SceUInt64 offset);
int mtpContextHttpGetProperty(MtpContext *context, const char *url, SceSize urlLength);
int mtpCmdHttpGetUrl(MtpOperation *operation, MtpOperation *response);
int mtpCmdHttpSendData(MtpOperation *operation, MtpOperation *response);
int mtpCmdHttpSendProperty(MtpOperation *operation, MtpOperation *response);
/* mtp_prop.c: the properties' descriptions and values, packed for the host */
#define MTP_DEVICE_PROP_BATTERY_LEVEL		0x5001
#define MTP_DEVICE_PROP_SYNCHRONIZATION_PARTNER	0xD401
#define MTP_DEVICE_PROP_DEVICE_FRIENDLY_NAME	0xD402
#define MTP_DEVICE_PROP_DEVICE_ICON		0xD405
/* a device property's dataset buffer: the icon's is larger */
#define MTP_DEVICE_PROP_DESC_SIZE	0x4000
#define MTP_DEVICE_PROP_DESC_SIZE_LARGE	0x10000

/* the object property the host may send in pieces larger than the device keeps */
#define MTP_OBJECT_PROP_REPRESENTATIVE_SAMPLE_DATA	0xDC86

/* an object property's value: its data type, where it is and how much room it has */
typedef struct MtpPropData {
	SceUInt32 dataType;		/* MTP_TYPE_* */
	void *data;			/* MTP_TYPE_STR: the string itself */
	SceUInt32 size;
} MtpPropData;

/* the room for a code list (count codes), and what the list filled it with */
typedef struct MtpCodeArray {
	SceUInt32 count;
	SceUInt16 *codes;
} MtpCodeArray;

/* each returns the bytes packed, or an error */
int mtpPropPackDevicePropDesc(SceUInt16 code, void *buf, SceSize size);
int mtpPropPackDevicePropValue(SceUInt16 code, void *buf, SceSize size);
int mtpPropPackObjectPropsSupported(SceUInt16 format, MtpCodeArray *array);
int mtpPropPackObjectPropDesc(SceUInt16 code, SceUInt16 format, void *buf);
int mtpPropPackObjectPropValue(const MtpPropData *value, void *buf);
int mtpPropUnpackObjectPropValue(MtpPropData *value, const void *buf);
int mtpTaskQueuePush(MtpContext *context, const MtpEvent *event);
void mtpTaskQueueSetTitleId(const char *titleId);
int mtpTaskQueueGetTitleId(char *titleId, SceSize size);
int mtpTaskQueueCancel(MtpContext *context);
int mtpTaskQueueCancelAll(void);
int mtpTaskQueueCancelAllForDoCancelAndPause(void);
int mtpTaskQueueGetContext(SceUInt32 contextId, MtpContext **context);
int mtpBgCopyRestartIdleTimer(void);
int mtpIfEndCommon(SceUInt32 contextId, void *result, const char *name);
int mtpXmlAllocatorInitialize(void);
int mtpXmlAllocatorUninitialize(void);
/* the application's callbacks (mtp_callback.c) */
typedef void (*MtpCallbackFunc)(int event, void *arg);
typedef int (*MtpEventHandler)(int event, void *arg);
int mtpCallbackRegister(SceUInt32 events, MtpCallbackFunc func, void *arg);
int mtpCallbackUnregister(SceUInt32 events, MtpCallbackFunc func);
int mtpCallbackNotify(int event);
void mtpCallbackSetEventHandler(MtpEventHandler handler, void *arg);
void mtpCallbackClearEventHandler(void);
int mtpCallbackInvokeEvent(int event);

/*
 * The API (mtp_base.c) and what it calls in the files that are not C yet; each prototype
 * moves to its file's block when that file is written.
 */
/* the operations the device asks the host for; each request's context names its own */
#define MTP_OPERATION_REQUEST_SEND_NUM_OF_OBJECT	0xC104
#define MTP_OPERATION_REQUEST_SEND_OBJECT_METADATA	0xC105
#define MTP_OPERATION_REQUEST_SEND_OBJECT		0xC107
#define MTP_OPERATION_HTTP_GET_DATA	0xC10B
#define MTP_OPERATION_HTTP_GET_PROPERTY	0xC113
#define MTP_OPERATION_REQUEST_OBJECT_INFO		0xC109
#define MTP_OPERATION_REQUEST_GET_SETTING_INFO		0xC10D
#define MTP_OPERATION_REQUEST_OBJECT_STATUS		0xC10F
#define MTP_OPERATION_REQUEST_SEND_OBJECT_THUMBNAIL	0xC110
#define MTP_OPERATION_REQUEST_DELETE_OBJECT		0xC111
#define MTP_OPERATION_REQUEST_SEND_SETTING_INFO		0xC112
#define MTP_OPERATION_REQUEST_PART_OF_OBJECT		0xC115
#define MTP_OPERATION_REQUEST_FS_OPERATION		0xC117
#define MTP_OPERATION_REQUEST_SEND_PART_OF_OBJECT	0xC118
#define MTP_OPERATION_REQUEST_STORAGE_SIZE		0xC119
#define MTP_OPERATION_REQUEST_COPY_CONFIRMATION_INFO	0xC120
#define MTP_OPERATION_REQUEST_EXPORT_OBJECT		0xC122
#define MTP_OPERATION_REQUEST_OBJECT_METADATA_ITEMS	0xC123
#define MTP_OPERATION_REQUEST_SPECIFIED_OBJECT_METADATA	0xC124
#define MTP_OPERATION_REQUEST_NP_ACCOUNT_INFO		0xC125
#define MTP_OPERATION_REQUEST_RPC_NET_OPERATION		0xC127
#define MTP_OPERATION_REQUEST_RPC_NET_OPERATION_ABORT	0xC128
#define MTP_OPERATION_REQUEST_SEARCH_OBJECT		0xC129
#define MTP_OPERATION_REQUEST_HANDOVER			0xC12A
#define MTP_OPERATION_REQUEST_RESUME_EXPORT_OBJECT	0xC12C
#define MTP_OPERATION_REQUEST_RESUME_IMPORT_OBJECT	0xC12D
#define MTP_OPERATION_REQUEST_NPDRM_RTC			0xC600
#define MTP_OPERATION_REQUEST_NPDRM			0xC601
#define MTP_OPERATION_REQUEST_NPDRM_LICENSE		0xC611

/* what mtpContextRequestFsOperation asks the host to do */
#define MTP_FS_OPERATION_CREATE_DIR	1
#define MTP_FS_OPERATION_DELETE_DIR	3
#define MTP_FS_OPERATION_CREATE_FILE	4
#define MTP_FS_OPERATION_MOVE_DIR	5

/* an object as the API's callers name it: its unique ID and its handle's storage bits */
typedef struct MtpObjectId {
	SceUInt32 uniqueId;
	SceUInt32 storage;
} MtpObjectId;

/* a storage the storage manager knows by name */
typedef struct MtpLogicalStorage MtpLogicalStorage;

/* a physical storage the storage manager knows by name, with its logical storages */
typedef struct MtpStorageInfo {
	const char *name;
	SceUInt16 index;		/* its slot in the storage manager */
	SceUInt16 started;		/* its handles are created */
	SceUInt32 numLogicalStorages;
	MtpLogicalStorage *logicalStorages;
} MtpStorageInfo;

typedef struct MtpStorage MtpStorage;
struct MtpStorage {
	SceUInt32 flags;		/* MTP_STORAGE_FLAG_* */
	MtpStorageInfo *info;
	void *reserved[3];
	int (*start)(MtpStorage *storage);
	int (*stop)(MtpStorage *storage);
	int (*activate)(MtpStorageInfo *info);		/* opens its databases */
	int (*deactivate)(MtpStorageInfo *info, int arg);
};
/* it is not started (its start has not run, or its stop has) */
#define MTP_STORAGE_FLAG_STOPPED	0x1000

typedef struct MtpCache MtpCache;

/* a storage's StorageInfo dataset, as the storage manager keeps it */
typedef struct MtpStorageDataset {
	SceUInt16 storageType;
	SceUInt16 filesystemType;
	SceUInt16 accessCapability;
	SceUInt16 padding;
	SceUInt64 maxCapacity;
	SceUInt64 freeSpaceInBytes;
	SceUInt32 freeSpaceInObjects;
	SceUInt32 padding2;
	const char *storageDescription;
	const char *volumeIdentifier;
} MtpStorageDataset;

typedef struct MtpDB MtpDB;

/* a storage's logical storage (104 bytes, in the storage manager's table) */
struct MtpLogicalStorage {
	const char *rootFolder;		/* its folder under the mount point */
	const char **rootDirs;		/* the folders the host may make at its root, or NULL */
	SceUInt16 index;		/* its number in its physical storage, from 1 */
	SceUInt16 largeObjectSupported;	/* it takes objects of 4 GiB and more */
	SceUInt32 writeProtected;	/* 1: the host may only read and delete */
	SceUInt32 failed;		/* bit 0: mtpDBInitGenericWay could not set it up */
	SceUInt32 maxObjects;		/* the objects its database has room for */
	SceUInt32 numObjects;		/* mtpDBGetUniqueIdInfo's */
	int dbType;			/* MTP_DB_TYPE_* of its database */
	MtpDB *db;
	const MtpStorageInfo *info;	/* info->name: the mount point */
	MtpCache *cache;
	int (*getStorageInfo)(MtpLogicalStorage *storage);
	/* the folder, under its root, that keeps objects of an object format */
	int (*getContentRoot)(MtpLogicalStorage *storage, SceUInt16 format, char *path);
	/* the path a storage path names on the device, and the path the PFS layer opens */
	char *(*convertNormalPath)(MtpLogicalStorage *storage, const char *path, char *normal,
		SceSize size);
	char *(*createPfsPath)(MtpLogicalStorage *storage, const char *path, char *pfsPath,
		SceSize size, int create);
	SceUInt8 reserved4[4];
	MtpStorageDataset dataset;	/* what GetStorageInfo sends */
};

/* a logical storage's database (DB) */
#define MTP_DB_TYPE_NORMAL	0	/* obsolete */
#define MTP_DB_TYPE_SQLITE	1
#define MTP_DB_NOT_OPEN		((struct sqlite3 *)-1)
struct MtpDB {
	int type;			/* MTP_DB_TYPE_* */
	struct sqlite3 *sqlite;		/* MTP_DB_NOT_OPEN until it is open */
	char path[256];			/* the database file */
	SceUID mutex;
	MtpDB *opened;			/* itself while it is open */
};
int mtpDBGetUniqueIdInfo(MtpDB *db, SceUInt32 *numObjects);

/* a storage's handle map (OMAP): its handles and their unique ids, in SQLite */
typedef struct MtpObjmap {
	SceUInt32 nextIndex;
	SceUInt32 magic;		/* MTP_OBJMAP_MAGIC while its storage is registered */
	struct sqlite3 *db;
	SceUInt32 handleBase;		/* its storage's bits of a handle */
	SceKernelLwMutexWork lock;
} MtpObjmap;
#define MTP_OBJMAP_MAGIC		0xDEADBEEF
int mtpObjmapCreate(MtpObjmap *objmap, SceUInt32 storageId);
int mtpObjmapDestroy(MtpObjmap *objmap);
/* a handle for each object of db */
int mtpObjmapSetupHandles(MtpObjmap *objmap, MtpDB *db);
int mtpObjmapUnsetupHandles(MtpObjmap *objmap);
int mtpObjmapDeallocate(MtpObjmap *objmap, SceUInt32 uniqueId);
int mtpObjmapAllocate(MtpObjmap *objmap, SceUInt32 flags, SceUInt32 uniqueId, SceUInt32 parent,
	SceUInt32 *handle);
int mtpObjmapConvertMtpHandleToUniqueId(MtpObjmap *objmap, SceUInt32 handle,
	SceUInt32 *uniqueId);
int mtpObjmapConvertUniqueIdToMtpHandle(MtpObjmap *objmap, SceUInt32 uniqueId,
	SceUInt32 *handle);
int mtpObjmapGetParentMtpHandleFromUniqueId(MtpObjmap *objmap, SceUInt32 uniqueId,
	SceUInt32 *parent);
int mtpObjmapGetParentMtpHandleFromMtpHandle(MtpObjmap *objmap, SceUInt32 handle,
	SceUInt32 *parent);
int mtpObjmapGetNumUniqueIds(MtpObjmap *objmap, SceUInt32 parent, SceUInt32 *num);
int mtpObjmapGetUniqueIds(MtpObjmap *objmap, SceUInt32 parent, SceUInt32 offset,
	SceUInt32 count, SceUInt32 *ids);

/* the host's storage (HOST), the one storage there is */
int mtpHostInitialize(void);
int mtpHostUninitialize(MtpStorage *storage);

/* the commands (CMD and CMDE) */
int mtpCmdGetDeviceInfo(const MtpOperation *operation, MtpOperation *response);
int mtpCmdOpenSession(const MtpOperation *operation, MtpOperation *response);
int mtpCmdCloseSession(const MtpOperation *operation, MtpOperation *response);
int mtpCmdGetStorageIds(const MtpOperation *operation, MtpOperation *response);
int mtpCmdGetStorageInfo(const MtpOperation *operation, MtpOperation *response);
int mtpCmdGetNumObjects(const MtpOperation *operation, MtpOperation *response);
int mtpCmdGetObjectHandles(const MtpOperation *operation, MtpOperation *response);
int mtpCmdGetObjectInfo(const MtpOperation *operation, MtpOperation *response);
int mtpCmdGetObject(const MtpOperation *operation, MtpOperation *response);
int mtpCmdSendObjectInfo(const MtpOperation *operation, MtpOperation *response);
int mtpCmdSendObject(const MtpOperation *operation, MtpOperation *response);
int mtpCmdGetDevicePropDesc(const MtpOperation *operation, MtpOperation *response);
int mtpCmdGetDevicePropValue(const MtpOperation *operation, MtpOperation *response);
int mtpCmdSetDevicePropValue(const MtpOperation *operation, MtpOperation *response);
int mtpCmdGetPartialObject(const MtpOperation *operation, MtpOperation *response);
int mtpCmdGetMarlinMsgHdls(const MtpOperation *operation, MtpOperation *response);
int mtpCmdGetMarlinMsgInfo(const MtpOperation *operation, MtpOperation *response);
int mtpCmdGetMarlinMsg(const MtpOperation *operation, MtpOperation *response);
int mtpCmdSendMarlinMsgInfo(const MtpOperation *operation, MtpOperation *response);
int mtpCmdSendMarlinMsg(const MtpOperation *operation, MtpOperation *response);
int mtpCmdGetVitaInformation(const MtpOperation *operation, MtpOperation *response);
int mtpCmdSendNumOfObject(const MtpOperation *operation, MtpOperation *response);
int mtpCmdGetBrowseInfo(const MtpOperation *operation, MtpOperation *response);
int mtpCmdSendObjectMetadata(const MtpOperation *operation, MtpOperation *response);
int mtpCmdSendObjectThumbnail(const MtpOperation *operation, MtpOperation *response);
int mtpCmdReportResult(const MtpOperation *operation, MtpOperation *response);
int mtpCmdSendInitiatorInfo(const MtpOperation *operation, MtpOperation *response);
int mtpCmdSendSettingInfo(const MtpOperation *operation, MtpOperation *response);
int mtpCmdGetSettingInfo(const MtpOperation *operation, MtpOperation *response);
int mtpCmdGetObjectStatusRequest(const MtpOperation *operation, MtpOperation *response);
int mtpCmdSendHostStatus(const MtpOperation *operation, MtpOperation *response);
int mtpCmdGetPartOfObjectRequest(const MtpOperation *operation, MtpOperation *response);
int mtpCmdSendPartOfObject(const MtpOperation *operation, MtpOperation *response);
int mtpCmdGetFsOperation(const MtpOperation *operation, MtpOperation *response);
int mtpCmdGetPartOfObject(const MtpOperation *operation, MtpOperation *response);
int mtpCmdSendStorageSize(const MtpOperation *operation, MtpOperation *response);
int mtpCmdGetTreateObjectRequest(const MtpOperation *operation, MtpOperation *response);
int mtpCmdSendCopyConfirmationInfo(const MtpOperation *operation, MtpOperation *response);
int mtpCmdGetObjectMetadataItemsRequest(const MtpOperation *operation,
					MtpOperation *response);
int mtpCmdGetCopyConfirmationInfoRequest(const MtpOperation *operation,
					 MtpOperation *response);
int mtpCmdKeepAlive(const MtpOperation *operation, MtpOperation *response);
int mtpCmdGetNetOperationData(const MtpOperation *operation, MtpOperation *response);
int mtpCmdSendNetOperationData(const MtpOperation *operation, MtpOperation *response);
int mtpCmdGetCapabilityInfo(const MtpOperation *operation, MtpOperation *response);
int mtpCmdSendCapabilityInfo(const MtpOperation *operation, MtpOperation *response);
int mtpCmdGetSearchInfo(const MtpOperation *operation, MtpOperation *response);
int mtpCmdGetHandoverInfo(const MtpOperation *operation, MtpOperation *response);
int mtpCmdGetPartOfObjectSpecfiedByHost(const MtpOperation *operation,
					MtpOperation *response);
int mtpCmdCheckObjectExistence(const MtpOperation *operation, MtpOperation *response);
int mtpCmdSendNumOfFoundObject(const MtpOperation *operation, MtpOperation *response);
int mtpCmdSendMarlinResults(const MtpOperation *operation, MtpOperation *response);
int mtpCmdGetMarlinLicenseInfo(const MtpOperation *operation, MtpOperation *response);
int mtpCmdSendMarlinLicenseInfo(const MtpOperation *operation, MtpOperation *response);
int mtpCmdGetMarlinLicense(const MtpOperation *operation, MtpOperation *response);
int mtpCmdSendMarlinLicense(const MtpOperation *operation, MtpOperation *response);
int mtpCmdGetMarlinEventInfo(const MtpOperation *operation, MtpOperation *response);
int mtpCmdSetMarlinLicenseIdToContext(const MtpOperation *operation, MtpOperation *response);
int mtpCmdGetChallenge(const MtpOperation *operation, MtpOperation *response);
int mtpCmdSendRtc(const MtpOperation *operation, MtpOperation *response);
int mtpCmdSendActivation(const MtpOperation *operation, MtpOperation *response);
int mtpCmdSendLicense(const MtpOperation *operation, MtpOperation *response);
int mtpCmdGetObjectPropsSupported(const MtpOperation *operation, MtpOperation *response);
int mtpCmdGetObjectPropDesc(const MtpOperation *operation, MtpOperation *response);
int mtpCmdGetObjectPropValue(const MtpOperation *operation, MtpOperation *response);
int mtpCmdSetObjectPropValue(const MtpOperation *operation, MtpOperation *response);
int mtpCmdGetObjectPropList(const MtpOperation *operation, MtpOperation *response);
int mtpCmdSetObjectPropList(const MtpOperation *operation, MtpOperation *response);
int mtpCmdGetObjectReferences(const MtpOperation *operation, MtpOperation *response);
int mtpCmdSetObjectReferences(const MtpOperation *operation, MtpOperation *response);

/* the contexts' requests */
int mtpContextGetNumOfObject(MtpContext *context, SceUInt32 type, SceUInt32 param);
int mtpContextImportBrowsingData(MtpContext *context, SceUInt32 type, SceUInt32 param1,
	SceUInt32 param2, SceUInt32 param3);
void mtpContextDeleteBrowsingContext(void *data, SceSize size);
int mtpContextImportObjectData(MtpContext *context, SceUInt32 source, const char *path,
	const char *name, int import);
int mtpContextSetResumeContextId(SceUInt32 contextId);
/* object: the unique ID, then the handle's storage bits (an MtpObjectId) */
int mtpContextExportObjectWithCheck(MtpContext *context, SceInt64 object, SceInt64 target);
int mtpContextRequestFsOperation(MtpContext *context, int operation, SceUInt32 param1,
	SceUInt32 param2, const char *path, SceSize size);
int mtpContextGetObjectStatus(MtpContext *context, SceUInt32 type, const char *path,
	SceSize length);
int mtpContextImportObjectThumbnail(MtpContext *context, SceUInt32 handle, SceUInt32 type);
int mtpContextDeleteObject(MtpContext *context, SceUInt32 handle);
int mtpContextGetSystemSetting(MtpContext *context, int setting);
int mtpContextSetSystemSetting(MtpContext *context, int setting, SceUInt32 size);
int mtpContextGetNpAccountInfo(MtpContext *context, SceUInt32 size);
int mtpContextGetPartOfObject(MtpContext *context, SceUInt32 handle, SceUInt64 offset,
	SceUInt64 size, int import);
int mtpContextSendPartOfObject(MtpContext *context, SceUInt32 handle, SceUInt64 offset,
	SceUInt64 size);
int mtpContextGetHostStorageSize(MtpContext *context, SceUInt32 storageId);
int mtpContextCompareObjectData(MtpContext *context, SceInt64 object);
int mtpContextGetTotalObjectSize(MtpContext *context, SceUInt32 numHandles);
int mtpContextGetSpecifiedObjectMetadata(MtpContext *context, SceUInt32 type,
	const SceUInt32 *items, SceUInt32 numItems);
int mtpContextCheckImportHandle(SceUInt32 handle, const MtpContext *context);
int mtpContextNpDrmGetRtc(MtpContext *context, const void *request, SceSize size);
int mtpContextNpDrmExecAuthentication(MtpContext *context, SceUInt32 arg, int activate,
	const void *request, SceSize size);
int mtpContextNpDrmGetLicense(MtpContext *context, const void *rtc, const void *licenseId,
	SceSize size);

/* SceSblGcAuthMgr's user library */
#include <libSceSblGcAuthMgr_stub/gc_auth_mgr_stub.h>

/* Marlin (MRLN) */
int mtpMarlinGetInfo(int type, SceSize size, SceUInt32 offset, void *data, SceSize *outSize);
int mtpMarlinSetInfo(int type, SceUInt32 offset, const void *data, SceSize size);
void mtpMarlinInitialize(void);
int mtpExecMarlinActivation(int reserved, SceUInt32 arg, SceUInt32 *result2,
	SceUInt32 *result3);
int mtpExecMarlinDeactivation(int reserved, SceUInt32 arg, SceUInt32 *result2,
	SceUInt32 *result3);
int mtpExecMarlinLicenseMove(int reserved, const char *licenseId, SceSize length,
	SceUInt32 arg, int unused);
int mtpExecMarlinLicenseCopy(int reserved, const char *licenseId, SceSize length,
	SceUInt32 arg, int unused);
int mtpExecMarlinLicenseCheck(int reserved, const char *licenseId, SceSize length,
	SceUInt32 arg, SceUInt32 *state);
int mtpExecMarlinSharedInfoAcq(int reserved, const char *licenseId, SceSize length,
	SceUInt32 *info);

/* the storages (STRG) and the objects (OBJ) */
typedef struct MtpObjectData MtpObjectData;
MtpStorage *mtpStrgFindStorageByName(const char *name);
int mtpStrgVTCreateMtpHandles(MtpStorage *storage);
int mtpStrgVTDestroyMtpHandles(MtpStorage *storage);
int mtpStrgVTConvertMtpHandleToUniqueId(SceUInt32 handle, SceUInt32 *uniqueId);
int mtpStrgRegisterObject(const MtpObjectData *data, SceUInt32 *handle);
int mtpStrgGetMtpHandleByPath(const char *path, SceUInt32 *handle);
int mtpStrgRegisterDirectory(const char *path, SceUInt32 *handle);
int mtpStrgVTConvertUniqueIdToMtpHandle(SceUInt32 storageId, SceUInt32 uniqueId,
	SceUInt32 *handle);
/* a logical storage's id: its physical storage's index, then its own */
#define MTP_LOGICAL_STORAGE_ID(storage) \
	((storage)->index | (SceUInt32)(storage)->info->index << 16)

/* the logical storage of an object handle's (or object ID's) storage bits */
#define MTP_OBJECT_STORAGE_ID(storage) \
	((((storage) >> 27 & 3) + 1) | ((((storage) >> 29 & 3) + 1) << 16))
/* an object handle's bit for a directory (the parents GetObjectHandles takes) */
#define MTP_OBJECT_HANDLE_DIR		0x4000000

int mtpStrgGetLogicalStorage(SceUInt32 storageId, MtpLogicalStorage **storage);
SceUInt32 mtpStrgGetNumPhysicalStorages(void);
int mtpStrgGetPhysicalStorageByIndex(SceUInt32 index, const MtpStorageInfo **storage);

/* the storage IDs GetStorageIDs sends: num is the room in, the IDs out */
typedef struct MtpStorageIds {
	SceUInt32 num;
	SceUInt32 *ids;
} MtpStorageIds;

/* the bytes of IDs written, or an error */
int mtpStrgGetStorageIds(MtpStorageIds *ids);
int mtpStrgIsStorageValid(SceUInt32 storageId);
int mtpStrgIsStorageAvailable(SceUInt32 storageId);
int mtpStrgGetStorageInfo(SceUInt32 storageId, const MtpStorageDataset **dataset);
int mtpStrgIsObjectHandleValid(SceUInt32 handle);
/* whether a storage takes objects of 4 GiB and more */
int mtpStrgIsLargeObjectSupported(SceUInt32 storageId);
/* MTP_ACCESS_*: whether a storage may be written */
#define MTP_ACCESS_READ_WRITE		0
#define MTP_ACCESS_READ_ONLY		1
int mtpStrgGetAccessCapability(SceUInt32 storageId);
/* whether a format may go to *parent (which it may change) for the context's request */
int mtpStrgCheckParentHandle(SceUInt32 storageId, SceUInt32 format, SceUInt32 *parent,
	MtpContext *context);
/* whether a directory of that name may be made at the storage's root */
int mtpStrgIsRootDirAllowed(MtpLogicalStorage *storage, const char *name);
int mtpStrgSetupMtpRootFolder(MtpLogicalStorage *storage);
int mtpStrgRegister(MtpStorage *storage);
int mtpStrgUnregister(MtpStorage *storage);
int mtpStrgActivate(MtpStorage *storage);
int mtpStrgDeactivate(MtpStorage *storage, int arg);
int mtpStrgVTSetupMtpHandles(SceUInt32 storageId);
int mtpStrgVTUnsetupMtpHandles(SceUInt32 storageId);
int mtpStrgUpdateLogicalInfo(MtpLogicalStorage *storage);
/* the handle of a storage's root ("association") */
#define MTP_OBJECT_ROOT_HANDLE_OF(physical, logical) \
	((((SceUInt8)(logical) - 1) & 3) << 27 | (((SceUInt8)(physical) - 1) & 3) << 29 | 0x7FFFFFF)
#define MTP_OBJECT_ROOT_HANDLE(storageId) \
	MTP_OBJECT_ROOT_HANDLE_OF((storageId) >> 16, storageId)
int mtpObjRemoveObject(MtpLogicalStorage *storage, SceUInt32 uniqueId);
/* counts (buf NULL) or packs an ObjectPropList: count elements, size bytes */
int mtpObjPackObjectPropList(MtpLogicalStorage *storage, SceUInt32 uniqueId, SceUInt16 format,
	SceUInt16 code, SceUInt32 group, SceUInt32 depth, void *buf, SceUInt32 *count,
	SceUInt32 *size, int *send);
int mtpObjPackObjectPropValue(MtpLogicalStorage *storage, SceUInt32 uniqueId,
	SceUInt16 code, void *buf, int size);
int mtpObjSetObjectPropValue(MtpLogicalStorage *storage, SceUInt32 uniqueId,
	SceUInt16 code, const void *value, int writeBack);
int mtpObjPackObjectReferences(MtpLogicalStorage *storage, SceUInt32 uniqueId, void *buf,
	SceSize size);
int mtpObjSetObjectReferences(MtpLogicalStorage *storage, SceUInt32 uniqueId,
	const void *references, SceSize size);
int mtpObjRemoveFile(MtpLogicalStorage *storage, SceUInt32 parent, const char *name);
int mtpObjAddRegularObject(MtpLogicalStorage *storage, SceUInt32 parent, SceUInt16 format,
	SceUInt32 size, SceUInt32 imagePixWidth, SceUInt32 imagePixHeight, const char *name,
	const char *dateCreated, const char *dateModified, SceUInt32 *handle);
/* the path the PFS layer opens for path in storage */
int mtpStrgCreatePfsPath(MtpLogicalStorage *storage, const char *path, char *pfsPath,
	SceSize size, int create);
/*
 * an object's property in the database and the cache: the columns of tbl_mtpObject, in
 * the order mtpDBObjectPropCode2QueryType numbers them
 */
#define MTP_DB_QUERY_UID			0
#define MTP_DB_QUERY_PARENT_UID			1
#define MTP_DB_QUERY_OBJECT_FORMAT		2
#define MTP_DB_QUERY_PROTECTION_STATUS		3
#define MTP_DB_QUERY_OBJECT_SIZE		4
#define MTP_DB_QUERY_DATE_CREATED		5
#define MTP_DB_QUERY_DATE_MODIFIED		6
#define MTP_DB_QUERY_FULL_PATH_NAME		7
#define MTP_DB_QUERY_NON_CONSUMABLE		8
#define MTP_DB_QUERY_NAME			9
#define MTP_DB_QUERY_PUOID			10
#define MTP_DB_QUERY_ARTIST			11
#define MTP_DB_QUERY_GENRE			12
#define MTP_DB_QUERY_ALBUM			13
#define MTP_DB_QUERY_ALBUM_ARTIST		14
#define MTP_DB_QUERY_USE_COUNT			15
#define MTP_DB_QUERY_SAMPLE_RATE		16
#define MTP_DB_QUERY_AUDIO_WAVE_CODEC		17
#define MTP_DB_QUERY_AUDIO_BIT_RATE		18
#define MTP_DB_QUERY_ORIGINAL_RELEASE_DATE	19
#define MTP_DB_QUERY_DURATION			20
#define MTP_DB_QUERY_NUMBER_OF_CHANNELS		21
#define MTP_DB_QUERY_RATING			22
#define MTP_DB_QUERY_BITRATE_TYPE		23
#define MTP_DB_QUERY_DRM_STATUS			24
#define MTP_DB_QUERY_TRACK			25
#define MTP_DB_QUERY_SAMPLE_FORMAT		26
#define MTP_DB_QUERY_BUYFLAG			27
#define MTP_DB_QUERY_AUDIO_BIT_DEPTH		28
#define MTP_DB_QUERY_WIDTH			29
#define MTP_DB_QUERY_HEIGHT			30
#define MTP_DB_QUERY_OBJECT_DATA_BASIC		31	/* the whole ObjectDataBasic */
#define MTP_DB_QUERY_OBJECT_DATA_EXTENSION	32	/* the columns after it */
#define MTP_DB_QUERY_OBJECT_DATA		33	/* the whole row, an ObjectData */
/* the object property cache (CACH): a storage's most used rows of its database */
int mtpCacheInitialize(MtpLogicalStorage *storage, MtpCache **cache, SceUInt32 maxEntries);
int mtpCacheUninitialize(MtpCache **cache);
int mtpCacheGetObjectProperty(MtpCache *cache, SceUInt32 uniqueId, int prop, void *buf,
	SceSize size);
int mtpCacheSetObjectProperty(MtpCache *cache, SceUInt32 uniqueId, int prop, const void *buf,
	SceSize size);
int mtpCacheWriteBackDirtyCache(MtpCache *cache);
int mtpCacheInvalidate(MtpCache *cache, SceUInt32 uniqueId);

/* an object's row of tbl_mtpObject (MTP_DB_QUERY_OBJECT_DATA_BASIC) */
typedef struct ObjectDataBasic {
	SceUInt32 uid;
	SceUInt32 parentUid;		/* MTP_OBJ_NO_PARENT for a root object */
	SceUInt8 reserved[4];
	SceUInt16 objectFormat;
	SceUInt16 protectionStatus;
	SceUInt64 objectSize;
	SceUInt8 reserved1[8];
	SceDateTime dateCreated;
	SceDateTime dateModified;
	char fullPathName[1023];
	SceUInt8 nonConsumable;
	char name[256];
	SceUInt8 puoid[16];		/* mtpUtilMd5 of the path and the creation date */
} ObjectDataBasic;
#define MTP_OBJ_NO_PARENT		0xFFFFFFFF

/* the columns of tbl_mtpObject after ObjectDataBasic's (MTP_DB_QUERY_OBJECT_DATA_EXTENSION) */
typedef struct ObjectDataExtension {
	char artist[256];
	char genre[256];
	char album[256];
	char albumArtist[256];
	SceUInt32 useCount;
	SceUInt32 sampleRate;
	SceUInt32 audioWaveCodec;
	SceUInt32 audioBitRate;
	SceDateTime originalReleaseDate;
	SceUInt32 duration;
	SceUInt16 numberOfChannels;
	SceUInt16 rating;
	SceUInt16 bitrateType;
	SceUInt16 drmStatus;
	SceUInt16 track;		/* also the SampleFormat column's */
	SceUInt8 buyflag;
	SceUInt8 padding;
	SceUInt32 audioBitDepth;
	SceUInt8 reserved[4];
	SceUInt32 width;
	SceUInt32 height;
} ObjectDataExtension;

/* the whole row of tbl_mtpObject (MTP_DB_QUERY_OBJECT_DATA), what the cache keeps */
typedef struct ObjectData {
	ObjectDataBasic basic;
	ObjectDataExtension extension;
} ObjectData;

/* the object mtpObjAddObjectData adds and mtpObjGetObjectData reads */
struct __attribute__((packed)) MtpObjectData {
	SceUInt64 objectSize;
	SceUInt16 objectFormat;
	SceDateTime dateCreated;
	SceDateTime dateModified;
	char fullPathName[1023];
	char name[256];
	SceUInt32 width;
	SceUInt32 height;
};

/* the properties mtpObjUpdateObjectData writes back */
typedef struct __attribute__((packed)) MtpObjectUpdateData {
	SceUInt64 objectSize;
	SceUInt16 objectFormat;
	SceDateTime dateCreated;
	SceDateTime dateModified;
	char name[256];
	SceUInt32 width;
	SceUInt32 height;
} MtpObjectUpdateData;

/* the database (DB) */
int mtpDBRegisterObject(MtpDB *db, ObjectDataBasic *object);
int mtpDBUnregisterObject(MtpDB *db, SceUInt32 uniqueId);
int mtpDBGetObjectProperty(MtpDB *db, SceUInt32 uniqueId, int type, void *buf, SceSize size);
int mtpDBSetObjectProperty(MtpDB *db, SceUInt32 uniqueId, int type, const void *buf,
	SceSize size);
int mtpDBGetNumObjectsByType(MtpDB *db, int type, const void *value, SceUInt32 *num);
int mtpDBGetObjectUniqueIdsByType(MtpDB *db, int type, const void *value, SceUInt32 offset,
	SceUInt32 count, SceUInt32 *ids);
/* where a property sits in an ObjectDataBasic, and its size */
int mtpDBGetObjectPropertyOffset(int type, SceUInt32 *offset, SceUInt32 *size);
int mtpDBOpen(MtpDB *db, int create);
int mtpDBClose(MtpDB *db);
int mtpDBFormat(MtpDB *db, SceUInt32 maxObjects);
int mtpDBSetupMtpHandlesList(MtpDB *db, MtpObjmap *objmap);
int mtpDBInitGenericWay(MtpStorageInfo *info, SceUInt32 cacheSize);
int mtpDBUninitGenericWay(MtpStorageInfo *info);
int mtpDBLock(MtpDB *db);
int mtpDBUnlock(MtpDB *db);

/* what a database type does (the SQLite database's: dbSqliteOperation) */
typedef struct MtpDBOperation {
	int (*open)(MtpDB *db, int create);
	int (*close)(MtpDB *db);
	int (*format)(MtpDB *db, SceUInt32 maxObjects);
	int (*getUniqueIdInfo)(MtpDB *db, SceUInt32 *numObjects);
	int (*registerObject)(MtpDB *db, ObjectDataBasic *object);
	int (*unregisterObject)(MtpDB *db, SceUInt32 uniqueId);
	int (*getObjectProperty)(MtpDB *db, SceUInt32 uniqueId, int type, void *buf,
		SceSize size);
	int (*setObjectProperty)(MtpDB *db, SceUInt32 uniqueId, int type, const void *buf,
		SceSize size);
	void *reserved;
	int (*getNumObjects)(MtpDB *db, SceUInt32 parent, SceUInt32 *num);
	int (*getNumObjectsByType)(MtpDB *db, int type, const void *value, SceUInt32 *num);
	int (*getObjectUniqueIds)(MtpDB *db, SceUInt32 parent, SceUInt32 offset, SceUInt32 count,
		SceUInt32 *ids);
	int (*getObjectUniqueIdsByType)(MtpDB *db, int type, const void *value, SceUInt32 offset,
		SceUInt32 count, SceUInt32 *ids);
	int (*setupMtpHandlesList)(MtpDB *db, MtpObjmap *objmap);
} MtpDBOperation;
extern const MtpDBOperation dbSqliteOperation;

/* the SQLite calls (SQL): a value bound to a statement's parameter */
#define MTP_SQL_BIND_INTEGER	1
#define MTP_SQL_BIND_TEXT	2
typedef struct MtpSqlBind {
	int type;			/* MTP_SQL_BIND_* */
	union {
		SceInt64 integer;
		const void *data;	/* a text's bytes, copied when it is bound */
	} value;
	SceSize size;
} MtpSqlBind;
struct sqlite3_stmt;
/* an SQLite result code as an SCE_MTP_ERROR_*, 0 for SQLITE_OK */
int mtpSqlConvertError(int err);
int mtpSqlCreateStatement(struct sqlite3 *db, const char *sql, struct sqlite3_stmt **stmt);
int mtpSqlBindStatement(struct sqlite3_stmt *stmt, const MtpSqlBind *bind, int num);
/* sqls: the statements to run in turn, ending with NULL */
int mtpSqlExec(struct sqlite3 *db, const char *const *sqls);
/* a property's column value as the MTP value the host reads */
void mtpDBFixToObjectPropValue(int type, const void *src, void *dst, SceSize size);
/* an MTP value the host sent as the property's column value */
void mtpDBFixFromObjectPropValue(int type, const void *src, void *dst, SceSize size);
/* an object property code's MTP_DB_QUERY_* */
int mtpDBObjectPropCode2QueryType(SceUInt16 code);

/* the unique ids' handle map */
int mtpStrgVTValidateUniqueId(SceUInt32 storageId, SceUInt32 uniqueId, SceUInt16 format,
	SceUInt32 parent, SceUInt32 *handle);
int mtpStrgVTInvalidateUniqueId(SceUInt32 storageId, SceUInt32 uniqueId);
int mtpStrgVTGetParentMtpHandleFromUniqueId(SceUInt32 storageId, SceUInt32 uniqueId,
	SceUInt32 *parent);
/* the index bits of an object handle; all set for a storage's root */
#define MTP_OBJECT_HANDLE_INDEX_MASK	0x7FFFFFF

#define MTP_OBJECT_PROP_STORAGE_ID	0xDC01
#define MTP_OBJECT_PROP_PARENT_OBJECT	0xDC0B
/* the object property codes the database keeps (MTP 1.1, and Sony's 0xD901) */
#define MTP_OBJECT_PROP_OBJECT_FORMAT			0xDC02
#define MTP_OBJECT_PROP_PROTECTION_STATUS		0xDC03
#define MTP_OBJECT_PROP_OBJECT_FILE_NAME		0xDC07
#define MTP_OBJECT_PROP_DATE_CREATED			0xDC08
#define MTP_OBJECT_PROP_DATE_MODIFIED			0xDC09
#define MTP_OBJECT_PROP_PERSISTENT_UID			0xDC41
#define MTP_OBJECT_PROP_NAME				0xDC44
#define MTP_OBJECT_PROP_ARTIST				0xDC46
#define MTP_OBJECT_PROP_NON_CONSUMABLE			0xDC4F
#define MTP_OBJECT_PROP_REPRESENTATIVE_SAMPLE_FORMAT	0xDC81
#define MTP_OBJECT_PROP_REPRESENTATIVE_SAMPLE_HEIGHT	0xDC83
#define MTP_OBJECT_PROP_REPRESENTATIVE_SAMPLE_WIDTH	0xDC84
#define MTP_OBJECT_PROP_WIDTH				0xDC87
#define MTP_OBJECT_PROP_HEIGHT				0xDC88
#define MTP_OBJECT_PROP_DURATION			0xDC89
#define MTP_OBJECT_PROP_RATING				0xDC8A
#define MTP_OBJECT_PROP_TRACK				0xDC8B
#define MTP_OBJECT_PROP_GENRE				0xDC8C
#define MTP_OBJECT_PROP_USE_COUNT			0xDC91
#define MTP_OBJECT_PROP_ORIGINAL_RELEASE_DATE		0xDC99
#define MTP_OBJECT_PROP_ALBUM_NAME			0xDC9A
#define MTP_OBJECT_PROP_ALBUM_ARTIST			0xDC9B
#define MTP_OBJECT_PROP_DRM_STATUS			0xDC9D
#define MTP_OBJECT_PROP_BITRATE_TYPE			0xDE92
#define MTP_OBJECT_PROP_SAMPLE_RATE			0xDE93
#define MTP_OBJECT_PROP_NUMBER_OF_CHANNELS		0xDE94
#define MTP_OBJECT_PROP_AUDIO_BIT_DEPTH			0xDE95
#define MTP_OBJECT_PROP_AUDIO_WAVE_CODEC		0xDE99
#define MTP_OBJECT_PROP_AUDIO_BITRATE			0xDE9A
#define MTP_OBJECT_PROP_SCE_BUYFLAG			0xD901
#define MTP_FORMAT_UNDEFINED		0x3000
#define MTP_FORMAT_ABSTRACT_AUDIO_ALBUM		0xBA03
#define MTP_FORMAT_ABSTRACT_AV_PLAYLIST		0xBA05
#define MTP_EVENT_OBJECT_PROP_CHANGED		0xC801
#define MTP_OBJECT_PROP_OBJECT_SIZE		0xDC04
/* the room for a property value that may be a representative sample's picture */
#define MTP_OBJECT_PROP_VALUE_SIZE_LARGE	0x10000
int mtpStrgVTGetNumUniqueIds(SceUInt32 storageId, SceUInt32 parent, SceUInt32 *num);
int mtpStrgVTGetUniqueIds(SceUInt32 storageId, SceUInt32 parent, SceUInt32 offset,
	SceUInt32 count, SceUInt32 *ids);

int mtpObjAddRootObject(MtpLogicalStorage *storage);
int mtpObjAddDirectoryObject(MtpLogicalStorage *storage, const char *name,
	SceUInt32 *uniqueId);
int mtpObjBuildFullPathName(MtpLogicalStorage *storage, SceUInt32 parent, const char *name,
	char *path, int withMountPoint);
int mtpObjAddObjectData(MtpLogicalStorage *storage, SceUInt32 parent,
	const MtpObjectData *data, const char *path, SceUInt32 *handle);

/* mtp_operation.cpp */
/* a data phase's transfer buffers: the heap's, or the large buffer's while it is free */
#define MTP_CMD_XFER_SIZE		0x10000
#define MTP_CMD_XFER_SIZE_LARGE		0x100000
#define MTP_CMD_XFER_ALIGN		0x40

/* what a received file's progress reports, and how often (in percent) */
#define MTP_CORE_PUBLIC_STATUS_RECEIVING	6
#define MTP_CMD_PROGRESS_STEP		10
int mtpCmdUtilReceiveFileData(const MtpOperation *operation, MtpOperation *response,
	const char *path, SceSize size, int create, SceUInt64 *received, int *responseCode,
	MtpContext *context);
int mtpObjPackObjectInfo(MtpLogicalStorage *storage, SceUInt32 uniqueId, void *buf,
	SceSize size);
int mtpStrgGetNumObjects(SceUInt32 storageId, SceUInt32 parent, SceUInt32 *num);
/* sends the handles itself; *send is cleared when that fails */
int mtpStrgGetMtpHandles(SceUInt32 storageId, SceUInt32 parent, SceUInt32 num, int *send);
int mtpObjDeleteObjectData(MtpLogicalStorage *storage, SceUInt32 uniqueId);
int mtpObjGetObjectData(MtpLogicalStorage *storage, SceUInt32 uniqueId, MtpObjectData *data);
int mtpObjUpdateObjectData(MtpLogicalStorage *storage, SceUInt32 uniqueId,
	const MtpObjectUpdateData *data);
int mtpObjNotifyProprtyUpdate(SceUInt32 uniqueId, SceUInt32 storageId);

/* the background copy (BGCP) */
int mtpBgCopyGetTaskInfo(SceUInt32 *id, int *type, void *path);

/* the network RPC, the search, the handover, the pause and resume of a transfer */
/* a network operation: what it sends, which way its data goes, and the host's answer */
#define MTP_NET_OPERATION_SEND	1	/* the caller's data phase sends */
#define MTP_NET_OPERATION_RECV	2	/* the caller's data phase receives */
typedef struct MtpNetOperation {
	void *sendData;
	SceSize sendSize;
	int type;		/* 0, MTP_NET_OPERATION_SEND or MTP_NET_OPERATION_RECV */
	void *recvData;
	SceSize recvSize;
} MtpNetOperation;

/* where a search's caller gets how many objects it found, and the search */
typedef struct MtpSearchResult {
	SceUInt32 *numFound;
	SceUInt32 *searchId;
} MtpSearchResult;

/* where a caller gets the host's answer when the request ends */
typedef struct MtpNetOperationResult {
	void *buffer;
	SceSize *size;
} MtpNetOperationResult;

int mtpContextRpcNetOperation(MtpContext *context, int type, const void *data, SceSize size);
int mtpContextRpcNetOperationAbort(MtpContext *context, int operation, int arg, int id);
int mtpContextSearchObject(MtpContext *context, const char *keyword, SceUInt32 type,
	SceUInt32 flags);
int mtpContextHandover(MtpContext *context, const void *info);
int mtpContextPauseExportObject(MtpContext *context, SceUInt32 contextId);
int mtpContextResumeExportObject(MtpContext *context);
int mtpContextResumeImportObject(MtpContext *context);

/* the background copy service's entry points, given by the shell (BGCP) */
extern SceLibMtpHostFuncs g_bgCopyServiceFuncs;

/* a copy the background copy service is asked to run */
typedef struct MtpBgCopyTask {
	int type;			/* 1..4: the kind of content */
	SceUInt32 reserved;
	SceUInt64 size;			/* its size in bytes */
	SceUInt32 reserved2;
	const char *name;
} MtpBgCopyTask;

int mtpBgCopyRequestTask(const MtpBgCopyTask *task, void *path);
int mtpBgCopyEndTask(void);
int mtpBgCopySetTaskInfo(const void *status);
int mtpBgCopyWriteData(const void *data, SceSize size, int index);
int mtpBgCopyReadData(void *data, SceUInt32 offset, SceSize size, int index, SceSize *read);
void mtpCmdMarlinFreeBuffers(void);

#ifdef __cplusplus
}

#include <common/shared_ptr.h>
#include <memory/heap_allocator.h>

/* the utilities' heap, which the task queue allocates its events from */
extern "C" paf::memory::HeapAllocator *g_mtpUtilHeap;

/* paf's allocation from a heap (ScePafStdc D00387E7), NULL when it has no memory */
void *operator new(size_t size, paf::memory::HeapAllocator *heap) throw();

/* forget the host's capabilityInfo; a connection that ended keeps it unless forced */
void mtpCapabilityInfoClear(bool force);

namespace bXCe {
namespace Sax {
class Parser;
class DocumentHandler;
}
}

/* the request running and the ones beside it (mtp_context_manager.cpp) */
class MtpContextManager {
public:
	MtpContextManager();
	virtual ~MtpContextManager();
	int initialize();
	int finalize();
	int lock();
	int unlock();
	void registerContext(paf::common::SharedPtr<MtpContext> context);
	void setCanceling(int canceling);
	int isCanceling();

	SceUID m_mutex;
	int m_canceling;
	paf::common::SharedPtr<MtpContext> m_current;
	paf::common::SharedPtr<MtpContext> m_cancel;
	paf::common::SharedPtr<MtpContext> m_terminate;
	paf::common::SharedPtr<MtpContext> m_pause;
};

/* hand a context to the context manager, which keeps it while the request runs */
void mtpContextRegister(paf::common::SharedPtr<MtpContext> context);

/* the background copy ("BGCP", bgcopy_service_manager.cpp) */
#include <std/string>
#include <std/map>
#include <std/vector>

/* a task's progress, as the background copy service reports it */
struct MtpBgCopyTaskStatus {
	int result;
	paf::string name;
	SceUInt32 progress;		/* in percent */
	int index;			/* the object being copied, from 0 */
	SceUInt32 total;		/* how many objects */
	int remaining;			/* seconds left, -1: not known */

	MtpBgCopyTaskStatus() : result(0), name(), progress(0), index(0), total(0), remaining(0)
	{
	}
};

/* an import the background copy service runs */
struct BgCopyImportContext {
	SceUInt32 id;			/* the service's ID for it */
	SceUInt32 reserved;
	void *pauseSource;		/* where a paused export resumes from */
	void *pauseTarget;
	int type;			/* MtpBgCopyTask's type */
	paf::string path;		/* where the copied data is kept */
	MtpBgCopyTaskStatus status;
	int restart;			/* it is started again after a disconnect */

	BgCopyImportContext() : id(0), reserved(0), pauseSource(NULL), pauseTarget(NULL), type(0),
		path((const char *)NULL, 0), restart(0)
	{
		status.name = "";
	}

	BgCopyImportContext(const BgCopyImportContext &src)
		: id(src.id), reserved(src.reserved), pauseSource(src.pauseSource),
		  pauseTarget(src.pauseTarget), type(src.type), restart(src.restart)
	{
		path = src.path;
		status.name = src.status.name;
		status.index = src.status.index;
		status.total = src.status.total;
		status.progress = src.status.progress;
		status.remaining = src.status.remaining;
		status.result = src.status.result;
	}
};

/* the background copy service's imports, by their ID; one of them is current */
class BgCopySrvMgr {
public:
	BgCopySrvMgr();
	~BgCopySrvMgr();
	int findContext(SceUInt32 id, BgCopyImportContext **context);
	int addContext(const BgCopyImportContext &context);
	int removeContext(SceUInt32 id);
	void setCurrentId(SceUInt32 id);
	SceUInt32 getCurrentId();
	int setPauseSource(void *data);
	int setPauseTarget(void *data);
	int setPath(paf::string path);
	int setStatus(const MtpBgCopyTaskStatus *status);
	int setRestart(bool restart);
	void *getPauseSource();
	void *getPauseTarget();
	int getPath(paf::string *path);
	int getType();
	int getStatus(MtpBgCopyTaskStatus *status);
	int clearStatus();
	int forEachRestart(int (*func)(SceUInt32 id));

	static SceUInt32 s_currentId;

	paf::map<SceUInt32, BgCopyImportContext *> m_contexts;
};

extern BgCopySrvMgr g_bgCopySrvMgr;

/* a task's progress as the service is told it (SceLibMtp_E3EBABB4, SceLibMtp_BF311CC8) */
struct MtpBgCopyProgress {
	SceUInt32 id;
	paf::string name;
	SceUInt32 progress;		/* in percent */
	int number;			/* the object being copied, from 1 */
	SceUInt32 total;
	int remaining;			/* seconds left */
	int reserved;
};

/* the service's side of a copy run by SceLibMtp_BF311CC8 */
class MtpBgCopyHost {
public:
	virtual ~MtpBgCopyHost() {}
	virtual int isRequested() = 0;
};

/* what SceLibMtp_BF311CC8 is given */
struct MtpBgCopyRunParam {
	SceUInt32 id;
	void *arg;
	void (*notify)(void *arg, const MtpBgCopyProgress *progress);
	int (*accept)(void);
	MtpBgCopyHost *host;
	SceUInt32 reserved[3];
	paf::string path;
};

/* starts the background copy service when it does not run (bgcopy_mtp_launcher.cpp) */
class BgCopyMtpLauncher {
public:
	BgCopyMtpLauncher();
	virtual ~BgCopyMtpLauncher();
	int start();
	bool isMtpTaskInProgress();
	static int execute(SceSize args, void *argp);

	/* nothing reads these: they are made, cleared and destroyed only */
	int m_state;
	paf::string m_sourceName;
	paf::vector<SceUInt8> m_sourceData;
	SceUInt32 m_sourceReserved;
	int m_sourceFlags;
	paf::string m_targetName;
	SceUInt32 m_targetInfo[4];
	SceUInt64 m_targetSize;
	int m_targetFlags;
	paf::vector<SceUInt8> m_targetData;
	SceUInt32 m_targetReserved;
	int m_targetKind;
	paf::string m_titleName;
	SceUInt64 m_titleSize;
	int m_titleFlags;
	SceUInt32 m_titleReserved;
	int m_titleKind;
	paf::string m_dirName;
	SceUInt64 m_dirSize;
	int m_dirFlags;
	int m_dirKind;
	SceUID m_thread;
	paf::vector<paf::string> m_files;
	int m_fileIndex;
	SceUID m_serviceIds[3];		/* -1: none */
};

extern BgCopyMtpLauncher g_bgCopyMtpLauncher;

/* mtp_bgcopy.cpp */
void mtpBgCopySetCurrentTask(SceUInt32 id);
int mtpBgCopyIsIdle(void);
int mtpBgCopySetRestart(int restart);
void *mtpExportGetPauseSource(void);
void *mtpExportGetPauseTarget(void);
void mtpBgCopySetTaskPath(const paf::string *path);
int mtpBgCopyGetTaskStatus(MtpBgCopyTaskStatus *status);
int mtpBgCopyClearTaskStatus(void);
int mtpBgCopyNotifyService(int arg1, int arg2);
int mtpBgCopyCancelTask(SceUInt32 id);
int mtpBgCopyIsServiceIdle(void);

/* bgcopy_service_manager.cpp */
struct SceAppMgrAppStatus;
int mtpBgCopyLaunchService(int arg1, int arg2);	/* both unused, -1 */
int mtpBgCopyGetServiceStatus(struct SceAppMgrAppStatus *status);

/* a parser of the initiatorInfo a host sends (mtp_init.cpp) */
class InitiatorInfoParser {
public:
	InitiatorInfoParser() : m_parser(NULL), m_handler(NULL) {}
	~InitiatorInfoParser();
	int Initialize();
	int Terminate();
	int Parse(const char *data, int length, bool isFinal);

	bXCe::Sax::Parser *m_parser;
	bXCe::Sax::DocumentHandler *m_handler;
};

/* a parser of the capabilityInfo a host sends (mtp_capability.cpp) */
class CapabilityInfoHandler;
class CapabilityInfoParser {
public:
	CapabilityInfoParser() : m_parser(NULL), m_handler(NULL) {}
	~CapabilityInfoParser();
	int Initialize();
	int Terminate();
	int Parse(const char *data, int length, bool isFinal);

	bXCe::Sax::Parser *m_parser;
	CapabilityInfoHandler *m_handler;
};
#endif

#endif
