/* SblSsMgr core (Sony's ss_mgr.c): module start, sysevents, the physical
 * range helper and the syscon NVS areas other modules may read and write. */
#include "ssmgr_internal.h"
#include <sysmem/sblaimgr.h>
#include <psp2kern/kernel/processmgr.h>
#include <sysmem/suspend.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysroot.h>
#include <sm_comm/sm_comm.h>
#include <sysmem/debug.h>
#include <string.h>
#include <threadmgr/threadmgr.h>

int ksceSblSmSchedProxyExecuteF00DCommand(int command, int a1, int a2, int a3);
const SceSelfAuthInfo ssPortabilityAuthInfo = {
	.program_authority_id = UINT64_C(0x2808000000000001),
	.capability = {
		0x80, 0, 0, 0, 0xc0, 0, 0xf0, 0,
		0, 0, 0, 0, 0xff, 0xff, 0xff, 0xff
	},
	.attribute = {
		0x80, 0x09, 0x80, 0x03, 0, 0, 0xc3, 0,
		0, 0, 0x80, 0x09, 0x80, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0xff, 0xff, 0xff, 0xff
	}
};
void *ksceSblSsMemset(void *dst, int c, SceSize size) __attribute__((noinline));

/* Portability service request, shared by its input and output. Name unknown. */
typedef struct SsPortabilityRequest {
	uint32_t keyType;
	ScePortabilityData input;
	ScePortabilityData output;
	uint32_t iv[4];
} SsPortabilityRequest;

_Static_assert(sizeof(SsPortabilityRequest) == 0x5c, "Portability request ABI");

/* Sysevent and start hooks of the DMAC5 and RNG parts. */
int dmac5ClearKeyring(void);	/* clears the cached DMAC5 keyring on resume */
int sceSblDmac5Init(void);
int sceSblRngInit(void);

#define SS_ERROR_INVALID_ARGUMENT	0x800F0016
#define SS_ERROR_NO_MEMORY		0x800F000C

#define SS_NVS_AREA_MAX			5
#define SS_NVS_CHUNK			0x10
#define SS_NVS_PROTECTED_END		0x400	/* below: not reachable through areas */
#define SS_NVS_RETAIL_END		0x760
#define SS_NVS_TOOL_END			0xb40

/* 4.00 is built with _FORTIFY_SOURCE: copies into stack buffers are checked */
#if FW_VERSION >= 0x04000000
#define ssMemcpyChk(dst, src, n) \
	__builtin___memcpy_chk(dst, src, n, __builtin_object_size(dst, 0))
#else
#define ssMemcpyChk(dst, src, n)	memcpy(dst, src, n)
#endif

typedef struct SsNvsArea {
	uint32_t offset;
	uint32_t size;
} SsNvsArea;

static const SsNvsArea ssNvsAreas[SS_NVS_AREA_MAX + 1] = {
	{ 0x4a4, 0x04 },
	{ 0x500, 0x01 },
	{ 0x482, 0x01 },
	{ 0x4e0, 0x20 },
	{ 0x483, 0x01 },
	{ 0x486, 0x01 },
};

#define SS_MGR_CORE(name) __attribute__((section(".ssMgrCore." #name)))

SS_MGR_CORE(SsMgrSysEventHandler)
static int SsMgrSysEventHandler(int resume, int eventid, void *args, void *opt)
{
	(void)args;
	(void)opt;

	switch (resume) {
	case 0:
		if (eventid == 0x200)
			return ksceSblSmSchedProxyExecuteF00DCommand(4, 0, 0, 0);
		return resume;
	case 1:
		if (eventid == 0x100000)
			return dmac5ClearKeyring();
		break;
	}
	return 0;
}

SS_MGR_CORE(module_start)
int module_start(SceSize args, void *argp)
{
	int ret;

	(void)args;
	(void)argp;

	ret = sceKernelRegisterSysEventHandler("SceSblSsMgr", SsMgrSysEventHandler, NULL);
	if (ret < 0)
		return ret;

	qafInit();
	aimgrInit();
	sceSblDmac5Init();
	sceSblRngInit();
	return 0;
}

/* Internal name inferred: the caller's SELF authentication info. */
SS_MGR_CORE(ssCheckProcessAuthInfo)
int ssCheckProcessAuthInfo(void)
{
	SceAuthInfo authInfo;
	int ret;

	ret = ksceKernelGetProcessSelfAuthInfo(sceKernelGetProcessId(), &authInfo.request);
	return ret & (ret >> 31);
}

