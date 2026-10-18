/*
 * licmgr.c: SceSblLicMgr, the activation of a development kit.
 * An activation code file ("VITA/ActivationCode") lists one code per
 * console; the code for this console's OpenPsId is checked by act_sm.self
 * and kept in tm0:activate (act.dat, actsig.dat) and in the NVS.
 */

#include <iofilemgr/iofilemgr.h>
#include <sysmem/sblaimgr.h>
#include <psp2kern/kernel/kbl/kbl.h>
#include <psp2kern/kernel/rtc.h>
#include <sm_comm/sm_comm.h>
#include <sysmem/sysmem.h>
#include <psp2kern/kernel/threadmgr/misc.h>
#include <intrmgr/syscall.h>
#include "post_ss_mgr_internal.h"
#include <acmgr/acmgr.h>
#include <sysmem/sysroot.h>

#define SCE_SBL_ERROR_LIC_KBL			0x800f1028	/* names unknown */
#define SCE_SBL_ERROR_LIC_NOT_FOUND		0x800f1302
#define SCE_SBL_ERROR_LIC_NO_TIME		0x800f1305
#define SCE_SBL_ERROR_LIC_INVALID_ARGUMENT	0x800f1316
#define SCE_SBL_ERROR_LIC_OVERFLOW		0x800f1322
#define SCE_SBL_ERROR_LIC_INVALID_FILE		0x800f1324
#define SCE_SBL_ERROR_LIC_UNSUPPORTED		0x800f1325
#define SCE_SBL_ERROR_LIC_NO_DATA		0x800f1326
#define SCE_SBL_ERROR_LIC_NO_MEMORY		0x800f1328
#define SCE_SBL_ERROR_LIC_NOT_ACTIVATED		0x800f1329
#define SCE_SBL_ERROR_LIC_NOT_DEVELOPMENT	0x800f132f
#define SCE_SBL_ERROR_LIC_NO_SECURE_TICK	0x800f1331
#define SCE_SBL_ERROR_LIC_NOT_DEVELOPMENT2	0x800f1333
#define SCE_SBL_ERROR_LIC_OTHER_CONSOLE		0x800f1339
#define SCE_SBL_ERROR_RTC_NO_CP			0x800f1002	/* name unknown */
#define SCE_RTC_ERROR_BEFORE_EPOCH		0x80251002	/* name unknown */

#define ACT_DIR			"tm0:activate"
#define ACT_DAT_PATH		"tm0:activate/act.dat"
#define ACT_SIG_PATH		"tm0:activate/actsig.dat"
#define ACT_SM_PATH		"os0:sm/act_sm.self"
#define ACT_FS_DIR		"ux0:/data/activate/"

#define ACT_NVS_OFFSET		0x520	/* name unknown */
#define ACT_FILE_HEADER_MAX	0x400	/* names unknown */
#define ACT_FILE_SIZE_MIN	0xf0
#define ACT_FILE_SIZE_MAX	0x10000000
#define ACT_CODE_NUM_MAX	0x80000
#define ACT_CODE_SIZE_MAX	0x100
#define ACT_CODE_SIZE_MIN	0x20
#define ACT_EXTRA_SIZE_MAX	0x400
#define ACT_CODES_PER_READ	0x10

/* the act_sm services (names unknown) */
#define ACT_SM_DECRYPT_NVS	4
#define ACT_SM_ACTIVATE		10
#define ACT_SM_CHECK		11

/* SceRtcTick of 1970-01-01 (name unknown) */
#define RTC_TICK_UNIX_EPOCH	0xdcbffeff2bc000ULL

/* the activation kept in the NVS (names unknown) */
typedef struct SceActNvsData {
	char magic[4];
	SceUInt32 issueNo;
	SceUInt32 endDate;
	SceUInt8 reserved[0x14];
} SceActNvsData;

/* act.dat, one code of the activation code file (names unknown) */
typedef struct SceActData {
	char magic[4];
	SceUInt8 version;
	SceUInt8 reserved0[3];
	SceUInt32 issueNo;
	SceUInt32 startDate;
	SceUInt32 endDate;
	SceOpenPsId openPsId;
	SceUInt8 reserved[0x1c];
	SceUInt8 key[0x40];
} SceActData;

/* actsig.dat (name unknown) */
typedef struct SceActSig {
	SceUInt8 signature[0x100];
} SceActSig;

/* the activation key: the OpenPsId and a byte-wise sum of its words (names unknown) */
typedef struct SceActivationKey {
	SceOpenPsId openPsId;
	SceUInt8 sum[4];
} SceActivationKey;

int ksceSblQafManagerGetQAFlags(void *flags);
int SceSblACMgrForKernel_sceSblACMgrGetMediaType(const char *path, SceUInt32 *mediaType);
int SceRtcForDriver_A0D7899A(SceRtcTick *tick);

int ksceSblPostSsMgrActivate(const char *path);
int ksceSblPostSsMgrGetExpireDate(SceUInt32 *date, int fromNvs);
int licMgrHexDecode(const char *hex, void *data, SceSize size);
int licMgrGetHeaderValue(const char *header, SceSize size, SceUInt32 *value, const char *key);
int licMgrParseCodeFile(SceActData *act, SceActSig *sig, const char *path);
int actSmDecryptNvs(SceActNvsData *nvs);
int actSmCheck(int timeResult, SceUInt32 now, const SceActNvsData *nvs, const SceActData *act,
	       const SceActSig *sig, SceUInt32 *endDate, int *status);
