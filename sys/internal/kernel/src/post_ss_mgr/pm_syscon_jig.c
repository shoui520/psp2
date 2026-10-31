/*
 * pm_syscon_jig.c: talking to pm_sm.self and, through the syscon, to a jig.
 * The product mode is turned on only after pm_sm has authenticated the jig
 * (pm_auth_common, pm_auth2_common); where the syscon keeps the mode, pm_sm
 * reads and writes it in an exchange with the syscon.
 */

#include <acmgr/acmgr.h>
#include <sm_comm/sm_comm.h>
#include <syscon/syscon.h>
#include <threadmgr/threadmgr.h>
#include "post_ss_mgr_internal.h"

#define PM_JIG_CHUNK		0xc	/* name unknown: bytes per jig packet */
#define PM_JIG_TIMEOUT_MS	6000	/* name unknown */
#define PM_JIG_POLL_MS		250

/* the pm_sm services (names unknown) */
#define PM_SM_SET_NVS_MODE	2
#define PM_SM_AUTH_CHALLENGE	3
#define PM_SM_AUTH_EXCHANGE4	4
#define PM_SM_AUTH_VERIFY5	5
#define PM_SM_AUTH_EXCHANGE6	6
#define PM_SM_AUTH_VERIFY7	7
#define PM_SM_SYSCON_MODE	8
#define PM_SM_AUTH_GET_CERT	9
#define PM_SM_AUTH_SET_CERT	10

/* PM_SM_SYSCON_MODE steps (names unknown) */
#define PM_SM_GET_MODE_REQUEST	0
#define PM_SM_GET_MODE_RESPONSE	1
#define PM_SM_SET_MODE_RESPONSE	3

/* a PM_SM_AUTH_* (services 3 to 7) request (names unknown) */
typedef struct ScePmSmAuthCommand {
	int type;
	SceUInt32 version;
	SceUInt32 data[0x28 / 4];
} ScePmSmAuthCommand;

/* a PM_SM_AUTH_*_CERT (services 9 and 10) request (names unknown) */
typedef struct ScePmSmCertCommand {
	int type;
	SceUInt32 reserved;
	SceUInt8 cert[0x104];
} ScePmSmCertCommand;

static SceUInt8 g_pmJigSeq;	/* name unknown */

int pmSmAuthGetChallenge(void *challenge, int type);
int pmSmAuthExchange4(void *data, int type);
int pmSmAuthVerify5(const void *data, int type);
int pmSmAuthExchange6(void *data, int type);
int pmSmAuthVerify7(const void *data, int type);
int pmSmAuthGetCertificate(void *certificate, int type);
int pmSmAuthSetCertificate(const void *certificate, int type);

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

static SceSblSmCommId g_pmSmId;	/* name unknown */

/* the self authentication pm_sm.self runs with */
static const SceSelfAuthInfo g_pmSmAuthInfo = POST_SS_SM_AUTH_INFO;

/* power the multi connector for the jig and open its port */
int pmJigOpen(void)	/* name unknown */
{
	int ret;

	if ((SceUInt32)SceSysconForDriver_FF86F4C5() <= 0x90003) {
		if (sceSysconSetMultiCnPort(0) != 0)
			return SCE_SBL_ERROR_PM_JIG;
		ret = sceSysconCtrlAccPower(0);
	} else {
		ret = sceSysconCtrlHostOutputViaDongle(0);
	}
	if (ret != 0)
		return SCE_SBL_ERROR_PM_JIG;
	if (sceSysconJigOpenPort() != 0)
		return SCE_SBL_ERROR_PM_JIG;
	if (sceSysconJigSetConfig(0, 0) != 0)
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
	if (sceSysconJigClosePort() != 0)
		return SCE_SBL_ERROR_PM_JIG;
	if ((SceUInt32)SceSysconForDriver_FF86F4C5() <= 0x90003) {
		if (sceSysconSetMultiCnPort(0x10000) != 0 || sceSysconCtrlAccPower(1) != 0)
			return SCE_SBL_ERROR_PM_JIG;
	} else if (sceSysconCtrlHostOutputViaDongle(1) != 0) {
		return SCE_SBL_ERROR_PM_JIG;
	}
	if (SceSysconForDriver_CBD6D8BC() & 8)
		SceDebugForDriver_391B74B7("UUU");
	return 0;
}

