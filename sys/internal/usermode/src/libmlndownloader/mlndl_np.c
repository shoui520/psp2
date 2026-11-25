#include "mlndl_internal.h"

#define MLNDL_NP_WORK_BUF_SIZE 0x10000

typedef struct MlndlNpTokenResult {
	uint8_t header[92];
	const char *token;
} MlndlNpTokenResult;

static uint8_t s_accountIdBuf[8];
static uint32_t s_accountIdLen;
static uint8_t s_entitlementIdBuf[132];
static uint32_t s_entitlementIdLen;
static char s_loginIdBuf[65];
static char s_passwordBuf[129];
static char s_personalizeUrlBuf[257];
static char s_registerUrlBuf[257];
static char s_deregisterUrlBuf[257];
static char s_licenseUrlBuf[257];
static char s_subscribeUrlBuf[257];
static char s_unsubscribeUrlBuf[257];
static char s_serviceProviderUrnBuf[48];

static uint32_t mlndlNpStrLenLimit(const char *str, uint32_t limit)
{
	uint32_t len = 0;

	if (str == NULL)
		return limit;
	while (str[len] != '\0') {
		len++;
		if (len > limit)
			return limit;
	}
	return len;
}

int mlndlFetchNpToken(char *outToken, uint32_t tokenBufSize)
{
	void *workBuf;
	uint32_t ctxId = 0;
	uint32_t reqId = 0;
	uint32_t resultSize = 0;
	uint32_t sessionReqId = 0;
	uint32_t sessionSize = 0;
	uint8_t userInfo[32];
	uint8_t sessionInfo[96];
	MlndlNpTokenResult tokenResult;
	int sessionCreated = 0;
	int err;

	workBuf = mlndlAlloc(0, MLNDL_NP_WORK_BUF_SIZE);
	if (workBuf == NULL) {
		err = MLNDL_ERR_OUT_OF_MEMORY;
	} else {
		err = SceNpCommerce2_AA090F5E(2, &ctxId);
		if (err == 0) {
			err = SceNpCommerce2_2C6028A0(ctxId, userInfo);
			if (err == 0) {
				err = sceNpCommerce2CreateSessionCreateReq(ctxId, &sessionReqId);
				if (err >= 0) {
					err = sceNpCommerce2CreateSessionStart(sessionReqId);
					if (err >= 0)
						err = sceNpCommerce2CreateSessionGetResult(
							sessionReqId, workBuf,
							MLNDL_NP_WORK_BUF_SIZE, &sessionSize);
					sceNpCommerce2DestroyReq(sessionReqId);
					sessionReqId = 0;
				}
				SceNpCommerce2_1BFF9C73(sessionInfo, workBuf, sessionSize);
				if (sessionReqId != 0)
					sceNpCommerce2DestroyReq(sessionReqId);

				if (err == 0) {
					sessionCreated = 1;
					err = SceNpCommerce2_8ECAE666(ctxId, 6);
					if (err == 0) {
						err = SceNpCommerce2_07C16199(ctxId, &reqId);
						if (err >= 0) {
							err = SceNpCommerce2_D35F548B(reqId);
							if (err >= 0)
								err = SceNpCommerce2_B7387C96(
									reqId, workBuf,
									MLNDL_NP_WORK_BUF_SIZE,
									&resultSize);
							sceNpCommerce2DestroyReq(reqId);
							reqId = 0;
							if (err >= 0) {
								SceNpCommerce2_5953F48D(
									&tokenResult, workBuf,
									resultSize);
								mlndlStrNCpy(outToken,
									tokenResult.token,
									tokenBufSize);
							}
						}
					}
				}
			}
		}
	}

	if (sessionCreated) {
		uint32_t closeReqId = 0;
		uint32_t closeSize = 0;
		if (workBuf != NULL && SceNpCommerce2_C737A929(ctxId, &closeReqId) >= 0) {
			if (SceNpCommerce2_2DC2A0F9(closeReqId) >= 0)
				SceNpCommerce2_7E3C9196(closeReqId, workBuf,
					MLNDL_NP_WORK_BUF_SIZE, &closeSize);
			sceNpCommerce2DestroyReq(closeReqId);
		}
	}

	SceNpCommerce2_1EC16D19(ctxId);
	if (workBuf != NULL)
		mlndlFree(0, workBuf);
	return err;
}