int actSmActivate(const SceActData *act, const SceActSig *sig, SceActNvsData *nvs,
		  const SceActData *oldAct, const SceActSig *oldSig);

static const char g_actMagic[4] = "act";	/* name unknown */

int g_licMgrLicenseStatus = -1;		/* name unknown */
int g_licMgrNewlineSize = 1;		/* name unknown: 2 for "\r\n" */
SceUInt32 g_licMgrIssueNo;		/* name unknown */
SceUInt32 g_licMgrEndDate;		/* name unknown */
int g_licMgrCodeFileExact;		/* name unknown: the last code has no newline */
static SceSblSmCommId g_actSmId;	/* name unknown */

int ksceSblLicMgrGetLicenseStatus(void)
{
	return g_licMgrLicenseStatus;
}

static inline int licMgrIsActivationNeeded(void)	/* name unknown */
{
	return (SceSblAIMgrForDriver_274663A0() && !SceSblAIMgrForDriver_37A79140()) || SceSblAIMgrForDriver_F4B98F66();
}

/* the current time in seconds since 1970 */
int licMgrGetCurrentTime(SceUInt32 *now)	/* name unknown */
{
	SceKblParam *kbl;
	SceRtcTick tick;
	int ret;

	if (SceSblAIMgrForDriver_274663A0()) {
		kbl = SceSysrootForKernel_9DB56D1F();
		if (kbl == NULL)
			return SCE_SBL_ERROR_LIC_KBL;
		*now = kbl->dipsw.cp_timestamp_2;
		if (*now == 0)
			return SCE_SBL_ERROR_LIC_NOT_ACTIVATED;
		if (SceRtcForDriver_A0D7899A(&tick) != 0)
			return SCE_SBL_ERROR_LIC_NO_MEMORY;
		if (tick.tick < RTC_TICK_UNIX_EPOCH)
			return SCE_SBL_ERROR_LIC_NOT_ACTIVATED;
	} else {
		if (!SceSblAIMgrForDriver_F4B98F66())
			return SCE_SBL_ERROR_LIC_NO_SECURE_TICK;
		ret = ksceRtcGetCurrentSecureTick(&tick);
		if (ret != 0)
			return ret;
		if (tick.tick < RTC_TICK_UNIX_EPOCH)
			return SCE_RTC_ERROR_BEFORE_EPOCH;
	}
	*now = (tick.tick - RTC_TICK_UNIX_EPOCH) / 1000000;
	return 0;
}

int licMgrReadFile(const char *path, void *data, SceSize size)	/* name unknown */
{
	SceUID fd;

	fd = sceIoOpen(path, SCE_O_RDONLY, 0);
	if (fd < 0)
		return fd;
	if (sceIoRead(fd, data, size) != size)
		return SCE_ERROR_ERRNO_EIO;
	sceIoClose(fd);
	return 0;
}

#if FW_VERSION >= 0x04000000
/* whether any QA flag is set */
int licMgrHasQaFlags(void)	/* name unknown */
{
	SceUInt8 flags[0x10];
	SceUInt8 any;
	int i;

	if (ksceSblQafManagerGetQAFlags(flags) != 0)
		return 0;
	any = 0;
	for (i = 0; i < sizeof(flags); i++)
		any |= flags[i];
	return any != 0;
}

int licMgrReadNvs(SceActNvsData *nvs)	/* name unknown */
{
	int ret;

	memset(nvs, 0, sizeof(*nvs));
	ret = ksceSblNvsReadData(ACT_NVS_OFFSET, nvs, sizeof(*nvs));
	if (ret != 0)
		return ret;
	if (memcmp(nvs, g_actMagic, sizeof(nvs->magic)) != 0)
		return SCE_SBL_ERROR_LIC_NO_DATA;
	SceKernelSuspendForDriver_4DF40893(0);
	ret = actSmDecryptNvs(nvs);
	SceKernelSuspendForDriver_2BB92967(0);
	return ret;
}
#endif

/* check the activation kept in tm0:activate and the NVS */
int licMgrLoad(void)	/* name unknown */
{
	SceActNvsData nvs;
	SceActData act;
	SceActSig sig;
	SceUInt32 now = 0;
	int ret;

	if (!licMgrIsActivationNeeded()) {
		g_licMgrEndDate = 0xffffffff;
		g_licMgrLicenseStatus = 0;
		g_licMgrIssueNo = 0;
		return 0;
	}
	ret = licMgrGetCurrentTime(&now);
	memset(&nvs, 0, sizeof(nvs));
	SceKernelSuspendForDriver_4DF40893(0);
	ksceSblNvsReadData(ACT_NVS_OFFSET, &nvs, sizeof(nvs));
	SceKernelSuspendForDriver_2BB92967(0);
	memset(&act, 0, sizeof(act));
	licMgrReadFile(ACT_DAT_PATH, &act, sizeof(act));
	memset(&sig, 0, sizeof(sig));
	licMgrReadFile(ACT_SIG_PATH, &sig, sizeof(sig));
	SceKernelSuspendForDriver_4DF40893(0);
	ret = actSmCheck(ret, now, &nvs, &act, &sig, &g_licMgrEndDate, &g_licMgrLicenseStatus);
	SceKernelSuspendForDriver_2BB92967(0);
	return ret;
}

