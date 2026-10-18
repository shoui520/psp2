/*
 * SceSblUtMgr (file name unknown): the utoken, tm0:utoken/utoken.dat. It
 * is sealed by utoken_sm.self; once opened, its records turn on test flags
 * (NP, communication, store, Trilithium) for this console.
 */

#include <iofilemgr/iofilemgr.h>
#include <rtc/rtc.h>
#include <sm_comm/sm_comm.h>
#include <sysmem/sysmem.h>
#include <intrmgr/syscall.h>
#include "post_ss_mgr_internal.h"
#include <acmgr/acmgr.h>
#include <sysmem/sysroot.h>

#define SCE_SBL_ERROR_UT_NOT_FOUND		0x800f1a02	/* names unknown */
#define SCE_SBL_ERROR_UT_NO_MEMORY		0x800f1a0c
#define SCE_SBL_ERROR_UT_INVALID_ARGUMENT	0x800f1a16
#define SCE_SBL_ERROR_UT_BROKEN			0x800f1a24
#define SCE_SBL_ERROR_UT_NOT_LOADED		0x800f1a26
#define SCE_SBL_ERROR_UT_SM			0x800f1a28

#define UT_DIR		"tm0:utoken"
#define UT_PATH		"tm0:utoken/utoken.dat"
#define UT_SM_PATH	"os0:sm/utoken_sm.self"

#define UT_SIZE			0x800	/* name unknown */
#define UT_TRILITHIUM_FLAG	"UT_TRILITHIUM_FLAG"

/* the utoken_sm services (names unknown) */
#define UT_SM_SEAL		1
#define UT_SM_OPEN		2

/* SceRtcTick of 1970-01-01 (name unknown) */
#define RTC_TICK_UNIX_EPOCH	0xdcbffeff2bc000ULL

/* the flags of a utoken (names unknown) */
#define UT_FLAG_COM_TEST_AUTH_ID	(1 << 0)
#define UT_FLAG_22599675		(1 << 1)
#define UT_FLAG_TRILITHIUM_FLAG		(1 << 2)
#define UT_FLAG_AUTH_ID_LIST		(1 << 3)
#define UT_FLAG_STORE			(1 << 4)
#define UT_FLAG_COM_TEST		(1 << 5)
#define UT_FLAG_9D1D8EE5		(1 << 6)
#define UT_FLAG_C93C0A0D		(1 << 7)
#define UT_FLAG_9B49C249		(1 << 8)
#define UT_FLAG_1923D80D		(1 << 9)
#define UT_FLAG_TRILITHIUM_BUFFER	(1 << 10)
#define UT_FLAG_NP_TEST			(1 << 11)

/* the record types of a utoken (names unknown) */
#define UT_RECORD_COM_TEST_AUTH_ID	1
#define UT_RECORD_TRILITHIUM_FLAG	3
#define UT_RECORD_AUTH_ID_LIST		4
#define UT_RECORD_COM_TEST		6
#define UT_RECORD_TRILITHIUM_BUFFER	0xb

#define UT_AUTH_ID_LIST_SIZE		0x20
#define UT_TRILITHIUM_BUFFER_SIZE	0x58

/* a utoken, as utoken_sm opens it (names unknown) */
typedef struct SceUtokenHeader {
	SceUInt32 reserved0[4];
	SceUInt32 flags;
	SceUInt32 reserved1;
	SceUInt32 expireDate;
	SceUInt32 reserved2[5];
} SceUtokenHeader;

/* the records that follow the header: a type and a size that counts both */
typedef struct SceUtokenRecord {
	SceUInt32 type;
	SceUInt32 size;
} SceUtokenRecord;

typedef struct SceUtokenAuthIdRecord {
	SceUtokenRecord record;
	SceUInt64 authId;
} SceUtokenAuthIdRecord;

typedef struct SceUtokenFlagRecord {
	SceUtokenRecord record;
	char flag[0x18];
} SceUtokenFlagRecord;

typedef struct SceUtokenAuthIdListRecord {
	SceUtokenRecord record;
	SceUInt64 authId[UT_AUTH_ID_LIST_SIZE];
} SceUtokenAuthIdListRecord;

typedef struct SceUtokenTrilithiumRecord {
	SceUtokenRecord record;
	SceUInt8 buffer[UT_TRILITHIUM_BUFFER_SIZE];
} SceUtokenTrilithiumRecord;

