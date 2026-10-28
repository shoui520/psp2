/* Shared ECC implementation; the original source filename is unavailable. */
#include "msif_ecdsa.h"

int MsifEcdsaVerifyCore(const struct MsifEcdsaSignature *signature,
	const uint8_t *digest, const struct MsifEcdsaPoint *publicKey,
	const struct MsifEcdsaCurve *curve, int words, int bytes)
{
	uint32_t prime[7], coefficientA[7], order[7];
	uint32_t r[7], inverseS[7], hash[7];
	uint32_t baseX[7], baseY[7], baseZ[7];
	uint32_t publicX[7], publicY[7], publicZ[7];
	uint32_t scalars[21];
	struct MsifEcdsaWordPoint base = {baseX, baseY, baseZ};
	struct MsifEcdsaWordPoint point = {publicX, publicY, publicZ};
	int index;

	MsifIntegerImport(prime, curve->prime, bytes);
	MsifIntegerImport(coefficientA, curve->coefficientA, bytes);
	MsifIntegerImport(order, curve->order, bytes);
	MsifIntegerImport(base.x, curve->baseX, bytes);
	MsifIntegerImport(base.y, curve->baseY, bytes);
	MsifIntegerImport(hash, digest, bytes);
	MsifIntegerImport(point.x, publicKey->x, bytes);
	MsifIntegerImport(point.y, publicKey->y, bytes);
	index = words;
	do {
		--index;
	} while (index + 1 > 0 && prime[index] == 0);
	if (index < 0)
		return -1;
	MsifIntegerRemainder(point.x, point.x, words, prime, words);
	MsifIntegerRemainder(point.y, point.y, words, prime, words);
	MsifIntegerImport(r, signature->r, bytes);
	MsifIntegerImport(inverseS, signature->s, bytes);
	if (!MsifIntegerInRange(r, order, words) ||
			!MsifIntegerInRange(inverseS, order, words))
		return -1;
	MsifIntegerInverse(inverseS, inverseS, order, words);
	MsifIntegerMultiply(&scalars[words], hash, words, inverseS, words);
	MsifIntegerRemainder(scalars, &scalars[words], 2 * words, order, words);
	MsifIntegerMultiply(&scalars[words], r, words, inverseS, words);
	MsifIntegerRemainder(&scalars[words], &scalars[words], 2 * words,
		order, words);
	MsifEcdsaMultiplyAdd(&point, &base, &point, scalars, &scalars[words],
		prime, coefficientA, words);
	MsifIntegerRemainder(point.x, point.x, words, order, words);
	for (index = 0; index < words && point.x[index] == r[index]; ++index)
		;
	if (words > 0)
		return words - index == 0 ? 0 : -1;
	return -1;
}

int MsifEcdsaVerify224(const struct MsifEcdsaSignature *signature,
	const uint8_t *digest, const struct MsifEcdsaPoint *publicKey,
	const struct MsifEcdsaCurve *curve)
{
	return MsifEcdsaVerifyCore(signature, digest, publicKey, curve, 7, 28);
}

