/*
 * The Traditional Chinese collator (collator.h). The names are ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include "collator.h"

/* the order of the character groups in a sort key */
const SceUInt8 CollatorTraditionalChinese::s_groupOrder[COLLATOR_CHAR_TYPE_COUNT] =
	{ 2, 3, 1, 5, 5, 5, 4, 5, 5 };

int CollatorTraditionalChinese::GetCharType(unsigned int c)
{
	if (IsLatin(c))
		return COLLATOR_CHAR_LATIN;
	if (IsDigit(c))
		return COLLATOR_CHAR_DIGIT;
	if (IsSymbol(c))
		return COLLATOR_CHAR_SYMBOL;
	if (IsHan(c))
		return COLLATOR_CHAR_HAN;
	return COLLATOR_CHAR_OTHER;
}
