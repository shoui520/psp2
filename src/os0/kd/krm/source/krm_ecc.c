#include "krm_internal.h"

typedef uint32_t BinaryFieldVector
	__attribute__((vector_size(16), aligned(4), may_alias));

int KrmEccInitializeAllocator(KrmEccContext *ctx);
int KrmEccAllocate(KrmEccContext *ctx, unsigned int size, void **result);
int KrmEccAllocateAligned(KrmEccContext *ctx, unsigned int size,
			  unsigned int alignment, void **allocation, void **result);
int KrmEccRelease(KrmEccContext *ctx, void *ptr);

int KrmEccInitialize(KrmEccContext *ctx)
{
	return KrmEccInitializeAllocator(ctx);
}

__attribute__((noinline)) int KrmEccInitializeAllocator(KrmEccContext *ctx)
{
	typedef unsigned int AllocationVector
		__attribute__((vector_size(8), aligned(4), may_alias));

	if (!ctx)
		return (int)0x80070057;
	ctx->base = ctx->workspace;
	*(AllocationVector *)&ctx->allocation = (AllocationVector){0, 0x8800};
	return 0;
}

int KrmEccRandomBytes(void *argument, unsigned char *dst, unsigned int size);

__attribute__((noinline)) int KrmEccRandom(unsigned char *dst, unsigned int size, KrmEccContext *ctx)
{
	void *argument = ctx;

	if (ctx)
		argument = ctx->randomArgument;

	return KrmEccRandomBytes(argument, dst, size) >= 0;
}

__attribute__((noinline)) void *KrmEccMalloc(unsigned int size, KrmEccContext *ctx)
{
	void *result = NULL;

	if (KrmEccAllocate(ctx, size, &result) < 0)
		return NULL;
	return result;
}

__attribute__((noinline)) void *KrmEccMemalign(unsigned int size, unsigned int alignment,
	KrmEccContext *ctx, void **allocation)
{
	void *result = NULL;

	if (KrmEccAllocateAligned(ctx, size, alignment, allocation, &result) < 0)
		return NULL;
	return result;
}

__attribute__((noinline)) intptr_t KrmEccFree(void *ptr, KrmEccContext *ctx)
{
	intptr_t result = (intptr_t)ptr;

	if (ctx && ptr)
		result = KrmEccRelease(ctx, ptr);
	return result;
}

int KrmEccInternalImportPoint(const unsigned char *src, uint32_t words[8]);
int KrmEccInternalValidatePoint(const KrmEccPublicKey *point, KrmEccContext *ctx);
int KrmEccInternalDecode(const KrmEccPrivateKey *privateKey,
	const KrmEccEncodedBlock *encoded, KrmEccPrivateKey *value, KrmEccContext *ctx);
int KrmEccInternalEncode(const KrmEccPublicKey *publicKey,
	const KrmEccPrivateKey *value, KrmEccEncodedBlock *encoded, KrmEccContext *ctx);
int KrmEccInternalGenerateKeys(KrmEccPublicKey *publicKey,
	KrmEccPrivateKey *privateKey, KrmEccContext *ctx);
int KrmEccInternalSignMessage(const void *src, SceSize size,
	const KrmEccPrivateKey *privateKey, KrmEccSignature *signature, KrmEccContext *ctx);
int KrmEccInternalVerifyMessage(const void *src, SceSize size,
	const KrmEccPublicKey *publicKey, const KrmEccSignature *signature, KrmEccContext *ctx);
int KrmEccInternalSignDigest(const void *digest, const KrmEccPrivateKey *privateKey,
	KrmEccSignature *signature, KrmEccContext *ctx);
int KrmEccInternalVerifyDigest(const void *digest, const KrmEccPublicKey *publicKey,
	const KrmEccSignature *signature, KrmEccContext *ctx);

int KrmEccImportPoint(const KrmEccPublicKey *publicKey, KrmEccPublicKey *point)
{
	return KrmEccInternalImportPoint((const unsigned char *)publicKey,
					point ? point->x : NULL);
}

int KrmEccValidatePoint(const KrmEccPublicKey *point, KrmEccContext *ctx)
{
	return KrmEccInternalValidatePoint(point, ctx);
}

int KrmEccDecode(const KrmEccPrivateKey *privateKey,
		 const KrmEccEncodedBlock *encoded, KrmEccPrivateKey *value,
		 KrmEccContext *ctx)
{
	return KrmEccInternalDecode(privateKey, encoded, value, ctx);
}

int KrmEccEncode(const KrmEccPublicKey *publicKey,
		 const KrmEccPrivateKey *value, KrmEccEncodedBlock *encoded,
		 KrmEccContext *ctx)
{
	return KrmEccInternalEncode(publicKey, value, encoded, ctx);
}

int KrmEccGenerateKeys(KrmEccPublicKey *publicKey,
		       KrmEccPrivateKey *privateKey, KrmEccContext *ctx)
{
	return KrmEccInternalGenerateKeys(publicKey, privateKey, ctx);
}

int KrmEccSignMessage(const void *src, SceSize size,
		     const KrmEccPrivateKey *privateKey,
		     KrmEccSignature *signature, KrmEccContext *ctx)
{
	return KrmEccInternalSignMessage(src, size, privateKey, signature, ctx);
}

int KrmEccVerifyMessage(const void *src, SceSize size,
		       const KrmEccPublicKey *publicKey,
		       const KrmEccSignature *signature, KrmEccContext *ctx)
{
	return KrmEccInternalVerifyMessage(src, size, publicKey, signature, ctx);
}

int KrmEccSignDigest(const void *digest, const KrmEccPrivateKey *privateKey,
		    KrmEccSignature *signature, KrmEccContext *ctx)
{
	return KrmEccInternalSignDigest(digest, privateKey, signature, ctx);
}

int KrmEccVerifyDigest(const void *digest, const KrmEccPublicKey *publicKey,
		      const KrmEccSignature *signature, KrmEccContext *ctx)
{
	return KrmEccInternalVerifyDigest(digest, publicKey, signature, ctx);
}

int KrmEccImportWords(const unsigned char *src, uint32_t words[8])
{
	if (!src || !words)
		return (int)0x80070057;
	__builtin_memset(words, 0, 32);
#define IMPORT_WORD(index) do { \
	words[index] ^= src[31 - (index) * 4]; \
	words[index] ^= (unsigned int)src[30 - (index) * 4] << 8; \
	words[index] ^= (unsigned int)src[29 - (index) * 4] << 16; \
	words[index] ^= (unsigned int)src[28 - (index) * 4] << 24; \
} while (0)
	IMPORT_WORD(0);
	IMPORT_WORD(1);
	IMPORT_WORD(2);
	IMPORT_WORD(3);
	IMPORT_WORD(4);
	IMPORT_WORD(5);
	IMPORT_WORD(6);
	IMPORT_WORD(7);
#undef IMPORT_WORD
	return 0;
}

int KrmEccExportWords(const uint32_t *words, unsigned char *dst, unsigned int bits)
{
	unsigned int wordCount = (bits + 31) >> 5;
	unsigned int byteCount = (bits + 7) >> 3;
	unsigned int offset;
	int count;
	uint32_t value;
	unsigned char *end;
	unsigned char *limit;

	if (__builtin_expect(!words, 0))
		return (int)0x80070057;
	if (__builtin_expect(!dst, 0))
		return (int)0x80070057;
	if (__builtin_expect(!bits, 0))
		return (int)0x80070057;
	offset = 0;
	if (wordCount == 0)
		return 0;
	limit = dst + byteCount;
	do {
		value = words[offset / 4];
		count = byteCount - offset;
		if (byteCount - offset >= 4)
			count = 4;
		end = limit - offset;
		if (count) {
			unsigned char *stop = end - count;

			do {
				*--end = value;
				value >>= 8;
			} while (end != stop);
		}
		offset += 4;
	} while (offset != wordCount * 4);
	return 0;
}

int KrmEccInternalImportPoint(const unsigned char *src, uint32_t words[8])
{
	int ret;

	if (__builtin_expect(!words, 0))
		return (int)0x80070057;
	if (__builtin_expect(!src, 0))
		return (int)0x80070057;
	ret = KrmEccImportWords(src, words);
	return (ret >> 31) & (int)0x80040e87;
}

typedef struct KrmEccReciprocal {
	uint32_t factor;
	unsigned int shift;
} KrmEccReciprocal;

typedef struct KrmEccModulus {
	unsigned int wordCount;
	unsigned int wordBytes;
	unsigned int productWords;
	unsigned int radixBits;
	unsigned int montgomeryEnabled;
	KrmEccReciprocal quotientReciprocal;
	uint32_t montgomeryInverse;
	const uint32_t *modulus;
	const uint32_t *reciprocal;
	const uint32_t *highWords;
	const uint32_t *one;
	int (*multiply)(const uint32_t *, const uint32_t *, uint32_t *,
		const struct KrmEccModulus *, void *);
} KrmEccModulus;

typedef struct KrmEccFieldOperations KrmEccFieldOperations;

/* Inferred complete descriptors at the curve's field and modulus pointers. */
typedef struct KrmEccField {
	unsigned int wordCount;
	unsigned int elementCount;
	unsigned int workspaceWords;
	unsigned int productWords;
	unsigned int multiplyWorkspaceWords;
	unsigned int flags;
	void *operationArgument;
	const uint32_t *one;
	const uint32_t *zero;
	const KrmEccFieldOperations *operations;
	const struct KrmEccField *extension;
	void *extensionArgument;
	const KrmEccModulus *modulus;
	const uint32_t *reductionCoefficients;
} KrmEccField;

/* Field dispatch layout inferred from the module-owned operation table. */
struct KrmEccFieldOperations {
	int (*add)(const uint32_t *, const uint32_t *, uint32_t *, unsigned int,
		const KrmEccField *, KrmEccContext *);
	int (*equal)(const uint32_t *, const uint32_t *, unsigned int,
		const KrmEccField *, KrmEccContext *);
	int (*release)(KrmEccField *, KrmEccContext *);
	int (*setIntegers)(const int32_t *, uint32_t *, unsigned int,
		const KrmEccField *, KrmEccContext *);
	int (*inverse)(const uint32_t *, uint32_t *, const KrmEccField *,
		void *, KrmEccContext *);
	int (*isZero)(const uint32_t *, unsigned int, const KrmEccField *, KrmEccContext *);
	int (*shift)(const uint32_t *, int, uint32_t *, unsigned int,
		const KrmEccField *, KrmEccContext *);
	int (*multiply)(const uint32_t *, const uint32_t *, uint32_t *,
		const KrmEccField *, void *, KrmEccContext *);
	int (*negate)(const uint32_t *, uint32_t *, unsigned int,
		const KrmEccField *, KrmEccContext *);
	int (*copyModulus)(uint32_t *, const KrmEccField *);
	int (*subtract)(const uint32_t *, const uint32_t *, uint32_t *, unsigned int,
		const KrmEccField *, KrmEccContext *);
	int (*zero)(uint32_t *, unsigned int, const KrmEccField *, KrmEccContext *);
};

typedef struct KrmEccCurve {
	const KrmEccField *field;
	const uint32_t *a;
	const uint32_t *b;
	const KrmEccPublicKey *generator;
	const uint32_t *order;
	/* PLACEHOLDER: ancillary curve hooks are zero in this module's curve. */
	void *operationArgument;
	int (*multiplyPoint)(const KrmEccPublicKey *point, const uint32_t *scalar,
		unsigned int wordCount, KrmEccPublicKey *result,
		const struct KrmEccCurve *curve, KrmEccContext *ctx);
	void *auxiliaryOperation;
	int infinityUsesOne;
	unsigned int workspaceWords;
} KrmEccCurve;

extern const KrmEccPublicKey g_krmEccGenerator;
extern const KrmEccModulus g_krmEccModulus;
extern const KrmEccField g_krmEccField;
extern const KrmEccCurve g_krmEccCurve;
extern const uint32_t g_krmEccCurveA[8];
extern const uint32_t g_krmEccCurveB[8];
extern const uint32_t g_krmEccOrder[8];
extern const uint32_t g_krmEccHighWords[4];
extern const uint32_t g_krmEccReductionCoefficients[8];
extern const uint32_t g_krmEccReciprocalWords[8];
extern const uint32_t g_krmEccPrime[8];
extern const uint32_t g_krmEccOne[8];
extern const KrmEccFieldOperations g_krmEccOperations;

int KrmEccReduce(const uint32_t *value, unsigned int valueWords,
		 const uint32_t *modulus, unsigned int modulusWords,
		 const uint32_t *reciprocal, uint32_t *quotient, uint32_t *remainder);

int KrmEccReduceScalar(const unsigned char *src, const KrmEccCurve *curve,
		       uint32_t result[8])
{
	uint32_t value[8] = {0};
	int ret = (int)0x80070057;

	if (__builtin_expect(!src, 0))
		return ret;
	if (__builtin_expect(!curve, 0))
		return ret;
	if (__builtin_expect(!result, 0))
		return ret;
	if (__builtin_expect(KrmEccImportWords(src, value) < 0, 0))
		ret = (int)0x80040e87;
	else
		ret = KrmEccReduce(value, 8, curve->order, 8, NULL, NULL, result) ?
			0 : (int)0x80040e87;
	return ret;
}

int KrmEccConvertInteger(const uint32_t *value, unsigned int valueWords,
	uint32_t *result, const KrmEccModulus *modulus, KrmEccContext *ctx);

int KrmEccImportScalar(const unsigned char *src, const KrmEccCurve *curve,
		       uint32_t result[8], KrmEccContext *ctx)
{
	uint32_t value[8] = {0};
	int ret = (int)0x80070057;

	if (__builtin_expect(!src, 0))
		goto done;
	if (__builtin_expect(!curve, 0))
		goto done;
	if (__builtin_expect(!result, 0))
		goto done;
	if (__builtin_expect(!ctx, 0))
		goto done;
	if (__builtin_expect(!curve->field, 0))
		goto done;
	if (__builtin_expect(!curve->field->modulus, 0))
		goto done;
	if (curve->field->modulus->wordCount != 8)
		goto done;
	ret = KrmEccInternalImportPoint(src, value);
	if (ret >= 0 && !KrmEccConvertInteger(value, 8, result,
					     curve->field->modulus, ctx))
		ret = (int)0x80040e87;
done:
	return ret;
}


int KrmEccImportCoordinates(const KrmEccPublicKey *publicKey,
	const KrmEccCurve *curve, KrmEccPublicKey *point, KrmEccContext *ctx)
{
	uint32_t value[8] = {0};
	int ret = (int)0x80070057;

	if (__builtin_expect(!publicKey, 0))
		goto done;
	if (__builtin_expect(!curve, 0))
		goto done;
	if (__builtin_expect(!point, 0))
		goto done;
	if (__builtin_expect(!ctx, 0))
		goto done;
	if (__builtin_expect(!curve->field, 0))
		goto done;
	if (__builtin_expect(!curve->field->modulus, 0))
		goto done;
	if (__builtin_expect(curve->field->modulus->wordCount != 8, 0))
		goto done;
	ret = KrmEccInternalImportPoint((const unsigned char *)publicKey->x, value);
	if (ret >= 0) {
		if (!KrmEccConvertInteger(value, 8, point->x,
					 curve->field->modulus, ctx)) {
			ret = (int)0x80040e87;
			goto done;
		}
		ret = KrmEccInternalImportPoint((const unsigned char *)publicKey->y, value);
		if (ret >= 0 && !KrmEccConvertInteger(value, 8, point->y,
						     curve->field->modulus, ctx))
			ret = (int)0x80040e87;
	}
done:
	return ret;
}

int KrmEccExportInteger(const uint32_t *words, unsigned char *dst)
{
	int ret;

	if (__builtin_expect(!words, 0))
		return (int)0x80070057;
	if (__builtin_expect(!dst, 0))
		return (int)0x80070057;
	ret = KrmEccExportWords(words, dst, 256);
	return (ret >> 31) & (int)0x80040e87;
}

int KrmEccConvertFromMontgomery(const uint32_t *value, uint32_t *result,
			      const KrmEccModulus *modulus);

int KrmEccNormalizeScalar(const uint32_t *src, const KrmEccCurve *curve,
			  uint32_t result[8])
{
	uint32_t value[8] = {0};

	if (__builtin_expect(!src, 0))
		return (int)0x80070057;
	if (__builtin_expect(!curve, 0))
		return (int)0x80070057;
	if (__builtin_expect(!result, 0))
		return (int)0x80070057;
	if (__builtin_expect(!curve->field, 0))
		return (int)0x80070057;
	if (__builtin_expect(!curve->field->modulus, 0))
		return (int)0x80070057;
	if (__builtin_expect(curve->field->modulus->wordCount != 8, 0))
		return (int)0x80070057;
	if (!KrmEccConvertFromMontgomery(src, value, curve->field->modulus))
		return (int)0x80040e87;
	if (KrmEccReduce(value, 8, curve->order, 8, NULL, NULL, result))
		return 0;
	return (int)0x80040e87;
}

int KrmEccExportNormalizedScalar(const uint32_t *src, unsigned char *dst)
{
	uint32_t value[8] = {0};
	int ret;

	if (__builtin_expect(!src, 0) || __builtin_expect(!dst, 0))
		return (int)0x80070057;
	ret = KrmEccNormalizeScalar(src, &g_krmEccCurve, value);
	if (ret >= 0)
		ret = KrmEccExportInteger(value, dst);
	return ret;
}

int KrmEccMultiplyAdd(const uint32_t *left, const uint32_t *right,
	const uint32_t *addend, uint32_t *result, const KrmEccField *field,
	void *workspace, KrmEccContext *ctx);
int KrmEccSquareRoot(const uint32_t *src, uint32_t *dst, unsigned int count,
	const KrmEccField *field, int *hasRoot, KrmEccContext *ctx);

int KrmEccRecoverY(const uint32_t x[8], uint32_t y[8],
		   void *workspace, KrmEccContext *ctx)
{
	int hasRoot = 0;

	if (__builtin_expect(!x, 0) || __builtin_expect(!y, 0) ||
	    __builtin_expect(!ctx, 0))
		return (int)0x80070057;
	uint32_t value[16];

	value[0] = 0;
	value[1] = 0;
	value[2] = 0;
	value[3] = 0;
	value[4] = 0;
	value[5] = 0;
	value[6] = 0;
	value[7] = 0;
	value[8] = 0;
	value[9] = 0;
	value[10] = 0;
	value[11] = 0;
	value[12] = 0;
	value[13] = 0;
	value[14] = 0;
	value[15] = 0;
	if (KrmEccMultiplyAdd(x, x, g_krmEccCurveA, value,
		&g_krmEccField, workspace, ctx) &&
	    KrmEccMultiplyAdd(x, value, g_krmEccCurveB, value,
		&g_krmEccField, workspace, ctx) &&
	    KrmEccSquareRoot(value, y, 1, &g_krmEccField, &hasRoot, ctx))
		return hasRoot ? 0 : (int)0x80040e89;
	return (int)0x80040e88;
}

int KrmEccImportCompressedPoint(const unsigned char *src, const KrmEccCurve *curve,
	void *workspace, KrmEccPublicKey *point, KrmEccContext *ctx)
{
	int ret;

	if (__builtin_expect(!src, 0) || __builtin_expect(!curve, 0) ||
	    __builtin_expect(!point, 0) || __builtin_expect(!ctx, 0))
		return (int)0x80070057;
	ret = KrmEccImportScalar(src, curve, point->x, ctx);
	if (ret >= 0)
		ret = KrmEccRecoverY(point->x, point->y, workspace, ctx);
	return ret;
}

int KrmEccExportCoordinates(const KrmEccPublicKey *point,
	const KrmEccCurve *curve, KrmEccPublicKey *publicKey)
{
	uint32_t value[8];
	int ret;

	if (__builtin_expect(!point, 0))
		return (int)0x80070057;
	if (__builtin_expect(!curve, 0))
		return (int)0x80070057;
	if (__builtin_expect(!publicKey, 0))
		return (int)0x80070057;
	if (__builtin_expect(!curve->field, 0))
		return (int)0x80070057;
	if (__builtin_expect(!curve->field->modulus, 0))
		return (int)0x80070057;
	if (__builtin_expect(curve->field->modulus->wordCount != 8, 0))
		return (int)0x80070057;
	if (!KrmEccConvertFromMontgomery(point->x, value, curve->field->modulus))
		return (int)0x80040e87;
	ret = KrmEccExportInteger(value, (unsigned char *)publicKey->x);
	if (ret < 0)
		return ret;
	if (!KrmEccConvertFromMontgomery(point->y, value, curve->field->modulus))
		return (int)0x80040e87;
	return KrmEccExportInteger(value, (unsigned char *)publicKey->y);
}

unsigned int KrmEccExportX(const KrmEccPublicKey *point,
	const KrmEccCurve *curve, unsigned char *dst)
{
	uint32_t value[8] = {0};

	if (__builtin_expect(!dst, 0))
		return 0x80070057;
	if (__builtin_expect(!point, 0))
		return 0x80070057;
	if (__builtin_expect(!curve, 0))
		return 0x80070057;
	if (__builtin_expect(!curve->field, 0))
		return 0x80070057;
	if (__builtin_expect(curve->field->flags != 1, 0))
		return 0x80070057;
	if (__builtin_expect(!curve->field->modulus, 0))
		return 0x80070057;
	if (__builtin_expect(curve->field->modulus->wordCount != 8, 0))
		return 0x80070057;
	if (__builtin_expect(!KrmEccConvertFromMontgomery(point->x, value,
						       curve->field->modulus), 0))
		return 0x80040e87;
	return KrmEccExportInteger(value, dst);
}

int KrmEccInternalValidatePoint(const KrmEccPublicKey *point, KrmEccContext *ctx)
{
	KrmEccPublicKey value;
	unsigned char workspace[632];
	int ret;
	unsigned char *clear = (unsigned char *)&value;
	unsigned int remaining = sizeof(workspace);
	unsigned char *end;

	end = clear + sizeof(value);
	do {
		*clear++ = 0;
	} while (clear != end);
	clear = workspace;
	do {
		clear[0] = 0;
		__builtin_prefetch(clear + 40);
		clear[1] = 0;
		clear[2] = 0;
		clear[3] = 0;
		clear[4] = 0;
		clear[5] = 0;
		clear[6] = 0;
		clear[7] = 0;
		clear[8] = 0;
		clear[9] = 0;
		clear[10] = 0;
		clear[11] = 0;
		clear[12] = 0;
		clear[13] = 0;
		clear[14] = 0;
		clear[15] = 0;
		clear[16] = 0;
		clear[17] = 0;
		clear[18] = 0;
		clear[19] = 0;
		clear[20] = 0;
		clear[21] = 0;
		clear[22] = 0;
		clear[23] = 0;
		clear[24] = 0;
		clear[25] = 0;
		clear[26] = 0;
		clear[27] = 0;
		clear[28] = 0;
		clear[29] = 0;
		clear[30] = 0;
		clear[31] = 0;
		clear += 32;
		remaining -= 32;
	} while (remaining >= 32);
	end = clear + remaining;
	do {
		*clear++ = 0;
	} while (clear != end);

	if (!point || !ctx)
		return (int)0x80070057;
	if (!KrmEccConvertInteger(point->x, 8, value.x,
		&g_krmEccModulus, NULL))
		return (int)0x80040e87;
	ret = KrmEccRecoverY(value.x, value.y, workspace, ctx);
	return ret == (int)0x80040e89 ? 1 : ret;
}

