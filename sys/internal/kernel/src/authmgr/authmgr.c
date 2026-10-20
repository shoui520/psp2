/*
 * SceSblAuthMgr: passes SELF authentication and key requests to the secure processor's
 * authentication module (kprx_auth_sm), through the secure module scheduler's proxy.
 *
 * A request is a packet in uncached memory: a 0x40-byte header (size, command, result) and the
 * command's data. The module started by sceSblAuthMgrOpen stays running until
 * sceSblAuthMgrClose; the key and bind commands start one for each request when none runs.
 */
#include <stdint.h>
#include <stddef.h>
#include <sysmem/suspend.h>
#include <sysmem/sysclib.h>
#include <sysmem/barrier.h>
#include <threadmgr/threadmgr.h>
#include <threadmgr/fast_mutex_object.h>
#include <sysmem/cpu.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysroot.h>
#include <sysmem/debug.h>
#include <smsc_proxy/smsc_proxy.h>
#include <post_ss_mgr/sblpostssmgr.h>
#include <post_ss_mgr/sblutmgr.h>
#include <kernel_boot_loader/kbl_param.h>
#include <authmgr/sblauthmgr.h>

#define AUTHMGR_NO_REQUEST	0x0fffffff	/* s_req_id while no module runs */
#define AUTHMGR_HANDLE		1		/* the one handle sceSblAuthMgrOpen gives */

/* the mailboxes of the module's session */
#define AUTHMGR_MAILBOX_INVOKE	0
#define AUTHMGR_MAILBOX_COMMAND	1

/* the event flag's bits: done, and the module's status in the upper half */
#define AUTHMGR_EVENT_DONE	0x1
#define AUTHMGR_EVENT_ERROR	0x20001

/* the module's states (SmschedIntrStatus.state) */
#define AUTHMGR_SM_STATE_EXITED	3
#define AUTHMGR_SM_STATE_IDLE	10

/* the commands */
#define AUTHMGR_CMD_AUTH_HEADER		1
#define AUTHMGR_CMD_SETUP_SEGMENT	2
#define AUTHMGR_CMD_AUTH_SEGMENT	3
#define AUTHMGR_CMD_GET_EKC		4
#define AUTHMGR_CMD_SET_DMAC5_KEY	5
#define AUTHMGR_CMD_CLEAR_DMAC5_KEY	6
#define AUTHMGR_CMD_DEC_BIND_DATA	7
#define AUTHMGR_CMD_VERIFY_SPFSO	8
#define AUTHMGR_CMD_STOP		0xffffffff

#define AUTHMGR_SPFSO_SIZE	0x200

/* the header of every packet */
typedef struct AuthMgrPacketHeader {
	SceSize size;
	SceUInt32 command;			/* (command << 16) | 1 */
	volatile int result;			/* -1 until the module answers */
	SceUInt32 reserved[13];
} AuthMgrPacketHeader;

/* AUTHMGR_CMD_AUTH_HEADER */
typedef struct AuthMgrAuthHeaderPacket {
	AuthMgrPacketHeader header;
	SelfAuthRequest auth;
	SceUInt32 rangeCount;			/* the header's physical ranges */
	SceUInt32 rangesPA;
	SceUInt32 reserved[6];
} AuthMgrAuthHeaderPacket;

/* AUTHMGR_CMD_SETUP_SEGMENT */
typedef struct AuthMgrSegmentPacket {
	AuthMgrPacketHeader header;
	SceUInt32 segment;
	int result;
	SceUInt32 reserved[2];
} AuthMgrSegmentPacket;

/* AUTHMGR_CMD_AUTH_SEGMENT, AUTHMGR_CMD_VERIFY_SPFSO: a buffer the module reads and writes */
typedef struct AuthMgrBufferPacket {
	AuthMgrPacketHeader header;
	SceUInt32 srcRangeCount;
	SceUInt32 srcRangesPA;
	SceUInt32 dstRangeCount;
	SceUInt32 dstRangesPA;
} AuthMgrBufferPacket;

/* AUTHMGR_CMD_GET_EKC, AUTHMGR_CMD_SET_DMAC5_KEY, AUTHMGR_CMD_CLEAR_DMAC5_KEY */
typedef struct AuthMgrKeyPacket {
	AuthMgrPacketHeader header;
	SceUInt8 data[0x100];
	SceSize size;
	SceUInt32 type;
	SceUInt32 slot;
	SceUInt32 reserved;
} AuthMgrKeyPacket;

/* AUTHMGR_CMD_DEC_BIND_DATA */
typedef struct AuthMgrBindPacket {
	AuthMgrPacketHeader header;
	SceKernelPARange input;
	SceKernelPARange output;
	SceUInt32 flags;
	SceUInt32 reserved;
} AuthMgrBindPacket;

/* the SELF commands share one packet */
typedef union AuthMgrSelfPacket {
	AuthMgrPacketHeader header;
	AuthMgrAuthHeaderPacket authHeader;
	AuthMgrSegmentPacket segment;
	AuthMgrBufferPacket block;
} AuthMgrSelfPacket;

