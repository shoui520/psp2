/*
 * SceNpDrm startup and remaining export placeholders.
 */

#include <skeleton.h>
#include <psp2kern/kernel/threadmgr/workqueues.h>
#include <psp2kern/kernel/threadmgr/misc.h>
#include <iofilemgr/iofilemgr.h>
#include <npdrm/npdrm.h>
#include <npdrm/psmdrm.h>
#include <gcauthmgr/sblgcauthmgrdrmbb.h>
#include <ss_mgr/ss_mgr.h>
#include <sysmem/sblaimgr.h>
#include <psp2kern/kernel/threadmgr/mutex.h>
#include <rtc/rtc.h>
#include <string.h>
#include <stdio.h>
#include <authmgr/sblauthmgr.h>

struct NpDrmActivationKeyMaterial {
	unsigned char activationKey[16];
	unsigned char keyTableIndexKey[16];
	unsigned char bindingSeed[32];
	unsigned char prime[20];
	unsigned char coefficientA[20];
	unsigned char coefficientB[20];
	unsigned char order[20];
	unsigned char baseX[20];
	unsigned char baseY[20];
	unsigned char encryptionPadding[8];
};

_Static_assert(sizeof(struct NpDrmActivationKeyMaterial) == 192,
	"activation key material ABI");

struct NpDrmActivationCache {
	SceNpDrmActivationData data;
	unsigned char encryptionPadding[8];
};

_Static_assert(sizeof(struct NpDrmActivationCache) == 0x1040,
	"activation cache transport ABI");

struct PsmDrmAesMaterial {
	unsigned char key[16];
	unsigned char iv[16];
};

struct PsmDrmActivationData {
	char magic[8];
	unsigned int version;
	unsigned int flags;
	SceUInt64 accountId;
	unsigned int bindingValidation;
	unsigned int recordValidation;
	SceRtcTick startTime;
	SceRtcTick expirationTime;
	unsigned char activationChecksum[32];
	unsigned char authenticatedHeaderData[176];
	ScePsmDrmKeySet keyset;
	unsigned char rsaSignature[256];
};

union PsmDrmWorkingData {
	struct PsmDrmActivationData activation;
	ScePsmDrmLicense license;
};

struct PsmDrmActivationState {
	unsigned int version;
	unsigned int flags;
	SceUInt64 accountId;
	SceRtcTick startTime;
	SceRtcTick expirationTime;
	struct PsmDrmAesMaterial binding;
};

_Static_assert(sizeof(struct PsmDrmAesMaterial) == 32, "PSM AES material ABI");
_Static_assert(sizeof(struct PsmDrmActivationData) == 1024, "PSM activation ABI");
_Static_assert(sizeof(union PsmDrmWorkingData) == 1024, "PSM working data ABI");
_Static_assert(sizeof(struct PsmDrmActivationState) == 64, "PSM activation state ABI");
_Static_assert(offsetof(struct PsmDrmActivationData, keyset) == 256,
	"PSM activation keyset ABI");
_Static_assert(offsetof(struct PsmDrmActivationState, binding) == 32,
	"PSM activation binding ABI");

SceUID gNpDrmMutex = -1;
struct NpDrmActivationKeyMaterial gNpDrmActivationKeyMaterial;
int gNpDrmIsDevelopment;
struct NpDrmActivationCache gNpDrmActivationCache;
union PsmDrmWorkingData gPsmDrmWorkingData;
SceKernelOpenPsId gNpDrmOpenPsId;
int gNpDrmActivationValid;
int gPsmDrmActivationValid;
struct PsmDrmAesMaterial gPsmDrmAesMaterial;
struct PsmDrmActivationState gPsmDrmActivationState;

int NpDrmLoadSettings(void);
int NpDrmInitializeActivation(void);
int PsmDrmInitializeActivation(void);
int NpDrmCreateMutex(void);
int PsmDrmCreateMutex(void);
int NpDrmDeleteMutex(void) __attribute__((noinline));
void *NpDrmClearMemory(void *buffer, SceSize size);
int NpDrmGetAccountId(SceUInt64 *accountId);
int NpDrmGetDebugSkuFlag(unsigned int *skuFlag);
int NpDrmVerifyRifSignature(const SceNpDrmLicense *license, int vita);
int NpDrmAesDecrypt(void *buffer, SceSize size, const void *key, void **end);
int NpDrmVerifyActivationSignature(const SceNpDrmActivationData *data);
int NpDrmIsLooseAccountBind(void);
int PsmDrmVerifyActivation(void);
int NpDrmAesCbcDecrypt(void *buffer, SceSize size, const void *key,
	const void *iv, void **end);
int NpDrmAesEncrypt(void *buffer, SceSize size, const void *key, void **end);
int NpDrmUpdateAccountId(SceUInt64 accountId);
int NpDrmUpdateDebugSettings(void);
int NpDrmFormatRifName(char *name, SceSize size, SceUInt64 accountId,
	SceUInt64 kind);
int NpDrmEbootSigGenerate(const char *ebootPath, const void *sha256,
	int type, void *signature, unsigned int swVersion, void *buffer,
	unsigned int bufferSize);
int NpDrmEbootSigGenerateMultiDisc(const char *ebootPath, const void *discInfo,
	void *signature, unsigned int swVersion, void *buffer,
	unsigned int bufferSize);
int NpDrmEbootSigConvert(const char *ebootPath, const void *signature,
	void *newSignature, void *buffer, unsigned int bufferSize);
int NpDrmEbootSigVerify(const char *ebootPath, const void *signature,
	unsigned int signatureSize, void *buffer, unsigned int bufferSize);
int NpDrmPspEbootSigGenerate(const char *ebootPath, const void *sha256,
	void *signature, void *buffer, unsigned int bufferSize);
extern const struct NpDrmActivationKeyMaterial gNpDrmEncryptedActivationKeyMaterial;
int SceSblGcAuthMgrDrmBBForDriver_812B2B5C(int type, unsigned int *value);

static int SceNpDrmInit(void *arg)
{
	NpDrmLoadSettings();
	NpDrmInitializeActivation();
	PsmDrmInitializeActivation();
	return 1;
}

int module_start(SceSize arglen, const void *argp)
{
	int ret;

	if (NpDrmCreateMutex() < 0)
		return 2;
	ret = PsmDrmCreateMutex();
	if (ret < 0) {
		NpDrmDeleteMutex();
		return ret;
	}
	sceKernelScheduleWorkTask(0x10023, "SceNpDrmInit", SceNpDrmInit, NULL);
	return 0;
}

int NpDrmRemoveActivationFile(SceUInt64 *accountId)
{
	int permission;
	SceUID fd;
	int ret;

	if (accountId) {
		gNpDrmActivationValid = 0;
		*accountId = 0;
	} else {
		gNpDrmActivationValid = 0;
	}
	permission = sceKernelSetThreadAccessLevel(0x80);
	fd = sceIoOpen("tm0:/npdrm/act.dat", 0x04000003, 6);
	if (fd >= 0) {
		if (accountId && sceIoPread(fd, &gNpDrmActivationCache.data,
			sizeof(gNpDrmActivationCache.data), 0) ==
			sizeof(gNpDrmActivationCache.data))
			*accountId = gNpDrmActivationCache.data.account_id;
		NpDrmClearMemory(&gNpDrmActivationCache,
			sizeof(gNpDrmActivationCache));
		sceIoPwrite(fd, &gNpDrmActivationCache.data,
			sizeof(gNpDrmActivationCache.data), 0);
		sceIoClose(fd);
	}
	ret = sceIoRemove("tm0:/npdrm/act.dat");
	sceIoSync("tm0:", 0);
	if (ret == (int)0x80010002)
		ret = 0;
	NpDrmClearMemory(&gNpDrmActivationCache, sizeof(gNpDrmActivationCache));
	sceKernelSetThreadAccessLevel(permission);
	return ret;
}

