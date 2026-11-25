#include "mlndl_internal.h"

static int32_t s_cbHttpError;
static int32_t s_cbHttpUnused;

static const MlndlHttpCallbacks s_httpCallbacks = {
	mlndlCbHttpOpen,
	mlndlCbHttpClose,
	mlndlCbHttpAddHeader,
	mlndlCbHttpRequest,
	mlndlCbHttpFree
};

static const MlndlHttpHeader s_actionHeadersV10[6] = {
	{ "Content-Type", "application/x-www-form-urlencoded" },
	{ "User-Agent", "PSN/ActionTokenAcquisition" },
	{ "X-Mln-ATAcq-Version", "1.0" },
	{ "X-UC-LoginId", NULL },
	{ "X-UC-EPassword", NULL },
	{ NULL, NULL }
};

static const MlndlHttpHeader s_actionHeadersV11[7] = {
	{ "Content-Type", "application/x-www-form-urlencoded" },
	{ "User-Agent", "PSN/ActionTokenAcquisition" },
	{ "X-Mln-ATAcq-Version", "1.1" },
	{ "X-Mln-ATAcq-Platform", "pc" },
	{ "X-UC-LoginId", NULL },
	{ "X-UC-EPassword", NULL },
	{ NULL, NULL }
};

static const MlndlHttpHeader s_actionHeadersV20[6] = {
	{ "Content-Type", "application/x-www-form-urlencoded" },
	{ "User-Agent", "PSN/ActionTokenAcquisition" },
	{ "X-Mln-ATAcq-Version", "2.0" },
	{ "X-UC-LoginId", NULL },
	{ "X-UC-EPassword", NULL },
	{ NULL, NULL }
};

static const MlndlHttpHeader s_personalizeHeaders[6] = {
	{ "Content-Type", "application/x-mln-personalization" },
	{ "User-Agent", "PSN/Personalization" },
	{ "X-Mln-Per-Version", "1.0" },
	{ "X-UC-LoginId", NULL },
	{ "X-Mln-Per-Platform", NULL },
	{ "X-UC-EPassword", NULL }
};

static const char s_emptyQuery[] = "";

static uint32_t mlndlReadBe32(const uint8_t *bytes)
{
	return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16)
		| ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
}

static uint32_t mlndlStrLenLimit(const char *str, uint32_t limit)
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

int mlndlMapMarlinError(uint32_t marlinErr)
{
	if (marlinErr == 0)
		return MLNDL_OK;
	if (marlinErr > 1 && marlinErr < 0xFF)
		return (int)(marlinErr | 0x80);
	if (marlinErr > 0xF0000 && marlinErr < 0xFFFFF)
		return (int)((marlinErr & 0xFF) | 0xE0);
	return MLNDL_RES_INTERNAL_ERROR;
}

int mlndlGetDeviceInfo(int mode, MlndlMarlinKind *kind, void *marlinHandle, uint32_t infoType,
	uint32_t *inOutSize, void *outBuffer)
{
	MlndlBuffer *infoBuf = NULL;
	uint32_t marlinErr;
	int err = MLNDL_OK;

	kind->magic = -1;
	if ((mode & MLNDL_MODE_ENGINE_MASK) != MLNDL_MODE_ENGINE_MARLIN)
		return MLNDL_RES_INVALID_PARAM;

	marlinErr = (uint32_t)SceMarlin_E8276179(marlinHandle, infoType, &infoBuf);
	if (marlinErr != 0) {
		err = mlndlMapMarlinError(marlinErr);
	} else {
		mlndlMemSet(outBuffer, 0, *inOutSize);
		if (infoBuf != NULL) {
			mlndlMemCpy(outBuffer, infoBuf->bytes, infoBuf->size);
			*inOutSize = infoBuf->size;
		}
	}

	if (infoBuf != NULL)
		SceMarlin_9609E2DF(infoBuf);
	return err;
}

int mlndlValidateSession(int mode, MlndlMarlinKind *kind, const MlndlConfig *config)
{
	if (kind == NULL || config == NULL || config->accountId == NULL)
		return MLNDL_RES_INVALID_PARAM;
	if ((mode & MLNDL_MODE_ENGINE_MASK) != MLNDL_MODE_ENGINE_MARLIN)
		return MLNDL_RES_INVALID_PARAM;
	return MLNDL_OK;
}

int mlndlValidateHandle(int mode, MlndlMarlinKind *kind)
{
	(void)kind;
	if ((mode & MLNDL_MODE_ENGINE_MASK) != MLNDL_MODE_ENGINE_MARLIN)
		return MLNDL_RES_INVALID_PARAM;
	return MLNDL_OK;
}

int mlndlValidateConfig(uint16_t requiredMask, const MlndlConfig *config)
{
	if (config == NULL)
		return MLNDL_RES_INVALID_PARAM;
	if ((requiredMask & MLNDL_CHECK_ACCOUNT) != 0) {
		if (config->accountIdLen <= 0 || config->accountId == NULL)
			return MLNDL_RES_INVALID_PARAM;
	}
	if ((requiredMask & MLNDL_CHECK_CREDENTIALS) != 0) {
		if (config->loginId == NULL || config->password == NULL)
			return MLNDL_RES_INVALID_PARAM;
	}
	if ((requiredMask & MLNDL_CHECK_PERSONALIZE_URL) != 0 && config->personalizeUrl == NULL)
		return MLNDL_RES_INVALID_PARAM;
	if ((requiredMask & MLNDL_CHECK_REGISTER_URL) != 0 && config->registerUrl == NULL)
		return MLNDL_RES_INVALID_PARAM;
	if ((requiredMask & MLNDL_CHECK_DEREGISTER_URL) != 0 && config->deregisterUrl == NULL)
		return MLNDL_RES_INVALID_PARAM;
	if ((requiredMask & MLNDL_CHECK_LICENSE_URL) != 0 && config->licenseUrl == NULL)
		return MLNDL_RES_INVALID_PARAM;
	if ((requiredMask & MLNDL_CHECK_SUBSCRIBE_URL) != 0 && config->subscribeUrl == NULL)
		return MLNDL_RES_INVALID_PARAM;
	if ((requiredMask & MLNDL_CHECK_UNSUBSCRIBE_URL) != 0 && config->unsubscribeUrl == NULL)
		return MLNDL_RES_INVALID_PARAM;
	if ((requiredMask & MLNDL_CHECK_ENTITLEMENT) != 0) {
		if (config->entitlementIdLen <= 0 || config->entitlementId == NULL)
			return MLNDL_RES_INVALID_PARAM;
	}
	return MLNDL_OK;
}

