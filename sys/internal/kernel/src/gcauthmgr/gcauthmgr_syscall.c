/* User syscalls: copy the caller's buffers in and out of kernel memory. */
#include <kernel/error.h>
#include <sysmem/sysmem.h>
#include <string.h>
#include "pcact.h"
#include "adhoc.h"
#include "gcauth.h"
#include "ms_savebb.h"

int _sceSblGcAuthMgrPcactGetChallenge(unsigned int mode,
		const unsigned int *entropy, GcAuthPcactEncryptedChallenge *output,
		const SceSize *sizes)
{
	GcAuthPcactEncryptedChallenge *challenge;
	unsigned int *entropyCopy;
	SceSize size[4];
	int result;
	int copy;

	result = sceKernelCopyFromUser(size, sizes, sizeof(size));
	if (result < 0)
		return result;
	if (entropy != NULL) {
		if (size[0] > 32)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		entropyCopy = __builtin_alloca(32);
		result = sceKernelCopyFromUser(entropyCopy, entropy, size[0]);
		if (result < 0)
			return result;
		entropy = entropyCopy;
	}
	if (output == NULL)
		return GcAuthPcactBeginActivationWithStack(mode, entropy, NULL);
	if (size[1] > sizeof(*challenge))
		return SCE_KERNEL_ERROR_NO_MEMORY;
	challenge = __builtin_alloca(sizeof(*challenge));
	memset(challenge, 0, sizeof(*challenge));
	result = GcAuthPcactBeginActivationWithStack(mode, entropy, challenge);
	copy = sceKernelCopyToUser(output, challenge, size[1]);
	if (copy < 0)
		return copy;
	return result;
}

int _sceSblGcAuthMgrPcactActivation(const GcAuthPcactActivationBundle *activation,
		unsigned int size)
{
	return GcAuthPcactApplyActivationWithStack(activation, size);
}

int _sceSblGcAuthMgrPkgVry(const unsigned char *digest,
		const GcAuthAdhocSignature160 *signature, const SceSize *sizes)
{
	GcAuthAdhocSignature160 *signatureCopy;
	unsigned char *digestCopy;
	SceSize size[4];
	int result;

	result = sceKernelCopyFromUser(size, sizes, sizeof(size));
	if (result < 0)
		return result;
	if (digest != NULL) {
		if (size[0] > 20)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		digestCopy = __builtin_alloca(20);
		result = sceKernelCopyFromUser(digestCopy, digest, size[0]);
		if (result < 0)
			return result;
		digest = digestCopy;
	}
	if (signature != NULL) {
		if (size[1] > sizeof(*signatureCopy))
			return SCE_KERNEL_ERROR_NO_MEMORY;
		signatureCopy = __builtin_alloca(sizeof(*signatureCopy));
		result = sceKernelCopyFromUser(signatureCopy, signature, size[1]);
		if (result < 0)
			return result;
		signature = signatureCopy;
	}
	return GcAuthAdhocVerifyFixed160ForUser(digest, signature);
}

int _sceSblGcAuthMgrSclkGetData1(SceSblGcAuthMgrSclkData1 *output,
		const SceSize *sizes)
{
	SceSblGcAuthMgrSclkData1 *data;
	SceSize size[2];
	int result;
	int copy;

	result = sceKernelCopyFromUser(size, sizes, sizeof(size));
	if (result < 0)
		return result;
	if (output == NULL)
		return GcAuthPcactCreateRtcChallengeWithStack(NULL);
	if (size[0] > sizeof(*data))
		return SCE_KERNEL_ERROR_NO_MEMORY;
	data = __builtin_alloca(sizeof(*data));
	__builtin_memset(data, 0, sizeof(*data));
	result = GcAuthPcactCreateRtcChallengeWithStack(data);
	copy = sceKernelCopyToUser(output, data, size[0]);
	if (copy < 0)
		return copy;
	return result;
}

int _sceSblGcAuthMgrSclkSetData2(const SceSblGcAuthMgrSclkData2 *activation,
		const SceSize *sizes)
{
	SceSblGcAuthMgrSclkData2 *data;
	SceSize size[2];
	int result;

	result = sceKernelCopyFromUser(size, sizes, sizeof(size));
	if (result < 0)
		return result;
	if (activation != NULL) {
		if (size[0] > sizeof(*data))
			return SCE_KERNEL_ERROR_NO_MEMORY;
		data = __builtin_alloca(sizeof(*data));
		result = sceKernelCopyFromUser(data, activation, size[0]);
		if (result < 0)
			return result;
		activation = data;
	}
	return GcAuthPcactApplyRtcActivationWithStack(activation);
}

int _sceSblGcAuthMgrAdhocBB224Init(void)
{
	return GcAuthAdhocInitializeStateForUser();
}

int _sceSblGcAuthMgrAdhocBB224Shutdown(void)
{
	return GcAuthAdhocClearStateForUser();
}

