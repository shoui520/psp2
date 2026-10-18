/*
 * pm_syscon_jig.c: talking to pm_sm.self and, through the syscon, to a jig.
 * The product mode is turned on only after pm_sm has authenticated the jig
 * (pm_auth_common, pm_auth2_common); where the syscon keeps the mode, pm_sm
 * reads and writes it in an exchange with the syscon.
 */

#include <acmgr/acmgr.h>
#include <sm_comm/sm_comm.h>
#include <syscon/syscon.h>
#include <psp2kern/kernel/threadmgr.h>
#include "post_ss_mgr_internal.h"

#define PM_JIG_CHUNK		0xc	/* name unknown: bytes per jig packet */
#define PM_JIG_TIMEOUT_MS	6000	/* name unknown */
#define PM_JIG_POLL_MS		250

/* the pm_sm services (names unknown) */
#define PM_SM_SET_NVS_MODE	2
#define PM_SM_SYSCON_MODE	8

/* PM_SM_SYSCON_MODE steps (names unknown) */
#define PM_SM_GET_MODE_REQUEST	0
#define PM_SM_GET_MODE_RESPONSE	1
#define PM_SM_SET_MODE_RESPONSE	3

/* the NVS bytes the product mode lives in (name unknown) */
typedef struct ScePmNvsArea {
	SceUInt32 value[8];
} ScePmNvsArea;

/* a PM_SM_SET_NVS_MODE request (names unknown) */
typedef struct ScePmSmNvsModeCommand {
	SceUInt32 mode;
	SceUInt32 reserved;
	ScePmNvsArea nvs;
} ScePmSmNvsModeCommand;

/* a PM_SM_SYSCON_MODE request: pm_sm's packet for the syscon and its reply (names unknown) */
typedef struct ScePmSmSysconModeCommand {
	SceUInt32 step;
	SceUInt32 reserved;
	SceUInt32 state;
	SceUInt32 data;
	SceUInt8 request[0x30];
	SceUInt8 response[0x30];
} ScePmSmSysconModeCommand;

int SceSblACMgrForKernel_sceSblACMgrGetMediaType(const char *path, SceUInt32 *mediaType);

static SceSblSmCommId g_pmSmId;	/* name unknown */

/* the self authentication pm_sm.self runs with */
static const SceSelfAuthInfo g_pmSmAuthInfo = POST_SS_SM_AUTH_INFO;

/* power the multi connector for the jig and open its port */
int pmJigOpen(void)	/* name unknown */
{
	int ret;

	if ((SceUInt32)SceSysconForDriver_FF86F4C5() <= 0x90003) {
		if (SceSysconForDriver_8AAB6308(0) != 0)
			return SCE_SBL_ERROR_PM_JIG;
		ret = SceSysconForDriver_8D1D97E8(0);
	} else {
		ret = SceSysconForDriver_DECCB2B4(0);
	}
	if (ret != 0)
		return SCE_SBL_ERROR_PM_JIG;
	if (SceSysconForDriver_44A173F5() != 0)
		return SCE_SBL_ERROR_PM_JIG;
	if (SceSysconForDriver_D24BF916(0, 0) != 0)
		return SCE_SBL_ERROR_PM_JIG;
	return 0;
}

/* send size bytes to the jig */
int pmJigWrite(const SceUInt8 *data, SceSize size)	/* name unknown */
{
	SceSize offset = 0, left = size, n;
	int ret;

	while (left != 0) {
		n = left < PM_JIG_CHUNK ? left : PM_JIG_CHUNK;
		ret = SceSysconForDriver_7BFBA09E(0, offset, n, data + offset);
		if (ret != 0)
			return ret;
		offset += n;
		left -= n;
	}
	return SceSysconForDriver_933D813F(1, 0, size);
}

/* wait for the jig to answer */
int pmJigWait(SceUInt32 timeout)	/* name unknown */
{
	SceUInt32 waited = 0;
	int received, done;

	do {
		SceSysconForDriver_0D0B6D25();
		received = SceSysconForDriver_4BC63A40();
		waited += PM_JIG_POLL_MS;
		SceSysconForDriver_EBE3262C();
		done = SceSysconForDriver_29CF4335();
		if (received || done)
			return 0;
		sceKernelDelayThread(PM_JIG_POLL_MS * 1000);
	} while (waited <= timeout);
	return SCE_SBL_ERROR_PM_TIMEOUT;
}