#if FW_VERSION < 0x04000000
/* whether any QA flag is set */
int licMgrHasQaFlags(void)	/* name unknown */
{
	SceUInt8 flags[0x10];
	SceUInt8 any;
	int i;

	if (ksceSblQafManagerGetQAFlags(flags) != 0)
		return 0;
	any = 0;
	for (i = 0; i < sizeof(flags); i++)
		any |= flags[i];
	return any != 0;
}

int licMgrReadNvs(SceActNvsData *nvs)	/* name unknown */
{
	int ret;

	memset(nvs, 0, sizeof(*nvs));
	ret = ksceSblNvsReadData(ACT_NVS_OFFSET, nvs, sizeof(*nvs));
	if (ret != 0)
		return ret;
	if (memcmp(nvs, g_actMagic, sizeof(nvs->magic)) != 0)
		return SCE_SBL_ERROR_LIC_NO_DATA;
	SceKernelSuspendForDriver_4DF40893(0);
	ret = actSmDecryptNvs(nvs);
	SceKernelSuspendForDriver_2BB92967(0);
	return ret;
}
#endif

int licMgrWriteFile(const char *path, const void *data, SceSize size)	/* name unknown */
{
	SceUID fd;
	int ret;

	ret = sceIoMkdir(ACT_DIR, 0606);
	if (ret != SCE_ERROR_ERRNO_EEXIST && ret < 0)
		return ret;
	fd = sceIoOpen(path, SCE_O_RDWR | SCE_O_CREAT | SCE_O_TRUNC, 0606);
	if (fd < 0)
		return fd;
	ret = sceIoWrite(fd, data, size) == size ? 0 : SCE_ERROR_ERRNO_EIO;
	if (fd != 0)
		sceIoClose(fd);
	return ret;
}

int licMgrClearActivationData(void)	/* name unknown */
{
	SceActNvsData nvs;
	SceActData act;
	SceActSig sig;
	int ret;

	memset(&nvs, 0xff, sizeof(nvs));
	memset(&act, 0xff, sizeof(act));
	memset(&sig, 0xff, sizeof(sig));
	SceKernelSuspendForDriver_4DF40893(0);
	ret = ksceSblNvsWriteData(ACT_NVS_OFFSET, &nvs, sizeof(nvs));
	if (ret == 0 && (ret = licMgrWriteFile(ACT_DAT_PATH, &act, sizeof(act))) == 0)
		ret = licMgrWriteFile(ACT_SIG_PATH, &sig, sizeof(sig));
	SceKernelSuspendForDriver_2BB92967(0);
	return ret;
}

/* activate from the code for this console in an activation code file */
int licMgrActivate(const char *path)	/* name unknown */
{
	SceActNvsData nvs;
	SceOpenPsId openPsId;
	SceActData act, oldAct;
	SceActSig sig, oldSig;
	SceUInt32 now = 0;
	int ret;

	memset(&act, 0, sizeof(act));
	memset(&sig, 0, sizeof(sig));
	if (SceSblAIMgrForDriver_274663A0() && !SceSblACMgrForKernel_BBA13D9C())
		return SCE_SBL_ERROR_LIC_NOT_DEVELOPMENT2;
	if (path == NULL)
		return SCE_SBL_ERROR_LIC_INVALID_ARGUMENT;
	if (path[0] == '\0')
		return 0;
	if (strncmp(path, "host0:", 6) != 0 &&
	    strncmp(path, ACT_FS_DIR, 0x13) != 0 &&
	    strncmp(path, "ur0:/temp/activation", 0x14) != 0)
		return SCE_SBL_ERROR_LIC_INVALID_ARGUMENT;

	SceKernelSuspendForDriver_4DF40893(0);
	ret = licMgrParseCodeFile(&act, &sig, path);
	SceKernelSuspendForDriver_2BB92967(0);
	if (ret != 0)
		return ret;
	ret = licMgrGetCurrentTime(&now);
	if (ret == SCE_SBL_ERROR_LIC_KBL || ret == SCE_SBL_ERROR_RTC_NO_CP ||
	    ret == SCE_SBL_ERROR_LIC_NOT_ACTIVATED)
		return SCE_SBL_ERROR_LIC_NO_TIME;
	if (ret != 0)
		return ret;
	if (memcmp(&act, g_actMagic, sizeof(act.magic)) != 0)
		return SCE_SBL_ERROR_LIC_INVALID_FILE;
	ret = ksceSblAimgrGetOpenPsId(&openPsId);
	if (ret != 0)
		return ret;
	if (memcmp(&act.openPsId, &openPsId, sizeof(openPsId)) != 0)
		return SCE_SBL_ERROR_LIC_OTHER_CONSOLE;

	memset(&nvs, 0, sizeof(nvs));
	SceKernelSuspendForDriver_4DF40893(0);
	ret = ksceSblNvsReadData(ACT_NVS_OFFSET, &nvs, sizeof(nvs));
	SceKernelSuspendForDriver_2BB92967(0);
	if (ret != 0)
		return ret;
	memset(&oldAct, 0, sizeof(oldAct));
	licMgrReadFile(ACT_DAT_PATH, &oldAct, sizeof(oldAct));
	memset(&oldSig, 0, sizeof(oldSig));
	licMgrReadFile(ACT_SIG_PATH, &oldSig, sizeof(oldSig));
	SceKernelSuspendForDriver_4DF40893(0);
	ret = actSmActivate(&act, &sig, &nvs, &oldAct, &oldSig);
	SceKernelSuspendForDriver_2BB92967(0);
	if (ret != 0)
		return ret;
	SceKernelSuspendForDriver_4DF40893(0);
	ret = ksceSblNvsWriteData(ACT_NVS_OFFSET, &nvs, sizeof(nvs));
	if (ret == 0 && (ret = licMgrWriteFile(ACT_DAT_PATH, &act, sizeof(act))) == 0)
		ret = licMgrWriteFile(ACT_SIG_PATH, &sig, sizeof(sig));
	SceKernelSuspendForDriver_2BB92967(0);
	return ret;
}

