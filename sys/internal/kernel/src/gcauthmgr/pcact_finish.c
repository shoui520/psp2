/* Activation verification and completion. */
#include <sysmem/sysmem.h>
#include <psp2kern/kernel/threadmgr/misc.h>
#include <sysmem/cpu.h>
#include <psp2common/kernel/rtc.h>
#include "pcact.h"

GcAuthPsmactActivationFile gcAuthPsmactActivationFile;

int GcAuthPsmactVerifyActivationWithStack(const GcAuthPsmactActivation *activation,
		const GcAuthPsmactRandomRecord *random, GcAuthPsmactDerivedKeys *output);
int GcAuthPsmactDecryptActivation(const GcAuthPsmactDerivedKeys *keys);
int kscePsmDrmWriteActData(void *activation, const char *aesKey);
extern SceRtcTick gcAuthActivationRetryDeadline;
int GcAuthPcactBeginActivation(const GcAuthPcactBeginActivationRequest *request);
int GcAuthPcactApplyActivation(const GcAuthPcactApplyActivationRequest *request);
int GcAuthPsmactBeginActivation(const GcAuthPsmactBeginActivationRequest *request);

int GcAuthPsmactApplyActivation(const GcAuthPsmactApplyActivationRequest *request)
{
	const GcAuthPsmactActivation *activation = request->activation;
	const GcAuthPsmactRandomRecord *random = request->random;
	GcAuthPsmactEncryptedActivation *encryptedActivation =
		request->encryptedActivation;
	GcAuthPsmactDerivedKeys keys;
	unsigned char mode;
	unsigned char *cursor;
	unsigned int index;
	unsigned int modes;
	int permission;
	int result;

	permission = ksceKernelSetPermission(128);
	if (permission < 0)
		return permission;
	if (activation == NULL || random == NULL)
		goto invalid;
	result = SceSysmemForDriver_BC996A7A(&gcAuthActivationWorkspace.psmact,
		activation, sizeof(*activation));
	if (result < 0)
		goto finish;
	mode = gcAuthActivationWorkspace.psmact.header.mode;
	if ((unsigned char)(mode - 1) > 5)
		goto invalid;
	modes = 1U << (unsigned char)(mode - 1);
	if (modes & 0x39) {
		result = GcAuthPsmactVerifyActivationWithStack(
			&gcAuthActivationWorkspace.psmact, random, &keys);
	} else if (modes & 6) {
		result = GcAuthPsmactVerifyActivationWithStack(
			&gcAuthActivationWorkspace.psmact,
			&gcAuthActivationRandom.psmact, &keys);
	} else {
		goto invalid;
	}
	if (result != 0)
		goto finish;
	switch (gcAuthActivationWorkspace.psmact.header.mode) {
	case 1:
	case 3:
		result = 0;
		break;
	case 2:
		if (encryptedActivation == NULL)
			goto invalid;
		result = SceSysmemForDriver_BC996A7A(
			&gcAuthActivationWorkspace.encryptedPsmact,
			encryptedActivation, sizeof(*encryptedActivation));
		if (result < 0)
			break;
		result = GcAuthPsmactDecryptActivation(&keys);
		if (result != 0)
			break;
		for (index = 0; index != sizeof(gcAuthPsmactActivationFile); ++index)
			gcAuthPsmactActivationFile.serialized[index] =
				gcAuthActivationWorkspace.encryptedPsmact.ciphertext[index];
		result = kscePsmDrmWriteActData(&gcAuthPsmactActivationFile, NULL);
		result &= result >> 31;
		break;
	case 4:
	case 5:
	case 6:
		if (encryptedActivation == NULL)
			goto invalid;
		result = SceSysmemForDriver_BC996A7A(
			&gcAuthActivationWorkspace.encryptedPsmact,
			encryptedActivation, sizeof(*encryptedActivation));
		if (result < 0)
			break;
		result = GcAuthPsmactDecryptActivation(&keys);
		if (result != 0)
			break;
		result = SceSysmemForDriver_6D88EF8A(encryptedActivation,
			gcAuthActivationWorkspace.encryptedPsmact.ciphertext,
			sizeof(gcAuthActivationWorkspace.encryptedPsmact.ciphertext));
		result &= result >> 31;
		break;
	default:
		goto invalid;
	}
	goto finish;
invalid:
	result = (int)0x808a080e;
finish:
	cursor = (unsigned char *)&gcAuthActivationRandom;
	do {
		*cursor++ = 0;
	} while (cursor != (unsigned char *)&gcAuthActivationRandom +
		 sizeof(gcAuthActivationRandom));
	cursor = (unsigned char *)&keys;
	do {
		*cursor++ = 0;
	} while (cursor != (unsigned char *)&keys + sizeof(keys));
	ksceKernelSetPermission(permission);
	return result;
}

int GcAuthResetState(void)
{
	gcAuthActivationRetryDeadline.tick = 0;
	return 0;
}

int GcAuthPcactBeginActivationWithStack(unsigned int mode,
		const unsigned int *entropy, GcAuthPcactEncryptedChallenge *output)
{
	GcAuthPcactBeginActivationRequest request;
	unsigned int state;
	int result;

	ENTER_SYSCALL(state);
	request.mode = mode;
	request.entropy = entropy;
	request.output = output;
	result = ksceKernelRunWithStack(8192,
		(int (*)(void *))GcAuthPcactBeginActivation, &request);
	EXIT_SYSCALL(state);
	return result;
}

int GcAuthPcactApplyActivationWithStack(const GcAuthPcactActivationBundle *activation,
		unsigned int size)
{
	GcAuthPcactApplyActivationRequest request;
	unsigned int state;
	int result;

	ENTER_SYSCALL(state);
	if (activation != NULL) {
		result = SceSysmemForDriver_BC996A7A(&gcAuthActivationWorkspace.pcact,
			activation, sizeof(*activation));
		if (result >= 0) {
			request.activation = &gcAuthActivationWorkspace.pcact;
			request.size = size;
			result = ksceKernelRunWithStack(8192,
				(int (*)(void *))GcAuthPcactApplyActivation, &request);
		}
	} else {
		result = (int)0x808a0380;
	}
	EXIT_SYSCALL(state);
	return result;
}

int GcAuthPsmactBeginActivationWithStack(unsigned char mode,
		GcAuthPsmactChallenge *output, GcAuthPsmactRandomRecord *random)
{
	GcAuthPsmactBeginActivationRequest request;
	unsigned int state;
	int result;

	ENTER_SYSCALL(state);
	request.mode = mode;
	request.output = output;
	request.random = random;
	result = ksceKernelRunWithStack(8192,
		(int (*)(void *))GcAuthPsmactBeginActivation, &request);
	EXIT_SYSCALL(state);
	return result;
}

int GcAuthPsmactApplyActivationWithStack(const GcAuthPsmactActivation *activation,
		const GcAuthPsmactRandomRecord *random,
		GcAuthPsmactEncryptedActivation *encryptedActivation)
{
	GcAuthPsmactApplyActivationRequest request;
	unsigned int state;
	int result;

	ENTER_SYSCALL(state);
	request.activation = activation;
	request.random = random;
	request.encryptedActivation = encryptedActivation;
	result = ksceKernelRunWithStack(8192,
		(int (*)(void *))GcAuthPsmactApplyActivation, &request);
	EXIT_SYSCALL(state);
	return result;
}