int NpDrmGetRifKey(const SceNpDrmLicense *license, void *klicense,
	int *flags, unsigned int *skuFlags, SceUInt64 *startTime,
	SceUInt64 *expirationTime, int vita)
{
	SceUInt64 accountId;
	unsigned int cartSecret[8];
	SceNpDrmLicense rif;
	short licenseFlags;
	unsigned int drmType;
	unsigned int skuFlag;
	unsigned int version;
	unsigned int licenseType;
	unsigned int debugSkuFlag;
	unsigned int cartBlacklist;
	unsigned int cartIndex;
	int legacy;
	int deviceBound;
	int gameCard;
	int keyFlags;
	int ret;

	if (!klicense || !license)
		return (int)0x80870001;
	NpDrmClearMemory(klicense, 16);
	memcpy(&rif, license, sizeof(rif));
	ret = NpDrmVerifyRifSignature(&rif, vita);
	if (ret < 0)
		return ret;
	licenseFlags = __builtin_bswap16((unsigned short)rif.license_flags);
	drmType = licenseFlags & 0xff;
	gameCard = (licenseFlags & 0x400) != 0;
	deviceBound = (licenseFlags & 0x100) != 0;
	if (__builtin_bswap16((unsigned short)rif.license_type)) {
		legacy = 0;
		skuFlag = __builtin_bswap32(rif.sku_flag);
	} else {
		legacy = 1;
		skuFlag = 0;
	}
	if (gNpDrmIsDevelopment && !NpDrmGetDebugSkuFlag(&debugSkuFlag) &&
		debugSkuFlag)
		skuFlag = debugSkuFlag;
	keyFlags = (licenseFlags & 0x2000) != 0 || skuFlag == 1;
	if (__builtin_bswap64(rif.flags) & 1)
		keyFlags |= 2;
	if (deviceBound)
		keyFlags |= 0x10000;
	if (gameCard)
		keyFlags |= 0x20000;
	version = __builtin_bswap16((unsigned short)rif.version_flags);
	licenseType = __builtin_bswap16((unsigned short)rif.license_type);
	if (skuFlags)
		*skuFlags = skuFlag;
	if (startTime) {
		if (rif.start_time.tick) {
			SceUInt64 time = __builtin_bswap64(rif.start_time.tick);

			if (time && time != 0x7fffffffffffffffULL)
				time = time * 1000 + 62135596800000000ULL;
			else
				time = 0x7fffffffffffffffULL;
			*startTime = time;
		} else {
			*startTime = 63113871600000000ULL;
		}
	}
	if (expirationTime) {
		SceUInt64 time = __builtin_bswap64(rif.expiration_time.tick);

		if (time) {
			if (time != 0x7fffffffffffffffULL)
				time = time * 1000 + 62135596800000000ULL;
		} else {
			time = 0x7fffffffffffffffULL;
		}
		*expirationTime = time;
	}
	if ((licenseFlags & 0xfd) && drmType != 13)
		return (int)0x80870005;
	if (deviceBound) {
		if (legacy)
			return (int)0x80870005;
		if (memcmp(&gNpDrmOpenPsId, rif.open_psid, 16))
			return (int)0x80870009;
	}
	if (gameCard) {
		if (SceSblGcAuthMgrDrmBBForDriver_22FD5D23(rif.cmd56_handshake_part) < 0)
			return (int)0x8087000a;
		cartIndex = __builtin_bswap32(rif.debug_upgradable);
		if (cartIndex) {
			unsigned int bit = cartIndex - 1;

			if (bit > 31)
				return (int)0x80870005;
			if (SceSblGcAuthMgrDrmBBForDriver_812B2B5C(0,
				&cartBlacklist) < 0 || ((cartBlacklist >> bit) & 1))
				return (int)0x8087000a;
		}
	}
	if (keyFlags & 0x20000) {
		ret = SceSblGcAuthMgrDrmBBForDriver_BB70DDC0(cartSecret);
		if (ret < 0)
			return ret;
	}
	if (flags)
		*flags = keyFlags;
	if (version > 1 || (!licenseType && vita == 1))
		return (int)0x80870005;
	if (drmType == 13 && !vita)
		return (int)0x80870005;
	ret = sceKernelLockMutex(gNpDrmMutex, 1, NULL);
	if (ret < 0)
		return ret;
	if (drmType != 13 && !drmType && !gNpDrmIsDevelopment) {
		ret = (int)0x80870003;
		goto unlock;
	}
	licenseFlags = __builtin_bswap16((unsigned short)rif.license_flags);
	if ((licenseFlags & 0xff) != 0 && (licenseFlags & 0xff) != 13 &&
		!(licenseFlags & 0x400)) {
		accountId = gNpDrmActivationCache.data.account_id;
		if (!gNpDrmActivationValid || !accountId)
			NpDrmGetAccountId(&accountId);
		if (rif.account_id != accountId) {
			ret = (int)0x80870008;
			goto unlock;
		}
		if (!gNpDrmActivationValid) {
			ret = (int)0x80870006;
			goto unlock;
		}
		if (__builtin_bswap16(gNpDrmActivationCache.data.act_type) <
			__builtin_bswap16((unsigned short)rif.version)) {
			ret = (int)0x80870007;
			goto unlock;
		}
	}
	if (!vita && !(keyFlags & 0x20000)) {
		unsigned int index;

		NpDrmAesDecrypt(rif.key_table, 16,
			gNpDrmActivationKeyMaterial.keyTableIndexKey, NULL);
		__builtin_memcpy(&index, &rif.key_table[12], sizeof(index));
		index = __builtin_bswap32(index) & 0x7f;
		NpDrmAesDecrypt(rif.key1, 16,
			gNpDrmActivationCache.data.primary_key_table[index], NULL);
		if (drmType)
			__builtin_memcpy(klicense, rif.key1, 16);
		else
			memset(klicense, 0xaa, 16);
		NpDrmClearMemory(&rif, sizeof(rif));
		ret = 0;
		goto unlock;
	}
	NpDrmClearMemory(gNpDrmActivationCache.data.secondary_key_table, 256);
	__builtin_memcpy(gNpDrmActivationCache.data.secondary_key_table, rif.key2, 16);
	memcpy(gNpDrmActivationCache.data.secondary_key_table[6], &rif, 112);
	if (keyFlags & 0x20000) {
		__builtin_memcpy(gNpDrmActivationCache.data.secondary_key_table[4],
			cartSecret, 16);
		__builtin_memcpy(gNpDrmActivationCache.data.secondary_key_table[5],
			&cartSecret[4], 16);
		if (!vita) {
			__builtin_memcpy(gNpDrmActivationCache.data.secondary_key_table,
				gNpDrmActivationCache.data.secondary_key_table[11], 16);
			NpDrmClearMemory(gNpDrmActivationCache.data.secondary_key_table[11],
				16);
		}
	} else if (drmType == 13) {
		__builtin_memcpy(gNpDrmActivationCache.data.secondary_key_table[4],
			gNpDrmActivationKeyMaterial.bindingSeed, 16);
		__builtin_memcpy(gNpDrmActivationCache.data.secondary_key_table[5],
			&gNpDrmActivationKeyMaterial.bindingSeed[16], 16);
		ret = SceSblAuthMgrForDriver_41DAEA12(
			gNpDrmActivationCache.data.secondary_key_table, 16,
			gNpDrmActivationCache.data.secondary_key_table[4], 144, 0);
		if (ret < 0)
			goto clear;
		goto copyKey;
	} else {
		__builtin_memcpy(gNpDrmActivationCache.data.secondary_key_table[4],
			gNpDrmActivationCache.data.primary_key_table[0], 16);
		__builtin_memcpy(gNpDrmActivationCache.data.secondary_key_table[5],
			gNpDrmActivationCache.data.primary_key_table[1], 16);
	}
	ret = SceSblAuthMgrForDriver_41DAEA12(gNpDrmActivationCache.data.secondary_key_table,
		16, gNpDrmActivationCache.data.secondary_key_table[4], 144, 0);
	if (ret < 0)
		goto clear;
	if (!drmType) {
		memset(klicense, 0, 16);
		goto success;
	}
copyKey:
	__builtin_memcpy(klicense, gNpDrmActivationCache.data.secondary_key_table, 16);
success:
	NpDrmClearMemory(&rif, sizeof(rif));
	NpDrmClearMemory(gNpDrmActivationCache.data.secondary_key_table, 256);
	ret = 0;
	goto unlock;
clear:
	NpDrmClearMemory(&rif, sizeof(rif));
	NpDrmClearMemory(gNpDrmActivationCache.data.secondary_key_table, 256);
unlock:
	sceKernelUnlockMutex(gNpDrmMutex, 1);
	return ret;
}