/* a utoken_sm request (names unknown) */
typedef struct SceUtSmCommand {
	SceUInt32 paddr;
	SceUInt32 size;
	SceUInt32 now;
	SceUInt32 reserved[4];
} SceUtSmCommand;

int SceSblACMgrForKernel_sceSblACMgrGetMediaType(const char *path, SceUInt32 *mediaType);
int utSmOpen(void *utoken, SceSize size, SceUInt32 now);
int utSmSeal(void *utoken, SceSize size, SceUInt32 now);

SceUInt8 g_utMgrTrilithiumBufferValid;			/* names unknown */
SceUInt8 g_utMgrStoreFlag;
SceUInt64 g_utMgrAuthIdList[UT_AUTH_ID_LIST_SIZE];
SceUInt8 g_utMgrC93C0A0DFlag;
SceUInt32 g_utMgrExpireDate;
SceUInt8 g_utMgr9D1D8EE5Flag;
SceUInt8 g_utMgrComTestFlag;
SceUInt8 g_utMgrNpTestFlag;
SceUInt64 g_utMgrComTestAuthId;
SceUInt8 g_utMgrAuthIdListValid;
int g_utMgrLoaded;
SceUInt8 g_utMgr22599675Flag;
SceUInt8 g_utMgr9B49C249Flag;
char g_utMgrTrilithiumFlag[0x18];
SceUInt8 g_utMgr1923D80DFlag;
SceUInt8 g_utMgrTrilithiumBuffer[UT_TRILITHIUM_BUFFER_SIZE];
static SceSblSmCommId g_utSmId;

/* the self authentication utoken_sm.self runs with */
static const SceSelfAuthInfo g_utSmAuthInfo = POST_SS_SM_AUTH_INFO;

SceUInt8 SceSblPostSsMgrForDriver_22599675(void)
{
	return g_utMgr22599675Flag;
}

SceUInt8 SceSblPostSsMgrForDriver_9B49C249(void)
{
	return g_utMgr9B49C249Flag;
}

SceUInt8 ksceSblUtMgrHasNpTestFlag(void)
{
	return g_utMgrNpTestFlag;
}

/* the current time in seconds since 1970 */
int utMgrGetCurrentTime(SceUInt32 *now)	/* name unknown */
{
	SceRtcTick tick;
	int ret;

	ret = SceRtcForDriver_401C0954(&tick);
	if (ret == 0)
		*now = (tick.tick - RTC_TICK_UNIX_EPOCH) / 1000000;
	return ret;
}

#if FW_VERSION >= 0x04000000
int ksceSblUtMgrGetTrilithiumBuffer(void *buffer)
{
	if (!g_utMgrTrilithiumBufferValid)
		return SCE_SBL_ERROR_UT_NOT_FOUND;
	memcpy(buffer, g_utMgrTrilithiumBuffer, sizeof(g_utMgrTrilithiumBuffer));
	return 0;
}
#endif

/* copy the record of a type, which must be size bytes */
int utMgrFindRecord(const void *records, SceSize size, SceUInt32 type, void *record,
		    SceSize recordSize)	/* name unknown */
{
	const SceUtokenRecord *r;
	SceSize pos;

	if (size < sizeof(*r))
		return SCE_SBL_ERROR_UT_NOT_FOUND;
	pos = 0;
	do {
		r = (const SceUtokenRecord *)((const char *)records + pos);
		if (r->size < sizeof(*r) || (r->size & 7) != 0)
			return SCE_SBL_ERROR_UT_BROKEN;
		pos += r->size;
		if (pos > size)
			return SCE_SBL_ERROR_UT_BROKEN;
		if (r->type == type) {
			if (r->size != recordSize)
				return SCE_SBL_ERROR_UT_BROKEN;
			memcpy(record, r, recordSize);
			return 0;
		}
	} while (pos + sizeof(*r) <= size);
	return SCE_SBL_ERROR_UT_NOT_FOUND;
}

#if FW_VERSION < 0x04000000
int ksceSblUtMgrGetTrilithiumBuffer(void *buffer)
{
	if (!g_utMgrTrilithiumBufferValid)
		return SCE_SBL_ERROR_UT_NOT_FOUND;
	memcpy(buffer, g_utMgrTrilithiumBuffer, sizeof(g_utMgrTrilithiumBuffer));
	return 0;
}
#endif