/* Names inferred from the point operations; no Sony identifiers were found. */
int KrmEccMultiplyPoint(const KrmEccPublicKey *point, const uint32_t *scalar,
	unsigned int wordCount, KrmEccPublicKey *result, const KrmEccCurve *curve,
	KrmEccContext *ctx);
int KrmEccAddPoints(const KrmEccPublicKey *left, const KrmEccPublicKey *right,
	KrmEccPublicKey *result, int sign, const KrmEccCurve *curve,
	void *workspace, KrmEccContext *ctx);

int KrmEccInternalDecode(const KrmEccPrivateKey *privateKey,
	const KrmEccEncodedBlock *encoded, KrmEccPrivateKey *value, KrmEccContext *ctx)
{
	unsigned char workspace[632];
	KrmEccPrivateKey scalar;
	KrmEccPublicKey first, second, result;
	unsigned char *wipe;
	int ret = (int)0x80070057;

	memset(workspace, 0, sizeof(workspace));
	__builtin_memset(&scalar, 0, sizeof(scalar));
	memset(&first, 0, sizeof(first));
	memset(&second, 0, sizeof(second));
	memset(&result, 0, sizeof(result));
	if (__builtin_expect(!privateKey, 0) || __builtin_expect(!encoded, 0) ||
	    __builtin_expect(!value, 0) || __builtin_expect(!ctx, 0))
		goto out;
	ret = KrmEccInitialize(ctx);
	if (ret < 0)
		goto out;
	ret = KrmEccInternalImportPoint((const unsigned char *)privateKey, scalar.scalar);
	if (ret < 0)
		goto out;
	ret = KrmEccImportCoordinates(&encoded->first, &g_krmEccCurve, &first, ctx);
	if (ret < 0)
		goto out;
	ret = KrmEccImportCoordinates(&encoded->second, &g_krmEccCurve, &second, ctx);
	if (ret < 0)
		goto out;
	if (!KrmEccMultiplyPoint(&first, scalar.scalar, 8, &result,
		&g_krmEccCurve, ctx) ||
	    !KrmEccAddPoints(&second, &result, &result, -1,
		&g_krmEccCurve, workspace, ctx)) {
		ret = (int)0x800480d5;
		goto out;
	}
	ret = KrmEccExportX(&result, &g_krmEccCurve, (unsigned char *)value);
out:
	wipe = (unsigned char *)&scalar;
	do
		*wipe++ = 0;
	while (wipe != (unsigned char *)(&scalar + 1));
	wipe = (unsigned char *)&result;
	do
		*wipe++ = 0;
	while (wipe != (unsigned char *)(&result + 1));
	return ret;
}

typedef struct KrmEccWorkspace {
	uint32_t *data;
	unsigned int wordCount;
	int allocated;
} KrmEccWorkspace;

int KrmEccEnsureWorkspace(KrmEccWorkspace *workspace, KrmEccContext *ctx);
int KrmEccIsPointOnCurve(const KrmEccPublicKey *point, const KrmEccCurve *curve,
	int flags, void *workspace, KrmEccContext *ctx);
int KrmEccIsPointAtInfinity(const KrmEccPublicKey *point, const KrmEccCurve *curve,
	KrmEccContext *ctx);
int KrmEccRandomScalar(const uint32_t *limit, uint32_t *scalar,
	unsigned int wordCount, KrmEccContext *ctx);

int KrmEccEncodePoint(const KrmEccPublicKey *publicKey,
	const KrmEccPublicKey *value, KrmEccEncodedBlock *encoded,
	void *workspace, KrmEccContext *ctx)
{
	KrmEccPrivateKey scalar;
	KrmEccPublicKey point, first, second;
	unsigned char *wipe;
	volatile unsigned char *erase;
	int ret = (int)0x80070057;

	if (__builtin_expect(!publicKey, 0) || __builtin_expect(!value, 0) ||
	    __builtin_expect(!encoded, 0) || __builtin_expect(!workspace, 0) ||
	    __builtin_expect(!ctx, 0))
		goto out;
	wipe = (unsigned char *)&scalar;
	do
		*wipe++ = 0;
	while (wipe != (unsigned char *)(&scalar + 1));
	ret = KrmEccImportCoordinates(publicKey, &g_krmEccCurve, &point, ctx);
	if (ret < 0)
		goto out;
	if (!KrmEccIsPointOnCurve(&point, &g_krmEccCurve, 0, workspace, ctx) ||
	    KrmEccIsPointAtInfinity(&point, &g_krmEccCurve, ctx) ||
	    !KrmEccRandomScalar(g_krmEccOrder, scalar.scalar, 8, ctx) ||
	    !KrmEccMultiplyPoint(&g_krmEccGenerator, scalar.scalar, 8, &first,
		&g_krmEccCurve, ctx) ||
	    !KrmEccMultiplyPoint(&point, scalar.scalar, 8, &second,
		&g_krmEccCurve, ctx) ||
	    !KrmEccAddPoints(value, &second, &second, 1, &g_krmEccCurve, workspace, ctx)) {
		ret = (int)0x800480d5;
		goto out;
	}
	ret = KrmEccExportCoordinates(&first, &g_krmEccCurve, &encoded->first);
	if (ret >= 0)
		ret = KrmEccExportCoordinates(&second, &g_krmEccCurve, &encoded->second);
out:
	erase = (volatile unsigned char *)&scalar;
	do
		*erase++ = 0;
	while (erase != (volatile unsigned char *)(&scalar + 1));
	return ret;
}

int KrmEccInternalEncode(const KrmEccPublicKey *publicKey,
	const KrmEccPrivateKey *value, KrmEccEncodedBlock *encoded, KrmEccContext *ctx)
{
	KrmEccPublicKey point;
	unsigned char workspace[632];
	unsigned char *wipe = (unsigned char *)&point;
	volatile unsigned char *erase;
	int ret = (int)0x80070057;

	do
		*wipe++ = 0;
	while (wipe != (unsigned char *)(&point + 1));
	if (__builtin_expect(!publicKey, 0) || __builtin_expect(!value, 0) ||
	    __builtin_expect(!encoded, 0) || __builtin_expect(!ctx, 0))
		goto out;
	ret = KrmEccInitialize(ctx);
	if (ret < 0)
		goto out;
	ret = KrmEccImportCompressedPoint((const unsigned char *)value,
		&g_krmEccCurve, workspace, &point, ctx);
	if (ret >= 0)
		ret = KrmEccEncodePoint(publicKey, &point, encoded, workspace, ctx);
out:
	erase = (volatile unsigned char *)&point;
	do
		*erase++ = 0;
	while (erase != (volatile unsigned char *)(&point + 1));
	return ret;
}

int KrmEccInternalGenerateKeys(KrmEccPublicKey *publicKey,
	KrmEccPrivateKey *privateKey, KrmEccContext *ctx)
{
	KrmEccPrivateKey scalar = {0};
	KrmEccPublicKey point;
	unsigned char workspace[632];
	int ret;

	memset(workspace, 0, sizeof(workspace));
	memset(&point, 0, sizeof(point));
	if (__builtin_expect(!privateKey, 0) || __builtin_expect(!publicKey, 0) ||
	    __builtin_expect(!ctx, 0))
		return (int)0x80070057;
	ret = KrmEccInitialize(ctx);
	if (ret < 0)
		return ret;
	if (!KrmEccRandomScalar(g_krmEccOrder, scalar.scalar, 8, ctx) ||
	    !KrmEccMultiplyPoint(&g_krmEccGenerator, scalar.scalar, 8, &point,
		&g_krmEccCurve, ctx) ||
	    !KrmEccIsPointOnCurve(&point, &g_krmEccCurve, 0, workspace, ctx))
		return (int)0x80040e88;
	ret = KrmEccExportInteger(scalar.scalar, (unsigned char *)privateKey);
	if (ret >= 0)
		ret = KrmEccExportCoordinates(&point, &g_krmEccCurve, publicKey);
	return ret;
}

int KrmEccExtendedGcd(const uint32_t *left, unsigned int leftWords,
	const uint32_t *right, unsigned int rightWords, uint32_t *coefficient,
	void *coefficientInfo, uint32_t *gcd, void *secondCoefficient,
	unsigned int *gcdWords, void *workspace, KrmEccContext *ctx);
uint32_t KrmEccMultiplyWord(const uint32_t *src, uint32_t word,
	uint32_t *result, unsigned int count);
uint32_t KrmEccMultiplyAddWord(const uint32_t *src, uint32_t word,
	uint32_t *result, unsigned int count);
int KrmEccAddIntegers(const uint32_t *left, unsigned int leftWords,
	const uint32_t *right, unsigned int rightWords, uint32_t *result,
	uint32_t *carry);

#define CLEAR_ECC_WORDS8(words) do { \
	volatile uint32_t *clearWords = (words); \
	clearWords[0] = 0; \
	clearWords[1] = 0; \
	clearWords[2] = 0; \
	clearWords[3] = 0; \
	clearWords[4] = 0; \
	clearWords[5] = 0; \
	clearWords[6] = 0; \
	clearWords[7] = 0; \
} while (0)
#define CLEAR_ECC_WORDS16(words) do { \
	CLEAR_ECC_WORDS8(words); \
	CLEAR_ECC_WORDS8((words) + 8); \
} while (0)

int KrmEccInternalSignDigest(const void *digest, const KrmEccPrivateKey *privateKey,
	KrmEccSignature *signature, KrmEccContext *ctx)
{
	typedef uint32_t EccClearVector __attribute__((vector_size(16)));
	unsigned int gcdWords = 0;
	union {
		uint32_t words[8];
		EccClearVector quadWords[2];
	} scalar;
	uint32_t r[8], inverse[8], hash[8];
	union {
		uint32_t words[16];
		EccClearVector quadWords[4];
	} product;
	uint32_t sum[16], reduced[16];
	KrmEccPublicKey point;
	uint32_t workspace[158];
	unsigned int attempt;
	uint32_t *wipe;
	int ret = (int)0x80070057;


	if (__builtin_expect(!digest, 0) || __builtin_expect(!privateKey, 0) ||
	    __builtin_expect(!signature, 0) || __builtin_expect(!ctx, 0))
		goto out;
	CLEAR_ECC_WORDS8(point.x);
	CLEAR_ECC_WORDS8(point.y);
	CLEAR_ECC_WORDS8(scalar.words);
	CLEAR_ECC_WORDS8(inverse);
	CLEAR_ECC_WORDS8(r);
	CLEAR_ECC_WORDS16(product.words);
	CLEAR_ECC_WORDS16(sum);
	CLEAR_ECC_WORDS16(reduced);
	CLEAR_ECC_WORDS8(hash);
	wipe = workspace;
	do
		*wipe++ = 0;
	while (wipe != workspace + 158);
	ret = KrmEccInitialize(ctx);
	if (ret < 0)
		goto out;
	ret = KrmEccReduceScalar(digest, &g_krmEccCurve, hash);
	if (ret < 0)
		goto out;
	attempt = 1000;
	for (;;) {
		if (!KrmEccRandomScalar(g_krmEccOrder, scalar.words, 8, ctx) ||
		    !KrmEccMultiplyPoint(&g_krmEccGenerator, scalar.words, 8, &point,
			&g_krmEccCurve, ctx))
			break;
		ret = KrmEccNormalizeScalar(point.x, &g_krmEccCurve, r);
		if (ret < 0)
			goto out;
		if (r[7] || r[6] || r[5] || r[4] || r[3] || r[2] || r[1] || r[0]) {
			ret = KrmEccExportInteger(r, (unsigned char *)signature->r);
			if (ret < 0)
				goto out;
			if (!KrmEccExtendedGcd(scalar.words, 8, g_krmEccOrder, 8, inverse,
				NULL, product.words, NULL, &gcdWords, workspace, ctx))
				break;
			if (gcdWords == 1 && product.words[0] == 1) {
				ret = KrmEccInternalImportPoint((const unsigned char *)privateKey, scalar.words);
				if (ret < 0)
					goto out;
				product.words[8] = KrmEccMultiplyWord(scalar.words, r[0], product.words, 8);
				product.words[9] = KrmEccMultiplyAddWord(scalar.words, r[1], product.words + 1, 8);
				product.words[10] = KrmEccMultiplyAddWord(scalar.words, r[2], product.words + 2, 8);
				product.words[11] = KrmEccMultiplyAddWord(scalar.words, r[3], product.words + 3, 8);
				product.words[12] = KrmEccMultiplyAddWord(scalar.words, r[4], product.words + 4, 8);
				product.words[13] = KrmEccMultiplyAddWord(scalar.words, r[5], product.words + 5, 8);
				product.words[14] = KrmEccMultiplyAddWord(scalar.words, r[6], product.words + 6, 8);
				product.words[15] = KrmEccMultiplyAddWord(scalar.words, r[7], product.words + 7, 8);
				scalar.quadWords[0] = (EccClearVector){0, 0, 0, 0};
				scalar.quadWords[1] = (EccClearVector){0, 0, 0, 0};
				if (!KrmEccReduce(product.words, 16, g_krmEccOrder, 8, NULL, NULL, sum) ||
				    !KrmEccAddIntegers(sum, 16, hash, 8, reduced, NULL) ||
				    !KrmEccReduce(reduced, 16, g_krmEccOrder, 8, NULL, NULL, scalar.words))
					break;
				product.quadWords[0] = (EccClearVector){0, 0, 0, 0};
				product.quadWords[1] = (EccClearVector){0, 0, 0, 0};
				product.quadWords[2] = (EccClearVector){0, 0, 0, 0};
				product.quadWords[3] = (EccClearVector){0, 0, 0, 0};
				product.words[8] = KrmEccMultiplyWord(scalar.words, inverse[0], product.words, 8);
				product.words[9] = KrmEccMultiplyAddWord(scalar.words, inverse[1], product.words + 1, 8);
				product.words[10] = KrmEccMultiplyAddWord(scalar.words, inverse[2], product.words + 2, 8);
				product.words[11] = KrmEccMultiplyAddWord(scalar.words, inverse[3], product.words + 3, 8);
				product.words[12] = KrmEccMultiplyAddWord(scalar.words, inverse[4], product.words + 4, 8);
				product.words[13] = KrmEccMultiplyAddWord(scalar.words, inverse[5], product.words + 5, 8);
				product.words[14] = KrmEccMultiplyAddWord(scalar.words, inverse[6], product.words + 6, 8);
				product.words[15] = KrmEccMultiplyAddWord(scalar.words, inverse[7], product.words + 7, 8);
				if (!KrmEccReduce(product.words, 16, g_krmEccOrder, 8, NULL, NULL, r))
					break;
				if (r[7] || r[6] || r[5] || r[4] || r[3] || r[2] || r[1] || r[0])
					goto export;
			}
		}
		if (--attempt == 0)
			goto export;
	}
	ret = (int)0x80040e8c;
	goto out;
export:
	ret = KrmEccExportInteger(r, (unsigned char *)signature->s);
out:
	CLEAR_ECC_WORDS8(scalar.words);
	CLEAR_ECC_WORDS8(inverse);
	CLEAR_ECC_WORDS16(product.words);
	CLEAR_ECC_WORDS16(sum);
	CLEAR_ECC_WORDS16(reduced);
	CLEAR_ECC_WORDS8(r);
	return ret;
}

int KrmEccSha256Init(SceSha256Context *ctx);
int KrmEccSha256Update(SceSha256Context *ctx, const void *src, SceSize size);
int KrmEccSha256Result(SceSha256Context *ctx, void *digest);

int KrmEccInternalSignMessage(const void *src, SceSize size,
	const KrmEccPrivateKey *privateKey, KrmEccSignature *signature, KrmEccContext *ctx)
{
	uint32_t digest[8];
	SceSha256Context hash;
	int ret;

	if (__builtin_expect(!src, 0) || __builtin_expect(!size, 0))
		return (int)0x80070057;
	ret = KrmEccSha256Init(&hash);
	if (ret < 0)
		return ret;
	ret = KrmEccSha256Update(&hash, src, size);
	if (ret < 0)
		return ret;
	ret = KrmEccSha256Result(&hash, digest);
	if (ret < 0)
		return ret;
	return KrmEccInternalSignDigest(digest, privateKey, signature, ctx);
}

int KrmEccCompareIntegers(const uint32_t *left, const uint32_t *right,
	unsigned int wordCount);

int KrmEccInternalVerifyDigest(const void *digest, const KrmEccPublicKey *publicKey,
	const KrmEccSignature *signature, KrmEccContext *ctx)
{
	unsigned int gcdWords = 0;
	uint32_t inverse[8], hash[8], gcd[16], product[16], scalar[16];
	KrmEccSignature nativeSignature;
	KrmEccPublicKey first, second, point;
	uint32_t workspace[158];
	unsigned char encoded[32];
	uint32_t *zero;
	unsigned char *clear;
	int index;
	volatile unsigned int difference;
	volatile unsigned char expectedByte;
	int ret;

	if (__builtin_expect(!publicKey, 0) || __builtin_expect(!digest, 0) ||
	    __builtin_expect(!signature, 0) || __builtin_expect(!ctx, 0))
		return (int)0x80070057;
	CLEAR_ECC_WORDS8(inverse);
	CLEAR_ECC_WORDS16(gcd);
	CLEAR_ECC_WORDS8(nativeSignature.r);
	CLEAR_ECC_WORDS8(nativeSignature.s);
	CLEAR_ECC_WORDS16(product);
	CLEAR_ECC_WORDS16(scalar);
	CLEAR_ECC_WORDS8(hash);
	zero = workspace;
	do
		*zero++ = 0;
	while (zero != workspace + 158);
	CLEAR_ECC_WORDS8(first.x);
	CLEAR_ECC_WORDS8(first.y);
	CLEAR_ECC_WORDS8(second.x);
	CLEAR_ECC_WORDS8(second.y);
	CLEAR_ECC_WORDS8(point.x);
	CLEAR_ECC_WORDS8(point.y);
	clear = encoded;
	do
		*clear++ = 0;
	while (clear != encoded + sizeof(encoded));
	ret = KrmEccInitialize(ctx);
	if (ret < 0)
		return ret;
	ret = KrmEccImportCoordinates(publicKey, &g_krmEccCurve, &point, ctx);
	if (ret < 0)
		return ret;
	ret = KrmEccReduceScalar(digest, &g_krmEccCurve, hash);
	if (ret < 0)
		return ret;
	ret = KrmEccInternalImportPoint((const unsigned char *)signature->r, nativeSignature.r);
	if (ret < 0)
		return ret;
	ret = KrmEccInternalImportPoint((const unsigned char *)signature->s, nativeSignature.s);
	if (ret < 0)
		return ret;
	if (!(nativeSignature.r[7] || nativeSignature.r[6] || nativeSignature.r[5] ||
	      nativeSignature.r[4] || nativeSignature.r[3] || nativeSignature.r[2] ||
	      nativeSignature.r[1] || nativeSignature.r[0]) ||
	    !(nativeSignature.s[7] || nativeSignature.s[6] || nativeSignature.s[5] ||
	      nativeSignature.s[4] || nativeSignature.s[3] || nativeSignature.s[2] ||
	      nativeSignature.s[1] || nativeSignature.s[0]) ||
	    KrmEccCompareIntegers(nativeSignature.r, g_krmEccOrder, 8) >= 0 ||
	    KrmEccCompareIntegers(nativeSignature.s, g_krmEccOrder, 8) >= 0)
		return (int)0x80040e8a;
	if (!KrmEccExtendedGcd(nativeSignature.s, 8, g_krmEccOrder, 8,
		inverse, NULL, gcd, NULL, &gcdWords, workspace, ctx))
		return (int)0x80040e8b;
	/* The original rejects only when both tests fail. */
	if (gcdWords != 1 && gcd[0] != 1)
		return (int)0x80040e8a;
	product[8] = KrmEccMultiplyWord(hash, inverse[0], product, 8);
	product[9] = KrmEccMultiplyAddWord(hash, inverse[1], product + 1, 8);
	product[10] = KrmEccMultiplyAddWord(hash, inverse[2], product + 2, 8);
	product[11] = KrmEccMultiplyAddWord(hash, inverse[3], product + 3, 8);
	product[12] = KrmEccMultiplyAddWord(hash, inverse[4], product + 4, 8);
	product[13] = KrmEccMultiplyAddWord(hash, inverse[5], product + 5, 8);
	product[14] = KrmEccMultiplyAddWord(hash, inverse[6], product + 6, 8);
	product[15] = KrmEccMultiplyAddWord(hash, inverse[7], product + 7, 8);
	if (!KrmEccReduce(product, 16, g_krmEccOrder, 8, NULL, NULL, scalar) ||
	    !KrmEccMultiplyPoint(&g_krmEccGenerator, scalar, 8, &first, &g_krmEccCurve, ctx))
		return (int)0x80040e8b;
	product[8] = KrmEccMultiplyWord(nativeSignature.r, inverse[0], product, 8);
	product[9] = KrmEccMultiplyAddWord(nativeSignature.r, inverse[1], product + 1, 8);
	product[10] = KrmEccMultiplyAddWord(nativeSignature.r, inverse[2], product + 2, 8);
	product[11] = KrmEccMultiplyAddWord(nativeSignature.r, inverse[3], product + 3, 8);
	product[12] = KrmEccMultiplyAddWord(nativeSignature.r, inverse[4], product + 4, 8);
	product[13] = KrmEccMultiplyAddWord(nativeSignature.r, inverse[5], product + 5, 8);
	product[14] = KrmEccMultiplyAddWord(nativeSignature.r, inverse[6], product + 6, 8);
	product[15] = KrmEccMultiplyAddWord(nativeSignature.r, inverse[7], product + 7, 8);
	if (!KrmEccReduce(product, 16, g_krmEccOrder, 8, NULL, NULL, scalar) ||
	    !KrmEccMultiplyPoint(&point, scalar, 8, &second, &g_krmEccCurve, ctx) ||
	    !KrmEccAddPoints(&first, &second, &first, 1, &g_krmEccCurve, workspace, ctx))
		return (int)0x80040e8b;
	if (KrmEccIsPointAtInfinity(&first, &g_krmEccCurve, NULL))
		return (int)0x80040e8a;
	ret = KrmEccExportNormalizedScalar(first.x, encoded);
	if (ret >= 0) {
		difference = 0;
		index = 31;
		do {
			unsigned char actualByte;

			expectedByte = ((const unsigned char *)signature->r)[index];
			actualByte = encoded[index--];
			difference |= expectedByte ^ actualByte;
		} while (index != -1);
		if (difference)
			ret = (int)0x800480cf;
	}
	return ret;
}

#undef CLEAR_ECC_WORDS16
#undef CLEAR_ECC_WORDS8

int KrmEccInternalVerifyMessage(const void *src, SceSize size,
	const KrmEccPublicKey *publicKey, const KrmEccSignature *signature, KrmEccContext *ctx)
{
	uint32_t digest[8];
	SceSha256Context hash;
	int ret;

	if (__builtin_expect(!src, 0) || __builtin_expect(!size, 0))
		return (int)0x80070057;
	ret = KrmEccSha256Init(&hash);
	if (ret >= 0)
		ret = KrmEccSha256Update(&hash, src, size);
	if (ret >= 0)
		ret = KrmEccSha256Result(&hash, digest);
	if (ret >= 0)
		ret = KrmEccInternalVerifyDigest(digest, publicKey, signature, ctx);
	return ret;
}