int NpDrmLoadActivationFile(void)
{
	SceUInt64 accountId;
	SceIoStat stat;
	int permission;
	int statPermission;
	SceUID fd;
	int ret;
	int size;

	gNpDrmActivationValid = 0;
	permission = sceKernelSetThreadAccessLevel(0x80);
	statPermission = sceKernelSetThreadAccessLevel(0x80);
	ret = sceIoGetstat("tm0:/npdrm/act.dat", &stat);
	sceKernelSetThreadAccessLevel(statPermission);
	if (ret) {
		ret = (int)0x80870006;
		sceKernelSetThreadAccessLevel(permission);
		goto clear;
	}
	fd = sceIoOpen("tm0:/npdrm/act.dat", 0x04000001, 0);
	if (fd < 0) {
		ret = fd;
		sceKernelSetThreadAccessLevel(permission);
		goto clear;
	}
	size = sceIoRead(fd, &gNpDrmActivationCache.data,
		sizeof(gNpDrmActivationCache.data));
	sceIoClose(fd);
	if (size != sizeof(gNpDrmActivationCache.data)) {
		sceKernelSetThreadAccessLevel(permission);
		ret = size < 0 ? size : (int)0x80870002;
clear:
		NpDrmClearMemory(&gNpDrmActivationCache.data,
			sizeof(gNpDrmActivationCache.data));
		goto out;
	}
	sceKernelSetThreadAccessLevel(permission);
	ret = NpDrmVerifyActivationSignature(&gNpDrmActivationCache.data);
	if (ret < 0) {
		NpDrmClearMemory(&gNpDrmActivationCache.data,
			sizeof(gNpDrmActivationCache.data));
		goto out;
	}
	NpDrmGetAccountId(&accountId);
	if (gNpDrmActivationCache.data.account_id == accountId ||
		NpDrmIsLooseAccountBind()) {
		if (__builtin_bswap32(gNpDrmActivationCache.data.version) <= 1 ||
			!memcmp(&gNpDrmOpenPsId,
				gNpDrmActivationCache.data.openpsid, 16)) {
			ret = 0;
			NpDrmClearMemory(gNpDrmActivationCache.data.secondary_key_table,
				sizeof(gNpDrmActivationCache) -
				offsetof(struct NpDrmActivationCache, data.secondary_key_table));
			NpDrmAesDecrypt(gNpDrmActivationCache.data.primary_key_table,
				sizeof(gNpDrmActivationCache.data.primary_key_table),
				gNpDrmActivationKeyMaterial.activationKey, NULL);
			gNpDrmActivationValid = 1;
			goto out;
		}
		ret = (int)0x80870009;
	} else {
		ret = (int)0x80870008;
	}
	goto clear;
out:
	return ret;
}

int PsmDrmLoadActivationFile(void)
{
	SceIoStat stat;
	SceUInt64 tick;
	int permission;
	int statPermission;
	SceUID fd;
	int ret;
	int size;

	gPsmDrmActivationValid = 0;
	permission = sceKernelSetThreadAccessLevel(0x80);
	statPermission = sceKernelSetThreadAccessLevel(0x80);
	ret = sceIoGetstat("tm0:/psmdrm/act.dat", &stat);
	sceKernelSetThreadAccessLevel(statPermission);
	if (ret) {
		ret = (int)0x80870006;
		sceKernelSetThreadAccessLevel(permission);
		goto clear;
	}
	fd = sceIoOpen("tm0:/psmdrm/act.dat", 0x04000001, 0);
	if (fd < 0) {
		ret = fd;
		sceKernelSetThreadAccessLevel(permission);
		goto clear;
	}
	size = sceIoRead(fd, &gPsmDrmWorkingData, sizeof(gPsmDrmWorkingData));
	sceIoClose(fd);
	if (size != sizeof(gPsmDrmWorkingData)) {
		sceKernelSetThreadAccessLevel(permission);
		ret = size < 0 ? size : (int)0x80870002;
clear:
		NpDrmClearMemory(&gPsmDrmWorkingData, sizeof(gPsmDrmWorkingData));
		return ret;
	}
	sceKernelSetThreadAccessLevel(permission);
	ret = PsmDrmVerifyActivation();
	if (ret < 0) {
		NpDrmClearMemory(&gPsmDrmWorkingData, sizeof(gPsmDrmWorkingData));
		return ret;
	}
	__builtin_memcpy(&gPsmDrmActivationState.binding.key,
		&gPsmDrmWorkingData.activation.keyset, 16);
	__builtin_memcpy(&gPsmDrmActivationState.binding.iv,
		gPsmDrmWorkingData.activation.keyset.hmac_key + 16, 16);
	NpDrmAesCbcDecrypt(&gPsmDrmActivationState.binding,
		sizeof(gPsmDrmActivationState.binding), gPsmDrmAesMaterial.key,
		gPsmDrmAesMaterial.iv, NULL);
	gPsmDrmActivationState.version =
		__builtin_bswap32(gPsmDrmWorkingData.activation.version);
	gPsmDrmActivationState.flags =
		__builtin_bswap32(gPsmDrmWorkingData.activation.flags);
	gPsmDrmActivationState.accountId =
		__builtin_bswap64(gPsmDrmWorkingData.activation.accountId);
	tick = __builtin_bswap64(gPsmDrmWorkingData.activation.startTime.tick);
	if (!tick)
		tick = 0x7fffffffffffffffULL;
	else if (tick != 0x7fffffffffffffffULL)
		tick = tick * 1000 + 62135596800000000ULL;
	gPsmDrmActivationState.startTime.tick = tick;
	tick = __builtin_bswap64(gPsmDrmWorkingData.activation.expirationTime.tick);
	if (!tick)
		tick = 0x7fffffffffffffffULL;
	else if (tick != 0x7fffffffffffffffULL)
		tick = tick * 1000 + 62135596800000000ULL;
	gPsmDrmActivationState.expirationTime.tick = tick;
	NpDrmClearMemory(&gPsmDrmWorkingData, sizeof(gPsmDrmWorkingData));
	gPsmDrmActivationValid = 1;
	return 0;
}