/* the packets, in one uncached allocation */
typedef struct AuthMgrPacketArea {
	AuthMgrSelfPacket self;
	SceUInt8 reserved[0x30];
	AuthMgrPacketHeader stop;
	SceUInt32 stopReserved[4];
	AuthMgrKeyPacket key;
	AuthMgrBindPacket bind;
	AuthMgrBufferPacket spfso;
} AuthMgrPacketArea;

_Static_assert(sizeof(AuthMgrPacketHeader) == 0x40, "AuthMgrPacketHeader");
_Static_assert(sizeof(AuthMgrAuthHeaderPacket) == 0x190, "AuthMgrAuthHeaderPacket");
_Static_assert(sizeof(AuthMgrKeyPacket) == 0x150, "AuthMgrKeyPacket");
_Static_assert(sizeof(AuthMgrBindPacket) == 0x58, "AuthMgrBindPacket");
_Static_assert(sizeof(AuthMgrPacketArea) == 0x408, "AuthMgrPacketArea");

static int authMgrWaitSelfSemaphore(void);
static int authMgrIntrHandlerforInvoke(SceUID reqId, int mailbox, void *argp, SceUInt32 status,
				       SceUInt32 result);
static int authMgrWaitSM(void);
static int authMgrSetupSrcPARanges(const void *addr, SceSize size);
static int authMgrIntrHandler(SceUID reqId, int mailbox, void *argp, SceUInt32 status,
			      SceUInt32 result);
static int authMgrSendRequest(SceUID reqId, AuthMgrPacketHeader *packet);
static int authMgrSendPacketCore(AuthMgrPacketHeader *packet, SceSize size, SceUInt32 command);
static int authMgrStopSM(SceUID reqId);
static int authMgrInvokeSM(void);
static int authMgrSendPacket(AuthMgrPacketHeader *packet, SceSize size, SceUInt32 command);
static int sceSblAuthMgrStart(void);

static SceKernelFastMutex s_send_cmd_mutex;
static SceUInt32 s_src_pa_ranges_pa;
static AuthMgrSelfPacket *s_self_packet;
static AuthMgrPacketHeader *s_stop_packet;
static SceUID s_self_api_sema;
static AuthMgrKeyPacket *s_key_packet;
static AuthMgrBindPacket *s_bind_packet;
static SceUInt8 s_spfso_auth[AUTHMGR_SPFSO_SIZE];
static AuthMgrBufferPacket *s_spfso_packet;
static SceKernelPARange s_src_pa_ranges[16] __attribute__((aligned(64)));
static SceUID s_req_id;
static SceUID s_evf_id;
static int s_self_handle;
static SceKernelFastMutex s_npdrm_api_mutex;

static __attribute__((noinline)) int authMgrWaitSelfSemaphore(void)
{
	int ret;

	ret = sceKernelWaitSema(s_self_api_sema, 1, NULL);
	if (ret > 0)
		ret = 0;
	return ret;
}

/* the module's start: it answers on the invoke mailbox */
static __attribute__((noinline)) int authMgrIntrHandlerforInvoke(SceUID reqId, int mailbox,
	void *argp, SceUInt32 status, SceUInt32 result)
{
	SceUInt32 bits;

	(void)argp;
	if (mailbox != AUTHMGR_MAILBOX_INVOKE) {
		SCE_DBG_PRINTF(0xb, 1269, "ERROR: invalid mailbox id [%d]\n", mailbox);
		SCE_PANIC(1270);
	}
	if (reqId != s_req_id) {
		SCE_DBG_PRINTF(0xb, 1273, "ERROR: req_id [%d] != s_req_id [%d]\n", reqId, s_req_id);
		if (s_req_id == AUTHMGR_NO_REQUEST)
			SCE_PANIC(1279);
		SCE_PANIC(1283);
	}
	if ((status & ~8U) == 2)
		bits = AUTHMGR_EVENT_DONE;
	else
		bits = result << 16 | AUTHMGR_EVENT_DONE;
	sceKernelSetEventFlag(s_evf_id, bits);
	return 0;
}

static __attribute__((noinline)) int authMgrWaitSM(void)
{
	SceUInt32 bits;
	int ret;

	sceKernelSysrootDisableAutoClockDown();
	ret = sceKernelWaitEventFlag(s_evf_id, AUTHMGR_EVENT_DONE, 5, &bits, NULL);
	sceKernelSysrootEnableAutoClockDown();
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 976, "ERROR: sceKernelWaitEventFlag ret = 0x%08x\n", ret);
		return ret;
	}
	ret = bits >> 16;
	if (ret != 0) {
		SCE_DBG_PRINTF(0xb, 984, "ERROR: authMgr status = 0x%08x\n", ret);
		ret = SCE_SBL_ERROR_AM_SM_FAILED;
	}
	return ret;
}

