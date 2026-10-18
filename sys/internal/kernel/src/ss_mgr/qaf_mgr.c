/* QAF boot flags and secure non-volatile token services. */
#include "ssmgr_internal.h"
#include <kernel/error.h>
#include <intrmgr/syscall.h>
#include <string.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysroot.h>
#include <kernel_boot_loader/kbl_param.h>
#include <syscon/syscon.h>
#include <sm_comm/sm_comm.h>

uint32_t qafInitialized;
SsMgrQaFlags qaFlags;
uint32_t qafSysconVersion;
volatile int qafCachedSysconFlag;
volatile int qafSysconFlagInitialized;
uint32_t qafBaryonVersion;
SceUID qafSmId;

/* QAF name: 24 bytes kept in NVS after the token header. */
#define QAF_NAME_SIZE		0x18
#define QAF_TOKEN_SIZE		0x180
#define NVS_QAF_TOKEN_HEAD	0x400		/* 0x80 bytes, name at +8 */
#define NVS_QAF_LOCKED		0x480		/* bit 0: token locked */
#define NVS_QAF_TOKEN_BODY	0x5a0		/* 0x100 bytes */

static const char qafWorkaroundName[] = "qaf_workaround";

#define SS_MGR_QAF(name) __attribute__((section(".ssMgrQaf." #name)))

/* The 4.00 debug build calls this; the release builds inline it into its callers. */
#if FW_VERSION >= 0x04000000
__attribute__((noinline))
#else
__attribute__((always_inline))
#endif
SS_MGR_QAF(SceSblSsMgrForKernel_83D254FF)
int SceSblSsMgrForKernel_83D254FF(void *flags)
{
	if (!flags)
		return (int)0x800f0716;
	if (!qafInitialized)
		return (int)0x800f0726;
	__builtin_memcpy(flags, &qaFlags, sizeof(qaFlags));
	return 0;
}

SS_MGR_QAF(SceSblSsMgrForKernel_E2DD0378)
int SceSblSsMgrForKernel_E2DD0378(char *name, SceSize size)
{
	uint8_t locked = 0xff;
	uint64_t head[0x80 / sizeof(uint64_t)];
	SsMgrQaFlags flags;
	unsigned int i, any;
	int ret;

	if (!name || !size)
		return SSMGR_ERROR_INVALID_ARGUMENT;
	if (size >= QAF_NAME_SIZE)
		size = QAF_NAME_SIZE;
	if (SceSblSsMgrForKernel_83D254FF(flags.words))
		return SSMGR_ERROR_NO_QAF;

	any = 0;
	for (i = 0; i < sizeof(flags.bytes); i++)
		any = (uint8_t)(any | flags.bytes[i]);
	if (!any)
		return SSMGR_ERROR_NO_QAF;

	if (qaFlags.bytes[9] & 2) {
		strncpy(name, qafWorkaroundName, size);
		name[size - 1] = '\0';
		return 0;
	}

	memset(head, 0, sizeof(head));
	SceKernelSuspendForDriver_4DF40893(0);
	ret = nvs_read_data(NVS_QAF_LOCKED, &locked, 1);
	locked &= 1;
	if (ret) {
		SceKernelSuspendForDriver_2BB92967(0);
		return ret;
	}
	if (locked) {
		SceKernelSuspendForDriver_2BB92967(0);
		return SSMGR_ERROR_NO_QAF;
	}
	ret = nvs_read_data(NVS_QAF_TOKEN_HEAD, head, sizeof(head));
	SceKernelSuspendForDriver_2BB92967(locked);
	if (ret)
		return ret;

	memcpy(name, (uint8_t *)head + 8, size);
	name[size - 1] = '\0';
	return 0;
}

/* Internal name inferred from the module-start call and state writes. */
SS_MGR_QAF(qafInit)
int qafInit(void)
{
	const SceKblParam *parameters = SceSysrootForKernel_9DB56D1F();

	if (parameters)
		__builtin_memcpy(&qaFlags, parameters->qa_flags, sizeof(qaFlags));
	qafInitialized = 1;
	return 0;
}

/* QAF manager queries: each answers one bit of the QA flags read at boot
 * (qafInit). The user-callable exports run under the syscall frame. */
