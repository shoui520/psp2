/* Serialized signature verification request. */
#include <threadmgr/threadmgr.h>
#include "adhoc.h"

extern SceUID gcAuthAdhocSemaphore;
extern const GcAuthAdhocPublicKey224 gcAuthAdhocPublicKey224Key1;
extern const GcAuthAdhocPublicKey224 gcAuthAdhocPublicKey224Key0;
extern const GcAuthAdhocPublicKey224 gcAuthAdhocPublicKey224Key1000;

int GcAuthAdhocVerifyRequest224(const GcAuthAdhocSign224Request *request)
{
	const GcAuthAdhocPublicKey224 *publicKey;
	int result;

	switch (request->keyId) {
	case 1:
		publicKey = &gcAuthAdhocPublicKey224Key1;
		break;
	case 0:
		publicKey = &gcAuthAdhocPublicKey224Key0;
		break;
	case 1000:
		publicKey = &gcAuthAdhocPublicKey224Key1000;
		break;
	default:
		return (int)0x808a040a;
	}
	if (sceKernelWaitSema(gcAuthAdhocSemaphore, 1, NULL) != 0)
		return (int)0x808a0008;
	result = (int)0x808a040d;
	if (GcAuthAdhocVerify224(publicKey, request->digest,
			request->signature) == 0)
		result = 0;
	if (sceKernelSignalSema(gcAuthAdhocSemaphore, 1) != 0)
		return (int)0x808a0008;
	return result;
}