/* the physical ranges of a buffer, in s_src_pa_ranges: returns their count */
static __attribute__((noinline)) int authMgrSetupSrcPARanges(const void *addr, SceSize size)
{
	SceKernelVARange range;
	SceKernelPAVector vector;
	int ret, count;

	range.addr = (uintptr_t)addr;
	range.size = size;
	vector.size = sizeof(vector);
	vector.capacity = 16;
	vector.pRanges = s_src_pa_ranges;
	ret = sceKernelVARangeToPAVector(&range, &vector);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 1113, "ERROR: authMgrSetupSrcPARanges failed ! ret = 0x%08x\n", ret);
		return SCE_SBL_ERROR_AM_NO_PHYSICAL_ADDRESS;
	}
	count = vector.total;
	sceKernelDcacheCleanRange(s_src_pa_ranges, (count * sizeof(SceKernelPARange) + 0x1f) & ~0x1f);
	return count;
}

/* a command's answer, on the command mailbox */
static __attribute__((noinline)) int authMgrIntrHandler(SceUID reqId, int mailbox, void *argp,
	SceUInt32 status, SceUInt32 result)
{
	SceUInt32 mailval = 0;
	int ret;

	(void)argp;
	(void)status;
	(void)result;
	if (reqId != s_req_id) {
		SCE_DBG_PRINTF(0xb, 1019, "ERROR: req_id [%d] != s_req_id [%d]\n", reqId, s_req_id);
		SCE_PANIC(1020);
	}
	if (mailbox != AUTHMGR_MAILBOX_COMMAND) {
		SCE_DBG_PRINTF(0xb, 1023, "ERROR: invalid mailbox id [0x%08x]\n", mailbox);
		SCE_PANIC(1024);
	}
	ret = sceSblSmSchedProxyReadCry2Arm(reqId, mailbox, &mailval);
	if (ret != 0) {
		SCE_DBG_PRINTF(0xb, 1047, "ERRORR: sceSblSmSchedProxyReadCry2Arm ret = 0x%08x\n", ret);
		return ret;
	}
	ret = sceSblSmSchedProxyWriteCry2Arm(reqId, mailbox, mailval);
	if (ret != 0) {
		SCE_DBG_PRINTF(0xb, 1052, "ERROR: sceSblSmSchedProxyWriteCry2Arm ret = 0x%08x\n", ret);
		return ret;
	}
	if (!(mailval & 1)) {
		SCE_DBG_PRINTF(0xb, 1064, "ERROR: Intr Handler : mailval = 0x%08x\n", mailval);
		SCE_PANIC(1065);
	}
	sceKernelSetEventFlag(s_evf_id, mailval == 1 ? AUTHMGR_EVENT_DONE : AUTHMGR_EVENT_ERROR);
	return ret;
}

static __attribute__((noinline)) int authMgrSendRequest(SceUID reqId, AuthMgrPacketHeader *packet)
{
	uintptr_t pa;
	int ret;

	packet->result = -1;
	dmb();
	(void)packet->result;
	dsb();
	ret = sceKernelVAtoPA(packet, &pa);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 1250, "ERROR: sceKernelVAtoPA ret = 0x%08x\n", ret);
		return SCE_SBL_ERROR_AM_NO_PHYSICAL_ADDRESS;
	}
	return SceSblSmSchedProxyForKernel_723B382F(reqId, AUTHMGR_MAILBOX_COMMAND, pa | 1);
}

static __attribute__((noinline)) int authMgrSendPacketCore(AuthMgrPacketHeader *packet, SceSize size,
	SceUInt32 command)
{
	int ret, releaseRet, unlockRet;

	if (packet == NULL)
		return SCE_SBL_ERROR_AM_EINVAL;
	packet->size = size;
	packet->command = command << 16 | 1;
	ret = lockFastMutex(&s_send_cmd_mutex);
	if (ret < 0)
		return ret;
	ret = sceSblSmSchedProxyRegisterIntrHandler(s_req_id, AUTHMGR_MAILBOX_COMMAND,
		(SceSblSmSchedCry2ArmHandler)authMgrIntrHandler, NULL);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 1562, "ERROR: authMgrSetupIntrHandler ret = 0x%08x\n", ret);
	} else {
		ret = authMgrSendRequest(s_req_id, packet);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 1571, "ERROR: authMgrSendRequest ret = 0x%08x\n", ret);
		} else {
			ret = authMgrWaitSM();
			if (ret == 0) {
				ret = packet->result;
				if (ret == -1) {
					SCE_DBG_PRINTF(0xb, 1587, "ERROR: return value is not updated.\n");
					SCE_PANIC(1588);
				}
			}
		}
	}
	releaseRet = SceSblSmSchedProxyForKernel_85EDA5FC(s_req_id, AUTHMGR_MAILBOX_COMMAND);
	if (releaseRet != 0) {
		ret = releaseRet;
		SCE_DBG_PRINTF(0xb, 1599, "ERROR: ReleaseHandler ret = 0x%08x\n", releaseRet);
	}
	unlockRet = sceKernelUnlockFastMutex(&s_send_cmd_mutex);
	if (unlockRet < 0)
		return unlockRet;
	return ret;
}