int mlndlBuildIdentityBoxes(const MlndlConfig *config, MlndlBuffer **outServiceProvider,
	MlndlBuffer **outAccountBox, MlndlBuffer **outCombinedBox)
{
	MlndlBuffer *spBuf = NULL;
	MlndlBuffer *acidBuf = NULL;
	MlndlBuffer *combinedBuf = NULL;
	uint8_t *acidRaw = NULL;
	uint8_t *etidRaw = NULL;
	uint32_t acidSize = 0;
	uint32_t etidSize = 0;
	const char *urn;
	uint32_t urnLen;
	int err = MLNDL_OK;

	if (config == NULL || config->accountId == NULL || outServiceProvider == NULL
		|| outAccountBox == NULL || outCombinedBox == NULL) {
		return MLNDL_RES_INVALID_PARAM;
	}

	if (config->accountIdLen != 0) {
		acidSize = (uint32_t)config->accountIdLen + 8;
		acidRaw = (uint8_t *)mlndlAlloc(0, acidSize);
		if (acidRaw == NULL) {
			err = MLNDL_RES_NO_MEMORY;
		} else {
			mlndlMemSet(acidRaw, 0, acidSize);
			acidRaw[0] = (uint8_t)(acidSize >> 24);
			acidRaw[1] = (uint8_t)(acidSize >> 16);
			acidRaw[2] = (uint8_t)(acidSize >> 8);
			acidRaw[3] = (uint8_t)acidSize;
			acidRaw[4] = 'A';
			acidRaw[5] = 'C';
			acidRaw[6] = 'I';
			acidRaw[7] = 'D';
			mlndlMemCpy(acidRaw + 8, config->accountId, (uint32_t)config->accountIdLen);
			err = mlndlAllocBuffer(acidSize, acidRaw, &acidBuf);
		}
	}

	if (err == MLNDL_OK && config->entitlementIdLen != 0 && config->entitlementId != NULL) {
		etidSize = (uint32_t)config->entitlementIdLen + 8;
		etidRaw = (uint8_t *)mlndlAlloc(0, etidSize);
		if (etidRaw == NULL) {
			err = MLNDL_RES_NO_MEMORY;
		} else {
			mlndlMemSet(etidRaw, 0, etidSize);
			etidRaw[0] = (uint8_t)(etidSize >> 24);
			etidRaw[1] = (uint8_t)(etidSize >> 16);
			etidRaw[2] = (uint8_t)(etidSize >> 8);
			etidRaw[3] = (uint8_t)etidSize;
			etidRaw[4] = 'E';
			etidRaw[5] = 'T';
			etidRaw[6] = 'I';
			etidRaw[7] = 'D';
			mlndlMemCpy(etidRaw + 8, config->entitlementId,
				(uint32_t)config->entitlementIdLen);

			combinedBuf = (MlndlBuffer *)mlndlAlloc(0, sizeof(MlndlBuffer));
			if (combinedBuf == NULL) {
				err = MLNDL_RES_NO_MEMORY;
			} else {
				combinedBuf->size = acidSize + etidSize;
				combinedBuf->bytes = (uint8_t *)mlndlAlloc(0, acidSize + etidSize);
				if (combinedBuf->bytes == NULL) {
					err = MLNDL_RES_NO_MEMORY;
				} else {
					mlndlMemCpy(combinedBuf->bytes, acidRaw, acidSize);
					mlndlMemCpy(combinedBuf->bytes + acidSize, etidRaw,
						etidSize);
				}
			}
		}
	}

	if (err == MLNDL_OK) {
		urn = (config->serviceProviderUrn != NULL)
			? config->serviceProviderUrn
			: "urn:marlin:organization:sne:service-provider:1";
		urnLen = mlndlStrLenLimit(urn, 256);
		err = mlndlAllocBuffer(urnLen, urn, &spBuf);
	}

	if (err == MLNDL_OK) {
		*outAccountBox = acidBuf;
		*outCombinedBox = combinedBuf;
		*outServiceProvider = spBuf;
		acidBuf = NULL;
		combinedBuf = NULL;
		spBuf = NULL;
	}

	if (acidRaw != NULL)
		mlndlFree(0, acidRaw);
	if (etidRaw != NULL)
		mlndlFree(0, etidRaw);
	if (acidBuf != NULL)
		mlndlFreeBuffer(acidBuf);
	if (combinedBuf != NULL)
		mlndlFreeBuffer(combinedBuf);
	if (spBuf != NULL)
		mlndlFreeBuffer(spBuf);
	return err;
}

static inline __attribute__((always_inline)) int mlndlHasOtherAccountRegistration(void *marlinHandle, MlndlMarlinPair *allPairs,
	int *outFound)
{
	MlndlMarlinDate zeroDate;
	MlndlMarlinDate startDate;
	MlndlMarlinDate endDate;
	MlndlMarlinPair *cursor;
	uint32_t marlinErr;

	mlndlMemSet(&zeroDate, 0, sizeof(zeroDate));
	*outFound = 0;

	for (cursor = allPairs; cursor != NULL; cursor = cursor->next) {
		if (cursor->parentId == NULL && cursor->memberId != NULL) {
			marlinErr = (uint32_t)SceMarlin_3AA04BBF(marlinHandle, NULL,
				cursor->memberId, &startDate, &endDate);
			if (marlinErr != 0)
				return mlndlMapMarlinError(marlinErr);
			if (mlndlMemCmp(&startDate, &zeroDate, sizeof(zeroDate)) == 0
				&& mlndlMemCmp(&endDate, &zeroDate, sizeof(zeroDate)) == 0) {
				*outFound = 1;
				return MLNDL_OK;
			}
		}
	}
	return MLNDL_OK;
}

int mlndlQueryRegistrationStatus(const MlndlConfig *config, int mode, MlndlMarlinKind *kind,
	void *marlinHandle, int *outStatus)
{
	MlndlBuffer *spBuf = NULL;
	MlndlBuffer *acidBuf = NULL;
	MlndlBuffer *combinedBuf = NULL;
	MlndlMarlinPair *allPairs = NULL;
	MlndlMarlinPair *matchingPairs = NULL;
	int regStatus = MLNDL_REG_UNREGISTERED;
	int engine = mode & MLNDL_MODE_ENGINE_MASK;
	uint8_t platform = (uint8_t)mode;
	uint32_t marlinErr;
	int foundOther = 0;
	int err;

	(void)kind;
	if (outStatus == NULL)
		return MLNDL_RES_INVALID_PARAM;

	err = mlndlValidateConfig(MLNDL_CHECK_ACCOUNT, config);
	if (err == MLNDL_OK && engine != MLNDL_MODE_ENGINE_MARLIN)
		err = MLNDL_RES_INVALID_PARAM;

	if (err == MLNDL_OK) {
		marlinErr = (uint32_t)SceMarlin_236015FA(marlinHandle);
		if (marlinErr == 0) {
			marlinErr = (uint32_t)SceMarlin_08CAD267(marlinHandle, &allPairs);
			if (marlinErr != 0) {
				err = mlndlMapMarlinError(marlinErr);
			} else {
				err = mlndlBuildIdentityBoxes(config, &spBuf, &acidBuf,
					&combinedBuf);
				if (err == MLNDL_OK) {
					marlinErr = (uint32_t)SceMarlin_BCED6CC4(marlinHandle,
						spBuf, acidBuf, &matchingPairs);
					if (marlinErr == 0) {
						if (matchingPairs != NULL) {
							regStatus = MLNDL_REG_REGISTERED;
						} else {
							regStatus = MLNDL_REG_UNREGISTERED;
							if (platform == MLNDL_PLATFORM_PSP2_PC
								|| platform == MLNDL_PLATFORM_PSP2) {
								err = mlndlHasOtherAccountRegistration(
									marlinHandle, allPairs,
									&foundOther);
								if (err == MLNDL_OK && foundOther)
									regStatus = MLNDL_REG_OTHER_USER;
							}
						}
					} else if (marlinErr == 15 || marlinErr == 16) {
						regStatus = MLNDL_REG_UNPERSONALIZED;
					} else if (marlinErr == 18) {
						regStatus = MLNDL_REG_UNREGISTERED;
						if (platform == MLNDL_PLATFORM_PSP2_PC
							|| platform == MLNDL_PLATFORM_PSP2) {
							err = mlndlHasOtherAccountRegistration(
								marlinHandle, allPairs,
								&foundOther);
							if (err == MLNDL_OK && foundOther)
								regStatus = MLNDL_REG_OTHER_USER;
						}
					} else if (marlinErr == 68) {
						regStatus = MLNDL_REG_CORRUPTED;
					} else {
						err = mlndlMapMarlinError(marlinErr);
					}
				}
			}
		} else if (marlinErr == 15 || marlinErr == 16) {
			regStatus = MLNDL_REG_UNPERSONALIZED;
		} else if (marlinErr == 68) {
			regStatus = MLNDL_REG_REVOKED;
		} else {
			err = mlndlMapMarlinError(marlinErr);
		}

		if (err == MLNDL_OK)
			*outStatus = regStatus;
	}

	if (engine == MLNDL_MODE_ENGINE_MARLIN) {
		if (matchingPairs != NULL)
			SceMarlin_CCA99803(matchingPairs);
		if (allPairs != NULL)
			SceMarlin_CCA99803(allPairs);
		if (acidBuf != NULL)
			mlndlFreeBuffer(acidBuf);
		if (spBuf != NULL)
			mlndlFreeBuffer(spBuf);
		if (combinedBuf != NULL)
			mlndlFreeBuffer(combinedBuf);
	}
	return err;
}