int _sceSblGcAuthMgrAdhocBB224Auth1(GcAuthAdhocContext224 *context,
		unsigned char *packet, const SceSize *sizes)
{
	GcAuthAdhocContext224 *contextCopy;
	unsigned char *packetCopy;
	SceSize size[2];
	int result;
	int copy;

	result = sceKernelCopyFromUser(size, sizes, sizeof(size));
	if (result < 0)
		return result;
	contextCopy = context;
	if (context != NULL) {
		contextCopy = __builtin_alloca(sizeof(*contextCopy));
		memset(contextCopy, 0, sizeof(*contextCopy));
	}
	if (packet != NULL) {
		if (size[0] > 0xd0)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		packetCopy = __builtin_alloca(0xd0);
		memset(packetCopy, 0, 0xd0);
	} else {
		packetCopy = NULL;
	}
	result = GcAuthAdhocCreateContext224ForUser(contextCopy, packetCopy);
	if (context != NULL) {
		copy = sceKernelCopyToUser(context, contextCopy, sizeof(*contextCopy));
		if (copy < 0)
			return copy;
	}
	if (packet != NULL) {
		copy = sceKernelCopyToUser(packet, packetCopy, size[0]);
		if (copy < 0)
			return copy;
	}
	return result;
}

int _sceSblGcAuthMgrAdhocBB224Auth2(GcAuthAdhocContext224 *context,
		const unsigned char *packet, unsigned char *output, const SceSize *sizes)
{
	GcAuthAdhocContext224 *contextCopy;
	unsigned char *packetCopy;
	unsigned char *outputCopy;
	SceSize size[4];
	int result;
	int copy;

	result = sceKernelCopyFromUser(size, sizes, sizeof(size));
	if (result < 0)
		return result;
	contextCopy = context;
	if (context != NULL) {
		contextCopy = __builtin_alloca(sizeof(*contextCopy));
		memset(contextCopy, 0, sizeof(*contextCopy));
	}
	if (packet != NULL) {
		if (size[0] > 0xd0)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		packetCopy = __builtin_alloca(0xd0);
		result = sceKernelCopyFromUser(packetCopy, packet, size[0]);
		if (result < 0)
			return result;
		packet = packetCopy;
	}
	if (output != NULL) {
		if (size[1] > 0x170)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		outputCopy = __builtin_alloca(0x170);
		memset(outputCopy, 0, 0x170);
	} else {
		outputCopy = NULL;
	}
	result = GcAuthAdhocAnswerChallenge224ForUser(contextCopy, packet, outputCopy);
	if (context != NULL) {
		copy = sceKernelCopyToUser(context, contextCopy, sizeof(*contextCopy));
		if (copy < 0)
			return copy;
	}
	if (output != NULL) {
		copy = sceKernelCopyToUser(output, outputCopy, size[1]);
		if (copy < 0)
			return copy;
	}
	return result;
}

int _sceSblGcAuthMgrAdhocBB224Auth3(GcAuthAdhocContext224 *context,
		const unsigned char *packet, unsigned char *output, const SceSize *sizes)
{
	GcAuthAdhocContext224 *contextCopy;
	unsigned char *packetCopy;
	unsigned char *outputCopy;
	SceSize size[4];
	int result;
	int copy;

	result = sceKernelCopyFromUser(size, sizes, sizeof(size));
	if (result < 0)
		return result;
	if (context != NULL) {
		contextCopy = __builtin_alloca(sizeof(*contextCopy));
		memset(contextCopy, 0, sizeof(*contextCopy));
		result = sceKernelCopyFromUser(contextCopy, context, sizeof(*contextCopy));
		if (result < 0)
			return result;
	} else {
		contextCopy = NULL;
	}
	if (packet != NULL) {
		if (size[0] > 0x170)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		packetCopy = __builtin_alloca(0x170);
		result = sceKernelCopyFromUser(packetCopy, packet, size[0]);
		if (result < 0)
			return result;
		packet = packetCopy;
	}
	if (output != NULL) {
		if (size[1] > 0x130)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		outputCopy = __builtin_alloca(0x130);
		memset(outputCopy, 0, 0x130);
	} else {
		outputCopy = NULL;
	}
	result = GcAuthAdhocAuthenticateResponse224ForUser(contextCopy, packet,
		outputCopy);
	if (context != NULL) {
		copy = sceKernelCopyToUser(context, contextCopy, sizeof(*contextCopy));
		if (copy < 0)
			return copy;
	}
	if (output != NULL) {
		copy = sceKernelCopyToUser(output, outputCopy, size[1]);
		if (copy < 0)
			return copy;
	}
	return result;
}

int _sceSblGcAuthMgrAdhocBB224Auth4(GcAuthAdhocContext224 *context,
		const unsigned char *packet, unsigned char *output, const SceSize *sizes)
{
	GcAuthAdhocContext224 *contextCopy;
	unsigned char *packetCopy;
	unsigned char *outputCopy;
	SceSize size[4];
	int result;
	int copy;

	result = sceKernelCopyFromUser(size, sizes, sizeof(size));
	if (result < 0)
		return result;
	if (context != NULL) {
		contextCopy = __builtin_alloca(sizeof(*contextCopy));
		memset(contextCopy, 0, sizeof(*contextCopy));
		result = sceKernelCopyFromUser(contextCopy, context, sizeof(*contextCopy));
		if (result < 0)
			return result;
	} else {
		contextCopy = NULL;
	}
	if (packet != NULL) {
		if (size[0] > 0x130)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		packetCopy = __builtin_alloca(0x130);
		result = sceKernelCopyFromUser(packetCopy, packet, size[0]);
		if (result < 0)
			return result;
		packet = packetCopy;
	}
	if (output != NULL) {
		if (size[1] > 0x60)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		outputCopy = __builtin_alloca(0x60);
		memset(outputCopy, 0, 0x60);
	} else {
		outputCopy = NULL;
	}
	result = GcAuthAdhocVerifyResponse224ForUser(contextCopy, packet, outputCopy);
	if (context != NULL) {
		copy = sceKernelCopyToUser(context, contextCopy, sizeof(*contextCopy));
		if (copy < 0)
			return copy;
	}
	if (output != NULL) {
		copy = sceKernelCopyToUser(output, outputCopy, size[1]);
		if (copy < 0)
			return copy;
	}
	return result;
}