/* authenticates the jig through pm_sm (challenge and response over pmJigWrite and pmJigRead) */
int pm_auth_common(int type)
{
	SceUInt8 data[0x28];
	int ret, closeRet;

	ret = pmJigOpen();
	if (ret != 0) {
		SCE_DBG_PRINTF(11, 511, "syscon jig setup fail\n");
		return ret;
	}
	sceKernelDelayThread(1000000);
	ret = pmSmAuthGetChallenge(data, type);
	if (ret != 0) {
		SCE_DBG_PRINTF(11, 520, "gen_req_hello fail\n");
		goto close;
	}
	ret = pmJigWrite(data, sizeof(data));
	if (ret != 0) {
		SCE_DBG_PRINTF(11, 527, "send message to SC fail\n");
		goto close;
	}
	memset(data, 0xff, sizeof(data));
	ret = pmJigRead(data, sizeof(data));
	if (ret != 0) {
		SCE_DBG_PRINTF(11, 535, "receive message from SC fail(0x%08X)\n", ret);
		goto close;
	}
	ret = pmSmAuthExchange4(data, type);
	if (ret != 0) {
		SCE_DBG_PRINTF(11, 544, "gen_challenge fail\n");
		goto close;
	}
	ret = pmJigWrite(data, sizeof(data));
	if (ret != 0) {
		SCE_DBG_PRINTF(11, 551, "send message to SC fail\n");
		goto close;
	}
	ret = pmJigRead(data, sizeof(data));
	if (ret != 0) {
		SCE_DBG_PRINTF(11, 559, "receive message from SC fail\n");
		goto close;
	}
	ret = pmSmAuthVerify5(data, type);
	if (ret != 0) {
		SCE_DBG_PRINTF(11, 568, "check_response fail\n");
		goto close;
	}
	if (type == 0xc)
		goto close;
	ret = pmSmAuthExchange6(data, type);
	if (ret != 0) {
		SCE_DBG_PRINTF(11, 582, "gen_req_result fail\n");
		goto close;
	}
	ret = pmJigWrite(data, sizeof(data));
	if (ret != 0) {
		SCE_DBG_PRINTF(11, 591, "send message to SC fail\n");
		goto close;
	}
	ret = pmJigRead(data, sizeof(data));
	if (ret != 0) {
		SCE_DBG_PRINTF(11, 597, "receive message from SC fail\n");
		goto close;
	}
	ret = pmSmAuthVerify7(data, type);
	if (ret != 0)
		SCE_DBG_PRINTF(11, 605, "check_result fail\n");
close:
	closeRet = pmJigClose();
	if (closeRet != 0) {
		SCE_DBG_PRINTF(11, 615, "stop pm_sm fail\n");
		ret = closeRet;
	}
	return ret;
}

