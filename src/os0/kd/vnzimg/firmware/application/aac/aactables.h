/*
 * The AAC decoder's constant tables, gathered at an instance's creation
 * (aactables.c). Bands and windows are by sampling index.
 */
#pragma once

#define AAC_SAMPLING_RATES	12

typedef struct AacTables {
	const uint8_t *bitReverse;
	const uint32_t *samplingRates;
	const uint32_t *swbOffsetLong[AAC_SAMPLING_RATES];
	const uint32_t *swbOffsetShort[AAC_SAMPLING_RATES];
	const uint32_t *numSwb[2];		/* long, short */
	const uint32_t *tnsMaxOrder;
	const uint32_t *tnsMaxBands[2];
	const uint32_t *predSfbMax;
	const int32_t *sineWindowLong;
	const int32_t *sineWindowShort;
	const int32_t *kbdWindowLong;
	const int32_t *kbdWindowShort;
	const int32_t *imdctCosLong;
	const int32_t *imdctSinLong;
	const int32_t *imdctCosShort;
	const int32_t *imdctSinShort;
	const int32_t *fftCos;
	const int32_t *fftSin;
	const int32_t *tnsCoef3;
	const int32_t *tnsCoef4;
	const int32_t *pow2Quarter;
	const int32_t *pow2Third;
	const int32_t *pow43Mantissa;
	const int32_t *pow43Exponent;
	uint32_t reserved;
	AacCodebook codebooks[AAC_CODEBOOKS];
} AacTables;

extern const uint32_t aacBitMasks[33];
extern const int32_t aacNegativePowers[25];
extern const int32_t aacInverseSqrt[257];

void aacTablesInit(AacTables *tables);