#if FW_VERSION >= 0x04000000
/* whether the utoken lists a program authority id */
int SceSblPostSsMgrForDriver_D8A2D465(SceUInt64 authId)
{
	int i;

	if (!g_utMgrAuthIdListValid || authId == 0)
		return 0;
	for (i = 0; i < UT_AUTH_ID_LIST_SIZE; i++) {
		if (g_utMgrAuthIdList[i] == authId)
			return 1;
	}
	return 0;
}
#endif

/* take the flags and records of an opened utoken */
int utMgrParseUtoken(const void *utoken, SceSize size)	/* name unknown */
{
	const SceUtokenHeader *p = utoken;
	const void *records = p + 1;
	SceUtokenHeader header;
	SceUtokenAuthIdRecord authId;
	SceUtokenFlagRecord flag;
	SceUtokenAuthIdListRecord list;
	SceUtokenTrilithiumRecord trilithium;
	int ret;

	memset(g_utMgrTrilithiumFlag, 0, sizeof(g_utMgrTrilithiumFlag));
	header = *p;
	g_utMgrExpireDate = header.expireDate;

	if (header.flags & UT_FLAG_COM_TEST_AUTH_ID) {
		ret = utMgrFindRecord(records, size, UT_RECORD_COM_TEST_AUTH_ID, &authId,
				      sizeof(authId));
		if (ret == SCE_SBL_ERROR_UT_BROKEN) {
			g_utMgrComTestFlag = 0;
			return ret;
		}
		if (ret == 0) {
			g_utMgrComTestAuthId = authId.authId;
			g_utMgrComTestFlag = 1;
		} else {
			g_utMgrComTestFlag = 0;
		}
	}
	if (header.flags & UT_FLAG_22599675)
		g_utMgr22599675Flag = 1;
	if (header.flags & UT_FLAG_TRILITHIUM_FLAG) {
		ret = utMgrFindRecord(records, size, UT_RECORD_TRILITHIUM_FLAG, &flag, sizeof(flag));
		if (ret == SCE_SBL_ERROR_UT_BROKEN)
			return ret;
		if (ret == 0)
			memcpy(g_utMgrTrilithiumFlag, flag.flag, sizeof(g_utMgrTrilithiumFlag));
	}
	if (header.flags & UT_FLAG_AUTH_ID_LIST) {
		ret = utMgrFindRecord(records, size, UT_RECORD_AUTH_ID_LIST, &list, sizeof(list));
		if (ret == SCE_SBL_ERROR_UT_BROKEN) {
			g_utMgrAuthIdListValid = 0;
			return ret;
		}
		if (ret == 0) {
			g_utMgrAuthIdListValid = 1;
			memcpy(g_utMgrAuthIdList, list.authId, sizeof(g_utMgrAuthIdList));
		} else {
			g_utMgrAuthIdListValid = 0;
		}
	}
	if (header.flags & UT_FLAG_STORE)
		g_utMgrStoreFlag = 1;
	if (header.flags & UT_FLAG_9D1D8EE5)
		g_utMgr9D1D8EE5Flag = 1;
	if (header.flags & UT_FLAG_C93C0A0D)
		g_utMgrC93C0A0DFlag = 1;
	if (header.flags & UT_FLAG_1923D80D)
		g_utMgr1923D80DFlag = 1;
	if (header.flags & UT_FLAG_9B49C249)
		g_utMgr9B49C249Flag = 1;
	if (header.flags & UT_FLAG_NP_TEST)
		g_utMgrNpTestFlag = 1;
	if (memcmp(g_utMgrTrilithiumFlag, UT_TRILITHIUM_FLAG, sizeof(g_utMgrTrilithiumFlag)) == 0)
		g_utMgrNpTestFlag = 1;
	if (header.flags & UT_FLAG_COM_TEST) {
		ret = utMgrFindRecord(records, size, UT_RECORD_COM_TEST, &authId, sizeof(authId));
		if (ret == SCE_SBL_ERROR_UT_BROKEN) {
			g_utMgrComTestFlag = 0;
			return ret;
		}
		if (ret == 0) {
			g_utMgrComTestAuthId = authId.authId;
			g_utMgrComTestFlag = 1;
		} else {
			g_utMgrComTestFlag = 0;
		}
	}
	if (header.flags & UT_FLAG_TRILITHIUM_BUFFER) {
		ret = utMgrFindRecord(records, size, UT_RECORD_TRILITHIUM_BUFFER, &trilithium,
				      sizeof(trilithium));
		if (ret == 0) {
			g_utMgrTrilithiumBufferValid = 1;
			memcpy(g_utMgrTrilithiumBuffer, trilithium.buffer,
			       sizeof(g_utMgrTrilithiumBuffer));
		} else {
			g_utMgrTrilithiumBufferValid = 0;
		}
	}
	return 0;
}