__attribute__((noinline)) int KrmEccRandomBytes(void *argument,
	unsigned char *dst, unsigned int size)
{
	(void)argument;
	KrmDriverGetRandomBytes(dst, size);
	return 0;
}

__attribute__((noinline)) int KrmEccSha256Init(SceSha256Context *ctx)
{
	return ksceSha256BlockInit(ctx);
}

__attribute__((noinline)) int KrmEccSha256Update(SceSha256Context *ctx,
	const void *src, SceSize size)
{
	return ksceSha256BlockUpdate(ctx, src, size);
}

__attribute__((noinline)) int KrmEccSha256Result(SceSha256Context *ctx, void *digest)
{
	return ksceSha256BlockResult(ctx, digest);
}

typedef struct KrmEccAllocationBlock {
	unsigned char size[4];
	unsigned char data[];
} KrmEccAllocationBlock;

__attribute__((noinline)) int KrmEccAllocate(KrmEccContext *ctx,
	unsigned int size, void **result)
{
	unsigned int alignedSize = size;
	unsigned int used, end, resetPending;
	const unsigned char *sizeBytes = (const unsigned char *)&alignedSize;
	KrmEccAllocationBlock *block;

	if (!ctx || !result || !size)
		return (int)0x80070057;
	*result = NULL;
	if (size & 3) {
		alignedSize = (size & ~3U) + 4;
		if (size > alignedSize)
			return (int)0x80070216;
	}
	used = ctx->allocation.used;
	end = used + alignedSize;
	if (used >= ctx->allocation.capacity || ctx->allocation.capacity <= end ||
	    used > end || ctx->allocation.capacity < end + 4 || used > end + 4)
		return (int)0x80000002;
	block = (KrmEccAllocationBlock *)(ctx->base + used);
	block->size[0] = sizeBytes[0];
	block->size[1] = sizeBytes[1];
	block->size[2] = sizeBytes[2];
	block->size[3] = sizeBytes[3];
	used = ctx->allocation.used;
	end = used + alignedSize + sizeof(*block);
	block = (KrmEccAllocationBlock *)(ctx->base + used);
	resetPending = ctx->resetPending;
	*result = block->data;
	ctx->allocation.used = end;
	if (resetPending)
		ctx->resetPending = 0;
	return 0;
}

__attribute__((noinline)) int KrmEccAllocateAligned(KrmEccContext *ctx, unsigned int size,
	unsigned int alignment, void **allocation, void **result)
{
	unsigned int alignedSize = size;
	unsigned int used, end, remainder, resetPending;
	const unsigned char *sizeBytes = (const unsigned char *)&alignedSize;
	KrmEccAllocationBlock *block;
	unsigned char *data;

	if (!ctx || !result || !size || !alignment)
		return (int)0x80070057;
	*result = NULL;
	if (allocation)
		*allocation = NULL;
	if (size & 3) {
		alignedSize = (size & ~3U) + 4;
		if (size > alignedSize)
			return (int)0x80070216;
	}
	used = ctx->allocation.used;
	block = (KrmEccAllocationBlock *)(ctx->base + used);
	data = block->data;
	if (allocation)
		*allocation = data;
	remainder = (uintptr_t)data % alignment;
	if (remainder) {
		if (alignedSize + alignment < alignedSize)
			return (int)0x80070216;
		alignedSize += alignment;
		*result = data + alignment - remainder;
	} else {
		*result = data;
	}
	end = used + alignedSize;
	if (used >= ctx->allocation.capacity || ctx->allocation.capacity <= end ||
	    used > end || ctx->allocation.capacity < end + 4 || used > end + 4) {
		*result = NULL;
		return (int)0x80000002;
	}
	block = (KrmEccAllocationBlock *)(ctx->base + used);
	block->size[0] = sizeBytes[0];
	block->size[1] = sizeBytes[1];
	block->size[2] = sizeBytes[2];
	block->size[3] = sizeBytes[3];
	resetPending = ctx->resetPending;
	ctx->allocation.used += alignedSize + sizeof(*block);
	if (resetPending)
		ctx->resetPending = 0;
	return 0;
}

__attribute__((noinline)) int KrmEccRelease(KrmEccContext *ctx, void *ptr)
{
	KrmEccAllocationBlock *block;
	unsigned int size = 0;
	unsigned char *sizeBytes = (unsigned char *)&size;

	if (!ptr)
		return (int)0x8004c3e8;
	if (!ctx)
		return (int)0x80070057;
	block = (KrmEccAllocationBlock *)((unsigned char *)ptr -
		__builtin_offsetof(KrmEccAllocationBlock, data));
	sizeBytes[0] = block->size[0];
	sizeBytes[1] = block->size[1];
	sizeBytes[2] = block->size[2];
	sizeBytes[3] = block->size[3];
	if ((unsigned char *)ptr + size != ctx->base + ctx->allocation.used)
		return (int)0x8004c055;
	ctx->allocation.used = ctx->allocation.used - sizeof(*block) - size;
	return 0;
}

__attribute__((noinline)) uint32_t KrmEccMultiplyAddWord(const uint32_t *src,
	uint32_t word, uint32_t *result, unsigned int count)
{
	uint64_t carry = 0;
	uint64_t product;
	int i = 0;
	unsigned int limit = count - 7;
	const uint32_t *source;
	uint32_t *destination, *end;

	if (!count)
		return 0;
#define MULTIPLY_ADD_WORD(n) do { \
	product = carry + destination[(n) - 16] + (uint64_t)source[(n) - 16] * word; \
	destination[(n) - 16] = product; \
	carry = product >> 32; \
} while (0)
	if (count > 8) {
		source = src + 16;
		destination = result + 16;
		do {
			__builtin_prefetch(source);
			__builtin_prefetch(destination);
			MULTIPLY_ADD_WORD(0);
			MULTIPLY_ADD_WORD(1);
			MULTIPLY_ADD_WORD(2);
			MULTIPLY_ADD_WORD(3);
			MULTIPLY_ADD_WORD(4);
			MULTIPLY_ADD_WORD(5);
			MULTIPLY_ADD_WORD(6);
			MULTIPLY_ADD_WORD(7);
			source += 8;
			destination += 8;
			i += 8;
		} while (limit > i + 1);
	}
	source = src + i;
	destination = result + i;
	end = result + count;
	do {
		product = carry + *destination + (uint64_t)*source++ * word;
		*destination++ = product;
		carry = product >> 32;
	} while (destination != end);
#undef MULTIPLY_ADD_WORD
	return carry;
}

uint32_t KrmEccMultiplySubtractWord(const uint32_t *src, uint32_t word,
	uint32_t *result, unsigned int count)
{
	uint32_t borrow = 0;
	uint64_t difference;
	int i = 0;
	unsigned int limit = count - 7;
	const uint32_t *source;
	uint32_t *destination, *end;

	if (!count)
		return 0;
#define MULTIPLY_SUBTRACT_WORD(n) do { \
	difference = (uint64_t)destination[(n) - 15] - borrow -	\
		(uint64_t)source[(n) - 15] * word; \
	destination[(n) - 15] = difference; \
	borrow = -(uint32_t)(difference >> 32); \
} while (0)
	if (count > 8) {
		source = src + 15;
		destination = result + 15;
		do {
			__builtin_prefetch(destination);
			__builtin_prefetch(source);
			MULTIPLY_SUBTRACT_WORD(0);
			MULTIPLY_SUBTRACT_WORD(1);
			MULTIPLY_SUBTRACT_WORD(2);
			MULTIPLY_SUBTRACT_WORD(3);
			MULTIPLY_SUBTRACT_WORD(4);
			MULTIPLY_SUBTRACT_WORD(5);
			MULTIPLY_SUBTRACT_WORD(6);
			MULTIPLY_SUBTRACT_WORD(7);
			source += 8;
			destination += 8;
			i += 8;
		} while (limit > i + 1);
	}
	source = src + i;
	destination = result + i;
	end = result + count;
	do {
		difference = (uint64_t)*destination - borrow -
			(uint64_t)*source++ * word;
		*destination++ = difference;
		borrow = -(uint32_t)(difference >> 32);
	} while (destination != end);
#undef MULTIPLY_SUBTRACT_WORD
	return borrow;
}

int KrmEccShiftWord(const uint32_t *src, int bits, uint32_t *result,
	unsigned int count, uint32_t *overflow)
{
	unsigned int magnitude = bits < 0 ? -(unsigned int)bits : (unsigned int)bits;
	unsigned int i;
	uint32_t carry = 0, word;

	if ((int)magnitude > 32)
		return 0;
	if (!count)
		goto out;
	if (bits == 32) {
		carry = src[count - 1];
		i = count - 1;
#define COPY_SHIFT_LEFT(n) result[i - (n)] = src[i - (n) - 1]
		while (i > 8) {
			__builtin_prefetch(src + i - 21);
			COPY_SHIFT_LEFT(0);
			COPY_SHIFT_LEFT(1);
			COPY_SHIFT_LEFT(2);
			COPY_SHIFT_LEFT(3);
			COPY_SHIFT_LEFT(4);
			COPY_SHIFT_LEFT(5);
			COPY_SHIFT_LEFT(6);
			COPY_SHIFT_LEFT(7);
			i -= 8;
		}
		while (i) {
			COPY_SHIFT_LEFT(0);
			i--;
		}
#undef COPY_SHIFT_LEFT
		result[0] = 0;
	} else if (bits > 0) {
		i = 0;
#define SHIFT_LEFT_WORD(n) do { \
	word = src[i + (n)]; \
	result[i + (n)] = (word << bits) | carry; \
	carry = word >> (32 - bits); \
} while (0)
		while (count - i > 8) {
			__builtin_prefetch(src + i + 19);
			__builtin_prefetch(result + i + 19);
			SHIFT_LEFT_WORD(0);
			SHIFT_LEFT_WORD(1);
			SHIFT_LEFT_WORD(2);
			SHIFT_LEFT_WORD(3);
			SHIFT_LEFT_WORD(4);
			SHIFT_LEFT_WORD(5);
			SHIFT_LEFT_WORD(6);
			SHIFT_LEFT_WORD(7);
			i += 8;
		}
		do {
			SHIFT_LEFT_WORD(0);
			i++;
		} while (i != count);
#undef SHIFT_LEFT_WORD
	} else if (!bits) {
		unsigned char *dst = (unsigned char *)result;
		const unsigned char *bytes = (const unsigned char *)src;

		const unsigned char *end = (const unsigned char *)(src + count);

		do {
			*dst++ = *bytes++;
		} while (bytes != end);
	} else if (bits == -32) {
		carry = src[0];
		for (i = 1; i < count; i++)
			result[i - 1] = src[i];
		result[count - 1] = 0;
	} else {
		i = count;
#define SHIFT_RIGHT_WORD(n) do { \
	word = src[i - (n) - 1]; \
	result[i - (n) - 1] = (word >> magnitude) | carry; \
	carry = word << (32 - magnitude); \
} while (0)
		while (i > 8) {
			__builtin_prefetch(src + i - 20);
			__builtin_prefetch(result + i - 20);
			SHIFT_RIGHT_WORD(0);
			SHIFT_RIGHT_WORD(1);
			SHIFT_RIGHT_WORD(2);
			SHIFT_RIGHT_WORD(3);
			SHIFT_RIGHT_WORD(4);
			SHIFT_RIGHT_WORD(5);
			SHIFT_RIGHT_WORD(6);
			SHIFT_RIGHT_WORD(7);
			i -= 8;
		}
		do {
			SHIFT_RIGHT_WORD(0);
			i--;
		} while (i);
#undef SHIFT_RIGHT_WORD
		carry >>= 32 - magnitude;
	}
out:
	*overflow = carry;
	return 1;
}


int KrmEccShiftInteger(const uint32_t *src, int bits, uint32_t *result,
	unsigned int count)
{
	uint32_t overflow = 0;
	unsigned int words, i, source;
	int ret;

	if (bits >= 0) {
		words = bits >> 5;
		ret = KrmEccShiftWord(src, bits - words * 32, result, count, &overflow);
		if (!ret)
			return 0;
		if (words) {
			for (i = count; i-- != 0;)
				result[i] = words > i ? 0 : result[i - words];
		}
	} else {
		words = (-(unsigned int)bits) >> 5;
		ret = KrmEccShiftWord(src, bits + words * 32, result, count, &overflow);
		if (!ret)
			return 0;
		if (words) {
			for (i = 0, source = words; i < count; i++, source++) {
				result[i] = 0;
				if (i <= source && source >= words && count > source)
					result[i] = result[source];
			}
		}
	}
	return 1;
}

__attribute__((noinline)) uint32_t KrmEccMultiplyWord(const uint32_t *src,
	uint32_t word, uint32_t *result, unsigned int count)
{
	uint64_t carry = 0;
	uint64_t product;
	unsigned int i = 0;

	if (!count)
		return 0;
#define MULTIPLY_WORD(n) do { \
	product = (uint64_t)src[i + (n)] * word + carry; \
	result[i + (n)] = product; \
	carry = product >> 32; \
} while (0)
	while (count - i > 8) {
		__builtin_prefetch(result + i + 19);
		__builtin_prefetch(src + i + 19);
		MULTIPLY_WORD(0);
		MULTIPLY_WORD(1);
		MULTIPLY_WORD(2);
		MULTIPLY_WORD(3);
		MULTIPLY_WORD(4);
		MULTIPLY_WORD(5);
		MULTIPLY_WORD(6);
		MULTIPLY_WORD(7);
		i += 8;
	}
	do {
		MULTIPLY_WORD(0);
		i++;
	} while (i < count);
#undef MULTIPLY_WORD
	return carry;
}

int KrmEccCompareSizedIntegers(const uint32_t *left, unsigned int leftWords,
	const uint32_t *right, unsigned int rightWords);

__attribute__((noinline)) int KrmEccCompareWord(const uint32_t *value, uint32_t word, unsigned int count)
{
	return KrmEccCompareSizedIntegers(value, count, &word, 1);
}

int KrmEccInitializeReciprocal(const uint32_t *value, unsigned int count,
	KrmEccReciprocal *result)
{
	uint32_t high = value[count - 1];
	uint32_t leading, next = 0, lower = 0, normalized, remainder, bits;
	uint32_t factor = 0, mask = 0x80000000, complement, carry, candidate;
	int width = 32;
	unsigned int shift, i;
	const uint32_t *source;
	uint64_t dividend, product, difference;

	if (!high)
		return 0;
	leading = high | 1;
	while (leading <= 0x07ffffff) {
		leading <<= 5;
		width -= 5;
	}
	width -= (0x24949cU >> (leading >> 28) >> (2 * (leading >> 28))) & 7;
	shift = 32 - width;
	if (count > 1) {
		next = value[count - 2];
		if (count == 2) {
			lower = next << shift;
			next = next >> 1 >> (width - 1);
			complement = ~lower;
		} else {
			lower = (next << shift) |
				(value[count - 3] >> 1 >> (width - 1));
			next = next >> 1 >> (width - 1);
			complement = ~lower;
		}
	} else {
		complement = 0xffffffffU;
	}
	normalized = (high << shift) | next;
	remainder = ~normalized;
	if (normalized <= remainder)
		return 0;
	for (i = 32; i; i--) {
		candidate = mask + factor;
		carry = complement >> 31;
		complement <<= 1;
		mask >>= 1;
		if (carry + remainder >= normalized - remainder) {
			factor = candidate;
			carry -= normalized;
		}
		remainder = carry + 2 * remainder;
	}
	bits = (0xffffffffU >> shift) - high;
	if (remainder < ((uint64_t)factor * lower) >> 32)
		factor--;
	source = value + count;
	i = count - 1;
	while (i != (unsigned int)-1 && bits < factor) {
		dividend = ((uint64_t)bits << 32) |
			(i ? ~source[-2] : 0xffffffffU);
		product = (uint64_t)*--source * factor;
		i--;
		if (product >= dividend) {
			factor--;
			break;
		}
		difference = dividend - product;
		bits = difference;
		if (difference >> 32)
			break;
	}
	*result = (KrmEccReciprocal){ factor, shift };
	return 1;
}

uint32_t KrmEccEstimateQuotient(uint32_t high, uint32_t middle, uint32_t low,
	const KrmEccReciprocal *reciprocal)
{
	unsigned int shift = reciprocal->shift;
	uint32_t normalizedHigh = (middle >> 1 >> (31 - shift)) | (high << shift);
	uint32_t normalizedLow = (low >> 1 >> (31 - shift)) | (middle << shift);
	uint64_t normalized = ((uint64_t)normalizedHigh << 32) | normalizedLow;
	uint64_t estimate = (uint64_t)reciprocal->factor * normalizedHigh + normalized;

	if ((int32_t)normalizedLow < 0)
		estimate += reciprocal->factor >> 1;
	return estimate >> 32;
}

int KrmEccDivideWord(const uint32_t *value, uint32_t divisor,
	const KrmEccReciprocal *reciprocal, uint32_t *quotient,
	unsigned int count, uint32_t *remainder)
{
	KrmEccReciprocal local;
	unsigned int words = count, i, bit;
	uint32_t rest = 0, word, mask, digit, carry, candidate;
	uint32_t high, low, estimate;
	uint64_t product, correction;
	int ret = 1;

	if (count && value[count - 1] < divisor) {
		words = count - 1;
		rest = value[words];
		if (quotient)
			quotient[words] = 0;
	}
	if (words <= 1 && !reciprocal) {
		for (i = words; i-- != 0;) {
			digit = 0;
			if (ret) {
				word = value[i];
				if (rest < divisor) {
					mask = 0x80000000;
					for (bit = 32; bit; bit--) {
						candidate = digit + mask;
						carry = word >> 31;
						word <<= 1;
						mask >>= 1;
						if (carry + rest >= divisor - rest) {
							digit = candidate;
							carry -= divisor;
						}
						rest = carry + 2 * rest;
					}
				} else {
					ret = 0;
				}
			}
			if (quotient)
				quotient[i] = digit;
		}
	} else {
		if (!reciprocal) {
			if (!KrmEccInitializeReciprocal(&divisor, 1, &local))
				return 0;
			reciprocal = &local;
		}
		if (!quotient) {
			uint32_t factor = reciprocal->factor;
			unsigned int shift = reciprocal->shift;
			const uint32_t *source = value + words;

			for (i = words; i-- != 0;) {
				word = *--source;
				high = (word >> 1 >> (31 - shift)) | (rest << shift);
				low = word << shift;
				product = (uint64_t)factor * high +
					(((uint64_t)high << 32) | low);
				if ((int32_t)low < 0)
					product += factor >> 1;
				estimate = product >> 32;
				correction = (uint64_t)divisor * ~estimate +
					(((uint64_t)(rest - divisor) << 32) | word);
				rest = ((correction >> 32) & divisor) + (uint32_t)correction;
			}
		} else {
			for (i = words; i-- != 0;) {
				word = value[i];
				high = (word >> 1 >> (31 - reciprocal->shift)) |
					(rest << reciprocal->shift);
				low = word << reciprocal->shift;
				product = (uint64_t)reciprocal->factor * high +
					((uint64_t)high << 32) + low;
				if ((int32_t)low < 0)
					product += reciprocal->factor >> 1;
				estimate = product >> 32;
				correction = (uint64_t)divisor * ~estimate +
					((uint64_t)(rest - divisor) << 32) + word;
				quotient[i] = (correction >> 32) + estimate + 1;
				rest = ((correction >> 32) & divisor) + (uint32_t)correction;
			}
		}
	}
	if (ret)
		*remainder = rest;
	return ret;
}

uint32_t KrmEccAddWords(const uint32_t *left, const uint32_t *right,
	uint32_t *result, unsigned int count);

int KrmEccReduce(const uint32_t *value, unsigned int valueWords,
	const uint32_t *modulus, unsigned int modulusWords,
	const uint32_t *reciprocal, uint32_t *quotient, uint32_t *remainder)
{
	KrmEccReciprocal local;
	const KrmEccReciprocal *division = (const KrmEccReciprocal *)reciprocal;
	unsigned int quotientWords, i, j;
	uint32_t high, estimate, digit, borrow;

	if (!modulusWords || !value || !modulus || !remainder ||
	    value == quotient || value == remainder ||
	    modulus == quotient || modulus == remainder || !modulus[modulusWords - 1])
		return 0;
	if (modulusWords > valueWords) {
		const unsigned char *src = (const unsigned char *)value;
		unsigned char *dst = (unsigned char *)remainder;

		for (i = 0; i < valueWords * sizeof(*value); i++)
			dst[i] = src[i];
		dst += i;
		i = (modulusWords - valueWords) * sizeof(*modulus);
		while (i > 32) {
			dst[0] = 0;
			__builtin_prefetch(dst + 40);
			dst[1] = 0;
			dst[2] = 0;
			dst[3] = 0;
			dst[4] = 0;
			dst[5] = 0;
			dst[6] = 0;
			dst[7] = 0;
			dst[8] = 0;
			dst[9] = 0;
			dst[10] = 0;
			dst[11] = 0;
			dst[12] = 0;
			dst[13] = 0;
			dst[14] = 0;
			dst[15] = 0;
			dst[16] = 0;
			dst[17] = 0;
			dst[18] = 0;
			dst[19] = 0;
			dst[20] = 0;
			dst[21] = 0;
			dst[22] = 0;
			dst[23] = 0;
			dst[24] = 0;
			dst[25] = 0;
			dst[26] = 0;
			dst[27] = 0;
			dst[28] = 0;
			dst[29] = 0;
			dst[30] = 0;
			dst[31] = 0;
			dst += 32;
			i -= 32;
		}
		while (i--)
			*dst++ = 0;
		return 1;
	}
	if (modulusWords == 1)
		return KrmEccDivideWord(value, modulus[0], division, quotient,
			valueWords, remainder) != 0;
	if (!division) {
		KrmEccInitializeReciprocal(modulus, modulusWords, &local);
		division = &local;
	}
	quotientWords = valueWords + 1 - modulusWords;
	remainder[modulusWords - 1] = 0;
	{
		const unsigned char *source = (const unsigned char *)(value + quotientWords);
		unsigned char *destination = (unsigned char *)remainder;
		const unsigned char *end = source + (modulusWords - 1) * sizeof(*modulus);

		do {
			*destination++ = *source++;
		} while (source != end);
	}
	for (i = quotientWords; i-- != 0;) {
		high = remainder[modulusWords - 1];
		j = modulusWords - 1;
#define SHIFT_REMAINDER(n) remainder[j - (n)] = remainder[j - (n) - 1]
		while (j > 8) {
			__builtin_prefetch(remainder + j - 21);
			SHIFT_REMAINDER(0);
			SHIFT_REMAINDER(1);
			SHIFT_REMAINDER(2);
			SHIFT_REMAINDER(3);
			SHIFT_REMAINDER(4);
			SHIFT_REMAINDER(5);
			SHIFT_REMAINDER(6);
			SHIFT_REMAINDER(7);
			j -= 8;
		}
		do {
			SHIFT_REMAINDER(0);
			j--;
		} while (j);
#undef SHIFT_REMAINDER
		remainder[0] = value[i];
		digit = 0;
		if (high || KrmEccCompareIntegers(remainder, modulus, modulusWords) >= 0) {
			unsigned int shift = division->shift;
			uint32_t normalizedHigh = (remainder[modulusWords - 1] >> 1 >>
				(31 - shift)) | (high << shift);
			uint32_t normalizedLow = (remainder[modulusWords - 2] >> 1 >>
				(31 - shift)) | (remainder[modulusWords - 1] << shift);
			uint64_t product = (uint64_t)division->factor * normalizedHigh +
				((uint64_t)normalizedHigh << 32) + normalizedLow;

			if ((int32_t)normalizedLow < 0)
				product += division->factor >> 1;
			estimate = product >> 32;
			digit = estimate == 0xffffffffU ? estimate : estimate + 1;
			borrow = KrmEccMultiplySubtractWord(modulus, digit, remainder, modulusWords);
			if (high < borrow) {
				digit--;
				borrow -= KrmEccAddWords(remainder, modulus, remainder, modulusWords);
			}
			if (borrow != high) {
				if (quotient)
					quotient[i] = digit;
				return 0;
			}
		}
		if (quotient)
			quotient[i] = digit;
	}
	return 1;
}

