/*
 * mtp_server_internal.h: internal types, IPMI method IDs, worker, session manager,
 * storage/USB handlers, MtpServer, and MtpMarlinService for SceShell's MTP subsystem.
 */
#ifndef SHELL_MTP_SERVER_INTERNAL_H
#define SHELL_MTP_SERVER_INTERNAL_H

#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <std/list>
#include <std/string.h>
#include <std/stdlib.h>
#include <common/shared_ptr.h>
#include <datetime/datetime.h>
#include <job/job.h>
#include <kernel/threadmgr.h>
#include <kernel/modulemgr.h>
#include <ipmi/server.h>
#include <ipmi/ipmi_error.h>
#include "../../../../common/src/mtp/libmtp/libmtp.h"
#include "../../../shell.h"

/* MTP service error codes (0x805400xx / 0x805401xx / 0x805402xx) */
#define MTP_SVC_ERROR_ALREADY_INITIALIZED	(-2141978622)	/* 0x80540002 */
#define MTP_SVC_ERROR_NOT_INITIALIZED		(-2141978621)	/* 0x80540003 */
#define MTP_SVC_ERROR_NO_MEM_POOL		(-2141978617)	/* 0x80540007 */
#define MTP_SVC_ERROR_CONTEXT_FINISHED		(-2141978604)	/* 0x80540014 */

#define MTP_SVC_ERROR_NOT_SUPPORTED		(-2141978369)	/* 0x805400FF */
#define MTP_SVC_ERROR_NO_MEMORY			(-2141978367)	/* 0x80540101 */
#define MTP_SVC_ERROR_INVALID_ARGUMENT		(-2141978366)	/* 0x80540102 */
#define MTP_SVC_ERROR_INVALID_STATE		(-2141978365)	/* 0x80540103 */
#define MTP_SVC_ERROR_MAX_CLIENTS		(-2141978364)	/* 0x80540104 */
#define MTP_SVC_ERROR_NOT_FOUND			(-2141978363)	/* 0x80540105 */
#define MTP_SVC_ERROR_ALREADY_ACTIVE		(-2141978362)	/* 0x80540106 */
#define MTP_SVC_ERROR_CONSOLE_INFO_CANCELED	(-2141978360)	/* 0x80540108 */
#define MTP_SVC_ERROR_CONSOLE_INFO_TIMEOUT	(-2141978359)	/* 0x80540109 */
#define MTP_SVC_ERROR_INTERNAL			(-2141978113)	/* 0x805401FF */

#define MTP_KERNEL_ERROR_WAIT_TIMEOUT		(-2147319803)	/* 0x80028005 */
#define MTP_KERNEL_ERROR_WAIT_CANCEL		(-2147319801)	/* 0x80028007 */
#define MTP_UDCD_ERROR_WAIT_TIMEOUT		(-2145112052)	/* 0x8024300C */
#define MTP_APPMGR_ERROR_NO_ACTIVE_APP		(-2139086830)	/* 0x80802012 */

#define MTP_MAX_CLIENTS				8
#define MTP_SESSION_PREPARING			((IPMI::Session *)(uintptr_t)0xDEADBEEFu)
#define MTP_MSG_SEND_TIMEOUT_US			500000u
#define MTP_STATUS_EVENT_MASK			831u		/* 0x33F */
#define MTP_USB_CDROM_MODE			1
#define MTP_USB_NORMAL_MODE			0
#define MTP_IF_TYPE_USB				1
#define MTP_IF_TYPE_TCPIP			2
#define MTP_NET_HOST_TYPE_PS3			4
#define MTP_PLATFORM_TYPE_PS			10

/* Session flag bits tracked in MtpSessionSlot::flags */
#define MTP_SESSION_FLAG_PENDING_DISCONNECT	0x01u
#define MTP_SESSION_FLAG_PENDING_RESET		0x02u
#define MTP_SESSION_FLAG_HOST_DISCOVERY		0x04u
#define MTP_SESSION_FLAG_NETWORK_IMPOSE		0x08u
#define MTP_SESSION_FLAG_NET_REGISTER		0x10u
#define MTP_SESSION_FLAG_DISABLE_NOTIFY		0x20u

/* Service event flag bits (SceMtpService) */
#define MTP_EVF_AVAILABLE			0x01u
#define MTP_EVF_CONNECTED			0x02u
#define MTP_EVF_USB_SWITCH			0x04u

/* Console info event flag bits (SceMtpConsoleInfo) */
#define MTP_CONSOLE_EVF_IDLE			0x01u
#define MTP_CONSOLE_EVF_REQUEST_READY		0x02u
#define MTP_CONSOLE_EVF_DONE			0x03u

/* Worker event flag bits (SceMtpServiceWorker) */
#define MTP_WORKER_EVF_TERMINATE		0x01u
#define MTP_WORKER_EVF_JOB_READY		0x02u

/* Worker running flag bits (SceMtpServiceWorkerRunning) */
#define MTP_WORKER_RUN_EVF_STARTED		0x01u
#define MTP_WORKER_RUN_EVF_TERMINATED		0x02u

/* Broadcast event IDs sent over IPMI session message queue 0 */
#define MTP_MSG_EVENT_STATUS			0
#define MTP_MSG_EVENT_NET_REGISTER		1
#define MTP_MSG_EVENT_BGCOPY			2

/* MTP host event codes from libmtp callback */
#define MTP_HOST_EVENT_AVAILABLE		1
#define MTP_HOST_EVENT_USB_DETECTED		2
#define MTP_HOST_EVENT_DISCONNECTED		4
#define MTP_HOST_EVENT_CONNECTED		8
#define MTP_HOST_EVENT_BUSY			16
#define MTP_HOST_EVENT_READY			32
#define MTP_HOST_EVENT_STORAGE_RESET		256
#define MTP_HOST_EVENT_HOST_RESET		512

/* Process activity state for foreground/background synchronization */
enum MtpProcessActivityState {
	MTP_PROC_ACTIVITY_NONE         = 0,
	MTP_PROC_ACTIVITY_ACTIVATING   = 1,
	MTP_PROC_ACTIVITY_ACTIVATED    = 2,
	MTP_PROC_ACTIVITY_DEACTIVATING = 3,
	MTP_PROC_ACTIVITY_DEACTIVATED  = 4
};

