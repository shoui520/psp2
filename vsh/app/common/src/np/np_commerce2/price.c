/*
 * SceNpCommerce2: the text of a price: the amount in the currency of a session, with its thousands
 * separators and its decimal letter, and the currency's code before or after it.
 */

#include "np_commerce2_internal.h"

/* copies a text to the place, when it fits with its end; gives the length or the error of a place too small */
#define NP_COMMERCE2_APPEND(result, place, text, size)							\
	do {												\
		(result) = SCE_NP_COMMERCE2_ERROR_INSUFFICIENT_BUF_SIZE;				\
		if ((SceUInt32)((place) + (size)) > (SceUInt32)((place) + sceClibStrnlen((text), (size)))) {	\
			sceClibStrncpy((place), (text), (size));					\
			(result) = sceClibStrnlen((text), (size));					\
		}											\
	} while (0)

/* the currency's code, with the space the price's layout asks for; gives the length it took */
int npCommerce2WriteCurrency(const SceNpCommerce2SessionInfo *info, char *place, SceSize size)
{
	char *start = place;
	SceSize maxSize = size;
	int ret;

	if (info->symbolPosition == SCE_NP_COMMERCE2_SYM_POS_POST) {
		NP_COMMERCE2_APPEND(ret, place, " ", size);
		if (ret < 0)
			return ret;
		place += ret;
		size -= ret;
	}
	NP_COMMERCE2_APPEND(ret, place, info->currencyCode, size);
	if (ret >= 0) {
		place += ret;
		ret = place - start;
		if (info->symbolPosition == SCE_NP_COMMERCE2_SYM_POS_PRE) {
			size = maxSize - ret;
			NP_COMMERCE2_APPEND(ret, place, " ", size);
			if (ret >= 0)
				return place + ret - start;
		}
	}
	return ret;
}

int sceNpCommerce2GetPrice(const SceNpCommerce2SessionInfo *info, char *buf, SceSize buflen, SceUInt32 price)
{
	char out[32];
	char digits[32];
	char *place = out;
	SceSize prefix = 0;
	SceSize remaining;
	SceSize length;
	int integerDigits;
	int ret = 0;
	int count;
	int i;

	if (g_initialized == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (info == NULL || buf == NULL)
		return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;

	if (info->symbolPosition == SCE_NP_COMMERCE2_SYM_POS_PRE) {
		ret = npCommerce2WriteCurrency(info, out, 32);
		if (ret < 0)
			return ret;
		prefix = ret;
		place = out + ret;
	}
	sceClibSnprintf(digits, 32, "%u", price);
	digits[31] = '\0';
	length = sceClibStrnlen(digits, buflen);
	if (out + 32 - prefix <= place + length + (sceClibStrnlen(digits, buflen) - 1) / 3 + 1)
		return SCE_NP_COMMERCE2_ERROR_INSUFFICIENT_BUF_SIZE;

	count = 0;
	integerDigits = sceClibStrnlen(digits, buflen) - info->decimals;
	if (integerDigits <= 0) {
		NP_COMMERCE2_APPEND(ret, place, "0", 32 - prefix);
		if (ret < 0)
			return ret;
		place += ret;
	} else {
		if (integerDigits != 0) {
			int left = integerDigits;
			SceUInt32 after = integerDigits - 1;

			for (i = 0; i < integerDigits; i++) {
				*place = digits[i];
				*++place = 0;
				if (left != 1 && after % 3 == 0) {
					NP_COMMERCE2_APPEND(ret, place, info->thousandSeparator, 32 - (place - out));
					if (ret < 0)
						return ret;
					place += ret;
				}
				left--;
				after--;
			}
		}
	}
	if (info->decimals != 0 && sceClibStrnlen(info->decimalLetter, 4) != 0) {
		const char *letter = info->decimalLetter;

		NP_COMMERCE2_APPEND(ret, place, letter, 32 - (place - out));
		if (ret < 0)
			return ret;
		place += ret;
		remaining = 32 - (place - out);
		if (integerDigits <= 0) {
			SceUInt32 zeros = -integerDigits;
			SceUInt32 done = 0;

			if (integerDigits != 0) {
				for (;;) {
					NP_COMMERCE2_APPEND(ret, place, "0", remaining);
					if (ret < 0)
						return ret;
					place += ret;
					done++;
					remaining = 32 - (place - out);
					if (done >= zeros)
						break;
				}
			}
			NP_COMMERCE2_APPEND(ret, place, digits, remaining);
			if (ret < 0)
				return ret;
			place += ret;
		} else {
			NP_COMMERCE2_APPEND(ret, place, digits + integerDigits, 32 - (place - out));
			if (ret < 0)
				return ret;
			place += ret;
		}
	}
	if (info->symbolPosition == SCE_NP_COMMERCE2_SYM_POS_POST) {
		ret = npCommerce2WriteCurrency(info, place, 32 - (place - out));
		if (ret < 0)
			return ret;
	}
	if (buflen > sceClibStrnlen(out, buflen))
		sceClibStrncpy(buf, out, buflen);
	else
		ret = SCE_NP_COMMERCE2_ERROR_INSUFFICIENT_BUF_SIZE;
	return ret;
}