#if FW_VERSION < 0x04000000
/* whether the utoken lists a program authority id */
int SceSblPostSsMgrForDriver_D8A2D465(SceUInt64 authId)
{
	int i;

	if (!g_utMgrAuthIdListValid || authId == 0)
		return 0;
	for (i = 0; i < UT_AUTH_ID_LIST_SIZE; i++) {
		if (g_utMgrAuthIdList[i] == authId)
			return 1;
	}
	return 0;
}
#endif

int utMgrWriteFile(const void *utoken)	/* name unknown */
{
	SceUID fd;
	int ret;

	ret = sceIoMkdir(UT_DIR, 0606);
	if (ret != SCE_ERROR_ERRNO_EEXIST && ret < 0)
		return ret;
	fd = sceIoOpen(UT_PATH, SCE_O_RDWR | SCE_O_CREAT | SCE_O_TRUNC, 0606);
	if (fd < 0)
		return fd;
	ret = sceIoWrite(fd, utoken, UT_SIZE) == UT_SIZE ? 0 : SCE_ERROR_ERRNO_EIO;
	if (fd != 0)
		sceIoClose(fd);
	return ret;
}

void *utMgrAllocUtoken(void)	/* name unknown */
{
	SceAllocOpt opt = { .size = sizeof(opt), .align = 0x20 };

	return SceSysmemForDriver_49D4DD9B(g_postSsMgrHeap, UT_SIZE, &opt);
}

int utMgrReadFile(void *utoken, SceSize *size)	/* name unknown */
{
	SceOff end;
	SceUID fd;
	int ret;

	fd = sceIoOpen(UT_PATH, SCE_O_RDONLY, 0);
	if (fd < 0)
		return SCE_ERROR_ERRNO_ENOENT;
	end = sceIoLseek(fd, 0, SCE_SEEK_END);
	if (end < 0) {
		ret = SCE_ERROR_ERRNO_EIO;
	} else if (end != UT_SIZE) {
		ret = SCE_SBL_ERROR_UT_BROKEN;
	} else if (sceIoLseek(fd, 0, SCE_SEEK_SET) < 0 ||
		   sceIoRead(fd, utoken, UT_SIZE) != UT_SIZE) {
		ret = SCE_ERROR_ERRNO_EIO;
	} else {
		*size = UT_SIZE;
		ret = 0;
	}
	if (fd != 0)
		sceIoClose(fd);
	return ret;
}

int sceSblUtMgrInit(void)
{
	SceUInt32 now = 0;
	SceSize size;
	void *utoken;
	int ret;

	SceSysrootForDriver_E25D2FD5(SceSblPostSsMgrForDriver_22599675);
	SceSysrootForDriver_A12C9950(SceSblPostSsMgrForDriver_D8A2D465);
	SceSysrootForDriver_E2E88E3E(SceSblPostSsMgrForDriver_9B49C249);
	SceSysrootForDriver_35875119(ksceSblUtMgrGetTrilithiumBuffer);
	SceSysrootForDriver_EE5D6CE9(ksceSblUtMgrHasNpTestFlag);
	if (utMgrGetCurrentTime(&now) != 0)
		now = 0xffffffff;
	size = 0;
	utoken = utMgrAllocUtoken();
	if (utoken == NULL)
		return SCE_SBL_ERROR_UT_NO_MEMORY;
	ret = utMgrReadFile(utoken, &size);
	if (ret != 0) {
		sceKernelFreeHeapMemory(g_postSsMgrHeap, utoken);
		return ret;
	}
	SceKernelSuspendForDriver_4DF40893(0);
	ret = utSmOpen(utoken, UT_SIZE, now);
	SceKernelSuspendForDriver_2BB92967(0);
	if (ret == 0) {
		g_utMgrLoaded = 1;
		ret = utMgrParseUtoken(utoken, size);
	}
	sceKernelFreeHeapMemory(g_postSsMgrHeap, utoken);
	return ret;
}

