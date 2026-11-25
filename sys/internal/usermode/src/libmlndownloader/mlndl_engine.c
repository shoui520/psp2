#include "mlndl_internal.h"

static void *s_marlinWorkMemory;
static void *s_marlinSecureMemory;
static int32_t s_marlinInitCount;

int mlndlMarlinInit(void)
{
	void *workMem = NULL;
	void *secMem = NULL;
	uint32_t memSize = 0;
	int err = MLNDL_OK;
	uint32_t marlinErr = 0;

	if (s_marlinInitCount == 0) {
		if (s_marlinWorkMemory != NULL)
			mlndlFree(0, s_marlinWorkMemory);
		if (s_marlinSecureMemory != NULL)
			mlndlFree(0, s_marlinSecureMemory);

		s_marlinWorkMemory = mlndlAlloc(0, MLNDL_MARLIN_HEAP_SIZE);
		if (s_marlinWorkMemory == NULL) {
			err = MLNDL_RES_INTERNAL_ERROR;
		} else {
			memSize = MLNDL_MARLIN_HEAP_SIZE;
			s_marlinSecureMemory = mlndlAlloc(0, MLNDL_MARLIN_HEAP_SIZE);
			if (s_marlinSecureMemory == NULL) {
				err = MLNDL_RES_INTERNAL_ERROR;
			} else {
				workMem = s_marlinWorkMemory;
				secMem = s_marlinSecureMemory;
				s_marlinInitCount++;
			}
		}
	}

	if (err == MLNDL_OK) {
		marlinErr = (uint32_t)SceMarlin_83B44D33(workMem, memSize, secMem, memSize);
		if (marlinErr != 0)
			err = mlndlMapMarlinError(marlinErr);
	}

	if (err != MLNDL_OK || marlinErr != 0) {
		if (s_marlinWorkMemory != NULL) {
			mlndlFree(0, s_marlinWorkMemory);
			s_marlinWorkMemory = NULL;
		}
		if (s_marlinSecureMemory != NULL) {
			mlndlFree(0, s_marlinSecureMemory);
			s_marlinSecureMemory = NULL;
		}
	}

	return err;
}

int mlndlMarlinTerm(void)
{
	SceMarlin_C76DA6C0();
	if (--s_marlinInitCount == 0) {
		if (s_marlinWorkMemory != NULL) {
			mlndlFree(0, s_marlinWorkMemory);
			s_marlinWorkMemory = NULL;
		}
		if (s_marlinSecureMemory != NULL) {
			mlndlFree(0, s_marlinSecureMemory);
			s_marlinSecureMemory = NULL;
		}
	}
	return MLNDL_OK;
}

int mlndlCheckRegistration(const MlndlConfig *config, int mode, MlndlSession *session,
	int *outStatus)
{
	int err;

	if (outStatus == NULL || session == NULL)
		return MLNDL_RES_INVALID_PARAM;

	err = mlndlValidateSession(mode, session->kind, config);
	if (err == MLNDL_OK) {
		err = mlndlQueryRegistrationStatus(config, mode, session->kind,
			session->marlinHandle, outStatus);
		mlndlValidateHandle(mode, session->kind);
	}
	return err;
}

int mlndlPersonalize(const MlndlConfig *config, int mode, MlndlSession *session,
	MlndlStatusInfo *statusInfo)
{
	int err;

	if (session == NULL)
		return MLNDL_RES_INVALID_PARAM;

	err = mlndlValidateSession(mode, session->kind, config);
	if (err == MLNDL_OK) {
		err = mlndlProtoPersonalizeChecked(config, mode, session->kind,
			session->marlinHandle, statusInfo);
		mlndlValidateHandle(mode, session->kind);
	}
	return err;
}