/* the end of the activation in seconds from now, or (fromNvs == 0) as loaded */
int ksceSblPostSsMgrGetExpireDate(SceUInt32 *date, int fromNvs)
{
	SceActNvsData nvs;
	SceUInt32 now = 0, end;
	int ret;

	if (!licMgrIsActivationNeeded()) {
		*date = 0xffffffff;
		return 0;
	}
	if (fromNvs == 0) {
		*date = g_licMgrEndDate;
		return 0;
	}
	ret = licMgrGetCurrentTime(&now);
	if (ret == SCE_SBL_ERROR_RTC_NO_CP || ret == SCE_SBL_ERROR_LIC_KBL) {
		*date = 0;
		return SCE_SBL_ERROR_LIC_NO_TIME;
	}
	if (ret != 0) {
		*date = 0;
		return ret;
	}
	SceKernelSuspendForDriver_4DF40893(0);
	ret = licMgrReadNvs(&nvs);
	SceKernelSuspendForDriver_2BB92967(0);
	end = ret == 0 ? nvs.endDate : g_licMgrEndDate;
	*date = end < now ? 0 : end - now;
	return ret;
}

int licMgrGetIssueNo(SceUInt32 *issueNo, int fromNvs)	/* name unknown */
{
	SceActNvsData nvs;
	int ret;

	if (!licMgrIsActivationNeeded())
		*issueNo = 0;
	if (fromNvs == 0) {
		*issueNo = g_licMgrIssueNo;
		return 0;
	}
	ret = licMgrReadNvs(&nvs);
	if (ret == 0)
		*issueNo = nvs.issueNo;
	return ret;
}

int sceSblLicMgrInit(void)
{
	int ret;

	ret = licMgrLoad();
	SceSysrootForKernel_71DB83A2(ksceSblLicMgrGetLicenseStatus);
	return ret;
}

/* the path of the first "*.afv" file in dir */
int licMgrFindCodeFile(const char *dir, char *path, SceSize size)	/* name unknown */
{
	SceIoDirent dirent;
	SceUID dfd;
	SceSize len;
	char *afv, *dot;

	dfd = sceIoDopen(dir);
	if (dfd < 0)
		return SCE_SBL_ERROR_LIC_NOT_FOUND;
	do {
		do {
			if (sceIoDread(dfd, &dirent) <= 0) {
				sceIoDclose(dfd);
				return SCE_SBL_ERROR_LIC_NOT_FOUND;
			}
		} while (strnlen(dirent.d_name, sizeof(dirent.d_name)) >= sizeof(dirent.d_name));
		afv = strstr(dirent.d_name, ".afv");
		dot = strrchr(dirent.d_name, '.');
	} while (afv == NULL || dot == NULL || afv != dot);
	sceIoDclose(dfd);

	len = strnlen(dirent.d_name, sizeof(dirent.d_name));
	if (len >= size || len + strnlen(dir, size) >= size)
		return SCE_SBL_ERROR_LIC_INVALID_ARGUMENT;
	memcpy(path, dir, strnlen(dir, size));
	memcpy(path + strnlen(dir, size), dirent.d_name, strnlen(dirent.d_name, sizeof(dirent.d_name)));
	path[strnlen(dir, size) + strnlen(dirent.d_name, sizeof(dirent.d_name))] = '\0';
	return 0;
}

int ksceSblPostSsMgrActivate(const char *path)
{
	if (SceSblAIMgrForDriver_274663A0()) {
		if (SceSblAIMgrForDriver_37A79140() && !SceSblAIMgrForDriver_F4B98F66())
			return SCE_SBL_ERROR_LIC_UNSUPPORTED;
		if (!SceSblACMgrForKernel_BBA13D9C())
			return SCE_SBL_ERROR_LIC_NOT_DEVELOPMENT;
	} else if (!SceSblAIMgrForDriver_F4B98F66()) {
		return SCE_SBL_ERROR_LIC_UNSUPPORTED;
	}
	return licMgrActivate(path);
}

#if FW_VERSION >= 0x04000000
int licMgrActivateFromFs(void *arg)	/* name unknown */
#else
int licMgrActivateFromFs(void)
#endif
{
	char path[0x100];
	int ret;

#if FW_VERSION >= 0x04000000
	(void)arg;
#endif
	memset(path, 0, sizeof(path));
	ret = licMgrFindCodeFile(ACT_FS_DIR, path, sizeof(path));
	if (ret == 0) {
		ret = ksceSblPostSsMgrActivate(path);
		if (ret == SCE_SBL_ERROR_LIC_NOT_FOUND)
			ret = SCE_SBL_ERROR_LIC_OTHER_CONSOLE;
	}
	return ret;
}

