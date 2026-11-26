#include "npdrm_package_internal.h"

typedef struct NpDrmPackageDecryptOpt {
	SceOff offset;
	SceUInt32 identifier;
	SceUInt32 padding;
} NpDrmPackageDecryptOpt;

typedef struct NpDrmPackageStartedOpt {
	SceSize len;
	SceSize size;
	SceUInt64 padding;
} NpDrmPackageStartedOpt;

typedef struct NpDrmPackageFinishedOpt {
	SceSize size;
} NpDrmPackageFinishedOpt;

typedef struct NpDrmCheckActDataOpt {
	SceUInt64 *startTime;
	SceUInt64 *expirationTime;
	SceUInt64 padding;
} NpDrmCheckActDataOpt;

typedef struct NpDrmGetRifInfoOpt {
	char *contentId;
	SceUInt64 *accountId;
	int *licenseVersion;
	int *drmType;
	int *flags;
	SceUInt32 *skuFlags;
	SceUInt64 *startTime;
	SceUInt64 *expirationTime;
	SceUInt64 *licenseExtFlags;
} NpDrmGetRifInfoOpt;

typedef struct NpDrmCheckDrmResetOpt {
	SceUInt64 accountId;
	int *isReset;
	SceSize inSize;
} NpDrmCheckDrmResetOpt;