static __attribute__((noinline)) int authMgrStopSM(SceUID reqId)
{
	AuthMgrPacketHeader *packet = s_stop_packet;
	SmschedIntrStatus status;
	int ret;

	memset(packet, 0, sizeof(*packet) + sizeof(((AuthMgrPacketArea *)NULL)->stopReserved));
	packet->size = 0x50;
	packet->command = AUTHMGR_CMD_STOP;
	ret = authMgrSendRequest(reqId, packet);
	if (ret == 0) {
		ret = sceSblSmSchedProxyWait(reqId, &status);
		if (ret != 0)
			SCE_DBG_PRINTF(0xb, 1197, "ERROR: sceSblSmSchedProxyWait ret = 0x%08x\n", ret);
		else if (status.state != AUTHMGR_SM_STATE_EXITED)
			SCE_DBG_PRINTF(0xb, 1207, "warning: SM status is not exit [0x%08x]\n", status.state);
	} else if (ret == SCE_SBL_ERROR_SMSCHED_ILLEGAL_STATE) {
		ret = sceSblSmSchedProxyGetStatus(reqId, &status);
		if (ret != 0)
			SCE_DBG_PRINTF(0xb, 1172, "ERROR: sceSblSmSchedProxyGetStatus ret = 0x%08x\n", ret);
		else if (status.state != AUTHMGR_SM_STATE_EXITED && status.state != AUTHMGR_SM_STATE_IDLE)
			ret = SCE_SBL_ERROR_AM_SM_NOT_STOPPED;
	} else {
		SCE_DBG_PRINTF(0xb, 1155, "ERROR: authMgrSendRequest ret = 0x%08x\n", ret);
	}
	return ret;
}

/* starts kprx_auth_sm, the scheduler's own instance, as the kernel */
static __attribute__((noinline)) int authMgrInvokeSM(void)
{
	const struct SceKblParam *kblParam = sceKernelSysrootGetKblParam();
	SelfAuthRequest auth;
	int ret;

	memset(&auth, 0, sizeof(auth));
	memset(&auth.caller, 0, sizeof(auth.caller));
	auth.mediaType = (kblParam->boot_type_indicator_1 & BOOT_TYPE_SPECIAL_MEDIA) ? 1 : 10;
	auth.caller.programAuthorityId = 0x2808000000000001ULL;
	auth.caller.capability.bits[0] = 0x00f000c000000080ULL;
	auth.caller.capability.bits[1] = 0xffffffff00000000ULL;
	auth.caller.attribute.bits[0] = 0x00c3000003800980ULL;
	auth.caller.attribute.bits[1] = 0x0000008009800000ULL;
	auth.caller.attribute.bits[3] = 0xffffffff00000000ULL;
	auth.selfType = SMSCHED_INVOKE_SELF_INSTANCE | 2;
	ret = sceSblSmSchedProxyInvoke(0, 0, 0, NULL, &auth, &s_req_id);
	if (ret != 0) {
		SCE_DBG_PRINTF(0xb, 909, "ERROR: sceSblSmSchedProxyInvoke ret = 0x%08x\n", ret);
	} else {
		ret = sceSblSmSchedProxyRegisterIntrHandler(s_req_id, AUTHMGR_MAILBOX_INVOKE,
			(SceSblSmSchedCry2ArmHandler)authMgrIntrHandlerforInvoke, NULL);
		if (ret != 0) {
			SCE_DBG_PRINTF(0xb, 920, "ERROR: authMgrSetupIntrHandler ret = 0x%08x\n", ret);
		} else {
			ret = authMgrWaitSM();
			if (ret != 0) {
				SCE_DBG_PRINTF(0xb, 933, "ERROR: Invoke SM ret = 0x%08x\n", ret);
			} else {
				ret = SceSblSmSchedProxyForKernel_85EDA5FC(s_req_id, AUTHMGR_MAILBOX_INVOKE);
				if (ret == 0)
					return 0;
				SCE_DBG_PRINTF(0xb, 942, "ERROR: authMgrReleaseHandler err = 0x%08x\n", ret);
			}
		}
	}
	if (ret < 0)
		s_req_id = AUTHMGR_NO_REQUEST;
	return ret;
}