int mlndlRegister(const MlndlConfig *config, int mode, MlndlSession *session,
	MlndlStatusInfo *statusInfo)
{
	int regStatus = 0;
	int err;

	if (session == NULL)
		return MLNDL_RES_INVALID_PARAM;

	err = mlndlValidateSession(mode, session->kind, config);
	if (err != MLNDL_OK)
		return err;

	err = mlndlQueryRegistrationStatus(config, mode, session->kind, session->marlinHandle,
		&regStatus);
	if (err == MLNDL_OK) {
		switch (regStatus) {
		case MLNDL_REG_REGISTERED:
			err = mlndlProtoRemoveRegistration(config, mode, session->kind,
				session->marlinHandle, 1);
			break;
		case MLNDL_REG_OTHER_USER:
			err = mlndlProtoRemoveRegistration(config, mode, session->kind,
				session->marlinHandle, 0);
			break;
		case MLNDL_REG_UNREGISTERED:
			break;
		case MLNDL_REG_UNPERSONALIZED:
			err = mlndlProtoPersonalizeChecked(config, mode, session->kind,
				session->marlinHandle, statusInfo);
			break;
		case MLNDL_REG_REVOKED:
		case MLNDL_REG_BROKEN_CERT:
		case MLNDL_REG_CORRUPTED:
			err = mlndlProtoResetRegistration(config, mode, session->kind,
				session->marlinHandle);
			if (err == MLNDL_OK)
				err = mlndlProtoPersonalizeChecked(config, mode, session->kind,
					session->marlinHandle, statusInfo);
			break;
		case MLNDL_REG_EXPIRED:
		case MLNDL_REG_CLOCK_TAMPERED:
			err = mlndlProtoResetRegistration(config, mode, session->kind,
				session->marlinHandle);
			break;
		default:
			err = MLNDL_RES_INTERNAL_ERROR;
			break;
		}
		if (err == MLNDL_OK)
			err = mlndlProtoRegister(config, mode, session->kind,
				session->marlinHandle, 0, statusInfo);
	}

	mlndlValidateHandle(mode, session->kind);
	return err;
}

int mlndlDeregister(const MlndlConfig *config, int mode, MlndlSession *session,
	MlndlStatusInfo *statusInfo)
{
	int regStatus = 0;
	int err;

	if (session == NULL)
		return MLNDL_RES_INVALID_PARAM;

	err = mlndlValidateSession(mode, session->kind, config);
	if (err != MLNDL_OK)
		return err;

	err = mlndlQueryRegistrationStatus(config, mode, session->kind, session->marlinHandle,
		&regStatus);
	if (err == MLNDL_OK) {
		switch (regStatus) {
		case MLNDL_REG_REGISTERED:
			err = mlndlProtoDeregister(config, mode, session->kind,
				session->marlinHandle, 0, statusInfo);
			if (err == MLNDL_OK)
				err = mlndlProtoRemoveRegistration(config, mode, session->kind,
					session->marlinHandle, 0);
			break;
		case MLNDL_REG_OTHER_USER:
			err = MLNDL_RES_OTHER_ACCOUNT;
			break;
		case MLNDL_REG_UNREGISTERED:
			err = mlndlProtoRemoveRegistration(config, mode, session->kind,
				session->marlinHandle, 0);
			if (err == MLNDL_OK)
				err = MLNDL_RES_NOT_REGISTERED;
			break;
		case MLNDL_REG_UNPERSONALIZED:
			err = MLNDL_RES_NOT_PERSONALIZED;
			break;
		case MLNDL_REG_REVOKED:
		case MLNDL_REG_EXPIRED:
		case MLNDL_REG_BROKEN_CERT:
		case MLNDL_REG_CLOCK_TAMPERED:
		case MLNDL_REG_CORRUPTED:
			err = mlndlProtoResetRegistration(config, mode, session->kind,
				session->marlinHandle);
			break;
		default:
			err = MLNDL_RES_INTERNAL_ERROR;
			break;
		}
	}

	mlndlValidateHandle(mode, session->kind);
	return err;
}

int mlndlCheckEntitlement(const MlndlConfig *config, int mode, MlndlSession *session,
	int *outStatus)
{
	int err;

	if (outStatus == NULL || session == NULL)
		return MLNDL_RES_INVALID_PARAM;

	err = mlndlValidateSession(mode, session->kind, config);
	if (err == MLNDL_OK) {
		err = mlndlQueryEntitlementStatus(config, mode, session->kind,
			session->marlinHandle, outStatus);
		mlndlValidateHandle(mode, session->kind);
	}
	return err;
}