/* QA flag bits, as (byte, bit) within qaFlags. */
#define QAF_ALLOW_LIMITED_DEBUG_MENU	0x06, 1		/* also screenshots, NP full test */
#define QAF_ALLOW_ALL_DEBUG_MENU	0x0c, 1
#define QAF_ALLOW_KERNEL_DEBUG		0x0d, 0
#define QAF_ALLOW_REMOTE_SYSMODULE	0x0d, 1
#define QAF_ALLOW_MINIMUM_DEBUG_MENU	0x0f, 0		/* also non-QA PUP, NP test */
#define QAF_ALLOW_FORCE_UPDATE		0x0f, 1

static inline int qafFlag(unsigned int byte, unsigned int bit)
{
	return (qaFlags.bytes[byte] >> bit) & 1;
}

int sceSblQafManagerGetQafNameForUser(char *name, SceSize size)
{
	SceUInt32 frame = sceSyscallEnter();
	char buffer[QAF_NAME_SIZE];
	int ret;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(frame);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	if (size > QAF_NAME_SIZE)
		size = QAF_NAME_SIZE;
	if (!name) {
		sceSyscallLeave(frame);
		return SSMGR_ERROR_INVALID_ARGUMENT;
	}

	if (__builtin_expect(
		(ret = sceKernelCopyFromUser(buffer, name, size)) != 0 ||
		(ret = SceSblSsMgrForKernel_E2DD0378(buffer, size)) != 0 ||
		(ret = sceKernelCopyToUser(name, buffer, size)) != 0, 0))
		goto out;
	sceSyscallLeave(frame);
	return 0;
out:
	sceSyscallLeave(frame);
	return ret;
}

/* Legacy token entry points return ENOSYS. */
int sceSblQafManagerDeleteQafTokenForUser(void)
{
	return SCE_ERROR_ERRNO_ENOSYS;
}

int sceSblQafManagerSetQafTokenForUser(void)
{
	return SCE_ERROR_ERRNO_ENOSYS;
}

int sceSblQafMgrGetQafToken(void)
{
	return SCE_ERROR_ERRNO_ENOSYS;
}

int sceSblQafMgrGetQafName(char *name, SceSize size)
{
	SceUInt32 frame = sceSyscallEnter();
	char buffer[QAF_NAME_SIZE];
	uint64_t token[QAF_TOKEN_SIZE / sizeof(uint64_t)];
	int ret;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(frame);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	if (size < QAF_NAME_SIZE) {
		sceSyscallLeave(frame);
		return SSMGR_ERROR_INVALID_ARGUMENT;
	}
	if (!name) {
		sceSyscallLeave(frame);
		return SSMGR_ERROR_INVALID_ARGUMENT;
	}

	if (__builtin_expect(
		(ret = qafLoadToken((uint8_t *)token)) != 0 ||
		(ret = sceKernelCopyFromUser(buffer, name, QAF_NAME_SIZE)) != 0 ||
		(ret = SceSblSsMgrForKernel_E2DD0378(buffer, QAF_NAME_SIZE)) != 0 ||
		(ret = sceKernelCopyToUser(name, buffer, QAF_NAME_SIZE)) != 0, 0))
		goto out;
	sceSyscallLeave(frame);
	return 0;
out:
	sceSyscallLeave(frame);
	return ret;
}

/* Internal name inferred: NP test is allowed by the QA flag or by Sysroot. */
__attribute__((noinline)) int qafIsAllowNpTest(void)
{
	if (qafFlag(QAF_ALLOW_MINIMUM_DEBUG_MENU))
		return 1;

	return SceSysrootForDriver_A43599E9() != 0;
}

int sceSblQafManagerIsAllowKernelDebugForUser(void)
{
	SceUInt32 frame = sceSyscallEnter();
	int allow = qafFlag(QAF_ALLOW_KERNEL_DEBUG);

	sceSyscallLeave(frame);
	return allow;
}

int sceSblQafMgrIsAllowRemoteSysmoduleLoad(void)
{
	SceUInt32 frame = sceSyscallEnter();
	int allow = qafFlag(QAF_ALLOW_REMOTE_SYSMODULE);

	sceSyscallLeave(frame);
	return allow;
}