int NpDrmCreateMutex(void)
{
	SceUID uid = sceKernelCreateMutex("SceNpDrm", 0, 0, NULL);

	if (uid >= 0) {
		gNpDrmMutex = uid;
		return 0;
	}
	return uid;
}

int NpDrmInitializeActivation(void)
{
	SceConsoleId consoleId;
	int ret;

	ret = SceSblSsMgrForDriver_FC6CDD68(&consoleId);
	if (ret < 0)
		return ret;
	ret = SceSblSsMgrForDriver_A5B5D269(&gNpDrmOpenPsId);
	if (ret < 0)
		return ret;
	gNpDrmIsDevelopment = SceSblAIMgrForDriver_274663A0() || SceSblAIMgrForDriver_F4B98F66();
	ret = sceKernelLockMutex(gNpDrmMutex, 1, NULL);
	if (ret < 0)
		return ret;
	memcpy(&gNpDrmActivationKeyMaterial, &gNpDrmEncryptedActivationKeyMaterial,
		sizeof(gNpDrmActivationKeyMaterial));
	ret = SceSblAuthMgrForDriver_868B9E9A(&gNpDrmActivationKeyMaterial,
		sizeof(gNpDrmActivationKeyMaterial), 0);
	NpDrmAesEncrypt(&gNpDrmActivationKeyMaterial,
		sizeof(gNpDrmActivationKeyMaterial.activationKey), &consoleId, NULL);
	NpDrmClearMemory(&consoleId, sizeof(consoleId));
	if (ret < 0) {
		sceKernelUnlockMutex(gNpDrmMutex, 1);
		return ret;
	}
	if (NpDrmLoadActivationFile() < 0 || !gNpDrmActivationValid)
		NpDrmClearMemory(&gNpDrmActivationCache, sizeof(gNpDrmActivationCache));
	sceKernelUnlockMutex(gNpDrmMutex, 1);
	return 0;
}

int NpDrmDeleteMutex(void)
{
	if (gNpDrmMutex >= 0) {
		sceKernelDeleteMutex(gNpDrmMutex);
		gNpDrmMutex = -1;
	}
	return 0;
}

int SceNpDrmForDriver_116FC0D6(SceUInt64 accountId)
{
	int ret = sceKernelLockMutex(gNpDrmMutex, 1, NULL);

	if (ret >= 0) {
		int result = NpDrmUpdateAccountId(accountId);

		sceKernelUnlockMutex(gNpDrmMutex, 1);
		return result;
	}
	return ret;
}

int SceNpDrmForDriver_A91C7443(void)
{
	int ret = sceKernelLockMutex(gNpDrmMutex, 1, NULL);

	if (ret >= 0) {
		int result = NpDrmUpdateDebugSettings();

		sceKernelUnlockMutex(gNpDrmMutex, 1);
		return result;
	}
	return ret;
}

int SceNpDrmForDriver_17573133(char *name,
	const SceNpDrmLicense *license, unsigned int isFixed)
{
	SceUInt64 accountId;
	unsigned int value;
	unsigned int licenseType;
	unsigned int drmType;
	unsigned int cartIndex;
	int freeLicense;
	int deviceBound;
	int gameCard;

	if (!license || !name || isFixed > 1)
		return (int)0x80870001;
	licenseType = __builtin_bswap16((unsigned short)license->license_type);
	drmType = __builtin_bswap16((unsigned short)license->license_flags);
	freeLicense = licenseType == 0;
	deviceBound = drmType & 0x100;
	gameCard = (drmType & 0x400) != 0;
	if (gNpDrmIsDevelopment) {
		NpDrmGetDebugSkuFlag(&value);
		licenseType = __builtin_bswap16((unsigned short)license->license_type);
	}
	accountId = license->account_id;
	if ((drmType & 0xfd) && (unsigned char)drmType != 13)
		return (int)0x80870005;
	if (deviceBound) {
		if (freeLicense)
			return (int)0x80870005;
		if (memcmp(&gNpDrmOpenPsId, license->open_psid, 16))
			return (int)0x80870009;
	}
	if (gameCard) {
		if (SceSblGcAuthMgrDrmBBForDriver_22FD5D23(license->cmd56_handshake_part) < 0)
			return (int)0x8087000a;
		cartIndex = __builtin_bswap32(license->debug_upgradable);
		if (cartIndex) {
			--cartIndex;
			if (cartIndex > 31)
				return (int)0x80870005;
			if (SceSblGcAuthMgrDrmBBForDriver_812B2B5C(0, &value) < 0 ||
				((value >> cartIndex) & 1))
				return (int)0x8087000a;
		}
	}
	if (isFixed) {
		if (!licenseType)
			return (int)0x80870005;
		if (gameCard)
			return NpDrmFormatRifName(name, 48, accountId, 1);
		if ((unsigned char)drmType == 13 || !accountId)
			return NpDrmFormatRifName(name, 48, 0, 1);
		return NpDrmFormatRifName(name, 48, accountId, 0);
	}
	snprintf(name, 47, "%s.rif", license->content_id);
	return 0;
}

int SceNpDrmForDriver_DF62F3B8(char *name, SceUInt64 accountId)
{
	if (!accountId) {
		NpDrmGetAccountId(&accountId);
		if (gNpDrmActivationValid && NpDrmIsLooseAccountBind())
			accountId = gNpDrmActivationCache.data.account_id;
		if (!accountId)
			return (int)0x8087000c;
	}
	return NpDrmFormatRifName(name, 48, accountId, 0);
}

int SceNpDrmForDriver_5D73448C(char *name, SceUInt64 accountId)
{
	return NpDrmFormatRifName(name, 48, accountId, 1);
}

