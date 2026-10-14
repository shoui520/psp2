/* Serialized signature verification request. */
#include <psp2kern/kernel/threadmgr.h>
#include "adhoc.h"

extern SceUID gcAuthAdhocSemaphore;
extern const GcAuthAdhocPublicKey160 gcAuthAdhocPublicKey160Key0;
extern const GcAuthAdhocPublicKey160 gcAuthAdhocPublicKey160Fixed;
int GcAuthAdhocVerify160(const GcAuthAdhocPublicKey160 *publicKey,
		const unsigned char *digest, const GcAuthAdhocSignature160 *signature);

int GcAuthAdhocVerifyRequest160(const GcAuthAdhocSign160Request *request)
{
	int result;

	if (request->keyId != 0)
		return (int)0x808a040a;
	if (ksceKernelWaitSema(gcAuthAdhocSemaphore, 1, NULL) != 0)
		return (int)0x808a0008;
	result = (int)0x808a040d;
	if (GcAuthAdhocVerify160(&gcAuthAdhocPublicKey160Key0, request->digest,
			request->signature) == 0)
		result = 0;
	if (ksceKernelSignalSema(gcAuthAdhocSemaphore, 1) != 0)
		result = (int)0x808a0008;
	return result;
}

int GcAuthAdhocVerifyFixed160(const GcAuthAdhocVerifyPair160 *request)
{
	const unsigned char *digest = request->digest;
	const GcAuthAdhocSignature160 *signature = request->signature;
	int result;

	if (ksceKernelWaitSema(gcAuthAdhocSemaphore, 1, NULL) != 0)
		return (int)0x808a0008;
	if (digest == NULL || signature == NULL ||
			GcAuthAdhocVerify160(&gcAuthAdhocPublicKey160Fixed,
				digest, signature) != 0)
		result = (int)0x808a000c;
	else
		result = 0;
	if (ksceKernelSignalSema(gcAuthAdhocSemaphore, 1) != 0)
		result = (int)0x808a0008;
	return result;
}