int sceSblQafMgrIsAllowAllDebugMenuDisplay(void)
{
	SceUInt32 frame = sceSyscallEnter();
	int allow = qafFlag(QAF_ALLOW_ALL_DEBUG_MENU);

	sceSyscallLeave(frame);
	return allow;
}

int sceSblQafMgrIsAllowLimitedDebugMenuDisplay(void)
{
	SceUInt32 frame = sceSyscallEnter();
	int allow = qafFlag(QAF_ALLOW_LIMITED_DEBUG_MENU);

	sceSyscallLeave(frame);
	return allow;
}

int sceSblQafMgrIsAllowMinimumDebugMenuDisplay(void)
{
	SceUInt32 frame = sceSyscallEnter();
	int allow = qafFlag(QAF_ALLOW_MINIMUM_DEBUG_MENU);

	sceSyscallLeave(frame);
	return allow;
}

int sceSblQafMgrIsAllowScreenShotAlways(void)
{
	SceUInt32 frame = sceSyscallEnter();
	int allow = qafFlag(QAF_ALLOW_LIMITED_DEBUG_MENU);

	sceSyscallLeave(frame);
	return allow;
}

int sceSblQafMgrIsAllowNonQAPup(void)
{
	SceUInt32 frame = sceSyscallEnter();
	int allow = qafFlag(QAF_ALLOW_MINIMUM_DEBUG_MENU);

	sceSyscallLeave(frame);
	return allow;
}

int sceSblQafMgrIsAllowForceUpdate(void)
{
	SceUInt32 frame = sceSyscallEnter();
	int allow = qafFlag(QAF_ALLOW_FORCE_UPDATE);

	sceSyscallLeave(frame);
	return allow;
}

int sceSblQafMgrIsAllowNpTest(void)
{
	SceUInt32 frame = sceSyscallEnter();
	int allow = qafIsAllowNpTest();

	sceSyscallLeave(frame);
	return allow;
}

int sceSblQafMgrIsAllowNpFullTest(void)
{
	SceUInt32 frame = sceSyscallEnter();
	int allow = qafFlag(QAF_ALLOW_LIMITED_DEBUG_MENU);

	sceSyscallLeave(frame);
	return allow;
}

/* Names unknown: the QAF service's 32-byte secure NVS record. */
typedef struct QafNvsVersionRecord {
	uint32_t flags;
	uint8_t version;
	uint8_t reserved[27];
} QafNvsVersionRecord;
typedef union QafNvsFlagWord {
	uint32_t value;
	uint8_t lowByte;
} QafNvsFlagWord;
/* Name unknown: complete service-1 transaction exchanged with qaf_sm. */
typedef struct QafNvsFlagRequest {
	uint32_t command;
	uint32_t reserved;
	uint32_t data;
	uint32_t flags;
	uint8_t challenge[48];
	uint8_t response[48];
} QafNvsFlagRequest;
typedef struct QafSecureNvsRequest {
	uint32_t command;
	uint32_t slot;
	QafNvsVersionRecord record;
	uint8_t challenge[48];
	uint8_t response[48];
} QafSecureNvsRequest;
/* Name unknown: only the version byte is interpreted by this module. */
typedef struct QafToken {
	uint8_t headerPrefix[5];
	uint8_t version;
	uint8_t headerData[122];
	uint8_t body[256];
} QafToken;

__attribute__((noinline)) int qafLoadSecureModule(const char *path);
__attribute__((noinline)) int qafStopSecureModule(void);
__attribute__((noinline)) int qafReadSecureNvs(unsigned int slot,
	QafNvsVersionRecord *record);
__attribute__((noinline)) int qafWriteSecureNvs(unsigned int slot,
	const QafNvsVersionRecord *record);
__attribute__((noinline)) int qafReadNvsFlags(uint32_t *data, uint32_t *flags);
__attribute__((noinline)) int qafWriteNvsFlags(const uint32_t *data,
	const uint32_t *flags);

/* Name unknown: a failed Syscon query conservatively caches a set flag. */
__attribute__((noinline, noclone)) int qafGetCachedSysconFlag(void)
{
	int flags;

	if (qafSysconFlagInitialized)
		return qafCachedSysconFlag;
	flags = SceSysconForDriver_EBE3262C();
	if (flags < 0 || (flags & 0x40)) {
		qafCachedSysconFlag = 1;
		qafSysconFlagInitialized = 1;
		return 1;
	}
	qafCachedSysconFlag = 0;
	qafSysconFlagInitialized = 1;
	return 0;
}