__attribute__((noinline)) int MsifIntegerInverse(uint32_t *destination, const uint32_t *value,
	const uint32_t *modulus, int words)
{
	uint32_t u[64], v[64];
	uint32_t vModulusCoefficient[65], vValueCoefficient[65];
	uint32_t uModulusCoefficient[65], uValueCoefficient[65];
	uint32_t valueBits = 0, modulusBits = 0;
	int index;

	if ((uint32_t)words - 1 > 63 || !((*modulus | *value) & 1))
		return 0;
	index = words;
	do {
		u[index - 1] = value[index - 1];
		valueBits |= value[index - 1];
		v[index - 1] = modulus[index - 1];
		modulusBits |= modulus[index - 1];
		vModulusCoefficient[index] = 0;
		vValueCoefficient[index] = 0;
		uModulusCoefficient[index] = 0;
		uValueCoefficient[index] = 0;
	} while (--index != 0);
	vValueCoefficient[0] = 0;
	vModulusCoefficient[0] = 1;
	uModulusCoefficient[0] = 0;
	uValueCoefficient[0] = 1;
	if (!valueBits || !modulusBits)
		return 0;
	do {
		while (!(v[0] & 1)) {
			uint32_t carry, borrow;
			uint32_t vCarry = 0, modulusCarry, valueCarry;

			if ((vModulusCoefficient[0] | vValueCoefficient[0]) & 1) {
				carry = MsifIntegerAdd(vModulusCoefficient,
					vModulusCoefficient, value, words, 0);
				borrow = MsifIntegerSubtract(vValueCoefficient,
					vValueCoefficient, modulus, words, 0);
				vModulusCoefficient[words] += carry;
				vValueCoefficient[words] -= borrow;
			}
			modulusCarry = vModulusCoefficient[words] << 31;
			valueCarry = vValueCoefficient[words] << 31;
			vModulusCoefficient[words] =
				(int32_t)vModulusCoefficient[words] >> 1;
			vValueCoefficient[words] =
				(int32_t)vValueCoefficient[words] >> 1;
			index = words;
			do {
				uint32_t vWord = v[--index];
				uint32_t modulusWord = vModulusCoefficient[index];
				uint32_t valueWord = vValueCoefficient[index];

				v[index] = vCarry | (vWord >> 1);
				vModulusCoefficient[index] = modulusCarry | (modulusWord >> 1);
				vValueCoefficient[index] = valueCarry | (valueWord >> 1);
				vCarry = vWord << 31;
				modulusCarry = modulusWord << 31;
				valueCarry = valueWord << 31;
			} while (index != 0);
		}
		while (!(u[0] & 1)) {
			uint32_t carry, borrow;
			uint32_t uCarry = 0, modulusCarry, valueCarry;

			if ((uModulusCoefficient[0] | uValueCoefficient[0]) & 1) {
				carry = MsifIntegerAdd(uModulusCoefficient,
					uModulusCoefficient, value, words, 0);
				borrow = MsifIntegerSubtract(uValueCoefficient,
					uValueCoefficient, modulus, words, 0);
				uModulusCoefficient[words] += carry;
				uValueCoefficient[words] -= borrow;
			}
			modulusCarry = uModulusCoefficient[words] << 31;
			valueCarry = uValueCoefficient[words] << 31;
			uModulusCoefficient[words] =
				(int32_t)uModulusCoefficient[words] >> 1;
			uValueCoefficient[words] =
				(int32_t)uValueCoefficient[words] >> 1;
			index = words;
			do {
				uint32_t uWord = u[--index];
				uint32_t modulusWord = uModulusCoefficient[index];
				uint32_t valueWord = uValueCoefficient[index];

				u[index] = uCarry | (uWord >> 1);
				uModulusCoefficient[index] = modulusCarry | (modulusWord >> 1);
				uValueCoefficient[index] = valueCarry | (valueWord >> 1);
				uCarry = uWord << 31;
				modulusCarry = modulusWord << 31;
				valueCarry = valueWord << 31;
			} while (index != 0);
		}
		if (MsifIntegerGreaterThan(u, v, words)) {
			MsifIntegerSubtract(u, u, v, words, 0);
			MsifIntegerSubtract(uModulusCoefficient, uModulusCoefficient,
				vModulusCoefficient, words + 1, 0);
			MsifIntegerSubtract(uValueCoefficient, uValueCoefficient,
				vValueCoefficient, words + 1, 0);
		} else {
			MsifIntegerSubtract(v, v, u, words, 0);
			MsifIntegerSubtract(vModulusCoefficient, vModulusCoefficient,
				uModulusCoefficient, words + 1, 0);
			MsifIntegerSubtract(vValueCoefficient, vValueCoefficient,
				uValueCoefficient, words + 1, 0);
		}
		index = words;
		while (index != 0 && v[index - 1] == 0)
			--index;
	} while (index != 0);
	for (index = words - 1; index > 0; --index)
		if (u[index] != 0)
			return 0;
	if (u[0] != 1)
		return 0;
	if ((int32_t)uValueCoefficient[words] < 0) {
		do {
			uValueCoefficient[words] += MsifIntegerAdd(uValueCoefficient,
				uValueCoefficient, modulus, words, 0);
		} while (uValueCoefficient[words] != 0);
	} else {
		while (uValueCoefficient[words] != 0 ||
				!MsifIntegerGreaterThan(modulus, uValueCoefficient, words))
			uValueCoefficient[words] -= MsifIntegerSubtract(uValueCoefficient,
				uValueCoefficient, modulus, words, 0);
	}
	index = 0;
	do {
		destination[index] = uValueCoefficient[index];
	} while (++index < words);
	return 1;
}

__attribute__((noinline)) void MsifIntegerMultiply(uint32_t *destination, const uint32_t *left,
	int leftWords, const uint32_t *right, int rightWords)
{
	uint32_t partial[65];
	uint32_t product[128];
	int index, rightIndex, resultWords;

	if ((uint32_t)leftWords - 1 <= 63 &&
			(uint32_t)rightWords - 1 <= 63) {
		for (index = 0; ; ++index) {
			rightIndex = 0;
			resultWords = leftWords + rightWords;
			if (index >= resultWords)
				break;
			product[index] = 0;
		}
		do {
			uint32_t *accumulator = &product[rightIndex];
			uint32_t multiplier = right[rightIndex++];

			MsifIntegerMultiplyWord(partial, left, leftWords, multiplier);
			MsifIntegerAdd(accumulator, accumulator, partial, leftWords + 1, 0);
		} while (rightIndex != rightWords);
		index = 0;
		do {
			destination[index] = product[index];
		} while (++index < resultWords);
	}
}

uint32_t *MsifIntegerMultiplyWord(uint32_t *destination, const uint32_t *value,
	int words, uint32_t multiplier)
{
	if (words > 0) {
		unsigned int index = 0;
		uint32_t multiplierHigh = multiplier >> 16;
		uint32_t carry = 0;

		do {
			uint32_t word = value[index];
			uint32_t lowLow = (uint32_t)(uint16_t)multiplier * (uint16_t)word;
			uint32_t lowHigh = (uint16_t)multiplier * (word >> 16);
			uint32_t highLow = multiplierHigh * (uint16_t)word;
			uint32_t middle = lowHigh + (lowLow >> 16);
			uint32_t low = lowLow + ((lowHigh + highLow) << 16);
			uint32_t sum = low + carry;

			destination[index++] = sum;
			carry = multiplierHigh * (word >> 16) + (middle >> 16) +
				((highLow + (uint16_t)middle) >> 16) +
				((((low | carry) & ~sum) | (low & carry)) >> 31);
		} while (index != (unsigned int)words);
		destination[index] = carry;
	}
	return destination;
}

