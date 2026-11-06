/*
 * The Turkish collator (collator.h). The names are ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include "collator.h"

/* the order of the character groups in a sort key */
const SceUInt8 CollatorTurkish::s_groupOrder[COLLATOR_CHAR_TYPE_COUNT] =
	{ 2, 3, 1, 4, 4, 4, 4, 4, 4 };

/* the letters' weights, by character */
static const SceUInt8 s_weight[0x160] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 1, 2, 3, 5, 6, 7, 8, 10, 11, 13, 14, 15, 16, 17, 18,
	20, 33, 22, 23, 25, 26, 28, 34, 35, 31, 32, 0, 0, 0, 0, 0,
	0, 1, 2, 3, 5, 6, 7, 8, 10, 12, 13, 14, 15, 16, 17, 18,
	20, 33, 22, 23, 25, 26, 28, 34, 35, 31, 32, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 19, 0, 0, 0, 0, 0, 27, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 19, 0, 0, 0, 0, 0, 27, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 9,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	12, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 24, 24,
};

/* the dotted and dotless i: I is ı's capital, İ is i's */
unsigned int CollatorTurkish::ToLower(unsigned int c)
{
	switch (c) {
	case 'I':
		return 0x131;
	case 0xc7:	/* Ç */
		return 0xe7;
	case 0xd6:	/* Ö */
		return 0xf6;
	case 0xdc:	/* Ü */
		return 0xfc;
	case 0x11e:	/* Ğ */
		return 0x11f;
	case 0x130:	/* İ */
		return 'i';
	case 0x15e:	/* Ş */
		return 0x15f;
	}
	if (c >= 'A' && c <= 'Z')
		return c + ('a' - 'A');
	return c;
}

unsigned int CollatorTurkish::RemoveAccent(unsigned int c)
{
	switch (c) {
	case 0xc7:
	case 0xd6:
	case 0xdc:
	case 0xe7:
	case 0xf6:
	case 0xfc:
	case 0x11e:
	case 0x11f:
	case 0x130:
	case 0x131:
	case 0x15e:
	case 0x15f:
		return c;
	}
	return Collator::RemoveAccent(c);
}

unsigned int CollatorTurkish::GetLatinWeight(unsigned int c)
{
	if (!m_attribute[COLLATOR_ATTR_CASE_SENSITIVE])
		return s_weight[c];
	if ((c >= 'A' && c <= 'Z') || c == 0xc7 || c == 0x11e || c == 0x130 || c == 0xd6 ||
	    c == 0x15e || c == 0xdc)
		return s_weight[c] * 2 + !m_attribute[COLLATOR_ATTR_UPPER_FIRST];
	if ((c >= 'a' && c <= 'z') || c == 0xe7 || c == 0x11f || c == 0x131 || c == 0xf6 ||
	    c == 0x15f || c == 0xfc)
		return s_weight[c] * 2 + !m_attribute[COLLATOR_ATTR_LOWER_FIRST];
	return c;
}

/* Ç, Ğ, İ, Ö, Ş and Ü after the letters they follow (GetIndexOrder) */
int CollatorTurkish::GetIndexOfLetter(unsigned int c)
{
	switch (c) {
	case 'I':
		return 8;
	case 'i':
		return 42;
	case 0xc7:
	case 0xe7:
		return 41;
	case 0xd6:
	case 0xf6:
		return 14;
	case 0xdc:
	case 0xfc:
		return 20;
	case 0x11e:
	case 0x11f:
		return 6;
	case 0x130:
		return 42;
	case 0x131:
		return 8;
	case 0x15e:
	case 0x15f:
		return 43;
	}
	return Collator::GetIndexOfLetter(c);
}

int CollatorTurkish::GetIndexOrder(int index)
{
	int order[26] = {
		0, 1, 2, 41, 3, 4, 5, 6, 7, 8, 42, 9, 10, 11, 12, 13, 14, 15, 17, 18, 43, 19,
		20, 21, 24, 25,
	};
	int i;

	for (i = 0; i < 26; i++) {
		if (order[i] == index)
			return i;
	}
	return 26;
}