/* IPMI method IDs for SceMtpService (base 0x12340000 = 305397760) */
enum MtpServiceIpmiMethod {
	MTP_METHOD_GET_SESSION_ID			= 0x12340000,
	MTP_METHOD_DISABLE_NOTIFY			= 0x12340001,
	MTP_METHOD_BEGIN_GET_NUM_OF_OBJECT		= 0x12340002,
	MTP_METHOD_END_GET_NUM_OF_OBJECT		= 0x12340003,
	MTP_METHOD_BEGIN_GET_OBJECT_METADATA		= 0x12340004,
	MTP_METHOD_END_GET_OBJECT_METADATA		= 0x12340005,
	MTP_METHOD_BEGIN_IMPORT_OBJECT			= 0x12340006,
	MTP_METHOD_END_IMPORT_OBJECT			= 0x12340007,
	MTP_METHOD_BEGIN_EXPORT_OBJECT_WITH_CHECK	= 0x12340008,
	MTP_METHOD_END_EXPORT_OBJECT			= 0x12340009,
	MTP_METHOD_BEGIN_CREATE_HOST_DIR		= 0x1234000A,
	MTP_METHOD_END_CREATE_HOST_DIR			= 0x1234000B,
	MTP_METHOD_BEGIN_RENAME_HOST_DIR		= 0x1234000C,
	MTP_METHOD_END_RENAME_HOST_DIR			= 0x1234000D,
	MTP_METHOD_BEGIN_MOVE_HOST_DIR			= 0x1234000E,
	MTP_METHOD_END_MOVE_HOST_DIR			= 0x1234000F,
	MTP_METHOD_BEGIN_CREATE_HOST_FILE		= 0x12340010,
	MTP_METHOD_END_CREATE_HOST_FILE			= 0x12340011,
	MTP_METHOD_BEGIN_GET_OBJECT_THUMBNAIL		= 0x12340012,
	MTP_METHOD_END_GET_OBJECT_THUMBNAIL		= 0x12340013,
	MTP_METHOD_BEGIN_GET_OBJECT_STATUS2		= 0x12340014,
	MTP_METHOD_END_GET_OBJECT_STATUS		= 0x12340015,
	MTP_METHOD_BEGIN_DELETE_OBJECT			= 0x12340016,
	MTP_METHOD_END_DELETE_OBJECT			= 0x12340017,
	MTP_METHOD_BEGIN_HTTP_GET_DATA_WITH_URL		= 0x12340018,
	MTP_METHOD_END_HTTP_GET_DATA_WITH_URL		= 0x12340019,
	MTP_METHOD_BEGIN_HTTP_GET_PROPERTY_WITH_URL	= 0x1234001A,
	MTP_METHOD_END_HTTP_GET_PROPERTY_WITH_URL	= 0x1234001B,
	MTP_METHOD_BEGIN_GET_SYSTEM_SETTING		= 0x1234001C,
	MTP_METHOD_END_GET_SYSTEM_SETTING		= 0x1234001D,
	MTP_METHOD_BEGIN_SET_SYSTEM_SETTING		= 0x1234001E,
	MTP_METHOD_END_SET_SYSTEM_SETTING		= 0x1234001F,
	MTP_METHOD_BEGIN_GET_NP_ACCOUNT_INFO		= 0x12340020,
	MTP_METHOD_END_GET_NP_ACCOUNT_INFO		= 0x12340021,
	MTP_METHOD_BEGIN_READ_OBJECT_WITH_OFFSET	= 0x12340022,
	MTP_METHOD_END_READ_OBJECT_WITH_OFFSET		= 0x12340023,
	MTP_METHOD_BEGIN_WRITE_OBJECT_WITH_OFFSET	= 0x12340024,
	MTP_METHOD_END_WRITE_OBJECT_WITH_OFFSET		= 0x12340025,
	MTP_METHOD_BEGIN_GET_HOST_STORAGE_SIZE		= 0x12340026,
	MTP_METHOD_END_GET_HOST_STORAGE_SIZE		= 0x12340027,
	MTP_METHOD_BEGIN_CHECK_SAME_OBJECT_EXIST	= 0x12340028,
	MTP_METHOD_END_CHECK_SAME_OBJECT_EXIST		= 0x12340029,
	MTP_METHOD_BEGIN_GET_TOTAL_OBJECT_SIZE		= 0x1234002A,
	MTP_METHOD_END_GET_TOTAL_OBJECT_SIZE		= 0x1234002B,
	MTP_METHOD_BEGIN_GET_SPECIFIED_METADATA		= 0x1234002C,
	MTP_METHOD_END_GET_SPECIFIED_METADATA		= 0x1234002D,
	MTP_METHOD_BEGIN_NPDRM_GET_RTC			= 0x1234002E,
	MTP_METHOD_END_NPDRM_GET_RTC			= 0x1234002F,
	MTP_METHOD_BEGIN_NPDRM_ACTIVATE			= 0x12340030,
	MTP_METHOD_END_NPDRM_ACTIVATE			= 0x12340031,
	MTP_METHOD_BEGIN_NPDRM_DEACTIVATE		= 0x12340032,
	MTP_METHOD_END_NPDRM_DEACTIVATE			= 0x12340033,
	MTP_METHOD_BEGIN_NPDRM_GET_LICENSE		= 0x12340034,
	MTP_METHOD_END_NPDRM_GET_LICENSE		= 0x12340035,
	MTP_METHOD_BEGIN_RECV_DATA			= 0x12340036,
	MTP_METHOD_END_RECV_DATA			= 0x12340037,
	MTP_METHOD_RECV_DATA_SYNC			= 0x12340038,
	MTP_METHOD_BEGIN_SEND_DATA			= 0x12340039,
	MTP_METHOD_END_SEND_DATA			= 0x1234003A,
	MTP_METHOD_SEND_DATA_SYNC			= 0x1234003B,
	MTP_METHOD_SET_DATA_PHASE_RESULT		= 0x1234003C,
	MTP_METHOD_GET_CONTEXT_PROGRESS			= 0x1234003D,
	MTP_METHOD_GET_HOST_STATUS			= 0x1234003E,
	MTP_METHOD_GET_HOST_VERSION			= 0x1234003F,
	MTP_METHOD_CANCEL_EXECUTION			= 0x12340040,
	MTP_METHOD_CHECK_HOST_RESET			= 0x12340041,
	MTP_METHOD_MARLIN_ACTIVATE			= 0x12340042,
	MTP_METHOD_MARLIN_DEACTIVATE			= 0x12340043,
	MTP_METHOD_MARLIN_LICENSE_MOVE			= 0x12340044,
	MTP_METHOD_MARLIN_LICENSE_COPY			= 0x12340045,
	MTP_METHOD_MARLIN_LICENSE_CHECK			= 0x12340046,
	MTP_METHOD_MARLIN_SHARED_INFO_ACQ		= 0x12340047,
	MTP_METHOD_ADD_OBJECT_DATA			= 0x12340048,
	MTP_METHOD_REMOVE_OBJECT_DATA			= 0x12340049,
	MTP_METHOD_GET_OBJECT_DATA			= 0x1234004A,
	MTP_METHOD_FIND_OBJECT_BY_PATH			= 0x1234004B,
	MTP_METHOD_UPDATE_OBJECT_DATA			= 0x1234004C,
	MTP_METHOD_RESET_STORAGE			= 0x1234004D,
	MTP_METHOD_START_HOST_DISCOVERY			= 0x1234004E,
	MTP_METHOD_STOP_HOST_DISCOVERY			= 0x1234004F,
	MTP_METHOD_GET_HOST_DISCOVERY_STATE		= 0x12340050,
	MTP_METHOD_GET_NUM_DISCOVERED_HOSTS		= 0x12340051,
	MTP_METHOD_GET_DISCOVERED_HOST			= 0x12340052,
	MTP_METHOD_START_NET_REGISTER			= 0x12340053,
	MTP_METHOD_SEND_NET_REGISTER_PIN		= 0x12340054,
	MTP_METHOD_STOP_NET_REGISTER			= 0x12340055,
	MTP_METHOD_GET_NUM_REGISTERED_HOSTS		= 0x12340056,
	MTP_METHOD_GET_REGISTERED_HOST			= 0x12340057,
	MTP_METHOD_UNREGISTER_HOST			= 0x12340058,
	MTP_METHOD_IS_HOST_ONLINE			= 0x12340059,
	MTP_METHOD_IS_HOST_REGISTERED			= 0x1234005A,
	MTP_METHOD_ASSIGN_HOST				= 0x1234005B,
	MTP_METHOD_UNASSIGN_HOST			= 0x1234005C,
	MTP_METHOD_IS_HOST_ASSIGNED			= 0x1234005D,
	MTP_METHOD_GET_ASSIGNED_HOST			= 0x1234005E,
	MTP_METHOD_GET_CURRENT_INTERFACE		= 0x1234005F,
	MTP_METHOD_START_INTERFACE			= 0x12340060,
	MTP_METHOD_STOP_INTERFACE			= 0x12340061,
	MTP_METHOD_IS_STARTED				= 0x12340062,
	MTP_METHOD_SWITCH_USB_MODE			= 0x12340063,
	MTP_METHOD_GET_USB_MODE_STATE			= 0x12340064,
	MTP_METHOD_GET_PUBLIC_STATUS			= 0x12340065,
	MTP_METHOD_GET_CAPABILITY			= 0x12340066,
	MTP_METHOD_SET_CMA_ACTIVE			= 0x12340067,
	MTP_METHOD_GET_CMA_ACTIVE			= 0x12340068,
	MTP_METHOD_START_CONSOLE_INFO			= 0x12340069,
	MTP_METHOD_BEGIN_SEND_CONSOLE_INFO_DATA		= 0x1234006A,
	MTP_METHOD_END_SEND_CONSOLE_INFO_DATA		= 0x1234006B,
	MTP_METHOD_CANCEL_CONSOLE_INFO			= 0x1234006C,
	MTP_METHOD_BEGIN_RPC_NET_OPERATION		= 0x1234006D,
	MTP_METHOD_END_RPC_NET_OPERATION		= 0x1234006E,
	MTP_METHOD_BEGIN_RPC_NET_OPERATION_SEND		= 0x1234006F,
	MTP_METHOD_END_RPC_NET_OPERATION_SEND		= 0x12340070,
	MTP_METHOD_BEGIN_RPC_NET_OPERATION_RECV		= 0x12340071,
	MTP_METHOD_END_RPC_NET_OPERATION_RECV		= 0x12340072,
	MTP_METHOD_CHECK_ABORTED			= 0x12340073,
	MTP_METHOD_BEGIN_SEARCH_OBJECT			= 0x12340074,
	MTP_METHOD_END_SEARCH_OBJECT			= 0x12340075,
	MTP_METHOD_BEGIN_CONNECT_TO_HOST		= 0x12340076,
	MTP_METHOD_END_CONNECT_TO_HOST			= 0x12340077,
	MTP_METHOD_STOP_INTERFACE_ALT			= 0x12340078,
	MTP_METHOD_BEGIN_HANDOVER			= 0x12340079,
	MTP_METHOD_END_HANDOVER				= 0x1234007A,
	MTP_METHOD_GET_DEVICE_ID			= 0x1234007B,
	MTP_METHOD_PAUSE_EXPORT_OBJECT			= 0x1234007C,
	MTP_METHOD_BEGIN_RESUME_EXPORT_OBJECT		= 0x1234007D,
	MTP_METHOD_END_RESUME_EXPORT_OBJECT		= 0x1234007E,
	MTP_METHOD_BEGIN_RESUME_IMPORT_OBJECT		= 0x1234007F,
	MTP_METHOD_END_RESUME_IMPORT_OBJECT		= 0x12340080,
	MTP_METHOD_NOOP					= 0x12340081,
	MTP_METHOD_BEGIN_LARGE_BUFFER			= 0x12340082,
	MTP_METHOD_END_LARGE_BUFFER			= 0x12340083,
	MTP_METHOD_SET_BG_COPY_FLAG			= 0x12340084,
	MTP_METHOD_SEND_SESSION_INFO			= 0x12340085,
	MTP_METHOD_BGCOPY_REQUEST_TASK			= 0x12340086,
	MTP_METHOD_BGCOPY_END_TASK			= 0x12340087,
	MTP_METHOD_BGCOPY_SET_TASK_INFO			= 0x12340088,
	MTP_METHOD_BGCOPY_WRITE_DATA			= 0x12340089,
	MTP_METHOD_BGCOPY_READ_DATA			= 0x1234008A,
	MTP_METHOD_BGCOPY_GET_TASK_INFO			= 0x1234008B,
	MTP_METHOD_BGCOPY_NOTIFY_TASK			= 0x1234008C
};