int SceNpDrmForDriver_742EBAF4(const struct NpDrmActivationCache *activation,
	const void *key)
{
	SceIoStat stat;
	SceUInt64 accountId;
	int permission;
	int cleanupPermission;
	int written;
	int result;
	int verifyResult;

	if (!activation)
		return (int)0x80870001;
	result = sceKernelLockMutex(gNpDrmMutex, 1, NULL);
	if (result < 0)
		return result;
	gNpDrmActivationValid = 0;
	memcpy(&gNpDrmActivationCache, activation,
		sizeof(gNpDrmActivationCache));
	if (key)
		NpDrmAesCbcDecrypt(&gNpDrmActivationCache,
			sizeof(gNpDrmActivationCache), key, NULL, NULL);
	verifyResult = NpDrmVerifyActivationSignature(&gNpDrmActivationCache.data);
	if (verifyResult < 0) {
		NpDrmClearMemory(&gNpDrmActivationCache,
			sizeof(gNpDrmActivationCache));
		sceKernelUnlockMutex(gNpDrmMutex, 1);
		return verifyResult;
	}
	NpDrmGetAccountId(&accountId);
	if (gNpDrmActivationCache.data.account_id != accountId &&
		!NpDrmIsLooseAccountBind()) {
		result = (int)0x80870008;
		goto clear;
	}
	if (__builtin_bswap32(gNpDrmActivationCache.data.version) > 1 &&
		memcmp(&gNpDrmOpenPsId, gNpDrmActivationCache.data.openpsid, 16)) {
		result = (int)0x80870009;
		goto clear;
	}
	permission = sceKernelSetThreadAccessLevel(0x80);
	if (sceIoGetstat("tm0:/npdrm", &stat) < 0)
		sceIoMkdir("tm0:/npdrm", 6);
	result = sceIoOpen("tm0:/npdrm/act.dat", 0x04000602, 6);
	if (result < 0) {
		sceKernelSetThreadAccessLevel(permission);
		goto clear;
	}
	written = sceIoWrite(result, &gNpDrmActivationCache.data,
		sizeof(gNpDrmActivationCache.data));
	sceIoClose(result);
	if (written < (int)sizeof(gNpDrmActivationCache.data)) {
		gNpDrmActivationValid = 0;
		cleanupPermission = sceKernelSetThreadAccessLevel(0x80);
		result = sceIoOpen("tm0:/npdrm/act.dat", 0x04000003, 6);
		if (result >= 0) {
			NpDrmClearMemory(&gNpDrmActivationCache,
				sizeof(gNpDrmActivationCache));
			sceIoPwrite(result, &gNpDrmActivationCache.data,
				sizeof(gNpDrmActivationCache.data), 0);
			sceIoClose(result);
		}
		sceIoRemove("tm0:/npdrm/act.dat");
		result = written < 0 ? written : (int)0x80870002;
		sceIoSync("tm0:", 0);
		NpDrmClearMemory(&gNpDrmActivationCache,
			sizeof(gNpDrmActivationCache));
		sceKernelSetThreadAccessLevel(cleanupPermission);
		sceKernelSetThreadAccessLevel(permission);
		goto clear;
	}
	sceKernelSetThreadAccessLevel(permission);
	result = NpDrmLoadActivationFile();
	sceKernelUnlockMutex(gNpDrmMutex, 1);
	return result;

clear:
	NpDrmClearMemory(&gNpDrmActivationCache, sizeof(gNpDrmActivationCache));
	sceKernelUnlockMutex(gNpDrmMutex, 1);
	return result;
}

int SceNpDrmForDriver_8B85A509(SceUInt64 *accountId)
{
	SceIoStat stat;
	int permission;
	int result;
	int ret;

	ret = sceKernelLockMutex(gNpDrmMutex, 1, NULL);
	if (ret < 0)
		return ret;
	permission = sceKernelSetThreadAccessLevel(0x80);
	result = sceIoGetstat("tm0:/npdrm/act.dat", &stat);
	sceKernelSetThreadAccessLevel(permission);
	if (result) {
		if (accountId)
			*accountId = 0;
		if (gNpDrmActivationValid) {
			NpDrmClearMemory(&gNpDrmActivationCache,
				sizeof(gNpDrmActivationCache));
			gNpDrmActivationValid = 0;
		}
		sceKernelUnlockMutex(gNpDrmMutex, 1);
		return (int)0x80870006;
	}
	result = NpDrmRemoveActivationFile(accountId);
	sceKernelUnlockMutex(gNpDrmMutex, 1);
	return result;
}

int SceNpDrmForDriver_077926F5(void)
{
	SceIoStat stat;
	SceUInt64 accountId;
	int permission;
	int statPermission;
	int readSize;
	int result;

	result = sceKernelLockMutex(gNpDrmMutex, 1, NULL);
	if (result < 0)
		return result;
	gNpDrmActivationValid = 0;
	permission = sceKernelSetThreadAccessLevel(0x80);
	statPermission = sceKernelSetThreadAccessLevel(0x80);
	result = sceIoGetstat("tm0:/npdrm/act.dat", &stat);
	sceKernelSetThreadAccessLevel(statPermission);
	if (result) {
		result = (int)0x80870006;
		goto restore;
	}
	result = sceIoOpen("tm0:/npdrm/act.dat", 0x04000001, 0);
	if (result < 0)
		goto restore;
	readSize = sceIoRead(result, &gNpDrmActivationCache.data,
		sizeof(gNpDrmActivationCache.data));
	sceIoClose(result);
	if (readSize != sizeof(gNpDrmActivationCache.data)) {
		result = readSize < 0 ? readSize : (int)0x80870002;
		goto restore;
	}
	sceKernelSetThreadAccessLevel(permission);
	result = NpDrmVerifyActivationSignature(&gNpDrmActivationCache.data);
	if (result < 0)
		goto clear;
	NpDrmGetAccountId(&accountId);
	if (gNpDrmActivationCache.data.account_id != accountId &&
		!NpDrmIsLooseAccountBind()) {
		result = (int)0x80870008;
		goto clear;
	}
	if (__builtin_bswap32(gNpDrmActivationCache.data.version) > 1 &&
		memcmp(&gNpDrmOpenPsId, gNpDrmActivationCache.data.openpsid, 16)) {
		result = (int)0x80870009;
		goto clear;
	}
	NpDrmClearMemory(gNpDrmActivationCache.data.secondary_key_table, 0x7c0);
	NpDrmAesDecrypt(gNpDrmActivationCache.data.primary_key_table, 0x800,
		gNpDrmActivationKeyMaterial.activationKey, NULL);
	gNpDrmActivationValid = 1;
	sceKernelUnlockMutex(gNpDrmMutex, 1);
	return 0;

restore:
	sceKernelSetThreadAccessLevel(permission);
clear:
	NpDrmClearMemory(&gNpDrmActivationCache.data,
		sizeof(gNpDrmActivationCache.data));
	sceKernelUnlockMutex(gNpDrmMutex, 1);
	return result;
}

int SceNpDrmForDriver_D91C3BCE(SceNpDrmActivationData *activation)
{
	SceIoStat stat;
	SceUInt64 accountId;
	int permission;
	int statPermission;
	int readSize;
	int result;

	if (!activation)
		return (int)0x80870001;
	result = sceKernelLockMutex(gNpDrmMutex, 1, NULL);
	if (result < 0)
		return result;
	permission = sceKernelSetThreadAccessLevel(0x80);
	statPermission = sceKernelSetThreadAccessLevel(0x80);
	result = sceIoGetstat("tm0:/npdrm/act.dat", &stat);
	sceKernelSetThreadAccessLevel(statPermission);
	if (result) {
		result = (int)0x80870006;
		goto restore;
	}
	result = sceIoOpen("tm0:/npdrm/act.dat", 0x04000001, 0);
	if (result < 0)
		goto restore;
	readSize = sceIoRead(result, activation, sizeof(*activation));
	sceIoClose(result);
	if (readSize != sizeof(*activation)) {
		result = readSize < 0 ? readSize : (int)0x80870002;
		goto restore;
	}
	sceKernelSetThreadAccessLevel(permission);
	result = NpDrmVerifyActivationSignature(activation);
	if (result < 0)
		goto clear;
	NpDrmGetAccountId(&accountId);
	if (activation->account_id != accountId && !NpDrmIsLooseAccountBind()) {
		result = (int)0x80870008;
		goto clear;
	}
	if (__builtin_bswap32(activation->version) > 1 &&
		memcmp(&gNpDrmOpenPsId, activation->openpsid, 16)) {
		result = (int)0x80870009;
		goto clear;
	}
	sceKernelUnlockMutex(gNpDrmMutex, 1);
	return 0;

restore:
	sceKernelSetThreadAccessLevel(permission);
clear:
	NpDrmClearMemory(activation, sizeof(*activation));
	sceKernelUnlockMutex(gNpDrmMutex, 1);
	return result;
}