uint32_t *KrmEccAllocateWords(unsigned int count, KrmEccContext *ctx);
int KrmEccDoubleAndAddPoints(const KrmEccPublicKey *left,
	const KrmEccPublicKey *right, KrmEccPublicKey *result, int sign,
	const KrmEccCurve *curve, void *workspace, KrmEccContext *ctx);
int KrmEccAddSubtractPoints(const KrmEccPublicKey *left,
	const KrmEccPublicKey *right, KrmEccPublicKey *sum, KrmEccPublicKey *difference,
	const KrmEccCurve *curve, void *workspace, KrmEccContext *ctx);
int KrmEccSetPointAtInfinity(KrmEccPublicKey *point, const KrmEccCurve *curve,
	KrmEccContext *ctx);

int KrmEccMultiplyPoint(const KrmEccPublicKey *point, const uint32_t *scalar,
	unsigned int wordCount, KrmEccPublicKey *result, const KrmEccCurve *curve,
	KrmEccContext *ctx)
{
	unsigned int fieldWords = curve->field->wordCount;
	unsigned int significant = wordCount, bitCount = 0, width = 32;
	unsigned int window, entries, maximum, twiceMaximum, tableWords, allocationWords;
	unsigned int bit, nextBit, accumulator = 0, current = 1, level, odd;
	unsigned int target, marker, input, distance, magnitude;
	uint32_t leading, *allocation = NULL, *table = NULL, *workspace = NULL;
	_Bool ok = 0;
	int seeded = 0, sign;
	int32_t difference, adjustment;

	while (significant && !scalar[significant - 1])
		significant--;
	if (significant) {
		leading = scalar[significant - 1] | 1;
		while (leading <= 0x07ffffff) {
			leading <<= 5;
			width -= 5;
		}
		bitCount = width + 32 * (significant - 1) -
			((0x24949c >> (leading >> 28) >> (2 * (leading >> 28))) & 7);
	}
	window = bitCount <= 22 ? 2 : bitCount <= 50 ? 3 : bitCount <= 110 ? 4 :
		bitCount <= 256 ? 5 : bitCount <= 600 ? 6 : 7;
	if (curve->multiplyPoint)
		return curve->multiplyPoint(point, scalar, wordCount, result, curve, ctx);
	entries = 1U << (window - 1);
	maximum = entries - 1;
	twiceMaximum = 2 * maximum;
	tableWords = fieldWords * (entries + 2);
	allocationWords = curve->workspaceWords + wordCount + tableWords;
	if (curve->workspaceWords <= allocationWords && wordCount <= allocationWords &&
	    fieldWords <= tableWords && tableWords <= allocationWords)
		allocation = KrmEccAllocateWords(allocationWords, ctx);
#define ECC_TABLE_POINT(n) ((KrmEccPublicKey *)(table + ((n) - 1) * fieldWords))
#define COPY_ECC_POINT(destination, source) do { \
	unsigned int pointCopySize = 2 * curve->field->wordCount * sizeof(uint32_t); \
	unsigned char *pointCopyDst = (unsigned char *)(destination); \
	const unsigned char *pointCopySrc = (const unsigned char *)(source); \
	const unsigned char *pointCopyEnd = pointCopySrc + pointCopySize; \
	if (pointCopySize) { \
		do { \
			*pointCopyDst++ = *pointCopySrc++; \
		} while (pointCopySrc != pointCopyEnd); \
	} \
} while (0)
	if (allocation) {
		unsigned char *clear = (unsigned char *)allocation;
		unsigned int remaining = allocationWords * sizeof(*allocation);

		while (remaining > 32) {
			clear[0] = 0;
			__builtin_prefetch(clear + 40);
			clear[1] = 0;
			clear[2] = 0;
			clear[3] = 0;
			clear[4] = 0;
			clear[5] = 0;
			clear[6] = 0;
			clear[7] = 0;
			clear[8] = 0;
			clear[9] = 0;
			clear[10] = 0;
			clear[11] = 0;
			clear[12] = 0;
			clear[13] = 0;
			clear[14] = 0;
			clear[15] = 0;
			clear[16] = 0;
			clear[17] = 0;
			clear[18] = 0;
			clear[19] = 0;
			clear[20] = 0;
			clear[21] = 0;
			clear[22] = 0;
			clear[23] = 0;
			clear[24] = 0;
			clear[25] = 0;
			clear[26] = 0;
			clear[27] = 0;
			clear[28] = 0;
			clear[29] = 0;
			clear[30] = 0;
			clear[31] = 0;
			clear += 32;
			remaining -= 32;
		}
		while (remaining--)
			*clear++ = 0;
		table = allocation + wordCount;
		workspace = table + (entries + 2) * fieldWords;
		COPY_ECC_POINT(table, point);
		if (window == 2) {
			COPY_ECC_POINT(result, point);
			ok = 1;
		} else {
			ok = KrmEccDoubleAndAddPoints(point, point, ECC_TABLE_POINT(3),
				1, curve, workspace, ctx) != 0;
			current = 3;
			if (window == 3) {
				COPY_ECC_POINT(result, ECC_TABLE_POINT(3));
			} else {
				if (ok)
					ok = KrmEccAddPoints(ECC_TABLE_POINT(3), ECC_TABLE_POINT(3),
						result, 1, curve, workspace, ctx) != 0;
				for (level = 4; level <= window; level++) {
					current *= 2;
					for (odd = 1; odd < (1U << (level - 3)); odd += 2) {
						if (ok)
							ok = KrmEccAddSubtractPoints(result, ECC_TABLE_POINT(odd),
								ECC_TABLE_POINT(current + odd), ECC_TABLE_POINT(current - odd),
								curve, workspace, ctx) != 0;
					}
					if (level != window && ok)
						ok = KrmEccAddPoints(result, result, result, 1,
							curve, workspace, ctx) != 0;
				}
			}
		}
	}
	bit = 0;
	for (nextBit = 0; nextBit < bitCount; nextBit++) {
		accumulator += ((scalar[nextBit >> 5] >> (nextBit & 31)) & 1) << (nextBit - bit);
		if (bit + window + 1 <= nextBit) {
			if (accumulator & 1) {
				accumulator -= ((accumulator + maximum) & twiceMaximum) - maximum;
				if (wordCount > bit >> 5)
					allocation[bit >> 5] |= 1U << (bit & 31);
			}
			accumulator >>= 1;
			bit++;
		}
	}
	while (!seeded && ok) {
		if (!accumulator) {
			ok = KrmEccSetPointAtInfinity(result, curve, ctx) != 0;
			seeded = 1;
		} else if (accumulator <= maximum && (accumulator & 1)) {
			COPY_ECC_POINT(result, ECC_TABLE_POINT(accumulator));
			seeded = 1;
		} else if (accumulator == current) {
			seeded = 1;
		} else if (!(accumulator & 1) && accumulator <= twiceMaximum) {
			odd = accumulator - 1 < maximum ? accumulator - 1 : maximum;
			ok = KrmEccAddPoints(ECC_TABLE_POINT(odd), ECC_TABLE_POINT(accumulator - odd),
				result, 1, curve, workspace, ctx) != 0;
			seeded = 1;
		} else if (((accumulator - current) & 1) && accumulator <= current + maximum) {
			ok = KrmEccAddPoints(result, ECC_TABLE_POINT(accumulator - current),
				result, 1, curve, workspace, ctx) != 0;
			seeded = 1;
		} else if ((accumulator & 1) && accumulator <= twiceMaximum + maximum) {
			odd = ((accumulator - maximum) >> 1) | 1;
			ok = KrmEccDoubleAndAddPoints(ECC_TABLE_POINT(odd),
				ECC_TABLE_POINT(accumulator - 2 * odd), result, 1, curve, workspace, ctx) != 0;
			seeded = 1;
		} else if (((accumulator - current) & 3) == 2 && accumulator - current <= twiceMaximum) {
			ok = KrmEccDoubleAndAddPoints(ECC_TABLE_POINT((accumulator - current) >> 1),
				result, result, 1, curve, workspace, ctx) != 0;
			seeded = 1;
		} else if (!(accumulator & 1) && accumulator <= 2 * (current + maximum)) {
			goto halve;
		} else if (!(accumulator & 3) && accumulator <= 4 * maximum) {
			goto halve;
		} else if (!(accumulator % 3) && !(accumulator & 1) && accumulator / 3 <= twiceMaximum) {
			odd = accumulator / 3 - 1 < maximum ? accumulator / 3 - 1 : maximum;
			ok = KrmEccAddPoints(ECC_TABLE_POINT(odd), ECC_TABLE_POINT(accumulator / 3 - odd),
				result, 1, curve, workspace, ctx) != 0;
			if (ok)
				ok = KrmEccDoubleAndAddPoints(result, result, result, 1, curve, workspace, ctx) != 0;
			seeded = 1;
		} else if (!(accumulator % 3) && ((accumulator - current) & 1) &&
			maximum >= accumulator / 3 - current) {
			ok = KrmEccAddPoints(ECC_TABLE_POINT(accumulator / 3 - current), result,
				result, 1, curve, workspace, ctx) != 0;
			if (ok)
				ok = KrmEccDoubleAndAddPoints(result, result, result, 1, curve, workspace, ctx) != 0;
			seeded = 1;
		} else if (accumulator & 1) {
			if (accumulator > 5 * maximum) {
				accumulator -= (twiceMaximum & (accumulator + maximum)) - maximum;
				if (wordCount > bit >> 5)
					allocation[bit >> 5] |= 1U << (bit & 31);
				goto halve;
			}
			odd = (3 - entries + accumulator) >> 2;
			ok = KrmEccAddPoints(ECC_TABLE_POINT(entries - 1),
				ECC_TABLE_POINT(1 - entries + 2 * odd), result, 1, curve, workspace, ctx) != 0;
			if (ok)
				ok = KrmEccDoubleAndAddPoints(result, ECC_TABLE_POINT(accumulator - 4 * odd),
					result, 1, curve, workspace, ctx) != 0;
			seeded = 1;
		} else {
halve:
			accumulator >>= 1;
			bit++;
		}
	}
	for (nextBit = bitCount; nextBit-- != 0 && ok;) {
		marker = (allocation[nextBit >> 5] >> (nextBit & 31)) & 1;
		target = nextBit + marker;
		while (bit > target && ok) {
			bit--;
			accumulator *= 2;
			ok = KrmEccAddPoints(result, result, result, 1, curve, workspace, ctx) != 0;
		}
		if (!ok)
			break;
		input = (scalar[nextBit >> 5] >> (nextBit & 31)) & 1;
		if (marker) {
			distance = bit - nextBit;
			difference = (accumulator << distance) - input;
			sign = difference > 0 ? -1 : 1;
			adjustment = -difference | 1;
			accumulator = difference + adjustment;
			magnitude = adjustment * sign;
			if (distance > 1 || maximum < magnitude) {
				ok = 0;
			} else {
				bit = nextBit;
				ok = distance ? KrmEccDoubleAndAddPoints(result, ECC_TABLE_POINT(magnitude),
					result, sign, curve, workspace, ctx) != 0 :
					KrmEccAddPoints(result, ECC_TABLE_POINT(magnitude), result,
						sign, curve, workspace, ctx) != 0;
			}
		} else {
			accumulator -= input << (nextBit - bit);
		}
	}
#undef COPY_ECC_POINT
#undef ECC_TABLE_POINT
	if (allocation)
		KrmEccFree(allocation, ctx);
	return ok;
}

int KrmEccCopySignedPoint(const KrmEccPublicKey *point, KrmEccPublicKey *result,
	int sign, const KrmEccCurve *curve, KrmEccContext *ctx);
int KrmEccInvertFieldElement(const uint32_t *value, uint32_t *result,
	const KrmEccField *field, void *workspace, KrmEccContext *ctx);
int KrmEccMultiplyFieldElements(const uint32_t *left, const uint32_t *right,
	uint32_t *result, unsigned int count, const KrmEccField *field,
	void *workspace, KrmEccContext *ctx);

int KrmEccDoubleAndAddPoints(const KrmEccPublicKey *left,
	const KrmEccPublicKey *right, KrmEccPublicKey *result, int sign,
	const KrmEccCurve *curve, void *workspace, KrmEccContext *ctx)
{
	const KrmEccField *field = curve->field;
	unsigned int words = field->wordCount, i;
	int binary = field->flags > 1, ok, status = 1;
	uint32_t *slope = workspace;
	uint32_t *otherY = workspace ? slope + words : NULL;
	uint32_t *x = workspace ? otherY + words : NULL;
	uint32_t *y = workspace ? x + words : NULL;
	void *scratch = workspace ? y + words : NULL;
	const uint32_t *signedY = right->x + words;
	const uint32_t *oppositeY = otherY;
	const uint32_t *leftY = left->x + words;
	uint32_t *resultY = result->x + words;
	uint32_t nonzero = 0, value;

#define COPY_ECC_WORDS(destination, source, count) do { \
	unsigned int copySize = (count) * sizeof(uint32_t); \
	unsigned char *copyDst = (unsigned char *)(destination); \
	const unsigned char *copySrc = (const unsigned char *)(source); \
	const unsigned char *copyEnd = copySrc + copySize; \
	if (copySize) { \
		do { \
			*copyDst++ = *copySrc++; \
		} while (copySrc != copyEnd); \
	} \
} while (0)

	if (workspace) {
		if (KrmEccIsPointAtInfinity(left, curve, ctx))
			return KrmEccCopySignedPoint(right, result, sign, curve, ctx) != 0;
		if (KrmEccIsPointAtInfinity(right, curve, ctx))
			return KrmEccAddPoints(left, left, result, 1, curve, workspace, ctx) != 0;
		ok = field->operations->subtract(left->x, right->x, x, 1, field, ctx) != 0;
		if (ok)
			ok = binary ? field->operations->add(right->x + words, right->x, otherY, 1, field, ctx) != 0 :
				field->operations->negate(right->x + words, otherY, 1, field, ctx) != 0;
	} else {
		if (KrmEccIsPointAtInfinity(left, curve, ctx) ||
		    KrmEccIsPointAtInfinity(right, curve, ctx))
			return 0;
		ok = 0;
	}
	if (sign == -1) {
		oppositeY = signedY;
		signedY = otherY;
	} else if (sign != 1) {
		oppositeY = NULL;
		signedY = NULL;
		goto failed_difference;
	}
	if (!ok || !field->operations->subtract(left->x, right->x, x, 1, field, ctx)) {
failed_difference:
		if (!field->operations->isZero(x, 1, field, ctx))
			goto failed;
		status = 0;
		goto equal_x;
	}
	if (!field->operations->isZero(x, 1, field, ctx)) {
		if (!KrmEccInvertFieldElement(x, slope, field, scratch, ctx) ||
		    !field->operations->subtract(leftY, signedY, y, 1, field, ctx))
			goto failed;
		ok = KrmEccMultiplyFieldElements(slope, y, slope, 1, field, scratch, ctx) != 0;
	} else {
equal_x:
		if (field->operations->equal(leftY, oppositeY, 1, field, ctx)) {
			COPY_ECC_WORDS(result, left, 2 * curve->field->wordCount);
			return status;
		}
		if (!field->operations->equal(leftY, signedY, 1, field, ctx))
			goto failed;
		if (binary) {
			if (!status || !KrmEccInvertFieldElement(left->x, slope, field, scratch, ctx) ||
			    !KrmEccMultiplyFieldElements(leftY, slope, slope, 1, field, scratch, ctx))
				goto failed;
			for (i = 0; i < words; i++)
				slope[i] ^= left->x[i];
			ok = 1;
		} else {
			if (!status || !KrmEccMultiplyFieldElements(left->x, left->x, slope,
				1, field, scratch, ctx) ||
			    !field->operations->add(curve->a, slope, x, 1, field, ctx) ||
			    !field->operations->add(x, slope, x, 1, field, ctx) ||
			    !field->operations->add(x, slope, x, 1, field, ctx) ||
			    !field->operations->add(leftY, leftY, y, 1, field, ctx) ||
			    !KrmEccInvertFieldElement(y, slope, field, scratch, ctx))
				goto failed;
			ok = KrmEccMultiplyFieldElements(x, slope, slope, 1, field, scratch, ctx) != 0;
		}
	}
	if (ok)
		ok = KrmEccMultiplyFieldElements(slope, slope, x, 1, field, scratch, ctx) != 0;
	if (!ok)
		goto failed;
	if (!binary) {
		if (!field->operations->subtract(x, left->x, x, 1, field, ctx) ||
		    !field->operations->subtract(x, right->x, x, 1, field, ctx))
			goto failed;
		if (field->operations->equal(x, left->x, 1, field, ctx))
			return KrmEccSetPointAtInfinity(result, curve, ctx) != 0;
		if (!field->operations->subtract(x, left->x, y, 1, field, ctx) ||
		    !KrmEccInvertFieldElement(y, otherY, field, scratch, ctx) ||
		    !KrmEccMultiplyFieldElements(leftY, otherY, otherY, 1, field, scratch, ctx) ||
		    !field->operations->add(otherY, otherY, otherY, 1, field, ctx) ||
		    !field->operations->add(otherY, slope, otherY, 1, field, ctx) ||
		    !KrmEccMultiplyFieldElements(otherY, otherY, slope, 1, field, scratch, ctx) ||
		    !field->operations->subtract(slope, x, slope, 1, field, ctx) ||
		    !field->operations->subtract(slope, left->x, x, 1, field, ctx) ||
		    !field->operations->subtract(x, left->x, y, 1, field, ctx) ||
		    !KrmEccMultiplyFieldElements(y, otherY, y, 1, field, scratch, ctx))
			goto copy_failure;
		ok = field->operations->subtract(y, leftY, resultY, 1, field, ctx) != 0;
		COPY_ECC_WORDS(result->x, x, field->wordCount);
		return ok;
	}
	for (i = 0; i < words; i++) {
		x[i] ^= slope[i] ^ curve->a[i] ^ right->x[i];
		nonzero |= x[i];
	}
	if (!nonzero)
		return KrmEccSetPointAtInfinity(result, curve, ctx) != 0;
	if (!KrmEccInvertFieldElement(x, otherY, field, scratch, ctx) ||
	    !KrmEccMultiplyFieldElements(otherY, left->x, otherY, 1, field, scratch, ctx) ||
	    !field->operations->add(otherY, slope, otherY, 1, field, ctx) ||
	    !KrmEccMultiplyFieldElements(otherY, otherY, slope, 1, field, scratch, ctx))
		return 0;
	for (i = 0; i < words; i++) {
		uint32_t originalX = left->x[i];
		uint32_t originalY = leftY[i];

		value = otherY[i] ^ slope[i] ^ curve->a[i] ^ x[i];
		x[i] = value ^ originalX;
		result->x[i] = value;
		resultY[i] = originalX ^ originalY;
	}
	if (!KrmEccMultiplyFieldElements(otherY, x, x, 1, field, scratch, ctx))
		return 0;
	return field->operations->add(resultY, x, resultY, 1, field, ctx) != 0;
failed:
	if (binary || field->operations->equal(x, left->x, 1, field, ctx))
		return 0;
copy_failure:
	COPY_ECC_WORDS(result->x, x, field->wordCount);
	return 0;
}
#undef COPY_ECC_WORDS

int KrmEccIsPointAtInfinity(const KrmEccPublicKey *point, const KrmEccCurve *curve,
	KrmEccContext *ctx)
{
	const KrmEccField *field = curve->field;
	unsigned int words = field->wordCount;
	const uint32_t *y;

	if (!field->operations->isZero(point->x, 1, field, ctx))
		return 0;
	y = point->x + words;
	if (curve->infinityUsesOne)
		return field->operations->equal(y, field->one, 1, field, ctx) != 0;
	return field->operations->isZero(y, 1, field, ctx) != 0;
}

int KrmEccNegatePoint(const KrmEccPublicKey *point, KrmEccPublicKey *result,
	const KrmEccCurve *curve, KrmEccContext *ctx)
{
	const KrmEccField *field = curve->field;
	unsigned int words = field->wordCount, bytes;
	const unsigned char *src = (const unsigned char *)point;
	unsigned char *dst = (unsigned char *)result;
	const unsigned char *end;
	const uint32_t *y;
	uint32_t *resultY;
	int ret, infinity;

	infinity = field->operations->isZero(point->x, 1, field, ctx);
	y = point->x + words;
	resultY = result->x + words;
	if (infinity &&
	    (curve->infinityUsesOne ? field->operations->equal(y, field->one, 1, field, ctx) :
		field->operations->isZero(y, 1, field, ctx))) {
		bytes = 4 * field->wordCount;
		src = (const unsigned char *)y;
		dst = (unsigned char *)resultY;
		end = src + bytes;
		if (bytes) {
			do {
				*dst++ = *src++;
			} while (src != end);
		}
		ret = 1;
	} else if (field->flags > 1) {
		ret = field->operations->add(point->x, y, resultY, 1, field, ctx);
	} else {
		ret = field->operations->negate(y, resultY, 1, field, ctx);
	}
	bytes = 4 * field->wordCount;
	src = (const unsigned char *)point;
	dst = (unsigned char *)result;
	end = src + bytes;
	if (bytes) {
		do {
			*dst++ = *src++;
		} while (src != end);
	}
	return ret;
}

int KrmEccCopySignedPoint(const KrmEccPublicKey *point, KrmEccPublicKey *result,
	int sign, const KrmEccCurve *curve, KrmEccContext *ctx)
{
	const unsigned char *src = (const unsigned char *)point;
	unsigned char *dst = (unsigned char *)result;
	const unsigned char *end;
	unsigned int bytes;

	if (sign != 1)
		return sign == -1 && KrmEccNegatePoint(point, result, curve, ctx) != 0;
	bytes = 8 * curve->field->wordCount;
	end = src + bytes;
	if (bytes) {
		do {
			*dst++ = *src++;
		} while (src != end);
	}
	return 1;
}