int mlndlQueryEntitlementStatus(const MlndlConfig *config, int mode, MlndlMarlinKind *kind,
	void *marlinHandle, int *outStatus)
{
	MlndlBuffer *spBuf = NULL;
	MlndlBuffer *acidBuf = NULL;
	MlndlBuffer *combinedBuf = NULL;
	MlndlMarlinPair *allPairs = NULL;
	MlndlMarlinPair *entPairs = NULL;
	MlndlMarlinPair *accPairs = NULL;
	MlndlMarlinPair *entCursor;
	MlndlMarlinPair *accCursor;
	int status = MLNDL_REG_UNREGISTERED;
	int engine = mode & MLNDL_MODE_ENGINE_MASK;
	uint32_t marlinErr;
	int err;

	(void)kind;
	if (outStatus == NULL)
		return MLNDL_RES_INVALID_PARAM;

	err = mlndlValidateConfig(MLNDL_CHECK_ACCOUNT | MLNDL_CHECK_ENTITLEMENT, config);
	if (err == MLNDL_OK && engine != MLNDL_MODE_ENGINE_MARLIN)
		err = MLNDL_RES_INVALID_PARAM;

	if (err == MLNDL_OK) {
		marlinErr = (uint32_t)SceMarlin_236015FA(marlinHandle);
		if (marlinErr == 0) {
			marlinErr = (uint32_t)SceMarlin_08CAD267(marlinHandle, &allPairs);
			if (marlinErr != 0) {
				err = mlndlMapMarlinError(marlinErr);
			} else {
				err = mlndlBuildIdentityBoxes(config, &spBuf, &acidBuf,
					&combinedBuf);
				if (err == MLNDL_OK) {
					marlinErr = (uint32_t)SceMarlin_BCED6CC4(marlinHandle,
						spBuf, combinedBuf, &entPairs);
					if (marlinErr == 0) {
						status = MLNDL_REG_NO_ENTITLEMENT;
						for (entCursor = entPairs;
							entCursor != NULL && err == MLNDL_OK;
							entCursor = entCursor->next) {
							if (entCursor->parentId == NULL
								&& entCursor->memberId != NULL) {
								status = MLNDL_REG_REGISTERED;
								break;
							}
							if (entCursor->parentId != NULL
								&& entCursor->memberId != NULL) {
								marlinErr = (uint32_t)SceMarlin_BCED6CC4(
									marlinHandle, spBuf,
									acidBuf, &accPairs);
								if (marlinErr == 0) {
									for (accCursor = accPairs;
										accCursor != NULL;
										accCursor = accCursor->next) {
										if (mlndlCompareBuffer(
											accCursor->memberId,
											entCursor->parentId) == 0) {
											status = MLNDL_REG_REGISTERED;
											break;
										}
									}
								} else if (marlinErr == 18) {
									status = MLNDL_REG_NO_ENTITLEMENT;
								} else if (marlinErr == 68) {
									status = MLNDL_REG_ENTITLEMENT_BROKEN;
								} else {
									err = mlndlMapMarlinError(
										marlinErr);
								}
							}
						}
					} else if (marlinErr == 18) {
						status = MLNDL_REG_NO_ENTITLEMENT;
					} else if (marlinErr == 68) {
						status = MLNDL_REG_CORRUPTED;
					} else {
						err = mlndlMapMarlinError(marlinErr);
					}
				}
			}
		} else if (marlinErr == 15 || marlinErr == 16) {
			status = MLNDL_REG_UNPERSONALIZED;
		} else if (marlinErr == 68) {
			status = MLNDL_REG_REVOKED;
		} else {
			err = mlndlMapMarlinError(marlinErr);
		}

		if (err == MLNDL_OK)
			*outStatus = status;
	}

	if (engine == MLNDL_MODE_ENGINE_MARLIN) {
		if (accPairs != NULL)
			SceMarlin_CCA99803(accPairs);
		if (allPairs != NULL)
			SceMarlin_CCA99803(allPairs);
		if (acidBuf != NULL)
			mlndlFreeBuffer(acidBuf);
		if (spBuf != NULL)
			mlndlFreeBuffer(spBuf);
		if (combinedBuf != NULL)
			mlndlFreeBuffer(combinedBuf);
	}
	return err;
}

int mlndlAppendQueryParam(uint8_t platform, int paramKind, uint32_t valueLen,
	const void *valueBytes, uint32_t *inOutLen, char *queryBuf)
{
	uint32_t offset;
	const char *prefix;
	uint32_t prefixLen;

	if (inOutLen == NULL || queryBuf == NULL)
		return MLNDL_RES_INVALID_PARAM;
	if (paramKind != 2 && valueBytes == NULL)
		return MLNDL_RES_INVALID_PARAM;

	offset = *inOutLen;
	if (paramKind == 2) {
		mlndlMemCpy(queryBuf, s_emptyQuery, 1);
		*inOutLen = 1;
		return MLNDL_OK;
	}

	switch (paramKind) {
	case 0:
		switch (platform) {
		case MLNDL_PLATFORM_PS3:
			prefix = "consoleid=";
			prefixLen = 10;
			break;
		case MLNDL_PLATFORM_PSP_PC:
		case MLNDL_PLATFORM_PSP:
		case MLNDL_PLATFORM_PSP2_PC:
		case MLNDL_PLATFORM_PSP2:
			prefix = "c" "1=";
			prefixLen = 3;
			break;
		case MLNDL_PLATFORM_DUID:
			prefix = "duid=";
			prefixLen = 5;
			break;
		case MLNDL_PLATFORM_DUID_CONTAINER:
		case MLNDL_PLATFORM_PSP2_CONTAINER:
			prefix = "duidcontainer=";
			prefixLen = 14;
			break;
		default:
			return MLNDL_RES_INVALID_PARAM;
		}
		break;
	case 1:
		prefix = "contentid=";
		prefixLen = 10;
		break;
	case 3:
		prefix = "entitlementid=";
		prefixLen = 14;
		break;
	case 4:
		prefix = "linkonly=";
		prefixLen = 9;
		break;
	default:
		return MLNDL_RES_INVALID_PARAM;
	}

	if (offset != 0) {
		mlndlMemCpy(queryBuf + offset, "&", 1);
		offset++;
	}
	mlndlMemCpy(queryBuf + offset, prefix, prefixLen);
	offset += prefixLen;
	mlndlMemCpy(queryBuf + offset, valueBytes, valueLen);
	offset += valueLen;
	*inOutLen = offset;
	return MLNDL_OK;
}