int mlndlBuildMdsUrl(const char *servicePath, const char *npEnv, char *outUrl)
{
	static const char urlFormat[] = "https://mds.%s.ac.playstation.net/%s";
	uint32_t pathLen = mlndlNpStrLenLimit(servicePath, 256);
	uint32_t fmtLen = mlndlNpStrLenLimit(urlFormat, 256);
	uint32_t envLen = mlndlNpStrLenLimit(npEnv, 256);
	int totalLen = (int)(fmtLen + envLen + pathLen);

	if (totalLen > 256)
		return MLNDL_ERR_INTERNAL;

	mlndlFormatString(outUrl, totalLen, urlFormat, npEnv, servicePath);
	return 0;
}

int mlndlBuildConfig(uint8_t flags, const char *token, const char *entitlementId,
	MlndlConfig *outConfig)
{
	uint8_t accountId[8];
	char npEnv[16];
	uint8_t onlineId[36];
	char loginId[68];
	int err = 0;

	if (outConfig == NULL)
		return MLNDL_ERR_INVALID_ARG;
	if ((flags & MLNDL_CFG_PASSWORD) != 0 && token == NULL)
		return MLNDL_ERR_INVALID_ARG;
	if ((flags & MLNDL_CFG_ENTITLEMENT) != 0 && entitlementId == NULL)
		return MLNDL_ERR_INVALID_ARG;

	if (token != NULL && mlndlNpStrLenLimit(token, 129) >= 129)
		return MLNDL_ERR_INVALID_ARG;
	if (entitlementId != NULL && mlndlNpStrLenLimit(entitlementId, 129) >= 129)
		return MLNDL_ERR_INVALID_ARG;

	if ((flags & (MLNDL_CFG_LOGIN | MLNDL_CFG_ACCOUNT)) != 0) {
		err = SceNpManager_DDF30B0E(loginId, onlineId, accountId);
		if (err != 0)
			return err;
	}

	mlndlMemSet(outConfig, 0, sizeof(MlndlConfig));

	if ((flags & MLNDL_CFG_LOGIN) != 0) {
		mlndlStrNCpy(s_loginIdBuf, loginId, 64);
		outConfig->loginId = s_loginIdBuf;
	}
	if ((flags & MLNDL_CFG_PASSWORD) != 0) {
		mlndlStrNCpy(s_passwordBuf, token, 128);
		outConfig->password = s_passwordBuf;
	}
	if ((flags & MLNDL_CFG_ENTITLEMENT) != 0) {
		uint32_t entLen = mlndlNpStrLenLimit(entitlementId, 129);
		s_entitlementIdLen = entLen;
		mlndlMemCpy(s_entitlementIdBuf, entitlementId, entLen);
		outConfig->entitlementIdLen = (int32_t)s_entitlementIdLen;
		outConfig->entitlementId = s_entitlementIdBuf;
	}
	if ((flags & MLNDL_CFG_ACCOUNT) != 0) {
		s_accountIdLen = 8;
		mlndlMemCpy(s_accountIdBuf, accountId, 8);
		outConfig->accountIdLen = (int32_t)s_accountIdLen;
		outConfig->accountId = s_accountIdBuf;
	}

	mlndlMemCpy(s_serviceProviderUrnBuf, "urn:marlin:organization:sne:service-provider:1", 47);
	outConfig->serviceProviderUrn = s_serviceProviderUrnBuf;

	if ((flags & MLNDL_CFG_URLS) != 0) {
		mlndlMemSet(npEnv, 0, sizeof(npEnv));
		err = sceNpCommonGetNpEnviroment(npEnv);
		if (err < 0)
			return err;

		err = mlndlBuildMdsUrl("ws/services/personalize", npEnv, s_personalizeUrlBuf);
		if (err == 0)
			err = mlndlBuildMdsUrl("ws/services/registrationActionToken", npEnv,
				s_registerUrlBuf);
		if (err == 0)
			err = mlndlBuildMdsUrl("ws/services/licenseActionToken", npEnv,
				s_licenseUrlBuf);
		if (err == 0)
			err = mlndlBuildMdsUrl("ws/services/deregistrationActionToken", npEnv,
				s_deregisterUrlBuf);
		if (err == 0)
			err = mlndlBuildMdsUrl("ws/services/subscriptionActionToken", npEnv,
				s_subscribeUrlBuf);
		if (err == 0)
			err = mlndlBuildMdsUrl("ws/services/unsubscriptionActionToken", npEnv,
				s_unsubscribeUrlBuf);
		if (err == 0) {
			outConfig->personalizeUrl = s_personalizeUrlBuf;
			outConfig->registerUrl = s_registerUrlBuf;
			outConfig->deregisterUrl = s_deregisterUrlBuf;
			outConfig->licenseUrl = s_licenseUrlBuf;
			outConfig->subscribeUrl = s_subscribeUrlBuf;
			outConfig->unsubscribeUrl = s_unsubscribeUrlBuf;
		}
	}

	return err;
}