/* sends a packet, starting the module for it when none runs */
static __attribute__((noinline)) int authMgrSendPacket(AuthMgrPacketHeader *packet, SceSize size,
	SceUInt32 command)
{
	int ret, stopRet;

	SceKernelSuspendForDriver_4DF40893(0);
	if (s_req_id != AUTHMGR_NO_REQUEST) {
		ret = authMgrSendPacketCore(packet, size, command);
		if (ret > 0)
			ret = 0;
	} else {
		ret = authMgrInvokeSM();
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 1470, "ERROR: Invoke sm fail ret = 0x%08x\n", ret);
		} else {
			ret = authMgrSendPacketCore(packet, size, command);
			if (ret > 0)
				ret = 0;
			stopRet = authMgrStopSM(s_req_id);
			if (stopRet < 0) {
				SCE_DBG_PRINTF(0xb, 1493, ">>> Stop SM fail ret = 0x%08x\n", stopRet);
				SceKernelSuspendForDriver_2BB92967(0);
				return stopRet;
			}
			s_req_id = AUTHMGR_NO_REQUEST;
		}
	}
	SceKernelSuspendForDriver_2BB92967(0);
	return ret;
}

static __attribute__((noinline)) int sceSblAuthMgrStart(void)
{
	AuthMgrPacketArea *area;
	SceAllocOpt opt;
	int ret;

	s_self_handle = -1;
	s_req_id = AUTHMGR_NO_REQUEST;
	ret = sceKernelVAtoPA(s_src_pa_ranges, &s_src_pa_ranges_pa);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 239, "ERROR: sceKernelVAtoPA ret = 0x%08x\n", ret);
		return SCE_SBL_ERROR_AM_NO_PHYSICAL_ADDRESS;
	}
	opt.size = sizeof(opt);
	opt.mmapKind = 0;
	opt.align = 0x40;
	opt.mmapBase = NULL;
	opt.mmapSize = 0;
	area = SceSysmemForDriver_0B4ED16A(sizeof(*area), &opt);
	if (area == NULL)
		return SCE_SBL_ERROR_AM_ENOMEM;
	s_self_packet = &area->self;
	s_stop_packet = &area->stop;
	s_key_packet = &area->key;
	s_bind_packet = &area->bind;
	s_spfso_packet = &area->spfso;
	return 0;
}

int sceSblAuthMgrModuleStart(SceSize args, const void *argp)
{
	int ret;

	(void)args;
	(void)argp;
	sceSblAuthMgrStart();
	s_evf_id = sceKernelCreateEventFlag("SceSblAuthMgr", 0, 0, NULL);
	if (s_evf_id < 0) {
		SCE_DBG_PRINTF(0xb, 283, "ERROR: sceKernelCreateEventFlag fail\n");
		return SCE_SBL_ERROR_AM_ENOMEM;
	}
	ret = sceKernelInitializeFastMutex(&s_send_cmd_mutex, "SceSblAuthMgrSendCmd", 0, NULL);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 291, "ERROR: sceKernelInitializeFastMutex ret = 0x%08x\n", ret);
		return ret;
	}
	ret = sceKernelInitializeFastMutex(&s_npdrm_api_mutex, "SceSblAuthMgrNpdrmApi", 0, NULL);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 299, "ERROR: sceKernelInitializeFastMutex ret = 0x%08x\n", ret);
		return ret;
	}
	s_self_api_sema = sceKernelCreateSema("SceSblAuthMgrSelfApi", 0, 1, 1, NULL);
	if (s_self_api_sema < 0) {
		SCE_DBG_PRINTF(0xb, 308, "ERROR: sceKernelCreateSema ret = 0x%08x\n", s_self_api_sema);
		return ret;
	}
	memset(s_spfso_auth, 0, sizeof(s_spfso_auth));
	return 0;
}

/* starts the module for a SELF; it runs, and the caller holds the SELF API, until Close */
int sceSblAuthMgrOpen(int *handle)
{
	int ret, result, signalRet;

	if (handle == NULL)
		return SCE_SBL_ERROR_AM_EINVAL;
	SceKernelSuspendForDriver_4DF40893(0);
	ret = authMgrWaitSelfSemaphore();
	if (ret < 0)
		goto resume;
	ret = lockFastMutex(&s_npdrm_api_mutex);
	if (ret < 0)
		goto signal;
	if (s_self_handle == AUTHMGR_HANDLE) {
		result = SCE_SBL_ERROR_AM_EPERM;
	} else if (s_req_id != AUTHMGR_NO_REQUEST) {
		result = SCE_SBL_ERROR_AM_EINVAL;
	} else {
		result = authMgrInvokeSM();
		if (result == 0) {
			*handle = AUTHMGR_HANDLE;
			s_self_handle = AUTHMGR_HANDLE;
			ret = sceKernelUnlockFastMutex(&s_npdrm_api_mutex);
			if (ret < 0)
				goto resume;
			return 0;
		}
		SCE_DBG_PRINTF(0xb, 410, "ERROR: authMgrInvokeSM ret = 0x%08x\n", result);
	}
	ret = sceKernelUnlockFastMutex(&s_npdrm_api_mutex);
	if (ret >= 0) {
		signalRet = sceKernelSignalSema(s_self_api_sema, 1);
		if (signalRet >= 0)
			return result;
		SceKernelSuspendForDriver_2BB92967(0);
		return signalRet;
	}
signal:
	signalRet = sceKernelSignalSema(s_self_api_sema, 1);
	if (signalRet < 0) {
		SceKernelSuspendForDriver_2BB92967(0);
		return signalRet;
	}
resume:
	SceKernelSuspendForDriver_2BB92967(0);
	return ret;
}

