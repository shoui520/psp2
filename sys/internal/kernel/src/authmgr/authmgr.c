/*
 * SceSblAuthMgr: SELF authentication and secure-module command packets.
 * Native debug names and source lines are retained across firmware variants.
 * PLACEHOLDER: secure-world payload flags not named by native records remain unk.
 */
#include <stdint.h>
#include <stddef.h>
#include <sysmem/suspend.h>
#include <psp2kern/kernel/sysclib.h>
#include <threadmgr/threadmgr.h>
#include <threadmgr/fast_mutex_object.h>
#include <sysmem/cpu.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysroot.h>
#include <sysmem/debug.h>

#define AM_EINVAL ((int)0x800f0516)
#define AM_EHANDLE ((int)0x800f0509)
#define AM_ENOMEM ((int)0x800f050c)
#define AM_EPHYSICAL ((int)0x800f0528)
#define AM_NO_REQUEST 0x0fffffff
#define NOINLINE __attribute__((noinline))

#if FW_DEBUG
#define AUTH_DBG_PRINTF(function, line, format, ...) \
	SceDebugForDriver_D9703808(1, 0xb, SCE_DBGINFO_FUNC(line, function), format, ##__VA_ARGS__)
#define AUTH_DBG_ASSERT(function, condition, line) \
	SceDebugForDriver_35A35322(condition, SCE_DBGINFO_FUNC(line, function), __builtin_return_address(0))
#else
#define AUTH_DBG_PRINTF(function, line, format, ...) do { } while (0)
#define AUTH_DBG_ASSERT(function, condition, line) do { } while (0)
#endif

#define AUTH_PANIC(debugLine, releaseLine) \
	SceDebugForDriver_391B5B74(SCE_DBGINFO(FW_DEBUG ? debugLine : releaseLine), __builtin_return_address(0))

/* Module-local ABI declarations while shared dependency headers are frozen. */
typedef void (*AuthCryHandler)(SceUID, int, void *, uint32_t, uint32_t);
int sceSblSmSchedProxyInvoke(uint32_t, uint32_t, uint32_t,
	const uint32_t *, const SelfAuthRequest *, SceUID *);
int sceSblSmSchedProxyWait(SceUID id, uint32_t *result);
int sceSblSmSchedProxyGetStatus(SceUID id, uint32_t *result);
int SceSblSmSchedProxyForKernel_723B382F(SceUID id, int channel, uint32_t value);
int sceSblSmSchedProxyWriteCry2Arm(SceUID id, int channel, uint32_t value);
int sceSblSmSchedProxyReadCry2Arm(SceUID id, int channel, uint32_t *value);
int SceSblSmSchedProxyForKernel_8B84AC2A(SceUID id, int channel,
	AuthCryHandler handler, void *argument);
int SceSblSmSchedProxyForKernel_85EDA5FC(SceUID id, int channel);
extern uint32_t __stack_chk_guard;

/* Uncached wire packets: the first 0x40 bytes are the native command header. */
struct AuthPacket {
	uint32_t size;
	uint32_t command;
	volatile int result;
	uint32_t reserved[13];			/* rest of the 0x40-byte header */
	uint8_t payload[];
};
struct AuthMgrState {
#if FW_DEBUG
	SceKernelFastMutex sendMutex;
	uint32_t sourcePhysicalAddress;
	struct AuthPacket *arena;
	struct AuthPacket *stopPacket;
	SceUID selfSemaphore;
	struct AuthPacket *keyPacket;
	struct AuthPacket *bindPacket;
	uint8_t cachedSpsfoAuth[0x200];
	struct AuthPacket *verifyPacket;
	uint8_t reserved[0x24];
	SceKernelPARange sourceRanges[16];
	volatile SceUID requestId;
	SceUID eventFlag;
	int selfHandle;
	SceKernelFastMutex npdrmMutex;
#else
	volatile SceUID requestId;
	uint8_t reserved[0x3c];
	SceKernelPARange sourceRanges[16];
	SceKernelFastMutex npdrmMutex;
	SceUID eventFlag;
	struct AuthPacket *bindPacket;
	struct AuthPacket *verifyPacket;
	int selfHandle;
	struct AuthPacket *arena;
	struct AuthPacket *stopPacket;
	uint8_t cachedSpsfoAuth[0x200];
	struct AuthPacket *keyPacket;
	SceKernelFastMutex sendMutex;
	uint32_t sourcePhysicalAddress;
	SceUID selfSemaphore;
#endif
};
static struct AuthMgrState s_state;
_Static_assert(offsetof(struct AuthPacket, payload) == 0x40, "packet header");
#if FW_DEBUG
_Static_assert(sizeof(s_state) == 0x34c, "debug state");
_Static_assert(offsetof(struct AuthMgrState, requestId) == 0x300, "request offset");
#else
_Static_assert(sizeof(s_state) == 0x364, "release state");
#endif

static inline int ErrorOnly(int result)
{
	return result < 0 ? result : 0;
}
static inline uint32_t RoundCacheSize(uint32_t size)
{
	return (size + 0x3f) & ~0x3fU;
}
static inline void CheckStackGuard(uint32_t guard)
{
	if (guard != __stack_chk_guard)
		__stack_chk_fail();
}
static inline uint32_t *PacketWords(struct AuthPacket *packet)
{
	return (uint32_t *)(void *)packet;
}
static NOINLINE int authMgrWaitSelfSemaphore(void)
{
	return ErrorOnly(sceKernelWaitSema(s_state.selfSemaphore, 1, NULL));
}
static NOINLINE int authMgrIntrHandlerforInvoke(SceUID id, int channel,
	void *argument, uint32_t status, uint32_t result)
{
	uint32_t bits;

	(void)argument;
	if (channel != 0) {
		SCE_DBG_PRINTF(0xb, 1270, "ERROR: invalid mailbox id [%d]\n", channel);
		AUTH_PANIC(1269, 1270);
	}
	if (id != s_state.requestId) {
		SCE_DBG_PRINTF(0xb, 1279, "ERROR: req_id [%d] != s_req_id [%d]\n",
			id, s_state.requestId);
		if (s_state.requestId == AM_NO_REQUEST)
			AUTH_PANIC(1283, 1283);
		AUTH_PANIC(1273, 1279);
	}
	bits = (status & ~8U) == 2 ? 1 : (result << 16) | 1;
	sceKernelSetEventFlag(s_state.eventFlag, bits);
	return 0;
}
static NOINLINE int authMgrWaitSM(void)
{
	uint32_t guard = __stack_chk_guard;
	uint32_t bits;
	int result;

	sceKernelSysrootDisableAutoClockDown();
	result = sceKernelWaitEventFlag(s_state.eventFlag, 1, 5, &bits, NULL);
	sceKernelSysrootEnableAutoClockDown();
	if (result < 0) {
		SCE_DBG_PRINTF(0xb, 976, "ERROR: sceKernelWaitEventFlag ret = 0x%08x\n", result);
	} else if ((bits >> 16) != 0) {
		SCE_DBG_PRINTF(0xb, 984, "ERROR: authMgr status = 0x%08x\n", bits >> 16);
		result = (int)0x800f052a;
	} else {
		result = 0;
	}
	CheckStackGuard(guard);
	return result;
}
static NOINLINE int authMgrSetupSrcPARanges(const void *address, uint32_t size)
{
	uint32_t guard = __stack_chk_guard;
	SceKernelVARange range = { .addr = (uintptr_t)address, .size = size };
	SceKernelPAVector vector;
	int result;

	vector.size = sizeof(vector);
	vector.capacity = 16;
	vector.pRanges = s_state.sourceRanges;
	result = sceKernelVARangeToPAVector(&range, &vector);
	if (result < 0) {
		SCE_DBG_PRINTF(0xb, 1113, "ERROR: authMgrSetupSrcPARanges failed ! ret = 0x%08x\n", result);
		result = AM_EPHYSICAL;
	} else {
		SceCpuForDriver_103872A5(s_state.sourceRanges,
			(vector.total * 8 + 0x1f) & ~0x1fU);
		result = vector.total;
	}
	CheckStackGuard(guard);
	return result;
}
static NOINLINE void authMgrIntrHandler(SceUID id, int channel,
	void *argument, uint32_t status, uint32_t resultWord)
{
	uint32_t guard = __stack_chk_guard;
	uint32_t value = 0;
	int result;

	(void)argument;
	(void)status;
	(void)resultWord;
	if (id != s_state.requestId) {
		SCE_DBG_PRINTF(0xb, 1020, "ERROR: req_id [%d] != s_req_id [%d]\n", id, s_state.requestId);
		AUTH_PANIC(1019, 1020);
	}
	if (channel != 1) {
		SCE_DBG_PRINTF(0xb, 1024, "ERROR: invalid mailbox id [0x%08x]\n", channel);
		AUTH_PANIC(1023, 1024);
	}
	result = sceSblSmSchedProxyReadCry2Arm(id, 1, &value);
	if (result != 0) {
		SCE_DBG_PRINTF(0xb, 1052, "ERRORR: sceSblSmSchedProxyReadCry2Arm ret = 0x%08x\n", result);
	} else {
		result = sceSblSmSchedProxyWriteCry2Arm(id, 1, value);
		if (result != 0) {
			SCE_DBG_PRINTF(0xb, 1047, "ERROR: sceSblSmSchedProxyWriteCry2Arm ret = 0x%08x\n", result);
		} else {
			if (!(value & 1)) {
				SCE_DBG_PRINTF(0xb, 1065, "ERROR: Intr Handler : mailval = 0x%08x\n", value);
				AUTH_PANIC(1064, 1065);
			}
			sceKernelSetEventFlag(s_state.eventFlag, value == 1 ? 1 : 0x20001);
		}
	}
	CheckStackGuard(guard);
}
static NOINLINE int authMgrSendRequest(SceUID id, struct AuthPacket *packet)
{
	uint32_t guard = __stack_chk_guard;
	uint32_t physicalAddress;
	int result;

	packet->result = -1;
	__asm__ volatile("dmb sy" : : : "memory");
	(void)packet->result;
	__asm__ volatile("dsb sy" : : : "memory");
	result = SceSysmemForDriver_8D160E65(packet, &physicalAddress);
	if (result < 0) {
		SCE_DBG_PRINTF(0xb, 1250, "ERROR: sceKernelVAtoPA ret = 0x%08x\n", result);
		result = AM_EPHYSICAL;
	} else {
		result = SceSblSmSchedProxyForKernel_723B382F(id, 1, physicalAddress | 1);
	}
	CheckStackGuard(guard);
	return result;
}
static NOINLINE int authMgrSendPacketCore(struct AuthPacket *packet,
	uint32_t size, uint32_t command)
{
	int result, releaseResult, unlockResult;

	if (packet == NULL)
		return AM_EINVAL;
	packet->size = size;
	packet->command = (command << 16) | 1;
	result = lockFastMutex(&s_state.sendMutex);
	if (result < 0)
		return result;
	result = SceSblSmSchedProxyForKernel_8B84AC2A(s_state.requestId, 1,
		authMgrIntrHandler, NULL);
	if (result < 0) {
		SCE_DBG_PRINTF(0xb, 1562, "ERROR: authMgrSetupIntrHandler ret = 0x%08x\n", result);
	} else {
		result = authMgrSendRequest(s_state.requestId, packet);
		if (result < 0) {
			SCE_DBG_PRINTF(0xb, 1571, "ERROR: authMgrSendRequest ret = 0x%08x\n", result);
		} else {
			result = authMgrWaitSM();
			if (result == 0) {
				result = packet->result;
				if (result == -1) {
					SCE_DBG_PRINTF(0xb, 1588, "ERROR: return value is not updated.\n");
					AUTH_PANIC(1587, 1588);
				}
			}
		}
	}
	releaseResult = SceSblSmSchedProxyForKernel_85EDA5FC(s_state.requestId, 1);
	if (releaseResult != 0) {
		SCE_DBG_PRINTF(0xb, 1599, "ERROR: ReleaseHandler ret = 0x%08x\n", releaseResult);
		result = releaseResult;
	}
	unlockResult = sceKernelUnlockFastMutex(&s_state.sendMutex);
	return unlockResult < 0 ? unlockResult : result;
}
static NOINLINE int authMgrStopSM(SceUID id)
{
	uint32_t guard = __stack_chk_guard;
	uint32_t status[2];
	struct AuthPacket *packet = s_state.stopPacket;
	int result;

	memset(packet, 0, 0x50);
	packet->size = 0x50;
	packet->command = UINT32_MAX;
	result = authMgrSendRequest(id, packet);
	if (result == 0) {
		result = sceSblSmSchedProxyWait(id, status);
		if (result != 0) {
			SCE_DBG_PRINTF(0xb, 1207, "ERROR: sceSblSmSchedProxyWait ret = 0x%08x\n", result);
		} else if (status[1] != 3) {
			SCE_DBG_PRINTF(0xb, 1155, "warning: SM status is not exit [0x%08x]\n", status[1]);
		}
	} else if (result == (int)0x800f0429) {
		result = sceSblSmSchedProxyGetStatus(id, status);
		if (result != 0) {
			SCE_DBG_PRINTF(0xb, 1197, "ERROR: sceSblSmSchedProxyGetStatus ret = 0x%08x\n", result);
		} else if (status[1] != 10 && status[1] != 3) {
			result = (int)0x800f0529;
		}
	} else {
		SCE_DBG_PRINTF(0xb, 1172, "ERROR: authMgrSendRequest ret = 0x%08x\n", result);
	}
	CheckStackGuard(guard);
	return result;
}
static NOINLINE int authMgrInvokeSM(void)
{
	uint32_t guard = __stack_chk_guard;
	uint8_t *bootParameters = SceSysrootForKernel_9DB56D1F();
	SelfAuthRequest auth;
	uint32_t *words = (uint32_t *)(void *)&auth;
	int result;

	memset(&auth, 0, sizeof(auth));
	memset(&auth.caller, 0, sizeof(auth.caller));
	auth.selfType = 0x1002;
	words[2] = 1;
	words[3] = 0x28080000;
	words[6] = 0x80;
	words[7] = 0xf000c0;
	words[8] = 0;
	words[9] = UINT32_MAX;
	words[14] = 0x03800980;
	words[15] = 0x00c30000;
	words[16] = 0x09800000;
	words[17] = 0x80;
	words[20] = 0;
	words[21] = UINT32_MAX;
	auth.mediaType = (*(uint32_t *)(void *)(bootParameters + 0x6c) & 0x40) ? 1 : 10;
	result = sceSblSmSchedProxyInvoke(0, 0, 0, NULL, &auth, (SceUID *)&s_state.requestId);
	if (result != 0) {
		SCE_DBG_PRINTF(0xb, 909, "ERROR: sceSblSmSchedProxyInvoke ret = 0x%08x\n", result);
	} else {
		result = SceSblSmSchedProxyForKernel_8B84AC2A(s_state.requestId, 0,
			(AuthCryHandler)authMgrIntrHandlerforInvoke, NULL);
		if (result != 0) {
			SCE_DBG_PRINTF(0xb, 920, "ERROR: authMgrSetupIntrHandler ret = 0x%08x\n", result);
		} else {
			result = authMgrWaitSM();
			if (result != 0) {
				SCE_DBG_PRINTF(0xb, 933, "ERROR: Invoke SM ret = 0x%08x\n", result);
			} else {
				result = SceSblSmSchedProxyForKernel_85EDA5FC(s_state.requestId, 0);
				if (result != 0)
					SCE_DBG_PRINTF(0xb, 942, "ERROR: authMgrReleaseHandler err = 0x%08x\n", result);
			}
		}
	}
	if (result < 0)
		s_state.requestId = AM_NO_REQUEST;
	CheckStackGuard(guard);
	return result;
}
static NOINLINE int authMgrSendPacket(struct AuthPacket *packet,
	uint32_t size, uint32_t command)
{
	SceUID previousId;
	int result, stopResult;

	SceKernelSuspendForDriver_4DF40893(0);
	previousId = s_state.requestId;
	if (previousId == AM_NO_REQUEST) {
		result = authMgrInvokeSM();
		if (result < 0) {
			SCE_DBG_PRINTF(0xb, 1470, "ERROR: Invoke sm fail ret = 0x%08x\n", result);
			previousId = s_state.requestId;
		} else {
			result = ErrorOnly(authMgrSendPacketCore(packet, size, command));
			stopResult = authMgrStopSM(s_state.requestId);
			if (stopResult < 0) {
				SCE_DBG_PRINTF(0xb, 1493, ">>> Stop SM fail ret = 0x%08x\n", stopResult);
				SceKernelSuspendForDriver_2BB92967(0);
				return stopResult;
			}
		}
	} else {
		result = ErrorOnly(authMgrSendPacketCore(packet, size, command));
		previousId = s_state.requestId;
	}
	s_state.requestId = previousId;
	SceKernelSuspendForDriver_2BB92967(0);
	return result;
}
static NOINLINE int sceSblAuthMgrStart(void)
{
	uint32_t guard = __stack_chk_guard;
	uint32_t option[5] = { 0x14, 0, 0x40, 0, 0 };
	uint8_t *arena;
	int result;

	s_state.selfHandle = -1;
	s_state.requestId = AM_NO_REQUEST;
	result = SceSysmemForDriver_8D160E65(s_state.sourceRanges, &s_state.sourcePhysicalAddress);
	if (result < 0) {
		SCE_DBG_PRINTF(0xb, 239, "ERROR: sceKernelVAtoPA ret = 0x%08x\n", result);
		result = AM_EPHYSICAL;
	} else {
		arena = SceSysmemForDriver_0B4ED16A(0x408, option);
		if (arena == NULL) {
			result = AM_ENOMEM;
		} else {
			s_state.arena = (void *)arena;
			s_state.stopPacket = (void *)(arena + 0x1c0);
			s_state.keyPacket = (void *)(arena + 0x210);
			s_state.bindPacket = (void *)(arena + 0x360);
			s_state.verifyPacket = (void *)(arena + 0x3b8);
			result = 0;
		}
	}
	CheckStackGuard(guard);
	return result;
}
int module_start(SceSize args, const void *argument)
{
	int result;

	(void)args;
	(void)argument;
	sceSblAuthMgrStart();
	s_state.eventFlag = sceKernelCreateEventFlag("SceSblAuthMgr", 0, 0, NULL);
	if (s_state.eventFlag < 0) {
		AUTH_DBG_PRINTF("sceSblAuthMgrModuleStart", 283, "ERROR: sceKernelCreateEventFlag fail\n");
		return AM_ENOMEM;
	}
	result = sceKernelInitializeFastMutex(&s_state.sendMutex, "SceSblAuthMgrSendCmd", 0, NULL);
	if (result < 0) {
		AUTH_DBG_PRINTF("sceSblAuthMgrModuleStart", 291, "ERROR: sceKernelInitializeFastMutex ret = 0x%08x\n", result);
		return result;
	}
	result = sceKernelInitializeFastMutex(&s_state.npdrmMutex, "SceSblAuthMgrNpdrmApi", 0, NULL);
	if (result < 0) {
		AUTH_DBG_PRINTF("sceSblAuthMgrModuleStart", 299, "ERROR: sceKernelInitializeFastMutex ret = 0x%08x\n", result);
		return result;
	}
	s_state.selfSemaphore = sceKernelCreateSema("SceSblAuthMgrSelfApi", 0, 1, 1, NULL);
	if (s_state.selfSemaphore < 0) {
		AUTH_DBG_PRINTF("sceSblAuthMgrModuleStart", 308, "ERROR: sceKernelCreateSema ret = 0x%08x\n", s_state.selfSemaphore);
		return result;
	}
	memset(s_state.cachedSpsfoAuth, 0, sizeof(s_state.cachedSpsfoAuth));
	return 0;
}
int sceSblAuthMgrOpen(int *handle)
{
	int result, operationResult, signalResult;

	if (handle == NULL)
		return AM_EINVAL;
	SceKernelSuspendForDriver_4DF40893(0);
	result = authMgrWaitSelfSemaphore();
	if (result >= 0) {
		result = lockFastMutex(&s_state.npdrmMutex);
		if (result >= 0) {
			if (s_state.selfHandle == 1) {
				operationResult = (int)0x800f0501;
			} else if (s_state.requestId == AM_NO_REQUEST) {
				operationResult = authMgrInvokeSM();
				if (operationResult == 0) {
					*handle = 1;
					s_state.selfHandle = 1;
					result = sceKernelUnlockFastMutex(&s_state.npdrmMutex);
					if (result >= 0)
						return 0;
					SceKernelSuspendForDriver_2BB92967(0);
					return result;
				}
				AUTH_DBG_PRINTF("sceSblAuthMgrOpen", 410, "ERROR: authMgrInvokeSM ret = 0x%08x\n", operationResult);
			} else {
				operationResult = AM_EINVAL;
			}
			result = sceKernelUnlockFastMutex(&s_state.npdrmMutex);
			if (result >= 0) {
				signalResult = sceKernelSignalSema(s_state.selfSemaphore, 1);
				if (signalResult >= 0)
					return operationResult;
				SceKernelSuspendForDriver_2BB92967(0);
				return signalResult;
			}
		}
		signalResult = sceKernelSignalSema(s_state.selfSemaphore, 1);
		if (signalResult < 0)
			result = signalResult;
	}
	SceKernelSuspendForDriver_2BB92967(0);
	return result;
}
int sceSblAuthMgrClose(int handle)
{
	int result, stopResult, signalResult;

	if (handle != 1)
		return AM_EHANDLE;
	result = lockFastMutex(&s_state.npdrmMutex);
	if (result >= 0) {
		stopResult = authMgrStopSM(s_state.requestId);
		if (stopResult == 0) {
			s_state.requestId = AM_NO_REQUEST;
			s_state.selfHandle = -1;
		} else {
			AUTH_DBG_PRINTF("sceSblAuthMgrClose", 483, "ERROR: authMgrStopSM ret = 0x%08x\n", stopResult);
		}
		result = ErrorOnly(sceKernelUnlockFastMutex(&s_state.npdrmMutex));
	}
	signalResult = sceKernelSignalSema(s_state.selfSemaphore, 1);
	SceKernelSuspendForDriver_2BB92967(0);
	return signalResult < 0 ? signalResult : result;
}
int sceSblAuthMgrAuthHeader(int handle, const void *header, uint32_t size, SelfAuthRequest *auth)
{
	uint32_t guard = __stack_chk_guard;
	struct AuthPacket *packet = s_state.arena;
	uint32_t *words = PacketWords(packet);
	uint32_t fselfAuth[22];
	uint32_t mode;
	int result;

	if (handle != 1) {
		result = AM_EHANDLE;
	} else if (auth == NULL || header == NULL) {
		AUTH_DBG_PRINTF("sceSblAuthMgrAuthHeader", 565, "sceSblAuthMgrAuthHeader SCE_SBL_ERROR_AM_EINVAL\n");
		AUTH_DBG_PRINTF("sceSblAuthMgrAuthHeader", 566, "\n self_header_addr : %p\n", header);
		AUTH_DBG_PRINTF("sceSblAuthMgrAuthHeader", 567, "\n auth_info : %p\n", auth);
		result = AM_EINVAL;
	} else {
		memset(packet, 0, 0x190);
		result = authMgrSetupSrcPARanges(header, size);
		if (result >= 0) {
			mode = SceSysrootForKernel_377895EB();
			if (mode == 1 || mode == 2 || mode == UINT32_MAX)
				auth->selfType = (auth->selfType & 0xfffff0ff) | 0x100;
			*(uint64_t *)(void *)auth->module.padding =
				auth->mediaType - 13 < 2 ?
				(uint64_t)*(uint32_t *)(void *)(s_state.cachedSpsfoAuth + 0xc) << 16 : 0;
			memcpy(packet->payload, auth, sizeof(*auth));
			words[0x170 / 4] = result;
			words[0x174 / 4] = s_state.sourcePhysicalAddress;
			SceCpuForDriver_103872A5((void *)header, size);
			result = authMgrSendPacketCore(packet, 0x190, 1);
			if (result >= 0) {
				memcpy(&auth->module, (uint8_t *)packet + 0xd8, sizeof(auth->module));
				if ((auth->module.programAuthorityId >> 32 & 0xfff00000) == 0x2f000000) {
					result = SceSysrootForDriver_D75D4F37(fselfAuth);
					if (result == 0) {
						/* Native overwrite preserves response.padding (license bits). */
						auth->module.programAuthorityId = (uint64_t)fselfAuth[0] | (uint64_t)fselfAuth[1] << 32;
						for (unsigned i = 0; i < 20; i++)
							((uint32_t *)(void *)auth->module.capability)[i] = fselfAuth[i + 2];
					} else if (result < 0 && result != (int)0x800f1a02) {
						AUTH_DBG_PRINTF("sceSblAuthMgrAuthHeader", 659, "[ERROR] overwrite fself authinfo\n");
						goto out;
					}
				}
				result = 0;
			}
		}
	}
out:
	CheckStackGuard(guard);
	return result;
}
int SceSblAuthMgrForKernel_89CCDA2C(int handle, uint32_t segmentIndex)
{
	struct AuthPacket *packet = s_state.arena;
	int result;

	if (handle != 1)
		return AM_EHANDLE;
	memset(packet, 0, 0x50);
	PacketWords(packet)[0x40 / 4] = segmentIndex;
	result = authMgrSendPacketCore(packet, 0x50, 2);
	return result < 0 ? result : (int)PacketWords(packet)[0x44 / 4];
}
int SceSblAuthMgrForKernel_BC422443(int handle, void *buffer, uint32_t size)
{
	struct AuthPacket *packet;
	uint32_t *words;
	int result;

	if (handle != 1)
		return AM_EHANDLE;
	if (buffer == NULL || size == 0 || ((uintptr_t)buffer & 0x1f))
		return AM_EINVAL;
	result = authMgrSetupSrcPARanges(buffer, size);
	if (result < 0)
		return result;
	packet = s_state.arena;
	words = PacketWords(packet);
	memset(packet, 0, 0x50);
	words[16] = words[18] = result;
	words[17] = words[19] = s_state.sourcePhysicalAddress;
	SceCpuForDriver_103872A5(buffer, RoundCacheSize(size));
	result = authMgrSendPacketCore(packet, 0x50, 3);
	if (result < 0)
		return result;
	SceCpuForDriver_02796361(buffer, RoundCacheSize(size));
	return 0;
}
int SceSblAuthMgrForDriver_868B9E9A(void *buffer, uint32_t size, uint32_t type)
{
	struct AuthPacket *packet;
	uint32_t *words;
	int result, unlockResult;

	if (buffer == NULL || type >= 3 || (size & 0xf) || size - 1 >= 0x100)
		return AM_EINVAL;
	result = lockFastMutex(&s_state.npdrmMutex);
	if (result < 0)
		return result;
	packet = s_state.keyPacket;
	words = PacketWords(packet);
	memcpy(packet->payload, buffer, size);
	words[80] = size;
	words[81] = type;
	words[82] = words[83] = 0;
	result = authMgrSendPacket(packet, 0x150, 4);
	if (result >= 0) {
		if (words[80] <= 0x100) {
			memcpy(buffer, packet->payload, words[80]);
			result = 0;
		} else {
			result = AM_EINVAL;
		}
	}
	unlockResult = sceKernelUnlockFastMutex(&s_state.npdrmMutex);
	return unlockResult < 0 ? unlockResult : result;
}
int SceSblAuthMgrForKernel_122ACDEA(const void *key, uint32_t size, int slot, int keyId)
{
	struct AuthPacket *packet;
	uint32_t *words;
	int result, unlockResult;

	if (key == NULL || (keyId != 0 && keyId != 0x10000 && keyId != 0x10001 &&
	    keyId != 0x20000 && keyId != 0x20001) || (size & 0xf) || size - 1 >= 0x20)
		return AM_EINVAL;
	result = lockFastMutex(&s_state.npdrmMutex);
	if (result < 0)
		return result;
	packet = s_state.keyPacket;
	words = PacketWords(packet);
	memcpy(packet->payload, key, size);
	words[80] = size;
	words[81] = keyId;
	words[82] = slot;
	words[83] = 0;
	result = authMgrSendPacket(packet, 0x150, 5);
	unlockResult = sceKernelUnlockFastMutex(&s_state.npdrmMutex);
	return unlockResult < 0 ? unlockResult : ErrorOnly(result);
}
int SceSblAuthMgrForKernel_F2BB723E(int slot, int value)
{
	struct AuthPacket *packet;
	uint32_t *words;
	int result, unlockResult;

	if (value != 0)
		return AM_EINVAL;
	result = lockFastMutex(&s_state.npdrmMutex);
	if (result < 0)
		return result;
	packet = s_state.keyPacket;
	words = PacketWords(packet);
	memset(packet->payload, 0, 0x100);
	words[80] = words[81] = words[83] = 0;
	words[82] = slot;
	result = authMgrSendPacket(packet, 0x150, 6);
	unlockResult = sceKernelUnlockFastMutex(&s_state.npdrmMutex);
	return unlockResult < 0 ? unlockResult : ErrorOnly(result);
}
int SceSblAuthMgrForDriver_41DAEA12(void *input, uint32_t inputSize,
	void *output, uint32_t outputSize, int flags)
{
	uint32_t guard = __stack_chk_guard;
	struct AuthPacket *packet;
	SceKernelVARange range;
	int result, unlockResult;

	if (input == NULL || output == NULL || outputSize - 1 >= 0x1000 || flags != 0 ||
	    (((uintptr_t)input | (uintptr_t)output) & 0x3f) ||
	    ((inputSize | outputSize) & 0xf) || inputSize > 0x1000) {
		result = AM_EINVAL;
		goto out;
	}
	result = lockFastMutex(&s_state.npdrmMutex);
	if (result < 0)
		goto out;
	packet = s_state.bindPacket;
	memset(packet, 0, 0x58);
	/* Explicit wrap checks retain the native pointer-addition branches. */
	if ((uint32_t)(uintptr_t)packet == 0xffffffc0) {
		result = AM_EINVAL;
		goto unlock;
	}
	if (inputSize != 0) {
		range.addr = (uintptr_t)input;
		range.size = inputSize;
		result = SceSysmemForDriver_AE36C775(&range, (void *)packet->payload);
		if (result < 0)
			goto unlock;
	}
	if ((uint32_t)(uintptr_t)packet == 0xffffffb8) {
		result = AM_EINVAL;
		goto unlock;
	}
	range.addr = (uintptr_t)output;
	range.size = outputSize;
	result = SceSysmemForDriver_AE36C775(&range, (void *)(packet->payload + 8));
	if (result >= 0) {
		PacketWords(packet)[20] = 0;
		SceCpuForDriver_103872A5(input, RoundCacheSize(inputSize));
		SceCpuForDriver_103872A5(output, RoundCacheSize(outputSize));
		result = authMgrSendPacket(packet, 0x58, 7);
		if (result >= 0)
			SceCpuForDriver_02796361(input, RoundCacheSize(inputSize));
	}
unlock:
	unlockResult = sceKernelUnlockFastMutex(&s_state.npdrmMutex);
	if (unlockResult < 0)
		result = unlockResult;
out:
	CheckStackGuard(guard);
	return result;
}
struct SpsfoContext {
	uint32_t reserved;
	void *buffer;
	uint32_t size;
};
int sceSblAuthMgrVerifySpsfo(struct SpsfoContext *context)
{
	struct AuthPacket *packet;
	uint32_t *words;
	uint64_t offset;
	int result, unlockResult, signalResult;

	if (context == NULL || context->buffer == NULL || context->size > 0x8000 ||
	    ((uintptr_t)context->buffer & 0x1f))
		return AM_EINVAL;
	result = authMgrWaitSelfSemaphore();
	if (result < 0)
		return result;
	result = lockFastMutex(&s_state.npdrmMutex);
	if (result < 0)
		goto signal;
	result = authMgrSetupSrcPARanges(context->buffer, context->size);
	if (result < 0) {
		AUTH_DBG_ASSERT("sceSblAuthMgrVerifySpsfo", 0, 1984);
	} else {
		packet = s_state.verifyPacket;
		words = PacketWords(packet);
		memset(packet, 0, 0x50);
		words[16] = result;
		words[17] = s_state.sourcePhysicalAddress;
		SceCpuForDriver_103872A5(context->buffer, RoundCacheSize(context->size));
		result = authMgrSendPacket(packet, 0x50, 8);
		if (result >= 0) {
			SceCpuForDriver_02796361(context->buffer, RoundCacheSize(context->size));
			if (context->buffer == NULL || context->size == 0) {
				result = AM_EINVAL;
			} else {
				offset = *(uint64_t *)(void *)((uint8_t *)context->buffer + 0x10);
				if (offset >= 0x30 && context->size >= 0x200 && offset <= context->size - 0x200) {
					memcpy(s_state.cachedSpsfoAuth, (uint8_t *)context->buffer + (uint32_t)offset, 0x200);
					result = 0;
				} else {
					result = (int)0x800f0524;
				}
			}
		}
	}
	unlockResult = sceKernelUnlockFastMutex(&s_state.npdrmMutex);
	if (unlockResult >= 0) {
		signalResult = sceKernelSignalSema(s_state.selfSemaphore, 1);
		return signalResult < 0 ? signalResult : result;
	}
	result = unlockResult;
signal:
	signalResult = sceKernelSignalSema(s_state.selfSemaphore, 1);
	return signalResult < 0 ? signalResult : result;
}
int SceSblAuthMgrForKernel_ABAB8466(uint32_t version)
{
	return version == UINT32_MAX || (version & 0xfffff000) <= (FW_SDK_VERSION & 0xfffff000) ? 0 : (int)0x800f0537;
}
int SceSblAuthMgrForKernel_2A83A012(void)
{
	/* Native body is exactly a constant-zero return; purpose is unknown. */
	return 0;
}