static int mlndlHexEncodeBytes(const uint8_t *src, uint32_t srcLen, char *dst,
	uint32_t dstCap, uint32_t *outLen)
{
	uint32_t bytePos;
	uint32_t nibblePos;
	uint32_t nibble;

	if (srcLen * 2 > dstCap)
		return MLNDL_RES_INVALID_PARAM;

	for (bytePos = 0; bytePos < srcLen; bytePos++) {
		for (nibblePos = 0; nibblePos < 2; nibblePos++) {
			nibble = (src[bytePos] >> (4 * (1 - nibblePos))) & 0xF;
			dst[bytePos * 2 + nibblePos] = (nibble > 9)
				? (char)(nibble + 'a' - 10)
				: (char)(nibble + '0');
		}
	}
	*outLen = srcLen * 2;
	return MLNDL_OK;
}

int mlndlFetchActionToken(const MlndlConfig *config, int mode, MlndlMarlinKind *kind,
	void *marlinHandle, int actionKind, const char *contentId, uint32_t *outLen,
	void **outBody, MlndlStatusInfo *statusInfo)
{
	MlndlHttpHeader headersV10[6];
	MlndlHttpHeader headersV11[7];
	MlndlHttpHeader headersV20[6];
	MlndlHttpHeader *headers;
	MlndlHttpContext *httpCtx = NULL;
	uint8_t infoBox[16];
	uint8_t deviceInfo[260];
	char hexBuf[512];
	char queryBuf[1024];
	char respHeader[256];
	uint8_t *tempBuf = NULL;
	const char *url;
	const char *linkOnlyStr = NULL;
	uint32_t infoType;
	uint32_t deviceInfoLen = 256;
	uint32_t hexLen = 512;
	uint32_t queryLen = 0;
	int version = mode & MLNDL_MODE_VERSION_MASK;
	int useEPassword = mode & MLNDL_MODE_EPASSWORD_MASK;
	uint8_t platform = (uint8_t)mode;
	int err = MLNDL_OK;

	mlndlCopyBytes(headersV10, s_actionHeadersV10, sizeof(headersV10));
	mlndlCopyBytes(headersV11, s_actionHeadersV11, sizeof(headersV11));
	mlndlCopyBytes(headersV20, s_actionHeadersV20, sizeof(headersV20));

	if (config == NULL || kind == NULL || outLen == NULL || outBody == NULL
		|| statusInfo == NULL) {
		mlndlHttpClose(NULL);
		return MLNDL_RES_INVALID_PARAM;
	}

	mlndlMemSet(hexBuf, 0, sizeof(hexBuf));
	mlndlMemSet(deviceInfo, 0, 256);
	mlndlMemSet(queryBuf, 0, sizeof(queryBuf));

	switch (actionKind) {
	case MLNDL_ACTION_REGISTER:
		url = config->registerUrl;
		infoType = 4;
		break;
	case MLNDL_ACTION_LICENSE:
		url = config->licenseUrl;
		infoType = 6;
		if (contentId == NULL) {
			mlndlHttpClose(NULL);
			return MLNDL_RES_INVALID_PARAM;
		}
		break;
	case MLNDL_ACTION_DEREGISTER:
		url = config->deregisterUrl;
		infoType = 5;
		break;
	case MLNDL_ACTION_SUBSCRIBE:
		url = config->subscribeUrl;
		infoType = 14;
		linkOnlyStr = "false";
		break;
	case MLNDL_ACTION_SUBSCRIBE_LINK_ONLY:
		url = config->subscribeUrl;
		infoType = 14;
		linkOnlyStr = "true";
		break;
	case MLNDL_ACTION_UNSUBSCRIBE:
		url = config->unsubscribeUrl;
		infoType = 15;
		break;
	default:
		mlndlHttpClose(NULL);
		return MLNDL_RES_INVALID_PARAM;
	}

	if (version == MLNDL_MODE_VERSION_20)
		headers = headersV20;
	else if (version == MLNDL_MODE_VERSION_11)
		headers = headersV11;
	else if (version == MLNDL_MODE_VERSION_10)
		headers = headersV10;
	else {
		mlndlHttpClose(NULL);
		return MLNDL_RES_INVALID_PARAM;
	}

	err = mlndlGetDeviceInfo(mode, kind, marlinHandle, infoType, &deviceInfoLen, deviceInfo);
	if (err == MLNDL_OK) {
		switch (platform) {
		case MLNDL_PLATFORM_PS3:
		case MLNDL_PLATFORM_PSP2_PC:
		case MLNDL_PLATFORM_PSP2: {
			int boxCount = 0;
			uint32_t pos = 4;
			const uint8_t *duidBox = NULL;

			while (pos + 8 < deviceInfoLen && boxCount < 20) {
				duidBox = deviceInfo + pos;
				if (mlndlReadBe32(duidBox + 4) == 0x44554944U)
					break;
				pos += mlndlReadBe32(duidBox);
				boxCount++;
			}
			if (pos < deviceInfoLen && boxCount < 20 && duidBox != NULL) {
				uint32_t duidLen = (((uint32_t)duidBox[14] << 8)
					| (uint32_t)duidBox[15]) >> 3;
				tempBuf = (uint8_t *)mlndlAlloc(0, duidLen);
				if (tempBuf == NULL) {
					err = MLNDL_RES_NO_MEMORY;
				} else {
					mlndlMemCpy(tempBuf, duidBox + 16, duidLen);
					mlndlMemCpy(deviceInfo, tempBuf, duidLen);
					deviceInfoLen = duidLen;
				}
			} else {
				err = MLNDL_RES_INTERNAL_ERROR;
			}
			break;
		}
		case MLNDL_PLATFORM_PSP_PC:
		case MLNDL_PLATFORM_PSP:
		case MLNDL_PLATFORM_DUID:
		case MLNDL_PLATFORM_DUID_CONTAINER:
			break;
		case MLNDL_PLATFORM_PSP2_CONTAINER:
			err = mlndlBuildInfoHeader(actionKind, infoBox);
			if (err == MLNDL_OK) {
				uint32_t totalLen = deviceInfoLen + 12;
				tempBuf = (uint8_t *)mlndlAlloc(0, totalLen);
				if (tempBuf == NULL) {
					err = MLNDL_RES_NO_MEMORY;
				} else {
					mlndlMemCpy(tempBuf, infoBox, 16);
					mlndlMemCpy(tempBuf + 16, deviceInfo + 4,
						deviceInfoLen - 4);
					mlndlMemCpy(deviceInfo, tempBuf, totalLen);
					deviceInfoLen = totalLen;
				}
			}
			break;
		default:
			err = MLNDL_RES_INVALID_PARAM;
			break;
		}
	}

	if (err == MLNDL_OK)
		err = mlndlHexEncodeBytes(deviceInfo, deviceInfoLen, hexBuf, sizeof(hexBuf),
			&hexLen);

	if (err == MLNDL_OK)
		err = mlndlAppendQueryParam(platform, 0, hexLen, hexBuf, &queryLen, queryBuf);

	if (err == MLNDL_OK && actionKind == MLNDL_ACTION_LICENSE)
		err = mlndlAppendQueryParam(platform, 1, mlndlStrLenLimit(contentId, 256),
			contentId, &queryLen, queryBuf);

	if (err == MLNDL_OK && (actionKind == MLNDL_ACTION_SUBSCRIBE
		|| actionKind == MLNDL_ACTION_SUBSCRIBE_LINK_ONLY
		|| actionKind == MLNDL_ACTION_UNSUBSCRIBE)) {
		err = mlndlAppendQueryParam(platform, 3, (uint32_t)config->entitlementIdLen,
			config->entitlementId, &queryLen, queryBuf);
	}

	if (err == MLNDL_OK && (actionKind == MLNDL_ACTION_SUBSCRIBE
		|| actionKind == MLNDL_ACTION_SUBSCRIBE_LINK_ONLY)) {
		err = mlndlAppendQueryParam(platform, 4, mlndlStrLenLimit(linkOnlyStr, 10),
			linkOnlyStr, &queryLen, queryBuf);
	}

	if (err == MLNDL_OK) {
		err = mlndlHttpOpen(url, &httpCtx);
		if (err == MLNDL_OK) {
			MlndlHttpHeader *hdr;
			for (hdr = headers; hdr->name != NULL; hdr++) {
				if (mlndlStrNCmp(hdr->name, "X-UC-LoginId", 12) == 0)
					hdr->value = config->loginId;
				if (mlndlStrNCmp(hdr->name, "X-UC-EPassword", 14) == 0) {
					if (useEPassword == 0)
						hdr->name = "X-UC-Password";
					hdr->value = config->password;
				}
				if (platform == MLNDL_PLATFORM_PSP
					&& mlndlStrNCmp(hdr->name, "User-Agent", 10) == 0) {
					hdr->value = "PSN/PSPDirectActionTokenAcquisition";
				} else if (platform == MLNDL_PLATFORM_PSP2
					&& mlndlStrNCmp(hdr->name, "User-Agent", 10) == 0) {
					hdr->value = "PSN/PSP2DirectActionTokenAcquisition";
				}
				err = mlndlHttpAddHeader(httpCtx, hdr->name, hdr->value);
				if (err != MLNDL_OK)
					break;
			}
		}

		if (err == MLNDL_OK) {
			err = mlndlHttpRequest(httpCtx, 0, queryLen, queryBuf, outLen, outBody);
			if (err == MLNDL_OK) {
				mlndlMemSet(respHeader, 0, sizeof(respHeader));
				if (mlndlHttpGetResponseHeader(httpCtx, "Content-Type",
					respHeader) != MLNDL_OK) {
					err = MLNDL_RES_HEADER_CONTENT_TYPE_MISSING;
				} else if (mlndlStrNCmp(respHeader, "text/mrln-actnt", 15) != 0
					&& mlndlStrNCmp(respHeader,
						"application/vnd.marlin.drm.actiontoken+xml",
						42) != 0) {
					err = MLNDL_RES_HEADER_CONTENT_TYPE_MISMATCH;
				} else if (mlndlHttpGetResponseHeader(httpCtx,
					"X-Mln-ATAcq-Version", respHeader) != MLNDL_OK) {
					err = MLNDL_RES_HEADER_VERSION_MISSING;
				} else {
					const char *expectedVer = NULL;
					if (version == MLNDL_MODE_VERSION_10)
						expectedVer = "1.0";
					else if (version == MLNDL_MODE_VERSION_11)
						expectedVer = "1.1";
					else if (version == MLNDL_MODE_VERSION_20)
						expectedVer = "2.0";
					if (expectedVer == NULL
						|| mlndlStrNCmp(respHeader, expectedVer, 3) != 0)
						err = MLNDL_RES_HEADER_VERSION_MISMATCH;
				}
				if (err == MLNDL_OK)
					err = mlndlParseStatusHeader(httpCtx,
						"X-Mln-ATAcq-Status", statusInfo);
			}
		}
	}

	if (tempBuf != NULL)
		mlndlFree(0, tempBuf);
	mlndlHttpClose(httpCtx);
	return err;
}