/* IPMI method IDs for SceMtpMlnSvc */
enum MtpMarlinIpmiMethod {
	MTP_MARLIN_METHOD_INITIALIZE	= 0x12340000,
	MTP_MARLIN_METHOD_FINALIZE	= 0x12340001,
	MTP_MARLIN_METHOD_GET_INFO	= 0x12340002,
	MTP_MARLIN_METHOD_SET_INFO	= 0x12340003
};

/* MtpStatus (104 bytes = 0x68, matches libmtp/mtp_internal.h) */
struct MtpStatus {
	int state;
	int platformType;
	int protocolVersion;
	int applicationType;
	char name[64];
	SceUInt32 flags;
	int platformClass;
	char osVersion[8];
	int reason;
	int extraArg;
};

/* MtpNetHost (200 bytes = 0xC8, matches libmtp/mtp_internal.h) */
struct MtpNetHost {
	char name[0x7E];
	SceUInt8 id[16];
	SceUInt8 padAfterId[2];
	int hostType;
	SceUInt16 requestPort;
	SceUInt16 padAfterPort;
	int mtpProtocolVersion;
	int wirelessProtocolVersion;
	char address[16];
	SceUInt64 lastConnectedTime;
	SceUInt8 macAddress[6];
	SceUInt8 padAfterMac[2];
	SceUInt32 supportedDevice;
	SceUInt32 reserved;
};

