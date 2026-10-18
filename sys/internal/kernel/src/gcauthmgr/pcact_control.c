/* Pcact activation control and transport. */
#include <threadmgr/threadmgr.h>
#include <sysmem/sysmem.h>
#include <string.h>
#include "pcact.h"
#include <sysmem/utils.h>
#include <rtc/rtc.h>
#include <npdrm/npdrm.h>
#include <npdrm/psmdrm.h>

SceRtcTick gcAuthActivationCurrentTick;
SceNpDrmActivationData gcAuthNpDrmActivation;
SceUInt64 gcAuthActivationRtcSeconds;
unsigned char gcAuthPcactActivationMode;
SceRtcTick gcAuthActivationRetryDeadline;
GcAuthActivationWorkspace gcAuthActivationWorkspace;
GcAuthActivationRandom gcAuthActivationRandom;
GcAuthPcactChallengeInput gcAuthPcactActivationInput;

int GcAuthPcactEncryptChallengeWithStack(unsigned char mode,
		const GcAuthPcactChallengeInput *input,
		const unsigned char *rtcSeconds, GcAuthPcactEncryptedChallenge *output,
		GcAuthPcactRandomRecord *random);
int GcAuthPcactGetAesDecryptKeyWithStack(const GcAuthPcactActivationBlock *activation,
		const GcAuthPcactRandomRecord *random, unsigned char *outputKey);
int GcAuthPsmactCreateChallengeWithStack(unsigned char mode,
		const unsigned char *rtcSeconds, GcAuthPsmactChallenge *output,
		GcAuthPsmactRandomRecord *random);
int GcAuthAesDecrypt(unsigned char *data, size_t size,
		const void *key, const unsigned char *iv);

int GcAuthPcactBeginActivation(const GcAuthPcactBeginActivationRequest *request)
{
	unsigned int mode = request->mode;
	const unsigned int *entropy = request->entropy;
	GcAuthPcactEncryptedChallenge *output = request->output;
	SceUInt64 tick;
	unsigned int *destination;
	unsigned int word0, word1, word2, word3;
	unsigned char challengeMode;
	int permission;
	int result;

	permission = sceKernelSetThreadAccessLevel(128);
	if (permission < 0)
		return permission;
	if (entropy == NULL || output == NULL)
		goto invalid;
	result = SceRtcForDriver_401C0954(&gcAuthActivationCurrentTick);
	if (result < 0)
		goto finish;
	switch (mode) {
	case 0:
		gcAuthPcactActivationMode = 0;
		if (gcAuthActivationCurrentTick.tick <
		    gcAuthActivationRetryDeadline.tick) {
			sceKernelSetThreadAccessLevel(permission);
			return (int)0x808a0384;
		}
		tick = gcAuthActivationCurrentTick.tick;
		challengeMode = 0;
		goto challenge;
	case 2:
		challengeMode = 2;
		gcAuthPcactActivationMode = 2;
		tick = gcAuthActivationCurrentTick.tick;
challenge:
		gcAuthPcactActivationInput = (GcAuthPcactChallengeInput){ 0 };
		gcAuthPcactActivationInput.format = 16;
		destination = gcAuthPcactActivationInput.entropyWords;
		word0 = entropy[0];
		word1 = entropy[1];
		word2 = entropy[2];
		word3 = entropy[3];
		destination[0] = word0;
		destination[1] = word1;
		destination[2] = word2;
		destination[3] = word3;
		word0 = entropy[4];
		word1 = entropy[5];
		word2 = entropy[6];
		word3 = entropy[7];
		destination[4] = word0;
		destination[5] = word1;
		destination[6] = word2;
		destination[7] = word3;
		gcAuthActivationRtcSeconds =
			(tick - 62135596800000000ULL) / 1000;
		result = GcAuthPcactEncryptChallengeWithStack(challengeMode,
			&gcAuthPcactActivationInput,
			(const unsigned char *)&gcAuthActivationRtcSeconds,
			output, &gcAuthActivationRandom.pcact);
		goto finish;
	case 1:
		gcAuthPcactActivationMode = 1;
		if (SceNpDrmForDriver_D91C3BCE(&gcAuthNpDrmActivation) >= 0) {
			result = (int)0x808a0383;
			goto finish;
		}
		SceNpDrmForDriver_8B85A509(NULL);
		gcAuthPcactActivationInput = (GcAuthPcactChallengeInput){ 0 };
		gcAuthPcactActivationInput.format = 16;
		destination = gcAuthPcactActivationInput.entropyWords;
		word0 = entropy[0];
		word1 = entropy[1];
		word2 = entropy[2];
		word3 = entropy[3];
		destination[0] = word0;
		destination[1] = word1;
		destination[2] = word2;
		destination[3] = word3;
		word0 = entropy[4];
		word1 = entropy[5];
		word2 = entropy[6];
		word3 = entropy[7];
		destination[4] = word0;
		destination[5] = word1;
		destination[6] = word2;
		destination[7] = word3;
		gcAuthActivationRtcSeconds =
			(gcAuthActivationCurrentTick.tick - 62135596800000000ULL) / 1000;
		GcAuthPcactEncryptChallengeWithStack(gcAuthPcactActivationMode,
			&gcAuthPcactActivationInput,
			(const unsigned char *)&gcAuthActivationRtcSeconds,
			output, &gcAuthActivationRandom.pcact);
		result = SceRtcForDriver_9C8AF264(&gcAuthActivationRetryDeadline,
			&gcAuthActivationCurrentTick, 30);
		goto finish;
	default:
		break;
	}
invalid:
	sceKernelSetThreadAccessLevel(permission);
	return (int)0x808a0380;
finish:
	sceKernelSetThreadAccessLevel(permission);
	return result;
}