int KrmEccAddPoints(const KrmEccPublicKey *left, const KrmEccPublicKey *right,
	KrmEccPublicKey *result, int sign, const KrmEccCurve *curve,
	void *workspace, KrmEccContext *ctx)
{
	const KrmEccField *field = curve->field;
	unsigned int words = field->wordCount, total, temporaryWords, i;
	int binary = field->flags > 1, allocated = 0;
	_Bool ok;
	uint32_t *slope = workspace, *x, *y;
	void *scratch;
	const uint32_t *leftY = left->x + words;
	const uint32_t *rightY = right->x + words;
	uint32_t *resultY = result->x + words;
	uint32_t coordinate, ordinate;
	unsigned char *dst;
	const unsigned char *src;
	const KrmEccField *pointField;
	unsigned int pointWords;
	int infinity;

	if (!workspace) {
		if (words > 2 * words || 3 * words < 2 * words)
			return 0;
		temporaryWords = 3 * words;
		total = temporaryWords + curve->workspaceWords;
		if (total < temporaryWords || total > temporaryWords + 4 * curve->workspaceWords)
			return 0;
		slope = KrmEccMalloc(4 * total, ctx);
		if (!slope)
			return 0;
		allocated = 1;
	}
	x = slope + words;
	y = x + words;
	scratch = y + words;
	ok = (field->flags == 1 || binary) && (sign == -1 || sign == 1);
	pointField = curve->field;
	pointWords = pointField->wordCount;
	infinity = pointField->operations->isZero(right->x, 1, pointField, ctx);
	if (infinity) {
		if (curve->infinityUsesOne)
			infinity = pointField->operations->equal(right->x + pointWords,
				pointField->one, 1, pointField, ctx);
		else
			infinity = pointField->operations->isZero(right->x + pointWords,
				1, pointField, ctx);
	}
	if (infinity) {
		dst = (unsigned char *)result;
		src = (const unsigned char *)left;
		for (i = 0, total = 8 * curve->field->wordCount; i < total; i++)
			dst[i] = src[i];
		goto out;
	}
	pointField = curve->field;
	pointWords = pointField->wordCount;
	infinity = pointField->operations->isZero(left->x, 1, pointField, ctx);
	if (infinity) {
		if (curve->infinityUsesOne)
			infinity = pointField->operations->equal(left->x + pointWords,
				pointField->one, 1, pointField, ctx);
		else
			infinity = pointField->operations->isZero(left->x + pointWords,
				1, pointField, ctx);
	}
	if (infinity) {
		if (sign == 1) {
			dst = (unsigned char *)result;
			src = (const unsigned char *)right;
			for (i = 0, total = 8 * curve->field->wordCount; i < total; i++)
				dst[i] = src[i];
		} else if (ok) {
			ok = KrmEccNegatePoint(right, result, curve, ctx) != 0;
		}
		goto out;
	}
	if (sign == -1) {
		dst = (unsigned char *)x;
		src = (const unsigned char *)rightY;
		for (i = 0; i < 4 * words; i++)
			dst[i] = src[i];
		if (!ok)
			goto out;
	} else if (!ok || !(binary ? field->operations->add(right->x, rightY, x, 1, field, ctx) :
		field->operations->negate(rightY, x, 1, field, ctx))) {
		goto failed;
	}
	if (!field->operations->equal(left->x, right->x, 1, field, ctx)) {
		if (!field->operations->add(leftY, x, y, 1, field, ctx) ||
		    (binary && !field->operations->add(y, right->x, y, 1, field, ctx)) ||
		    !field->operations->subtract(left->x, right->x, x, 1, field, ctx) ||
		    !KrmEccInvertFieldElement(x, slope, field, scratch, ctx) ||
		    !KrmEccMultiplyFieldElements(y, slope, slope, 1, field, scratch, ctx))
			goto failed;
	} else {
		if (field->operations->equal(leftY, x, 1, field, ctx)) {
			ok = field->operations->zero(result->x, 2, field, ctx) != 0;
			if (curve->infinityUsesOne) {
				dst = (unsigned char *)resultY;
				src = (const unsigned char *)field->one;
				for (i = 0; i < 4 * words; i++)
					dst[i] = src[i];
			}
			goto out;
		}
		if (!binary) {
			if (!field->operations->add(leftY, leftY, x, 1, field, ctx) ||
			    !KrmEccMultiplyFieldElements(left->x, left->x, slope, 1, field, scratch, ctx) ||
			    !field->operations->add(slope, curve->a, y, 1, field, ctx) ||
			    !field->operations->add(slope, y, y, 1, field, ctx) ||
			    !field->operations->add(slope, y, y, 1, field, ctx) ||
			    !KrmEccInvertFieldElement(x, slope, field, scratch, ctx) ||
			    !KrmEccMultiplyFieldElements(y, slope, slope, 1, field, scratch, ctx))
				goto failed;
		} else if (!KrmEccInvertFieldElement(left->x, slope, field, scratch, ctx) ||
			!KrmEccMultiplyFieldElements(leftY, slope, slope, 1, field, scratch, ctx) ||
			!field->operations->add(left->x, slope, slope, 1, field, ctx)) {
			goto failed;
		}
	}
	if (!binary) {
		ok = KrmEccMultiplyFieldElements(slope, slope, x, 1, field, scratch, ctx) &&
			field->operations->subtract(x, left->x, x, 1, field, ctx) &&
			field->operations->subtract(x, right->x, x, 1, field, ctx) &&
			field->operations->subtract(left->x, x, y, 1, field, ctx);
		dst = (unsigned char *)result;
		src = (const unsigned char *)x;
		for (i = 0; i < 4 * words; i++)
			dst[i] = src[i];
		if (!ok || !KrmEccMultiplyFieldElements(slope, y, x, 1, field, scratch, ctx))
			goto failed;
		ok = field->operations->subtract(x, leftY, resultY, 1, field, ctx) != 0;
	} else {
		ok = KrmEccMultiplyFieldElements(slope, slope, x, 1, field, scratch, ctx);
		i = 0;
		if (words > 3 &&
		    ((uintptr_t)x >= (uintptr_t)(slope + 4) ||
		     (uintptr_t)slope >= (uintptr_t)(x + 4)) &&
		    ((uintptr_t)x >= (uintptr_t)(right->x + 4) ||
		     (uintptr_t)right->x >= (uintptr_t)(x + 4)) &&
		    ((uintptr_t)x >= (uintptr_t)(curve->a + 4) ||
		     (uintptr_t)curve->a >= (uintptr_t)(x + 4))) {
			unsigned int groups = words / 4;

			do {
				*(BinaryFieldVector *)(x + i) ^=
					*(const BinaryFieldVector *)(slope + i) ^
					*(const BinaryFieldVector *)(curve->a + i) ^
					*(const BinaryFieldVector *)(right->x + i);
				i += 4;
			} while (--groups);
		}
		if (words > 6 && i + 1 < words - 7) {
			do {
				__builtin_prefetch(x + i + 13);
				__builtin_prefetch(slope + i + 13);
				x[i] ^= slope[i] ^ curve->a[i] ^ right->x[i];
				x[i + 1] ^= slope[i + 1] ^ curve->a[i + 1] ^ right->x[i + 1];
				x[i + 2] ^= slope[i + 2] ^ curve->a[i + 2] ^ right->x[i + 2];
				x[i + 3] ^= slope[i + 3] ^ curve->a[i + 3] ^ right->x[i + 3];
				x[i + 4] ^= slope[i + 4] ^ curve->a[i + 4] ^ right->x[i + 4];
				x[i + 5] ^= slope[i + 5] ^ curve->a[i + 5] ^ right->x[i + 5];
				x[i + 6] ^= slope[i + 6] ^ curve->a[i + 6] ^ right->x[i + 6];
				x[i + 7] ^= slope[i + 7] ^ curve->a[i + 7] ^ right->x[i + 7];
				i += 8;
			} while (i + 1 < words - 7);
		}
		for (; i < words; i++)
			x[i] ^= slope[i] ^ curve->a[i] ^ right->x[i];
		if (ok)
			ok = KrmEccMultiplyFieldElements(slope, x, y, 1, field, scratch, ctx) != 0;
		for (i = 0; i < words; i++) {
			coordinate = left->x[i] ^ x[i];
			ordinate = leftY[i] ^ y[i] ^ coordinate;
			resultY[i] = ordinate;
			result->x[i] = coordinate;
		}
	}
	goto out;
failed:
	ok = 0;
out:
	if (allocated)
		KrmEccFree(slope, ctx);
	return ok;
}

int KrmEccAddSubtractPoints(const KrmEccPublicKey *left,
	const KrmEccPublicKey *right, KrmEccPublicKey *sum, KrmEccPublicKey *difference,
	const KrmEccCurve *curve, void *workspace, KrmEccContext *ctx)
{
	const KrmEccField *field = curve->field;
	unsigned int words = field->wordCount, i;
	int binary = field->flags > 1, ok;
	uint32_t *sumX = workspace, *sumSlope, *differenceX, *differenceSlope, *inverse;
	void *scratch;
	const uint32_t *leftY = left->x + words, *rightY = right->x + words;
	uint32_t sx, sy, dx, dy, originalY, common;
	unsigned char *dst;
	const unsigned char *src;
	const KrmEccField *pointField;
	unsigned int pointWords, bytes;
	int infinity;

	if (!field->flags || !workspace)
		return 0;
	pointField = field;
	pointWords = words;
	infinity = pointField->operations->isZero(left->x, 1, pointField, ctx);
	if (infinity)
		infinity = curve->infinityUsesOne ?
			pointField->operations->equal(left->x + pointWords, pointField->one,
				1, pointField, ctx) :
			pointField->operations->isZero(left->x + pointWords, 1, pointField, ctx);
	if (!infinity) {
		pointField = curve->field;
		pointWords = pointField->wordCount;
		infinity = pointField->operations->isZero(right->x, 1, pointField, ctx);
		if (infinity)
			infinity = curve->infinityUsesOne ?
				pointField->operations->equal(right->x + pointWords, pointField->one,
					1, pointField, ctx) :
				pointField->operations->isZero(right->x + pointWords, 1, pointField, ctx);
	}
	if (infinity || field->operations->equal(left->x, right->x, 1, field, ctx)) {
		ok = KrmEccAddPoints(left, right, workspace, 1, curve,
			sumX + 2 * words, ctx);
		if (ok)
			ok = KrmEccAddPoints(left, right, difference, -1, curve,
				sumX + 2 * words, ctx) != 0;
		dst = (unsigned char *)sum;
		src = workspace;
		for (i = 0, bytes = 8 * curve->field->wordCount; i < bytes; i++)
			dst[i] = src[i];
		return ok;
	}
	sumSlope = sumX + words;
	differenceX = sumSlope + words;
	differenceSlope = differenceX + words;
	inverse = differenceSlope + words;
	scratch = inverse + words;
	if (!field->operations->subtract(left->x, right->x, sumX, 1, field, ctx))
		return 0;
	ok = KrmEccInvertFieldElement(sumX, inverse, field, scratch, ctx) != 0;
	if (binary) {
		if (!ok)
			return 0;
		for (i = 0; i < words; i++) {
			common = rightY[i] ^ leftY[i];
			sumSlope[i] = common;
			differenceSlope[i] = right->x[i] ^ common;
		}
	} else if (!ok ||
		!field->operations->subtract(leftY, rightY, sumSlope, 1, field, ctx) ||
		!field->operations->add(leftY, rightY, differenceSlope, 1, field, ctx)) {
		return 0;
	}
	if (!KrmEccMultiplyFieldElements(inverse, sumSlope, sumSlope, 1, field, scratch, ctx) ||
	    !KrmEccMultiplyFieldElements(inverse, differenceSlope, differenceSlope, 1, field, scratch, ctx) ||
	    !KrmEccMultiplyFieldElements(sumSlope, sumSlope, sumX, 1, field, scratch, ctx) ||
	    !KrmEccMultiplyFieldElements(differenceSlope, differenceSlope, differenceX, 1, field, scratch, ctx))
		return 0;
	if (binary) {
		for (i = 0; i < words; i++) {
			common = left->x[i] ^ curve->a[i] ^ right->x[i];
			sumX[i] ^= sumSlope[i] ^ common;
			differenceX[i] ^= differenceSlope[i] ^ common;
		}
	} else if (!field->operations->add(left->x, right->x, inverse, 1, field, ctx) ||
		!field->operations->subtract(sumX, inverse, sumX, 1, field, ctx) ||
		!field->operations->subtract(differenceX, inverse, differenceX, 1, field, ctx)) {
		return 0;
	}
	if (!field->operations->subtract(left->x, sumX, inverse, 1, field, ctx) ||
	    !KrmEccMultiplyFieldElements(sumSlope, inverse, sumSlope, 1, field, scratch, ctx) ||
	    !field->operations->subtract(left->x, differenceX, inverse, 1, field, ctx) ||
	    !KrmEccMultiplyFieldElements(differenceSlope, inverse, differenceSlope, 1, field, scratch, ctx))
		return 0;
	if (!binary) {
		if (!field->operations->subtract(sumSlope, leftY, sumSlope, 1, field, ctx) ||
		    !field->operations->subtract(differenceSlope, leftY, differenceSlope, 1, field, ctx))
			return 0;
		for (i = 0; i < words; i++) {
			sx = sumX[i];
			sy = sumSlope[i];
			dx = differenceX[i];
			dy = differenceSlope[i];
			sum->x[i] = sx;
			sum->x[words + i] = sy;
			difference->x[i] = dx;
			difference->x[words + i] = dy;
		}
		return 1;
	}
	for (i = 0; i < words; i++) {
		sx = sumX[i];
		sy = sumSlope[i];
		dx = differenceX[i];
		dy = differenceSlope[i];
		originalY = leftY[i];
		sum->x[i] = sx;
		sum->x[words + i] = sx ^ originalY ^ sy;
		difference->x[i] = dx;
		difference->x[words + i] = dy ^ originalY ^ dx;
	}
	return 1;
}

int KrmEccIsPointOnCurve(const KrmEccPublicKey *point, const KrmEccCurve *curve,
	int flags, void *workspace, KrmEccContext *ctx)
{
	const KrmEccField *field = curve->field;
	unsigned int words = field->wordCount;
	const uint32_t *y = point->x + words;
	KrmEccWorkspace local;
	uint32_t *left, *right, *scratch;
	int ok = 0;

	(void)flags;
	if (field->operations->isZero(point->x, 1, field, ctx) &&
	    (curve->infinityUsesOne ? field->operations->equal(y, field->one, 1, field, ctx) :
	     field->operations->isZero(y, 1, field, ctx)))
		return 1;
	local.data = workspace;
	local.wordCount = curve->workspaceWords;
	local.allocated = 0;
	if (!KrmEccEnsureWorkspace(&local, ctx))
		goto out;
	left = local.data;
	right = left + words;
	scratch = right + words;
	if (!KrmEccMultiplyFieldElements(point->x, point->x, left, 1, field, scratch, ctx))
		goto out;
	if (field->flags <= 1) {
		if (!field->operations->add(left, curve->a, left, 1, field, ctx) ||
		    !KrmEccMultiplyFieldElements(point->x, left, left, 1, field, scratch, ctx) ||
		    !field->operations->add(left, curve->b, left, 1, field, ctx) ||
		    !KrmEccMultiplyFieldElements(y, y, right, 1, field, scratch, ctx))
			goto out;
	} else {
		if (!field->operations->add(point->x, curve->a, right, 1, field, ctx) ||
		    !KrmEccMultiplyFieldElements(left, right, left, 1, field, scratch, ctx) ||
		    !field->operations->add(left, curve->b, left, 1, field, ctx) ||
		    !field->operations->add(y, point->x, right, 1, field, ctx) ||
		    !KrmEccMultiplyFieldElements(y, right, right, 1, field, scratch, ctx))
			goto out;
	}
	ok = field->operations->equal(left, right, 1, field, ctx) != 0;
out:
	if (local.allocated)
		KrmEccFree(local.data, ctx);
	return ok;
}

int KrmEccSetPointAtInfinity(KrmEccPublicKey *point, const KrmEccCurve *curve,
	KrmEccContext *ctx)
{
	const KrmEccField *field = curve->field;
	unsigned int words = field->wordCount, bytes;
	unsigned char *destination;
	const unsigned char *source, *end;
	int ok = field->operations->zero(point->x, 2, field, ctx) != 0;

	if (curve->infinityUsesOne) {
		destination = (unsigned char *)(point->x + words);
		source = (const unsigned char *)field->one;
		bytes = 4 * field->wordCount;
		if (bytes) {
			end = source + bytes;
			do {
				*destination++ = *source++;
			} while (source != end);
		}
	}
	return ok;
}

int KrmEccMultiplyFieldElements(const uint32_t *left, const uint32_t *right,
	uint32_t *result, unsigned int count, const KrmEccField *field,
	void *workspace, KrmEccContext *ctx)
{
	KrmEccWorkspace local = { workspace, field->productWords, 0 };
	unsigned int words = field->wordCount, offset, total;
	int ok = 0;

	if (!KrmEccEnsureWorkspace(&local, ctx))
		goto out;
	total = words * count;
	if (total) {
		offset = words;
		do {
			int done;

			if (!field->operations->multiply(left, right, result,
				field, local.data, ctx))
				goto out;
			done = offset == total;
			offset += words;
			left += words;
			right += words;
			result += words;
			if (done)
				break;
		} while (1);
	}
	ok = 1;
out:
	if (local.allocated)
		KrmEccFree(local.data, ctx);
	return ok;
}

int KrmEccFieldEqual(const uint32_t *left, const uint32_t *right,
	unsigned int count, const KrmEccField *field, KrmEccContext *ctx)
{
	(void)ctx;
	return KrmEccCompareIntegers(left, right, field->wordCount * count) == 0;
}

int KrmEccFieldIsZero(const uint32_t *value, unsigned int count,
	const KrmEccField *field, KrmEccContext *ctx)
{
	unsigned int words = field->wordCount * count;

	(void)ctx;
	while (words) {
		if (value[words - 1])
			return 0;
		words--;
	}
	return 1;
}

int KrmEccFieldZero(uint32_t *result, unsigned int count,
	const KrmEccField *field, KrmEccContext *ctx)
{
	unsigned char *cursor = (unsigned char *)result;
	unsigned int bytes = 4 * field->wordCount * count;
	unsigned char *end;

	(void)ctx;
	if (!bytes)
		return 1;
	while (bytes - 1 > 31) {
		cursor[0] = 0;
		__builtin_prefetch(cursor + 40);
		cursor[1] = 0;
		cursor[2] = 0;
		cursor += 32;
		cursor[-29] = 0;
		cursor[-28] = 0;
		cursor[-27] = 0;
		cursor[-26] = 0;
		cursor[-25] = 0;
		cursor[-24] = 0;
		cursor[-23] = 0;
		cursor[-22] = 0;
		cursor[-21] = 0;
		cursor[-20] = 0;
		cursor[-19] = 0;
		cursor[-18] = 0;
		cursor[-17] = 0;
		cursor[-16] = 0;
		cursor[-15] = 0;
		cursor[-14] = 0;
		cursor[-13] = 0;
		cursor[-12] = 0;
		cursor[-11] = 0;
		cursor[-10] = 0;
		cursor[-9] = 0;
		cursor[-8] = 0;
		cursor[-7] = 0;
		cursor[-6] = 0;
		cursor[-5] = 0;
		cursor[-4] = 0;
		cursor[-3] = 0;
		cursor[-2] = 0;
		cursor[-1] = 0;
		bytes -= 32;
	}
	end = cursor + bytes;
	if (bytes) {
		do {
			*cursor++ = 0;
		} while (cursor != end);
	}
	return 1;
}

int KrmEccFieldCopyModulus(uint32_t *result, const KrmEccField *field)
{
	unsigned int words = field->wordCount;
	unsigned int copyWords = words + 1, bytes;
	const unsigned char *end;
	unsigned char *destination = (unsigned char *)result;
	const unsigned char *source = (const unsigned char *)field->modulus->modulus;

	if (copyWords > words) {
		bytes = 4 * words;
		if (bytes) {
			end = source + bytes;
			do {
				*destination++ = *source++;
			} while (source != end);
		}
		destination = (unsigned char *)(result + words);
		destination[0] = 0;
		destination[1] = 0;
		destination[2] = 0;
		destination[3] = 0;
	} else {
		bytes = 4 * copyWords;
		if (bytes) {
			end = source + bytes;
			do {
				*destination++ = *source++;
			} while (source != end);
		}
	}
	return 1;
}

int KrmEccSubtractModulo(const uint32_t *left, const uint32_t *right,
	uint32_t *result, const uint32_t *modulus, unsigned int words);
int KrmEccNegateModulo(const uint32_t *value, uint32_t *result,
	const uint32_t *modulus, unsigned int words);
int KrmEccMultiplyModulo(const uint32_t *left, const uint32_t *right,
	uint32_t *result, const KrmEccModulus *modulus, void *workspace, KrmEccContext *ctx);
int KrmEccShiftModulo(const uint32_t *value, int shift, uint32_t *result,
	const KrmEccModulus *modulus);

int KrmEccFieldSubtract(const uint32_t *left, const uint32_t *right,
	uint32_t *result, unsigned int count, const KrmEccField *field, KrmEccContext *ctx)
{
	int offset = 0, words = field->wordCount;
	unsigned int total = words * count;

	(void)ctx;
	while (1) {
		const uint32_t *fieldLeft = left + offset;
		const uint32_t *fieldRight = right + offset;
		uint32_t *fieldResult = result + offset;
		int done = offset == total;

		offset += words;
		if (done)
			break;
		if (!KrmEccSubtractModulo(fieldLeft, fieldRight, fieldResult,
			field->modulus->modulus, field->modulus->wordCount))
			return 0;
	}
	return 1;
}

int KrmEccFieldNegate(const uint32_t *value, uint32_t *result,
	unsigned int count, const KrmEccField *field, KrmEccContext *ctx)
{
	unsigned int offset = 0, words = field->wordCount, total = words * count;

	(void)ctx;
	while (1) {
		const uint32_t *fieldValue = value;
		uint32_t *fieldResult = result;
		int done = offset == total;

		value += words;
		result += words;
		offset += words;
		if (done)
			break;
		if (!KrmEccNegateModulo(fieldValue, fieldResult,
			field->modulus->modulus, field->modulus->wordCount))
			return 0;
	}
	return 1;
}

int KrmEccFieldMultiply(const uint32_t *left, const uint32_t *right,
	uint32_t *result, const KrmEccField *field, void *workspace, KrmEccContext *ctx)
{
	return KrmEccMultiplyModulo(left, right, result, field->modulus, workspace, ctx) != 0;
}

int KrmEccFieldShift(const uint32_t *value, int shift, uint32_t *result,
	unsigned int count, const KrmEccField *field, KrmEccContext *ctx)
{
	unsigned int offset, words = field->wordCount, total = words * count;
	_Bool ok = 1;

	(void)ctx;
	if (!total)
		return 1;
	offset = words;
	do {
		int done;

		if (ok)
			ok = KrmEccShiftModulo(value, shift, result, field->modulus) != 0;
		done = offset == total;
		value += words;
		result += words;
		offset += words;
		if (done)
			break;
	} while (1);
	return ok;
}

int KrmEccInverseModulo(const uint32_t *value, const uint32_t *modulus,
	unsigned int words, uint32_t *result, const uint32_t *coefficientInfo,
	void *workspace, KrmEccContext *ctx);

int KrmEccFieldInverse(const uint32_t *value, uint32_t *result,
	const KrmEccField *field, void *workspace, KrmEccContext *ctx)
{
	KrmEccWorkspace *local = workspace;
	unsigned int words = field->wordCount;
	const KrmEccModulus *modulus = field->modulus;
	uint32_t *temporary = local->data;
	uint32_t *scratch = temporary + words;

	if (modulus->montgomeryEnabled != 1)
		return KrmEccInverseModulo(value, modulus->modulus, words, result,
			&g_krmEccInverseCoefficientInfo, scratch, ctx) != 0;
	if (!KrmEccMultiplyFieldElements(value, field->reductionCoefficients,
		temporary, 1, field, scratch, ctx))
		return 0;
	return KrmEccInverseModulo(temporary, modulus->modulus, words, result,
		&g_krmEccInverseCoefficientInfo, scratch, ctx) != 0;
}