/* MtpStatusEventMsg: 108-byte status payload (eventCode + 104-byte MtpStatus) */
struct MtpStatusEventMsg {
	int eventCode;
	MtpStatus hostStatus;
};

/* 112-byte IPMI notification packet sent on session message queue 0 */
struct MtpBroadcastMessage {
	int msgType;
	union {
		MtpStatusEventMsg statusEvent;
		SceUInt32 registerEvent;
		SceUInt32 bgCopyEvent;
		SceUInt8 rawBytes[108];
	} body;
};

/* 32-byte capability query result returned by SceLibMtp_94040962 */
struct MtpCapabilityInfo {
	SceUChar8 supported;
	SceUChar8 pad[3];
	SceUInt32 capabilityStatus;
	paf::datetime::DateTime timestamp;
};

/* Background copy task status (matches libmtp MtpBgCopyTaskStatus, 32 bytes) */
struct MtpBgCopyStatusWire {
	int result;
	paf::string name;
	SceUInt32 progress;
	int index;
	SceUInt32 total;
	int remaining;
};

/* Background copy task request (48 bytes) passed to SceLibMtp_E89AF226 / SceLibMtp_87D21619 */
struct MtpBgCopyTaskWire {
	int contentType;
	SceUInt32 reserved;
	SceUInt32 sizeLow;
	SceUInt32 sizeHigh;
	MtpBgCopyStatusWire status;
};

/* Account link info passed to mtpNotifyAccountLinkResult (0x812B1C8A) */
struct MtpAccountLinkInfo {
	int errorCode;
	int hasErrorFlag;
	int accountType;
	int reserved;
	SceUInt8 linked;
	SceUInt8 padBeforeAccountId[7];
	SceUInt64 localAccountId;
	SceUInt8 accountMetadata[72];
	SceUInt8 hostLinked;
	SceUInt8 hostValid;
	SceUInt8 padBeforeHostAccountId[6];
	SceUInt64 hostAccountId;
};

/* SceMtpInitParam (24 bytes, matches libmtp/mtp_internal.h) */
struct SceMtpInitParam {
	int devThreadPriority;
	int devThreadCpuAffinityMask;
	int xferThreadPriority;
	int xferThreadCpuAffinityMask;
	int reserved;
	const void *largeBufferFuncs;
};

/* Startup parameters passed to shellMtpServerStart */
struct ShellMtpServerParam {
	int serverThreadPriority;
	int devThreadPriority;
	int devThreadCpuAffinityMask;
	int xferThreadPriority;
	int xferThreadCpuAffinityMask;
	const void *largeBufferFuncs;
};

/* Callback invoked when MTP needs the shell to start sending console info */
typedef int (*MtpConsoleInfoStartCallback)(void *userArg);

/* Callback invoked by MtpServiceWorkerJob once the worker finishes its data phase */
typedef int (*MtpServiceEndCallback)(IPMI::Session *session, SceUInt32 requestId,
				     SceUInt32 contextId);

/* Event listener registered by mtp_event_handler.cpp (vtable slots +0x08..+0x24) */
class MtpEventListener {
public:
	virtual ~MtpEventListener() {}
	virtual void onMtpAvailable() = 0;		/* +0x08 */
	virtual void onMtpConnectionTimeout() = 0;	/* +0x0C */
	virtual void onMtpConnected() = 0;		/* +0x10 */
	virtual void onMtpDisconnected() = 0;		/* +0x14 */
	virtual void onMtpBusy() = 0;			/* +0x18 */
	virtual void onUsbDetected() = 0;		/* +0x1C */
	virtual void onHostReady() = 0;			/* +0x20 */
	virtual void onCmaDeactivated() = 0;		/* +0x24 */
};

/* Active ticket -> libmtp contextId mapping stored per session */
struct MtpContextMapping {
	SceUInt32 ticket;
	SceUInt32 contextId;
};

/* Per-client session state inside MtpServiceSessionMgr (148 bytes) */
struct MtpSessionSlot {
	IPMI::Session *session;
	paf::list<MtpContextMapping *> contexts;
	SceUInt8 hostReset;
	SceUInt8 padBeforeLastState[3];
	int lastHostState;
	MtpStatus deferredStatus;
	int networkImposeToken;
	paf::list<SceUInt32> deferredRegisterEvents;
	SceUInt32 flags;
};

/* MtpServiceWorker ("SRVWKR", 96 bytes): serializes async send/recv data transfers */
class MtpServiceWorker {
public:
	MtpServiceWorker();
	~MtpServiceWorker();