int _sceSblGcAuthMgrAdhocBB224Auth5(GcAuthAdhocContext224 *context,
		const GcAuthAdhocFinalPacket *packet, const SceSize *sizes)
{
	GcAuthAdhocContext224 *contextCopy;
	GcAuthAdhocFinalPacket *packetCopy;
	SceSize size[2];
	int result;
	int copy;

	result = sceKernelCopyFromUser(size, sizes, sizeof(size));
	if (result < 0)
		return result;
	if (context != NULL) {
		contextCopy = __builtin_alloca(sizeof(*contextCopy));
		memset(contextCopy, 0, sizeof(*contextCopy));
		result = sceKernelCopyFromUser(contextCopy, context, sizeof(*contextCopy));
		if (result < 0)
			return result;
	} else {
		contextCopy = NULL;
	}
	if (packet != NULL) {
		if (size[0] > 0x60)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		packetCopy = __builtin_alloca(0x60);
		result = sceKernelCopyFromUser(packetCopy, packet, size[0]);
		if (result < 0)
			return result;
		packet = packetCopy;
	}
	result = GcAuthAdhocFinishContext224ForUser(contextCopy, packet);
	if (context != NULL) {
		copy = sceKernelCopyToUser(context, contextCopy, sizeof(*contextCopy));
		if (copy < 0)
			return copy;
	}
	return result;
}

int _sceSblGcAuthMgrAdhocBB224GetKeys(const GcAuthAdhocContext224 *context,
		unsigned char *random, unsigned char *contextRandom,
		const GcAuthAdhocKeysParam *param)
{
	GcAuthAdhocKeysParam paramCopy;
	GcAuthAdhocContext224 *contextCopy;
	unsigned char *randomCopy;
	unsigned char *contextRandomCopy;
	unsigned char *seedCopy;
	unsigned char *seed;
	int result;
	int copy;

	result = sceKernelCopyFromUser(&paramCopy, param, sizeof(paramCopy));
	if (result < 0)
		return result;
	seed = paramCopy.seed;
	if (context != NULL) {
		contextCopy = __builtin_alloca(sizeof(*contextCopy));
		result = sceKernelCopyFromUser(contextCopy, context, sizeof(*contextCopy));
		if (result < 0)
			return result;
		context = contextCopy;
	}
	randomCopy = random;
	if (random != NULL) {
		if (paramCopy.randomSize > 16)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		randomCopy = __builtin_alloca(16);
		__builtin_memset(randomCopy, 0, 16);
	}
	if (contextRandom != NULL) {
		if (paramCopy.contextRandomSize > 16)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		contextRandomCopy = __builtin_alloca(16);
		__builtin_memset(contextRandomCopy, 0, 16);
	} else {
		contextRandomCopy = NULL;
	}
	if (seed != NULL) {
		if (paramCopy.seedSize > 16)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		seedCopy = __builtin_alloca(16);
		__builtin_memset(seedCopy, 0, 16);
	} else {
		seedCopy = NULL;
	}
	result = GcAuthAdhocGetSeeds224ForUser(context, randomCopy,
		contextRandomCopy, seedCopy);
	if (random != NULL) {
		copy = sceKernelCopyToUser(random, randomCopy, paramCopy.randomSize);
		if (copy < 0)
			return copy;
	}
	if (contextRandom != NULL) {
		copy = sceKernelCopyToUser(contextRandom, contextRandomCopy,
			paramCopy.contextRandomSize);
		if (copy < 0)
			return copy;
	}
	if (seed != NULL) {
		copy = sceKernelCopyToUser(seed, seedCopy, paramCopy.seedSize);
		if (copy < 0)
			return copy;
	}
	return result;
}

int _sceSblGcAuthMgrGetMediaIdType01(GcAuthCartSecretData *secret,
		const SceSize *sizes)
{
	GcAuthCartSecretData *secretCopy;
	SceSize size[2];
	int result;
	int copy;

	result = sceKernelCopyFromUser(size, sizes, sizeof(size));
	if (result < 0)
		return result;
	if (secret == NULL)
		return GcAuthGetCartSecretForUser(NULL);
	if (size[0] > sizeof(*secretCopy))
		return SCE_KERNEL_ERROR_NO_MEMORY;
	secretCopy = __builtin_alloca(sizeof(*secretCopy));
	__builtin_memset(secretCopy, 0, sizeof(*secretCopy));
	result = GcAuthGetCartSecretForUser(secretCopy);
	copy = sceKernelCopyToUser(secret, secretCopy, size[0]);
	if (copy < 0)
		return copy;
	return result;
}

int _sceSblGcAuthMgrAdhocBB160Init(void)
{
	return GcAuthAdhocInitializeState160ForUser();
}

int _sceSblGcAuthMgrAdhocBB160Shutdown(void)
{
	return GcAuthAdhocClearState160ForUser();
}

