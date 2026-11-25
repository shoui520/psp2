#include "mlndl_internal.h"

static uint32_t s_marlinKindDescriptor[3];
static MlndlMarlinKind s_marlinKind = { -1, s_marlinKindDescriptor };
static MlndlSession s_marlinSession;
static void *s_marlinOpenedHandle;
static uint32_t s_marlinOpenReserved;

static int mlndlConvertError(int code)
{
	if (code == 0)
		return 0;
	if (code > 0 && code < 128)
		return code + (int)MLNDL_ERR_BASE;
	if (code > 128 && code < 256)
		return code + (int)MLNDL_ERR_MARLIN_BASE;
	return code;
}

static int mlndlConvertMarlinResult(int code)
{
	int shifted;

	if (code == 0)
		return 0;
	shifted = code + 128;
	if (shifted == 0)
		return 0;
	if (shifted > 0 && shifted < 128)
		return shifted + (int)MLNDL_ERR_BASE;
	if (shifted > 128 && shifted < 256)
		return shifted + (int)MLNDL_ERR_MARLIN_BASE;
	return shifted;
}

int module_start(uint32_t arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	return 0;
}

int module_stop(uint32_t arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	return 0;
}

int mlndl_27E075AF(void *memory, uint32_t size)
{
	return mlndlHeapInit(0, memory, size);
}

int mlndl_41AC7B0D(void)
{
	mlndlHeapTerm(0);
	return 0;
}

int mlndlOpenSession(MlndlSession **outSession)
{
	int err;

	if (outSession == NULL)
		return MLNDL_ERR_INVALID_ARG;

	s_marlinSession.flags = 0;
	s_marlinSession.kind = &s_marlinKind;
	s_marlinOpenReserved = 0;

	err = SceMarlin_9CD4CE1E(&s_marlinKind, &s_marlinOpenedHandle, NULL);
	if (err == 0 || err == -128) {
		s_marlinSession.marlinHandle = s_marlinOpenedHandle;
		*outSession = &s_marlinSession;
		return 0;
	}
	return mlndlConvertMarlinResult(err);
}

__attribute__((noinline)) int mlndlCloseSession(MlndlSession *session)
{
	int err;

	if (session == NULL)
		return MLNDL_ERR_INVALID_ARG;

	err = SceMarlin_8FEB4F94(session->marlinHandle);
	if (err != 0)
		return mlndlConvertMarlinResult(err);
	return 0;
}

int mlndl_15B43EC9(int *outStatus)
{
	MlndlSession *session = NULL;
	MlndlConfig config;
	int err;

	if (outStatus == NULL) {
		err = MLNDL_ERR_INVALID_ARG;
	} else {
		err = mlndlMarlinInit();
		if (err != 0) {
			err = mlndlConvertError(err);
		} else {
			err = mlndlBuildConfig(MLNDL_CFG_ACCOUNT, NULL, NULL, &config);
			if (err == 0) {
				err = mlndlOpenSession(&session);
				if (err == 0) {
					err = mlndlCheckRegistration(&config,
						MLNDL_MODE_PSP2_DIRECT, session, outStatus);
					if (err != 0)
						err = mlndlConvertError(err);
				}
			}
		}
	}

	mlndlCloseSession(session);
	mlndlMarlinTerm();
	return err;
}

int mlndl_C0F382A6(int32_t *outMaxConsole, int32_t *outCurrentConsole, const char *token)
{
	MlndlSession *session = NULL;
	MlndlStatusInfo statusInfo;
	MlndlConfig config;
	int err;

	err = mlndlMarlinInit();
	if (err != 0) {
		err = mlndlConvertError(err);
	} else {
		err = mlndlBuildConfig(MLNDL_CFG_ACCOUNT | MLNDL_CFG_LOGIN | MLNDL_CFG_PASSWORD
			| MLNDL_CFG_URLS, token, NULL, &config);
		if (err == 0) {
			err = mlndlOpenSession(&session);
			if (err == 0) {
				err = mlndlRegister(&config, MLNDL_MODE_PSP2_DIRECT, session,
					&statusInfo);
				if (err != 0)
					err = mlndlConvertError(err);
			}
		}
	}

	if (outMaxConsole != NULL)
		*outMaxConsole = statusInfo.maxConsole;
	if (outCurrentConsole != NULL)
		*outCurrentConsole = statusInfo.currentConsole;

	mlndlCloseSession(session);
	mlndlMarlinTerm();
	return err;
}