/* Name unknown: optionally reject a downgrade of secure NVS slot 0x15. */
__attribute__((noinline)) int qafUpdateNvsVersion(unsigned int version, int checkVersion)
{
	QafNvsVersionRecord record __attribute__((aligned(8)));
	int ret, result, stopResult;

	SceKernelSuspendForDriver_4DF40893(0);
	ret = qafLoadSecureModule("os0:sm/qaf_sm.self");
	if (ret != 0) {
		SceKernelSuspendForDriver_2BB92967(0);
		return ret;
	}
	if (checkVersion && qafReadSecureNvs(0x15, &record) == 0 &&
	    record.version > version) {
		result = 0x800f0730;
	} else {
		record = (QafNvsVersionRecord) { 0 };
		record.version = version;
		result = qafWriteSecureNvs(0x15, &record);
	}
	stopResult = qafStopSecureModule();
	SceKernelSuspendForDriver_2BB92967(0);
	return stopResult != 0 ? stopResult : result;
}

/* Name unknown: update bit 1, then read both words back to verify the write. */
__attribute__((noinline)) int qafSetNvsFlag(int clear)
{
	uint32_t data;
	QafNvsFlagWord flags;
	uint32_t expectedData, expectedFlags;
	int ret, result, stopResult;

	SceKernelSuspendForDriver_4DF40893(0);
	ret = qafLoadSecureModule("os0:sm/qaf_sm.self");
	if (ret != 0) {
		SceKernelSuspendForDriver_2BB92967(0);
		return ret;
	}
	data = ret;
	flags.value = ret;
	result = qafReadNvsFlags(&data, &flags.value);
	if (result == 0) {
		if (clear == 0)
			flags.lowByte |= 2;
		else
			flags.lowByte &= ~2U;
		expectedData = data;
		expectedFlags = flags.value;
		result = qafWriteNvsFlags(&data, &flags.value);
		if (result == 0) {
			result = qafReadNvsFlags(&data, &flags.value);
			if (result == 0) {
				result = SSMGR_ERROR_NVS;
				if (memcmp(&data, &expectedData, sizeof(data)) == 0 &&
				    memcmp(&flags.value, &expectedFlags, sizeof(flags.value)) == 0)
					result = 0;
			}
		}
	}
	stopResult = qafStopSecureModule();
	SceKernelSuspendForDriver_2BB92967(0);
	return stopResult != 0 ? stopResult : result;
}

/* Syscon NVS uses transfers of at most eight bytes. */
__attribute__((noinline)) int nvs_read_data(uint32_t offset, void *buffer,
	uint32_t size)
{
	uint32_t done;
	int chunk, ret;

	ret = SceSysconForDriver_81A6060D(0);
	if (ret)
		return SSMGR_ERROR_NVS;
	done = 0;
	while (size) {
		if (size < 8)
			chunk = size;
		else
			chunk = 8;
		ret = SceSysconForDriver_ACAFA2B8(offset + done,
			(uint8_t *)buffer + done, chunk);
		if (ret)
			break;
		size -= chunk;
		done += chunk;
	}
	return ret;
}

__attribute__((noinline)) int nvs_write_data(uint32_t offset,
	const void *buffer, uint32_t size)
{
	uint32_t done;
	int chunk, ret;

	ret = SceSysconForDriver_81A6060D(0);
	if (ret)
		return SSMGR_ERROR_NVS;
	done = 0;
	while (size) {
		if (size < 8)
			chunk = size;
		else
			chunk = 8;
		ret = SceSysconForDriver_10C9657A(offset + done,
			(const uint8_t *)buffer + done, chunk);
		if (ret)
			break;
		size -= chunk;
		done += chunk;
	}
	return ret;
}

