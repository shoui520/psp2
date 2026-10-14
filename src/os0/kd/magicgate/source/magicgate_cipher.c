/* Descriptive file name: the original has no source-file debug records. */
#include "magicgate_private.h"

uint64_t sessionKey;

int cipherBuffer(void *output, const void *input, void *iv, unsigned int size,
		 int slot, const void *key, int mode)
{
	int result;

	if (slot >= 0)
		key = &slotKeys[slot];
	else if (slot == -2)
		key = &sessionKey;
	else if (!key) {
		result = (int)0x80501000;
		goto out;
	}
	if (mode == 2)
		result = calculateDesMac(key, input, output, size, iv);
	else if (mode == 1) {
		if (iv)
			result = decryptDesCbc(key, input, output, size, iv);
		else
			result = decryptDesEcb(key, input, output, size);
	} else if (iv)
		result = encryptDesCbc(key, input, output, size, iv);
	else
		result = encryptDesEcb(key, input, output, size);
out:
	SceMagicGateForDriver_492EE950();
	return result;
}