	int initialize();
	int finalize();
	int run(SceUInt32 contextId);
	int reset();
	int setTerminate();
	int setRecvJob(IPMI::Session *session, SceUInt32 requestId,
		       SceUInt32 contextId, SceSize size);
	int setSendJob(IPMI::Session *session, SceUInt32 requestId,
		       SceUInt32 contextId, const void *srcData, SceSize size);

private:
	int setSendRecvJob(IPMI::Session *session, SceUInt32 requestId,
			   SceUInt32 contextId, void *buffer, SceSize size, int isRecv);
	int execSendRecvJob();
	void lock();
	void unlock();

	SceKernelLwMutexWork m_mutex;
	IPMI::Session *m_session;
	SceUInt32 m_requestId;
	SceUInt32 m_contextId;
	SceSize m_transferSize;
	void *m_transferBuffer;
	int m_isRecv;
	SceUInt8 m_jobPending;
	SceUInt8 m_waitingForFinish;
	SceUInt8 m_padBeforeEvf[2];
	SceUID m_eventFlag;
	SceKernelLwCondWork m_condJobFinish;
	int m_state;
	SceUID m_runningEventFlag;
	SceUInt32 m_completedCount;
	SceUInt32 m_queuedCount;
};

/* MtpServiceSessionMgr ("SESMGR", 1216 bytes): tracks up to 8 IPMI sessions */
class MtpServiceSessionMgr {
public:
	MtpServiceSessionMgr();
	~MtpServiceSessionMgr();

	int initialize();
	int finalize();
	int prepare(int *outSlotIndex);
	int set(SceUInt32 slotIndex, IPMI::Session *session);
	int clear(IPMI::Session *session);
	int cancelByProcessId(SceUID pid);
	int terminateByProcessId(SceUID pid);
	int disableNotify(IPMI::Session *session);
	int lockSession(IPMI::Session *session);
	int unlockSession();
	int addContext(IPMI::Session *session, SceUInt32 ticket, SceUInt32 contextId);
	int deleteContext(IPMI::Session *session, SceUInt32 ticket);
	int getContext(IPMI::Session *session, SceUInt32 ticket, SceUInt32 *outContextId);
	int getSessionIndex(IPMI::Session *session, SceUInt32 *outIndex);
	int getSessionByIndex(SceUInt32 index, IPMI::Session **outSession);
	int markHostResetAll();
	int checkHostReset(IPMI::Session *session, SceUInt8 *outReset);
	int onStartHostDiscovery(IPMI::Session *session);
	int onStopHostDiscovery(IPMI::Session *session);
	int onStartRegister(IPMI::Session *session);
	int onStopRegister(IPMI::Session *session);
	int enableNetworkImpose(IPMI::Session *session);
	int disableNetworkImpose(IPMI::Session *session);
	void onProcessActivate(SceUID pid);
	void onProcessDeactivate(SceUID pid);
	int broadcastMtpStatusEvent(MtpStatusEventMsg *eventMsg);
	int broadcastMtpStatusEventDuringSuspend(SceUID pid, const MtpStatus *currentStatus);
	int broadcastNetMtpRegisterEvent(const SceUInt32 *eventCode);
	int broadcastNetMtpRegisterEventDuringSuspend(SceUID pid);
	int broadcastMtpBgCopyEvent(const SceUInt32 *eventCode);
	bool hasSessionForProcess(SceUID pid);

private:
	int findSlotByProcessId(SceUID pid, SceUInt32 *outIndex);
	void restartHostDiscoveryOnProcessActivate(SceUID pid);
	void enableNetworkImposeOnProcessActivate(SceUID pid);
	void disableNetworkImposeOnProcessDeactivate(SceUID pid);
	void stopHostDiscoveryOnProcessDeactivate(SceUID pid);
	void lock();
	void unlock();

	SceKernelLwMutexWork m_mutex;
	MtpSessionSlot m_slots[MTP_MAX_CLIENTS];
};

/* MtpServiceActivationMgr ("ACTIVE", 36 bytes): guards ux0: storage activation */
class MtpServiceActivationMgr {
public:
	MtpServiceActivationMgr() : m_isActive(0), m_deactivatePending(0) {}
	int initialize();
	int finalize();
	int activate();
	int deactivate();

	SceKernelLwMutexWork m_mutex;
	SceUInt8 m_isActive;
	SceUInt8 m_deactivatePending;
	SceUInt8 m_pad[2];
};

/* MtpServer ("SERVER", 1448 bytes): owns IPMI servers, job queues, session mgr, worker */
class MtpServer : public IPMI::Server::EventHandler {
public:
	MtpServer();
	virtual ~MtpServer();

	/* IPMI::Server::EventHandler overrides */
	virtual void onSyncMethodDispatch(IPMI::Session *session, SceUInt32 methodId,
					  const IPMI::DataInfo *inList, SceUInt32 inCount,
					  IPMI::BufferInfo *outList, SceUInt32 outCount);
	virtual void onAsyncMethodDispatch(IPMI::Session *session, SceUInt32 requestId,
					   SceUInt32 methodId, const IPMI::DataInfo *inList,
					   SceUInt32 inCount);
	virtual int onNewClient(IPMI::Server *server, const IPMI::Session::Config *config,
				const void *param, SceSize paramSize);
	virtual void onSessionClosedByClient(IPMI::Session *session);
	virtual void onSessionKilled(IPMI::Session *session);
	virtual void onFatalError(int error);

	int initialize(const ShellMtpServerParam *param);
	int finalize();
	int start();

	void enqueueStorageResetJob();
	void enqueueStorageActivationJob(bool activate);
	int switchUsbMode(int usbMode);
	static bool isUsbCdromMode();
	int getUsbModeState(int *outState);
	static bool isDialogSuppressed();
	void setCmaActive(int active);
	int isCmaActive() const;
	void enqueueWatchConnectionJob();

	void notifyMtpAvailable();
	void notifyUsbDetection();
	void notifyMtpBusy();
	void notifyMtpConnection();
	void notifyMtpDisconnection();
	void notifyHostReady();

	void onHostEvent(int eventCode);
	void onBgCopyEvent(int eventCode);