/* the second jig authentication through pm_sm */
int pm_auth2_common(int type)
{
	SceUInt8 cert[0x104];
	SceUInt8 pkt[0x28];
	SceUInt8 ack[2];
	SceSize left, offset, n;
	SceUInt8 seq;
	int ret, closeRet;

	if ((type & ~8) != 4)
		return SCE_SBL_ERROR_PM_INVALID_ARGUMENT;
	ret = pmJigOpen();
	if (ret != 0) {
		SCE_DBG_PRINTF(11, 640, "syscon jig setup fail\n");
		return ret;
	}
	sceKernelDelayThread(1000000);
	memset(cert, 0, sizeof(cert));
	ret = pmSmAuthGetCertificate(cert, type);
	if (ret != 0) {
		SCE_DBG_PRINTF(11, 650, "gen_challenge fail\n");
		goto close;
	}
	left = sizeof(cert);
	offset = 0;
	seq = g_pmJigSeq;
	n = 0x26;
	pkt[0] = 0xf1;
	pkt[1] = seq;
	for (;;) {
		memcpy(pkt + 2, cert + offset, n);
		ret = pmJigWrite(pkt, n + 2);
		if (ret != 0) {
			SCE_DBG_PRINTF(11, 657, "send message to SC fail\n");
			goto close;
		}
		if ((signed char)pkt[1] < 0)
			break;
		seq = g_pmJigSeq;
		ret = pmJigWait(10000);
		if (ret != 0) {
			SCE_DBG_PRINTF(11, 657, "send message to SC fail\n");
			goto close;
		}
		ret = SceSysconForDriver_3C80B529(0, 2, ack);
		if (ret != 0) {
			SCE_DBG_PRINTF(11, 657, "send message to SC fail\n");
			goto close;
		}
		if (ack[0] != 0xf2 || (ack[1] & 0x7f) != seq) {
			ret = 0x800f0a24;
			SCE_DBG_PRINTF(11, 657, "send message to SC fail\n");
			goto close;
		}
		left -= n;
		offset += n;
		seq = (g_pmJigSeq + 1) & 0x7f;
		g_pmJigSeq = seq;
		if (left == 0)
			goto recv;
		pkt[0] = 0xf1;
		if (left <= 0x26) {
			pkt[1] = seq | 0x80;
			n = left;
		} else {
			pkt[1] = seq;
			n = 0x26;
		}
	}
	g_pmJigSeq = (g_pmJigSeq + 1) & 0x7f;
recv:
	left = sizeof(cert);
	offset = 0;
	memset(cert, 0, sizeof(cert));
	do {
		n = left < 0x26 ? left : 0x26;
		ret = pmJigRead(pkt, n + 2);
		if (ret != 0) {
			SCE_DBG_PRINTF(11, 665, "receive message from SC fail(0x%08X)\n", ret);
			goto close;
		}
		memcpy(cert + offset, pkt + 2, n);
		if ((signed char)pkt[1] < 0)
			break;
		pkt[0] = 0xf2;
		ret = pmJigWrite(pkt, 2);
		if (ret != 0) {
			SCE_DBG_PRINTF(11, 665, "receive message from SC fail(0x%08X)\n", ret);
			goto close;
		}
		left -= n;
		offset += n;
	} while (left != 0);
	ret = pmSmAuthSetCertificate(cert, type);
	if (ret != 0)
		SCE_DBG_PRINTF(11, 674, "vefiry_response fail\n");
close:
	closeRet = pmJigClose();
	if (closeRet != 0) {
		SCE_DBG_PRINTF(11, 683, "stop pm_sm fail\n");
		ret = closeRet;
	}
	return ret;
}

/* pm_sm service 3 of the jig authentication, a 0x28-byte challenge */
int pmSmAuthGetChallenge(void *challenge, int type)	/* name unknown */
{
	ScePmSmAuthCommand command;
	SceUInt32 result = 0;
	int ret;

	switch (type) {
	case 4:
	case 6:
	case 12:
		command.type = type;
		break;
	case 14:
		command.type = 6;
		break;
	default:
		return SCE_SBL_ERROR_PM_INVALID_ARGUMENT;
	}
	switch (type) {
	case 4:
	case 6:
	case 12:
		command.version = 1;
		break;
	case 14:
		command.version = 2;
		break;
	default:
		return SCE_SBL_ERROR_PM_INVALID_ARGUMENT;
	}
	ret = sceSblSmCommCallFunc(g_pmSmId, PM_SM_AUTH_CHALLENGE, &result, &command,
				    sizeof(command));
	if (ret != 0)
		return ret;
	__builtin_memcpy(challenge, command.data, sizeof(command.data));
	return result;
}

