/*
 * ScebXCe's parsers: the SAX parser, which hands each token to the user's handler, and the
 * document builder, which makes a tree of nodes from them; with the readers of character
 * references they share.
 */
#include "bxce_internal.h"
#include "byte_type.h"

/* UTF-8: the largest code point each length holds, its lead byte, and the trail bytes' bits */
SceUInt32 g_bxceUtf8Max[6] = { 0x7f, 0x7ff, 0xffff, 0x1fffff, 0x3ffffff, 0x7fffffff };
SceUInt32 g_bxceUtf8Lead[6] = { 0x00, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc };
SceUInt32 g_bxceUtf8TrailMask = 0x3f;
SceUInt32 g_bxceUtf8Trail = 0x80;

/* what each predefined entity stands for, in BXCE_REF_* order */
const char *g_bxceEntityText[5] = { "&", "\"", "<", ">", "'" };

/* a character reference's code point as UTF-8, NUL-terminated; XML's Char production checked */
int ScebXCe_BD9D727B(SceUInt32 value, char **out, int *length)
{
	char *s = *out;
	int count;

	if ((int)value < 9 || value == 11 || value == 12 ||
	    ((int)value > 13 && (int)value < 32) ||
	    ((int)value > 0xd7ff && (int)value < 0xe000) ||
	    ((int)value > 0xfffd && (int)value < 0x10000) ||
	    (int)value > 0x10ffff) {
		*s = 0;
		*length = 0;
		return SCE_BXCE_ERROR_INVALID_CHARACTER;
	}

	if ((value & 0xff80) == 0) {
		count = 1;
		*s = value;
	} else if (s == NULL) {
		count = 0;
	} else {
		char *p;
		int i;

		for (count = 1; count < 7; count++) {
			if (value <= g_bxceUtf8Max[count - 1])
				break;
		}
		if (count > 1) {
			p = &s[count - 1];
			for (i = count - 1; i != 0; i--) {
				*p-- = (value & g_bxceUtf8TrailMask) | g_bxceUtf8Trail;
				value >>= 6;
			}
		}
		*s = value | g_bxceUtf8Lead[count - 1];
	}

	s[count] = 0;
	*length = count;
	return 0;
}

/* a decimal character reference's digits */
int ScebXCe_CC190FA8(SceUInt32 *value, const char *digits, SceUInt32 length)
{
	SceUInt32 i;
	int result;

	if (length == 0)
		return SCE_BXCE_ERROR_XML_UNEXPECTED_EOF;

	result = 0;
	for (i = 0; i < length; i++) {
		if (s_bxceByteType[(SceUInt8)*digits] != BXCE_BT_DIGIT)
			return SCE_BXCE_ERROR_INVALID_DECIMAL;
		result = result * 10 + *digits - '0';
		digits++;
	}

	*value = result;
	return 0;
}

/* a hexadecimal character reference's digits */
int ScebXCe_DFB92627(SceUInt32 *value, const char *digits, SceUInt32 length)
{
	SceUInt32 i;
	int result;

	if (length == 0)
		return SCE_BXCE_ERROR_XML_UNEXPECTED_EOF;

	result = 0;
	i = 0;
	do {
		char c = *digits;

		if (s_bxceByteType[(SceUInt8)c] == BXCE_BT_DIGIT)
			result = result * 16 + c - '0';
		else if (c >= 'a' && c <= 'f')
			result = result * 16 + c - 'a' + 10;
		else if (c >= 'A' && c <= 'F')
			result = result * 16 + c - 'A' + 10;
		else
			return SCE_BXCE_ERROR_INVALID_HEXADECIMAL;
		i++;
		digits++;
	} while (i < length);

	*value = result;
	return 0;
}

/* the UTF-8 byte order mark */
int bxceIsUtf8Bom(const char *s)
{
	if (s == NULL)
		return 0;
	return (SceUInt8)s[0] == 0xef && (SceUInt8)s[1] == 0xbb && (SceUInt8)s[2] == 0xbf;
}