int KrmEccFieldSetIntegers(const int32_t *values, uint32_t *result,
	unsigned int count, const KrmEccField *field, KrmEccContext *ctx)
{
	unsigned int i = 0, words = field->wordCount;
	uint32_t magnitude;
	int32_t value;
	int ok = 1;

	if (!count)
		return 1;
	do {
		value = values[i];
		magnitude = __builtin_abs(value);
		if (!KrmEccConvertInteger(&magnitude, 1, result, field->modulus, ctx))
			return 0;
		if (value < 0)
			ok = KrmEccNegateModulo(result, result, field->modulus->modulus,
				field->modulus->wordCount) != 0;
		else
			ok = 1;
		i++;
		result += words;
	} while (i != count && ok);
	return ok;
}

int KrmEccAddModulo(const uint32_t *left, const uint32_t *right,
	uint32_t *result, const uint32_t *modulus, unsigned int words);
intptr_t KrmEccReleaseModulus(const KrmEccModulus *modulus, KrmEccContext *ctx);
int KrmEccSquareRootModulo(const uint32_t *value, uint32_t *result,
	const KrmEccModulus *modulus, int *hasRoot, KrmEccContext *ctx);

int KrmEccFieldAdd(const uint32_t *left, const uint32_t *right,
	uint32_t *result, unsigned int count, const KrmEccField *field, KrmEccContext *ctx)
{
	int offset = 0, words = field->wordCount;
	unsigned int total = words * count;

	(void)ctx;
	while (1) {
		const uint32_t *fieldLeft = left + offset;
		const uint32_t *fieldRight = right + offset;
		uint32_t *fieldResult = result + offset;
		int done = offset == total;

		offset += words;
		if (done)
			break;
		if (!KrmEccAddModulo(fieldLeft, fieldRight, fieldResult,
			field->modulus->modulus, field->modulus->wordCount))
			return 0;
	}
	return 1;
}

int KrmEccFieldRelease(KrmEccField *field, KrmEccContext *ctx)
{
	if (field->operationArgument && field->modulus) {
		KrmEccReleaseModulus(field->modulus, ctx);
		KrmEccFree((void *)field->modulus, ctx);
		field->operationArgument = NULL;
		field->modulus = NULL;
	}
	return 1;
}

int KrmEccSquareRoot(const uint32_t *value, uint32_t *result,
	unsigned int count, const KrmEccField *field, int *hasRoot, KrmEccContext *ctx)
{
	unsigned int offset, words = field->wordCount, total = words * count;
	int root, ret, allRoots = 1;

	for (offset = 0; offset != total; offset += words) {
		root = 0;
		ret = KrmEccSquareRootModulo(value, result, field->modulus, &root, ctx);
		if (allRoots)
			allRoots = root != 0;
		value += words;
		result += words;
		if (!ret)
			return 0;
	}
	*hasRoot = allRoots;
	return 1;
}

int KrmEccInvertFieldElement(const uint32_t *value, uint32_t *result,
	const KrmEccField *field, void *workspace, KrmEccContext *ctx)
{
	KrmEccWorkspace local = { workspace, field->multiplyWorkspaceWords, 0 };
	int ok = !field->operations->isZero(value, 1, field, ctx) &&
		KrmEccEnsureWorkspace(&local, ctx) &&
		field->operations->inverse(value, result, field, &local, ctx) != 0;

	if (local.allocated)
		KrmEccFree(local.data, ctx);
	return ok;
}

int KrmEccMultiplyAdd(const uint32_t *left, const uint32_t *right,
	const uint32_t *addend, uint32_t *result, const KrmEccField *field,
	void *workspace, KrmEccContext *ctx)
{
	unsigned int words = field->wordCount;
	KrmEccWorkspace local = { workspace, words + field->productWords, 0 };
	uint32_t *product;
	int ok = 0;

	if (!KrmEccEnsureWorkspace(&local, ctx))
		goto out;
	product = local.data;
	if (KrmEccMultiplyFieldElements(left, right, product, 1, field,
		product + words, ctx))
		ok = field->operations->add(product, addend, result, 1, field, ctx) != 0;
out:
	if (local.allocated)
		KrmEccFree(local.data, ctx);
	return ok;
}

int KrmEccMontgomeryMultiply256(const uint32_t *left, const uint32_t *right,
	uint32_t *result, const KrmEccModulus *modulus, void *workspace)
{
	uint32_t product[9] = { 0 }, reduction[9] = { 0 };
	uint32_t factor;
	uint64_t carry, value;
	unsigned int i, j;

	(void)workspace;
	for (i = 0; i < 8; i++) {
		carry = 0;
		for (j = 0; j < 8; j++) {
			value = (uint64_t)left[j] * right[i] + product[j] + carry;
			product[j] = value;
			carry = value >> 32;
		}
		product[8] = carry;
		factor = modulus->montgomeryInverse * (product[0] - reduction[0]);
		carry = 0;
		for (j = 0; j < 8; j++) {
			value = (uint64_t)modulus->modulus[j] * factor + reduction[j] + carry;
			reduction[j] = value;
			carry = value >> 32;
		}
		reduction[8] = carry;
		for (j = 0; j < 8; j++) {
			product[j] = product[j + 1];
			reduction[j] = reduction[j + 1];
		}
	}
	return KrmEccSubtractModulo(product, reduction, result, modulus->modulus, 8) != 0;
}

int KrmEccRandomBelow(const uint32_t *limit, uint32_t *result,
	unsigned int words, KrmEccContext *ctx);
int KrmEccLucasSequence(const uint32_t *p, const uint32_t *q,
	const uint32_t *exponent, unsigned int words, uint32_t *u, uint32_t *v,
	const KrmEccModulus *modulus, KrmEccContext *ctx);
int KrmEccPowerModulo(const uint32_t *value, const uint32_t *exponent,
	unsigned int words, uint32_t *result, const KrmEccModulus *modulus, KrmEccContext *ctx);
uint32_t KrmEccAddWord(const uint32_t *value, uint32_t word,
	uint32_t *result, unsigned int words);
uint32_t KrmEccSubtractWords(const uint32_t *left, const uint32_t *right,
	uint32_t *result, unsigned int words);

int KrmEccSquareRootModulo(const uint32_t *value, uint32_t *result,
	const KrmEccModulus *modulus, int *hasRoot, KrmEccContext *ctx)
{
	unsigned int words = modulus->wordCount, allocationWords, significant, i, attempt;
	uint32_t *allocation, *half, *random, *check, *u, *v, *scratch;
	const uint32_t *prime;
	int ok = 0, root = 1, complete;



	if (!words)
		return 0;
	allocationWords = 6 * words + modulus->productWords;
	if (allocationWords <= 6 * words)
		return 0;
	allocation = KrmEccAllocateWords(allocationWords, ctx);
	if (!allocation)
		return 0;
	if (!value || !result || value == result)
		goto out;
	significant = words;
	while (significant && !value[significant - 1])
		significant--;
	if (!significant) {
		for (i = 0; i < 4 * words; i++)
			((unsigned char *)result)[i] = ((const unsigned char *)value)[i];
		ok = 1;
		goto out;
	}
	prime = modulus->modulus;
	half = allocation + words;
	random = half + words;
	check = random + words;
	u = check + words;
	v = u + words;
	scratch = v + words;
	switch (prime[0] & 7) {
	case 1:
		ok = KrmEccShiftModulo(value, -2, allocation, modulus) &&
			KrmEccShiftInteger(modulus->modulus, -1, half, words);
		half[0] |= 1;
		complete = 0;
		attempt = 0;
		while (ok && !complete) {
			if (++attempt <= 100 && KrmEccRandomBelow(modulus->modulus, random, words, ctx) &&
			    KrmEccLucasSequence(random, allocation, half, words, u, v, modulus, ctx) &&
			    KrmEccMultiplyModulo(v, v, check, modulus, scratch, ctx)) {
				if (!KrmEccCompareIntegers(check, value, words)) {
					for (i = 0; i < 4 * words; i++)
						((unsigned char *)result)[i] = ((const unsigned char *)v)[i];
					complete = 1;
					root = 1;
				} else {
					KrmEccSubtractWords(modulus->modulus, modulus->one, random, words);
					if (KrmEccCompareIntegers(u, modulus->one, words) &&
					    KrmEccCompareIntegers(u, random, words)) {
						complete = 1;
						root = 0;
					}
				}
			} else {
				ok = 0;
			}
		}
		goto out;
	case 3:
	case 7:
		if (!KrmEccShiftInteger(prime, -2, half, words))
			goto out;
		KrmEccAddWord(half, 1, half, words);
		if (!KrmEccPowerModulo(value, half, words, result, modulus, ctx))
			goto out;
		break;
	case 5:
		if (!KrmEccShiftInteger(prime, -3, half, words) ||
		    !KrmEccAddModulo(value, value, check, modulus->modulus, modulus->wordCount) ||
		    !KrmEccPowerModulo(check, half, words, random, modulus, ctx) ||
		    !KrmEccMultiplyModulo(value, random, check, modulus, scratch, ctx) ||
		    !KrmEccMultiplyModulo(check, random, random, modulus, scratch, ctx) ||
		    !KrmEccAddModulo(random, random, random, modulus->modulus, modulus->wordCount) ||
		    !KrmEccSubtractModulo(random, modulus->one, random,
			modulus->modulus, modulus->wordCount) ||
		    !KrmEccMultiplyModulo(check, random, result, modulus, scratch, ctx))
			goto out;
		break;
	default:
		if (KrmEccCompareWord(prime, 2, words))
			goto out;
		for (i = 0; i < 4 * words; i++)
			((unsigned char *)result)[i] = ((const unsigned char *)value)[i];
		break;
	}
	if (!KrmEccMultiplyModulo(result, result, check, modulus, scratch, ctx))
		goto out;
	root = KrmEccCompareIntegers(check, value, words) == 0;
	ok = 1;
out:
	KrmEccFree(allocation, ctx);
	if (ok)
		*hasRoot = root;
	return ok;
}

int KrmEccIsReduced(const uint32_t *value, const uint32_t *modulus,
	unsigned int words);

int KrmEccMultiplyModulo(const uint32_t *left, const uint32_t *right,
	uint32_t *result, const KrmEccModulus *modulus, void *workspace, KrmEccContext *ctx)
{
	KrmEccWorkspace local = { workspace, modulus->productWords, 0 };
	int ok = KrmEccIsReduced(left, modulus->modulus, modulus->wordCount) != 0;

	if (left != right && ok)
		ok = KrmEccIsReduced(right, modulus->modulus, modulus->wordCount);
	if (ok && KrmEccEnsureWorkspace(&local, ctx))
		ok = modulus->multiply(left, right, result, modulus, local.data) != 0;
	else
		ok = 0;
	if (local.allocated)
		KrmEccFree(local.data, ctx);
	return ok;
}

int KrmEccShiftModulo(const uint32_t *value, int shift, uint32_t *result,
	const KrmEccModulus *modulus)
{
	unsigned int words = modulus->wordCount;
	uint32_t overflow, borrow, quotient, low, carry;
	int step, ok = 1;

	if (value != result && 4 * words) {
		const unsigned char *src = (const unsigned char *)value;
		const unsigned char *end = src + 4 * words;
		unsigned char *dst = (unsigned char *)result;

		do
			*dst++ = *src++;
		while (src != end);
	}
	if (!KrmEccIsReduced(value, modulus->modulus, words) ||
	    (shift < 0 && !(modulus->modulus[0] & 1)))
		return 0;
	while (shift > 0 && ok) {
		step = shift >= 32 ? 32 : shift;
		overflow = 0;
		if (!KrmEccShiftWord(result, step, result, words, &overflow))
			return 0;
		low = words > 1 ? result[words - 2] : 0;
		quotient = KrmEccEstimateQuotient(overflow, result[words - 1], low,
			&modulus->quotientReciprocal);
		overflow -= KrmEccMultiplySubtractWord(modulus->modulus, quotient, result, words);
		if (overflow || KrmEccCompareIntegers(result, modulus->modulus, words) >= 0)
			borrow = overflow - KrmEccSubtractWords(result, modulus->modulus, result, words);
		else
			borrow = overflow;
		shift -= step;
		ok = borrow == 0;
	}
	while (shift < 0 && ok) {
		step = -shift >= 32 ? 32 : -shift;
		overflow = 0;
		carry = KrmEccMultiplyAddWord(modulus->modulus,
			(0xffffffffU >> (32 - step)) & (0U - modulus->montgomeryInverse * result[0]),
			result, words);
		ok = KrmEccShiftWord(result, -step, result, words, &overflow);
		if (!ok) {
			result[words - 1] |= carry << (32 - step);
			return 0;
		}
		shift += step;
		ok = overflow == 0;
		result[words - 1] |= carry << (32 - step);
		if (!ok)
			break;
	}
	return ok;
}

int KrmEccConvertFromMontgomery(const uint32_t *value, uint32_t *result,
	const KrmEccModulus *modulus)
{
	int ok = KrmEccIsReduced(value, modulus->modulus, modulus->wordCount);

	if (ok)
		return KrmEccShiftModulo(value, -(int)modulus->radixBits, result, modulus) != 0;
	return ok;
}

int KrmEccConvertInteger(const uint32_t *value, unsigned int valueWords,
	uint32_t *result, const KrmEccModulus *modulus, KrmEccContext *ctx)
{
	unsigned int words = modulus->wordCount, significant = valueWords, copied, bytes;
	unsigned char *clear, *end;
	uint32_t *allocation = NULL;
	const uint32_t *source = value;
	_Bool ok;

	if (KrmEccCompareSizedIntegers(value, valueWords, modulus->modulus, words) >= 0) {
		allocation = KrmEccAllocateWords(words, ctx);
		if (!allocation)
			return 0;
		ok = KrmEccReduce(value, valueWords, modulus->modulus, words,
			&modulus->quotientReciprocal.factor, NULL, allocation);
		if (words > valueWords) {
			ok = 0;
			goto out;
		}
		if (!ok)
			goto out;
		source = allocation;
		significant = words;
	}
	/* The original trims using the input even when source is the reduced value. */
	while (significant && !value[significant - 1])
		significant--;
	if (words < significant) {
		const unsigned char *src = (const unsigned char *)source;
		const unsigned char *limit = src + 4 * words;
		unsigned char *dst = (unsigned char *)result;

		if (words) {
			do
				*dst++ = *src++;
			while (src != limit);
		}
		goto shift;
	}
	copied = significant;
	if (4 * copied) {
		const unsigned char *src = (const unsigned char *)source;
		const unsigned char *limit = src + 4 * copied;
		unsigned char *dst = (unsigned char *)result;

		do
			*dst++ = *src++;
		while (src != limit);
	}
	clear = (unsigned char *)(result + copied);
	bytes = 4 * (words - copied);
	if (bytes) {
		while (bytes - 1 > 31) {
			clear[0] = 0;
			__builtin_prefetch(clear + 40);
			clear[1] = 0;
			clear[2] = 0;
			clear += 32;
			clear[-29] = 0;
			clear[-28] = 0;
			clear[-27] = 0;
			clear[-26] = 0;
			clear[-25] = 0;
			clear[-24] = 0;
			clear[-23] = 0;
			clear[-22] = 0;
			clear[-21] = 0;
			clear[-20] = 0;
			clear[-19] = 0;
			clear[-18] = 0;
			clear[-17] = 0;
			clear[-16] = 0;
			clear[-15] = 0;
			clear[-14] = 0;
			clear[-13] = 0;
			clear[-12] = 0;
			clear[-11] = 0;
			clear[-10] = 0;
			clear[-9] = 0;
			clear[-8] = 0;
			clear[-7] = 0;
			clear[-6] = 0;
			clear[-5] = 0;
			clear[-4] = 0;
			clear[-3] = 0;
			clear[-2] = 0;
			clear[-1] = 0;
			bytes -= 32;
		}
		end = clear + bytes;
		do {
			*clear++ = 0;
		} while (clear != end);
	}
shift:
	ok = KrmEccShiftModulo(result, modulus->radixBits, result, modulus) != 0;
out:
	if (allocation)
		KrmEccFree(allocation, ctx);
	return ok;
}

__attribute__((noinline)) intptr_t KrmEccReleaseModulus(const KrmEccModulus *modulus, KrmEccContext *ctx)
{
	KrmEccModulus *owned = (KrmEccModulus *)modulus;
	intptr_t result = (intptr_t)owned->modulus;

	if (owned->modulus) {
		result = KrmEccFree((void *)owned->modulus, ctx);
		owned->modulus = NULL;
	}
	return result;
}

int KrmEccAddIntegerWords(const uint32_t *left, unsigned int leftWords,
	const uint32_t *right, unsigned int rightWords, uint32_t *result, uint32_t *carryOut)
{
	unsigned int i = 0, bytes;
	const uint32_t *leftCursor = left, *rightCursor = right;
	uint32_t *resultCursor = result;
	uint32_t carry = 0, a, b, sum;
	const unsigned char *source;
	unsigned char *destination;

#define ADD_INTEGER_WORD(n) do { \
	a = leftCursor[n]; \
	b = rightCursor[n]; \
	sum = a + b + carry; \
	resultCursor[n] = sum; \
	carry = ((sum & (a ^ b)) ^ (a | b)) >> 31; \
} while (0)
	while (rightWords > i + 8) {
		__builtin_prefetch(leftCursor + 12);
		__builtin_prefetch(rightCursor + 12);
		ADD_INTEGER_WORD(0);
		ADD_INTEGER_WORD(1);
		ADD_INTEGER_WORD(2);
		ADD_INTEGER_WORD(3);
		ADD_INTEGER_WORD(4);
		ADD_INTEGER_WORD(5);
		ADD_INTEGER_WORD(6);
		ADD_INTEGER_WORD(7);
		i += 8;
		leftCursor += 8;
		rightCursor += 8;
		resultCursor += 8;
	}
	while (i < rightWords) {
		ADD_INTEGER_WORD(0);
		i++;
		leftCursor++;
		rightCursor++;
		resultCursor++;
	}
#undef ADD_INTEGER_WORD
	while (i < leftWords) {
		a = left[i];
		sum = a + carry;
		result[i++] = sum;
		if (carry <= sum) {
			carry = 0;
			if (left != result) {
				source = (const unsigned char *)(left + i);
				destination = (unsigned char *)(result + i);
				bytes = 4 * (leftWords - i);
				while (bytes--)
					*destination++ = *source++;
			}
			break;
		}
		carry = 1;
	}
	if (carryOut) {
		*carryOut = carry;
		return 1;
	}
	return carry == 0;
}

int KrmEccAddIntegers(const uint32_t *left, unsigned int leftWords,
	const uint32_t *right, unsigned int rightWords, uint32_t *result, uint32_t *carry)
{
	if (leftWords < rightWords || ((leftWords | rightWords) & 0x80000000U))
		return 0;
	return KrmEccAddIntegerWords(left, leftWords, right, rightWords, result, carry);
}

int KrmEccAddSizedIntegers(const uint32_t *left, unsigned int leftWords,
	const uint32_t *right, unsigned int rightWords, uint32_t *result, unsigned int *resultWords)
{
	unsigned int words = leftWords;
	uint32_t carry = 0;
	int ok;

	if (leftWords < rightWords) {
		ok = KrmEccAddIntegers(right, rightWords, left, leftWords, result, &carry);
		if (!ok)
			return 0;
		words = rightWords;
	} else {
		ok = KrmEccAddIntegers(left, leftWords, right, rightWords, result, &carry);
		if (!ok)
			return 0;
	}
	if (carry)
		result[words++] = carry;
	*resultWords = words;
	return 1;
}

uint32_t KrmEccAddWord(const uint32_t *value, uint32_t word,
	uint32_t *result, unsigned int words)
{
	unsigned int i, bytes;
	uint32_t input, sum;
	const unsigned char *source;
	unsigned char *destination;

	if (__builtin_expect(!words, 0))
		return word;
	input = value[0];
	sum = word + input;
	result[0] = sum;
	i = 0;
	if (__builtin_expect(word > sum, 1)) {
		while (1) {
			if (++i == words)
				return 1;
			sum = value[i] + 1;
			result[i] = sum;
			if (sum)
				break;
		}
	}
	if (value != result) {
		source = (const unsigned char *)(value + i + 1);
		destination = (unsigned char *)(result + i + 1);
		bytes = 4 * (words - i - 1);
		while (bytes--)
			*destination++ = *source++;
	}
	return 0;
}

/* These owner helpers are inlined in the original modular add/subtract paths. */
static inline __attribute__((always_inline)) int KrmEccIsReducedInline(
	const uint32_t *value, const uint32_t *modulus, unsigned int words)
{
	const uint32_t *first = value + words, *second = modulus + words;
	uint32_t a, b;
	unsigned int remaining = words - 1;

	do {
		if (remaining-- == UINT32_MAX)
			return 0;
		a = *--first;
		b = *--second;
	} while (a == b);
	return a > b ? 0 : 1;
}

static inline __attribute__((always_inline)) uint32_t KrmEccAddWordsInline(
	const uint32_t *left, const uint32_t *right, uint32_t *result, unsigned int count);

int KrmEccAddModulo(const uint32_t *left, const uint32_t *right,
	uint32_t *result, const uint32_t *modulus, unsigned int words)
{
	unsigned int i, remaining;
	uint32_t carry = 0, borrow = 0, a, b, m, sum, difference;
	uint64_t wide;
	int reduce, comparison = 0;

	if (!words || !KrmEccIsReducedInline(left, modulus, words) ||
	    !KrmEccIsReducedInline(right, modulus, words))
		return 0;
	a = left[words - 1];
	b = right[words - 1];
	m = modulus[words - 1];
	if (b > m - a) {
		reduce = 1;
	} else if (m - a - b > 1) {
		reduce = 0;
	} else {
		remaining = words;
		while (remaining) {
			remaining--;
			wide = (uint64_t)left[remaining] + right[remaining];
			sum = wide;
			m = modulus[remaining];
			difference = sum - m;
			comparison += (int)(wide >> 32) - (sum < m);
			if (difference == (uint32_t)comparison && ((comparison + 3) & 2))
				continue;
			comparison = ((comparison + 2) & 2) - 1;
			break;
		}
		reduce = comparison >= 0;
	}
	if (!reduce)
		return KrmEccAddWordsInline(left, right, result, words) == 0;
	for (i = 0; i < words; i++) {
		a = left[i];
		b = right[i];
		m = modulus[i];
		sum = a + b + carry;
		difference = sum - m - borrow;
		carry = (((sum ^ a) | (sum ^ b)) ^ sum) >> 31;
		borrow = ((difference ^ sum | m ^ sum) ^ sum) >> 31;
		result[i] = difference;
	}
	return carry == borrow;
}