int sceSblAuthMgrClose(int handle)
{
	int ret, stopRet, signalRet;

	if (handle != AUTHMGR_HANDLE)
		return SCE_SBL_ERROR_AM_EBADF;
	ret = lockFastMutex(&s_npdrm_api_mutex);
	if (ret >= 0) {
		stopRet = authMgrStopSM(s_req_id);
		if (stopRet == 0) {
			s_req_id = AUTHMGR_NO_REQUEST;
			s_self_handle = -1;
		} else {
			SCE_DBG_PRINTF(0xb, 483, "ERROR: authMgrStopSM ret = 0x%08x\n", stopRet);
		}
		ret = sceKernelUnlockFastMutex(&s_npdrm_api_mutex);
		if (ret > 0)
			ret = 0;
	}
	signalRet = sceKernelSignalSema(s_self_api_sema, 1);
	if (signalRet < 0) {
		SceKernelSuspendForDriver_2BB92967(0);
		return signalRet;
	}
	SceKernelSuspendForDriver_2BB92967(0);
	return ret;
}

int sceSblAuthMgrAuthHeader(int handle, const void *header, SceSize size, SelfAuthRequest *auth)
{
	AuthMgrAuthHeaderPacket *packet;
	SceSblUtMgrTrilithiumBuffer trilithium;
	int ret, count, mode;

	if (handle != AUTHMGR_HANDLE)
		return SCE_SBL_ERROR_AM_EBADF;
	if (header == NULL || auth == NULL) {
		SCE_DBG_PRINTF(0xb, 565, "sceSblAuthMgrAuthHeader SCE_SBL_ERROR_AM_EINVAL\n");
		SCE_DBG_PRINTF(0xb, 566, "\n self_header_addr : %p\n", header);
		SCE_DBG_PRINTF(0xb, 567, "\n auth_info : %p\n", auth);
		return SCE_SBL_ERROR_AM_EINVAL;
	}
	packet = &s_self_packet->authHeader;
	memset(packet, 0, sizeof(*packet));
	count = authMgrSetupSrcPARanges(header, size);
	if (count < 0)
		return count;
	mode = SceSysrootForKernel_377895EB();
	if (mode == 1 || mode == 2 || mode == -1)
		auth->selfType = (auth->selfType & ~0xf00) | 0x100;
	if (auth->mediaType - 13 <= 1)
		auth->module.spfsoInfo = (SceUInt64)*(SceUInt32 *)&s_spfso_auth[0xc] << 16;
	else
		auth->module.spfsoInfo = 0;
	memcpy(&packet->auth, auth, sizeof(*auth));
	packet->rangeCount = count;
	packet->rangesPA = s_src_pa_ranges_pa;
	sceKernelDcacheCleanRange(header, size);
	ret = authMgrSendPacketCore(&packet->header, sizeof(*packet), AUTHMGR_CMD_AUTH_HEADER);
	if (ret < 0)
		return ret;
	memcpy(&auth->module, &packet->auth.module, sizeof(auth->module));
	if ((auth->module.programAuthorityId & 0xfff0000000000000ULL) == 0x2f00000000000000ULL) {
		/* an fself: the utoken's authentication replaces the one it carries */
		ret = SceSysrootForDriver_D75D4F37(&trilithium);
		if (ret != (int)0x800f1a02) {
			if (ret == 0) {
				auth->module.programAuthorityId = trilithium.programAuthorityId;
				__builtin_memcpy(&auth->module.capability, &trilithium.capability, sizeof(trilithium.capability));
				__builtin_memcpy(&auth->module.attribute, &trilithium.attribute, sizeof(trilithium.attribute));
				__builtin_memcpy(&auth->module.sharedSecret0, &trilithium.sharedSecret0, sizeof(trilithium.sharedSecret0));
			} else if (ret < 0) {
				SCE_DBG_PRINTF(0xb, 659, "[ERROR] overwrite fself authinfo\n");
				return ret;
			}
		}
	}
	return 0;
}

/* vitasdk: ksceSblAuthMgrSetupAuthSegment */
int SceSblAuthMgrForKernel_89CCDA2C(int handle, SceUInt32 segment)
{
	AuthMgrSegmentPacket *packet;
	int ret;

	if (handle != AUTHMGR_HANDLE)
		return SCE_SBL_ERROR_AM_EBADF;
	packet = &s_self_packet->segment;
	memset(packet, 0, sizeof(*packet));
	packet->segment = segment;
	ret = authMgrSendPacketCore(&packet->header, sizeof(*packet), AUTHMGR_CMD_SETUP_SEGMENT);
	if (ret < 0)
		return ret;
	return packet->result;
}