int GcAuthPcactApplyActivation(const GcAuthPcactApplyActivationRequest *request)
{
	GcAuthPcactActivationBundle *activation = request->activation;
	unsigned int size = request->size;
	unsigned int aesKey[4];
	int permission;
	int result;

	permission = sceKernelSetThreadAccessLevel(128);
	if (permission < 0)
		return permission;
	if ((activation == NULL) | (size != sizeof(*activation))) {
		sceKernelSetThreadAccessLevel(permission);
		return (int)0x808a0380;
	}
	result = GcAuthPcactGetAesDecryptKeyWithStack(&activation->key,
		&gcAuthActivationRandom.pcact, (unsigned char *)aesKey);
	if (result >= 0)
		result = SceNpDrmForDriver_742EBAF4(&activation->npDrm,
			(const char *)aesKey);
	memset(&activation->npDrm, 0, sizeof(activation->npDrm));
	memset(&gcAuthActivationRandom, 0, sizeof(gcAuthActivationRandom));
	aesKey[0] = 0;
	aesKey[1] = 0;
	aesKey[2] = 0;
	aesKey[3] = 0;
	sceKernelSetThreadAccessLevel(permission);
	return result;
}

int GcAuthPsmactBeginActivation(const GcAuthPsmactBeginActivationRequest *request)
{
	unsigned char mode = request->mode;
	GcAuthPsmactChallenge *output = request->output;
	GcAuthPsmactRandomRecord *random = request->random;
	SceRtcTick tick;
	SceUInt64 accountId, startTime, endTime;
	SceUInt64 rtcSeconds;
	int actType, versionFlag;
	unsigned int index;
	unsigned char *destination;
	const unsigned char *source;
	int permission;
	int result;
	int copyResult;

	permission = sceKernelSetThreadAccessLevel(128);
	if (permission < 0)
		return permission;
	if (output == NULL || random == NULL)
		goto invalid;
	if (SceRtcForDriver_401C0954(&tick) < 0)
		goto clockError;
	switch (mode) {
	case 1:
	case 4:
	case 5:
	case 6:
		break;
	case 2:
		if (tick.tick < gcAuthActivationRetryDeadline.tick) {
			result = (int)0x808a080f;
			goto finishPsm;
		}
		break;
	case 3:
		if (ScePsmDrmForDriver_B09003A7(&actType, &versionFlag, &accountId,
			&startTime, &endTime) >= 0) {
			result = (int)0x808a0800;
			ScePsmDrmForDriver_4CD5375C(NULL);
			goto finishPsm;
		}
		ScePsmDrmForDriver_4CD5375C(NULL);
		break;
	default:
		goto invalid;
	}
	rtcSeconds = (tick.tick - 62135596800000000ULL) / 1000;
	result = GcAuthPsmactCreateChallengeWithStack(mode,
		(const unsigned char *)&rtcSeconds,
		&gcAuthActivationWorkspace.challenge, &gcAuthActivationRandom.psmact);
	if (result != 0)
		goto finishPsm;
	copyResult = sceKernelCopyToUser(output,
		&gcAuthActivationWorkspace.challenge, sizeof(*output));
	if (copyResult < 0) {
		result = copyResult;
		goto finishPsm;
	}
	if ((unsigned char)(mode - 1) > 5)
		goto invalid;
	if ((1U << (unsigned char)(mode - 1)) & 0x39) {
		destination = (unsigned char *)random;
		source = (const unsigned char *)&gcAuthActivationRandom;
		for (index = 0; index != sizeof(*random); ++index)
			destination[index] = source[index];
	} else if (((1U << (unsigned char)(mode - 1)) & 6) == 0) {
		goto invalid;
	}
	if (mode == 3 && SceRtcForDriver_9C8AF264(&gcAuthActivationRetryDeadline,
		&tick, 300) < 0)
		goto clockError;
	goto finishPsm;
invalid:
	result = (int)0x808a080e;
	goto finishPsm;
clockError:
	result = (int)0x808a0810;
finishPsm:
	sceKernelSetThreadAccessLevel(permission);
	return result;
}

int GcAuthPsmactDecryptActivation(const GcAuthPsmactDerivedKeys *keys)
{
	GcAuthPsmactEncryptedActivation *activation =
		&gcAuthActivationWorkspace.encryptedPsmact;
	const unsigned char *cursor;
	unsigned char iv[16];
	unsigned char mac[32];
	unsigned int index;
	int difference;

	if (activation->header.format != 48 ||
	    activation->header.packetType != 3 ||
	    activation->header.mode != 4 || activation->header.recordSize != 48)
		return (int)0x808a0807;
	cursor = activation->header.reserved;
	do {
		if (*cursor++ != 0)
			return (int)0x808a0807;
	} while (cursor != activation->header.reserved +
		 sizeof(activation->header.reserved));
	if (SceKernelUtilsForDriver_83EFA1CC(keys->stageTwoKey, 32, activation,
		offsetof(GcAuthPsmactEncryptedActivation, mac), mac) != 0)
		return (int)0x808a0808;
	difference = 0;
	for (index = 0; ; ) {
		const unsigned char *stored = &activation->mac[index];
		const unsigned char *computed = &mac[index];

		if (++index == 33)
			break;
		difference |= *stored ^ *computed;
	}
	if (difference != 0)
		return (int)0x808a0809;
	memset(iv, 0, sizeof(iv));
	GcAuthAesDecrypt(activation->ciphertext, sizeof(activation->ciphertext),
		keys->stageOneKey, iv);
	return 0;
}