int _sceSblGcAuthMgrAdhocBB160Auth1(GcAuthAdhocContext160 *context,
		unsigned char *packet, const SceSize *sizes)
{
	GcAuthAdhocContext160 *contextCopy;
	unsigned char *packetCopy;
	SceSize size[2];
	int result;
	int copy;

	result = sceKernelCopyFromUser(size, sizes, sizeof(size));
	if (result < 0)
		return result;
	contextCopy = context;
	if (context != NULL) {
		contextCopy = __builtin_alloca(sizeof(*contextCopy));
		memset(contextCopy, 0, sizeof(*contextCopy));
	}
	if (packet != NULL) {
		if (size[0] > 0xa0)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		packetCopy = __builtin_alloca(0xa0);
		memset(packetCopy, 0, 0xa0);
	} else {
		packetCopy = NULL;
	}
	result = GcAuthAdhocCreateContext160ForUser(contextCopy, packetCopy);
	if (context != NULL) {
		copy = sceKernelCopyToUser(context, contextCopy, sizeof(*contextCopy));
		if (copy < 0)
			return copy;
	}
	if (packet != NULL) {
		copy = sceKernelCopyToUser(packet, packetCopy, size[0]);
		if (copy < 0)
			return copy;
	}
	return result;
}

int _sceSblGcAuthMgrAdhocBB160Auth2(GcAuthAdhocContext160 *context,
		const unsigned char *packet, unsigned char *output, const SceSize *sizes)
{
	GcAuthAdhocContext160 *contextCopy;
	unsigned char *packetCopy;
	unsigned char *outputCopy;
	SceSize size[4];
	int result;
	int copy;

	result = sceKernelCopyFromUser(size, sizes, sizeof(size));
	if (result < 0)
		return result;
	contextCopy = context;
	if (context != NULL) {
		contextCopy = __builtin_alloca(sizeof(*contextCopy));
		memset(contextCopy, 0, sizeof(*contextCopy));
	}
	if (packet != NULL) {
		if (size[0] > 0xa0)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		packetCopy = __builtin_alloca(0xa0);
		result = sceKernelCopyFromUser(packetCopy, packet, size[0]);
		if (result < 0)
			return result;
		packet = packetCopy;
	}
	if (output != NULL) {
		if (size[1] > 0x110)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		outputCopy = __builtin_alloca(0x110);
		memset(outputCopy, 0, 0x110);
	} else {
		outputCopy = NULL;
	}
	result = GcAuthAdhocAnswerChallenge160ForUser(contextCopy, packet, outputCopy);
	if (context != NULL) {
		copy = sceKernelCopyToUser(context, contextCopy, sizeof(*contextCopy));
		if (copy < 0)
			return copy;
	}
	if (output != NULL) {
		copy = sceKernelCopyToUser(output, outputCopy, size[1]);
		if (copy < 0)
			return copy;
	}
	return result;
}

int _sceSblGcAuthMgrAdhocBB160Auth3(GcAuthAdhocContext160 *context,
		const unsigned char *packet, unsigned char *output, const SceSize *sizes)
{
	GcAuthAdhocContext160 *contextCopy;
	unsigned char *packetCopy;
	unsigned char *outputCopy;
	SceSize size[4];
	int result;
	int copy;

	result = sceKernelCopyFromUser(size, sizes, sizeof(size));
	if (result < 0)
		return result;
	if (context != NULL) {
		contextCopy = __builtin_alloca(sizeof(*contextCopy));
		memset(contextCopy, 0, sizeof(*contextCopy));
		result = sceKernelCopyFromUser(contextCopy, context, sizeof(*contextCopy));
		if (result < 0)
			return result;
	} else {
		contextCopy = NULL;
	}
	if (packet != NULL) {
		if (size[0] > 0x110)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		packetCopy = __builtin_alloca(0x110);
		result = sceKernelCopyFromUser(packetCopy, packet, size[0]);
		if (result < 0)
			return result;
		packet = packetCopy;
	}
	if (output != NULL) {
		if (size[1] > 0x100)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		outputCopy = __builtin_alloca(0x100);
		memset(outputCopy, 0, 0x100);
	} else {
		outputCopy = NULL;
	}
	result = GcAuthAdhocAuthenticateResponse160ForUser(contextCopy, packet,
		outputCopy);
	if (context != NULL) {
		copy = sceKernelCopyToUser(context, contextCopy, sizeof(*contextCopy));
		if (copy < 0)
			return copy;
	}
	if (output != NULL) {
		copy = sceKernelCopyToUser(output, outputCopy, size[1]);
		if (copy < 0)
			return copy;
	}
	return result;
}

int _sceSblGcAuthMgrAdhocBB160Auth4(GcAuthAdhocContext160 *context,
		const unsigned char *packet, unsigned char *output, const SceSize *sizes)
{
	GcAuthAdhocContext160 *contextCopy;
	unsigned char *packetCopy;
	unsigned char *outputCopy;
	SceSize size[4];
	int result;
	int copy;

	result = sceKernelCopyFromUser(size, sizes, sizeof(size));
	if (result < 0)
		return result;
	if (context != NULL) {
		contextCopy = __builtin_alloca(sizeof(*contextCopy));
		memset(contextCopy, 0, sizeof(*contextCopy));
		result = sceKernelCopyFromUser(contextCopy, context, sizeof(*contextCopy));
		if (result < 0)
			return result;
	} else {
		contextCopy = NULL;
	}
	if (packet != NULL) {
		if (size[0] > 0x100)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		packetCopy = __builtin_alloca(0x100);
		result = sceKernelCopyFromUser(packetCopy, packet, size[0]);
		if (result < 0)
			return result;
		packet = packetCopy;
	}
	if (output != NULL) {
		if (size[1] > 0x60)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		outputCopy = __builtin_alloca(0x60);
		memset(outputCopy, 0, 0x60);
	} else {
		outputCopy = NULL;
	}
	result = GcAuthAdhocVerifyResponse160ForUser(contextCopy, packet, outputCopy);
	if (context != NULL) {
		copy = sceKernelCopyToUser(context, contextCopy, sizeof(*contextCopy));
		if (copy < 0)
			return copy;
	}
	if (output != NULL) {
		copy = sceKernelCopyToUser(output, outputCopy, size[1]);
		if (copy < 0)
			return copy;
	}
	return result;
}