void MsifIntegerRemainder(uint32_t *destination, const uint32_t *source,
	int sourceWords, const uint32_t *modulus, int modulusWords)
{
	uint32_t normalizedModulus[65], product[65], normalizedSource[129];
	unsigned int sourceZeros = 0, modulusZeros = 0;
	unsigned int leadingZeros, shift;
	int index, sourceLength, modulusLength, sourceIsSmaller;

	if ((uint32_t)sourceWords - 1 > 127 ||
			(uint32_t)modulusWords - 1 > 63)
		return;
	index = 0;
	leadingZeros = 32;
	do {
		uint32_t word;

		++index;
		if (leadingZeros != 32)
			break;
		word = source[sourceWords - index];
		shift = ((int32_t)(0u - (word >> 16)) >> 16) & 16;
		leadingZeros = 16 - shift;
		word >>= shift;
		shift = ((int32_t)(word - 256) >> 16) & 8;
		word <<= shift;
		leadingZeros += shift;
		shift = ((int32_t)(word - 4096) >> 16) & 4;
		word <<= shift;
		leadingZeros += shift;
		shift = ((int32_t)(word - 16384) >> 16) & 2;
		word <<= shift;
		leadingZeros += shift + 2 - ((word >> 14) & ~(word >> 15));
		sourceZeros += leadingZeros;
	} while (index != sourceWords);
	index = 0;
	leadingZeros = 32;
	do {
		uint32_t word;

		++index;
		if (leadingZeros != 32)
			break;
		word = modulus[modulusWords - index];
		shift = ((int32_t)(0u - (word >> 16)) >> 16) & 16;
		leadingZeros = 16 - shift;
		word >>= shift;
		shift = ((int32_t)(word - 256) >> 16) & 8;
		word <<= shift;
		leadingZeros += shift;
		shift = ((int32_t)(word - 4096) >> 16) & 4;
		word <<= shift;
		leadingZeros += shift;
		shift = ((int32_t)(word - 16384) >> 16) & 2;
		word <<= shift;
		leadingZeros += shift + 2 - ((word >> 14) & ~(word >> 15));
		modulusZeros += leadingZeros;
	} while (index != modulusWords);
	modulusLength = modulusWords - (modulusZeros >> 5);
	if (modulusLength == 0)
		return;
	sourceLength = sourceWords - (sourceZeros >> 5);
	if (sourceLength == modulusLength)
		sourceIsSmaller = MsifIntegerGreaterThan(modulus, source, sourceLength);
	else
		sourceIsSmaller = sourceLength < modulusLength;
	if (sourceIsSmaller == 1) {
		for (index = 0; index < sourceLength; ++index)
			destination[index] = source[index];
		for (index = 0; index < modulusWords - sourceLength; ++index)
			destination[sourceLength + index] = 0;
	} else {
		uint32_t top;
		uint32_t *position;
		int remaining = sourceLength;

		shift = modulusZeros & 31;
		normalizedModulus[modulusLength] = 0;
		MsifIntegerShiftLeft(normalizedModulus, modulus, modulusLength, shift);
		if ((sourceZeros & 31) < shift)
			normalizedSource[sourceLength] = source[sourceLength - 1] >> (32 - shift);
		else
			normalizedSource[sourceLength] = 0;
		MsifIntegerShiftLeft(normalizedSource, source, sourceLength, shift);
		top = normalizedModulus[modulusLength - 1];
		position = &normalizedSource[sourceLength + 1];
		while (remaining >= modulusLength) {
			uint32_t high = *--position;
			uint32_t quotient, borrow;
			uint32_t *window = position - modulusLength;

			if (high >= top) {
				quotient = UINT32_MAX;
			} else {
				quotient = position[-1];
				for (index = 32; index != 0; --index) {
					uint32_t sign = (int32_t)high >> 31;
					uint32_t carry = quotient >> 31;

					quotient <<= 1;
					high = carry | (high << 1);
					if ((sign | high) >= top) {
						high -= top;
						quotient |= 1;
					}
				}
			}
			MsifIntegerMultiplyWord(product, normalizedModulus,
				modulusLength, quotient);
			borrow = MsifIntegerSubtract(window, window, product,
				modulusLength + 1, 0);
			while (borrow != 0)
				borrow ^= MsifIntegerAdd(window, window, normalizedModulus,
					modulusLength + 1, 0);
			--remaining;
		}
		MsifIntegerShiftRight(destination, normalizedSource, modulusWords, shift);
		for (index = 0; index < modulusWords - modulusLength; ++index)
			destination[modulusLength + index] = 0;
	}
}

void MsifIntegerShiftRight(uint32_t *destination, const uint32_t *source,
	int words, unsigned int shift)
{
	int index;

	shift &= 63;
	if (shift != 0) {
		uint32_t carry = 0;
		unsigned int reverse = 32 - shift;

		for (index = words - 1; index >= 0; --index) {
			uint32_t word = source[index];
			uint32_t shifted = shift < 32 ? word >> shift : 0;

			destination[index] = carry | shifted;
			carry = reverse < 32 ? word << reverse : 0;
		}
	} else {
		for (index = 0; index < words; ++index)
			destination[index] = source[index];
	}
}

uint32_t MsifIntegerSubtract(uint32_t *destination, const uint32_t *left,
	const uint32_t *right, int words, uint32_t borrow)
{
	int index = 0, count = 0;

	borrow &= 1;
	while (count < words) {
		uint32_t rightWord = right[index];
		uint32_t leftWord, difference;

		++count;
		leftWord = left[index];
		difference = leftWord - rightWord - borrow;
		destination[index] = difference;
		borrow = (((~leftWord | rightWord) & difference) |
			(~leftWord & rightWord)) >> 31;
		++index;
	}
	return borrow;
}