int mlndlProcessActionToken(const MlndlConfig *config, int mode, MlndlMarlinKind *kind,
	void *marlinHandle, uint32_t tokenLen, void *tokenBody, int unusedFlag,
	MlndlBuffer **outLicense)
{
	MlndlBuffer tokenBuf;
	MlndlBuffer *spBuf = NULL;
	MlndlBuffer *acidBuf = NULL;
	MlndlBuffer *combinedBuf = NULL;
	MlndlBuffer *identityBuf = NULL;
	MlndlBuffer *licenseBuf = NULL;
	MlndlMarlinActionResult *results = NULL;
	MlndlMarlinActionResult *cursor;
	uint32_t marlinErr;
	int err = MLNDL_OK;

	(void)kind;
	(void)unusedFlag;
	tokenBuf.size = tokenLen;
	tokenBuf.bytes = (uint8_t *)tokenBody;

	if ((mode & MLNDL_MODE_ENGINE_MASK) != MLNDL_MODE_ENGINE_MARLIN)
		return MLNDL_RES_INVALID_PARAM;

	err = mlndlBuildIdentityBoxes(config, &spBuf, &acidBuf, &combinedBuf);
	if (err == MLNDL_OK) {
		identityBuf = (combinedBuf != NULL) ? combinedBuf : acidBuf;
		marlinErr = (uint32_t)SceMarlin_88AC64AD(marlinHandle, &tokenBuf, spBuf,
			identityBuf, &s_httpCallbacks, &results);
		if (marlinErr == MLNDL_RES_TRANSPORT_ERROR) {
			err = s_cbHttpError;
			s_cbHttpError = 0;
		} else if (marlinErr != 0 && marlinErr != 41) {
			err = mlndlMapMarlinError(marlinErr);
		}

		if (err == MLNDL_OK) {
			for (cursor = results; cursor != NULL; cursor = cursor->next) {
				if (cursor->kind == 4) {
					SceMarlin_9609E2DF(licenseBuf);
					licenseBuf = (MlndlBuffer *)cursor->payload;
					cursor->payload = NULL;
				} else if (cursor->kind == 5) {
					if ((uintptr_t)cursor->payload == 1) {
						licenseBuf = (MlndlBuffer *)cursor->extra;
						cursor->extra = NULL;
					}
				} else if (cursor->kind == 16) {
					MlndlMarlinErrorDetail *detail =
						(MlndlMarlinErrorDetail *)cursor->payload;
					if (detail != NULL) {
						if (detail->code != 47) {
							err = MLNDL_RES_ACTION_SERVER_UNKNOWN_CODE;
						} else if (detail->message == NULL) {
							err = MLNDL_RES_ACTION_SERVER_NO_DETAIL;
						} else if (detail->message->size == 9
							&& mlndlStrNCmp(
								(const char *)detail->message->bytes,
								"CODE:0000", 5) == 0) {
							uint32_t code = mlndlReadBe32(
								detail->message->bytes + 5);
							switch (code) {
							case 0x39303031U:
								err = MLNDL_RES_ACTION_SERVER_CODE_9001;
								break;
							case 0x39303032U:
								err = MLNDL_RES_ACTION_SERVER_CODE_9002;
								break;
							case 0x39303033U:
								err = MLNDL_RES_ACTION_SERVER_CODE_9003;
								break;
							case 0x39303034U:
								err = MLNDL_RES_SERVER_LIMIT;
								break;
							case 0x39303036U:
								err = MLNDL_RES_SERVER_MAINTENANCE;
								break;
							default:
								err = MLNDL_RES_ACTION_SERVER_UNKNOWN_CODE;
								break;
							}
							s_cbHttpUnused = 0;
						} else {
							err = MLNDL_RES_ACTION_SERVER_BAD_RESPONSE;
						}
						break;
					}
				}
			}
			if (err == MLNDL_OK) {
				*outLicense = licenseBuf;
				licenseBuf = NULL;
			}
		}
	}

	if (spBuf != NULL)
		mlndlFreeBuffer(spBuf);
	if (identityBuf != NULL)
		mlndlFreeBuffer(identityBuf);
	if (results != NULL)
		SceMarlin_45B29121(results);
	if (licenseBuf != NULL)
		SceMarlin_9609E2DF(licenseBuf);
	return err;
}