/* Prototype debug records name the QAF NVS flag check. */
__attribute__((noinline)) int sceSblQafManagerCheckFlag(void)
{
	uint32_t baryonVersion = qafBaryonVersion;
	uint32_t sysconVersion;
	uint32_t data;
	QafNvsFlagWord flags;
	int ret, result, stopResult;

	if (__builtin_expect(baryonVersion == 0, 1)) {
		baryonVersion = SceSysconForDriver_FF86F4C5();
		qafBaryonVersion = baryonVersion;
	}
	if (qafSysconVersion == 0)
		SceSysconForDriver_D2F456DC((uint8_t *)&qafSysconVersion);
	sysconVersion = qafSysconVersion;
	if (!((qafGetCachedSysconFlag() != 0 &&
	       (sysconVersion & 0xffffff) >= 0x3600) ||
	      (qafGetCachedSysconFlag() == 0 && baryonVersion > 0x90902)))
		return (int)0x800f0725;
	data = 0;
	SceKernelSuspendForDriver_4DF40893(0);
	ret = qafLoadSecureModule("os0:sm/qaf_sm.self");
	if (ret != 0) {
		SceKernelSuspendForDriver_2BB92967(0);
		return ret;
	}
	result = qafReadNvsFlags(&data, &flags.value);
	if (result == 0 && (flags.lowByte & 2))
		result = (int)0x800f0726;
	stopResult = qafStopSecureModule();
	SceKernelSuspendForDriver_2BB92967(0);
	return stopResult != 0 ? stopResult : result;
}

/* Name unknown: this revision policy is inlined into all three setters. */
static inline __attribute__((always_inline)) int qafSupportsNvsFlag(void)
{
	uint32_t baryonVersion = qafBaryonVersion;
	uint32_t sysconVersion;

	if (__builtin_expect(baryonVersion == 0, 1)) {
		baryonVersion = SceSysconForDriver_FF86F4C5();
		qafBaryonVersion = baryonVersion;
	}
	if (qafSysconVersion == 0)
		SceSysconForDriver_D2F456DC((uint8_t *)&qafSysconVersion);
	sysconVersion = qafSysconVersion;
	return (qafGetCachedSysconFlag() != 0 &&
		__builtin_expect((sysconVersion & 0xffffff) >= 0x3600, 0)) ||
	       (qafGetCachedSysconFlag() == 0 && baryonVersion > 0x90902);
}

__attribute__((noinline)) int sceSblQafManagerSetFlag(void)
{
	if (!qafSupportsNvsFlag())
		return (int)0x800f0725;
	return qafSetNvsFlag(1);
}

__attribute__((noinline)) int sceSblQafManagerUpdateQaFlagVersion(unsigned int version)
{
	if (!qafSupportsNvsFlag())
		return (int)0x800f0725;
	return qafUpdateNvsVersion(version, 1);
}

__attribute__((noinline)) int sceSblQafManagerSetQaFlagVersion(unsigned int version)
{
	if (!qafSupportsNvsFlag())
		return (int)0x800f0725;
	return qafUpdateNvsVersion(version, 0);
}