void MsifIntegerImport(uint32_t *destination, const uint8_t *source, int bytes)
{
	int words = bytes / 4;
	const uint8_t *end = source + bytes;
	int index;

	for (index = 0; index < words; ++index) {
		int byte = 0;

		*destination++ = end[-1];
		while (--byte != -4)
			destination[-1] |= (uint32_t)end[byte - 1] << (-8 * byte);
		end -= 4;
	}
}

int MsifIntegerInRange(const uint32_t *value, const uint32_t *limit, int words)
{
	int index = words;

	do {
		index = (int32_t)((uint32_t)index - 1);
	} while ((int32_t)((uint32_t)index + 1) > 0 && value[index] == 0);
	return index >= 0 && MsifIntegerGreaterThan(limit, value, words) == 1;
}

void MsifEcdsaMultiplyAdd(struct MsifEcdsaWordPoint *destination,
	const struct MsifEcdsaWordPoint *base,
	const struct MsifEcdsaWordPoint *publicKey, const uint32_t *baseScalar,
	const uint32_t *publicScalar, const uint32_t *prime,
	const uint32_t *coefficientA, int words)
{
	struct MsifEcdsaWordPoint result, convertedBase, convertedPublic;
	struct MsifEcdsaWordPoint sum, accumulator;
	uint32_t convertedA[7];
	uint32_t resultX[7], resultY[7], resultZ[7];
	uint32_t baseX[7], baseY[7], baseZ[7];
	uint32_t publicX[7], publicY[7], publicZ[7];
	uint32_t sumX[7], sumY[7], sumZ[7];
	uint32_t accumulatorX[7], accumulatorY[7], accumulatorZ[7];
	void (*doublePoint)(struct MsifEcdsaWordPoint *, const uint32_t *,
		const uint32_t *, int, uint32_t);
	uint32_t carry, montgomeryInverse, mask;
	int index, count, zeros, baseZeros, publicZeros, bit;

	if ((unsigned int)words - 1 > 6)
		return;
	result.x = resultX;
	result.y = resultY;
	result.z = resultZ;
	convertedBase.x = baseX;
	convertedBase.y = baseY;
	convertedBase.z = baseZ;
	convertedPublic.x = publicX;
	convertedPublic.y = publicY;
	convertedPublic.z = publicZ;
	index = 0;
	carry = 3;
	do {
		uint32_t coefficient = coefficientA[index];
		uint32_t added = coefficient + carry;

		if (added != prime[index])
			break;
		++index;
		carry = (coefficient & ~added) >> 31;
	} while (index != words);
	if (carry == 0 && index == words)
		doublePoint = MsifEcdsaDoubleMinusThree;
	else
		doublePoint = MsifEcdsaDouble;
	count = 31;
	montgomeryInverse = 1;
	mask = 2;
	do {
		uint32_t correction;

		--count;
		correction = (0u - *prime) * montgomeryInverse & mask;
		mask *= 2;
		montgomeryInverse |= correction;
	} while (count != 0);
	MsifEcdsaPrepare(&convertedBase, &convertedPublic, convertedA,
		base, publicKey, coefficientA, prime, words, montgomeryInverse);
	sum.x = sumX;
	sum.y = sumY;
	sum.z = sumZ;
	accumulator.x = accumulatorX;
	accumulator.y = accumulatorY;
	accumulator.z = accumulatorZ;
	zeros = 32;
	index = 0;
	baseZeros = 0;
	do {
		uint32_t word;
		unsigned int shift;

		++index;
		if (zeros != 32)
			break;
		word = baseScalar[words - index];
		shift = ((int32_t)(0u - (word >> 16)) >> 16) & 16;
		zeros = 16 - shift;
		word >>= shift;
		shift = ((int32_t)(word - 256) >> 16) & 8;
		word <<= shift;
		zeros += shift;
		shift = ((int32_t)(word - 4096) >> 16) & 4;
		word <<= shift;
		zeros += shift;
		shift = ((int32_t)(word - 16384) >> 16) & 2;
		word <<= shift;
		zeros += shift + 2 - ((word >> 14) & ~(word >> 15));
		baseZeros += zeros;
	} while (index != words);
	index = 0;
	zeros = 32;
	publicZeros = 0;
	do {
		uint32_t word;
		unsigned int shift;

		++index;
		if (zeros != 32)
			break;
		word = publicScalar[words - index];
		shift = ((int32_t)(0u - (word >> 16)) >> 16) & 16;
		zeros = 16 - shift;
		word >>= shift;
		shift = ((int32_t)(word - 256) >> 16) & 8;
		word <<= shift;
		zeros += shift;
		shift = ((int32_t)(word - 4096) >> 16) & 4;
		word <<= shift;
		zeros += shift;
		shift = ((int32_t)(word - 16384) >> 16) & 2;
		word <<= shift;
		zeros += shift + 2 - ((word >> 14) & ~(word >> 15));
		publicZeros += zeros;
	} while (index != words);
	if (publicZeros < baseZeros)
		baseZeros = publicZeros;
	bit = 32 * words - baseZeros;
	for (index = 0; index != words; ++index) {
		sum.x[index] = convertedBase.x[index];
		sum.y[index] = convertedBase.y[index];
		sum.z[index] = convertedBase.z[index];
	}
	MsifEcdsaPointAdd(&sum, &convertedPublic, convertedA, prime,
		words, montgomeryInverse);
	for (index = 1; index != words; ++index) {
		accumulator.x[index] = 0;
		accumulator.y[index] = 0;
		accumulator.z[index] = 0;
	}
	accumulator.x[0] = 1;
	accumulator.y[0] = 1;
	accumulator.z[0] = 0;
	while (--bit >= 0) {
		uint32_t selection;

		doublePoint(&accumulator, convertedA, prime, words, montgomeryInverse);
		selection = ((publicScalar[bit >> 5] >> (bit & 31)) & 1) |
			(2 * ((baseScalar[bit >> 5] >> (bit & 31)) & 1));
		switch (selection) {
		case 2:
			MsifEcdsaPointAdd(&accumulator, &convertedBase, convertedA,
				prime, words, montgomeryInverse);
			break;
		case 3:
			MsifEcdsaPointAdd(&accumulator, &sum, convertedA,
				prime, words, montgomeryInverse);
			break;
		case 1:
			MsifEcdsaPointAdd(&accumulator, &convertedPublic, convertedA,
				prime, words, montgomeryInverse);
			break;
		}
	}
	for (index = 0; index != words; ++index) {
		result.x[index] = accumulator.x[index];
		result.y[index] = accumulator.y[index];
		result.z[index] = accumulator.z[index];
	}
	MsifEcdsaToAffine(destination, &result, prime, words, montgomeryInverse);
}