int _sceSblGcAuthMgrAdhocBB160Auth5(GcAuthAdhocContext160 *context,
		const GcAuthAdhocFinalPacket *packet, const SceSize *sizes)
{
	GcAuthAdhocContext160 *contextCopy;
	GcAuthAdhocFinalPacket *packetCopy;
	SceSize size[2];
	int result;
	int copy;

	result = sceKernelCopyFromUser(size, sizes, sizeof(size));
	if (result < 0)
		return result;
	if (context != NULL) {
		contextCopy = __builtin_alloca(sizeof(*contextCopy));
		memset(contextCopy, 0, sizeof(*contextCopy));
		result = sceKernelCopyFromUser(contextCopy, context, sizeof(*contextCopy));
		if (result < 0)
			return result;
	} else {
		contextCopy = NULL;
	}
	if (packet != NULL) {
		if (size[0] > 0x60)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		packetCopy = __builtin_alloca(0x60);
		result = sceKernelCopyFromUser(packetCopy, packet, size[0]);
		if (result < 0)
			return result;
		packet = packetCopy;
	}
	result = GcAuthAdhocFinishContext160ForUser(contextCopy, packet);
	if (context != NULL) {
		copy = sceKernelCopyToUser(context, contextCopy, sizeof(*contextCopy));
		if (copy < 0)
			return copy;
	}
	return result;
}

int _sceSblGcAuthMgrAdhocBB160GetKeys(const GcAuthAdhocContext160 *context,
		unsigned char *random, unsigned char *contextRandom,
		const GcAuthAdhocKeysParam *param)
{
	GcAuthAdhocKeysParam paramCopy;
	GcAuthAdhocContext160 *contextCopy;
	unsigned char *randomCopy;
	unsigned char *contextRandomCopy;
	unsigned char *seedCopy;
	unsigned char *seed;
	int result;
	int copy;

	result = sceKernelCopyFromUser(&paramCopy, param, sizeof(paramCopy));
	if (result < 0)
		return result;
	seed = paramCopy.seed;
	if (context != NULL) {
		contextCopy = __builtin_alloca(sizeof(*contextCopy));
		result = sceKernelCopyFromUser(contextCopy, context, sizeof(*contextCopy));
		if (result < 0)
			return result;
		context = contextCopy;
	}
	randomCopy = random;
	if (random != NULL) {
		if (paramCopy.randomSize > 16)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		randomCopy = __builtin_alloca(16);
		__builtin_memset(randomCopy, 0, 16);
	}
	if (contextRandom != NULL) {
		if (paramCopy.contextRandomSize > 16)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		contextRandomCopy = __builtin_alloca(16);
		__builtin_memset(contextRandomCopy, 0, 16);
	} else {
		contextRandomCopy = NULL;
	}
	if (seed != NULL) {
		if (paramCopy.seedSize > 16)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		seedCopy = __builtin_alloca(16);
		__builtin_memset(seedCopy, 0, 16);
	} else {
		seedCopy = NULL;
	}
	result = GcAuthAdhocGetSeeds160ForUser(context, randomCopy,
		contextRandomCopy, seedCopy);
	if (random != NULL) {
		copy = sceKernelCopyToUser(random, randomCopy, paramCopy.randomSize);
		if (copy < 0)
			return copy;
	}
	if (contextRandom != NULL) {
		copy = sceKernelCopyToUser(contextRandom, contextRandomCopy,
			paramCopy.contextRandomSize);
		if (copy < 0)
			return copy;
	}
	if (seed != NULL) {
		copy = sceKernelCopyToUser(seed, seedCopy, paramCopy.seedSize);
		if (copy < 0)
			return copy;
	}
	return result;
}

int _sceSblGcAuthMgrAdhocBB160UniCastEncrypt(GcAuthAdhocContext160 *context,
		unsigned char *data, unsigned int size,
		const GcAuthAdhocEncryptParam *param)
{
	GcAuthAdhocEncryptParam paramCopy;
	GcAuthAdhocContext160 *contextCopy;
	unsigned char *dataCopy;
	GcAuthAdhocAuthTag *tagCopy;
	GcAuthAdhocAuthTag *tag;
	int encrypt;
	int result;
	int copy;

	result = sceKernelCopyFromUser(&paramCopy, param, sizeof(paramCopy));
	if (result < 0)
		return result;
	tag = paramCopy.tag;
	encrypt = paramCopy.encrypt;
	if (context != NULL) {
		contextCopy = __builtin_alloca(sizeof(*contextCopy));
		memset(contextCopy, 0, sizeof(*contextCopy));
		result = sceKernelCopyFromUser(contextCopy, context, sizeof(*contextCopy));
		if (result < 0)
			return result;
	} else {
		contextCopy = NULL;
	}
	dataCopy = data;
	if (data != NULL) {
		SceSize dataSize = paramCopy.dataSize;

		if (dataSize > 0x910)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		dataCopy = __builtin_alloca(0x910);
		memset(dataCopy, 0, 0x910);
		result = sceKernelCopyFromUser(dataCopy, data, dataSize);
		if (result < 0)
			return result;
	}
	if (tag != NULL) {
		if (paramCopy.tagSize > 0x20)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		tagCopy = __builtin_alloca(0x20);
		__builtin_memset(tagCopy, 0, 0x20);
	} else {
		tagCopy = NULL;
	}
	result = GcAuthAdhocUniCastEncrypt160ForUser(contextCopy, dataCopy, size,
		tagCopy, encrypt);
	if (context != NULL) {
		copy = sceKernelCopyToUser(context, contextCopy, sizeof(*contextCopy));
		if (copy < 0)
			return copy;
	}
	if (data != NULL) {
		copy = sceKernelCopyToUser(data, dataCopy, paramCopy.dataSize);
		if (copy < 0)
			return copy;
	}
	if (tag != NULL) {
		copy = sceKernelCopyToUser(tag, tagCopy, paramCopy.tagSize);
		if (copy < 0)
			return copy;
	}
	return result;
}

