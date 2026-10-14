/* Serialized ECC verification input. */
#include "adhoc.h"

extern GcAuthAdhocVerifyScratch gcAuthAdhocVerifyScratch;

int GcAuthExecuteKirkCommand(void *output, const void *input, int command,
		size_t size, int keyId);

int GcAuthAdhocVerify160(const GcAuthAdhocPublicKey160 *publicKey,
		const unsigned char *digest, const GcAuthAdhocSignature160 *signature)
{
	GcAuthAdhocVerify160Input *input = &gcAuthAdhocVerifyScratch.verify160;
	const unsigned char *signatureBytes = (const unsigned char *)signature;
	unsigned char *outputSignature = (unsigned char *)&input->signature;
	unsigned int index;

	for (index = 0; index != 40; ++index)
		input->publicKey.serialized[index] = publicKey->serialized[index];
	for (index = 0; index != 20; ++index)
		input->digest[index] = digest[index];
	for (index = 0; index != 40; ++index)
		outputSignature[index] = signatureBytes[index];
	if (GcAuthExecuteKirkCommand(NULL, input, 17, 100, 0) != 0)
		return (int)0x808a010d;
	return 0;
}