int ksceSblLicMgrGetActivationKey(SceActivationKey *key)
{
	SceUInt8 sum[4];
	int ret, i, j;

	ret = ksceSblAimgrGetOpenPsId(&key->openPsId);
	if (ret != 0)
		return ret;
	*(SceUInt32 *)sum = 0;
	for (i = 0; i < sizeof(key->openPsId); i += 4)
		for (j = 0; j < 4; j++)
			sum[j] += ((const SceUInt8 *)&key->openPsId)[i + j];
	*(SceUInt32 *)key->sum = *(SceUInt32 *)sum;
	return 0;
}

int sceSblLicMgrGetIssueNoForUser(SceUInt32 *issueNo, int fromNvs)
{
	SceUInt32 value;
	SceUInt32 state;
	int ret;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0))
		return SCE_KERNEL_ERROR_NOT_ALLOWED;
	if (issueNo == NULL)
		return SCE_SBL_ERROR_LIC_INVALID_ARGUMENT;
	state = sceSyscallEnter();
	ret = licMgrGetIssueNo(&value, fromNvs);
	if (ret == 0)
		ret = SceSysmemForDriver_6D88EF8A(issueNo, &value, sizeof(value));
	sceSyscallLeave(state);
	return ret;
}

int sceSblLicMgrGetActivationKeyForUser(SceActivationKey *key)
{
	SceActivationKey value;
	SceUInt32 state;
	int ret;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0))
		return SCE_KERNEL_ERROR_NOT_ALLOWED;
	if (key == NULL)
		return SCE_SBL_ERROR_LIC_INVALID_ARGUMENT;
	state = sceSyscallEnter();
	ret = ksceSblLicMgrGetActivationKey(&value);
	if (ret == 0)
		ret = SceSysmemForDriver_6D88EF8A(key, &value, sizeof(value));
	sceSyscallLeave(state);
	return ret;
}

int sceSblLicMgrGetLicenseStatus(void)
{
	SceUInt32 state;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0))
		return SCE_KERNEL_ERROR_NOT_ALLOWED;
	state = sceSyscallEnter();
	sceSyscallLeave(state);
	return g_licMgrLicenseStatus;
}

/* seconds left of the activation */
int sceSblLicMgrGetUsageTimeLimit(SceUInt32 *limit)
{
	SceUInt32 now = 0, left;
	SceUInt32 state;
	int ret;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0))
		return SCE_KERNEL_ERROR_NOT_ALLOWED;
	if (limit == NULL)
		return SCE_SBL_ERROR_LIC_INVALID_ARGUMENT;
	state = sceSyscallEnter();
	if (g_licMgrLicenseStatus == 2) {
		sceSyscallLeave(state);
		return SCE_SBL_ERROR_LIC_NOT_ACTIVATED;
	}
	ret = licMgrGetCurrentTime(&now);
	if (ret != 0) {
		if (!licMgrHasQaFlags()) {
			sceSyscallLeave(state);
			return ret;
		}
		now = 0x4e9fb874;	/* 2011-11-01 */
	}
	ret = ksceSblPostSsMgrGetExpireDate(&left, 0);
	left = now < left ? left - now : 0;
	if (ret == SCE_SBL_ERROR_LIC_NO_DATA) {
		if (licMgrHasQaFlags()) {
			left = ~now;
			ret = SceSysmemForDriver_6D88EF8A(limit, &left, sizeof(left));
		}
	} else if (ret == 0) {
		if (licMgrHasQaFlags())
			left = ~now;
		ret = SceSysmemForDriver_6D88EF8A(limit, &left, sizeof(left));
	}
	sceSyscallLeave(state);
	return ret;
}

int sceSblLicMgrGetExpireDateForUser(SceUInt32 *date, int fromNvs)
{
	SceUInt32 value;
	SceUInt32 state;
	int ret;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0))
		return SCE_KERNEL_ERROR_NOT_ALLOWED;
	if (date == NULL)
		return SCE_SBL_ERROR_LIC_INVALID_ARGUMENT;
	state = sceSyscallEnter();
	if (g_licMgrLicenseStatus == 2) {
		sceSyscallLeave(state);
		return SCE_SBL_ERROR_LIC_NOT_ACTIVATED;
	}
	ret = ksceSblPostSsMgrGetExpireDate(&value, fromNvs);
	if (ret == 0)
		ret = SceSysmemForDriver_6D88EF8A(date, &value, sizeof(value));
	sceSyscallLeave(state);
	return ret;
}

int sceSblLicMgrActivateDevkitForUser(const char *path)
{
	char kpath[0x100];
	SceUInt32 state;
	int ret;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0))
		return SCE_KERNEL_ERROR_NOT_ALLOWED;
	if (path == NULL)
		return SCE_SBL_ERROR_LIC_INVALID_ARGUMENT;
	state = sceSyscallEnter();
	memset(kpath, 0, sizeof(kpath));
	ret = SceSysmemForDriver_DB3EC244(kpath, path, sizeof(kpath));
	if (ret >= 0) {
		if (ret == sizeof(kpath)) {
			sceSyscallLeave(state);
			return SCE_SBL_ERROR_LIC_INVALID_ARGUMENT;
		}
		ret = ksceSblPostSsMgrActivate(kpath);
	}
	sceSyscallLeave(state);
	return ret;
}