SS_MGR_CORE(sceSblSsInfraAllocatePARangeVector)
int sceSblSsInfraAllocatePARangeVector(void *addr, SceSize size, SceUID heap,
				       SceKernelPAVector *vector)
{
	SceKernelVARange range = { (uint32_t)addr, size };
	SceAllocOpt opt = { .size = sizeof(opt), .align = 0x20 };
	SceKernelPARange *ranges;
	int ret;

	vector->size = sizeof(*vector);
	vector->capacity = 0;
	vector->pRanges = NULL;

	ret = sceKernelVARangeToPAVector(&range, vector);
	if (ret < 0)
		return ret;

	ranges = SceSysmemForDriver_49D4DD9B(heap,
			(vector->total * sizeof(*ranges) + 0x1f) & ~0x1f, &opt);
	if (!ranges)
		return SS_ERROR_NO_MEMORY;

	vector->pRanges = ranges;
	vector->capacity = vector->total;

	ret = sceKernelVARangeToPAVector(&range, vector);
	if (ret < 0)
		return ret;

	if (vector->nDataInVector != vector->total) {
		SCE_DBG_ASSERT(0, 145);
		return SS_ERROR_INVALID_ARGUMENT;
	}
	return 0;
}

/* Frees the ranges of sceSblSsInfraAllocatePARangeVector. */
SS_MGR_CORE(SceSblSsMgrForDriver_C38D0CEA)
int SceSblSsMgrForDriver_C38D0CEA(SceUID heap, SceKernelPAVector *vector)
{
	if (sceKernelFreeHeapMemory(heap, vector->pRanges) != 0)
		return SS_ERROR_INVALID_ARGUMENT;
	return 0;
}

SS_MGR_CORE(ksceSblSsGetNvsData)
int ksceSblSsGetNvsData(uint32_t area, void *buffer, SceSize size)
{
	uint8_t chunk[SS_NVS_CHUNK];
	uint32_t offset, remaining, length;
	int ret;

	if ((area > SS_NVS_AREA_MAX) | (buffer == NULL))
		return SS_ERROR_INVALID_ARGUMENT;

	offset = ssNvsAreas[area].offset;
	remaining = ssNvsAreas[area].size;
	if (offset < SS_NVS_PROTECTED_END) {
		SCE_DBG_ASSERT_FUNC(0, 188, "sceSblSsGetNvsData");
		return SS_ERROR_INVALID_ARGUMENT;
	}

	if (!SceSblAIMgrForDriver_274663A0() && !SceSblAIMgrForDriver_3B638885()) {
		if (offset >= SS_NVS_RETAIL_END)
			return SS_ERROR_INVALID_ARGUMENT;
	} else if (offset >= SS_NVS_TOOL_END) {
		return SS_ERROR_INVALID_ARGUMENT;
	}

	if (remaining > size)
		return SS_ERROR_INVALID_ARGUMENT;
	if (size != 4 && size - 1 > 1 && size != 8 && size != 0x10 && size != 0x20)
		return SS_ERROR_INVALID_ARGUMENT;

	SceKernelSuspendForDriver_4DF40893(0);
	while (remaining) {
		length = remaining < SS_NVS_CHUNK ? remaining : SS_NVS_CHUNK;
		ret = ksceSysconNvsReadData(offset, chunk, length);
		if (ret) {
			SceKernelSuspendForDriver_2BB92967(0);
			return ret;
		}
		offset += length;
		memcpy(buffer, chunk, length);
		remaining -= length;
		buffer = (uint8_t *)buffer + length;
	}
	SceKernelSuspendForDriver_2BB92967(0);
	return 0;
}

SS_MGR_CORE(ksceSblSsSetNvsData)
int ksceSblSsSetNvsData(uint32_t area, const void *buffer, SceSize size)
{
	uint8_t chunk[SS_NVS_CHUNK];
	uint32_t offset, remaining, length;
	int ret;

	if ((area > SS_NVS_AREA_MAX) | (buffer == NULL))
		return SS_ERROR_INVALID_ARGUMENT;

	offset = ssNvsAreas[area].offset;
	remaining = ssNvsAreas[area].size;
	if (offset < SS_NVS_PROTECTED_END) {
		SCE_DBG_ASSERT_FUNC(0, 260, "sceSblSsSetNvsData");
		return SS_ERROR_INVALID_ARGUMENT;
	}

	if (remaining > size)
		return SS_ERROR_INVALID_ARGUMENT;
	if (size != 4 && size - 1 > 1 && size != 8 && size != 0x10 && size != 0x20)
		return SS_ERROR_INVALID_ARGUMENT;

	SceKernelSuspendForDriver_4DF40893(0);
	while (remaining) {
		length = remaining < SS_NVS_CHUNK ? remaining : SS_NVS_CHUNK;
		ssMemcpyChk(chunk, buffer, length);
		ret = ksceSysconNvsWriteData(offset, chunk, length);
		if (ret) {
			SceKernelSuspendForDriver_2BB92967(0);
			return ret;
		}
		remaining -= length;
		buffer = (const uint8_t *)buffer + length;
		offset += length;
	}
	SceKernelSuspendForDriver_2BB92967(0);
	return 0;
}