int SceNpDrmForDriver_FF63672D(SceNpDrmLicense *license)
{
	unsigned char klicense[16];
	unsigned int value;
	unsigned int licenseType;
	unsigned int drmType;
	unsigned int cartIndex;
	int freeLicense;
	int deviceBound;
	int gameCard;
	int result;

	if (!license)
		return (int)0x80870001;
	licenseType = __builtin_bswap16((unsigned short)license->license_type);
	drmType = __builtin_bswap16((unsigned short)license->license_flags);
	freeLicense = !licenseType;
	deviceBound = drmType & 0x100;
	gameCard = (drmType & 0x400) != 0;
	if (gNpDrmIsDevelopment) {
		NpDrmGetDebugSkuFlag(&value);
		licenseType = __builtin_bswap16((unsigned short)license->license_type);
	}
	if ((drmType & 0xfd) && (unsigned char)drmType != 13)
		return (int)0x80870005;
	if (deviceBound) {
		if (freeLicense)
			return (int)0x80870005;
		if (memcmp(&gNpDrmOpenPsId, license->open_psid, 16))
			return (int)0x80870009;
	}
	if (gameCard) {
		if (SceSblGcAuthMgrDrmBBForDriver_22FD5D23(license->cmd56_handshake_part) < 0)
			return (int)0x8087000a;
		cartIndex = __builtin_bswap32(license->debug_upgradable);
		if (cartIndex) {
			--cartIndex;
			if (cartIndex > 31)
				return (int)0x80870005;
			if (SceSblGcAuthMgrDrmBBForDriver_812B2B5C(0, &value) < 0 ||
				((value >> cartIndex) & 1))
				return (int)0x8087000a;
		}
	}
	if ((unsigned char)drmType == 13 || !(unsigned char)drmType)
		return (int)0x80870005;
	if (!licenseType || !gameCard)
		return 0;
	result = NpDrmGetRifKey(license, klicense, NULL, NULL, NULL, NULL, 1);
	if (result < 0)
		return result;
	NpDrmAesDecrypt(license->key_table, 16, klicense, NULL);
	NpDrmClearMemory(klicense, sizeof(klicense));
	__builtin_memcpy(license->key1, license->key_table, 16);
	NpDrmClearMemory(&license->flags, 0x168);
	return 0;
}

int SceNpDrmForDriver_DACB71F4(const SceNpDrmLicense *license, void *klicense,
	int *flags, SceUInt64 *startTime, SceUInt64 *expirationTime)
{
	return NpDrmGetRifKey(license, klicense, flags, NULL, startTime,
		expirationTime, 0);
}

int SceNpDrmForDriver_C070FE89(SceNpDrmLicense *license,
	unsigned int provisional)
{
	union {
		SceRtcTick currentTime;
		unsigned int skuFlag;
	} value;
	SceUInt64 oldProvisional;
	SceUInt64 startTime;
	SceUInt64 expirationTime;
	unsigned int licenseType;
	unsigned int drmType;
	unsigned int cartIndex;
	int result;

	if (!license || provisional > 1)
		return (int)0x80870001;
	oldProvisional = __builtin_bswap64(license->flags) & 1;
	if (!provisional) {
		license->flags &= ~0x0100000000000000ULL;
		result = NpDrmVerifyRifSignature(license, 1);
		if (result < 0) {
			if (oldProvisional)
				license->flags |= 0x0100000000000000ULL;
			return result;
		}
		return 0;
	}
	drmType = __builtin_bswap16((unsigned short)license->license_flags);
	licenseType = __builtin_bswap16((unsigned short)license->license_type);
	if (gNpDrmIsDevelopment)
		NpDrmGetDebugSkuFlag(&value.skuFlag);
	if (license->start_time.tick) {
		startTime = __builtin_bswap64(license->start_time.tick);
		if (startTime && startTime != 0x7fffffffffffffffULL)
			startTime = startTime * 1000 + 62135596800000000ULL;
		else
			startTime = 0x7fffffffffffffffULL;
	} else {
		startTime = 63113871600000000ULL;
	}
	expirationTime = __builtin_bswap64(license->expiration_time.tick);
	if (expirationTime) {
		if (expirationTime != 0x7fffffffffffffffULL)
			expirationTime = expirationTime * 1000 + 62135596800000000ULL;
	} else {
		expirationTime = 0x7fffffffffffffffULL;
	}
	if ((drmType & 0xfd) && (unsigned char)drmType != 13)
		return (int)0x80870005;
	if (drmType & 0x100) {
		if (!licenseType)
			return (int)0x80870005;
		if (memcmp(&gNpDrmOpenPsId, license->open_psid, 16))
			return (int)0x80870009;
	}
	if (drmType & 0x400) {
		if (SceSblGcAuthMgrDrmBBForDriver_22FD5D23(license->cmd56_handshake_part) < 0)
			return (int)0x8087000a;
		cartIndex = __builtin_bswap32(license->debug_upgradable);
		if (cartIndex) {
			--cartIndex;
			if (cartIndex > 31)
				return (int)0x80870005;
			if (SceSblGcAuthMgrDrmBBForDriver_812B2B5C(0, &value.skuFlag) < 0 ||
				((value.skuFlag >> cartIndex) & 1))
				return (int)0x8087000a;
		}
	}
	if (expirationTime == 0x7fffffffffffffffULL &&
		(SceRtcForDriver_401C0954(&value.currentTime) ||
		value.currentTime.tick < startTime)) {
		license->flags |= 0x0100000000000000ULL;
		result = NpDrmVerifyRifSignature(license, 1);
		if (result < 0) {
			if (!oldProvisional)
				license->flags &= ~0x0100000000000000ULL;
			return result;
		}
	}
	return 0;
}

int SceNpDrmForDriver_723322B5(const SceNpDrmLicense *license, void *klicense,
	int *flags, int *skuFlags, SceUInt64 *startTime, SceUInt64 *expirationTime)
{
	return NpDrmGetRifKey(license, klicense, flags,
		(unsigned int *)skuFlags, startTime, expirationTime, 1);
}