void MsifEcdsaDoubleMinusThree(struct MsifEcdsaWordPoint *point,
	const uint32_t *coefficientA, const uint32_t *prime, int words,
	uint32_t montgomeryInverse)
{
	uint32_t slope[7], scaledX[7], temporary[7], squaredSlope[7];
	int index = words;

	if ((unsigned int)words - 1 > 6)
		return;
	while (index-- != 0) {
		if (point->z[index] == 0)
			continue;
		MsifMontgomeryMultiply(slope, point->z, point->z, prime,
			words, montgomeryInverse);
		MsifModularAdd(scaledX, point->x, slope, prime, words);
		MsifModularSubtract(temporary, point->x, slope, prime, words);
		MsifMontgomeryMultiply(slope, scaledX, temporary, prime,
			words, montgomeryInverse);
		MsifModularAdd(temporary, slope, slope, prime, words);
		MsifModularAdd(slope, temporary, slope, prime, words);
		MsifMontgomeryMultiply(temporary, point->y, point->z, prime,
			words, montgomeryInverse);
		MsifModularAdd(point->z, temporary, temporary, prime, words);
		MsifMontgomeryMultiply(scaledX, point->x, point->y, prime,
			words, montgomeryInverse);
		MsifMontgomeryMultiply(scaledX, scaledX, point->y, prime,
			words, montgomeryInverse);
		MsifModularAdd(scaledX, scaledX, scaledX, prime, words);
		MsifModularAdd(scaledX, scaledX, scaledX, prime, words);
		MsifMontgomeryMultiply(squaredSlope, slope, slope, prime,
			words, montgomeryInverse);
		MsifModularAdd(temporary, scaledX, scaledX, prime, words);
		MsifModularSubtract(point->x, squaredSlope, temporary, prime, words);
		MsifMontgomeryMultiply(temporary, point->y, point->y, prime,
			words, montgomeryInverse);
		MsifMontgomeryMultiply(temporary, temporary, temporary, prime,
			words, montgomeryInverse);
		MsifModularAdd(temporary, temporary, temporary, prime, words);
		MsifModularAdd(temporary, temporary, temporary, prime, words);
		MsifModularAdd(temporary, temporary, temporary, prime, words);
		MsifModularSubtract(scaledX, scaledX, point->x, prime, words);
		MsifMontgomeryMultiply(slope, slope, scaledX, prime,
			words, montgomeryInverse);
		MsifModularSubtract(point->y, slope, temporary, prime, words);
		break;
	}
}

void MsifEcdsaToMontgomery(struct MsifEcdsaWordPoint *destination,
	const struct MsifEcdsaWordPoint *source, const uint32_t *radixSquared,
	const uint32_t *prime, int words, uint32_t montgomeryInverse)
{
	int index = words;

	do {
		--index;
	} while (index + 1 > 0 && source->x[index] == 0 && source->y[index] == 0);
	if (index >= 0) {
		MsifMontgomeryMultiply(destination->x, source->x, radixSquared,
			prime, words, montgomeryInverse);
		MsifMontgomeryMultiply(destination->y, source->y, radixSquared,
			prime, words, montgomeryInverse);
		MsifMontgomeryReduce(destination->z, radixSquared, words,
			prime, words, montgomeryInverse);
	} else {
		for (index = 1; index < words; ++index) {
			destination->x[index] = 0;
			destination->y[index] = 0;
			destination->z[index] = 0;
		}
		destination->x[0] = 1;
		destination->y[0] = 1;
		destination->z[0] = 0;
	}
}