int sceSblLicMgrClearActivationData(void)
{
	SceUInt32 state = sceSyscallEnter();
	int ret;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(state);
		return SCE_KERNEL_ERROR_NOT_ALLOWED;
	}
	ret = licMgrClearActivationData();
	sceSyscallLeave(state);
	return ret;
}

int sceSblLicMgrActivateFromFs(void)
{
	SceUInt32 state = sceSyscallEnter();
	int ret;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(state);
		return SCE_KERNEL_ERROR_NOT_ALLOWED;
	}
#if FW_VERSION >= 0x04000000
	ret = SceThreadmgrForDriver_E54FD746(0x1000, licMgrActivateFromFs, NULL);
#else
	ret = licMgrActivateFromFs();
#endif
	sceSyscallLeave(state);
	return ret;
}

static inline int licMgrHexDigit(SceUInt8 c, SceUInt8 *digit)	/* name unknown */
{
	if (c - '0' < 10)
		*digit = c - '0';
	else if (c - 'a' < 6)
		*digit = c - 'a' + 10;
	else if (c - 'A' < 6)
		*digit = c - 'A' + 10;
	else
		return 0;
	return 1;
}

int licMgrHexDecode(const char *hex, void *data, SceSize size)
{
	const SceUInt8 *s = (const SceUInt8 *)hex;
	SceUInt8 *out = data;
	SceUInt8 hi, lo;

	for (; out != (SceUInt8 *)data + size; s += 2) {
		if (!licMgrHexDigit(s[0], &hi) || !licMgrHexDigit(s[1], &lo))
			return SCE_SBL_ERROR_LIC_INVALID_ARGUMENT;
		*out++ = hi * 0x10 + lo;
	}
	return 0;
}

/* the blanks of the activation code file ('\n' ends a field) */
static inline __attribute__((always_inline)) int licMgrIsBlank(SceUInt8 c)	/* name unknown */
{
	return c == '\t' || c == '\v' || c == '\f' || c == '\r' || c == ' ';
}

static inline __attribute__((always_inline)) int licMgrIsDigit(SceUInt8 c)	/* name unknown */
{
	return c - '0' < 10;
}

static inline __attribute__((always_inline)) int licMgrIsFieldEnd(SceUInt8 c)	/* name unknown */
{
	return c == ',' || c == '\0' || c == '\n';
}

/* the decimal number at s, of at most len characters */
static inline __attribute__((always_inline)) int licMgrParseDecimal(const SceUInt8 *s, SceSize len, SceUInt32 *value,
				     int invalid)	/* name unknown */
{
	SceInt64 v;
	SceSize i;

	if (len == 0 || !licMgrIsDigit(s[0]))
		return invalid;
	v = (int)(s[0] - '0');
	for (i = 1; i != len && licMgrIsDigit(s[i]); i++) {
		v = v * 10 + (int)(s[i] - '0');
		if (v >> 32)
			return SCE_SBL_ERROR_LIC_OVERFLOW;
	}
	*value = v;
	return 0;
}

/* the value of "key = number" in the header */
int licMgrGetHeaderValue(const char *header, SceSize size, SceUInt32 *value, const char *key)
{
	const char *p;
	SceSize pos;

	p = strstr(header, key);
	if (p == NULL || p - header >= size)
		return SCE_SBL_ERROR_LIC_NOT_FOUND;
	p = strchr(header + (p - header), '=');
	if (p == NULL)
		return SCE_SBL_ERROR_LIC_NOT_FOUND;
	pos = p - header + 1;
	if (pos >= size)
		return SCE_SBL_ERROR_LIC_NOT_FOUND;
	while (licMgrIsBlank(header[pos])) {
		if (++pos >= size)
			return SCE_SBL_ERROR_LIC_NOT_FOUND;
	}
	return licMgrParseDecimal((const SceUInt8 *)header + pos, size - pos, value,
				  SCE_SBL_ERROR_LIC_INVALID_ARGUMENT);
}

/* read size bytes, retrying while the read is interrupted */
SceSSize licMgrReadAll(SceUID fd, void *data, SceSize size)	/* name unknown */
{
	SceSize done;
	int n;

	for (done = 0; done < size; ) {
		n = sceIoRead(fd, (char *)data + done, size - done);
		done += n;
		if (n <= 0 && n != SCE_ERROR_ERRNO_EAGAIN)
			return SCE_ERROR_ERRNO_EIO;
	}
	return done;
}

/* the next decimal field of a code: after the field end, then blanks */
static inline __attribute__((always_inline)) int licMgrParseCodeField(const SceUInt8 *code, SceSize size, SceSize *pos,
				       SceUInt32 *value)	/* name unknown */
{
	SceSize i = *pos, n;
	int ret;

	while (licMgrIsBlank(code[i])) {
		if (++i >= size)
			return SCE_SBL_ERROR_LIC_INVALID_FILE;
	}
	ret = licMgrParseDecimal(code + i, size - i, value, SCE_SBL_ERROR_LIC_INVALID_FILE);
	if (ret != 0)
		return ret;
	for (n = 0; n != size - i && !licMgrIsFieldEnd(code[i + n]); n++)
		;
	i += n + 1;
	if (i >= size)
		return SCE_SBL_ERROR_LIC_INVALID_FILE;
	*pos = i;
	return 0;
}