/* receive size bytes from the jig */
int pmJigRead(SceUInt8 *data, SceSize size)	/* name unknown */
{
	SceSize offset = 0, n;
	int ret;

	ret = pmJigWait(PM_JIG_TIMEOUT_MS);
	if (ret != 0)
		return ret;
	while (size != 0) {
		n = size < PM_JIG_CHUNK ? size : PM_JIG_CHUNK;
		ret = SceSysconForDriver_3C80B529(offset, n, data + offset);
		if (ret != 0)
			return ret;
		offset += n;
		size -= n;
	}
	return 0;
}

/* close the jig port and give the multi connector back */
int pmJigClose(void)	/* name unknown */
{
	if (SceSysconForDriver_483FAE05() != 0)
		return SCE_SBL_ERROR_PM_JIG;
	if ((SceUInt32)SceSysconForDriver_FF86F4C5() <= 0x90003) {
		if (SceSysconForDriver_8AAB6308(0x10000) != 0 || SceSysconForDriver_8D1D97E8(1) != 0)
			return SCE_SBL_ERROR_PM_JIG;
	} else if (SceSysconForDriver_DECCB2B4(1) != 0) {
		return SCE_SBL_ERROR_PM_JIG;
	}
	if (SceSysconForDriver_CBD6D8BC() & 8)
		SceDebugForDriver_391B74B7("UUU");
	return 0;
}

/* PLACEHOLDER: cryptographic routine, deferred: authenticates the
 * jig through pm_sm (challenge and response over pmJigWrite and pmJigRead) */
POST_SS_DEFERRED int pm_auth_common(int type, const char *path)
{
	(void)path;
	if (type != 0xe)
		return SCE_SBL_ERROR_PM_INVALID_ARGUMENT;
	return SCE_SBL_ERROR_UNSUPPORTED;
}

/* PLACEHOLDER: cryptographic routine, deferred: the second jig
 * authentication through pm_sm */
POST_SS_DEFERRED int pm_auth2_common(int type)
{
	if (type != 4 && type != 0xc)
		return SCE_SBL_ERROR_PM_INVALID_ARGUMENT;
	return SCE_SBL_ERROR_UNSUPPORTED;
}

/* PLACEHOLDER: cryptographic routine, deferred: pm_sm service 3 of
 * the jig authentication, a 0x28-byte challenge */
POST_SS_DEFERRED int pmSmAuthGetChallenge(void *challenge, int type)	/* name unknown */
{
	(void)type;
	SceSblSsMgrForDriver_CD98CC92(challenge, 0, 0x28);
	return SCE_SBL_ERROR_PM_INVALID_ARGUMENT;
}

/* PLACEHOLDER: cryptographic routine, deferred: pm_sm service 4 of
 * the jig authentication */
POST_SS_DEFERRED int pmSmAuthExchange4(void *data, int type)	/* name unknown */
{
	(void)type;
	SceSblSsMgrForDriver_CD98CC92(data, 0, 0x28);
	return SCE_SBL_ERROR_PM_INVALID_ARGUMENT;
}

/* PLACEHOLDER: cryptographic routine, deferred: pm_sm service 5 of
 * the jig authentication */
POST_SS_DEFERRED int pmSmAuthVerify5(const void *data, int type)	/* name unknown */
{
	(void)data;
	(void)type;
	return SCE_SBL_ERROR_PM_INVALID_ARGUMENT;
}

/* PLACEHOLDER: cryptographic routine, deferred: pm_sm service 6 of
 * the jig authentication */
POST_SS_DEFERRED int pmSmAuthExchange6(void *data, int type)	/* name unknown */
{
	(void)type;
	SceSblSsMgrForDriver_CD98CC92(data, 0, 0x28);
	return SCE_SBL_ERROR_PM_INVALID_ARGUMENT;
}

/* PLACEHOLDER: cryptographic routine, deferred: pm_sm service 7 of
 * the jig authentication */
POST_SS_DEFERRED int pmSmAuthVerify7(const void *data, int type)	/* name unknown */
{
	(void)data;
	(void)type;
	return SCE_SBL_ERROR_PM_INVALID_ARGUMENT;
}