void MsifEcdsaPrepare(struct MsifEcdsaWordPoint *convertedBase,
	struct MsifEcdsaWordPoint *convertedPublic, uint32_t *convertedA,
	const struct MsifEcdsaWordPoint *base,
	const struct MsifEcdsaWordPoint *publicKey, const uint32_t *coefficientA,
	const uint32_t *prime, int words, uint32_t montgomeryInverse)
{
	uint32_t radixSquared[15];
	int index, length;

	if ((unsigned int)words - 1 > 6)
		return;
	length = 2 * words;
	index = 0;
	do {
		radixSquared[index++] = 0;
	} while (index < length);
	radixSquared[length] = 1;
	MsifIntegerRemainder(radixSquared, radixSquared, length + 1, prime, words);
	if (base != 0)
		MsifEcdsaToMontgomery(convertedBase, base, radixSquared,
			prime, words, montgomeryInverse);
	if (publicKey != 0)
		MsifEcdsaToMontgomery(convertedPublic, publicKey, radixSquared,
			prime, words, montgomeryInverse);
	if (coefficientA != 0)
		MsifMontgomeryMultiply(convertedA, radixSquared, coefficientA,
			prime, words, montgomeryInverse);
}

void MsifEcdsaToAffine(struct MsifEcdsaWordPoint *destination,
	const struct MsifEcdsaWordPoint *source, const uint32_t *prime,
	int words, uint32_t montgomeryInverse)
{
	uint32_t inverseZ[7];
	int index = words;

	if ((unsigned int)words - 1 > 6)
		return;
	while (index-- != 0) {
		if (source->z[index] == 0)
			continue;
		MsifMontgomeryMultiply(inverseZ, source->z, source->z,
			prime, words, montgomeryInverse);
		MsifMontgomeryMultiply(inverseZ, inverseZ, source->z,
			prime, words, montgomeryInverse);
		MsifMontgomeryReduce(inverseZ, inverseZ, words,
			prime, words, montgomeryInverse);
		MsifIntegerInverse(inverseZ, inverseZ, prime, words);
		MsifMontgomeryMultiply(destination->y, source->y, inverseZ,
			prime, words, montgomeryInverse);
		MsifMontgomeryMultiply(inverseZ, inverseZ, source->z,
			prime, words, montgomeryInverse);
		MsifMontgomeryMultiply(destination->x, source->x, inverseZ,
			prime, words, montgomeryInverse);
		return;
	}
	index = 0;
	do {
		destination->x[index] = 0;
		destination->y[index] = 0;
		++index;
	} while (index != words);
}

int MsifIntegerGreaterThan(const uint32_t *left, const uint32_t *right, int words)
{
	const uint32_t *leftEnd = left + words;
	const uint32_t *rightEnd = right + words;

	do {
		uint32_t leftWord, rightWord;

		if (leftEnd <= left)
			break;
		leftWord = *--leftEnd;
		rightWord = *--rightEnd;
		if (leftWord != rightWord)
			break;
	} while (1);
	return words > 0 && *leftEnd > *rightEnd;
}

void MsifIntegerShiftLeft(uint32_t *destination, const uint32_t *source,
	int words, unsigned int shift)
{
	int amount = shift & 63;
	int index;

	if (amount != 0) {
		uint32_t carry = 0;
		unsigned int reverse = 32u - amount;

		for (index = 0; index < words; ++index) {
			uint32_t word = source[index];

			destination[index] = carry | (amount < 32 ? word << amount : 0);
			carry = reverse < 32 ? word >> reverse : 0;
		}
	} else {
		while (amount < words) {
			destination[amount] = source[amount];
			++amount;
		}
	}
}

uint32_t MsifIntegerAdd(uint32_t *destination, const uint32_t *left,
	const uint32_t *right, int words, uint32_t carry)
{
	int index = 0, count = 0;

	carry &= 1;
	while (count < words) {
		uint32_t leftWord = left[index];
		uint32_t rightWord, sum;

		++count;
		rightWord = right[index];
		sum = leftWord + rightWord + carry;
		destination[index] = sum;
		carry = (((rightWord | leftWord) & ~sum) |
			(leftWord & rightWord)) >> 31;
		++index;
	}
	return carry;
}

__attribute__((noinline)) void MsifMontgomeryMultiply(uint32_t *destination, const uint32_t *left,
	const uint32_t *right, const uint32_t *prime, int words,
	uint32_t montgomeryInverse)
{
	uint32_t accumulator[65], rightProduct[65], primeProduct[65];
	uint32_t carry;
	int index;

	if ((unsigned int)words - 1 > 63)
		return;
	for (index = 0; index <= words; ++index)
		accumulator[index] = 0;
	index = 0;
	do {
		uint32_t productCarry;

		MsifIntegerMultiplyWord(primeProduct, prime, words,
			(accumulator[0] + *right * *left) * montgomeryInverse);
		MsifIntegerMultiplyWord(rightProduct, right, words, *left++);
		productCarry = MsifIntegerAdd(primeProduct, primeProduct,
			rightProduct, words + 1, 0);
		carry = MsifIntegerAdd(accumulator, accumulator + 1,
			primeProduct + 1, words, (accumulator[0] | primeProduct[0]) != 0) +
			productCarry;
		accumulator[words] = carry;
		++index;
	} while (index != words);
	if (carry != 0 || MsifIntegerGreaterThan(prime, accumulator, words) == 0)
		MsifIntegerSubtract(accumulator, accumulator, prime, words, 0);
	for (index = 0; index != words; ++index)
		destination[index] = accumulator[index];
}