/* one code: "openpsid,start,end,issue,key" */
static inline __attribute__((always_inline)) int licMgrParseCode(SceActData *act, const SceUInt8 *code, SceSize size)	/* name unknown */
{
	SceSize pos;
	int ret;

	if (size < ACT_CODE_SIZE_MIN)
		return SCE_SBL_ERROR_LIC_INVALID_FILE;
	memset(act, 0, sizeof(*act));
	memcpy(act->magic, g_actMagic, sizeof(act->magic));
	act->version = 1;
	ret = licMgrHexDecode((const char *)code, &act->openPsId, sizeof(act->openPsId));
	if (ret != 0)
		return ret;
	for (pos = 0; pos != size && !licMgrIsFieldEnd(code[pos]); pos++)
		;
	if (++pos >= size)
		return SCE_SBL_ERROR_LIC_INVALID_FILE;
	if ((ret = licMgrParseCodeField(code, size, &pos, &act->startDate)) != 0 ||
	    (ret = licMgrParseCodeField(code, size, &pos, &act->endDate)) != 0 ||
	    (ret = licMgrParseCodeField(code, size, &pos, &act->issueNo)) != 0)
		return ret;
	while (licMgrIsBlank(code[pos]))
		pos++;
	if (pos + sizeof(act->key) * 2 >= size)
		return SCE_SBL_ERROR_LIC_INVALID_FILE;
	ret = licMgrHexDecode((const char *)code + pos, act->key, sizeof(act->key));
	return ret == 0 ? pos : ret;
}

/* find this console's code in an activation code file */
int licMgrParseCodeFile(SceActData *act, SceActSig *sig, const char *path)
{
	SceOpenPsId openPsId;
	SceUInt32 value, codeSize = 0, extraSize = 0;
	SceUInt32 left, chunk, found, i;
	SceSize headerSize, bufSize, len;
	SceSSize n;
	SceOff size;
	SceUID fd;
	char *buf, *p;
	int ret, newline;

	g_licMgrCodeFileExact = 0;
	g_licMgrNewlineSize = 1;
	ret = ksceSblAimgrGetOpenPsId(&openPsId);
	if (ret != 0)
		return ret;
	fd = sceIoOpen(path, SCE_O_RDONLY, 0);
	if (fd < 0) {
		ret = SCE_ERROR_ERRNO_ENOENT;
		goto out;
	}
	size = sceIoLseek(fd, 0, SCE_SEEK_END);
	if (size < 0 || size >= ACT_FILE_SIZE_MAX) {
		ret = SCE_ERROR_ERRNO_EIO;
		goto out;
	}
	if (size < ACT_FILE_SIZE_MIN) {
		ret = SCE_SBL_ERROR_LIC_INVALID_FILE;
		goto out;
	}
	if (sceIoLseek(fd, 0, SCE_SEEK_SET) < 0) {
		ret = SCE_ERROR_ERRNO_EIO;
		goto out;
	}

	/* the header, up to the line after the last '#' */
	buf = SceSysmemForDriver_7B4CB60A(g_postSsMgrHeap, ACT_FILE_HEADER_MAX);
	if (buf == NULL) {
		ret = SCE_SBL_ERROR_LIC_NO_MEMORY;
		goto out;
	}
	memset(buf, 0, ACT_FILE_HEADER_MAX);
	len = size < ACT_FILE_HEADER_MAX - 1 ? size : ACT_FILE_HEADER_MAX - 1;
	n = licMgrReadAll(fd, buf, len);
	if (n != len || n < 0) {
		sceKernelFreeHeapMemory(g_postSsMgrHeap, buf);
		ret = SCE_ERROR_ERRNO_EIO;
		goto out;
	}
	p = strrchr(buf, '#');
	if (p == NULL || (p = strchr(p, '\n')) == NULL)
		goto bad_header;
	headerSize = p - buf + 1;
	p = strchr(buf, '\r');
	newline = p != NULL && p - buf + 1 < headerSize && p[1] == '\n' ? 2 : 1;
	g_licMgrNewlineSize = newline;
	if (strstr(buf, "VITA/ActivationCode") == NULL ||
	    licMgrGetHeaderValue(buf, headerSize, &value, "format_version") != 0 || value != 1 ||
	    licMgrGetHeaderValue(buf, headerSize, &value, "code_num") != 0 ||
	    value - 1 >= ACT_CODE_NUM_MAX ||
	    licMgrGetHeaderValue(buf, headerSize, &codeSize, "code_size") != 0 ||
	    codeSize - 1 >= ACT_CODE_SIZE_MAX)
		goto bad_header;
	if (newline == 2)
		codeSize++;
	ret = licMgrGetHeaderValue(buf, headerSize, &extraSize, "extra_data_size");
	if (ret == 0) {
		if (extraSize > ACT_EXTRA_SIZE_MAX)
			goto bad_header;
		if (newline == 2)
			extraSize++;
	} else if (ret == SCE_SBL_ERROR_LIC_NOT_FOUND) {
		extraSize = 0;
	} else {
		goto bad_header;
	}
	len = codeSize * value + headerSize;
	if (len == size + newline)
		g_licMgrCodeFileExact = 1;
	else if (len >= size + newline)
		goto bad_header;
	sceKernelFreeHeapMemory(g_postSsMgrHeap, buf);

	/* the codes, ACT_CODES_PER_READ at a time */
	if (sceIoLseek(fd, headerSize, SCE_SEEK_SET) < 0) {
		ret = SCE_ERROR_ERRNO_EIO;
		goto out;
	}
	bufSize = codeSize * ACT_CODES_PER_READ;
	if (extraSize > bufSize)
		bufSize = extraSize;
	buf = SceSysmemForDriver_7B4CB60A(g_postSsMgrHeap, bufSize);
	if (buf == NULL) {
		ret = SCE_SBL_ERROR_LIC_NO_MEMORY;
		goto out;
	}
	found = 0;
	for (left = value; left != 0; left -= chunk) {
		memset(buf, 0, codeSize * ACT_CODES_PER_READ);
		chunk = left <= ACT_CODES_PER_READ ? left : ACT_CODES_PER_READ;
		len = chunk * codeSize;
		if (left <= ACT_CODES_PER_READ && g_licMgrCodeFileExact)
			len -= g_licMgrNewlineSize;
		n = licMgrReadAll(fd, buf, len);
		if (n != len || n < 0) {
			ret = SCE_SBL_ERROR_LIC_NO_TIME;
			goto free;
		}
		for (i = 0; i < chunk; i++) {
			ret = licMgrParseCode(act, (const SceUInt8 *)buf + codeSize * i, codeSize);
			found++;
			if (ret < 0) {
				ret = SCE_SBL_ERROR_LIC_INVALID_FILE;
				goto free;
			}
			if (memcmp(&act->openPsId, &openPsId, sizeof(openPsId)) == 0)
				goto extra;
			memset(act->magic, 0, sizeof(act->magic));
		}
	}
	ret = SCE_SBL_ERROR_LIC_NOT_FOUND;
	goto free;

extra:
	/* the signature of the code found, in the extra data after the codes */
	ret = 0;
	if (extraSize != 0) {
		len = value * codeSize + headerSize;
		if (sceIoLseek(fd, extraSize * (found - 1) + len, SCE_SEEK_SET) < 0) {
			ret = SCE_ERROR_ERRNO_EIO;
		} else {
			extraSize -= g_licMgrNewlineSize;
			memset(buf, 0, extraSize);
			n = licMgrReadAll(fd, buf, extraSize);
			if (n < 0 || n != extraSize)
				ret = SCE_SBL_ERROR_LIC_NO_TIME;
			else if (n != sizeof(*sig) * 2 || licMgrHexDecode(buf, sig, sizeof(*sig)) < 0)
				ret = SCE_SBL_ERROR_LIC_INVALID_FILE;
		}
	}
free:
	sceKernelFreeHeapMemory(g_postSsMgrHeap, buf);
out:
	if (fd > 0)
		sceIoClose(fd);
	return ret;

bad_header:
	sceKernelFreeHeapMemory(g_postSsMgrHeap, buf);
	ret = SCE_SBL_ERROR_LIC_INVALID_FILE;
	goto out;
}