extern "C" {

int _sceNpDrmPackageCheck(const void *buf, SceSize size, const void *digest, SceUInt32 identifier);
int _sceNpDrmPackageDecrypt(void *buf, SceSize size, const NpDrmPackageDecryptOpt *opt);
int _sceNpDrmPackageTransform(void *buf, SceSize size, NpDrmTransformState *state, SceUInt32 flags);
int _sceNpDrmPackageInstallStarted(int kind, SceUInt32 total, const char *msg,
	const NpDrmPackageStartedOpt *opt);
int _sceNpDrmPackageInstallFinished(SceUInt32 result, const char *msg, SceSize msgLen,
	const NpDrmPackageFinishedOpt *opt);
int _sceNpDrmPackageUninstallStarted(int kind, SceUInt32 total, const char *msg,
	const NpDrmPackageStartedOpt *opt);
int _sceNpDrmPackageUninstallFinished(SceUInt32 result, const char *msg, SceSize msgLen,
	const NpDrmPackageFinishedOpt *opt);
int _sceNpDrmSaveDataInstallStarted(int kind, SceUInt32 total, const char *msg,
	const NpDrmPackageStartedOpt *opt);
int _sceNpDrmSaveDataInstallFinished(SceUInt32 result, const char *msg, SceSize msgLen,
	const NpDrmPackageFinishedOpt *opt);
int _sceNpDrmSaveDataFormatStarted(int kind, SceUInt32 total, const char *msg,
	const NpDrmPackageStartedOpt *opt);
int _sceNpDrmSaveDataFormatFinished(SceUInt32 result, const char *msg, SceSize msgLen,
	const NpDrmPackageFinishedOpt *opt);
int _sceNpDrmCheckActData(int *actType, int *versionFlag, SceUInt64 *accountId,
	const NpDrmCheckActDataOpt *opt);
int _sceNpDrmRemoveActData(SceUInt64 *accountId);
int _sceNpDrmGetRifInfo(const void *license, SceSize licenseSize, int checkSign,
	const NpDrmGetRifInfoOpt *opt);
int _sceNpDrmGetRifName(char *rifName, SceUInt64 accountId);
int _sceNpDrmGetFixedRifName(char *rifName, SceUInt64 accountId);
int _sceNpDrmGetRifNameForInstall(char *rifName, const void *license, SceUInt32 isFixed);
int _sceNpDrmCheckDrmReset(const void *inAddr, SceSize size, const NpDrmCheckDrmResetOpt *opt);
int _sceNpDrmPresetRifProvisionalFlag(void *license);

int sceNpDrmPackageCheck(const void *buf, SceSize size, const void *digest, SceUInt32 identifier)
{
	return _sceNpDrmPackageCheck(buf, size, digest, identifier);
}

int sceNpDrmPackageDecrypt(void *buf, SceSize size, SceOff offset, SceUInt32 identifier)
{
	NpDrmPackageDecryptOpt opt;

	opt.offset = offset;
	opt.identifier = identifier;
	return _sceNpDrmPackageDecrypt(buf, size, &opt);
}

int sceNpDrmPackageTransform(void *buf, SceSize size, NpDrmTransformState *state, SceUInt32 flags)
{
	return _sceNpDrmPackageTransform(buf, size, state, flags);
}

int sceNpDrmPackageInstallStarted(int kind, SceUInt32 total, const char *msg, SceSize msgLen)
{
	NpDrmPackageStartedOpt opt;

	opt.len = msgLen;
	opt.size = msgLen;
	return _sceNpDrmPackageInstallStarted(kind, total, msg, &opt);
}

int sceNpDrmPackageInstallFinished(SceUInt32 result, const char *msg, SceSize msgLen)
{
	NpDrmPackageFinishedOpt opt;

	opt.size = msgLen;
	return _sceNpDrmPackageInstallFinished(result, msg, msgLen, &opt);
}

int sceNpDrmPackageUninstallStarted(int kind, SceUInt32 total, const char *msg, SceSize msgLen)
{
	NpDrmPackageStartedOpt opt;

	opt.len = msgLen;
	opt.size = msgLen;
	return _sceNpDrmPackageUninstallStarted(kind, total, msg, &opt);
}

int sceNpDrmPackageUninstallFinished(SceUInt32 result, const char *msg, SceSize msgLen)
{
	NpDrmPackageFinishedOpt opt;

	opt.size = msgLen;
	return _sceNpDrmPackageUninstallFinished(result, msg, msgLen, &opt);
}

int sceNpDrmSaveDataInstallStarted(int kind, SceUInt32 total, const char *msg, SceSize msgLen)
{
	NpDrmPackageStartedOpt opt;

	opt.len = msgLen;
	opt.size = msgLen;
	return _sceNpDrmSaveDataInstallStarted(kind, total, msg, &opt);
}

int sceNpDrmSaveDataInstallFinished(SceUInt32 result, const char *msg, SceSize msgLen)
{
	NpDrmPackageFinishedOpt opt;

	opt.size = msgLen;
	return _sceNpDrmSaveDataInstallFinished(result, msg, msgLen, &opt);
}

int sceNpDrmSaveDataFormatStarted(int kind, SceUInt32 total, const char *msg, SceSize msgLen)
{
	NpDrmPackageStartedOpt opt;

	opt.len = msgLen;
	opt.size = msgLen;
	return _sceNpDrmSaveDataFormatStarted(kind, total, msg, &opt);
}

int sceNpDrmSaveDataFormatFinished(SceUInt32 result, const char *msg, SceSize msgLen)
{
	NpDrmPackageFinishedOpt opt;

	opt.size = msgLen;
	return _sceNpDrmSaveDataFormatFinished(result, msg, msgLen, &opt);
}

int sceNpDrmCheckActData(int *actType, int *versionFlag, SceUInt64 *accountId,
	SceUInt64 *startTime, SceUInt64 *expirationTime)
{
	NpDrmCheckActDataOpt opt;

	opt.startTime = startTime;
	opt.expirationTime = expirationTime;
	return _sceNpDrmCheckActData(actType, versionFlag, accountId, &opt);
}

int sceNpDrmRemoveActData(SceUInt64 *accountId)
{
	return _sceNpDrmRemoveActData(accountId);
}

int sceNpDrmGetRifInfo(const void *license, SceSize licenseSize, int checkSign,
	char *contentId, SceUInt64 *accountId, int *licenseVersion, int *drmType,
	int *flags, SceUInt32 *skuFlags, SceUInt64 *startTime, SceUInt64 *expirationTime,
	SceUInt64 *licenseExtFlags)
{
	NpDrmGetRifInfoOpt opt;

	opt.contentId = contentId;
	opt.accountId = accountId;
	opt.licenseVersion = licenseVersion;
	opt.drmType = drmType;
	opt.flags = flags;
	opt.skuFlags = skuFlags;
	opt.startTime = startTime;
	opt.expirationTime = expirationTime;
	opt.licenseExtFlags = licenseExtFlags;
	return _sceNpDrmGetRifInfo(license, licenseSize, checkSign, &opt);
}

int sceNpDrmGetRifName(char *rifName, SceUInt64 accountId)
{
	return _sceNpDrmGetRifName(rifName, accountId);
}

int sceNpDrmGetFixedRifName(char *rifName, SceUInt64 accountId)
{
	return _sceNpDrmGetFixedRifName(rifName, accountId);
}

int sceNpDrmGetRifNameForInstall(char *rifName, const void *license, SceUInt32 isFixed)
{
	return _sceNpDrmGetRifNameForInstall(rifName, license, isFixed);
}

int sceNpDrmCheckDrmReset(const void *inAddr, SceSize size, SceUInt64 accountId, int *isReset)
{
	NpDrmCheckDrmResetOpt opt;

	opt.accountId = accountId;
	opt.isReset = isReset;
	opt.inSize = size;
	return _sceNpDrmCheckDrmReset(inAddr, size, &opt);
}

int sceNpDrmPresetRifProvisionalFlag(void *license)
{
	return _sceNpDrmPresetRifProvisionalFlag(license);
}

} /* extern "C" */