/* vitasdk: ksceSblAuthMgrLoadBlock */
int SceSblAuthMgrForKernel_BC422443(int handle, void *buffer, SceSize size, SceUInt32 reservedLow,
				     int reservedHigh)
{
	AuthMgrBufferPacket *packet;
	SceSize cacheSize;
	int ret, count;

	if (handle != AUTHMGR_HANDLE)
		return SCE_SBL_ERROR_AM_EBADF;
	if (buffer == NULL || size == 0 || ((uintptr_t)buffer & 0x1f))
		return SCE_SBL_ERROR_AM_EINVAL;
	count = authMgrSetupSrcPARanges(buffer, size);
	if (count < 0)
		return count;
	packet = &s_self_packet->block;
	cacheSize = (size + 0x3f) & ~0x3f;
	memset(packet, 0, sizeof(*packet));
	packet->srcRangeCount = count;
	packet->srcRangesPA = s_src_pa_ranges_pa;
	packet->dstRangeCount = count;
	packet->dstRangesPA = s_src_pa_ranges_pa;
	sceKernelDcacheCleanRange(buffer, cacheSize);
	ret = authMgrSendPacketCore(&packet->header, sizeof(*packet), AUTHMGR_CMD_AUTH_SEGMENT);
	if (ret < 0)
		return ret;
	sceKernelDcacheInvalidateRange(buffer, cacheSize);
	return 0;
}

int sceSblAuthMgrGetEKc(void *buffer, SceSize size, SceUInt32 type)
{
	AuthMgrKeyPacket *packet;
	int ret, unlockRet;

	if (buffer == NULL)
		return SCE_SBL_ERROR_AM_EINVAL;
	if (type > 2 || (size & 0xf) || size - 1 > 0xff)
		return SCE_SBL_ERROR_AM_EINVAL;
	ret = lockFastMutex(&s_npdrm_api_mutex);
	if (ret < 0)
		return ret;
	packet = s_key_packet;
	memcpy(packet->data, buffer, size);
	packet->size = size;
	packet->type = type;
	packet->slot = 0;
	packet->reserved = 0;
	ret = authMgrSendPacket(&packet->header, sizeof(*packet), AUTHMGR_CMD_GET_EKC);
	if (ret >= 0) {
		if (packet->size > sizeof(packet->data)) {
			ret = SCE_SBL_ERROR_AM_EINVAL;
		} else {
			ret = 0;
			memcpy(buffer, packet->data, packet->size);
		}
	}
	unlockRet = sceKernelUnlockFastMutex(&s_npdrm_api_mutex);
	if (unlockRet < 0)
		return unlockRet;
	return ret;
}

int sceSblAuthMgrSetDmac5Key(const void *key, SceSize size, int slot, int keyId)
{
	AuthMgrKeyPacket *packet;
	int ret, unlockRet;

	if (key == NULL || (keyId != 0 && keyId != 0x10000 && keyId != 0x10001 &&
	    keyId != 0x20000 && keyId != 0x20001) || (size & 0xf) || size - 1 >= 0x20)
		return SCE_SBL_ERROR_AM_EINVAL;
	ret = lockFastMutex(&s_npdrm_api_mutex);
	if (ret < 0)
		return ret;
	packet = s_key_packet;
	memcpy(packet->data, key, size);
	packet->size = size;
	packet->type = keyId;
	packet->slot = slot;
	packet->reserved = 0;
	ret = authMgrSendPacket(&packet->header, sizeof(*packet), AUTHMGR_CMD_SET_DMAC5_KEY);
	unlockRet = sceKernelUnlockFastMutex(&s_npdrm_api_mutex);
	if (unlockRet < 0)
		return unlockRet;
	if (ret > 0)
		ret = 0;
	return ret;
}

int sceSblAuthMgrClearDmac5Key(int slot, int value)
{
	AuthMgrKeyPacket *packet;
	int ret, unlockRet;

	if (value != 0)
		return SCE_SBL_ERROR_AM_EINVAL;
	ret = lockFastMutex(&s_npdrm_api_mutex);
	if (ret < 0)
		return ret;
	packet = s_key_packet;
	memset(packet->data, 0, sizeof(packet->data));
	packet->size = 0;
	packet->type = 0;
	packet->reserved = 0;
	packet->slot = slot;
	ret = authMgrSendPacket(&packet->header, sizeof(*packet), AUTHMGR_CMD_CLEAR_DMAC5_KEY);
	unlockRet = sceKernelUnlockFastMutex(&s_npdrm_api_mutex);
	if (unlockRet < 0)
		return unlockRet;
	if (ret > 0)
		ret = 0;
	return ret;
}

/* a buffer's one physical range; an empty buffer has none */
static inline int authMgrSetupPARange(const void *addr, SceSize size, SceKernelPARange *range)
{
	SceKernelVARange va;

	if (range == NULL)
		return SCE_SBL_ERROR_AM_EINVAL;
	if (size == 0)
		return 0;
	va.addr = (uintptr_t)addr;
	va.size = size;
	return SceSysmemForDriver_AE36C775(&va, range);
}

