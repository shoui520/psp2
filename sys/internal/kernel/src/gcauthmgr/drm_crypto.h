#pragma once

#include <sysmem/aes.h>
#include <marlin_hci/rsa.h>

struct GcAuthAesCbcState {
	SceAesFullContext aes;
	unsigned char chain[32];
};

_Static_assert(sizeof(struct GcAuthAesCbcState) == 1008, "AES CBC state ABI");

/* drm_cipher.c */
int GcAuthInitializeDrmBb(void);

/* drm_crypto.c */
int GcAuthAesCbc(unsigned char *data, size_t size, const void *key,
		const unsigned char *iv, int decrypt);
int GcAuthDrmEncrypt(unsigned char *data, size_t size, int keyId);
int GcAuthDrmDecrypt(unsigned char *data, size_t size, int keyId);
int GcAuthDrmEncryptPrivate(unsigned char *data, size_t size);
int GcAuthDrmDecryptPrivate(unsigned char *data, size_t size);
int GcAuthDrmEncryptCbc(unsigned char *data, size_t size, unsigned char *iv, int keyId);
int GcAuthDrmDecryptCbc(unsigned char *data, size_t size, unsigned char *iv, int keyId);
int GcAuthGenerateRandom20(unsigned char *data);

/* drm_helpers.c */
int GcAuthAesEncrypt(unsigned char *data, size_t size, const void *key, const unsigned char *iv);
int GcAuthAesDecrypt(unsigned char *data, size_t size, const void *key, const unsigned char *iv);
int GcAuthFillRandomBytes(unsigned char *data, int size);

/* rsa_signature.c */
unsigned int GcAuthRsaSign2048(MarlinRsaData *output,
			     const MarlinRsaData *digest,
			     const struct MarlinRsaPrivateKey *key, int algorithm);
int GcAuthRsaVerify2048(const MarlinRsaData *signature,
		      const MarlinRsaData *digest,
		      const MarlinRsaExponentKey *key, int algorithm);