int _sceSblGcAuthMgrAdhocBB160UniCastDecrypt(GcAuthAdhocContext160 *context,
		unsigned char *data, unsigned int size,
		const GcAuthAdhocDecryptParam *param)
{
	GcAuthAdhocDecryptParam paramCopy;
	GcAuthAdhocContext160 *contextCopy;
	unsigned char *dataCopy;
	GcAuthAdhocAuthTag *tagCopy;
	const GcAuthAdhocAuthTag *tag;
	int decrypt;
	int checkCounter;
	int result;
	int copy;

	result = sceKernelCopyFromUser(&paramCopy, param, sizeof(paramCopy));
	if (result < 0)
		return result;
	tag = paramCopy.tag;
	decrypt = paramCopy.decrypt;
	checkCounter = paramCopy.checkCounter;
	if (context != NULL) {
		contextCopy = __builtin_alloca(sizeof(*contextCopy));
		memset(contextCopy, 0, sizeof(*contextCopy));
		result = sceKernelCopyFromUser(contextCopy, context, sizeof(*contextCopy));
		if (result < 0)
			return result;
	} else {
		contextCopy = NULL;
	}
	dataCopy = data;
	if (data != NULL) {
		SceSize dataSize = paramCopy.dataSize;

		if (dataSize > 0x910)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		dataCopy = __builtin_alloca(0x910);
		memset(dataCopy, 0, 0x910);
		result = sceKernelCopyFromUser(dataCopy, data, dataSize);
		if (result < 0)
			return result;
	}
	if (tag != NULL) {
		if (paramCopy.tagSize > 0x20)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		tagCopy = __builtin_alloca(0x20);
		result = sceKernelCopyFromUser(tagCopy, tag, paramCopy.tagSize);
		if (result < 0)
			return result;
		tag = tagCopy;
	}
	result = GcAuthAdhocUniCastDecrypt160ForUser(contextCopy, dataCopy, size,
		tag, decrypt, checkCounter);
	if (context != NULL) {
		copy = sceKernelCopyToUser(context, contextCopy, sizeof(*contextCopy));
		if (copy < 0)
			return copy;
	}
	if (data != NULL) {
		copy = sceKernelCopyToUser(data, dataCopy, paramCopy.dataSize);
		if (copy < 0)
			return copy;
	}
	return result;
}

int _sceSblGcAuthMgrAdhocBB160BroadCastEncrypt(unsigned char *data,
		unsigned int size, GcAuthAdhocAuthTag *tag,
		const GcAuthAdhocBroadCastEncryptParam *param)
{
	GcAuthAdhocBroadCastEncryptParam paramCopy;
	unsigned char *dataCopy;
	GcAuthAdhocAuthTag *tagCopy;
	int encrypt;
	int result;
	int copy;

	result = sceKernelCopyFromUser(&paramCopy, param, sizeof(paramCopy));
	if (result < 0)
		return result;
	encrypt = paramCopy.encrypt;
	if (data != NULL) {
		SceSize dataSize = paramCopy.dataSize;

		if (dataSize > 0x910)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		dataCopy = __builtin_alloca(0x910);
		memset(dataCopy, 0, 0x910);
		result = sceKernelCopyFromUser(dataCopy, data, dataSize);
		if (result < 0)
			return result;
	} else {
		dataCopy = NULL;
	}
	if (tag != NULL) {
		if (paramCopy.tagSize > 0x20)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		tagCopy = __builtin_alloca(0x20);
		__builtin_memset(tagCopy, 0, 0x20);
	} else {
		tagCopy = NULL;
	}
	result = GcAuthAdhocBroadCastEncrypt160ForUser(dataCopy, size, tagCopy,
		encrypt);
	if (data != NULL) {
		copy = sceKernelCopyToUser(data, dataCopy, paramCopy.dataSize);
		if (copy < 0)
			return copy;
	}
	if (tag != NULL) {
		copy = sceKernelCopyToUser(tag, tagCopy, paramCopy.tagSize);
		if (copy < 0)
			return copy;
	}
	return result;
}