int sceSblAuthMgrDecBindData(void *input, SceSize inputSize, void *output, SceSize outputSize,
	int flags)
{
	AuthMgrBindPacket *packet;
	int ret, unlockRet;

	if (input == NULL || output == NULL)
		return SCE_SBL_ERROR_AM_EINVAL;
	if (flags != 0 || outputSize - 1 >= 0x1000)
		return SCE_SBL_ERROR_AM_EINVAL;
	if ((((uintptr_t)input | (uintptr_t)output) & 0x3f) || ((inputSize | outputSize) & 0xf) ||
	    inputSize > 0x1000)
		return SCE_SBL_ERROR_AM_EINVAL;
	ret = lockFastMutex(&s_npdrm_api_mutex);
	if (ret < 0)
		return ret;
	packet = s_bind_packet;
	memset(packet, 0, sizeof(*packet));
	ret = authMgrSetupPARange(input, inputSize, &packet->input);
	if (ret >= 0) {
		ret = authMgrSetupPARange(output, outputSize, &packet->output);
		if (ret >= 0) {
			packet->flags = 0;
			sceKernelDcacheCleanRange(input, (inputSize + 0x3f) & ~0x3f);
			sceKernelDcacheCleanRange(output, (outputSize + 0x3f) & ~0x3f);
			ret = authMgrSendPacket(&packet->header, sizeof(*packet), AUTHMGR_CMD_DEC_BIND_DATA);
			if (ret >= 0)
				sceKernelDcacheInvalidateRange(input, (inputSize + 0x3f) & ~0x3f);
		}
	}
	unlockRet = sceKernelUnlockFastMutex(&s_npdrm_api_mutex);
	if (unlockRet < 0)
		return unlockRet;
	return ret;
}

/* has the module check an SPFSO file, and keeps its authentication data */
int sceSblAuthMgrVerifySpsfo(SceSblSpfsoCtx *ctx)
{
	AuthMgrBufferPacket *packet;
	SceUInt64 offset;
	int ret, result;

	if (ctx == NULL || ctx->base == NULL || ctx->size > 0x8000 || ((uintptr_t)ctx->base & 0x1f))
		return SCE_SBL_ERROR_AM_EINVAL;
	ret = authMgrWaitSelfSemaphore();
	if (ret < 0)
		return ret;
	ret = lockFastMutex(&s_npdrm_api_mutex);
	if (ret < 0)
		goto signal;
	result = authMgrSetupSrcPARanges(ctx->base, ctx->size);
	if (result < 0) {
		SCE_DBG_ASSERT(0, 1984);
	} else {
		packet = s_spfso_packet;
		memset(packet, 0, sizeof(*packet));
		packet->srcRangeCount = result;
		packet->srcRangesPA = s_src_pa_ranges_pa;
		sceKernelDcacheCleanRange(ctx->base, (ctx->size + 0x3f) & ~0x3f);
		result = authMgrSendPacket(&packet->header, sizeof(*packet), AUTHMGR_CMD_VERIFY_SPFSO);
		if (result >= 0) {
			sceKernelDcacheInvalidateRange(ctx->base, (ctx->size + 0x3f) & ~0x3f);
			if (ctx->base == NULL || ctx->size == 0) {
				result = SCE_SBL_ERROR_AM_EINVAL;
			} else {
				offset = *(SceUInt64 *)((SceUInt8 *)ctx->base + 0x10);
				if (offset < 0x30 || ctx->size < AUTHMGR_SPFSO_SIZE ||
				    (SceUInt64)ctx->size - AUTHMGR_SPFSO_SIZE < offset) {
					result = SCE_SBL_ERROR_AM_INVALID_SPFSO;
				} else {
					result = 0;
					memcpy(s_spfso_auth, (SceUInt8 *)ctx->base + (SceUInt32)offset,
					       AUTHMGR_SPFSO_SIZE);
				}
			}
		}
	}
	ret = sceKernelUnlockFastMutex(&s_npdrm_api_mutex);
	if (ret >= 0) {
		ret = sceKernelSignalSema(s_self_api_sema, 1);
		if (ret < 0)
			return ret;
		return result;
	}
signal:
	result = sceKernelSignalSema(s_self_api_sema, 1);
	if (result < 0)
		return result;
	return ret;
}

/* vitasdk: ksceSblAuthMgrCompareSwVersion */
int SceSblAuthMgrForKernel_ABAB8466(SceUInt32 version)
{
	if (version != 0xffffffff && (version & 0xfffff000) > (FW_SDK_VERSION & 0xfffff000))
		return SCE_SBL_ERROR_AM_NEWER_SYSTEM_VERSION;
	return 0;
}

int SceSblAuthMgrForKernel_2A83A012(void)
{
	return 0;
}
