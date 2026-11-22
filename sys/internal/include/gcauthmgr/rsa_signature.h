/*
 * GcAuthMgr's RSA signatures: PKCS#1 v1.5 blocks signed with a private key (rsa_signature.c) and
 * checked with a public one (rsa_verify.c), their DigestInfo encoding (rsa_digest.c), over the
 * Marlin integer routines. SceDrmPsmKdc links the check for its revocation list, SceLibMarlin
 * the check and the encoding. The names are ours: no record names them.
 */
#pragma once

#include <marlin_hci/rsa.h>

unsigned int GcAuthRsaSign2048(MarlinRsaData *output,
			     const MarlinRsaData *digest,
			     const struct MarlinRsaPrivateKey *key, int algorithm);
int GcAuthRsaVerify2048(const MarlinRsaData *signature,
		      const MarlinRsaData *digest,
		      const MarlinRsaExponentKey *key, int algorithm);

/* rsa_digest.c */
int GcAuthRsaEncodeDigest(unsigned char *output, int size,
			const MarlinRsaData *digest, int algorithm);