/* the self authentication act_sm.self runs with */
static const SceSelfAuthInfo g_actSmAuthInfo = POST_SS_SM_AUTH_INFO;

int invoke_act_secure_module(const char *path)	/* name unknown */
{
	SceAuthInfo info;
	int ret;

	memset(&info, 0, sizeof(info));
	memcpy(&info.request, &g_actSmAuthInfo, sizeof(info.request));
	ret = SceSblACMgrForKernel_sceSblACMgrGetMediaType(path, &info.media_type);
	if (ret < 0)
		return ret;
	info.self_type = (info.self_type & ~0xf) | 2;
	return ksceSblSmCommStartSmFromFile(0, path, 0, &info, &g_actSmId);
}

/* PLACEHOLDER: cryptographic routine, deferred: act_sm service 4,
 * decrypts the activation kept in the NVS */
POST_SS_DEFERRED int actSmDecryptNvs(SceActNvsData *nvs)
{
	(void)nvs;
	return SCE_SBL_ERROR_UNSUPPORTED;
}

/* PLACEHOLDER: cryptographic routine, deferred: act_sm service 11,
 * verifies the activation and gives its end date and the license status */
POST_SS_DEFERRED int actSmCheck(int timeResult, SceUInt32 now, const SceActNvsData *nvs,
				const SceActData *act, const SceActSig *sig, SceUInt32 *endDate,
				int *status)
{
	(void)timeResult;
	(void)now;
	(void)nvs;
	(void)act;
	(void)sig;
	(void)endDate;
	(void)status;
	return SCE_SBL_ERROR_UNSUPPORTED;
}

/* PLACEHOLDER: cryptographic routine, deferred: act_sm service 10,
 * verifies a new activation code and gives the NVS data for it */
POST_SS_DEFERRED int actSmActivate(const SceActData *act, const SceActSig *sig,
				   SceActNvsData *nvs, const SceActData *oldAct,
				   const SceActSig *oldSig)
{
	(void)act;
	(void)sig;
	(void)nvs;
	(void)oldAct;
	(void)oldSig;
	return SCE_SBL_ERROR_UNSUPPORTED;
}