int _sceSblGcAuthMgrAdhocBB160BroadCastDecrypt(GcAuthAdhocContext160 *context,
		unsigned char *data, unsigned int size,
		const GcAuthAdhocDecryptParam *param)
{
	GcAuthAdhocDecryptParam paramCopy;
	GcAuthAdhocContext160 *contextCopy;
	unsigned char *dataCopy;
	GcAuthAdhocAuthTag *tagCopy;
	const GcAuthAdhocAuthTag *tag;
	int decrypt;
	int checkCounter;
	int result;
	int copy;

	result = sceKernelCopyFromUser(&paramCopy, param, sizeof(paramCopy));
	if (result < 0)
		return result;
	tag = paramCopy.tag;
	decrypt = paramCopy.decrypt;
	checkCounter = paramCopy.checkCounter;
	if (context != NULL) {
		contextCopy = __builtin_alloca(sizeof(*contextCopy));
		memset(contextCopy, 0, sizeof(*contextCopy));
		result = sceKernelCopyFromUser(contextCopy, context, sizeof(*contextCopy));
		if (result < 0)
			return result;
	} else {
		contextCopy = NULL;
	}
	dataCopy = data;
	if (data != NULL) {
		SceSize dataSize = paramCopy.dataSize;

		if (dataSize > 0x910)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		dataCopy = __builtin_alloca(0x910);
		memset(dataCopy, 0, 0x910);
		result = sceKernelCopyFromUser(dataCopy, data, dataSize);
		if (result < 0)
			return result;
	}
	if (tag != NULL) {
		if (paramCopy.tagSize > 0x20)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		tagCopy = __builtin_alloca(0x20);
		result = sceKernelCopyFromUser(tagCopy, tag, paramCopy.tagSize);
		if (result < 0)
			return result;
		tag = tagCopy;
	}
	result = GcAuthAdhocBroadCastDecrypt160ForUser(contextCopy, dataCopy, size,
		tag, decrypt, checkCounter);
	if (context != NULL) {
		copy = sceKernelCopyToUser(context, contextCopy, sizeof(*contextCopy));
		if (copy < 0)
			return copy;
	}
	if (data != NULL) {
		copy = sceKernelCopyToUser(data, dataCopy, paramCopy.dataSize);
		if (copy < 0)
			return copy;
	}
	return result;
}

int _sceSblGcAuthMgrMsSaveBBMacInit(GcAuthMsSaveBbMacContext *context, int type)
{
	GcAuthMsSaveBbMacContext contextCopy;
	int result;
	int copy;

	if (context == NULL)
		return GcAuthMsSaveBbMacInitForUser(NULL, type);
	__builtin_memset(&contextCopy, 0, sizeof(contextCopy));
	result = sceKernelCopyFromUser(&contextCopy, context, sizeof(contextCopy));
	if (result < 0)
		return result;
	result = GcAuthMsSaveBbMacInitForUser(&contextCopy, type);
	copy = sceKernelCopyToUser(context, &contextCopy, sizeof(contextCopy));
	if (copy < 0)
		return copy;
	return result;
}

int _sceSblGcAuthMgrMsSaveBBMacUpdate(GcAuthMsSaveBbMacContext *context,
		const unsigned char *data, SceSize size)
{
	GcAuthMsSaveBbMacContext contextCopy;
	int result;
	int copy;

	if (context == NULL)
		return GcAuthMsSaveBbMacUpdateForUser(NULL, data, size);
	__builtin_memset(&contextCopy, 0, sizeof(contextCopy));
	result = sceKernelCopyFromUser(&contextCopy, context, sizeof(contextCopy));
	if (result < 0)
		return result;
	result = GcAuthMsSaveBbMacUpdateForUser(&contextCopy, data, size);
	copy = sceKernelCopyToUser(context, &contextCopy, sizeof(contextCopy));
	if (copy < 0)
		return copy;
	return result;
}

int _sceSblGcAuthMgrMsSaveBBMacFinal(GcAuthMsSaveBbMacContext *context,
		unsigned char *mac, const unsigned char *key, const SceSize *sizes)
{
	GcAuthMsSaveBbMacContext contextCopy;
	unsigned char macCopy[16];
	GcAuthMsSaveBbMacContext *contextArgument;
	unsigned char *macArgument;
	unsigned char *keyCopy;
	SceSize size[4];
	int result;
	int copy;

	result = sceKernelCopyFromUser(size, sizes, sizeof(size));
	if (result < 0)
		return result;
	if (context != NULL) {
		__builtin_memset(&contextCopy, 0, sizeof(contextCopy));
		contextArgument = &contextCopy;
		result = sceKernelCopyFromUser(&contextCopy, context, sizeof(contextCopy));
		if (result < 0)
			return result;
	} else {
		contextArgument = NULL;
	}
	macArgument = mac;
	if (mac != NULL) {
		if (size[0] > sizeof(macCopy))
			return SCE_KERNEL_ERROR_NO_MEMORY;
		__builtin_memset(macCopy, 0, sizeof(macCopy));
		macArgument = macCopy;
		result = sceKernelCopyFromUser(macCopy, mac, size[0]);
		if (result < 0)
			return result;
	}
	if (key != NULL) {
		if (size[1] > 16)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		keyCopy = __builtin_alloca(16);
		result = sceKernelCopyFromUser(keyCopy, key, size[1]);
		if (result < 0)
			return result;
		key = keyCopy;
	}
	result = GcAuthMsSaveBbMacFinalForUser(contextArgument, macArgument, key);
	if (context != NULL) {
		copy = sceKernelCopyToUser(context, contextArgument, sizeof(contextCopy));
		if (copy < 0)
			return copy;
	}
	if (mac != NULL) {
		copy = sceKernelCopyToUser(mac, macArgument, size[0]);
		if (copy < 0)
			return copy;
	}
	return result;
}