SS_MGR_CORE(ksceSblSsEncryptWithPortability)
int ksceSblSsEncryptWithPortability(SceUInt32 key_type, void *iv,
				  ScePortabilityData *src, ScePortabilityData *dst)
{
	SceSize msgSize;
	int ret;

	if (!iv || !src)
		goto invalid;
	if (!dst)
		goto invalid;
	if (key_type > 20)
		goto invalid;
	msgSize = src->msg_size;
	if (msgSize != dst->msg_size || msgSize > 32 || (msgSize & 15))
		goto invalid;
	ret = ssCheckProcessAuthInfo();
	if (ret < 0)
		return ret;
	if (__builtin_expect(key_type == 0, 0))
		goto copy;
invalid:
	return 0x800f1716;
copy:
	memcpy(dst->msg, src->msg, src->msg_size);
	return ret;
}

SS_MGR_CORE(ksceSblSsDecryptWithPortability)
int ksceSblSsDecryptWithPortability(SceUInt32 key_type, void *iv,
				  ScePortabilityData *src, ScePortabilityData *dst)
{
	SceSblSmCommId id;
	SceUInt32 serviceResult;
	SsPortabilityRequest request;
	SceAuthInfo auth;
	int ret;

	if (!iv || !dst)
		return 0x800f1716;
	if (!src)
		return 0x800f1716;
	if (key_type > 20)
		return 0x800f1716;
	if (dst->msg_size != src->msg_size || dst->msg_size > 32 || (dst->msg_size & 15))
		return 0x800f1716;
	ret = ksceKernelGetProcessSelfAuthInfo(sceKernelGetProcessId(), &auth.request);
	if (ret < 0)
		return ret;
	if (__builtin_expect(key_type - 1 <= 19, 0))
		goto secure;
	memcpy(dst->msg, src->msg, src->msg_size);
	return 0;
secure:
	id = -1;
	serviceResult = 0;
	request.keyType = key_type;
	__builtin_memcpy(&request.input, src, sizeof(request.input));
	__builtin_memcpy(request.iv, iv, sizeof(request.iv));
	request.output.msg_size = request.input.msg_size;
	{
		struct SysrootSelfInfo selfInfo;

		ksceSblSsMemset(&selfInfo, 0, sizeof(selfInfo));
		selfInfo.size = sizeof(selfInfo);
		ret = SceSysrootForKernel_F10AB792(SCE_KERNEL_SYSROOT_SELF_INDEX_ENCDEC_W_PORTABILITY_SM,
					   &selfInfo);
		if (__builtin_expect(ret < 0, 0)) {
			ret = SS_ERROR_INVALID_ARGUMENT;
			goto stop;
		}
		ksceSblSsMemset(&auth, 0, sizeof(auth));
		memcpy(&auth.request, &ssPortabilityAuthInfo, sizeof(auth.request));
		auth.media_type = 2;
		auth.self_type = (auth.self_type & ~15U) | 2;
		ret = ksceSblSmCommStartSmFromData(0, selfInfo.selfData, selfInfo.selfSize,
						0, &auth, &id);
	}
	if (ret != 0)
		goto stop;
	ret = ksceSblSmCommCallFunc(id, 0x2000a, &serviceResult, &request, sizeof(request));
	if (ret != 0)
		goto stop;
	ret = serviceResult;
	if (ret == 0)
		__builtin_memcpy(dst, &request.output, sizeof(request.output));
stop:
	if (id != -1) {
		SceSblSmCommPair stopResult = { -1, -1 };

		ksceSblSmCommStopSm(id, &stopResult);
	}
	return ret;
}

/* Not memset: callers clear key material, which a builtin could drop. */
SS_MGR_CORE(ksceSblSsMemset)
void *ksceSblSsMemset(void *dst, int c, SceSize size)
{
	uint8_t *p = dst;
	uint8_t *end = p + size;

	if (size) {
		do {
			*(volatile uint8_t *)p++ = (uint8_t)c;
		} while (p != end);
	}
	return dst;
}
