/*
 * The Danish collator (collator.h). The names are ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include "collator.h"

/* the order of the character groups in a sort key */
const SceUInt8 CollatorDanish::s_groupOrder[COLLATOR_CHAR_TYPE_COUNT] =
	{ 2, 3, 1, 4, 4, 4, 4, 4, 4 };

unsigned int CollatorDanish::RemoveAccent(unsigned int c)
{
	if (c == 0xf8 || c == 0xe6 || c == 0xe5 || c == 0xd8 || c == 0xc6 || c == 0xc5)
		return c;
	return Collator::RemoveAccent(c);
}

/* Æ and æ are letters of their own, not ligatures */
unsigned int CollatorDanish::ExpandLigature(unsigned int *c, unsigned int *next)
{
	switch (*c) {
	case 0xc6:
	case 0xe6:
		return *c;
	}
	return Collator::ExpandLigature(c, next);
}

/* Æ, Ø and Å after z */
unsigned int CollatorDanish::GetLatinWeight(unsigned int c)
{
	if (m_attribute[COLLATOR_ATTR_CASE_SENSITIVE]) {
		switch (c) {
		case 0xc6:
			return 52 + !m_attribute[COLLATOR_ATTR_UPPER_FIRST];
		case 0xe6:
			return 52 + !m_attribute[COLLATOR_ATTR_LOWER_FIRST];
		case 0xd8:
			return 54 + !m_attribute[COLLATOR_ATTR_UPPER_FIRST];
		case 0xf8:
			return 54 + !m_attribute[COLLATOR_ATTR_LOWER_FIRST];
		case 0xc5:
			return 56 + !m_attribute[COLLATOR_ATTR_UPPER_FIRST];
		case 0xe5:
			return 56 + !m_attribute[COLLATOR_ATTR_LOWER_FIRST];
		}
	} else {
		switch (c) {
		case 0xc6:
		case 0xe6:
			return 26;
		case 0xd8:
		case 0xf8:
			return 27;
		case 0xc5:
		case 0xe5:
			return 28;
		}
	}
	return Collator::GetLatinWeight(c);
}

/* indexed with z */
int CollatorDanish::GetIndexOfLetter(unsigned int c)
{
	if (c == 0xf8 || c == 0xe6 || c == 0xe5 || c == 0xd8 || c == 0xc6 || c == 0xc5)
		return 25;
	return Collator::GetIndexOfLetter(c);
}