int mlndlProtoRegister(const MlndlConfig *config, int mode, MlndlMarlinKind *kind,
	void *marlinHandle, int unusedFlag, MlndlStatusInfo *statusInfo)
{
	uint32_t tokenLen = 0;
	void *tokenBody = NULL;
	MlndlBuffer *unusedLicense = NULL;
	int err;

	err = mlndlValidateConfig(MLNDL_CHECK_ACCOUNT | MLNDL_CHECK_CREDENTIALS
		| MLNDL_CHECK_REGISTER_URL, config);
	if (err == MLNDL_OK) {
		err = mlndlFetchActionToken(config, mode, kind, marlinHandle,
			MLNDL_ACTION_REGISTER, NULL, &tokenLen, &tokenBody, statusInfo);
		if (err == MLNDL_OK)
			err = mlndlProcessActionToken(config, mode, kind, marlinHandle, tokenLen,
				tokenBody, unusedFlag, &unusedLicense);
	}
	if (tokenBody != NULL)
		mlndlHttpFreeBody(tokenBody);
	return err;
}

int mlndlProtoAcquireLicense(const MlndlConfig *config, const char *contentId, int mode,
	MlndlMarlinKind *kind, void *marlinHandle, int unusedFlag, MlndlBuffer **outLicense)
{
	uint32_t tokenLen = 0;
	void *tokenBody = NULL;
	MlndlStatusInfo statusInfo;
	int err;

	if (contentId == NULL || outLicense == NULL)
		return MLNDL_RES_INVALID_PARAM;

	err = mlndlValidateConfig(MLNDL_CHECK_ACCOUNT | MLNDL_CHECK_CREDENTIALS
		| MLNDL_CHECK_LICENSE_URL, config);
	if (err == MLNDL_OK) {
		err = mlndlFetchActionToken(config, mode, kind, marlinHandle, MLNDL_ACTION_LICENSE,
			contentId, &tokenLen, &tokenBody, &statusInfo);
		if (err == MLNDL_OK)
			err = mlndlProcessActionToken(config, mode, kind, marlinHandle, tokenLen,
				tokenBody, unusedFlag, outLicense);
	}
	if (tokenBody != NULL)
		mlndlHttpFreeBody(tokenBody);
	return err;
}

int mlndlProtoDeregister(const MlndlConfig *config, int mode, MlndlMarlinKind *kind,
	void *marlinHandle, int unusedFlag, MlndlStatusInfo *statusInfo)
{
	uint32_t tokenLen = 0;
	void *tokenBody = NULL;
	MlndlBuffer *unusedLicense = NULL;
	int err;

	err = mlndlValidateConfig(MLNDL_CHECK_ACCOUNT | MLNDL_CHECK_CREDENTIALS
		| MLNDL_CHECK_DEREGISTER_URL, config);
	if (err == MLNDL_OK) {
		err = mlndlFetchActionToken(config, mode, kind, marlinHandle,
			MLNDL_ACTION_DEREGISTER, NULL, &tokenLen, &tokenBody, statusInfo);
		if (err == MLNDL_OK)
			err = mlndlProcessActionToken(config, mode, kind, marlinHandle, tokenLen,
				tokenBody, unusedFlag, &unusedLicense);
	}
	if (tokenBody != NULL)
		mlndlHttpFreeBody(tokenBody);
	return err;
}

int mlndlProtoSubscribe(const MlndlConfig *config, int mode, MlndlMarlinKind *kind,
	void *marlinHandle, int linkOnly, int unusedFlag, MlndlStatusInfo *statusInfo)
{
	uint32_t tokenLen = 0;
	void *tokenBody = NULL;
	MlndlBuffer *unusedLicense = NULL;
	int actionKind;
	int err;

	err = mlndlValidateConfig(MLNDL_CHECK_ACCOUNT | MLNDL_CHECK_CREDENTIALS
		| MLNDL_CHECK_SUBSCRIBE_URL | MLNDL_CHECK_ENTITLEMENT, config);
	if (err == MLNDL_OK) {
		actionKind = (linkOnly != 0)
			? MLNDL_ACTION_SUBSCRIBE_LINK_ONLY
			: MLNDL_ACTION_SUBSCRIBE;
		err = mlndlFetchActionToken(config, mode, kind, marlinHandle, actionKind, NULL,
			&tokenLen, &tokenBody, statusInfo);
		if (err == MLNDL_OK)
			err = mlndlProcessActionToken(config, mode, kind, marlinHandle, tokenLen,
				tokenBody, unusedFlag, &unusedLicense);
	}
	if (tokenBody != NULL)
		mlndlHttpFreeBody(tokenBody);
	return err;
}

int mlndlProtoUnsubscribe(const MlndlConfig *config, int mode, MlndlMarlinKind *kind,
	void *marlinHandle, int unusedFlag, MlndlStatusInfo *statusInfo)
{
	uint32_t tokenLen = 0;
	void *tokenBody = NULL;
	MlndlBuffer *unusedLicense = NULL;
	int err;

	err = mlndlValidateConfig(MLNDL_CHECK_ACCOUNT | MLNDL_CHECK_CREDENTIALS
		| MLNDL_CHECK_UNSUBSCRIBE_URL | MLNDL_CHECK_ENTITLEMENT, config);
	if (err == MLNDL_OK) {
		err = mlndlFetchActionToken(config, mode, kind, marlinHandle,
			MLNDL_ACTION_UNSUBSCRIBE, NULL, &tokenLen, &tokenBody, statusInfo);
		if (err == MLNDL_OK)
			err = mlndlProcessActionToken(config, mode, kind, marlinHandle, tokenLen,
				tokenBody, unusedFlag, &unusedLicense);
	}
	if (tokenBody != NULL)
		mlndlHttpFreeBody(tokenBody);
	return err;
}

static int mlndlConvertMarlinDate(const MlndlMarlinDate *src, MlndlDateTime *dst)
{
	if (dst == NULL)
		return MLNDL_RES_INVALID_PARAM;
	dst->year = (uint16_t)src->year;
	dst->month = (uint16_t)src->month;
	dst->day = (uint16_t)src->day;
	dst->hour = (uint16_t)src->hour;
	dst->minute = (uint16_t)src->minute;
	dst->second = (uint16_t)src->second;
	dst->microsecond = (uint32_t)(src->millisecond * 1000);
	return MLNDL_OK;
}