static inline __attribute__((always_inline)) uint32_t KrmEccAddWordsInline(
	const uint32_t *left, const uint32_t *right,
	uint32_t *result, unsigned int count)
{
	unsigned int i = 0;
	uint32_t carry = 0, a, b, sum;

#define ADD_WORD(n) do { \
	a = left[i + (n)]; \
	b = right[i + (n)]; \
	sum = a + b + carry; \
	result[i + (n)] = sum; \
	carry = ((sum & (a ^ b)) ^ (a | b)) >> 31; \
} while (0)
	while (count > i + 8) {
		__builtin_prefetch(left + i + 12);
		__builtin_prefetch(right + i + 12);
		ADD_WORD(0);
		ADD_WORD(1);
		ADD_WORD(2);
		ADD_WORD(3);
		ADD_WORD(4);
		ADD_WORD(5);
		ADD_WORD(6);
		ADD_WORD(7);
		i += 8;
	}
	while (i < count) {
		ADD_WORD(0);
		i++;
	}
#undef ADD_WORD
	return carry;
}

uint32_t KrmEccAddWords(const uint32_t *left, const uint32_t *right,
	uint32_t *result, unsigned int count)
{
	return KrmEccAddWordsInline(left, right, result, count);
}

int KrmEccCompareSizedIntegers(const uint32_t *left, unsigned int leftWords,
	const uint32_t *right, unsigned int rightWords)
{
	uint32_t a, b;
	const uint32_t *cursor;

	if (leftWords > rightWords) {
		if (left[leftWords - 1])
			return 1;
		cursor = left + leftWords;
		while (--leftWords > rightWords) {
			cursor--;
			if (cursor[-1])
				return 1;
		}
	}
	if (rightWords > leftWords) {
		if (right[rightWords - 1])
			return -1;
		cursor = right + rightWords;
		while (--rightWords > leftWords) {
			cursor--;
			if (cursor[-1])
				return -1;
		}
	}
	if (!leftWords)
		return 0;
	while (leftWords) {
		a = left[--leftWords];
		b = right[leftWords];
		if (a != b)
			return a > b ? 1 : -1;
	}
	return 0;
}

__attribute__((noinline)) int KrmEccCompareIntegers(const uint32_t *left,
	const uint32_t *right, unsigned int wordCount)
{
	const uint32_t *a = left + wordCount, *b = right + wordCount;
	uint32_t first, second;

	while (wordCount) {
		first = *--a;
		wordCount--;
		second = *--b;
		if (first != second) {
			if (first > second)
				return 1;
			return -1;
		}
	}
	return 0;
}

int KrmEccNegateModulo(const uint32_t *value, uint32_t *result,
	const uint32_t *modulus, unsigned int words)
{
	unsigned int i;
	uint32_t any = 0, word, borrow = 0, a, b, difference;

	i = 0;
	if (words > 3 &&
	    ((uintptr_t)result >= (uintptr_t)(value + 4) ||
	     (uintptr_t)value >= (uintptr_t)(result + 4))) {
		BinaryFieldVector vectorAny = {0, 0, 0, 0};
		BinaryFieldVector vectorWord;
		unsigned int groups = words / 4, group;

		for (group = 0; group < groups; group++) {
			vectorWord = *(const BinaryFieldVector *)(value + group * 4);
			vectorAny |= vectorWord;
			*(BinaryFieldVector *)(result + group * 4) = vectorWord;
		}
		vectorAny |= __builtin_shuffle(vectorAny,
			(BinaryFieldVector){0, 0, 0, 0}, (BinaryFieldVector){2, 3, 4, 5});
		vectorAny |= __builtin_shuffle(vectorAny,
			(BinaryFieldVector){0, 0, 0, 0}, (BinaryFieldVector){1, 2, 3, 4});
		any = vectorAny[0];
		i = groups * 4;
	}
#define COPY_OR_WORD(n) do { \
	word = value[i + (n)]; \
	result[i + (n)] = word; \
	any |= word; \
} while (0)
	if (words > 6 && i + 1 <= words && i + 1 < words - 7) {
		do {
			__builtin_prefetch(value + i + 23);
			COPY_OR_WORD(0);
			COPY_OR_WORD(1);
			COPY_OR_WORD(2);
			COPY_OR_WORD(3);
			COPY_OR_WORD(4);
			COPY_OR_WORD(5);
			COPY_OR_WORD(6);
			COPY_OR_WORD(7);
			i += 8;
		} while (i + 1 < words - 7);
	}
#undef COPY_OR_WORD
	for (; i < words; i++) {
		word = value[i];
		result[i] = word;
		any |= word;
	}
	if (!any)
		return 1;
	i = 0;
#define NEGATE_WORD(n) do { \
	a = modulus[i + (n)]; \
	b = result[i + (n)]; \
	difference = a - b - borrow; \
	result[i + (n)] = difference; \
	borrow = ((difference ^ a | b ^ a) ^ a) >> 31; \
} while (0)
	while (words > i + 8) {
		__builtin_prefetch(modulus + i + 13);
		__builtin_prefetch(result + i + 13);
		NEGATE_WORD(0);
		NEGATE_WORD(1);
		NEGATE_WORD(2);
		NEGATE_WORD(3);
		NEGATE_WORD(4);
		NEGATE_WORD(5);
		NEGATE_WORD(6);
		NEGATE_WORD(7);
		i += 8;
	}
	while (i < words) {
		NEGATE_WORD(0);
		i++;
	}
#undef NEGATE_WORD
	return borrow == 0;
}

static inline __attribute__((always_inline)) uint32_t KrmEccSubtractWordsInline(
	const uint32_t *left, const uint32_t *right, uint32_t *result, unsigned int words);

__attribute__((noinline)) int KrmEccSubtractModulo(const uint32_t *left,
	const uint32_t *right, uint32_t *result, const uint32_t *modulus, unsigned int words)
{
	unsigned int i, compareWords;
	int comparison = 0;
	uint32_t carry = 0, borrow = 0, a, b, m, sum, difference;
	int valid;

	if (!words)
		return 0;
	compareWords = words;
	do {
		compareWords--;
		if (left[compareWords] != right[compareWords]) {
			comparison = left[compareWords] > right[compareWords] ? 1 : -1;
			break;
		}
	} while (compareWords);
	if (comparison >= 0) {
		valid = KrmEccIsReducedInline(left, modulus, words);
		borrow = KrmEccSubtractWordsInline(left, right, result, words);
		return valid && borrow == 0;
	}
	valid = KrmEccIsReducedInline(right, modulus, words);
	for (i = 0; i < words; i++) {
		a = left[i];
		b = right[i];
		m = modulus[i];
		sum = a + m + carry;
		difference = sum - b - borrow;
		result[i] = difference;
		carry = (((sum ^ a) | (sum ^ m)) ^ sum) >> 31;
		borrow = ((difference ^ sum | b ^ sum) ^ sum) >> 31;
	}
	return valid && carry == borrow;
}

static inline __attribute__((always_inline)) uint32_t KrmEccSubtractWordsInline(
	const uint32_t *left, const uint32_t *right,
	uint32_t *result, unsigned int words)
{
	unsigned int i = 0;
	const uint32_t *first = left, *second = right;
	uint32_t *output = result;
	uint32_t borrow = 0, a, b, difference;

#define SUBTRACT_WORD(n) do { \
	a = first[(n)]; \
	b = second[(n)]; \
	difference = a - b - borrow; \
	output[(n)] = difference; \
	borrow = ((difference ^ a | b ^ a) ^ a) >> 31; \
} while (0)
	if (!words)
		return 0;
	if (words > 8) {
		do {
			__builtin_prefetch(first + 12);
			__builtin_prefetch(second + 12);
			first += 8;
			second += 8;
			output += 8;
			SUBTRACT_WORD(-8);
			SUBTRACT_WORD(-7);
			SUBTRACT_WORD(-6);
			SUBTRACT_WORD(-5);
			SUBTRACT_WORD(-4);
			SUBTRACT_WORD(-3);
			SUBTRACT_WORD(-2);
			SUBTRACT_WORD(-1);
			i += 8;
		} while (words - 7 > i + 1);
	}
	do {
		SUBTRACT_WORD(0);
		first++;
		second++;
		output++;
		i++;
	} while (i < words);
#undef SUBTRACT_WORD
	return borrow;
}

__attribute__((noinline)) uint32_t KrmEccSubtractWords(const uint32_t *left,
	const uint32_t *right, uint32_t *result, unsigned int words)
{
	return KrmEccSubtractWordsInline(left, right, result, words);
}

__attribute__((noinline)) int KrmEccIsReduced(const uint32_t *value,
	const uint32_t *modulus, unsigned int words)
{
	return KrmEccIsReducedInline(value, modulus, words);
}

__attribute__((noinline)) uint32_t *KrmEccAllocateWords(unsigned int count,
	KrmEccContext *ctx)
{
	if (count && count < 4 * count)
		return KrmEccMalloc(4 * count, ctx);
	return NULL;
}

__attribute__((noinline)) int KrmEccEnsureWorkspace(KrmEccWorkspace *workspace,
	KrmEccContext *ctx)
{
	unsigned int words;

	if (__builtin_expect(workspace->allocated != 0, 1))
		return 0;
	if (workspace->data)
		return 1;
	words = workspace->wordCount;
	if (!words)
		return 1;
	if (words >= 4 * words)
		return 0;
	workspace->data = KrmEccMalloc(4 * words, ctx);
	if (!workspace->data)
		return 0;
	workspace->allocated = 1;
	return 1;
}

typedef struct KrmEccGcdMatrix {
	uint32_t leftScale;
	uint32_t leftSubtract;
	uint32_t rightSubtract;
	uint32_t rightScale;
} KrmEccGcdMatrix;

typedef struct KrmEccGcdCarry {
	uint32_t left;
	uint32_t right;
} KrmEccGcdCarry;

/* Both matrix helpers return the carry pair in one aligned vector store. */
typedef unsigned int CarryVector
	__attribute__((vector_size(8), aligned(4), may_alias));

int KrmEccApplyGcdCoefficientMatrix(const KrmEccGcdMatrix *matrix,
	uint32_t *left, uint32_t *right, unsigned int words, KrmEccGcdCarry *carry);
int KrmEccApplyGcdRemainderMatrix(const KrmEccGcdMatrix *matrix,
	uint32_t *left, uint32_t *right, unsigned int words, KrmEccGcdCarry *carry);

int KrmEccExtendedGcd(const uint32_t *left, unsigned int leftWords,
	const uint32_t *right, unsigned int rightWords, uint32_t *coefficient,
	void *coefficientInfo, uint32_t *gcd, void *secondCoefficient,
	unsigned int *gcdWords, void *workspace, KrmEccContext *ctx)
{
	unsigned int maximum = leftWords > rightWords ? leftWords : rightWords;
	KrmEccWorkspace local = { workspace, 8 * maximum + 6, 0 };
	KrmEccGcdMatrix matrix;
	KrmEccGcdCarry carry;
	uint32_t *remainders[2], *coefficients[2], *product, *quotient, *terminal;
	const uint32_t *multiplicand, *multiplier;
	unsigned int lengths[2] = { leftWords, rightWords }, coefficientWords = 1;
	unsigned int i, j, side, survivor, count, quotientWords, productWords;
	unsigned int firstWords, secondWords, bits[2], shift, iterations = 0, rightSignificant;
	uint32_t high[2], low[2], q, limit, previousScale, previousSubtract, overflow;
	uint32_t temporary, a, b;
	uint64_t wide, cost;
	int ok = 0, active, proceed;
	unsigned char *destination;
	const unsigned char *source;
	unsigned int bytes, outputWords = 0;

#define GCD_PRODUCT(first, firstCount, second, secondCount, destination) do { \
	firstWords = (firstCount); \
	secondWords = (secondCount); \
	multiplicand = (first); \
	multiplier = (second); \
	if (firstWords < secondWords) { \
		count = firstWords; firstWords = secondWords; secondWords = count; \
		multiplicand = (second); multiplier = (first); \
	} \
	if (secondWords) { \
		(destination)[firstWords] = KrmEccMultiplyWord(multiplicand, multiplier[0], destination, firstWords); \
		j = 1; \
		while (secondWords > j + 8) { \
			__builtin_prefetch(multiplier + j + 8); \
			__builtin_prefetch((destination) + firstWords + j + 8); \
			(destination)[firstWords + j + 0] = KrmEccMultiplyAddWord(multiplicand, multiplier[j + 0], (destination) + j + 0, firstWords); \
			(destination)[firstWords + j + 1] = KrmEccMultiplyAddWord(multiplicand, multiplier[j + 1], (destination) + j + 1, firstWords); \
			(destination)[firstWords + j + 2] = KrmEccMultiplyAddWord(multiplicand, multiplier[j + 2], (destination) + j + 2, firstWords); \
			(destination)[firstWords + j + 3] = KrmEccMultiplyAddWord(multiplicand, multiplier[j + 3], (destination) + j + 3, firstWords); \
			(destination)[firstWords + j + 4] = KrmEccMultiplyAddWord(multiplicand, multiplier[j + 4], (destination) + j + 4, firstWords); \
			(destination)[firstWords + j + 5] = KrmEccMultiplyAddWord(multiplicand, multiplier[j + 5], (destination) + j + 5, firstWords); \
			(destination)[firstWords + j + 6] = KrmEccMultiplyAddWord(multiplicand, multiplier[j + 6], (destination) + j + 6, firstWords); \
			(destination)[firstWords + j + 7] = KrmEccMultiplyAddWord(multiplicand, multiplier[j + 7], (destination) + j + 7, firstWords); \
			j += 8; \
		} \
		for (; j < secondWords; j++) \
			(destination)[firstWords + j] = KrmEccMultiplyAddWord(multiplicand, multiplier[j], (destination) + j, firstWords); \
	} else { \
		for (j = 0; j < firstWords; j++) (destination)[j] = 0; \
	} \
} while (0)
	while (lengths[0] && !left[lengths[0] - 1])
		lengths[0]--;
	while (lengths[1] && !right[lengths[1] - 1])
		lengths[1]--;
	rightSignificant = lengths[1];
	if (!KrmEccEnsureWorkspace(&local, ctx) || !lengths[0] || !lengths[1] ||
	    !left || !right || !coefficient || !gcd || left == gcd || right == gcd ||
	    left == coefficient || right == coefficient)
		goto out;
	destination = (unsigned char *)local.data;
	bytes = 4 * local.wordCount;
	for (i = 0; i < bytes; i++)
		destination[i] = 0;
	remainders[0] = local.data + 1;
	remainders[1] = local.data + maximum + 3;
	coefficients[0] = local.data + 2 * maximum + 4;
	coefficients[1] = coefficients[0] + maximum;
	product = coefficients[1] + maximum;
	quotient = product + 2 * maximum;
	if (quotient + 2 * maximum + 2 != local.data + local.wordCount)
		goto out;
	destination = (unsigned char *)remainders[0];
	source = (const unsigned char *)left;
	for (i = 0; i < 4 * lengths[0]; i++)
		destination[i] = source[i];
	destination = (unsigned char *)remainders[1];
	source = (const unsigned char *)right;
	for (i = 0; i < 4 * lengths[1]; i++)
		destination[i] = source[i];
	coefficients[0][0] = 1;
	while (lengths[0] && lengths[1]) {
		side = KrmEccCompareSizedIntegers(remainders[1], lengths[1],
			remainders[0], lengths[0]) > 0;
		if (++iterations > 32 * (leftWords + 1 + rightWords))
			goto out;
		count = lengths[0] > lengths[1] ? lengths[0] : lengths[1];
		matrix = (KrmEccGcdMatrix){ 1, 0, 0, 1 };
		if (count == 1) {
			a = remainders[0][0];
			b = remainders[1][0];
			if (a > ~b) {
				if (side) b -= a; else a -= b;
				overflow = KrmEccAddWords(coefficients[0], coefficients[1],
					coefficients[side], coefficientWords);
				if (overflow) {
					if (rightWords <= coefficientWords)
						goto out;
					coefficients[side][coefficientWords++] = overflow;
				}
			}
			while (b) {
				if (a >= b) {
					q = a / b;
					a -= b * q;
					matrix.leftScale += matrix.rightSubtract * q;
					matrix.leftSubtract += matrix.rightScale * q;
				}
				if (!a)
					break;
				q = b / a;
				b -= a * q;
				matrix.rightSubtract += matrix.leftScale * q;
				matrix.rightScale += matrix.leftSubtract * q;
			}
			remainders[0][0] = a;
			remainders[1][0] = b;
			lengths[0] = a != 0;
			lengths[1] = b != 0;
		} else {
			for (i = 0; i < 2; i++) {
				temporary = remainders[i][lengths[i] - 1] | 1;
				bits[i] = 32 * lengths[i];
				while (!(temporary & 0x80000000U)) {
					temporary <<= 1;
					bits[i]--;
				}
			}
			if (bits[0] > bits[1] + 16 || bits[1] > bits[0] + 16) {
				productWords = coefficientWords;
				while (productWords && !coefficients[1 - side][productWords - 1])
					productWords--;
				if (!KrmEccReduce(remainders[side], lengths[side],
					remainders[1 - side], lengths[1 - side], NULL, quotient, product))
					goto out;
				quotientWords = lengths[side] + 1 - lengths[1 - side];
				while (quotientWords && !quotient[quotientWords - 1])
					quotientWords--;
				lengths[side] = lengths[1 - side];
				while (lengths[side] && !product[lengths[side] - 1])
					lengths[side]--;
				for (i = 0; i < lengths[side]; i++)
					remainders[side][i] = product[i];
				GCD_PRODUCT(quotient, quotientWords, coefficients[1 - side], productWords, product);
				productWords += quotientWords;
				while (productWords && !product[productWords - 1])
					productWords--;
				if (!KrmEccAddSizedIntegers(product, productWords, coefficients[side],
					coefficientWords, coefficients[side], &coefficientWords))
					goto out;
				continue;
			}
			shift = 32 * count - (bits[0] > bits[1] ? bits[0] : bits[1]);
			for (i = 0; i < 2; i++) {
				remainders[i][lengths[i]] = 0;
				high[i] = (remainders[i][count - 1] << shift) |
					(remainders[i][count - 2] >> 1 >> (31 - shift));
				low[i] = (remainders[i][count - 2] << shift) |
					(remainders[i][(int)count - 3] >> 1 >> (31 - shift));
			}
			active = ((high[0] | high[1]) >> 31) && high[0] && high[1];
			proceed = 1;
			while (active && proceed) {
				if (high[0] && high[0] - 1 > high[1]) {
					limit = high[1] + 2;
					if (limit > high[0] >> 2) {
						previousScale = matrix.leftScale;
						previousSubtract = matrix.leftSubtract;
						do {
							high[0] -= high[1] + (low[1] > low[0]);
							low[0] -= low[1];
							matrix.leftScale += matrix.rightSubtract;
							matrix.leftSubtract += matrix.rightScale;
							if ((matrix.leftScale | matrix.leftSubtract) & 0x80000000U) {
								matrix.leftScale = previousScale;
								matrix.leftSubtract = previousSubtract;
								proceed = 0;
								break;
							}
							previousScale = matrix.leftScale;
							previousSubtract = matrix.leftSubtract;
						} while (limit <= high[0]);
						active = 1;
					} else {
						q = high[0] / limit;
						active = q > 3;
						/* Bound the growing row by the signed-word limit. */
						cost = (uint64_t)q * (matrix.rightScale + matrix.rightSubtract);
						proceed = !(cost >> 32) &&
							(uint32_t)cost <= 0x7fffffffU - (matrix.leftScale + matrix.leftSubtract);
						if (proceed) {
							matrix.leftScale += matrix.rightSubtract * q;
							matrix.leftSubtract += matrix.rightScale * q;
							wide = (uint64_t)q * low[1];
							high[0] -= high[1] * q + (wide >> 32) + (low[0] < (uint32_t)wide);
							low[0] -= wide;
							if ((matrix.leftScale | matrix.leftSubtract) & 0x80000000U)
								active = 0;
						}
					}
				} else {
					proceed = 0;
					active = 1;
				}
				if (!high[1] || high[1] - 1 <= high[0])
					continue;
				limit = high[0] + 2;
				if (limit > high[1] >> 2) {
					previousScale = matrix.rightScale;
					previousSubtract = matrix.rightSubtract;
					for (;;) {
						high[1] -= high[0] + (low[0] > low[1]);
						low[1] -= low[0];
						matrix.rightScale += matrix.leftSubtract;
						matrix.rightSubtract += matrix.leftScale;
						if ((matrix.rightScale | matrix.rightSubtract) & 0x80000000U) {
							matrix.rightScale = previousScale;
							matrix.rightSubtract = previousSubtract;
							proceed = 0;
							break;
						}
						previousScale = matrix.rightScale;
						previousSubtract = matrix.rightSubtract;
						if (limit > high[1]) {
							proceed = 1;
							break;
						}
					}
				} else {
					q = high[1] / limit;
					if (q <= 3)
						active = 0;
					cost = (uint64_t)q * (matrix.leftScale + matrix.leftSubtract);
					if (!(cost >> 32) && (uint32_t)cost <=
					    0x7fffffffU - (matrix.rightScale + matrix.rightSubtract)) {
						matrix.rightScale += matrix.leftSubtract * q;
						matrix.rightSubtract += matrix.leftScale * q;
						wide = (uint64_t)q * low[0];
						high[1] -= high[0] * q + (wide >> 32) + (low[1] < (uint32_t)wide);
						low[1] -= wide;
						if ((matrix.rightScale | matrix.rightSubtract) & 0x80000000U)
							active = 0;
					}
				}
			}
			if (!active || ((matrix.leftScale | matrix.leftSubtract |
				matrix.rightScale | matrix.rightSubtract) & 0x80000000U))
				goto out;
			if (!(matrix.leftSubtract | matrix.rightSubtract)) {
				if (side) matrix.rightSubtract = 1; else matrix.leftSubtract = 1;
			}
			if (!KrmEccApplyGcdRemainderMatrix(&matrix, remainders[0], remainders[1], count, &carry) ||
			    carry.left || carry.right)
				goto out;
			for (i = 0; i < 2; i++) {
				lengths[i] = count;
				while (lengths[i] && !remainders[i][lengths[i] - 1])
					lengths[i]--;
			}
		}
		if (!KrmEccApplyGcdCoefficientMatrix(&matrix, coefficients[0], coefficients[1],
			coefficientWords, &carry))
			goto out;
		if (carry.left || carry.right) {
			if (rightWords <= coefficientWords)
				goto out;
			coefficients[0][coefficientWords] = carry.left;
			coefficients[1][coefficientWords++] = carry.right;
		}
	}
	survivor = lengths[0] == 0;
	for (i = 0; i < lengths[survivor]; i++)
		gcd[i] = remainders[survivor][i];
	terminal = coefficients[1 - survivor];
	if (KrmEccCompareIntegers(right, terminal, rightWords) < 0 ||
	    KrmEccCompareIntegers(terminal, coefficients[survivor], rightWords) < 0)
		goto out;
	if (survivor)
		KrmEccSubtractWords(coefficients[0], coefficients[1], coefficient, rightWords);
	else
		for (i = 0; i < rightWords; i++)
			coefficient[i] = coefficients[0][i];
	if (secondCoefficient) {
		if (secondCoefficient == left || secondCoefficient == terminal)
			goto out;
		GCD_PRODUCT(left, leftWords, terminal, rightWords, (uint32_t *)secondCoefficient);
	}
	if (coefficientInfo) {
		KrmEccSubtractWords(terminal, coefficient, terminal, rightWords);
		GCD_PRODUCT(left, leftWords, terminal, rightWords, product);
		if (!KrmEccAddIntegers(product, leftWords + rightWords, gcd, lengths[survivor], product, NULL) ||
		    !KrmEccReduce(product, leftWords + rightSignificant, right, rightSignificant, NULL, quotient, terminal))
			goto out;
		for (i = 0; i < rightSignificant; i++)
			if (terminal[i])
				goto out;
		for (i = 0; i < leftWords; i++)
			((uint32_t *)coefficientInfo)[i] = quotient[i];
	}
	outputWords = lengths[survivor];
	ok = 1;