int mlndl_6731EACF(int32_t *outMaxConsole, int32_t *outCurrentConsole, const char *token)
{
	MlndlSession *session = NULL;
	MlndlStatusInfo statusInfo;
	MlndlConfig config;
	int err;

	err = mlndlMarlinInit();
	if (err != 0) {
		err = mlndlConvertError(err);
	} else {
		err = mlndlBuildConfig(MLNDL_CFG_ACCOUNT | MLNDL_CFG_LOGIN | MLNDL_CFG_PASSWORD
			| MLNDL_CFG_URLS, token, NULL, &config);
		if (err == 0) {
			err = mlndlOpenSession(&session);
			if (err == 0) {
				err = mlndlDeregister(&config, MLNDL_MODE_PSP2_DIRECT, session,
					&statusInfo);
				if (err != 0)
					err = mlndlConvertError(err);
			}
		}
	}

	if (outMaxConsole != NULL)
		*outMaxConsole = statusInfo.maxConsole;
	if (outCurrentConsole != NULL)
		*outCurrentConsole = statusInfo.currentConsole;

	mlndlCloseSession(session);
	mlndlMarlinTerm();
	return err;
}

int mlndl_05D8AAAD(const char *contentId, const char *token, uint32_t *outSize, void **outLicense,
	int32_t *outMaxConsole, int32_t *outCurrentConsole)
{
	MlndlSession *session = NULL;
	MlndlStatusInfo statusInfo;
	MlndlConfig config;
	uint32_t licenseSize = 0;
	void *licenseBuf = NULL;
	int regStatus = MLNDL_REG_UNPERSONALIZED;
	int err;

	if (contentId == NULL || outSize == NULL || outLicense == NULL) {
		err = MLNDL_ERR_INVALID_ARG;
	} else {
		err = mlndlMarlinInit();
		if (err != 0) {
			err = mlndlConvertError(err);
		} else {
			err = mlndlOpenSession(&session);
			if (err == 0) {
				err = mlndlBuildConfig(MLNDL_CFG_ACCOUNT | MLNDL_CFG_LOGIN
					| MLNDL_CFG_PASSWORD | MLNDL_CFG_URLS,
					token, NULL, &config);
				if (err == 0) {
					err = mlndlCheckRegistration(&config,
						MLNDL_MODE_PSP2_DIRECT, session, &regStatus);
					if (err != 0) {
						err = mlndlConvertError(err);
					} else {
						if (regStatus != MLNDL_REG_REGISTERED) {
							err = mlndlRegister(&config,
								MLNDL_MODE_PSP2_DIRECT, session,
								&statusInfo);
							if (err != 0)
								err = mlndlConvertError(err);
						}
						if (err == 0) {
							err = mlndlAcquireLicense(&config,
								contentId, MLNDL_MODE_PSP2_DIRECT,
								session, &licenseSize, &licenseBuf);
							if (err != 0) {
								err = mlndlConvertError(err);
							} else {
								*outSize = licenseSize;
								*outLicense = licenseBuf;
								licenseBuf = NULL;
							}
						}
					}
				}
			}
		}
	}

	if (regStatus != MLNDL_REG_REGISTERED) {
		if (outMaxConsole != NULL)
			*outMaxConsole = statusInfo.maxConsole;
		if (outCurrentConsole != NULL)
			*outCurrentConsole = statusInfo.currentConsole;
	}
	if (licenseBuf != NULL)
		mlndlFree(0, licenseBuf);

	mlndlCloseSession(session);
	mlndlMarlinTerm();
	return err;
}

int mlndl_8F409146(void *licenseBuffer)
{
	if (licenseBuffer != NULL)
		mlndlFree(0, licenseBuffer);
	return 0;
}

int mlndl_84E05172(uint32_t *outSize)
{
	MlndlConfig config;
	int err;

	err = mlndlGetMaxLicenseSize(&config, outSize);
	if (err != 0)
		return mlndlConvertError(err);
	return 0;
}

int mlndl_C2783A9F(void)
{
	int err;

	err = mlndlHttpCancel();
	if (err != 0)
		return mlndlConvertError(err);
	return 0;
}

