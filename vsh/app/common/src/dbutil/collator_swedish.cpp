/*
 * The Swedish collator (collator.h). The names are ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include "collator.h"

/* the order of the character groups in a sort key */
const SceUInt8 CollatorSwedish::s_groupOrder[COLLATOR_CHAR_TYPE_COUNT] =
	{ 2, 3, 1, 4, 4, 4, 4, 4, 4 };

unsigned int CollatorSwedish::RemoveAccent(unsigned int c)
{
	if (c == 0xf6 || c == 0xe5 || c == 0xe4 || c == 0xd6 || c == 0xc5 || c == 0xc4)
		return c;
	return Collator::RemoveAccent(c);
}

/* Å, Ä and Ö after z */
unsigned int CollatorSwedish::GetLatinWeight(unsigned int c)
{
	if (m_attribute[COLLATOR_ATTR_CASE_SENSITIVE]) {
		switch (c) {
		case 0xc5:
			return 52 + !m_attribute[COLLATOR_ATTR_UPPER_FIRST];
		case 0xe5:
			return 52 + !m_attribute[COLLATOR_ATTR_LOWER_FIRST];
		case 0xc4:
			return 54 + !m_attribute[COLLATOR_ATTR_UPPER_FIRST];
		case 0xe4:
			return 54 + !m_attribute[COLLATOR_ATTR_LOWER_FIRST];
		case 0xd6:
			return 56 + !m_attribute[COLLATOR_ATTR_UPPER_FIRST];
		case 0xf6:
			return 56 + !m_attribute[COLLATOR_ATTR_LOWER_FIRST];
		}
	} else {
		switch (c) {
		case 0xc5:
		case 0xe5:
			return 26;
		case 0xc4:
		case 0xe4:
			return 27;
		case 0xd6:
		case 0xf6:
			return 28;
		}
	}
	return Collator::GetLatinWeight(c);
}

/* indexed with z */
int CollatorSwedish::GetIndexOfLetter(unsigned int c)
{
	if (c == 0xf6 || c == 0xe5 || c == 0xe4 || c == 0xd6 || c == 0xc5 || c == 0xc4)
		return 25;
	return Collator::GetIndexOfLetter(c);
}