/* seal a utoken and keep it */
int ksceSblUtMgrExecuteUtokenSmCommand1(void *utoken, SceSize size)
{
	SceUInt32 now = 0;
	int ret;

	if (utoken == NULL || size < UT_SIZE)
		return SCE_SBL_ERROR_UT_INVALID_ARGUMENT;
	if (utMgrGetCurrentTime(&now) != 0)
		now = 0xffffffff;
	SceKernelSuspendForDriver_4DF40893(0);
	ret = utSmSeal(utoken, UT_SIZE, now);
	SceKernelSuspendForDriver_2BB92967(0);
	if (ret == 0)
		ret = utMgrWriteFile(utoken);
	return ret;
}

/* read the kept utoken and open it */
int utMgrReadUtoken(void *utoken, SceSize size)	/* name unknown */
{
	SceUInt32 now = 0;
	int ret;

	if (utoken == NULL || size < UT_SIZE)
		return SCE_SBL_ERROR_UT_INVALID_ARGUMENT;
	ret = utMgrReadFile(utoken, &size);
	if (ret != 0)
		return ret;
	if (utMgrGetCurrentTime(&now) != 0)
		now = 0xffffffff;
	SceKernelSuspendForDriver_4DF40893(0);
	ret = utSmOpen(utoken, UT_SIZE, now);
	SceKernelSuspendForDriver_2BB92967(0);
	return ret;
}

int ksceSblUtMgrResetUtokenFile(void)
{
	void *utoken;
	int ret;

	utoken = utMgrAllocUtoken();
	if (utoken == NULL)
		return SCE_SBL_ERROR_UT_NO_MEMORY;
	memset(utoken, 0, UT_SIZE);
	ret = utMgrWriteFile(utoken);
	if (ret == 0) {
		ret = sceIoRemove(UT_PATH);
		ret = ret < 0 ? ret : 0;
	}
	sceKernelFreeHeapMemory(g_postSsMgrHeap, utoken);
	return ret;
}

SceUInt8 ksceSblUtMgrHasComTestFlag(void)
{
	return g_utMgrComTestFlag;
}

SceUInt8 ksceSblUtMgrHasStoreFlag(void)
{
	return g_utMgrStoreFlag;
}

SceUInt8 SceSblPostSsMgrForDriver_9D1D8EE5(void)
{
	return g_utMgr9D1D8EE5Flag;
}

SceUInt8 SceSblPostSsMgrForDriver_C93C0A0D(void)
{
	return g_utMgrC93C0A0DFlag;
}

SceUInt8 SceSblPostSsMgrForDriver_1923D80D(void)
{
	return g_utMgr1923D80DFlag;
}

/* whether a process is the one the utoken's communication test is for */
int SceSblPostSsMgrForDriver_128FB35A(SceUID pid)
{
	SceUInt64 authId;

	if (!g_utMgrComTestFlag || SceSblACMgrForKernel_sceSblACMgrGetPaid(pid, &authId) != 0)
		return 0;
	return authId == g_utMgrComTestAuthId;
}

int sceSblUtMgrUpdateUtoken(const void *utoken, SceSize size)
{
	SceUInt32 state = sceSyscallEnter();
	void *kutoken;
	int ret;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(state);
		return SCE_KERNEL_ERROR_NOT_ALLOWED;
	}
	if (utoken == NULL || size < UT_SIZE) {
		sceSyscallLeave(state);
		return SCE_SBL_ERROR_UT_INVALID_ARGUMENT;
	}
	kutoken = utMgrAllocUtoken();
	if (kutoken == NULL) {
		sceSyscallLeave(state);
		return SCE_SBL_ERROR_UT_NO_MEMORY;
	}
	ret = SceSysmemForDriver_BC996A7A(kutoken, utoken, UT_SIZE);
	if (ret == 0)
		ret = ksceSblUtMgrExecuteUtokenSmCommand1(kutoken, UT_SIZE);
	sceKernelFreeHeapMemory(g_postSsMgrHeap, kutoken);
	sceSyscallLeave(state);
	return ret;
}

int sceSblUtMgrReadUtoken(void *utoken, SceSize size)
{
	SceUInt32 state = sceSyscallEnter();
	void *kutoken;
	int ret;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(state);
		return SCE_KERNEL_ERROR_NOT_ALLOWED;
	}
	if (utoken == NULL || size < UT_SIZE) {
		sceSyscallLeave(state);
		return SCE_SBL_ERROR_UT_INVALID_ARGUMENT;
	}
	kutoken = utMgrAllocUtoken();
	if (kutoken == NULL) {
		sceSyscallLeave(state);
		return SCE_SBL_ERROR_UT_NO_MEMORY;
	}
	ret = utMgrReadUtoken(kutoken, UT_SIZE);
	if (ret == 0)
		ret = SceSysmemForDriver_6D88EF8A(utoken, kutoken, UT_SIZE);
	sceKernelFreeHeapMemory(g_postSsMgrHeap, kutoken);
	sceSyscallLeave(state);
	return ret;
}

