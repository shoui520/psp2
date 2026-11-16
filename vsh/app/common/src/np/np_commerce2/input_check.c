/*
 * SceNpCommerce2: the checks of the texts the application gives (an account's name, its password, its
 * e-mail address, a free text). They set bits of a flag word: nothing set is a text that passes.
 */

#include "np_commerce2_internal.h"

/* the text has no character that is not a printable ASCII character (the length is cut off at max) */
#define NP_COMMERCE2_ONLY_PRINTABLE(text, max)							\
	({											\
		SceSize i_ = 0;									\
		const char *p_;									\
		for (p_ = (text); i_ < sceClibStrnlen((text), (max)); p_++) {			\
			SceUInt32 c_ = *p_;							\
			if (c_ > 0x7f || (sceClibLookCtypeTable(c_) & 0x97) == 0)		\
				break;								\
			i_++;									\
		}										\
		i_ == sceClibStrnlen((text), (max));						\
	})

static int npCommerce2IsLetter(int c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

/* sets the bits of what is wrong with a text of one of the kinds (16: an account's login name, 39: an e-mail
 * address, 40: a password, 0xff00: any text): 1 empty, 2 length, 4 no digit, 8 no letter, 0x10 a character
 * three times in a row, 0x20 fewer than three different characters, 0x40 a name that does not start with a
 * letter, 0x80 a character that does not belong */
int npCommerce2CheckInput(int kind, const char *text, SceUInt32 *flags)
{
	char seen[4];
	int ret = 0;
	SceSize i;
	SceUInt32 run;
	SceUInt32 count;
	SceUInt32 j;
	const char *p;
	char previous;
	int c;

	switch (kind) {
	case 39:
		if (sceClibStrnlen(text, 0x40) != 0) {
			if (!NP_COMMERCE2_ONLY_PRINTABLE(text, 0x40))
				*flags |= 0x80;
			if (*text == '@' || text[sceClibStrnlen(text, 0x40) - 1] == '@' ||
			    (p = sceClibStrchr(text, '@')) == NULL || sceClibStrchr(p, '.') == NULL)
				*flags |= 0x80;
		} else {
			*flags |= 1;
		}
		break;
	case 40:
		if (sceClibStrnlen(text, 0x1e) != 0) {
			if (!NP_COMMERCE2_ONLY_PRINTABLE(text, 0x1e))
				*flags |= 0x80;
			if (sceClibStrnlen(text, 0x1e) < 8 || sceClibStrnlen(text, 0x1e) > 0x1e)
				*flags |= 2;
			i = 0;
			for (p = text; i < sceClibStrnlen(text, 0x1e); p++) {
				c = *p;
				if (c >= '0' && c <= '9')
					break;
				i++;
			}
			if (i == sceClibStrnlen(text, 0x1e))
				*flags |= 4;
			i = 0;
			for (p = text; i < sceClibStrnlen(text, 0x1e); p++) {
				c = *p;
				if (npCommerce2IsLetter(c))
					break;
				i++;
			}
			if (i == sceClibStrnlen(text, 0x1e))
				*flags |= 8;
			i = 0;
			run = 0;
			previous = 0;
			p = text;
			while (i < sceClibStrnlen(text, 0x1e)) {
				SceUInt32 now = 1;

				if (sceClibToupper(previous) == sceClibToupper(*p))
					now = run + 1;
				run = now;
				if (run >= 3) {
					*flags |= 0x10;
					break;
				}
				previous = *p++;
				i++;
			}
			i = 0;
			count = 0;
			p = text;
			while (i < sceClibStrnlen(text, 0x1e)) {
				j = 0;
				while (j < count && seen[j] != *p)
					j++;
				if (j == count)
					seen[count++] = *p;
				if (count >= 3)
					break;
				p++;
				i++;
			}
			if (i == sceClibStrnlen(text, 0x1e))
				*flags |= 0x20;
		} else {
			*flags |= 1;
		}
		break;
	case 16:
		if (sceClibStrnlen(text, 0x10) != 0) {
			if (sceClibStrnlen(text, 0x10) < 3 || sceClibStrnlen(text, 0x10) > 0x10)
				*flags |= 2;
			if (!npCommerce2IsLetter(*text))
				*flags |= 0x40;
			i = 1;
			for (p = text + 1; i < sceClibStrnlen(text, 0x10); p++) {
				c = *p;
				if (!npCommerce2IsLetter(c) && !(c >= '0' && c <= '9') && c != '_' && c != '-')
					break;
				i++;
			}
			if (i != sceClibStrnlen(text, 0x10))
				*flags |= 0x80;
		} else {
			*flags |= 1;
		}
		break;
	case 0xff00:
		if (sceClibStrnlen(text, 0x400) != 0) {
			if (!NP_COMMERCE2_ONLY_PRINTABLE(text, 0x400))
				*flags |= 0x80;
		} else {
			*flags |= 1;
		}
		break;
	default:
		ret = SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
		break;
	}
	return ret;
}
