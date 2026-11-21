/*
 * GcAuthMgr's RSA signatures (rsa_signature.c): PKCS#1 v1.5 blocks signed with a private key and
 * checked with a public one, over the Marlin integer routines. SceDrmPsmKdc links the same file
 * to check its revocation list. The names are ours: no record names them.
 */
#pragma once

#include <marlin_hci/rsa.h>

unsigned int GcAuthRsaSign2048(MarlinRsaData *output,
			     const MarlinRsaData *digest,
			     const struct MarlinRsaPrivateKey *key, int algorithm);
int GcAuthRsaVerify2048(const MarlinRsaData *signature,
		      const MarlinRsaData *digest,
		      const MarlinRsaExponentKey *key, int algorithm);