/* pm_sm service 4 of the jig authentication */
int pmSmAuthExchange4(void *data, int type)	/* name unknown */
{
	ScePmSmAuthCommand command;
	SceUInt32 result = 0;
	int ret;

	__builtin_memcpy(command.data, data, sizeof(command.data));
	switch (type) {
	case 4:
	case 6:
	case 12:
		command.type = type;
		break;
	case 14:
		command.type = 6;
		break;
	default:
		return SCE_SBL_ERROR_PM_INVALID_ARGUMENT;
	}
	switch (type) {
	case 4:
	case 6:
	case 12:
		command.version = 1;
		break;
	case 14:
		command.version = 2;
		break;
	default:
		return SCE_SBL_ERROR_PM_INVALID_ARGUMENT;
	}
	ret = sceSblSmCommCallFunc(g_pmSmId, PM_SM_AUTH_EXCHANGE4, &result, &command,
				    sizeof(command));
	if (ret != 0)
		return ret;
	__builtin_memcpy(data, command.data, sizeof(command.data));
	return result;
}

/* pm_sm service 5 of the jig authentication */
int pmSmAuthVerify5(const void *data, int type)	/* name unknown */
{
	ScePmSmAuthCommand command;
	SceUInt32 result = 0;
	int ret;

	__builtin_memcpy(command.data, data, sizeof(command.data));
	switch (type) {
	case 4:
	case 6:
	case 12:
		command.type = type;
		break;
	case 14:
		command.type = 6;
		break;
	default:
		return SCE_SBL_ERROR_PM_INVALID_ARGUMENT;
	}
	switch (type) {
	case 4:
	case 6:
	case 12:
		command.version = 1;
		break;
	case 14:
		command.version = 2;
		break;
	default:
		return SCE_SBL_ERROR_PM_INVALID_ARGUMENT;
	}
	ret = sceSblSmCommCallFunc(g_pmSmId, PM_SM_AUTH_VERIFY5, &result, &command,
				    sizeof(command));
	return ret == 0 ? (int)result : ret;
}

/* pm_sm service 6 of the jig authentication */
int pmSmAuthExchange6(void *data, int type)	/* name unknown */
{
	ScePmSmAuthCommand command;
	SceUInt32 result = 0;
	int ret;

	__builtin_memcpy(command.data, data, sizeof(command.data));
	switch (type) {
	case 4:
	case 6:
	case 12:
		command.type = type;
		break;
	case 14:
		command.type = 6;
		break;
	default:
		return SCE_SBL_ERROR_PM_INVALID_ARGUMENT;
	}
	switch (type) {
	case 4:
	case 6:
	case 12:
		command.version = 1;
		break;
	case 14:
		command.version = 2;
		break;
	default:
		return SCE_SBL_ERROR_PM_INVALID_ARGUMENT;
	}
	ret = sceSblSmCommCallFunc(g_pmSmId, PM_SM_AUTH_EXCHANGE6, &result, &command,
				    sizeof(command));
	if (ret != 0)
		return ret;
	__builtin_memcpy(data, command.data, sizeof(command.data));
	return result;
}

/* pm_sm service 7 of the jig authentication */
int pmSmAuthVerify7(const void *data, int type)	/* name unknown */
{
	ScePmSmAuthCommand command;
	SceUInt32 result = 0;
	int ret;

	__builtin_memcpy(command.data, data, sizeof(command.data));
	switch (type) {
	case 4:
	case 6:
	case 12:
		command.type = type;
		break;
	case 14:
		command.type = 6;
		break;
	default:
		return SCE_SBL_ERROR_PM_INVALID_ARGUMENT;
	}
	switch (type) {
	case 4:
	case 6:
	case 12:
		command.version = 1;
		break;
	case 14:
		command.version = 2;
		break;
	default:
		return SCE_SBL_ERROR_PM_INVALID_ARGUMENT;
	}
	ret = sceSblSmCommCallFunc(g_pmSmId, PM_SM_AUTH_VERIFY7, &result, &command,
				    sizeof(command));
	return ret == 0 ? (int)result : ret;
}