int _sceSblGcAuthMgrMsSaveBBCipherInit(GcAuthMsSaveBbCipherContext *context,
		int type, int mode, const GcAuthMsSaveBbCipherInitParam *param)
{
	GcAuthMsSaveBbCipherInitParam paramCopy;
	GcAuthMsSaveBbCipherContext contextCopy;
	unsigned char keyCopy[16];
	unsigned char seedCopy[16];
	GcAuthMsSaveBbCipherContext *contextArgument;
	unsigned char *keyArgument;
	const unsigned char *seed;
	unsigned char *key;
	int result;
	int copy;

	result = sceKernelCopyFromUser(&paramCopy, param, sizeof(paramCopy));
	if (result < 0)
		return result;
	seed = paramCopy.seed;
	key = paramCopy.key;
	if (context != NULL) {
		contextArgument = &contextCopy;
		__builtin_memset(&contextCopy, 0, sizeof(contextCopy));
		result = sceKernelCopyFromUser(&contextCopy, context, sizeof(contextCopy));
		if (result < 0)
			return result;
	} else {
		contextArgument = NULL;
	}
	keyArgument = key;
	if (key != NULL) {
		if (paramCopy.keySize > 16)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		keyArgument = keyCopy;
		__builtin_memset(keyCopy, 0, sizeof(keyCopy));
		result = sceKernelCopyFromUser(keyCopy, key, paramCopy.keySize);
		if (result < 0)
			return result;
	}
	if (seed != NULL) {
		if (paramCopy.seedSize > 16)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		result = sceKernelCopyFromUser(seedCopy, seed, paramCopy.seedSize);
		if (result < 0)
			return result;
		seed = seedCopy;
	}
	result = GcAuthMsSaveBbCipherInitForUser(contextArgument, type, mode,
		keyArgument, seed);
	if (context != NULL) {
		copy = sceKernelCopyToUser(context, contextArgument, sizeof(contextCopy));
		if (copy < 0)
			return copy;
	}
	if (key != NULL) {
		copy = sceKernelCopyToUser(key, keyArgument, paramCopy.keySize);
		if (copy < 0)
			return copy;
	}
	return result;
}

int _sceSblGcAuthMgrMsSaveBBCipherUpdate(GcAuthMsSaveBbCipherContext *context,
		unsigned char *data, SceSize size)
{
	GcAuthMsSaveBbCipherContext contextCopy;
	int result;
	int copy;

	if (context == NULL)
		return GcAuthMsSaveBbCipherUpdateForUser(NULL, data, size);
	__builtin_memset(&contextCopy, 0, sizeof(contextCopy));
	result = sceKernelCopyFromUser(&contextCopy, context, sizeof(contextCopy));
	if (result < 0)
		return result;
	result = GcAuthMsSaveBbCipherUpdateForUser(&contextCopy, data, size);
	copy = sceKernelCopyToUser(context, &contextCopy, sizeof(contextCopy));
	if (copy < 0)
		return copy;
	return result;
}

int _sceSblGcAuthMgrMsSaveBBCipherFinal(GcAuthMsSaveBbCipherContext *context)
{
	GcAuthMsSaveBbCipherContext contextCopy;
	int result;
	int copy;

	if (context == NULL)
		return GcAuthMsSaveBbCipherFinalForUser(NULL);
	__builtin_memset(&contextCopy, 0, sizeof(contextCopy));
	result = sceKernelCopyFromUser(&contextCopy, context, sizeof(contextCopy));
	if (result < 0)
		return result;
	result = GcAuthMsSaveBbCipherFinalForUser(&contextCopy);
	copy = sceKernelCopyToUser(context, &contextCopy, sizeof(contextCopy));
	if (copy < 0)
		return copy;
	return result;
}

int _sceSblGcAuthMgrPsmactCreateC1(unsigned char mode,
		GcAuthPsmactChallenge *output, GcAuthPsmactRandomRecord *random,
		const SceSize *sizes)
{
	GcAuthPsmactRandomRecord *randomCopy;
	SceSize size[2];
	int result;
	int copy;

	result = sceKernelCopyFromUser(size, sizes, sizeof(size));
	if (result < 0)
		return result;
	if (random == NULL)
		return GcAuthPsmactBeginActivationWithStack(mode, output, NULL);
	if (size[0] > sizeof(*randomCopy))
		return SCE_KERNEL_ERROR_NO_MEMORY;
	randomCopy = __builtin_alloca(sizeof(*randomCopy));
	memset(randomCopy, 0, sizeof(*randomCopy));
	result = GcAuthPsmactBeginActivationWithStack(mode, output, randomCopy);
	copy = sceKernelCopyToUser(random, randomCopy, size[0]);
	if (copy < 0)
		return copy;
	return result;
}

int _sceSblGcAuthMgrPsmactVerifyR1(const GcAuthPsmactActivation *activation,
		const GcAuthPsmactRandomRecord *random,
		GcAuthPsmactEncryptedActivation *encryptedActivation, const SceSize *sizes)
{
	GcAuthPsmactRandomRecord *randomCopy;
	SceSize size[2];
	int result;

	result = sceKernelCopyFromUser(size, sizes, sizeof(size));
	if (result < 0)
		return result;
	if (random != NULL) {
		if (size[0] > sizeof(*randomCopy))
			return SCE_KERNEL_ERROR_NO_MEMORY;
		randomCopy = __builtin_alloca(sizeof(*randomCopy));
		result = sceKernelCopyFromUser(randomCopy, random, size[0]);
		if (result < 0)
			return result;
		random = randomCopy;
	}
	return GcAuthPsmactApplyActivationWithStack(activation, random,
		encryptedActivation);
}