	void notifyProcessDeactivated(SceUID pid);
	void notifyProcessActivated(SceUID pid, int appId, const char *titleId);
	void notifyProcessEnded(SceUID pid, int appId, const char *titleId);
	void notifyLiveAreaOrient(const char *titleId);
	void notifyWifiTerminate();

	int enqueueServiceJob(paf::common::SharedPtr<paf::job::JobItem> &job);
	int enqueueCancelJob(paf::common::SharedPtr<paf::job::JobItem> &job);
	int startServiceJob(IPMI::Session *session, SceUInt32 requestId,
			    SceUInt32 contextId, MtpServiceEndCallback endCallback);

	void setEventListener(MtpEventListener *listener);
	int setConsoleInfoCallback(MtpConsoleInfoStartCallback callback, void *userArg);
	int startConsoleInfo(int *outStarted);
	int beginSendConsoleInfoData(char *outUrl, SceSize outUrlSize, int *outFinished);
	int endSendConsoleInfoData(int httpStatus);
	int sendConsoleInfoData(const ShellMtpHttpRequest *request, int timeoutMs);
	int cancelConsoleInfo();

	MtpServiceSessionMgr &sessionMgr() { return m_sessionMgr; }
	MtpServiceWorker &worker() { return m_worker; }
	MtpServiceActivationMgr &activationMgr() { return m_activationMgr; }
	int processActivityState() const { return m_processActivityState; }
	void setProcessActivityState(int state) { m_processActivityState = state; }

private:
	int initializeJobQueue(int priority);
	int initializeMtp(const ShellMtpServerParam *param);
	int initializeServer(int priority, IPMI::Server **outServer,
			     const IPMI::Server::Config *config,
			     void **outWorkBuf, SceSize *outWorkBufSize,
			     SceUID *outThreadId, SceKernelThreadEntry threadEntry);
	int finalizeServer();

	static int serverThreadEntry(SceSize args, void *argp);
	static int serverCancelThreadEntry(SceSize args, void *argp);
	static void hostEventTrampoline(int eventCode, void *userArg);
	static void bgCopyEventTrampoline(int eventCode, void *userArg);

	IPMI::Server *m_server;
	IPMI::Server *m_cancelServer;
	paf::job::JobQueue *m_serviceJobQueue;
	paf::job::JobQueue *m_cancelJobQueue;
	MtpServiceSessionMgr m_sessionMgr;
	MtpServiceActivationMgr m_activationMgr;
	MtpServiceWorker m_worker;
	int m_state;
	SceUID m_serverThreadId;
	SceUID m_cancelThreadId;
	SceSize m_serverWorkBufSize;
	void *m_serverWorkBuf;
	SceSize m_cancelWorkBufSize;
	void *m_cancelWorkBuf;
	MtpEventListener *m_eventListener;
	SceUID m_serviceEventFlag;
	SceUInt8 m_cmaActive;
	SceUInt8 m_padAfterCma[3];
	MtpConsoleInfoStartCallback m_consoleInfoCallback;
	void *m_consoleInfoArg;
	const ShellMtpHttpRequest *m_pendingConsoleInfoReq;
	SceUInt8 m_consoleInfoAborted;
	SceUInt8 m_padAfterAborted[3];
	SceUID m_consoleInfoEventFlag;
	int m_consoleInfoHttpStatus;
	SceUInt8 m_consoleInfoReservedFlag;
	SceUInt8 m_padBeforeActivity[3];
	int m_processActivityState;
};

/* Global MtpServer singleton (0x815C3118) */
extern MtpServer g_mtpServer;

/* IPMI dispatch helpers (mtp_service_dispatch.cpp) */
int mtpServiceDispatchSync(IPMI::Session *session, SceUInt32 methodId,
			   const IPMI::DataInfo *inList, SceUInt32 inCount,
			   IPMI::BufferInfo *outList, SceUInt32 outCount);
int mtpServiceDispatchAsync(IPMI::Session *session, SceUInt32 requestId,
			    SceUInt32 methodId, const IPMI::DataInfo *inList);
int mtpServiceRespondRecvData(IPMI::Session *session, SceUInt32 requestId, int result,
			      const void *buffer, SceSize *receivedSize,
			      SceUInt64 *totalSize);
int mtpServiceRespondSendData(IPMI::Session *session, SceUInt32 requestId, int result,
			      const SceSize *sentSize, const SceUInt64 *totalSize);

/* libmtp module loader wrapper (mtp_wrapper.cpp) */
int mtpWrapperInitialize(const SceMtpInitParam *param);
int mtpWrapperFinalize(void);

/* Shell MTP job queue accessor (shared with mtp_event_handler.cpp) */
extern paf::job::JobQueue *shellGetMtpJobQueue(void);

