#ifndef SCE_MSIF_ECDSA_H
#define SCE_MSIF_ECDSA_H

#include <stdint.h>

struct MsifEcdsaSignature {
	const uint8_t *r;
	const uint8_t *s;
};

struct MsifEcdsaPoint {
	const uint8_t *x;
	const uint8_t *y;
};

struct MsifEcdsaCurve {
	const uint8_t *prime;
	const uint8_t *coefficientA;
	const uint8_t *coefficientB;
	const uint8_t *order;
	const uint8_t *baseX;
	const uint8_t *baseY;
};

struct MsifEcdsaWordPoint {
	uint32_t *x;
	uint32_t *y;
	uint32_t *z;
};

int MsifEcdsaVerifyCore(const struct MsifEcdsaSignature *signature,
	const uint8_t *digest, const struct MsifEcdsaPoint *publicKey,
	const struct MsifEcdsaCurve *curve, int words, int bytes);
int MsifEcdsaVerify224(const struct MsifEcdsaSignature *signature,
	const uint8_t *digest, const struct MsifEcdsaPoint *publicKey,
	const struct MsifEcdsaCurve *curve);

void MsifIntegerImport(uint32_t *destination, const uint8_t *source, int bytes);
void MsifIntegerRemainder(uint32_t *destination, const uint32_t *source,
	int sourceWords, const uint32_t *modulus, int modulusWords);
void MsifIntegerShiftLeft(uint32_t *destination, const uint32_t *source,
	int words, unsigned int shift);
void MsifIntegerShiftRight(uint32_t *destination, const uint32_t *source,
	int words, unsigned int shift);
int MsifIntegerInRange(const uint32_t *value, const uint32_t *limit, int words);
int MsifIntegerInverse(uint32_t *destination, const uint32_t *value,
	const uint32_t *modulus, int words);
uint32_t MsifIntegerAdd(uint32_t *destination, const uint32_t *left,
	const uint32_t *right, int words, uint32_t carry);
uint32_t MsifIntegerSubtract(uint32_t *destination, const uint32_t *left,
	const uint32_t *right, int words, uint32_t borrow);
int MsifIntegerGreaterThan(const uint32_t *left, const uint32_t *right,
	int words);
void MsifIntegerMultiply(uint32_t *destination, const uint32_t *left,
	int leftWords, const uint32_t *right, int rightWords);
uint32_t *MsifIntegerMultiplyWord(uint32_t *destination, const uint32_t *value,
	int words, uint32_t multiplier);
void MsifEcdsaMultiplyAdd(struct MsifEcdsaWordPoint *destination,
	const struct MsifEcdsaWordPoint *base,
	const struct MsifEcdsaWordPoint *publicKey, const uint32_t *baseScalar,
	const uint32_t *publicScalar, const uint32_t *prime,
	const uint32_t *coefficientA, int words);

void MsifEcdsaDoubleMinusThree(struct MsifEcdsaWordPoint *point,
	const uint32_t *coefficientA, const uint32_t *prime, int words,
	uint32_t montgomeryInverse);
void MsifEcdsaDouble(struct MsifEcdsaWordPoint *point,
	const uint32_t *coefficientA, const uint32_t *prime, int words,
	uint32_t montgomeryInverse);
void MsifEcdsaPrepare(struct MsifEcdsaWordPoint *convertedBase,
	struct MsifEcdsaWordPoint *convertedPublic, uint32_t *convertedA,
	const struct MsifEcdsaWordPoint *base,
	const struct MsifEcdsaWordPoint *publicKey, const uint32_t *coefficientA,
	const uint32_t *prime, int words, uint32_t montgomeryInverse);
void MsifEcdsaPointAdd(struct MsifEcdsaWordPoint *destination,
	const struct MsifEcdsaWordPoint *source, const uint32_t *coefficientA,
	const uint32_t *prime, int words, uint32_t montgomeryInverse);
void MsifEcdsaToAffine(struct MsifEcdsaWordPoint *destination,
	const struct MsifEcdsaWordPoint *source, const uint32_t *prime,
	int words, uint32_t montgomeryInverse);
void MsifMontgomeryMultiply(uint32_t *destination, const uint32_t *left,
	const uint32_t *right, const uint32_t *prime, int words,
	uint32_t montgomeryInverse);
uint32_t *MsifModularAdd(uint32_t *destination, const uint32_t *left,
	const uint32_t *right, const uint32_t *prime, int words);
uint32_t *MsifModularSubtract(uint32_t *destination, const uint32_t *left,
	const uint32_t *right, const uint32_t *prime, int words);
void MsifEcdsaToMontgomery(struct MsifEcdsaWordPoint *destination,
	const struct MsifEcdsaWordPoint *source, const uint32_t *radixSquared,
	const uint32_t *prime, int words, uint32_t montgomeryInverse);
void MsifMontgomeryReduce(uint32_t *destination, const uint32_t *source,
	int sourceWords, const uint32_t *prime, int words,
	uint32_t montgomeryInverse);

#endif