int mlndl_BC6A8671(void)
{
	MlndlSession *session = NULL;
	MlndlStatusInfo statusInfo;
	MlndlConfig config;
	char tokenBuf[128];
	int err;

	err = mlndlMarlinInit();
	if (err != 0) {
		err = mlndlConvertError(err);
	} else {
		err = mlndlFetchNpToken(tokenBuf, sizeof(tokenBuf));
		if (err == 0) {
			err = mlndlBuildConfig(MLNDL_CFG_ACCOUNT | MLNDL_CFG_LOGIN
				| MLNDL_CFG_PASSWORD | MLNDL_CFG_URLS, tokenBuf, NULL, &config);
			if (err == 0) {
				err = mlndlOpenSession(&session);
				if (err == 0) {
					err = mlndlPersonalize(&config, MLNDL_MODE_PSP2_DIRECT,
						session, &statusInfo);
					if (err != 0)
						err = mlndlConvertError(err);
				}
			}
		}
	}

	mlndlCloseSession(session);
	mlndlMarlinTerm();
	return err;
}

int mlndl_14538FCC(int32_t *outMaxConsole, int32_t *outCurrentConsole)
{
	MlndlSession *session = NULL;
	MlndlStatusInfo statusInfo;
	MlndlConfig config;
	char tokenBuf[128];
	int err;

	err = mlndlMarlinInit();
	if (err != 0) {
		err = mlndlConvertError(err);
	} else {
		err = mlndlFetchNpToken(tokenBuf, sizeof(tokenBuf));
		if (err == 0) {
			err = mlndlBuildConfig(MLNDL_CFG_ACCOUNT | MLNDL_CFG_LOGIN
				| MLNDL_CFG_PASSWORD | MLNDL_CFG_URLS, tokenBuf, NULL, &config);
			if (err == 0) {
				err = mlndlOpenSession(&session);
				if (err == 0) {
					err = mlndlRegister(&config, MLNDL_MODE_PSP2_DIRECT,
						session, &statusInfo);
					if (err != 0)
						err = mlndlConvertError(err);
				}
			}
		}
	}

	if (outMaxConsole != NULL)
		*outMaxConsole = statusInfo.maxConsole;
	if (outCurrentConsole != NULL)
		*outCurrentConsole = statusInfo.currentConsole;

	mlndlCloseSession(session);
	mlndlMarlinTerm();
	return err;
}

int mlndl_764AC6DC(int32_t *outMaxConsole, int32_t *outCurrentConsole)
{
	MlndlSession *session = NULL;
	MlndlStatusInfo statusInfo;
	MlndlConfig config;
	char tokenBuf[128];
	int err;

	err = mlndlMarlinInit();
	if (err != 0) {
		err = mlndlConvertError(err);
	} else {
		err = mlndlFetchNpToken(tokenBuf, sizeof(tokenBuf));
		if (err == 0) {
			err = mlndlBuildConfig(MLNDL_CFG_ACCOUNT | MLNDL_CFG_LOGIN
				| MLNDL_CFG_PASSWORD | MLNDL_CFG_URLS, tokenBuf, NULL, &config);
			if (err == 0) {
				err = mlndlOpenSession(&session);
				if (err == 0) {
					err = mlndlDeregister(&config, MLNDL_MODE_PSP2_DIRECT,
						session, &statusInfo);
					if (err != 0)
						err = mlndlConvertError(err);
				}
			}
		}
	}

	if (outMaxConsole != NULL)
		*outMaxConsole = statusInfo.maxConsole;
	if (outCurrentConsole != NULL)
		*outCurrentConsole = statusInfo.currentConsole;

	mlndlCloseSession(session);
	mlndlMarlinTerm();
	return err;
}

int mlndl_5483B464(const char *entitlementId, int *outStatus)
{
	MlndlSession *session = NULL;
	MlndlConfig config;
	int err;

	if (outStatus == NULL || entitlementId == NULL) {
		err = MLNDL_ERR_INVALID_ARG;
	} else {
		err = mlndlMarlinInit();
		if (err != 0) {
			err = mlndlConvertError(err);
		} else {
			err = mlndlBuildConfig(MLNDL_CFG_ACCOUNT | MLNDL_CFG_ENTITLEMENT,
				NULL, entitlementId, &config);
			if (err == 0) {
				err = mlndlOpenSession(&session);
				if (err == 0) {
					err = mlndlCheckEntitlement(&config,
						MLNDL_MODE_PSP2_DIRECT, session, outStatus);
					if (err != 0)
						err = mlndlConvertError(err);
				}
			}
		}
	}

	mlndlCloseSession(session);
	mlndlMarlinTerm();
	return err;
}