int mlndlProtoGetValidity(const MlndlConfig *config, int mode, MlndlMarlinKind *kind,
	void *marlinHandle, MlndlDateTime *outStart, MlndlDateTime *outEnd)
{
	MlndlBuffer *spBuf = NULL;
	MlndlBuffer *acidBuf = NULL;
	MlndlBuffer *combinedBuf = NULL;
	MlndlMarlinPair *entPairs = NULL;
	MlndlMarlinPair *accPairs = NULL;
	MlndlMarlinPair *entCursor;
	MlndlMarlinPair *accCursor;
	MlndlMarlinDate startDate;
	MlndlMarlinDate endDate;
	uint32_t marlinErr;
	int err;

	(void)kind;
	if ((mode & MLNDL_MODE_ENGINE_MASK) != MLNDL_MODE_ENGINE_MARLIN)
		return MLNDL_RES_INVALID_PARAM;

	err = mlndlBuildIdentityBoxes(config, &spBuf, &acidBuf, &combinedBuf);
	if (err != MLNDL_OK)
		return err;
	if (combinedBuf == NULL)
		return MLNDL_RES_INVALID_PARAM;

	marlinErr = (uint32_t)SceMarlin_BCED6CC4(marlinHandle, spBuf, combinedBuf, &entPairs);
	if (marlinErr != 0)
		return mlndlMapMarlinError(marlinErr);
	if (entPairs == NULL)
		return MLNDL_RES_NO_ENTITLEMENT;

	entCursor = entPairs;
	while (entCursor != NULL) {
		if (entCursor->parentId == NULL)
			break;
		marlinErr = (uint32_t)SceMarlin_BCED6CC4(marlinHandle, spBuf, acidBuf, &accPairs);
		if (marlinErr != 0)
			return mlndlMapMarlinError(marlinErr);
		if (accPairs == NULL)
			return MLNDL_RES_NOT_REGISTERED;
		for (accCursor = accPairs; accCursor != NULL; accCursor = accCursor->next) {
			if (mlndlCompareBuffer(accPairs->memberId, entCursor->parentId) == 0)
				break;
		}
		if (accCursor != NULL)
			break;
		entCursor = entCursor->next;
	}

	if (entCursor == NULL)
		return MLNDL_RES_NO_ENTITLEMENT;

	marlinErr = (uint32_t)SceMarlin_3AA04BBF(marlinHandle, entCursor->parentId,
		entCursor->memberId, &startDate, &endDate);
	if (marlinErr != 0)
		return mlndlMapMarlinError(marlinErr);

	err = mlndlConvertMarlinDate(&startDate, outStart);
	if (err == MLNDL_OK)
		err = mlndlConvertMarlinDate(&endDate, outEnd);
	return err;
}

int mlndlProtoPurgeExpired(int mode, MlndlMarlinKind *kind, void *marlinHandle,
	const MlndlDateTime *currentTime)
{
	MlndlMarlinPair *allPairs = NULL;
	MlndlMarlinPair *cursor;
	MlndlMarlinDate startDate;
	MlndlMarlinDate endDate;
	uint32_t marlinErr;
	int err = MLNDL_OK;

	(void)kind;
	if ((mode & MLNDL_MODE_ENGINE_MASK) != MLNDL_MODE_ENGINE_MARLIN)
		return MLNDL_RES_INVALID_PARAM;

	marlinErr = (uint32_t)SceMarlin_08CAD267(marlinHandle, &allPairs);
	if (marlinErr != 0) {
		err = mlndlMapMarlinError(marlinErr);
	} else {
		for (cursor = allPairs; cursor != NULL; cursor = cursor->next) {
			if (cursor->parentId == NULL)
				continue;
			marlinErr = (uint32_t)SceMarlin_3AA04BBF(marlinHandle, cursor->parentId,
				cursor->memberId, &startDate, &endDate);
			if (marlinErr != 0) {
				err = mlndlMapMarlinError(marlinErr);
				break;
			}
			int endFields[7] = {
				(uint16_t)endDate.year,
				(uint16_t)endDate.month,
				(uint16_t)endDate.day,
				(uint16_t)endDate.hour,
				(uint16_t)endDate.minute,
				(uint16_t)endDate.second,
				endDate.millisecond * 1000
			};
			int nowFields[7] = {
				currentTime->year,
				currentTime->month,
				currentTime->day,
				currentTime->hour,
				currentTime->minute,
				currentTime->second,
				(int)currentTime->microsecond
			};
			if (endFields[0] != 0 || endFields[1] != 0 || endFields[2] != 0
				|| endFields[3] != 0 || endFields[4] != 0 || endFields[5] != 0
				|| endFields[6] != 0) {
				for (int part = 0; part < 7; part++) {
					if (nowFields[part] < endFields[part])
						break;
					if (nowFields[part] > endFields[part]) {
						SceMarlin_7E65734B(marlinHandle, cursor->parentId,
							cursor->memberId);
						break;
					}
				}
			}
		}
	}

	if (allPairs != NULL)
		SceMarlin_CCA99803(allPairs);
	return err;
}

int mlndlProtoRemoveRegistration(const MlndlConfig *config, int mode, MlndlMarlinKind *kind,
	void *marlinHandle, uint32_t onlyMatching)
{
	MlndlBuffer *spBuf = NULL;
	MlndlBuffer *acidBuf = NULL;
	MlndlBuffer *combinedBuf = NULL;
	MlndlBuffer *identityBuf;
	MlndlMarlinPair *pairs = NULL;
	MlndlMarlinPair *cursor;
	uint32_t marlinErr;
	int err;

	(void)kind;
	if (onlyMatching >= 2)
		return MLNDL_RES_INVALID_PARAM;

	err = mlndlValidateConfig(MLNDL_CHECK_ACCOUNT, config);
	if (err == MLNDL_OK) {
		if ((mode & MLNDL_MODE_ENGINE_MASK) != MLNDL_MODE_ENGINE_MARLIN) {
			err = MLNDL_RES_INVALID_PARAM;
		} else if (onlyMatching != 0) {
			err = mlndlBuildIdentityBoxes(config, &spBuf, &acidBuf, &combinedBuf);
			if (err == MLNDL_OK) {
				identityBuf = (combinedBuf != NULL) ? combinedBuf : acidBuf;
				marlinErr = (uint32_t)SceMarlin_BCED6CC4(marlinHandle, spBuf,
					identityBuf, &pairs);
				if (marlinErr != 0)
					err = mlndlMapMarlinError(marlinErr);
				else if (pairs != NULL)
					SceMarlin_7E65734B(marlinHandle, pairs->parentId,
						pairs->memberId);
			}
		} else {
			marlinErr = (uint32_t)SceMarlin_08CAD267(marlinHandle, &pairs);
			if (marlinErr != 0) {
				err = mlndlMapMarlinError(marlinErr);
			} else {
				for (cursor = pairs; cursor != NULL; cursor = cursor->next)
					SceMarlin_7E65734B(marlinHandle, cursor->parentId,
						cursor->memberId);
			}
		}
	}

	if (pairs != NULL)
		SceMarlin_CCA99803(pairs);
	if (spBuf != NULL)
		mlndlFreeBuffer(spBuf);
	if (acidBuf != NULL)
		mlndlFreeBuffer(acidBuf);
	if (combinedBuf != NULL)
		mlndlFreeBuffer(combinedBuf);
	return err;
}

int mlndlProtoResetRegistration(const MlndlConfig *config, int mode, MlndlMarlinKind *kind,
	void *marlinHandle)
{
	int err;

	(void)kind;
	(void)marlinHandle;
	err = mlndlValidateConfig(MLNDL_CHECK_ACCOUNT, config);
	if (err == MLNDL_OK && (mode & MLNDL_MODE_ENGINE_MASK) != MLNDL_MODE_ENGINE_MARLIN)
		return MLNDL_RES_INVALID_PARAM;
	return err;
}