const SceSelfAuthInfo qafSmAuthInfo = {
	.programAuthorityId = UINT64_C(0x2808000000000001),
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
int SceSblACMgrForKernel_sceSblACMgrGetMediaType(const char *path, uint32_t *mediaType);

int qafLoadSecureModule(const char *path)
{
	uint32_t command[4];
	SelfAuthRequest auth;
	int ret;

	memset(&auth, 0, sizeof(auth));
	memcpy(&auth.caller, &qafSmAuthInfo, sizeof(auth.caller));
	ret = SceSblACMgrForKernel_sceSblACMgrGetMediaType(path, &auth.mediaType);
	if (ret < 0)
		return ret;
	auth.selfType = (auth.selfType & ~0xfU) | 2;
	command[0] = 0xff;
	command[1] = 0;
	command[2] = 0;
	command[3] = 0;
	return SceSblSmCommForKernel_7863A0CC(0, path,
		(uint32_t)(uintptr_t)command, &auth, &qafSmId);
}

int qafStopSecureModule(void)
{
	SceSblSmCommPair result;

	return SceSblSmCommForKernel_0631F8ED(qafSmId, &result);
}

int qafReadNvsFlags(uint32_t *data, uint32_t *flags)
{
	QafNvsFlagRequest request;
	uint32_t serviceResult;
	int ret;

	request.command = 0;
	request.reserved = 0;
	ret = SceSblSmCommForKernel_DB9FC204(qafSmId, 1, &serviceResult,
		&request, sizeof(request));
	if (ret == 0) {
		ret = serviceResult;
		if (ret == 0) {
			ret = SceSysconForDriver_EBDF88B9(request.challenge, 16,
				request.response, sizeof(request.response));
			if (ret == 0) {
				request.command = 1;
				ret = SceSblSmCommForKernel_DB9FC204(qafSmId, 1,
					&serviceResult, &request, sizeof(request));
				if (ret == 0) {
					ret = serviceResult;
					if (ret == 0) {
						*data = request.data;
						*flags = request.flags;
					}
				}
			}
		}
	}
	return ret;
}

int qafWriteNvsFlags(const uint32_t *data, const uint32_t *flags)
{
	QafNvsFlagRequest request;
	uint32_t serviceResult;
	int ret;

	request.data = *data;
	request.flags = *flags;
	request.command = 2;
	request.reserved = 0;
	ret = SceSblSmCommForKernel_DB9FC204(qafSmId, 1, &serviceResult,
		&request, sizeof(request));
	if (ret == 0) {
		ret = serviceResult;
		if (ret == 0) {
			ret = SceSysconForDriver_63683B9B(request.challenge,
				sizeof(request.challenge), request.response, 16);
			if (ret == 0) {
				request.command = 3;
				ret = SceSblSmCommForKernel_DB9FC204(qafSmId, 1,
					&serviceResult, &request, sizeof(request));
				if (ret == 0)
					ret = serviceResult;
			}
		}
	}
	return ret;
}

/* Name unknown: service 12 validates a token; service 13 also returns it. */
__attribute__((noinline)) int qafInvokeTokenService(void *token, unsigned int service)
{
	uint64_t buffer[QAF_TOKEN_SIZE / sizeof(uint64_t)];
	uint32_t serviceResult = 0;
	int ret;

	if (service - 12 > 1)
		return (int)0x800f0716;
	memcpy(buffer, token, sizeof(buffer));
	ret = SceSblSmCommForKernel_DB9FC204(qafSmId, service,
		&serviceResult, buffer, sizeof(buffer));
	if (ret == 0) {
		if (service == 13)
			memcpy(token, buffer, sizeof(buffer));
		ret = serviceResult;
	}
	return ret;
}

int qafReadSecureNvs(unsigned int slot, QafNvsVersionRecord *record)
{
	QafSecureNvsRequest request;
	uint32_t serviceResult;
	int ret;

	if (slot - 1 > 0x1e)
		return (int)0x800f0716;
	request.command = 0;
	request.slot = slot;
	ret = SceSblSmCommForKernel_DB9FC204(qafSmId, 2, &serviceResult,
		&request, sizeof(request));
	if (__builtin_expect(ret != 0, 0))
		return ret;
	ret = serviceResult;
	if (ret != 0)
		return ret;
	ret = SceSysconForDriver_EBDF88B9(request.challenge, 16,
		request.response, sizeof(request.response));
	if (ret != 0)
		return ret;
	request.command = 1;
	ret = SceSblSmCommForKernel_DB9FC204(qafSmId, 2,
		&serviceResult, &request, sizeof(request));
	if (ret != 0)
		return ret;
	ret = serviceResult;
	if (ret == 0)
		__builtin_memcpy(record, &request.record, sizeof(*record));
	return ret;
}

int qafWriteSecureNvs(unsigned int slot, const QafNvsVersionRecord *record)
{
	QafSecureNvsRequest request;
	uint32_t serviceResult;
	int ret;

	if (slot - 1 > 0x1e)
		return (int)0x800f0716;
	__builtin_memcpy(&request.record, record, sizeof(*record));
	request.slot = slot;
	request.command = 2;
	ret = SceSblSmCommForKernel_DB9FC204(qafSmId, 2, &serviceResult,
		&request, sizeof(request));
	if (__builtin_expect(ret == 0, 1)) {
		ret = serviceResult;
		if (ret == 0) {
			ret = SceSysconForDriver_63683B9B(request.challenge,
				sizeof(request.challenge), request.response, 16);
			if (ret == 0) {
				request.command = 3;
				ret = SceSblSmCommForKernel_DB9FC204(qafSmId, 2,
					&serviceResult, &request, sizeof(request));
				if (ret == 0)
					ret = serviceResult;
			}
		}
	}
	return ret;
}

__attribute__((noinline)) int qafValidateToken(void *token, int service)
{
	static const char secureModulePath[] = "os0:sm/qaf_sm.self";
	int ret, result, stopResult;

	SceKernelSuspendForDriver_4DF40893(0);
	ret = qafLoadSecureModule(secureModulePath);
	if (ret != 0) {
		SceKernelSuspendForDriver_2BB92967(0);
		return ret;
	}
	result = qafInvokeTokenService(token, service);
	stopResult = qafStopSecureModule();
	SceKernelSuspendForDriver_2BB92967(0);
	return stopResult != 0 ? stopResult : result;
}

/* Reads the stored token, or an empty token when locked, for service 13. */
__attribute__((noinline)) int qafGetToken(uint8_t *token)
{
	uint8_t locked = 0xff;
	int ret, status;

	if (!token)
		return SSMGR_ERROR_INVALID_ARGUMENT;

	memset(token, 0, QAF_TOKEN_SIZE);
	SceKernelSuspendForDriver_4DF40893(0);
	ret = nvs_read_data(NVS_QAF_LOCKED, &locked, 1);
	status = locked &= 1;
	if (ret) {
		SceKernelSuspendForDriver_2BB92967(0);
		return ret;
	}
	if (status) {
		SceKernelSuspendForDriver_2BB92967(ret);
		goto validate;
	}
	ret = nvs_read_data(NVS_QAF_TOKEN_HEAD, token, 0x80);
	if (ret) {
		SceKernelSuspendForDriver_2BB92967(status);
		return ret;
	}
	status = nvs_read_data(NVS_QAF_TOKEN_BODY, token + 0x80, 0x100);
	SceKernelSuspendForDriver_2BB92967(ret);
	if (status)
		return status;

validate:
	SceKernelSuspendForDriver_4DF40893(0);
	ret = qafValidateToken(token, 0xd);
	SceKernelSuspendForDriver_2BB92967(0);
	return ret;
}

__attribute__((noinline)) int qafSetToken(const void *data)
{
	const QafToken *token = data;
	QafToken readback;
	uint8_t readLocked = 0xff;
	uint8_t writeLocked;
	int ret;

	if (!token)
		return (int)SSMGR_ERROR_INVALID_ARGUMENT;
	SceKernelSuspendForDriver_4DF40893(0);
	ret = qafValidateToken((void *)token, 12);
	SceKernelSuspendForDriver_2BB92967(0);
	if (ret != 0)
		return ret;
	ret = sceSblQafManagerCheckFlag();
	if (ret == (int)SSMGR_ERROR_NO_QAF) {
		ret = sceSblQafManagerSetQaFlagVersion(token->version);
		if (ret != 0)
			return ret;
		ret = sceSblQafManagerSetFlag();
	} else {
		if (ret != 0)
			return ret;
		ret = sceSblQafManagerUpdateQaFlagVersion(token->version);
		if (ret == (int)0x800f0730)
			return ret;
	}
	if (ret != 0)
		return ret;
	memset(&readback, 0, sizeof(readback));
	SceKernelSuspendForDriver_4DF40893(0);
	ret = nvs_write_data(NVS_QAF_TOKEN_HEAD, token,
		sizeof(*token) - sizeof(token->body));
	if (ret == 0) {
		ret = nvs_write_data(NVS_QAF_TOKEN_BODY, token->body,
			sizeof(token->body));
		if (ret == 0) {
			writeLocked = 0;
			ret = nvs_write_data(NVS_QAF_LOCKED, &writeLocked, 1);
		}
	}
	SceKernelSuspendForDriver_2BB92967(0);
	if (ret != 0)
		return ret;
	SceKernelSuspendForDriver_4DF40893(0);
	ret = nvs_read_data(NVS_QAF_TOKEN_HEAD, &readback,
		sizeof(readback) - sizeof(readback.body));
	if (ret == 0) {
		ret = nvs_read_data(NVS_QAF_TOKEN_BODY, readback.body,
			sizeof(readback.body));
		if (ret == 0) {
			ret = nvs_read_data(NVS_QAF_LOCKED, &readLocked, 1);
			readLocked &= 1;
		}
	}
	SceKernelSuspendForDriver_2BB92967(0);
	if (ret != 0)
		return ret;
	if (memcmp(token, &readback, sizeof(readback)) != 0)
		return (int)SSMGR_ERROR_NVS;
	return readLocked != 0 ? (int)SSMGR_ERROR_NVS : 0;
}

/* Erases the stored QAF token and marks it locked. Internal name inferred. */
__attribute__((noinline)) int qafDeleteToken(void)
{
	uint64_t blank[QAF_TOKEN_SIZE / sizeof(uint64_t)];
	uint8_t locked;
	int ret, ret2;

	memset(blank, 0xff, sizeof(blank));
	SceKernelSuspendForDriver_4DF40893(0);
	ret = nvs_write_data(NVS_QAF_TOKEN_HEAD, blank, 0x80);
	if (ret) {
		SceKernelSuspendForDriver_2BB92967(0);
		return ret;
	}
	ret2 = nvs_write_data(NVS_QAF_TOKEN_BODY,
			     (uint8_t *)blank + 0x80, 0x100);
	if (ret2) {
		SceKernelSuspendForDriver_2BB92967(ret);
		return ret2;
	}
	locked = 1;
	ret = nvs_write_data(NVS_QAF_LOCKED, &locked, 1);
	SceKernelSuspendForDriver_2BB92967(ret2);
	return ret;
}

/* Reads the stored token and validates it through secure service 12. */
__attribute__((noinline)) int qafLoadToken(uint8_t *token)
{
	uint8_t locked = 0xff;
	SceUInt32 status;
	int ret;

	if (!token)
		return SSMGR_ERROR_INVALID_ARGUMENT;

	memset(token, 0, QAF_TOKEN_SIZE);
	SceKernelSuspendForDriver_4DF40893(0);
	ret = nvs_read_data(NVS_QAF_LOCKED, &locked, 1);
	status = locked &= 1;
	if (ret) {
		SceKernelSuspendForDriver_2BB92967(0);
		return ret;
	}
	if (status) {
		SceKernelSuspendForDriver_2BB92967(ret);
		return SSMGR_ERROR_NO_QAF;
	}

	ret = nvs_read_data(NVS_QAF_TOKEN_HEAD, token, 0x80);
	if (__builtin_expect(ret != 0, 0))
		goto unlock;
	status = nvs_read_data(NVS_QAF_TOKEN_BODY, token + 0x80, 0x100);
	SceKernelSuspendForDriver_2BB92967(ret);
	if (status)
		return status;
	SceKernelSuspendForDriver_4DF40893(status);
	ret = qafValidateToken(token, 0xc);
unlock:
	SceKernelSuspendForDriver_2BB92967(status);
	return ret;
}

int sceSblQafMgrDeleteQafToken2(void)
{
	SceUInt32 frame = sceSyscallEnter();
	int ret;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(frame);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	ret = qafDeleteToken();
	sceSyscallLeave(frame);
	return ret;
}

int sceSblQafMgrSetQafToken2(const void *token)
{
	SceUInt32 frame = sceSyscallEnter();
	uint64_t buffer[QAF_TOKEN_SIZE / sizeof(uint64_t)];
	int ret;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(frame);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	if (!token) {
		sceSyscallLeave(frame);
		return SSMGR_ERROR_INVALID_ARGUMENT;
	}
	ret = sceKernelCopyFromUser(buffer, token, sizeof(buffer));
	if (__builtin_expect(ret == 0, 1))
		ret = qafSetToken(buffer);
	sceSyscallLeave(frame);
	return ret;
}

int sceSblQafMgrGetQafToken2(void *token)
{
	SceUInt32 frame = sceSyscallEnter();
	uint64_t buffer[QAF_TOKEN_SIZE / sizeof(uint64_t)];
	int ret;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(frame);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	if (!token) {
		sceSyscallLeave(frame);
		return SSMGR_ERROR_INVALID_ARGUMENT;
	}
	ret = qafGetToken((uint8_t *)buffer);
	if (__builtin_expect(ret == 0, 1))
		ret = sceKernelCopyToUser(token, buffer, sizeof(buffer));
	sceSyscallLeave(frame);
	return ret;
}