/* External C APIs used by MTP server */
extern "C" {
extern int sceClibPrintf(const char *fmt, ...);
extern int sceAppMgrGetStatusByAppId(int appId, void *status);
extern int sceAppMgrGetStatusByName(const char *titleId, void *status);
extern int SceShell_6B8772A2(SceUID pid);
extern int SceShell_794BF5BE(unsigned int token);
extern int sceUsbstorVStorSetDeviceInfo(const char *product, const char *revision);
extern int sceUsbstorVStorSetImgFilePath(const char *path);
extern int sceUsbstorVStorStart(int type);
extern int sceUsbstorVStorStop(void);

/* SceLibMtp imports (verified against vsh/app/common/src/mtp/libmtp/) */
extern int sceMtpInit(const SceMtpInitParam *param);
extern int sceMtpEnd(void);
extern int sceMtpStart(int ifType, const MtpNetHost *host, int unused, int run);
extern int sceMtpStop(void);
extern int sceMtpSuspend(void);
extern int sceMtpResume(void);
extern int sceMtpReset(void);
extern int sceMtpGetCurrentInterface(int *ifType);
extern int sceMtpSetFinishCallback(SceUInt32 contextId, void (*handler)(void *userArg),
				   void *userArg);
extern int sceMtpCheckContextValid(SceUInt32 contextId);
extern int sceMtpGetBgTaskInfo(SceUInt32 *taskId, int *taskType, paf::string *path);
extern int sceMtpPauseExportObject(SceUInt32 contextId);

extern int sceMtpBeginGetNumOfObject(int storageId, int formatCode, SceUInt32 *contextId);
extern int SceLibMtp_123CA37E(SceUInt32 contextId, SceUInt32 *numObjects);
extern int sceMtpBeginGetObjectMetadata(int storageId, int formatCode, int offset, int count,
					SceUInt32 *contextId);
extern int SceLibMtp_4F285949(SceUInt32 contextId);
extern int sceMtpBeginImportObject(int category, const void *metadata, SceSize metadataSize,
				   SceUInt32 *contextId, const void *extra, SceSize extraSize);
extern int sceMtpEndImportObject(SceUInt32 contextId, SceUInt64 *objectId);
extern int sceMtpBeginExportObjectWithCheck(SceUInt32 idLow, SceUInt32 idHigh,
					    SceUInt32 flagsLow, SceUInt32 flagsHigh,
					    SceUInt32 *contextId, int checkFlag);
extern int SceLibMtp_8F3DBF97(SceUInt32 contextId);
extern int sceMtpBeginCreateHostDir(const void *dirInfo, int parentId, int flags,
				    SceUInt32 *contextId);
extern int SceLibMtp_549279DC(SceUInt32 contextId, SceUInt32 *dirHandle);
extern int sceMtpBeginDeleteHostDir(const void *dirInfo, int dirHandle, int flags,
				    SceUInt32 *contextId);
extern int SceLibMtp_F2A1482E(SceUInt32 contextId);
extern int sceMtpBeginMoveHostDir(int dirHandle, int destHandle, SceUInt32 *contextId);
extern int SceLibMtp_6079A84A(SceUInt32 contextId);
extern int sceMtpBeginCreateHostFile(const void *fileInfo, int parentId, int flags,
				     SceUInt32 *contextId);
extern int SceLibMtp_F195BEC6(SceUInt32 contextId, SceUInt32 *fileHandle);
extern int sceMtpBeginGetObjectThumbnail(int objectHandle, int flags, SceUInt32 *contextId);
extern int SceLibMtp_709C09B8(SceUInt32 contextId);
extern int sceMtpBeginGetObjectStatus2(int objectHandle, const void *query, int querySize,
				       SceUInt32 *contextId);
extern int SceLibMtp_7A229158(SceUInt32 contextId);
extern int sceMtpBeginDeleteObject(int objectHandle, SceUInt32 *contextId);
extern int SceLibMtp_F988D2EE(SceUInt32 contextId);
extern int sceMtpBeginSetSystemSetting(int settingId, SceUInt32 *contextId);
extern int SceLibMtp_D5FB089F(SceUInt32 contextId);
extern int SceLibMtp_CFA3A9DA(int settingId, int settingValue, SceUInt32 *contextId);
extern int SceLibMtp_EAA968F4(SceUInt32 contextId);
extern int sceMtpBeginGetNpAccountInfo(int accountFlags, SceUInt32 *contextId);
extern int SceLibMtp_DD5917EF(SceUInt32 contextId, SceUInt32 *outResult);
extern int sceMtpBeginReadObjectWithOffset(int objectHandle, int unused,
					   SceUInt64 offset, SceUInt64 readSize,
					   SceUInt32 *contextId);
extern int SceLibMtp_7FDDA203(SceUInt32 contextId);
extern int sceMtpBeginWriteObjectWithOffset(int objectHandle, int unused,
					    SceUInt64 offset, SceUInt64 writeSize,
					    SceUInt32 *contextId);
extern int SceLibMtp_C5E9C54E(SceUInt32 contextId);
extern int sceMtpBeginGetHostStorageSize(int storageId, SceUInt32 *contextId);
extern int SceLibMtp_4CAFB048(SceUInt32 contextId, void *storageInfo);
extern int sceMtpBeginCheckSameObjectExist(SceUInt32 idLow, SceUInt32 idHigh,
					   SceUInt32 *contextId);
extern int SceLibMtp_905BB9E8(SceUInt32 contextId, SceUInt32 *exists);
extern int sceMtpBeginGetTotalObjectSize(int category, SceUInt32 *contextId);
extern int SceLibMtp_4239328A(SceUInt32 contextId, void *countOut, void *sizeOut);
extern int sceMtpBeginSpecifiedObjectMetadata(int category, const void *handles, int count,
					      SceUInt32 *contextId);
extern int SceLibMtp_D4A132BD(SceUInt32 contextId);
extern int sceMtpBeginNpDrmGetRtc(const void *request, SceSize size, SceUInt32 *contextId);
extern int SceLibMtp_2D3DD76D(SceUInt32 contextId, void *rtcOut, int *httpStatus);
extern int sceMtpBeginNpDrmActivate(SceUInt32 flags, const void *request, SceSize size,
				    SceUInt32 *contextId);
extern int SceLibMtp_48DE8672(SceUInt32 contextId, void *actDatOut, int *httpStatus,
			      int *npError, SceUInt32 *resultFlags);
extern int sceMtpBeginNpDrmDeactivate(SceUInt32 flags, const void *request, SceSize size,
				      SceUInt32 *contextId);
extern int SceLibMtp_923FAB90(SceUInt32 contextId, int *httpStatus, int *npError,
			      SceUInt32 *resultFlags);
extern int sceMtpBeginNpDrmGetLicense(const void *rtc, const void *licenseId, SceSize size,
				      SceUInt32 *contextId);
extern int SceLibMtp_D9EEE248(SceUInt32 contextId, void *licenseOut, int *httpStatus);
extern int SceLibMtp_D6C90706(const void *request, SceSize size, SceUInt32 *contextId);
extern int SceLibMtp_6E2FE43A(SceUInt32 contextId, void *responseBuf, SceSize *responseSize);
extern int sceMtpBeginRpcNetOperationSend(SceSize size, SceUInt32 *contextId);
extern int SceLibMtp_D919C89C(SceUInt32 contextId, void *responseBuf, SceSize *responseSize);
extern int sceMtpBeginRpcNetOperationRecv(const void *request, SceSize size,
					  SceUInt32 *contextId);
extern int SceLibMtp_0311F66F(SceUInt32 contextId);
extern int SceLibMtp_FB181427(int category, int flags, int operation, int *aborted);
extern int sceMtpBeginSearchObject(int category, const void *query, int querySize,
				   SceUInt32 *contextId);
extern int SceLibMtp_BBCC3F5B(SceUInt32 contextId, SceUInt32 *numFound, SceUInt32 *searchId);
extern int sceMtpBeginHandover(int handoverParam, SceUInt32 *contextId);
extern int SceLibMtp_462A6667(SceUInt32 contextId);
extern int sceMtpBeginResumeExportObject(SceUInt32 *contextId);
extern int SceLibMtp_33B08344(SceUInt32 contextId);
extern int sceMtpBeginResumeImportObject(int category, int unused,
					 SceUInt64 offset, SceUInt64 resumeSize,
					 const void *extra, SceUInt32 *contextId);
extern int SceLibMtp_83242E03(SceUInt32 contextId, SceUInt64 *objectId);

extern void *SceLibMtp_DA13726B(SceSize size);
extern void SceLibMtp_492421F7(void *buf);
extern int SceLibMtp_0497FA87(SceUInt32 contextId, void *progressOut);
extern int SceLibMtp_F6C15FE8(void *versionOut);
extern int SceLibMtp_2EBE47B8(const char *mountPoint);
extern int SceLibMtp_A824A205(const char *mountPoint);
extern SceUInt64 SceLibMtp_38CBDF4A(void);
extern int SceLibMtp_5120B22A(SceUInt32 eventMask, void (*callback)(int, void *), void *userArg);
extern int SceLibMtp_39CEBDD1(SceUInt32 eventMask, void (*callback)(int, void *));
extern int SceLibMtp_934240B4(void (*callback)(int, void *), void *userArg);
extern int SceLibMtp_E7CB0238(void);
extern int SceLibMtp_4C1B7DBB(void);
extern int SceLibMtp_D63692E8(MtpNetHost *hostOut);
extern int SceLibMtp_94040962(int featureId, void *capabilityOut);
extern void SceLibMtp_4E2CF833(int canceling);
extern int SceLibMtp_3AF2F5D8(int accountLinkStatus);
extern int SceLibMtp_DE41AE27(const char *titleId);
extern int SceLibMtp_BB32CD0A(void);
extern void SceLibMtp_AF9A2787(void);
extern int SceLibMtp_965F1CD0(void);
extern int SceLibMtp_B7984D4C(int *startedOut);
extern void SceLibMtp_768EBC03(int timeoutSeconds);
extern int SceLibMtp_08C00AFC(int *progressOut);

/* Marlin & object-map & net-MTP & bgcopy SceLibMtp imports */
extern int SceLibMtp_C10B3D7C(int infoType, SceSize maxSize, SceUInt32 offset,
			      void *buffer, SceSize *outSize);
extern int SceLibMtp_6211F119(int infoType, SceUInt32 offset, const void *buffer, SceSize size);
extern int SceLibMtp_C12DE479(SceUInt32 flags, SceUInt32 *outResult, SceUInt32 *outDetail);
extern int SceLibMtp_24369599(SceUInt32 flags, SceUInt32 *outResult, SceUInt32 *outDetail);
extern int SceLibMtp_A55E6AB6(const char *contentId, SceSize idSize, SceUInt32 flags,
			      SceUInt32 *outResult);
extern int SceLibMtp_0BC34F1F(const char *contentId, SceSize idSize, SceUInt32 flags,
			      SceUInt32 *outResult);
extern int SceLibMtp_542F3064(const char *contentId, SceSize idSize, SceUInt32 flags,
			      SceUInt32 *outResult);
extern int SceLibMtp_E35C1C83(const char *contentId, SceSize idSize, SceUInt32 *outResult);
extern int SceLibMtp_81BCF297(const void *objectData, SceUInt64 *objectIdOut);
extern int SceLibMtp_E2A9E59C(SceUInt32 idLow, SceUInt32 idHigh);
extern int SceLibMtp_9310EEEF(SceUInt32 idLow, SceUInt32 idHigh, void *objectDataOut);
extern int SceLibMtp_6BB1C852(int bgCopyFlag);
extern int SceLibMtp_E107F90D(const char *path, SceUInt64 *objectIdOut);
extern int SceLibMtp_EA4E8C39(SceUInt32 idLow, SceUInt32 idHigh, const void *updateData);
extern int SceLibMtp_79F3EE1B(void);
extern int SceLibMtp_71197C62(void);
extern int SceLibMtp_06123E14(int *stateOut);
extern int SceLibMtp_06714DA7(SceUInt32 *numHostsOut);
extern int SceLibMtp_2CA2CD16(SceUInt32 index, MtpNetHost *hostOut);
extern int SceLibMtp_6DBD4FF7(MtpNetHost *host, void (*callback)(const SceUInt32 *eventCode));
extern int SceLibMtp_66B08C7D(const char *pin);
extern int SceLibMtp_55B66FA0(void);
extern int SceLibMtp_8E76FE4D(SceUInt32 *numHostsOut);
extern int SceLibMtp_0898F989(SceUInt32 index, MtpNetHost *hostOut);
extern int SceLibMtp_9144D0B4(const MtpNetHost *host);
extern int SceLibMtp_56FA46BB(const MtpNetHost *host, int *onlineOut);
extern int SceLibMtp_389F2FE2(const MtpNetHost *host, int *registeredOut);
extern "C" int SceLibMtp_5B4AF4B8(const MtpNetHost *host);
extern int SceLibMtp_27F8701C(void);
extern int SceLibMtp_8CD95681(int *assignedOut);
extern int SceLibMtp_ED13170C(MtpNetHost *hostOut);
extern int SceLibMtp_A78FEA82(int ifType);
extern int SceLibMtp_29FAC63B(SceUInt8 *deviceIdOut);
extern int SceLibMtp_E89AF226(const MtpBgCopyTaskWire *task, paf::string *outPath);
extern int SceLibMtp_E7542628(void);
extern int SceLibMtp_B15DA25F(const MtpBgCopyStatusWire *status);
extern int SceLibMtp_972A378F(const void *buffer, SceSize size, int index);
extern int SceLibMtp_ADE08924(void *buffer, SceUInt32 offset, SceSize size, int index,
			      SceSize *bytesReadOut);
extern int SceLibMtp_87D21619(int taskId, const MtpBgCopyTaskWire *task);
}

#endif
