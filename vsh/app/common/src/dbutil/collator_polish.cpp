/*
 * The Polish collator (collator.h). The names are ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include "collator.h"

/* the order of the character groups in a sort key */
const SceUInt8 CollatorPolish::s_groupOrder[COLLATOR_CHAR_TYPE_COUNT] =
	{ 2, 3, 1, 4, 4, 4, 4, 4, 4 };

/* the letters' weights, by character */
static const SceUInt8 s_weight[0x17d] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 1, 3, 4, 6, 7, 9, 10, 11, 12, 13, 14, 15, 17, 18, 20,
	22, 23, 24, 25, 27, 28, 29, 30, 31, 32, 33, 0, 0, 0, 0, 0,
	0, 1, 3, 4, 6, 7, 9, 10, 11, 12, 13, 14, 15, 17, 18, 20,
	22, 23, 24, 25, 27, 28, 29, 30, 31, 32, 33, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 21, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 21, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 2, 2, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 8, 8, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 16, 16, 19, 19, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 26, 26, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 34, 34, 35, 35,
};

unsigned int CollatorPolish::RemoveAccent(unsigned int c)
{
	switch (c) {
	case 0xd3:	/* Ó */
	case 0xf3:
	case 0x104:	/* Ą */
	case 0x105:
	case 0x106:	/* Ć */
	case 0x107:
	case 0x118:	/* Ę */
	case 0x119:
	case 0x141:	/* Ł */
	case 0x142:
	case 0x143:	/* Ń */
	case 0x144:
	case 0x15a:	/* Ś */
	case 0x15b:
	case 0x179:	/* Ź */
	case 0x17a:
	case 0x17b:	/* Ż */
	case 0x17c:
		return c;
	}
	return Collator::RemoveAccent(c);
}

unsigned int CollatorPolish::GetLatinWeight(unsigned int c)
{
	if (!m_attribute[COLLATOR_ATTR_CASE_SENSITIVE])
		return s_weight[c];
	if ((c >= 'A' && c <= 'Z') || c == 0x104 || c == 0x106 || c == 0x118 || c == 0x141 ||
	    c == 0x143 || c == 0xd3 || c == 0x15a || c == 0x179 || c == 0x17b)
		return s_weight[c] * 2 + !m_attribute[COLLATOR_ATTR_UPPER_FIRST];
	if ((c >= 'a' && c <= 'z') || c == 0x105 || c == 0x107 || c == 0x119 || c == 0x142 ||
	    c == 0x144 || c == 0xf3 || c == 0x15b || c == 0x17a || c == 0x17c)
		return s_weight[c] * 2 + !m_attribute[COLLATOR_ATTR_LOWER_FIRST];
	return c;
}

int CollatorPolish::GetIndexOfLetter(unsigned int c)
{
	switch (c) {
	case 0x104:
	case 0x105:
		return 0;
	case 0x106:
	case 0x107:
		return 2;
	case 0x118:
	case 0x119:
		return 4;
	case 0x141:
	case 0x142:
		return 11;
	case 0x143:
	case 0x144:
		return 13;
	case 0xd3:
	case 0xf3:
		return 14;
	case 0x15a:
	case 0x15b:
		return 18;
	case 0x179:
	case 0x17a:
	case 0x17b:
	case 0x17c:
		return 25;
	}
	return Collator::GetIndexOfLetter(c);
}
