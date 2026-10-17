/* Password derivation family; the original records no source filename. */
#include <stdint.h>
#include <string.h>
#include <psp2kern/kernel/debug.h>
#include <psp2kern/kernel/utils.h>

/* Sony's 4.00 password copies use the checked object-size interface. */
#if FW_VERSION >= 0x04000000
#include <sysmem/fortify.h>
#endif

/* Pinned upstream PBKDF2 loop with the separately recorded Sony adaptation. */
#define SCE_WLANBT_PBKDF2 1
#define SHA1_DIGEST_SIZE 20
#define strlen(password) strnlen(password, 64)
#define HMAC_SHA1(key, keySize, input, inputSize, output, outputSize) \
	ksceHmacSha1Digest(key, keySize, input, inputSize, output)
#include "cmm_wpa_sce.c"
#undef HMAC_SHA1
#undef strlen
#undef SHA1_DIGEST_SIZE
#undef SCE_WLANBT_PBKDF2

int wlanDerivePasswordKey(const char *password, const void *salt, int saltSize,
			  uint8_t *output)
{
	char copy[64];
	size_t length = strnlen(password, 63);

	memcpy(copy, password, length);
	copy[length] = '\0';
	if (strnlen(copy, 64) <= 63 && saltSize <= 32) {
		wlanPbkdf2Sha1Block(copy, salt, saltSize, 1, output);
		wlanPbkdf2Sha1Block(copy, salt, saltSize, 2, output + 20);
		return 1;
	}
	return 0;
}