__attribute__((noinline)) void MsifMontgomeryReduce(uint32_t *destination, const uint32_t *source,
	int sourceWords, const uint32_t *prime, int words,
	uint32_t montgomeryInverse)
{
	uint32_t carries[64];
	union {
		uint32_t primeProduct[65];
		struct {
			uint32_t lowProduct[64];
			uint32_t accumulator[129];
		} reduction;
	} storage;
	uint32_t *position, *high;
	int index;

	if ((unsigned int)words - 1 > 63 || sourceWords < words || sourceWords > 2 * words)
		return;
	/* At 64 words, the product carry occupies the unused accumulator prefix. */
	high = &storage.reduction.accumulator[words + 1];
	index = 0;
	do {
		storage.reduction.accumulator[index + 1] = source[index];
		high[index] = 0;
		++index;
	} while (index != words);
	for (index = 0; index != sourceWords - words; ++index)
		high[index] = source[words + index];
	position = storage.reduction.accumulator;
	for (index = 0; index != words; ++index) {
		uint32_t word = *++position;
		uint32_t productCarry;

		MsifIntegerMultiplyWord(storage.primeProduct, prime, words,
			word * montgomeryInverse);
		productCarry = storage.primeProduct[words];
		carries[index] = MsifIntegerAdd(position, position,
			storage.primeProduct, words, 0) + productCarry;
	}
	if (MsifIntegerAdd(high, high, carries, words, 0) != 0 ||
		MsifIntegerGreaterThan(prime, high, words) == 0)
		MsifIntegerSubtract(high, high, prime, words, 0);
	index = 0;
	do {
		destination[index] = high[index];
		++index;
	} while (index != words);
}

uint32_t *MsifModularAdd(uint32_t *destination, const uint32_t *left,
	const uint32_t *right, const uint32_t *prime, int words)
{
	uint32_t carry = 0;
	int index = 0, count = 0;
	int remaining;

	while (count < words) {
		uint32_t leftWord = left[index];
		uint32_t rightWord, sum;

		++count;
		rightWord = right[index];
		sum = leftWord + rightWord + carry;
		destination[index] = sum;
		carry = (((rightWord | leftWord) & ~sum) |
			(rightWord & leftWord)) >> 31;
		++index;
	}
	remaining = words & ~(words >> 31);
	if (carry == 0) {
		const uint32_t *end = destination + remaining;
		const uint32_t *primeEnd = prime + remaining;

		do {
			if (remaining-- <= 0)
				break;
			if (*--end != *--primeEnd)
				break;
		} while (1);
		if (remaining >= 0 && destination[remaining] < prime[remaining])
			return destination;
	}
	carry = 0;
	index = 0;
	while (index < words) {
		uint32_t primeWord = prime[index++];
		uint32_t word = *destination;
		uint32_t difference = word - primeWord - carry;

		*destination++ = difference;
		carry = (((~word | primeWord) & difference) |
			(primeWord & ~word)) >> 31;
	}
	return destination;
}

uint32_t *MsifModularSubtract(uint32_t *destination, const uint32_t *left,
	const uint32_t *right, const uint32_t *prime, int words)
{
	uint32_t borrow = 0;
	int index = 0, count = 0;

	while (count < words) {
		uint32_t rightWord = right[index];
		uint32_t leftWord, difference;

		++count;
		leftWord = left[index];
		difference = leftWord - rightWord - borrow;
		destination[index] = difference;
		borrow = (((~leftWord | rightWord) & difference) |
			(~leftWord & rightWord)) >> 31;
		++index;
	}
	if (borrow != 0) {
		uint32_t carry = 0;
		int correction = 0;

		while (correction < words) {
			uint32_t primeWord = prime[correction++];
			uint32_t word = *destination;
			uint32_t sum = primeWord + word + carry;

			*destination++ = sum;
			carry = (((primeWord | word) & ~sum) |
				(word & primeWord)) >> 31;
		}
	}
	return destination;
}

