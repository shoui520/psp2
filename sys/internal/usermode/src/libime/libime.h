/*
 * SceIme (vs0:sys/external/libime.suprx): the IME of the system software's own applications.
 * The SceIme API (sceImeOpen, ...) sits on a layer, SceImeVsh, that talks to the IME service
 * ("SceImeService") over IPMI and hands the events it reports to the application's handler.
 *
 * No debug record names its files, types or fields; the names are ours, taken from what the
 * code does with them.
 */

#ifndef _LIBIME_H
#define _LIBIME_H

#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <libime/libime_api.h>
#include <libime/libime_error.h>

/* SceImeVsh's own errors */
#define IME_VSH_ERROR_NOT_OPENED		SCE_IME_ERROR_NOT_OPENED
#define IME_VSH_ERROR_INVALID_HANDLE		((int)0x801007B9)
#define IME_VSH_ERROR_BUSY			((int)0x801007B2)
#define IME_VSH_ERROR_SERVICE_REFUSED		((int)0x801007B1)
#define IME_VSH_ERROR_BAD_REPLY			((int)0x801007B7)

/* what the IPMI client reports */
#define IME_IPMI_ERROR_REFUSED			((int)0x80020580)
#define IME_IPMI_ERROR_BUSY			((int)0x80020581)
#define IME_IPMI_ERROR_NOT_READY		((int)0x80020501)
#define IME_IPMI_ERROR_UNAVAILABLE		((int)0x80020583)

/* the text helpers' errors */
#define IME_TEXT_ERROR_RANGE			((int)0x80000001)
#define IME_TEXT_ERROR_POINTER			((int)0x80000002)

/* the service's methods (the first argument of an IPMI call) and its command numbers */
#define IME_CMD_OPEN				3
#define IME_CMD_POLL				4
#define IME_CMD_CLOSE				2
#define IME_CMD_UPDATE			5

/* the events the service reports (id of an ImeVshEvent) */
#define IME_VSH_EVENT_OPEN			0
#define IME_VSH_EVENT_TEXT			0x100
#define IME_VSH_EVENT_INSERT			0x101
#define IME_VSH_EVENT_INSERT_SPACE		0x102
#define IME_VSH_EVENT_BACKSPACE			0x103
#define IME_VSH_EVENT_CLEAR_BEFORE		0x104
#define IME_VSH_EVENT_CLEAR			0x105
#define IME_VSH_EVENT_REPLACE			0x106
#define IME_VSH_EVENT_CAPITALIZE		0x114
#define IME_VSH_EVENT_ENTER			0x200

/* the sizes of the IME's buffers */
#define IME_PREEDIT_LENGTH			30
#define IME_WORK_SIZE				0x5000
#define IME_NUM_EVENTS				10

/* the service's IPMI client (SceIpmi's IPMI::Client: its virtual table, in declaration order) */
typedef struct ImeIpmiClient ImeIpmiClient;

typedef struct ImeIpmiClientVtbl {
	int (*destroy)(ImeIpmiClient *pClient);
	int (*connect)(ImeIpmiClient *pClient, const void *pParam, SceSize paramSize, int *pResult);
	int (*disconnect)(ImeIpmiClient *pClient);
	int (*terminateConnection)(ImeIpmiClient *pClient);
	int (*invokeSyncMethod)(ImeIpmiClient *pClient, SceUInt32 methodId, const void *pIn, SceSize inSize,
		int *pResult, void *pOut, SceSize *pOutSize, SceSize outSize);
	int (*invokeSyncMethodList)(void);
	int (*invokeAsyncMethod)(ImeIpmiClient *pClient, SceUInt32 methodId, const void *pIn, SceSize inSize,
		int *pRequestId, const void *pEventFlag);
	int (*invokeAsyncMethodList)(void);
	int (*tryGetResult)(ImeIpmiClient *pClient, int requestId, int *pResult, void *pOut, SceSize *pOutSize,
		SceSize outSize);
} ImeIpmiClientVtbl;

struct ImeIpmiClient {
	const ImeIpmiClientVtbl *pVtbl;
};

/* IPMI::Client::Config as the library fills it (SceIpmi has no name for it here) */
typedef struct ImeIpmiConfig {
	char serviceName[16];
	SceUInt32 reserved;
	SceUInt32 syncCallCount;
	SceSize maxSyncCallInSize;
	SceSize maxSyncCallOutSize;
	SceUInt32 asyncCallCount;
	SceSize maxAsyncCallInSize;
	SceSize maxAsyncCallOutSize;
	SceUInt32 eventFlagCount;
	SceUInt32 msgQueueCount;
	SceSize msgQueueBufferSize[32];
	int budget;
	int openLimit;
} ImeIpmiConfig;