out:
	if (local.allocated)
		KrmEccFree(local.data, ctx);
	if (ok)
		*gcdWords = outputWords;
#undef GCD_PRODUCT
	return ok;
}

int KrmEccInverseModulo(const uint32_t *value, const uint32_t *modulus,
	unsigned int words, uint32_t *result, const uint32_t *coefficientInfo,
	void *workspace, KrmEccContext *ctx)
{
	KrmEccWorkspace local = { workspace, 9 * words + 6, 0 };
	unsigned int significant = words, gcdWords;
	uint32_t *gcd;
	const uint32_t *scan;
	int ok = 0;

	(void)coefficientInfo;
	if (KrmEccEnsureWorkspace(&local, ctx) && KrmEccIsReduced(value, modulus, words)) {
		if (significant && !value[significant - 1]) {
			scan = value + significant - 1;
			while (--significant) {
				if (*--scan)
					break;
			}
		}
		if (significant) {
			gcd = local.data;
			gcdWords = 0;
			ok = KrmEccExtendedGcd(value, words, modulus, words, result, NULL,
				gcd, NULL, &gcdWords, gcd + words, ctx);
			if (ok)
				ok = gcdWords == 1 && gcd[0] == 1;
		}
	}
	if (local.allocated == 1)
		KrmEccFree(local.data, ctx);
	return ok;
}

int KrmEccApplyGcdCoefficientMatrix(const KrmEccGcdMatrix *matrix,
	uint32_t *left, uint32_t *right, unsigned int words, KrmEccGcdCarry *carry)
{

	uint32_t leftScale = matrix->leftScale, leftSubtract = matrix->leftSubtract;
	uint32_t rightSubtract = matrix->rightSubtract, rightScale = matrix->rightScale;
	uint32_t leftCarry = 0, rightCarry = 0, a, b;
	uint64_t first, second, leftProduct, rightProduct;
	unsigned int i;
	int ok = 1;

	if (leftSubtract > ~leftScale ||
	    rightSubtract > ~rightScale) {
		ok = 0;
	} else {
		for (i = 0; i < words; i++) {
			a = left[i];
			leftProduct = (uint64_t)leftScale * a + leftCarry;
			rightProduct = (uint64_t)rightSubtract * a + rightCarry;
			b = right[i];
			first = (uint64_t)leftSubtract * b + (uint32_t)leftProduct;
			second = (uint64_t)rightScale * b + (uint32_t)rightProduct;
			left[i] = first;
			right[i] = second;
			leftCarry = (leftProduct >> 32) + (first >> 32);
			rightCarry = (rightProduct >> 32) + (second >> 32);
		}
	}
	*(CarryVector *)carry = (CarryVector){ leftCarry, rightCarry };
	return ok;
}

int KrmEccApplyGcdRemainderMatrix(const KrmEccGcdMatrix *matrix,
	uint32_t *left, uint32_t *right, unsigned int words, KrmEccGcdCarry *carry)
{
	uint32_t leftScale = matrix->leftScale, leftSubtract = matrix->leftSubtract;
	uint32_t rightSubtract = matrix->rightSubtract, rightScale = matrix->rightScale;
	int32_t leftCarry = 0, rightCarry = 0;
	uint32_t a, b;
	uint64_t first, second, rightProduct;
	unsigned int i;
	int ok = 1;

	if ((leftScale | leftSubtract | rightScale |
	     rightSubtract) & 0x80000000U) {
		ok = 0;
	} else {
		i = 0;
#define REMAINDER_WORD(n) do { \
	a = left[i + (n)]; \
	b = right[i + (n)]; \
	rightProduct = (uint64_t)rightScale * b + rightCarry; \
	first = (uint64_t)leftScale * a + leftCarry - \
		(uint64_t)leftSubtract * b; \
	left[i + (n)] = first; \
	second = rightProduct - (uint64_t)rightSubtract * a; \
	right[i + (n)] = second; \
	leftCarry = first >> 32; \
	rightCarry = second >> 32; \
} while (0)
		while (words > i + 8) {
			__builtin_prefetch(left + i + 9);
			__builtin_prefetch(right + i + 9);
			REMAINDER_WORD(0);
			REMAINDER_WORD(1);
			REMAINDER_WORD(2);
			REMAINDER_WORD(3);
			REMAINDER_WORD(4);
			REMAINDER_WORD(5);
			REMAINDER_WORD(6);
			REMAINDER_WORD(7);
			i += 8;
		}
		while (i < words) {
			REMAINDER_WORD(0);
			i++;
		}
#undef REMAINDER_WORD
	}
	*(CarryVector *)carry = (CarryVector){ leftCarry, rightCarry };
	return ok;
}

int KrmEccRandomBelow(const uint32_t *limit, uint32_t *result,
	unsigned int words, KrmEccContext *ctx)
{
	unsigned int significant = words, shift, attempt;
	uint32_t high, sample, normalized;
	int bits;

	while (significant && !limit[significant - 1])
		result[--significant] = 0;
	if (!significant || limit == result)
		return 0;
	high = limit[significant - 1];
	attempt = 100;
	while (KrmEccRandom((unsigned char *)result, 4 * (significant - 1), ctx)) {
		normalized = high | 1;
		bits = 32;
		while (normalized <= 0x7ffffff) {
			normalized <<= 5;
			bits -= 5;
		}
		bits -= (0x24949cU >> (normalized >> 28) >> (2 * (normalized >> 28))) & 7;
		shift = 32 - bits;
		sample = 0;
		do {
			if (!KrmEccRandom((unsigned char *)&sample, 4, ctx)) {
				result[significant - 1] = sample >> shift;
				return 0;
			}
			sample >>= shift;
		} while (sample > high);
		result[significant - 1] = sample;
		if (KrmEccCompareIntegers(result, limit, significant) < 0)
			return 1;
		if (!--attempt)
			break;
	}
	return 0;
}

int KrmEccRandomScalar(const uint32_t *limit, uint32_t *scalar,
	unsigned int wordCount, KrmEccContext *ctx)
{
	unsigned int significant;
	int attempt;
	const uint32_t *scan;
	int ok;

	if (KrmEccCompareWord(limit, 1, wordCount) > 0)
		for (attempt = 100; attempt; attempt--) {
			ok = KrmEccRandomBelow(limit, scalar, wordCount, ctx);
			if (!ok)
				break;
			if (wordCount) {
				if (scalar[wordCount - 1])
					return ok;
				significant = wordCount;
				scan = scalar + wordCount - 1;
				while (--significant)
					if (*--scan)
						return ok;
			}
		}
	return 0;
}

int KrmEccLucasSequence(const uint32_t *p, const uint32_t *q,
	const uint32_t *exponent, unsigned int words, uint32_t *u, uint32_t *v,
	const KrmEccModulus *modulus, KrmEccContext *ctx)
{
	unsigned int significant = words, bits, fieldWords = modulus->wordCount, i;
	unsigned int allocationWords = 2 * fieldWords + modulus->productWords;
	uint32_t *allocation, *temporary, *scratch;
	const uint32_t *scan;
	const unsigned char *source;
	unsigned char *destination;
	unsigned int bytes;
	int ok;
	uint32_t normalized;
	unsigned int highBits;

	if (significant && !exponent[significant - 1]) {
		scan = exponent + significant - 1;
		while (--significant) {
			if (*--scan)
				break;
		}
	}
	bits = 0;
	if (significant) {
		normalized = exponent[significant - 1] | 1;
		highBits = 32;
		while (normalized <= 0x7ffffff) {
			normalized <<= 5;
			highBits -= 5;
		}
		highBits -= (0x24949cU >> (normalized >> 28) >> (2 * (normalized >> 28))) & 7;
		bits = highBits + 32 * (significant - 1);
	}
	if (allocationWords < 2 * fieldWords)
		return 0;
	allocation = KrmEccAllocateWords(allocationWords, ctx);
	if (!allocation)
		return 0;
	temporary = allocation + fieldWords;
	scratch = temporary + fieldWords;
	if (bits) {
		for (i = 0; i < 4 * fieldWords; i++)
			((unsigned char *)v)[i] = ((const unsigned char *)p)[i];
		source = (const unsigned char *)modulus->one;
		for (i = 0; i < 4 * fieldWords; i++)
			((unsigned char *)u)[i] = source[i];
		ok = KrmEccShiftModulo(q, 2, temporary, modulus) &&
			KrmEccMultiplyModulo(p, p, allocation, modulus, scratch, ctx) &&
			KrmEccSubtractModulo(allocation, temporary, allocation, modulus->modulus, modulus->wordCount);
		bits--;
		while (bits && ok) {
			bits--;
			ok = KrmEccMultiplyModulo(u, u, temporary, modulus, scratch, ctx) &&
				KrmEccMultiplyModulo(temporary, allocation, temporary, modulus, scratch, ctx) &&
				KrmEccMultiplyModulo(u, v, u, modulus, scratch, ctx) &&
				KrmEccMultiplyModulo(v, v, v, modulus, scratch, ctx) &&
				KrmEccAddModulo(v, temporary, v, modulus->modulus, modulus->wordCount) &&
				KrmEccShiftModulo(v, -1, v, modulus);
			if ((exponent[bits >> 5] >> (bits & 31)) & 1)
				ok = ok && KrmEccMultiplyModulo(allocation, u, temporary, modulus, scratch, ctx) &&
					KrmEccMultiplyModulo(p, u, u, modulus, scratch, ctx) &&
					KrmEccAddModulo(u, v, u, modulus->modulus, modulus->wordCount) &&
					KrmEccShiftModulo(u, -1, u, modulus) &&
					KrmEccMultiplyModulo(p, v, v, modulus, scratch, ctx) &&
					KrmEccAddModulo(v, temporary, v, modulus->modulus, modulus->wordCount) &&
					KrmEccShiftModulo(v, -1, v, modulus);
		}
	} else {
		ok = KrmEccAddModulo(modulus->one, modulus->one, v, modulus->modulus, modulus->wordCount) != 0;
		destination = (unsigned char *)u;
		bytes = 4 * fieldWords;
		while (bytes > 32) {
			__builtin_prefetch(destination + 40);
			destination[0] = 0;
			destination[1] = 0;
			destination[2] = 0;
			destination[3] = 0;
			destination[4] = 0;
			destination[5] = 0;
			destination[6] = 0;
			destination[7] = 0;
			destination[8] = 0;
			destination[9] = 0;
			destination[10] = 0;
			destination[11] = 0;
			destination[12] = 0;
			destination[13] = 0;
			destination[14] = 0;
			destination[15] = 0;
			destination[16] = 0;
			destination[17] = 0;
			destination[18] = 0;
			destination[19] = 0;
			destination[20] = 0;
			destination[21] = 0;
			destination[22] = 0;
			destination[23] = 0;
			destination[24] = 0;
			destination[25] = 0;
			destination[26] = 0;
			destination[27] = 0;
			destination[28] = 0;
			destination[29] = 0;
			destination[30] = 0;
			destination[31] = 0;
			destination += 32;
			bytes -= 32;
		}
		while (bytes--)
			*destination++ = 0;
	}
	KrmEccFree(allocation, ctx);
	return ok;
}

typedef struct KrmEccPowerTable {
	const KrmEccModulus *modulus;
	uint32_t ready[64];
	uint32_t *powers[64];
	void *workspace;
} KrmEccPowerTable;

int KrmEccPowerModulo(const uint32_t *value, const uint32_t *exponent,
	unsigned int words, uint32_t *result, const KrmEccModulus *modulus, KrmEccContext *ctx)
{
	unsigned int fieldWords = modulus->wordCount, significant = words, bits, window;
	unsigned int maximum, allocationWords, i, index, position, level, pending, selected, probe;
	unsigned int threshold, accumulated, bit;
	uint16_t windowLimits[5];
	uint32_t normalized;
	unsigned int highBits, copyBytes;
	uint32_t *allocation = NULL, *destination;
	const uint32_t *source;
	KrmEccPowerTable *table;
	void *tableAllocation = NULL;
	int valid, ok = 0, seeded = 0, keepGoing;

#define ACCUMULATE_POWER(powerIndex, input) do { \
	index = (powerIndex); \
	destination = table->powers[index]; \
	if (table->ready[index]) { \
		KrmEccMultiplyModulo(destination, input, destination, table->modulus, table->workspace, ctx); \
	} else { \
		table->ready[index] = 1; \
		source = (input); \
		copyBytes = 4 * table->modulus->wordCount; \
		for (i = 0; i < copyBytes; i++) \
			((unsigned char *)destination)[i] = ((const unsigned char *)source)[i]; \
	} \
} while (0)
	while (significant && !exponent[significant - 1])
		significant--;
	bits = 0;
	if (significant) {
		normalized = exponent[significant - 1] | 1;
		highBits = 32;
		while (normalized <= 0x7ffffff) {
			normalized <<= 5;
			highBits -= 5;
		}
		highBits -= (0x24949cU >> (normalized >> 28) >> (2 * (normalized >> 28))) & 7;
		bits = highBits + 32 * (significant - 1);
	}
	__builtin_memcpy(windowLimits, g_krmPowerWindowLimits,
			 sizeof(windowLimits));
	window = bits <= windowLimits[0] ? 1 : bits <= windowLimits[1] ? 2 :
		bits <= windowLimits[2] ? 3 : bits <= windowLimits[3] ? 4 :
		bits <= windowLimits[4] ? 5 : 6;
	maximum = (1U << window) - 1;
	valid = KrmEccIsReduced(value, modulus->modulus, fieldWords);
	table = KrmEccMemalign(sizeof(*table), 4, ctx, &tableAllocation);
	allocationWords = fieldWords * maximum + modulus->productWords;
	if (!fieldWords || allocationWords < fieldWords * maximum)
		goto out;
	allocation = KrmEccAllocateWords(allocationWords, ctx);
	if (!valid || !table || !allocation)
		goto out;
	table->modulus = modulus;
	table->powers[0] = NULL;
	table->workspace = allocation + fieldWords * maximum;
	if (!KrmEccAddModulo(modulus->one, modulus->one, allocation, modulus->modulus, modulus->wordCount))
		goto out;
	if (!KrmEccCompareIntegers(value, allocation, fieldWords) && bits) {
		threshold = 32 * fieldWords;
		if (threshold >= 1024)
			threshold = 1024;
		accumulated = 0;
		position = bits;
		while (position) {
			position--;
			bit = (exponent[position >> 5] >> (position & 31)) & 1;
			if (seeded) {
				if (!KrmEccMultiplyModulo(allocation, allocation, allocation, modulus, table->workspace, ctx) ||
				    (bit && !KrmEccAddModulo(allocation, allocation, allocation, modulus->modulus, modulus->wordCount)))
					goto out;
			} else {
				accumulated = bit + 2 * accumulated;
				if (position && threshold > 2 * accumulated)
					continue;
				if (!KrmEccShiftModulo(modulus->one, accumulated, allocation, modulus))
					goto out;
			}
			seeded = 1;
		}
		table->powers[1] = allocation;
		ok = seeded;
		goto copyResult;
	}
	for (index = 1; index <= maximum; index++) {
		table->ready[index] = 0;
		table->powers[index] = allocation +
			(fieldWords * ((index & 1 ? 0 : maximum) + index - 1) >> 1);
		source = modulus->one;
		for (i = 0; i < 4 * fieldWords; i++)
			((unsigned char *)table->powers[index])[i] = ((const unsigned char *)source)[i];
	}
	for (i = 0; i < 4 * fieldWords; i++)
		((unsigned char *)result)[i] = ((const unsigned char *)value)[i];
	ok = 1;
	pending = 0;
	level = 0;
	position = 1;
	if (bits)
		do {
			bit = (exponent[(position - 1) >> 5] >> ((position - 1) & 31)) & 1;
			if (pending >> (window + 2)) {
				ok = 0;
				keepGoing = 0;
				if (bit)
					goto setBit;
				goto nextBit;
			}
			if (!bit) {
				keepGoing = ok && bits != position;
				goto nextBit;
			}
			if (!ok) {
				keepGoing = 0;
				goto setBit;
			}
			while (level >= window + 1) {
				if (pending & 1) {
					selected = pending & maximum;
					pending -= selected;
					ACCUMULATE_POWER(selected, result);
				}
				ok = KrmEccMultiplyModulo(result, result, result, table->modulus, table->workspace, ctx) != 0;
				pending >>= 1;
				level--;
				if (!ok)
					break;
			}
			keepGoing = ok && bits != position;
setBit:
			pending |= 1U << level;
nextBit:
			level++;
			position++;
		} while (keepGoing);
	while (pending && ok) {
		if (pending <= maximum) {
			selected = pending;
			pending = 0;
		} else if (pending & 1) {
			selected = pending > 3 * maximum ? pending & maximum : maximum;
			pending -= selected;
		} else {
			pending >>= 1;
			ok = KrmEccMultiplyModulo(result, result, result, table->modulus, table->workspace, ctx) != 0;
			continue;
		}
		ACCUMULATE_POWER(selected, result);
	}
	for (selected = maximum; selected > 1 && ok; selected--)
		if (table->ready[selected]) {
			index = selected >> 1;
			probe = index;
			if (selected & 1)
				index = 1;
			for (; probe != selected; probe++)
				if (table->ready[probe]) {
					index = probe;
					if (table->ready[selected - probe])
						break;
				}
			probe = index;
			source = table->powers[selected];
			ACCUMULATE_POWER(probe, source);
			ACCUMULATE_POWER(selected - probe, source);
		}
copyResult:
	if (ok) {
		if (!fieldWords)
			ok = 0;
		else {
			source = table->powers[1];
			for (i = 0; i < 4 * fieldWords; i++)
				((unsigned char *)result)[i] = ((const unsigned char *)source)[i];
		}
	}
out:
	if (allocation)
		KrmEccFree(allocation, ctx);
	if (table)
		KrmEccFree(tableAllocation, ctx);
#undef ACCUMULATE_POWER
	return ok;
}

/* P-256 parameters in the field's Montgomery representation. */
#define KRM_ECC_CONSTANT __attribute__((section(".rodata.krmEcc"), aligned(4)))
const KrmEccFieldOperations g_krmEccOperations KRM_ECC_CONSTANT = {
	.add = KrmEccFieldAdd,
	.equal = KrmEccFieldEqual,
	.release = KrmEccFieldRelease,
	.setIntegers = KrmEccFieldSetIntegers,
	.inverse = KrmEccFieldInverse,
	.isZero = KrmEccFieldIsZero,
	.shift = KrmEccFieldShift,
	.multiply = KrmEccFieldMultiply,
	.negate = KrmEccFieldNegate,
	.copyModulus = KrmEccFieldCopyModulus,
	.subtract = KrmEccFieldSubtract,
	.zero = KrmEccFieldZero,
};
const uint32_t g_krmEccReservedOperationWords[8] KRM_ECC_CONSTANT = {
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
};
const uint32_t g_krmEccOne[8] KRM_ECC_CONSTANT = {
	0x00000001, 0x00000000, 0x00000000, 0xffffffff, 
	0xffffffff, 0xffffffff, 0xfffffffe, 0x00000000,
};
const uint32_t g_krmEccPrime[8] KRM_ECC_CONSTANT = {
	0xffffffff, 0xffffffff, 0xffffffff, 0x00000000, 
	0x00000000, 0x00000000, 0x00000001, 0xffffffff,
};
const uint32_t g_krmEccReservedModulusWords[4] KRM_ECC_CONSTANT = {
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
};
const KrmEccCurve g_krmEccCurve KRM_ECC_CONSTANT = {
	.field = &g_krmEccField, .a = g_krmEccCurveA, .b = g_krmEccCurveB,
	.generator = &g_krmEccGenerator, .order = g_krmEccOrder,
	.workspaceWords = 158,
};
const uint32_t g_krmEccCurveA[8] KRM_ECC_CONSTANT = {
	0xfffffffc, 0xffffffff, 0xffffffff, 0x00000003, 
	0x00000000, 0x00000000, 0x00000004, 0xfffffffc,
};
const uint32_t g_krmEccReservedCurveWords[8] KRM_ECC_CONSTANT = {
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
};
const uint32_t g_krmEccReciprocalWords[8] KRM_ECC_CONSTANT = {
	0xffffffff, 0xffffffff, 0x00000001, 0x00000001, 
	0xffffffff, 0xfffffffe, 0x00000001, 0xfffffffe,
};
const uint32_t g_krmEccCurveB[8] KRM_ECC_CONSTANT = {
	0x29c4bddf, 0xd89cdf62, 0x78843090, 0xacf005cd, 
	0xf7212ed6, 0xe5a220ab, 0x04874834, 0xdc30061d,
};
const uint32_t g_krmEccReductionCoefficients[8] KRM_ECC_CONSTANT = {
	0x00000000, 0x00000003, 0xfffffffe, 0x00000001, 
	0x00000002, 0xfffffffd, 0x00000003, 0xfffffffe,
};
const uint32_t g_krmEccHighWords[4] KRM_ECC_CONSTANT = {
	0xffffffff, 0xffffffff, 0xffffffff, 0xfffffffe,
};
const uint32_t g_krmEccOrder[8] KRM_ECC_CONSTANT = {
	0xfc632551, 0xf3b9cac2, 0xa7179e84, 0xbce6faad, 
	0xffffffff, 0xffffffff, 0x00000000, 0xffffffff,
};
const uint32_t g_krmEccReservedFieldWords[9] KRM_ECC_CONSTANT = {
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 
	0x00000000, 0x00000000, 0x00000000, 0xffffffff, 
	0x00000000,
};
const KrmEccField g_krmEccField KRM_ECC_CONSTANT = {
	.wordCount = 8, .elementCount = 1, .workspaceWords = 94,
	.productWords = 16, .multiplyWorkspaceWords = 86, .flags = 1,
	.one = g_krmEccOne, .operations = &g_krmEccOperations,
	.modulus = &g_krmEccModulus,
	.reductionCoefficients = g_krmEccReductionCoefficients,
};
const KrmEccModulus g_krmEccModulus KRM_ECC_CONSTANT = {
	.wordCount = 8, .wordBytes = 4, .productWords = 16,
	.radixBits = 256, .montgomeryEnabled = 1,
	.montgomeryInverse = 0xffffffff,
	.modulus = g_krmEccPrime, .reciprocal = g_krmEccReciprocalWords,
	.highWords = g_krmEccHighWords, .one = g_krmEccOne,
	.multiply = KrmEccMontgomeryMultiply256,
};
const KrmEccPublicKey g_krmEccGenerator KRM_ECC_CONSTANT = {
	.x = { 0x18a9143c, 0x79e730d4, 0x5fedb601, 0x75ba95fc, 
	0x77622510, 0x79fb732b, 0xa53755c6, 0x18905f76 },
	.y = { 0xce95560a, 0xddf25357, 0xba19e45c, 0x8b4ab8e4, 
	0xdd21f325, 0xd2e88688, 0x25885d85, 0x8571ff18 },
};
#undef KRM_ECC_CONSTANT