void MsifEcdsaPointAdd(struct MsifEcdsaWordPoint *destination,
	const struct MsifEcdsaWordPoint *source, const uint32_t *coefficientA,
	const uint32_t *prime, int words, uint32_t montgomeryInverse)
{
	uint32_t scaledSourceX[7], scaledDestinationX[7], differenceX[7];
	uint32_t scaledSourceY[7], temporary[7], differenceY[7];
	uint32_t carry;
	int index;

	if ((unsigned int)words - 1 > 6)
		return;
	index = words;
	while (--index >= 0 && destination->z[index] == 0)
		;
	if (index < 0) {
		index = 0;
		do {
			destination->x[index] = source->x[index];
			destination->y[index] = source->y[index];
			destination->z[index] = source->z[index];
			++index;
		} while (index != words);
		return;
	}
	index = words;
	while (--index >= 0 && source->z[index] == 0)
		;
	if (index < 0)
		return;
	MsifMontgomeryMultiply(scaledSourceY, destination->z, destination->z,
		prime, words, montgomeryInverse);
	MsifMontgomeryMultiply(temporary, source->z, source->z,
		prime, words, montgomeryInverse);
	MsifMontgomeryMultiply(scaledSourceX, source->x, scaledSourceY,
		prime, words, montgomeryInverse);
	MsifMontgomeryMultiply(scaledDestinationX, destination->x, temporary,
		prime, words, montgomeryInverse);
	MsifModularSubtract(differenceX, scaledSourceX, scaledDestinationX,
		prime, words);
	MsifMontgomeryMultiply(scaledSourceY, scaledSourceY, destination->z,
		prime, words, montgomeryInverse);
	MsifMontgomeryMultiply(temporary, temporary, source->z,
		prime, words, montgomeryInverse);
	MsifMontgomeryMultiply(scaledSourceY, scaledSourceY, source->y,
		prime, words, montgomeryInverse);
	MsifMontgomeryMultiply(temporary, temporary, destination->y,
		prime, words, montgomeryInverse);
	MsifModularSubtract(differenceY, scaledSourceY, temporary, prime, words);
	index = words;
	while (--index >= 0 && differenceX[index] == 0)
		;
	if (index < 0) {
		index = words;
		while (--index >= 0 && differenceY[index] == 0)
			;
		if (index < 0) {
			MsifEcdsaDouble(destination, coefficientA, prime, words,
				montgomeryInverse);
			return;
		}
		for (index = 1; index != words; ++index) {
			destination->x[index] = 0;
			destination->y[index] = 0;
			destination->z[index] = 0;
		}
		destination->x[0] = 1;
		destination->y[0] = 1;
		destination->z[0] = 0;
		return;
	}
	MsifModularAdd(scaledSourceX, scaledSourceX, scaledDestinationX,
		prime, words);
	MsifModularAdd(scaledSourceY, scaledSourceY, temporary, prime, words);
	MsifMontgomeryMultiply(temporary, source->z, destination->z,
		prime, words, montgomeryInverse);
	MsifMontgomeryMultiply(destination->z, temporary, differenceX,
		prime, words, montgomeryInverse);
	MsifMontgomeryMultiply(scaledDestinationX, differenceY, differenceY,
		prime, words, montgomeryInverse);
	MsifMontgomeryMultiply(temporary, differenceX, differenceX,
		prime, words, montgomeryInverse);
	MsifMontgomeryMultiply(scaledSourceX, scaledSourceX, temporary,
		prime, words, montgomeryInverse);
	MsifModularSubtract(destination->x, scaledDestinationX, scaledSourceX,
		prime, words);
	MsifModularSubtract(scaledSourceX, scaledSourceX, destination->x, prime, words);
	MsifModularSubtract(scaledSourceX, scaledSourceX, destination->x, prime, words);
	MsifMontgomeryMultiply(scaledSourceX, scaledSourceX, differenceY,
		prime, words, montgomeryInverse);
	MsifMontgomeryMultiply(temporary, temporary, differenceX,
		prime, words, montgomeryInverse);
	MsifMontgomeryMultiply(scaledSourceY, scaledSourceY, temporary,
		prime, words, montgomeryInverse);
	MsifModularSubtract(temporary, scaledSourceX, scaledSourceY, prime, words);
	carry = temporary[0] & 1;
	if (carry != 0)
		carry = MsifIntegerAdd(temporary, temporary, prime, words, 0) << 31;
	index = words;
	do {
		uint32_t word = temporary[--index];

		destination->y[index] = carry | (word >> 1);
		carry = word << 31;
	} while (index != 0);
}

void MsifEcdsaDouble(struct MsifEcdsaWordPoint *point,
	const uint32_t *coefficientA, const uint32_t *prime, int words,
	uint32_t montgomeryInverse)
{
	uint32_t slope[7], scaledX[7], temporary[7], squaredSlope[7];
	int index;

	if ((unsigned int)words - 1 > 6)
		return;
	index = words;
	while (--index >= 0 && point->z[index] == 0)
		;
	if (index < 0)
		return;
	MsifMontgomeryMultiply(slope, point->x, point->x,
		prime, words, montgomeryInverse);
	MsifMontgomeryMultiply(scaledX, point->z, point->z,
		prime, words, montgomeryInverse);
	MsifMontgomeryMultiply(scaledX, scaledX, scaledX,
		prime, words, montgomeryInverse);
	MsifModularAdd(temporary, slope, slope, prime, words);
	MsifModularAdd(slope, slope, temporary, prime, words);
	MsifMontgomeryMultiply(scaledX, coefficientA, scaledX,
		prime, words, montgomeryInverse);
	MsifModularAdd(slope, slope, scaledX, prime, words);
	MsifMontgomeryMultiply(temporary, point->y, point->z,
		prime, words, montgomeryInverse);
	MsifModularAdd(point->z, temporary, temporary, prime, words);
	MsifMontgomeryMultiply(scaledX, point->x, point->y,
		prime, words, montgomeryInverse);
	MsifMontgomeryMultiply(scaledX, scaledX, point->y,
		prime, words, montgomeryInverse);
	MsifModularAdd(scaledX, scaledX, scaledX, prime, words);
	MsifModularAdd(scaledX, scaledX, scaledX, prime, words);
	MsifMontgomeryMultiply(squaredSlope, slope, slope,
		prime, words, montgomeryInverse);
	MsifModularAdd(temporary, scaledX, scaledX, prime, words);
	MsifModularSubtract(point->x, squaredSlope, temporary, prime, words);
	MsifMontgomeryMultiply(temporary, point->y, point->y,
		prime, words, montgomeryInverse);
	MsifMontgomeryMultiply(temporary, temporary, temporary,
		prime, words, montgomeryInverse);
	MsifModularAdd(temporary, temporary, temporary, prime, words);
	MsifModularAdd(temporary, temporary, temporary, prime, words);
	MsifModularAdd(temporary, temporary, temporary, prime, words);
	MsifModularSubtract(scaledX, scaledX, point->x, prime, words);
	MsifMontgomeryMultiply(slope, slope, scaledX,
		prime, words, montgomeryInverse);
	MsifModularSubtract(point->y, slope, temporary, prime, words);
}