int mlndlGetCurrentDateTime(MlndlDateTime *outDateTime)
{
	MlndlRtcTick tick;
	MlndlDateTime dateTime;

	if (outDateTime == NULL || sceDrmBridgeGetCurrentSecureTick(&tick) != 0)
		return MLNDL_ERR_INTERNAL;

	sceRtcSetTick(&dateTime, &tick);
	outDateTime->year = dateTime.year;
	outDateTime->month = dateTime.month;
	outDateTime->day = dateTime.day;
	outDateTime->hour = dateTime.hour;
	outDateTime->minute = dateTime.minute;
	outDateTime->second = dateTime.second;
	outDateTime->microsecond = dateTime.microsecond;
	return 0;
}

int mlndl_C4B91CB9(int32_t *outMaxConsole, int32_t *outCurrentConsole, const char *entitlementId)
{
	MlndlSession *session = NULL;
	MlndlDateTime currentTime;
	MlndlStatusInfo statusInfo;
	MlndlConfig config;
	char tokenBuf[128];
	int err;

	err = mlndlMarlinInit();
	if (err != 0) {
		err = mlndlConvertError(err);
	} else {
		err = mlndlFetchNpToken(tokenBuf, sizeof(tokenBuf));
		if (err == 0) {
			err = mlndlBuildConfig(MLNDL_CFG_ACCOUNT | MLNDL_CFG_LOGIN
				| MLNDL_CFG_PASSWORD | MLNDL_CFG_URLS | MLNDL_CFG_ENTITLEMENT,
				tokenBuf, entitlementId, &config);
			if (err == 0) {
				err = mlndlOpenSession(&session);
				if (err == 0) {
					if (mlndlGetCurrentDateTime(&currentTime) == 0)
						mlndlPurgeExpiredEntitlements(
							MLNDL_MODE_PSP2_DIRECT, session,
							&currentTime);
					err = mlndlSubscribe(&config, MLNDL_MODE_PSP2_DIRECT,
						session, 0, &statusInfo);
					if (err != 0)
						err = mlndlConvertError(err);
				}
			}
		}
	}

	if (outMaxConsole != NULL)
		*outMaxConsole = statusInfo.maxConsole;
	if (outCurrentConsole != NULL)
		*outCurrentConsole = statusInfo.currentConsole;

	mlndlCloseSession(session);
	mlndlMarlinTerm();
	return err;
}

int mlndl_7C51CBF6(int32_t *outMaxConsole, int32_t *outCurrentConsole, const char *entitlementId)
{
	MlndlSession *session = NULL;
	MlndlDateTime currentTime;
	MlndlStatusInfo statusInfo;
	MlndlConfig config;
	char tokenBuf[128];
	int err;

	err = mlndlMarlinInit();
	if (err != 0) {
		err = mlndlConvertError(err);
	} else {
		err = mlndlFetchNpToken(tokenBuf, sizeof(tokenBuf));
		if (err == 0) {
			err = mlndlBuildConfig(MLNDL_CFG_ACCOUNT | MLNDL_CFG_LOGIN
				| MLNDL_CFG_PASSWORD | MLNDL_CFG_URLS | MLNDL_CFG_ENTITLEMENT,
				tokenBuf, entitlementId, &config);
			if (err == 0) {
				err = mlndlOpenSession(&session);
				if (err == 0) {
					if (mlndlGetCurrentDateTime(&currentTime) == 0)
						mlndlPurgeExpiredEntitlements(
							MLNDL_MODE_PSP2_DIRECT, session,
							&currentTime);
					err = mlndlSubscribe(&config, MLNDL_MODE_PSP2_DIRECT,
						session, 1, &statusInfo);
					if (err != 0)
						err = mlndlConvertError(err);
				}
			}
		}
	}

	if (outMaxConsole != NULL)
		*outMaxConsole = statusInfo.maxConsole;
	if (outCurrentConsole != NULL)
		*outCurrentConsole = statusInfo.currentConsole;

	mlndlCloseSession(session);
	mlndlMarlinTerm();
	return err;
}

