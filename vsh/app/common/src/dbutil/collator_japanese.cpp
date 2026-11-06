/*
 * The Japanese collator (collator.h). The names are ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include "collator.h"
#include <ces/libces_jis.h>

/* the letter index of hiragana and katakana, U+3040-U+30FF */
static const unsigned int s_kanaIndex[0xc0] = {
	44, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 32, 32, 32, 32, 32,
	32, 32, 32, 32, 32, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 34,
	34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 35, 35, 35, 35, 35, 36,
	36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 37, 37,
	37, 37, 37, 38, 38, 38, 38, 38, 38, 39, 39, 39, 39, 39, 40, 40,
	40, 40, 40, 40, 31, 32, 32, 44, 44, 44, 44, 44, 44, 44, 44, 44,
	44, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 32, 32, 32, 32, 32,
	32, 32, 32, 32, 32, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 34,
	34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 35, 35, 35, 35, 35, 36,
	36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 37, 37,
	37, 37, 37, 38, 38, 38, 38, 38, 38, 39, 39, 39, 39, 39, 40, 40,
	40, 40, 40, 40, 31, 32, 32, 40, 40, 40, 40, 44, 44, 44, 44, 44,
};

/* the letter index of a to z */
static const unsigned int s_latinIndex[26] = {
	0, 0, 0, 3, 3, 3, 6, 6, 6, 9, 9, 9, 12,
	12, 12, 15, 15, 15, 18, 18, 18, 21, 21, 21, 21, 25,
};

/* the order of the character groups in a sort key */
const SceUInt8 CollatorJapanese::s_groupOrder[COLLATOR_CHAR_TYPE_COUNT] =
	{ 3, 4, 2, 7, 1, 7, 6, 5, 7 };

/* code page 932, set up on the first Han character: Han characters sort by their Shift_JIS code */
static SceCesSJisUcsProfile *s_sjisProfile;
static SceCesUcsProfileSheet s_sjisProfileSheet;

int CollatorJapanese::GetIndexOfLetter(unsigned int c)
{
	int index = COLLATOR_INDEX_NONE;

	switch (GetCharType(c)) {
	case COLLATOR_CHAR_LATIN:
		return s_latinIndex[ToLower(c) - 'a'];
	case COLLATOR_CHAR_KANA:
		return s_kanaIndex[c - 0x3040];
	}
	return index;
}

int CollatorJapanese::GetCharType(unsigned int c)
{
	if (IsLatin(c))
		return COLLATOR_CHAR_LATIN;
	if (IsDigit(c))
		return COLLATOR_CHAR_DIGIT;
	if (IsSymbol(c))
		return COLLATOR_CHAR_SYMBOL;
	if (IsCyrillic(c))
		return COLLATOR_CHAR_CYRILLIC;
	if (IsKana(c))
		return COLLATOR_CHAR_KANA;
	if (IsHan(c)) {
		uint8_t sjis[16];
		uint32_t length;
		int type = COLLATOR_CHAR_HAN;

		if (s_sjisProfile == NULL)
			s_sjisProfile = sceCesUcsProfileInitSJis1997Cp932(&s_sjisProfileSheet);
		if (sceCesUtf32ToMbc(c, &s_sjisProfile->cesMbcsUcsPrf, sjis, sizeof(sjis),
				     &length) == 0 && length != 0)
			type = COLLATOR_CHAR_HAN_READING;
		return type;
	}
	return COLLATOR_CHAR_OTHER;
}

/* a Han character by its Shift_JIS code, which orders JIS X 0208's kanji by reading */
unsigned int CollatorJapanese::GetHanReadingWeight(unsigned int c)
{
	uint8_t sjis[16];
	uint32_t length;

	if (c >= 0xffff)
		return 0;
	if (s_sjisProfile == NULL)
		s_sjisProfile = sceCesUcsProfileInitSJis1997Cp932(&s_sjisProfileSheet);
	if (sceCesUtf32ToMbc(c, &s_sjisProfile->cesMbcsUcsPrf, sjis, sizeof(sjis), &length) != 0 ||
	    length == 0)
		return 0xffffffff;
	return sjis[0] << 8 | sjis[1];
}

CollatorJapanese::CollatorJapanese()
{
}

CollatorJapanese::~CollatorJapanese()
{
}

/* the letter indexes in order: a to z, then the kana rows */
int CollatorJapanese::GetIndexOrder(int index)
{
	int order[19] = {
		31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 0, 3, 6, 9, 12, 15, 18, 21, 25,
	};
	int i;

	for (i = 0; i < 19; i++) {
		if (order[i] == index)
			return i;
	}
	return 19;
}