static int mlndlMapStatusToError(int status)
{
	switch (status) {
	case MLNDL_REG_OTHER_USER:
		return MLNDL_RES_OTHER_ACCOUNT;
	case MLNDL_REG_UNREGISTERED:
		return MLNDL_RES_NOT_REGISTERED;
	case MLNDL_REG_UNPERSONALIZED:
		return MLNDL_RES_NOT_PERSONALIZED;
	case MLNDL_REG_REVOKED:
		return MLNDL_RES_DEVICE_REVOKED;
	case MLNDL_REG_EXPIRED:
		return MLNDL_RES_EXPIRED_CERT;
	case MLNDL_REG_BROKEN_CERT:
	case MLNDL_REG_CORRUPTED:
		return MLNDL_RES_INVALID_STATE;
	case MLNDL_REG_CLOCK_TAMPERED:
		return MLNDL_RES_CLOCK_ERROR;
	default:
		return MLNDL_RES_INTERNAL_ERROR;
	}
}

int mlndlSubscribe(const MlndlConfig *config, int mode, MlndlSession *session, int linkOnly,
	MlndlStatusInfo *statusInfo)
{
	int status = 0;
	int err;

	if (session == NULL)
		return MLNDL_RES_INVALID_PARAM;

	err = mlndlQueryRegistrationStatus(config, mode, session->kind, session->marlinHandle,
		&status);
	if (err != MLNDL_OK)
		return err;

	if (status == MLNDL_REG_ENTITLEMENT_BROKEN) {
		err = mlndlProtoResetRegistration(config, mode, session->kind,
			session->marlinHandle);
		if (err != MLNDL_OK)
			return err;
		linkOnly = 0;
	} else if (status != MLNDL_REG_REGISTERED && status != MLNDL_REG_NO_ENTITLEMENT) {
		return mlndlMapStatusToError(status);
	}

	err = mlndlQueryEntitlementStatus(config, mode, session->kind, session->marlinHandle,
		&status);
	if (err != MLNDL_OK)
		return err;

	if (status == MLNDL_REG_REGISTERED) {
		if (linkOnly == 0) {
			err = mlndlProtoRemoveRegistration(config, mode, session->kind,
				session->marlinHandle, 1);
			if (err != MLNDL_OK)
				return err;
		}
	} else if (status == MLNDL_REG_ENTITLEMENT_BROKEN) {
		err = mlndlProtoResetRegistration(config, mode, session->kind,
			session->marlinHandle);
		if (err != MLNDL_OK)
			return err;
		linkOnly = 0;
	} else if (status != MLNDL_REG_NO_ENTITLEMENT) {
		return mlndlMapStatusToError(status);
	}

	return mlndlProtoSubscribe(config, mode, session->kind, session->marlinHandle, linkOnly,
		0, statusInfo);
}

int mlndlUnsubscribe(const MlndlConfig *config, int mode, MlndlSession *session,
	MlndlStatusInfo *statusInfo)
{
	int status = 0;
	int err;

	if (session == NULL)
		return MLNDL_RES_INVALID_PARAM;

	err = mlndlQueryRegistrationStatus(config, mode, session->kind, session->marlinHandle,
		&status);
	if (err != MLNDL_OK)
		return err;

	if (status != MLNDL_REG_REGISTERED)
		return mlndlMapStatusToError(status);

	err = mlndlQueryEntitlementStatus(config, mode, session->kind, session->marlinHandle,
		&status);
	if (err != MLNDL_OK)
		return err;

	if (status == MLNDL_REG_REGISTERED)
		return mlndlProtoUnsubscribe(config, mode, session->kind, session->marlinHandle,
			0, statusInfo);
	if (status == MLNDL_REG_NO_ENTITLEMENT)
		return MLNDL_RES_NO_ENTITLEMENT;
	if (status == MLNDL_REG_ENTITLEMENT_BROKEN) {
		err = mlndlProtoResetRegistration(config, mode, session->kind,
			session->marlinHandle);
		if (err == MLNDL_OK)
			err = mlndlProtoUnsubscribe(config, mode, session->kind,
				session->marlinHandle, 0, statusInfo);
		return err;
	}
	return mlndlMapStatusToError(status);
}

