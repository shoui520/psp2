/*
 * The Korean collator (collator.h). The names are ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include "collator.h"

/* the order of the character groups in a sort key */
const SceUInt8 CollatorKorean::s_groupOrder[COLLATOR_CHAR_TYPE_COUNT] =
	{ 2, 3, 1, 5, 5, 4, 5, 5, 5 };

int CollatorKorean::GetCharType(unsigned int c)
{
	if (IsLatin(c))
		return COLLATOR_CHAR_LATIN;
	if (IsDigit(c))
		return COLLATOR_CHAR_DIGIT;
	if (IsSymbol(c))
		return COLLATOR_CHAR_SYMBOL;
	if (IsHangul(c))
		return COLLATOR_CHAR_HANGUL;
	return COLLATOR_CHAR_OTHER;
}