/* what the layer calls the application's handler with (the event as it is, 36 bytes) */
typedef struct ImeVshCallParam {
	SceUInt32 value[8];
} ImeVshCallParam;

typedef struct ImeVshCall {
	SceUInt32 id;
	ImeVshCallParam param;
} ImeVshCall;

typedef void (*ImeVshHandler)(void *arg, const ImeVshCall *pCall);

/* what sceAppMgrGetStatusByName reports of the caller (the layer reads one field) */
typedef struct ImeAppStatus {
	SceUInt8 reserved[56];
	SceInt32 value;
} ImeAppStatus;

/* SceImeParam as the layer takes it (64 bytes) */
typedef struct ImeVshParam {
	SceUInt32 size;
	SceUInt32 inputMethod;
	SceUInt64 supportedLanguages;
	SceUInt32 languagesForced;
	SceUInt32 type;
	SceUInt32 option;
	void *arg;
	ImeVshHandler handler;
	SceImeTextFilter filter;
	void *work;
	SceUInt8 enterLabel;
	SceUInt8 flag;
	SceUInt8 reserved1[2];
	SceUInt32 sdkVersion;
	SceUInt8 reserved2[12];
} ImeVshParam;

/* what the service is sent to open (40 bytes) */
typedef struct ImeOpenRequest {
	SceUInt64 supportedLanguages;
	SceUInt32 languagesForced;
	SceUInt32 type;
	SceUInt32 option;
	SceUInt32 sessionId;
	SceUInt8 enterLabel;
	SceUInt8 flag;
	SceUInt8 reserved[2];
	SceUInt32 sdkVersion;
	SceUInt32 flags;
	SceUInt32 reserved2;
} ImeOpenRequest;

/* what the service is sent to update (40 bytes): the pending changes and their values */
typedef struct ImeUpdateRequest {
	SceUInt32 flags;			/* 1: kind, 2: capitalize, 4: caret index, 8: caret geometry, 0x10: preedit geometry */
	SceUInt32 kind;
	SceUInt32 capitalize;
	SceUInt32 caretIndex;
	SceUInt32 caretX;
	SceUInt32 caretY;
	SceUInt32 caretHeight;
	SceUInt32 preeditX;
	SceUInt32 preeditY;
	SceUInt32 preeditHeight;
} ImeUpdateRequest;

/* one event the service reports (188 bytes) */
typedef struct ImeVshEvent {
	SceUInt32 id;
	SceUInt32 param[46];
} ImeVshEvent;

/* the connection to the service: what sceImeOpen keeps in the layer (2104 bytes) */
typedef struct ImeVshHandle {
	SceUInt8 openEventSeen;
	SceUInt8 skipToOpenEvent;		/* the events before the open event are dropped */
	SceUInt8 spacePending;
	SceUInt8 afterSpace;
	ImeUpdateRequest update;		/* the changes the next update sends */
	SceUInt8 dirty;
	SceUInt8 padding1[3];
	void *pArg;				/* the handler's argument */
	ImeVshHandler handler;
	SceImeTextFilter filter;
	struct ImeVshHandle *pSelf;
	SceInt32 numEvents;
	ImeVshEvent events[IME_NUM_EVENTS];
	SceUInt8 padding2[4];
	ImeVshParam param;
	SceUInt32 preeditLength;
	ImeIpmiClient *pClient;
	int requestId;
	SceUInt8 requestPending;
	SceUInt8 padding3;
	SceWChar16 preedit[IME_PREEDIT_LENGTH + 1];
	SceUInt32 sessionId;
	SceUInt8 buttonIntercepted;
	SceUInt8 padding5[7];
} ImeVshHandle;

/* the library's state, one object (the SceIme API's view of the open IME) */
typedef struct ImeState {
	SceUInt32 sdkVersion;
	ImeVshHandle *pHandle;
	void (*handler)(void *arg, const SceImeEvent *pEvent);
	SceUInt8 opened;
	SceUInt8 firstEvent;
	SceUInt8 padding1[2];
	SceWChar16 *pShadowText;
	SceWChar16 *pText;
	SceUInt32 maxTextLength;
	SceUInt32 textBufferLength;
	SceUInt32 caretIndex;
	SceUInt32 preeditIndex;
	SceUInt32 preeditLength;
	void *pArg;
	SceWChar16 *pWork;
	SceImeTextFilter filter;
	SceUInt32 reserved;
	SceInt32 composeState;
	SceUInt8 reserved2;
	SceUInt8 updated;
	SceUInt8 padding2[2];
} ImeState;

#endif /* _LIBIME_H */