/* pm_sm updates the product mode in the NVS data */
int pmMgrSetNvsMode(int mode, void *nvs)
{
	ScePmSmNvsModeCommand command;
	SceUInt32 result = 0;
	int ret;

	command.nvs = *(const ScePmNvsArea *)nvs;
	command.mode = mode;
	ret = sceSblSmCommCallFunc(g_pmSmId, PM_SM_SET_NVS_MODE, &result, &command,
				    sizeof(command));
	ret = ret == 0 ? (int)result : SCE_SBL_ERROR_PM_SM;
	*(ScePmNvsArea *)nvs = command.nvs;
	return ret;
}

int invoke_pm_secure_module(const char *path)
{
	SelfAuthRequest info;
	int ret;

	memset(&info, 0, sizeof(info));
	memcpy(&info.caller, &g_pmSmAuthInfo, sizeof(info.caller));
	ret = SceSblACMgrForKernel_sceSblACMgrGetMediaType(path, &info.mediaType);
	if (ret < 0)
		return ret;
	info.selfType = (info.selfType & ~0xf) | 2;
	return sceSblSmCommStartSm(0, path, 0, &info, &g_pmSmId);
}

int stop_pm_secure_module(void)
{
	SceSblSmCommPair result;

	return sceSblSmCommStopSm(g_pmSmId, &result);
}

/* pm_sm service 9 of the jig authentication, a 0x104-byte certificate */
int pmSmAuthGetCertificate(void *certificate, int type)	/* name unknown */
{
	ScePmSmCertCommand command;
	SceUInt32 result = 0;
	int ret;

	if (type == 4)
		command.type = 0x48;
	else if (type == 0xc)
		command.type = 0x49;
	else
		return SCE_SBL_ERROR_PM_INVALID_ARGUMENT;
	ret = sceSblSmCommCallFunc(g_pmSmId, PM_SM_AUTH_GET_CERT, &result, &command,
				    sizeof(command));
	if (ret != 0)
		return ret;
	(memcpy)(certificate, command.cert, sizeof(command.cert));
	return result;
}

/* pm_sm service 10 of the jig authentication */
int pmSmAuthSetCertificate(const void *certificate, int type)	/* name unknown */
{
	ScePmSmCertCommand command;
	SceUInt32 result = 0;
	int ret;

	if (type == 4)
		command.type = 0x48;
	else if (type == 0xc)
		command.type = 0x49;
	else
		return SCE_SBL_ERROR_PM_INVALID_ARGUMENT;
	(memcpy)(command.cert, certificate, sizeof(command.cert));
	ret = sceSblSmCommCallFunc(g_pmSmId, PM_SM_AUTH_SET_CERT, &result, &command,
				    sizeof(command));
	return ret == 0 ? (int)result : ret;
}

/* the product mode the syscon keeps, read by pm_sm */
int pmSmGetMode(SceUInt32 *state, SceUInt32 *data)
{
	ScePmSmSysconModeCommand command;
	SceUInt32 result;
	int ret;

	command.step = PM_SM_GET_MODE_REQUEST;
	command.reserved = 0;
	ret = sceSblSmCommCallFunc(g_pmSmId, PM_SM_SYSCON_MODE, &result, &command,
				    sizeof(command));
	if (ret == 0 && (ret = result) == 0 &&
	    (ret = SceSysconForDriver_EBDF88B9(command.request, 0x10, command.response,
					       0x30)) == 0) {
		command.step = PM_SM_GET_MODE_RESPONSE;
		ret = sceSblSmCommCallFunc(g_pmSmId, PM_SM_SYSCON_MODE, &result, &command,
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
	ret = sceSblSmCommCallFunc(g_pmSmId, PM_SM_SYSCON_MODE, &result, &command,
				    sizeof(command));
	if (ret == 0 && (ret = result) == 0 &&
	    (ret = SceSysconForDriver_63683B9B(command.request, 0x30, command.response,
					       0x10)) == 0) {
		command.step = PM_SM_SET_MODE_RESPONSE;
		ret = sceSblSmCommCallFunc(g_pmSmId, PM_SM_SYSCON_MODE, &result, &command,
					    sizeof(command));
		if (ret == 0)
			ret = result;
	}
	return ret;
}
