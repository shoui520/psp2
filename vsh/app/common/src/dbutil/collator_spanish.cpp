/*
 * The Spanish collator (collator.h). The names are ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include "collator.h"

/* the order of the character groups in a sort key */
const SceUInt8 CollatorSpanish::s_groupOrder[COLLATOR_CHAR_TYPE_COUNT] =
	{ 2, 3, 1, 4, 4, 4, 4, 4, 4 };

unsigned int CollatorSpanish::RemoveAccent(unsigned int c)
{
	switch (c) {
	case 0xd1:
	case 0xf1:
		return c;
	}
	return Collator::RemoveAccent(c);
}

/* Ñ after n */
unsigned int CollatorSpanish::GetLatinWeight(unsigned int c)
{
	if (m_attribute[COLLATOR_ATTR_CASE_SENSITIVE]) {
		switch (c) {
		case 0xd1:
			return 28 + !m_attribute[COLLATOR_ATTR_UPPER_FIRST];
		case 0xf1:
			return 28 + !m_attribute[COLLATOR_ATTR_LOWER_FIRST];
		}
		if ((c >= 'a' && c <= 'n') || (c > 'n' && c <= 'z'))
			return (c - 'a' + (c > 'n')) * 2 + !m_attribute[COLLATOR_ATTR_LOWER_FIRST];
		if ((c >= 'A' && c <= 'N') || (c > 'N' && c <= 'Z'))
			return (c - 'A' + (c > 'N')) * 2 + !m_attribute[COLLATOR_ATTR_UPPER_FIRST];
	} else {
		switch (c) {
		case 0xd1:
		case 0xf1:
			return 14;
		}
		if ((c >= 'a' && c <= 'n') || (c > 'n' && c <= 'z'))
			return c - 'a' + (c > 'n');
		if ((c >= 'A' && c <= 'N') || (c > 'N' && c <= 'Z'))
			return c - 'A' + (c > 'N');
	}
	return c;
}

/* indexed with n */
int CollatorSpanish::GetIndexOfLetter(unsigned int c)
{
	switch (c) {
	case 0xd1:
	case 0xf1:
		return 13;
	}
	return Collator::GetIndexOfLetter(c);
}