int SceNpDrmForDriver_DB406EAE(const SceNpDrmLicense *license, unsigned int licenseSize,
	int checkSign, char *contentId, SceUInt64 *accountId, int *licenseVersion,
	int *drmType, int *flags, unsigned int *skuFlags, SceUInt64 *startTime,
	SceUInt64 *expirationTime, SceUInt64 *flags2)
{
	union {
		SceUInt64 accountId;
		unsigned int skuFlag;
	} value;
	unsigned int licenseFlags;
	unsigned int type;
	unsigned int skuFlag;
	unsigned int resultFlags;
	unsigned int cartIndex;
	SceUInt64 time;
	int shortLicense;
	int deviceBound;
	int gameCard;
	int result;

	if (!license)
		return (int)0x80870001;
	if (licenseSize <= 151) {
		result = (int)0x80870004;
		type = 0;
		goto finishValidation;
	}
	if (checkSign) {
		result = NpDrmVerifyRifSignature(license,
			licenseSize >= 512 && license->license_type != 0);
		if (result < 0) {
			type = 0;
			goto finishValidation;
		}
	}
	licenseFlags = __builtin_bswap16((unsigned short)license->license_flags);
	type = (unsigned char)licenseFlags;
	deviceBound = (licenseFlags & 0x100) != 0;
	gameCard = (licenseFlags & 0x400) != 0;
	shortLicense = !__builtin_bswap16((unsigned short)license->license_type) ||
		licenseSize < 512;
	skuFlag = shortLicense ? 0 : __builtin_bswap32(license->sku_flag);
	if (gNpDrmIsDevelopment && !NpDrmGetDebugSkuFlag(&value.skuFlag) &&
		value.skuFlag)
		skuFlag = value.skuFlag;
	resultFlags = (licenseFlags & 0x2000) != 0 || skuFlag == 1;
	if (__builtin_bswap64(license->flags) & 1)
		resultFlags |= 2;
	if (deviceBound)
		resultFlags |= 0x10000;
	if (gameCard)
		resultFlags |= 0x20000;
	if (contentId)
		__builtin_memcpy(contentId, license->content_id, 48);
	if (licenseVersion)
		*licenseVersion = __builtin_bswap16((unsigned short)license->version);
	if (accountId)
		*accountId = license->account_id;
	if (flags)
		*flags = resultFlags;
	if (skuFlags)
		*skuFlags = skuFlag;
	if (startTime) {
		if (license->start_time.tick) {
			time = __builtin_bswap64(license->start_time.tick);
			if (time) {
				if (time != 0x7fffffffffffffffULL)
					time = time * 1000 + 62135596800000000ULL;
			} else {
				time = 0x7fffffffffffffffULL;
			}
		} else {
			time = 63113871600000000ULL;
		}
		*startTime = time;
	}
	if (expirationTime) {
		time = __builtin_bswap64(license->expiration_time.tick);
		if (time) {
			if (time != 0x7fffffffffffffffULL)
				time = time * 1000 + 62135596800000000ULL;
		} else {
			time = 0x7fffffffffffffffULL;
		}
		*expirationTime = time;
	}
	if (flags2) {
		if (!shortLicense || ((licenseFlags & 0x1000) &&
			license->expiration_time.tick && licenseSize > 159))
			*flags2 = __builtin_bswap64(license->flags) & ~1ULL;
		else
			*flags2 = 0;
	}
	result = (int)0x80870005;
	if ((licenseFlags & 0xfd) && type != 13)
		goto finishValidation;
	if (deviceBound) {
		if (shortLicense)
			goto finishValidation;
		if (memcmp(&gNpDrmOpenPsId, license->open_psid, 16)) {
			result = (int)0x80870009;
			goto finishValidation;
		}
	}
	if (gameCard) {
		if (SceSblGcAuthMgrDrmBBForDriver_22FD5D23(license->cmd56_handshake_part) < 0) {
			result = (int)0x8087000a;
			goto finishValidation;
		}
		cartIndex = __builtin_bswap32(license->debug_upgradable);
		if (cartIndex) {
			--cartIndex;
			if (cartIndex > 31)
				goto finishValidation;
			if (SceSblGcAuthMgrDrmBBForDriver_812B2B5C(0, &value.skuFlag) < 0 ||
				((value.skuFlag >> cartIndex) & 1)) {
				result = (int)0x8087000a;
				goto finishValidation;
			}
		}
	}
	result = 0;
finishValidation:
	if (drmType)
		*drmType = type;
	if (result)
		return result;
	result = sceKernelLockMutex(gNpDrmMutex, 1, NULL);
	if (result < 0)
		return result;
	result = 0;
	if (!type && !gNpDrmIsDevelopment) {
		result = (int)0x80870003;
		goto unlock;
	}
	licenseFlags = __builtin_bswap16((unsigned short)license->license_flags);
	if ((unsigned char)licenseFlags && (unsigned char)licenseFlags != 13 &&
		!(licenseFlags & 0x400)) {
		value.accountId = gNpDrmActivationCache.data.account_id;
		if (!gNpDrmActivationValid || !value.accountId)
			NpDrmGetAccountId(&value.accountId);
		if (license->account_id != value.accountId)
			result = (int)0x80870008;
		else if (!gNpDrmActivationValid)
			result = (int)0x80870006;
		else if (__builtin_bswap16((unsigned short)gNpDrmActivationCache.data.act_type) <
			__builtin_bswap16((unsigned short)license->version))
			result = (int)0x80870007;
	}
unlock:
	sceKernelUnlockMutex(gNpDrmMutex, 1);
	return result;
}

int NpDrmCheckDrmResetForDriver(const void *in_addr, unsigned int size,
	SceUInt64 account_id, int *pReset)
{
	const unsigned char *input = in_addr;
	SceIoStat stat;
	SceUInt64 activationAccountId;
	SceUInt64 currentAccountId;
	unsigned int activationType;
	unsigned int mode;
	int permission;
	int readPermission;
	int result;
	int readSize;
	SceUID fd;

	if (size - 2 > 62 || !input)
		return (int)0x80870001;
	mode = input[1];
	if (pReset)
		*pReset = 0;
	if (!input[0] || sceKernelLockMutex(gNpDrmMutex, 1, NULL) < 0)
		return 0;
	if (!gNpDrmActivationValid) {
		if (NpDrmLoadActivationFile() < 0)
			gNpDrmActivationValid = 0;
		else if (gNpDrmActivationValid)
			goto activationLoaded;
		permission = sceKernelSetThreadAccessLevel(0x80);
		result = sceIoGetstat("tm0:/npdrm/act.dat", &stat);
		sceKernelSetThreadAccessLevel(permission);
		if (!result) {
			readPermission = sceKernelSetThreadAccessLevel(0x80);
			permission = sceKernelSetThreadAccessLevel(0x80);
			result = sceIoGetstat("tm0:/npdrm/act.dat", &stat);
			sceKernelSetThreadAccessLevel(permission);
			if (!result) {
				fd = sceIoOpen("tm0:/npdrm/act.dat", 0x04000001, result);
				if (fd >= 0) {
					readSize = sceIoRead(fd, &gNpDrmActivationCache,
						sizeof(gNpDrmActivationCache));
					sceIoClose(fd);
					if (readSize == sizeof(gNpDrmActivationCache)) {
						sceKernelSetThreadAccessLevel(readPermission);
						goto activationLoaded;
					}
				}
			}
			sceKernelSetThreadAccessLevel(readPermission);
		}
		NpDrmClearMemory(&gNpDrmActivationCache, sizeof(gNpDrmActivationCache));
	}
activationLoaded:
	activationType = __builtin_bswap16((unsigned short)gNpDrmActivationCache.data.act_type);
	activationAccountId = gNpDrmActivationCache.data.account_id;
	if (!gNpDrmActivationValid) {
		permission = sceKernelSetThreadAccessLevel(0x80);
		result = sceIoGetstat("tm0:/npdrm/act.dat", &stat);
		sceKernelSetThreadAccessLevel(permission);
		if (!result && gNpDrmActivationCache.data.account_id) {
			NpDrmGetAccountId(&currentAccountId);
			if (gNpDrmActivationCache.data.account_id != currentAccountId)
				NpDrmIsLooseAccountBind();
		}
		sceKernelUnlockMutex(gNpDrmMutex, 1);
		return 0;
	}
	mode &= 0xc0;
	sceKernelUnlockMutex(gNpDrmMutex, 1);
	if (mode == 0x80) {
		if (!activationType)
			return 0;
	} else if (mode == 0xc0) {
		return 0;
	}
	if (activationAccountId != account_id && NpDrmIsLooseAccountBind())
		return 0;
	if (pReset)
		*pReset = 1;
	if (sceKernelLockMutex(gNpDrmMutex, 1, NULL) < 0)
		goto retryRemoval;
	permission = sceKernelSetThreadAccessLevel(0x80);
	result = sceIoGetstat("tm0:/npdrm/act.dat", &stat);
	sceKernelSetThreadAccessLevel(permission);
	if (result) {
		activationAccountId = 0;
		if (gNpDrmActivationValid) {
			NpDrmClearMemory(&gNpDrmActivationCache, sizeof(gNpDrmActivationCache));
			gNpDrmActivationValid = 0;
		}
		sceKernelUnlockMutex(gNpDrmMutex, 1);
		goto retryRemoval;
	}
	result = NpDrmRemoveActivationFile(&activationAccountId);
	sceKernelUnlockMutex(gNpDrmMutex, 1);
	if (result >= 0)
		return 0;
retryRemoval:
	result = sceKernelLockMutex(gNpDrmMutex, 1, NULL);
	if (result < 0)
		return result;
	permission = sceKernelSetThreadAccessLevel(0x80);
	result = sceIoGetstat("tm0:/npdrm/act.dat", &stat);
	sceKernelSetThreadAccessLevel(permission);
	if (result) {
		activationAccountId = 0;
		if (gNpDrmActivationValid) {
			NpDrmClearMemory(&gNpDrmActivationCache, sizeof(gNpDrmActivationCache));
			gNpDrmActivationValid = 0;
		}
		sceKernelUnlockMutex(gNpDrmMutex, 1);
		return (int)0x80870006;
	}
	result = NpDrmRemoveActivationFile(&activationAccountId);
	sceKernelUnlockMutex(gNpDrmMutex, 1);
	return result < 0 ? result : 0;
}