int mlndlGetEntitlementPeriod(const MlndlConfig *config, int mode, MlndlSession *session,
	MlndlDateTime *outStart, MlndlDateTime *outEnd)
{
	if (config == NULL || session == NULL || outStart == NULL || outEnd == NULL)
		return MLNDL_RES_INVALID_PARAM;
	if (config->entitlementIdLen == 0 || config->entitlementId == NULL)
		return MLNDL_RES_INVALID_PARAM;
	return mlndlProtoGetValidity(config, mode, session->kind, session->marlinHandle,
		outStart, outEnd);
}

int mlndlPurgeExpiredEntitlements(int mode, MlndlSession *session,
	const MlndlDateTime *currentTime)
{
	if (currentTime == NULL)
		return MLNDL_RES_INVALID_PARAM;
	return mlndlProtoPurgeExpired(mode, session->kind, session->marlinHandle, currentTime);
}

int mlndlAcquireLicense(const MlndlConfig *config, const char *contentId, int mode,
	MlndlSession *session, uint32_t *outSize, void **outLicense)
{
	MlndlBuffer *licenseBuf = NULL;
	int err;

	if (session == NULL || contentId == NULL || outSize == NULL || outLicense == NULL) {
		err = MLNDL_RES_INVALID_PARAM;
	} else {
		err = mlndlValidateSession(mode, session->kind, config);
		if (err == MLNDL_OK) {
			err = mlndlProtoAcquireLicense(config, contentId, mode, session->kind,
				session->marlinHandle, 0, &licenseBuf);
			if (err == MLNDL_OK && licenseBuf != NULL) {
				*outLicense = mlndlAlloc(0, licenseBuf->size);
				if (*outLicense != NULL) {
					mlndlMemCpy(*outLicense, licenseBuf->bytes,
						licenseBuf->size);
					*outSize = licenseBuf->size;
				} else {
					err = MLNDL_RES_NO_MEMORY;
				}
			}
			mlndlValidateHandle(mode, session->kind);
		}
	}

	if (licenseBuf != NULL)
		SceMarlin_9609E2DF(licenseBuf);
	return err;
}

int mlndlGetMaxLicenseSize(const MlndlConfig *config, uint32_t *outSize)
{
	(void)config;
	if (outSize == NULL)
		return MLNDL_RES_INVALID_PARAM;
	*outSize = MLNDL_MAX_LICENSE_SIZE;
	return MLNDL_OK;
}

int mlndlFreeBuffer(MlndlBuffer *buffer)
{
	if (buffer == NULL)
		return MLNDL_RES_INVALID_PARAM;
	mlndlFree(0, buffer->bytes);
	mlndlFree(0, buffer);
	return MLNDL_OK;
}

int mlndlAllocBuffer(uint32_t size, const void *source, MlndlBuffer **outBuffer)
{
	MlndlBuffer *buffer = NULL;
	uint8_t *bytes = NULL;
	int err = MLNDL_OK;

	if (outBuffer == NULL) {
		err = MLNDL_RES_INVALID_PARAM;
	} else {
		buffer = (MlndlBuffer *)mlndlAlloc(0, sizeof(MlndlBuffer));
		if (buffer == NULL) {
			err = MLNDL_RES_NO_MEMORY;
		} else {
			if (size != 0) {
				bytes = (uint8_t *)mlndlAlloc(0, size);
				if (bytes == NULL) {
					err = MLNDL_RES_NO_MEMORY;
				} else {
					mlndlMemSet(bytes, 0, size);
					if (source != NULL)
						mlndlMemCpy(bytes, source, size);
				}
			}
			if (err == MLNDL_OK) {
				buffer->bytes = bytes;
				buffer->size = size;
				*outBuffer = buffer;
				bytes = NULL;
				buffer = NULL;
			}
		}
	}

	mlndlFree(0, bytes);
	if (buffer != NULL) {
		mlndlFree(0, buffer->bytes);
		mlndlFree(0, buffer);
	}
	return err;
}

int mlndlCompareBuffer(const MlndlBuffer *left, const MlndlBuffer *right)
{
	if (left == NULL || right == NULL)
		return MLNDL_RES_INVALID_PARAM;
	if (left->size != right->size)
		return MLNDL_RES_INVALID_PARAM;
	if (mlndlMemCmp(left->bytes, right->bytes, left->size) != 0)
		return MLNDL_RES_INVALID_PARAM;
	return MLNDL_OK;
}