int mlndl_27A84309(int32_t *outMaxConsole, int32_t *outCurrentConsole, const char *entitlementId)
{
	MlndlSession *session = NULL;
	MlndlDateTime currentTime;
	MlndlStatusInfo statusInfo;
	MlndlConfig config;
	char tokenBuf[128];
	int err;

	err = mlndlMarlinInit();
	if (err != 0) {
		err = mlndlConvertError(err);
	} else {
		err = mlndlFetchNpToken(tokenBuf, sizeof(tokenBuf));
		if (err == 0) {
			err = mlndlBuildConfig(MLNDL_CFG_ACCOUNT | MLNDL_CFG_LOGIN
				| MLNDL_CFG_PASSWORD | MLNDL_CFG_URLS | MLNDL_CFG_ENTITLEMENT,
				tokenBuf, entitlementId, &config);
			if (err == 0) {
				err = mlndlOpenSession(&session);
				if (err == 0) {
					if (mlndlGetCurrentDateTime(&currentTime) == 0)
						mlndlPurgeExpiredEntitlements(
							MLNDL_MODE_PSP2_DIRECT, session,
							&currentTime);
					err = mlndlUnsubscribe(&config, MLNDL_MODE_PSP2_DIRECT,
						session, &statusInfo);
					if (err != 0)
						err = mlndlConvertError(err);
				}
			}
		}
	}

	if (outMaxConsole != NULL)
		*outMaxConsole = statusInfo.maxConsole;
	if (outCurrentConsole != NULL)
		*outCurrentConsole = statusInfo.currentConsole;

	mlndlCloseSession(session);
	mlndlMarlinTerm();
	return err;
}

int mlndlDateTimeToTick(const MlndlDateTime *dateTime, MlndlRtcTick *outTick)
{
	MlndlDateTime clamped;

	if (dateTime->year != 0 && dateTime->year <= 9999)
		clamped.year = dateTime->year;
	if (dateTime->month > 0 && dateTime->month <= 12)
		clamped.month = dateTime->month;
	if (dateTime->day > 0 && dateTime->day <= 31)
		clamped.day = dateTime->day;
	if (dateTime->hour <= 23)
		clamped.hour = dateTime->hour;
	if (dateTime->minute <= 59)
		clamped.minute = dateTime->minute;
	if (dateTime->second <= 59)
		clamped.second = dateTime->second;
	if (dateTime->microsecond < 1000000)
		clamped.microsecond = dateTime->microsecond;

	sceRtcGetTick(&clamped, outTick);
	return 0;
}

int mlndl_E16D6FD9(const char *entitlementId, MlndlDateTime *outStart, MlndlDateTime *outEnd,
	uint32_t *outValidityState)
{
	MlndlSession *session = NULL;
	MlndlRtcTick nowTick;
	MlndlRtcTick startTick;
	MlndlRtcTick endTick;
	MlndlConfig config;
	int err;

	err = mlndlMarlinInit();
	if (err != 0) {
		err = mlndlConvertError(err);
	} else {
		err = mlndlBuildConfig(MLNDL_CFG_ACCOUNT | MLNDL_CFG_ENTITLEMENT,
			NULL, entitlementId, &config);
		if (err == 0) {
			err = mlndlOpenSession(&session);
			if (err == 0) {
				err = mlndlGetEntitlementPeriod(&config, MLNDL_MODE_PSP2_DIRECT,
					session, outStart, outEnd);
				if (err != 0) {
					err = mlndlConvertError(err);
				} else if (outValidityState != NULL) {
					*outValidityState = 0;
					if (sceDrmBridgeGetCurrentSecureTick(&nowTick) != 0) {
						*outValidityState = 3;
					} else {
						mlndlDateTimeToTick(outStart, &startTick);
						if (startTick.tick > nowTick.tick) {
							*outValidityState = 1;
						} else {
							mlndlDateTimeToTick(outEnd, &endTick);
							if (endTick.tick < nowTick.tick)
								*outValidityState = 2;
						}
					}
				}
			}
		}
	}

	mlndlCloseSession(session);
	mlndlMarlinTerm();
	return err;
}