int SceNpDrmForDriver_90B1A6D3(const char *ebootPath, const void *sha256,
	void *signature, int swVersion)
{
	int result;
	int signatureResult;

	result = sceKernelLockMutex(gNpDrmMutex, 1, NULL);
	if (result >= 0) {
		signatureResult = NpDrmEbootSigGenerate(ebootPath, sha256, 2,
			signature, swVersion, gNpDrmActivationCache.data.secondary_key_table,
			0x7c0);
		sceKernelUnlockMutex(gNpDrmMutex, 1);
		return signatureResult;
	}
	return result;
}

int SceNpDrmForDriver_6D9223E1(const char *ebootPath, const void *sha256,
	void *signature, int swVersion)
{
	int result;
	int signatureResult;

	result = sceKernelLockMutex(gNpDrmMutex, 1, NULL);
	if (result >= 0) {
		signatureResult = NpDrmEbootSigGenerate(ebootPath, sha256, 3,
			signature, swVersion, gNpDrmActivationCache.data.secondary_key_table,
			0x7c0);
		sceKernelUnlockMutex(gNpDrmMutex, 1);
		return signatureResult;
	}
	return result;
}

int SceNpDrmForDriver_39A7A666(const char *ebootPath, const void *discInfo,
	void *signature, int swVersion)
{
	int result;
	int signatureResult;

	result = sceKernelLockMutex(gNpDrmMutex, 1, NULL);
	if (result >= 0) {
		signatureResult = NpDrmEbootSigGenerateMultiDisc(ebootPath, discInfo,
			signature, swVersion, gNpDrmActivationCache.data.secondary_key_table,
			0x7c0);
		sceKernelUnlockMutex(gNpDrmMutex, 1);
		return signatureResult;
	}
	return result;
}

int SceNpDrmForDriver_A29B75F9(const char *ebootPath, const void *signature,
	void *newSignature)
{
	int result;
	int signatureResult;

	result = sceKernelLockMutex(gNpDrmMutex, 1, NULL);
	if (result >= 0) {
		signatureResult = NpDrmEbootSigConvert(ebootPath, signature,
			newSignature, gNpDrmActivationCache.data.secondary_key_table,
			0x7c0);
		sceKernelUnlockMutex(gNpDrmMutex, 1);
		return signatureResult;
	}
	return result;
}

int SceNpDrmForDriver_7A319692(const char *ebootPath, const void *signature)
{
	int result;
	int signatureResult;

	result = sceKernelLockMutex(gNpDrmMutex, 1, NULL);
	if (result >= 0) {
		signatureResult = NpDrmEbootSigVerify(ebootPath, signature, 512,
			gNpDrmActivationCache.data.secondary_key_table, 0x7c0);
		sceKernelUnlockMutex(gNpDrmMutex, 1);
		return signatureResult;
	}
	return result;
}

int SceNpDrmForDriver_EF387FC4(const char *ebootPath, const void *sha256,
	void *signature)
{
	int result;
	int signatureResult;

	result = sceKernelLockMutex(gNpDrmMutex, 1, NULL);
	if (result >= 0) {
		signatureResult = NpDrmPspEbootSigGenerate(ebootPath, sha256,
			signature, gNpDrmActivationCache.data.secondary_key_table, 0x7c0);
		sceKernelUnlockMutex(gNpDrmMutex, 1);
		return signatureResult;
	}
	return result;
}

STUB_FUNC(int, SceNpDrmForDriver_4E321BDE, (void *a, void *b, void *c, void *d))
STUB_FUNC(int, SceNpDrmForDriver_9265B350, (int *actType, int *versionFlag, SceUInt64 *accountId,
	  SceUInt64 *startTime, SceUInt64 *expirationTime))
STUB_FUNC(int, SceNpDrmForDriver_B6CA3A2C, (const char *ebootPath, const void *signature))
STUB_FUNC(int, SceNpDrmForDriver_FC84CA1A, (void))
STUB(SceNpDrmForDriver_FE7B17B6)
STUB_FUNC(int, ScePsmDrmForDriver_4CD5375C, (SceUInt64 *accountId))
STUB(ScePsmDrmForDriver_791198CE)
STUB_FUNC(int, ScePsmDrmForDriver_8C8CFD01, (const ScePsmDrmLicense *license,
	  ScePsmDrmKeySet *keydata, int *flags, SceRtcTick *startTime, SceRtcTick *expirationTime))
STUB_FUNC(int, ScePsmDrmForDriver_984F9017, (const ScePsmDrmLicense *license, char *contentId,
	  SceUInt64 *accountId, SceUInt64 *startTime, SceUInt64 *expirationTime))
STUB_FUNC(int, ScePsmDrmForDriver_B09003A7, (int *actType, int *versionFlag, SceUInt64 *accountId,
	  SceUInt64 *startTime, SceUInt64 *expirationTime))
STUB_FUNC(int, ScePsmDrmForDriver_CB73E9D3, (void *activation, const char *aesKey))
