/*
 * sw_ecc.h — internal SHA-1, FIPS 186-2 PRNG, and prime-field ECC/ECDSA/ECDH
 * routines linked into DTCPIP-SW.
 */
#ifndef DTCPIP_SW_ECC_H
#define DTCPIP_SW_ECC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct SwSha1Ctx {
	uint32_t state[5];
	uint32_t blocksHi;
	uint32_t blocksLo;
	uint32_t buffered;
	unsigned char buf[64];
};

struct SwEccPoint {
	unsigned char *x;
	unsigned char *y;
};

struct SwEccCurve {
	unsigned char *prime;
	unsigned char *coeffA;
	unsigned char *coeffB;
	unsigned char *order;
	unsigned char *genX;
	unsigned char *genY;
};

/* sw_prng.c */
int swSha1Compress(uint32_t state[5], const unsigned char block[64]);
int swSha1Final(unsigned char out[20], struct SwSha1Ctx *ctx);
int swSha1Digest(unsigned char out[20], const unsigned char *data, int len);
uint32_t *swPrngAddMod2_160(const uint32_t a[5], const uint32_t b[5], uint32_t out[5]);
int swPrngFips186(unsigned char out[20], unsigned char xkey[20], const unsigned char xseed[20]);

/* sw_ecc.c */
int swEccScalarMulBase(struct SwEccPoint *out, const unsigned char *scalar,
		       const struct SwEccCurve *curve);
int swEccScalarMulBase224(struct SwEccPoint *out, const unsigned char *scalar,
			  const struct SwEccCurve *curve);
int swEccScalarMulPoint(struct SwEccPoint *out, const unsigned char *scalar,
			const struct SwEccPoint *point, const struct SwEccCurve *curve);
int swEccScalarMulPoint224(struct SwEccPoint *out, const unsigned char *scalar,
			   const struct SwEccPoint *point, const struct SwEccCurve *curve);
int swEccEcdsaSign(struct SwEccPoint *sig, const unsigned char *hash,
		   const unsigned char *nonce, const unsigned char *privKey,
		   const struct SwEccCurve *curve);
int swEccEcdsaSign224(struct SwEccPoint *sig, const unsigned char *hash,
		      const unsigned char *nonce, const unsigned char *privKey,
		      const struct SwEccCurve *curve);
int swEccEcdsaVerify(const struct SwEccPoint *sig, const unsigned char *hash,
		     const struct SwEccPoint *pubKey, const struct SwEccCurve *curve);
int swEccEcdsaVerify224(const struct SwEccPoint *sig, const unsigned char *hash,
			const struct SwEccPoint *pubKey, const struct SwEccCurve *curve);

#ifdef __cplusplus
}
#endif

#endif