int mlndlCbHttpOpen(const char *url, MlndlHttpContext **outContext)
{
	int err;

	if (url == NULL) {
		s_cbHttpError = MLNDL_RES_INVALID_PARAM;
		return MLNDL_RES_TRANSPORT_ERROR;
	}
	err = mlndlHttpOpen(url, outContext);
	if (err == MLNDL_OK) {
		s_cbHttpUnused = 0;
		s_cbHttpError = 0;
		return MLNDL_OK;
	}
	s_cbHttpError = err;
	return MLNDL_RES_TRANSPORT_ERROR;
}

int mlndlCbHttpClose(MlndlHttpContext *context)
{
	mlndlHttpClose(context);
	return MLNDL_OK;
}

int mlndlCbHttpAddHeader(MlndlHttpContext *context, const char *name, const char *value)
{
	int err;

	err = mlndlHttpAddHeader(context, name, value);
	if (err != MLNDL_OK) {
		s_cbHttpError = err;
		return MLNDL_RES_TRANSPORT_ERROR;
	}
	return MLNDL_OK;
}

int mlndlCbHttpRequest(MlndlHttpContext *context, int method, uint32_t reqLen,
	const void *reqBody, uint32_t *outLen, void **outBody)
{
	int err;

	if (method == 0)
		err = mlndlHttpRequest(context, 1, reqLen, reqBody, outLen, outBody);
	else if (method == 1)
		err = mlndlHttpRequest(context, 0, reqLen, reqBody, outLen, outBody);
	else
		err = MLNDL_RES_INVALID_PARAM;

	if (err == MLNDL_OK)
		return MLNDL_OK;
	s_cbHttpError = err;
	return MLNDL_RES_TRANSPORT_ERROR;
}

void mlndlCbHttpFree(void *body)
{
	mlndlHttpFreeBody(body);
}

int mlndlProtoPersonalize(const MlndlConfig *config, int mode, MlndlMarlinKind *kind,
	void *marlinHandle, MlndlStatusInfo *statusInfo)
{
	MlndlHttpHeader headers[6];
	MlndlHttpContext *httpCtx = NULL;
	MlndlBuffer *reqBuf = NULL;
	MlndlBuffer respBuf;
	char respHeader[256];
	uint32_t respLen = 0;
	void *respBody = NULL;
	int engine = mode & MLNDL_MODE_ENGINE_MASK;
	int useEPassword = mode & MLNDL_MODE_EPASSWORD_MASK;
	uint8_t platform = (uint8_t)mode;
	uint32_t marlinErr;
	int err = MLNDL_OK;

	(void)kind;
	mlndlCopyBytes(headers, s_personalizeHeaders, sizeof(headers));

	if (config == NULL || statusInfo == NULL || engine != MLNDL_MODE_ENGINE_MARLIN) {
		err = MLNDL_RES_INVALID_PARAM;
	} else {
		marlinErr = (uint32_t)SceMarlin_C4FF32EF(marlinHandle, &reqBuf, statusInfo);
		if (marlinErr != 0) {
			err = mlndlMapMarlinError(marlinErr);
		} else {
			err = mlndlHttpOpen(config->personalizeUrl, &httpCtx);
			if (err == MLNDL_OK) {
				for (int pos = 0; pos < 6; pos++) {
					if (mlndlStrNCmp(headers[pos].name, "X-UC-LoginId", 12) == 0)
						headers[pos].value = config->loginId;
					if (mlndlStrNCmp(headers[pos].name, "X-Mln-Per-Platform",
						18) == 0) {
						if (platform == MLNDL_PLATFORM_PS3) {
							headers[pos].value = "ps3";
						} else if (platform == MLNDL_PLATFORM_PSP_PC
							|| platform == MLNDL_PLATFORM_PSP) {
							headers[pos].value = "psp";
						} else if (platform == MLNDL_PLATFORM_PSP2_PC
							|| platform == MLNDL_PLATFORM_PSP2) {
							headers[pos].value = "psp2";
						} else {
							err = MLNDL_RES_INVALID_PARAM;
							break;
						}
					}
					if (mlndlStrNCmp(headers[pos].name, "X-UC-EPassword",
						14) == 0) {
						if (useEPassword == 0)
							headers[pos].name = "X-UC-Password";
						headers[pos].value = config->password;
					}
					if (platform == MLNDL_PLATFORM_PSP
						&& mlndlStrNCmp(headers[pos].name, "User-Agent",
							10) == 0) {
						headers[pos].value = "PSN/PSPDirectPersonalization";
					} else if (platform == MLNDL_PLATFORM_PSP2
						&& mlndlStrNCmp(headers[pos].name, "User-Agent",
							10) == 0) {
						headers[pos].value =
							"PSN/PSP2DirectPersonalization";
					}
					err = mlndlHttpAddHeader(httpCtx, headers[pos].name,
						headers[pos].value);
					if (err != MLNDL_OK)
						break;
				}
			}

			if (err == MLNDL_OK) {
				err = mlndlHttpRequest(httpCtx, 0, reqBuf->size, reqBuf->bytes,
					&respLen, &respBody);
				if (err == MLNDL_OK) {
					mlndlMemSet(respHeader, 0, sizeof(respHeader));
					if (mlndlHttpGetResponseHeader(httpCtx, "Content-Type",
						respHeader) != MLNDL_OK) {
						err = MLNDL_RES_HEADER_CONTENT_TYPE_MISSING;
					} else if (mlndlStrNCmp(respHeader,
						"application/x-mln-personalization", 33) != 0) {
						err = MLNDL_RES_HEADER_CONTENT_TYPE_MISMATCH;
					} else if (mlndlHttpGetResponseHeader(httpCtx,
						"X-Mln-Per-Version", respHeader) != MLNDL_OK) {
						err = MLNDL_RES_HEADER_VERSION_MISSING;
					} else if (mlndlStrNCmp(respHeader, "1.0", 3) != 0) {
						err = MLNDL_RES_HEADER_VERSION_MISMATCH;
					}
					if (err == MLNDL_OK) {
						err = mlndlParseStatusHeader(httpCtx,
							"X-Mln-Per-Status", statusInfo);
						if (err == MLNDL_OK) {
							respBuf.size = respLen;
							respBuf.bytes = (uint8_t *)respBody;
							marlinErr = (uint32_t)SceMarlin_14097094(
								marlinHandle, &respBuf);
							if (marlinErr != 0)
								err = mlndlMapMarlinError(
									marlinErr);
						}
					}
				}
			}
		}
	}

	mlndlHttpFreeBody(respBody);
	if (engine == MLNDL_MODE_ENGINE_MARLIN)
		SceMarlin_9609E2DF(reqBuf);
	mlndlHttpClose(httpCtx);
	return err;
}

int mlndlProtoPersonalizeChecked(const MlndlConfig *config, int mode, MlndlMarlinKind *kind,
	void *marlinHandle, MlndlStatusInfo *statusInfo)
{
	if (config != NULL && config->accountIdLen > 0 && config->accountId != NULL
		&& config->loginId != NULL && config->password != NULL
		&& config->personalizeUrl != NULL) {
		return mlndlProtoPersonalize(config, mode, kind, marlinHandle, statusInfo);
	}
	return MLNDL_RES_INVALID_PARAM;
}
