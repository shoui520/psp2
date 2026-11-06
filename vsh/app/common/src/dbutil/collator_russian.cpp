/*
 * The Russian collator (collator.h). The names are ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include "collator.h"

/* the order of the character groups in a sort key */
const SceUInt8 CollatorRussian::s_groupOrder[COLLATOR_CHAR_TYPE_COUNT] =
	{ 2, 3, 1, 4, 5, 5, 5, 5, 5 };

int CollatorRussian::GetCharType(unsigned int c)
{
	if (IsLatin(c))
		return COLLATOR_CHAR_LATIN;
	if (IsDigit(c))
		return COLLATOR_CHAR_DIGIT;
	if (IsSymbol(c))
		return COLLATOR_CHAR_SYMBOL;
	if (IsCyrillic(c))
		return COLLATOR_CHAR_CYRILLIC;
	return COLLATOR_CHAR_OTHER;
}

/* the Cyrillic capitals and small letters, U+0410-U+044F, by letter */
unsigned int CollatorRussian::GetCyrillicWeight(unsigned int c)
{
	if (m_attribute[COLLATOR_ATTR_CASE_SENSITIVE]) {
		if (c >= 0x410 && c <= 0x42f)
			return (c - 0x410) * 2 + !m_attribute[COLLATOR_ATTR_UPPER_FIRST];
		if (c >= 0x430 && c <= 0x44f)
			return (c - 0x430) * 2 + !m_attribute[COLLATOR_ATTR_LOWER_FIRST];
	} else if (c >= 0x410 && c <= 0x42f) {
		c -= 0x410;
	} else if (c >= 0x430 && c <= 0x44f) {
		c -= 0x430;
	}
	return c;
}