/* pm_sm updates the product mode in the NVS data */
int pmMgrSetNvsMode(int mode, void *nvs)
{
	ScePmSmNvsModeCommand command;
	SceUInt32 result = 0;
	int ret;

	command.nvs = *(const ScePmNvsArea *)nvs;
	command.mode = mode;
	ret = SceSblSmCommForKernel_DB9FC204(g_pmSmId, PM_SM_SET_NVS_MODE, &result, &command,
				    sizeof(command));
	ret = ret == 0 ? (int)result : SCE_SBL_ERROR_PM_SM;
	*(ScePmNvsArea *)nvs = command.nvs;
	return ret;
}

int invoke_pm_secure_module(const char *path)
{
	SceAuthInfo info;
	int ret;

	memset(&info, 0, sizeof(info));
	memcpy(&info.request, &g_pmSmAuthInfo, sizeof(info.request));
	ret = SceSblACMgrForKernel_sceSblACMgrGetMediaType(path, &info.media_type);
	if (ret < 0)
		return ret;
	info.self_type = (info.self_type & ~0xf) | 2;
	return SceSblSmCommForKernel_7863A0CC(0, path, 0, &info, &g_pmSmId);
}

int stop_pm_secure_module(void)
{
	SceSblSmCommPair result;

	return SceSblSmCommForKernel_0631F8ED(g_pmSmId, &result);
}

/* PLACEHOLDER: cryptographic routine, deferred: pm_sm service 9 of
 * the jig authentication, a 0x104-byte certificate */
POST_SS_DEFERRED int pmSmAuthGetCertificate(void *certificate, int type)	/* name unknown */
{
	(void)type;
	SceSblSsMgrForDriver_CD98CC92(certificate, 0, 0x104);
	return SCE_SBL_ERROR_PM_INVALID_ARGUMENT;
}

/* PLACEHOLDER: cryptographic routine, deferred: pm_sm service 10 of
 * the jig authentication */
POST_SS_DEFERRED int pmSmAuthSetCertificate(const void *certificate, int type)	/* name unknown */
{
	(void)certificate;
	(void)type;
	return SCE_SBL_ERROR_PM_INVALID_ARGUMENT;
}

/* the product mode the syscon keeps, read by pm_sm */
int pmSmGetMode(SceUInt32 *state, SceUInt32 *data)
{
	ScePmSmSysconModeCommand command;
	SceUInt32 result;
	int ret;

	command.step = PM_SM_GET_MODE_REQUEST;
	command.reserved = 0;
	ret = SceSblSmCommForKernel_DB9FC204(g_pmSmId, PM_SM_SYSCON_MODE, &result, &command,
				    sizeof(command));
	if (ret == 0 && (ret = result) == 0 &&
	    (ret = SceSysconForDriver_EBDF88B9(command.request, 0x10, command.response,
					       0x30)) == 0) {
		command.step = PM_SM_GET_MODE_RESPONSE;
		ret = SceSblSmCommForKernel_DB9FC204(g_pmSmId, PM_SM_SYSCON_MODE, &result, &command,
					    sizeof(command));
		if (ret == 0 && (ret = result) == 0) {
			*state = command.state;
			*data = command.data;
		}
	}
	return ret;
}

/* the product mode the syscon keeps, written by pm_sm */
int pmSmSetMode(SceUInt32 *state, SceUInt32 *data, int mode)
{
	ScePmSmSysconModeCommand command;
	SceUInt32 result;
	int ret;

	command.state = *state;
	command.data = *data;
	command.reserved = 0;
	command.step = mode;
	ret = SceSblSmCommForKernel_DB9FC204(g_pmSmId, PM_SM_SYSCON_MODE, &result, &command,
				    sizeof(command));
	if (ret == 0 && (ret = result) == 0 &&
	    (ret = SceSysconForDriver_63683B9B(command.request, 0x30, command.response,
					       0x10)) == 0) {
		command.step = PM_SM_SET_MODE_RESPONSE;
		ret = SceSblSmCommForKernel_DB9FC204(g_pmSmId, PM_SM_SYSCON_MODE, &result, &command,
					    sizeof(command));
		if (ret == 0)
			ret = result;
	}
	return ret;
}