int sceSblUtMgrResetUtokenFile(void)
{
	SceUInt32 state = sceSyscallEnter();
	int ret;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(state);
		return SCE_KERNEL_ERROR_NOT_ALLOWED;
	}
	ret = ksceSblUtMgrResetUtokenFile();
	sceSyscallLeave(state);
	return ret;
}

/* seconds left of the utoken, or -1 when it does not expire */
int sceSblUtMgrGetCurrentSecureTick(SceUInt32 *left)
{
	SceUInt32 state = sceSyscallEnter();
	SceUInt32 now = 0, value = 0;
	int ret;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(state);
		return SCE_KERNEL_ERROR_NOT_ALLOWED;
	}
	if (left == NULL) {
		sceSyscallLeave(state);
		return SCE_SBL_ERROR_UT_INVALID_ARGUMENT;
	}
	if (!g_utMgrLoaded) {
		sceSyscallLeave(state);
		return SCE_SBL_ERROR_UT_NOT_LOADED;
	}
	ret = utMgrGetCurrentTime(&now);
	if (ret == 0) {
		if (g_utMgrExpireDate == 0)
			value = 0xffffffff;
		else if (now <= g_utMgrExpireDate)
			value = g_utMgrExpireDate - now;
		else
			value = 0;
	} else if (g_utMgrExpireDate == 0xffffffff) {
		value = g_utMgrExpireDate - now;
	} else if (g_utMgrExpireDate == 0) {
		value = 0xffffffff;
	} else {
		sceSyscallLeave(state);
		return ret;
	}
	ret = SceSysmemForDriver_6D88EF8A(left, &value, sizeof(value));
	sceSyscallLeave(state);
	return ret;
}

int sceSblUtMgrIsTrilithiumFlagEnabled(char *flag, SceSize size)
{
	SceUInt32 state = sceSyscallEnter();
	int ret;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(state);
		return SCE_KERNEL_ERROR_NOT_ALLOWED;
	}
	if (size == 0 || flag == NULL) {
		sceSyscallLeave(state);
		return SCE_SBL_ERROR_UT_INVALID_ARGUMENT;
	}
	if (!g_utMgrLoaded) {
		sceSyscallLeave(state);
		return SCE_SBL_ERROR_UT_NOT_LOADED;
	}
	if (size > sizeof(g_utMgrTrilithiumFlag))
		size = sizeof(g_utMgrTrilithiumFlag);
	ret = SceSysmemForDriver_6D88EF8A(flag, g_utMgrTrilithiumFlag, size);
	sceSyscallLeave(state);
	return ret;
}

int utSmStart(const char *path)	/* name unknown */
{
	SceAuthInfo info;
	int ret;

	memset(&info, 0, sizeof(info));
	memcpy(&info.request, &g_utSmAuthInfo, sizeof(info.request));
	ret = SceSblACMgrForKernel_sceSblACMgrGetMediaType(path, &info.media_type);
	if (ret < 0)
		return ret;
	info.self_type = (info.self_type & ~0xf) | 2;
	return ksceSblSmCommStartSmFromFile(0, path, 0, &info, &g_utSmId);
}

/* PLACEHOLDER: cryptographic routine, deferred: utoken_sm services 1
 * and 2, which seal and open a utoken in place */
POST_SS_DEFERRED int utSmCommand(int service, void *utoken, SceSize size, SceUInt32 now)	/* name unknown */
{
	(void)utoken;
	(void)size;
	(void)now;
	if (service != UT_SM_SEAL && service != UT_SM_OPEN)
		return SCE_SBL_ERROR_UT_INVALID_ARGUMENT;
	return SCE_SBL_ERROR_UNSUPPORTED;
}

int utSmOpen(void *utoken, SceSize size, SceUInt32 now)	/* name unknown */
{
	return utSmCommand(UT_SM_OPEN, utoken, size, now);
}

int utSmSeal(void *utoken, SceSize size, SceUInt32 now)	/* name unknown */
{
	return utSmCommand(UT_SM_SEAL, utoken, size, now);
}
