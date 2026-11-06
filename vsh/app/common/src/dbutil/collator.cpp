/*
 * dbutil's collators (collator.h): the base class's sort keys, character classes and folding,
 * and the factory. The names are ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include <system_param.h>
#include <std/new>
#include <std/string.h>
#include "collator.h"

/* the length of a UTF-8 sequence by its first byte */
static const SceUInt8 s_utf8Length[256] = {
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 1, 1,
};

/* a byte order mark, skipped at the start of a string */
static const char s_utf8Bom[] = "\xef\xbb\xbf";

static inline unsigned int DecodeUtf8(const SceUInt8 *str, int length)
{
	const SceUInt8 mask[6] = { 0x7f, 0x3f, 0x1f, 0x0f, 0x07, 0x03 };
	unsigned int c;
	int i;

	c = (SceUInt8)(str[0] & mask[length - 1]) << (6 * (length - 1));
	for (i = 1; i < length; i++)
		c |= (str[i] & 0x3f) << (6 * (length - 1 - i));
	return c;
}

static inline SceUInt32 ToBigEndian(SceUInt32 value)
{
	return (value << 24) | ((value << 8) & 0xff0000) | ((value >> 8) & 0xff00) | (value >> 24);
}

Collator::Collator()
{
	m_attribute[COLLATOR_ATTR_IGNORE_ARTICLES] = false;
	m_attribute[COLLATOR_ATTR_CASE_SENSITIVE] = false;
	m_attribute[COLLATOR_ATTR_UPPER_FIRST] = true;
	m_attribute[COLLATOR_ATTR_LOWER_FIRST] = false;
	m_attribute[COLLATOR_ATTR_NUMERIC] = false;
}

Collator *CreateCollator(int language)
{
	switch (language) {
	case SCE_SYSTEM_PARAM_LANG_JAPANESE:
		return new CollatorJapanese;
	case SCE_SYSTEM_PARAM_LANG_ENGLISH_US:
	case SCE_SYSTEM_PARAM_LANG_ENGLISH_GB:
		return new CollatorEnglish;
	case SCE_SYSTEM_PARAM_LANG_FRENCH:
		return new CollatorFrench;
	case SCE_SYSTEM_PARAM_LANG_SPANISH:
		return new CollatorSpanish;
	case SCE_SYSTEM_PARAM_LANG_GERMAN:
		return new CollatorGerman;
	case SCE_SYSTEM_PARAM_LANG_ITALIAN:
		return new CollatorItalian;
	case SCE_SYSTEM_PARAM_LANG_DUTCH:
		return new CollatorDutch;
	case SCE_SYSTEM_PARAM_LANG_PORTUGUESE_PT:
		return new CollatorPortuguese;
	case SCE_SYSTEM_PARAM_LANG_RUSSIAN:
		return new CollatorRussian;
	case SCE_SYSTEM_PARAM_LANG_KOREAN:
		return new CollatorKorean;
	case SCE_SYSTEM_PARAM_LANG_CHINESE_T:
		return new CollatorTraditionalChinese;
	case SCE_SYSTEM_PARAM_LANG_CHINESE_S:
		return new CollatorSimplifiedChinese;
	case SCE_SYSTEM_PARAM_LANG_FINNISH:
		return new CollatorFinnish;
	case SCE_SYSTEM_PARAM_LANG_SWEDISH:
		return new CollatorSwedish;
	case SCE_SYSTEM_PARAM_LANG_DANISH:
		return new CollatorDanish;
	case SCE_SYSTEM_PARAM_LANG_NORWEGIAN:
		return new CollatorNorwegian;
	case SCE_SYSTEM_PARAM_LANG_POLISH:
		return new CollatorPolish;
	case SCE_SYSTEM_PARAM_LANG_PORTUGUESE_BR:
		return new CollatorPortuguese;
	case SCE_SYSTEM_PARAM_LANG_TURKISH:
		return new CollatorTurkish;
	default:
		return new CollatorEnglish;
	}
}

void DestroyCollator(Collator *collator)
{
	delete collator;
}

void Collator::SetAttribute(int attribute, bool value)
{
	switch (attribute) {
	case COLLATOR_ATTR_IGNORE_ARTICLES:
	case COLLATOR_ATTR_CASE_SENSITIVE:
	case COLLATOR_ATTR_NUMERIC:
		m_attribute[attribute] = value;
		break;
	case COLLATOR_ATTR_UPPER_FIRST:
		m_attribute[COLLATOR_ATTR_UPPER_FIRST] = value;
		m_attribute[COLLATOR_ATTR_LOWER_FIRST] = !value;
		break;
	case COLLATOR_ATTR_LOWER_FIRST:
		m_attribute[COLLATOR_ATTR_LOWER_FIRST] = value;
		m_attribute[COLLATOR_ATTR_UPPER_FIRST] = !value;
		break;
	}
}

int Collator::Compare(const char *left, int leftLength, const char *right, int rightLength)
{
	int leftKeyLength;
	int rightKeyLength;
	SceUInt8 *leftKey;
	SceUInt8 *rightKey;
	int result;

	GetSortKey(left, leftLength, NULL, 0, &leftKeyLength);
	GetSortKey(right, rightLength, NULL, 0, &rightKeyLength);
	leftKey = new SceUInt8[leftKeyLength];
	rightKey = new SceUInt8[rightKeyLength];
	GetSortKey(left, leftLength, leftKey, leftKeyLength, NULL);
	GetSortKey(right, rightLength, rightKey, rightKeyLength, NULL);
	result = sce_paf_memcmp(leftKey, rightKey,
				leftKeyLength < rightKeyLength ? leftKeyLength : rightKeyLength);
	if (result == 0)
		result = leftKeyLength - rightKeyLength;
	delete[] leftKey;
	delete[] rightKey;
	return result;
}

unsigned int Collator::GetWeight(int type, unsigned int c)
{
	switch (type) {
	case COLLATOR_CHAR_DIGIT:
		return GetDigitWeight(c);
	case COLLATOR_CHAR_SYMBOL:
		return GetSymbolWeight(c);
	case COLLATOR_CHAR_LATIN:
		return GetLatinWeight(c);
	case COLLATOR_CHAR_CYRILLIC:
		return GetCyrillicWeight(c);
	case COLLATOR_CHAR_KANA:
		return GetKanaWeight(c);
	case COLLATOR_CHAR_HANGUL:
		return GetHangulWeight(c);
	case COLLATOR_CHAR_HAN:
		return GetHanWeight(c);
	case COLLATOR_CHAR_HAN_READING:
		return GetHanReadingWeight(c);
	default:
		return c;
	}
}

/* the value of the run of digits at str, saturated; returns where the run ends */
const SceUInt8 *Collator::ParseNumber(const SceUInt8 *str, int length, unsigned int *value)
{
	const SceUInt8 *p = str;
	unsigned int number = 0;
	int i;

	for (i = 0; i < length; i++) {
		unsigned int charLength = s_utf8Length[*p];
		unsigned int c = Normalize(DecodeUtf8(p, charLength));
		unsigned int digit;
		unsigned int next;

		if (GetCharType(c) != COLLATOR_CHAR_DIGIT)
			break;
		digit = c - '0';
		next = 0xffffffff;
		if (number <= (0xffffffff - digit) / 10)
			next = digit + number * 10;
		p += charLength;
		number = next;
	}
	*value = number;
	return p;
}

/* str past a leading article and the space after it */
const SceUInt8 *Collator::SkipArticle(const SceUInt8 *str, int length)
{
	const char *articles[3];
	unsigned int word[8];
	unsigned int article[8];
	const SceUInt8 *p = str;
	const SceUInt8 *next;
	int consumed = 0;
	unsigned int count = 0;
	unsigned int i;

	articles[0] = "the";
	articles[1] = "a";
	articles[2] = "an";
	for (;;) {
		unsigned int charLength = s_utf8Length[*p];
		unsigned int c = DecodeUtf8(p, charLength);

		next = p + charLength;
		word[count] = FoldCase(c);
		if (word[count] == ' ')
			break;
		p = next;
		consumed += charLength;
		if (consumed >= length)
			return str;
		if (++count >= 8)
			return str;
	}
	if (count == 0)
		return str;
	for (i = 0; i < 3; i++) {
		const char *a = articles[i];
		int articleLength = sce_paf_strlen(a);
		int j;

		if (articleLength != (int)count)
			continue;
		for (j = 0; j < articleLength; j++, a++)
			article[j] = FoldCase(DecodeUtf8((const SceUInt8 *)a,
							 s_utf8Length[*(const SceUInt8 *)a]));
		if (sce_paf_memcmp(word, article, count * sizeof(word[0])) == 0)
			return next;
	}
	return str;
}

int Collator::GetSortKey(const char *str, int length, SceUInt8 *key, int keySize, int *keyLength)
{
	const SceUInt8 *start;
	const SceUInt8 *p;
	unsigned int c;
	unsigned int next = 0;
	unsigned int charLength = 0;
	int position;
	int size = 0;

	if (str != NULL) {
		start = (const SceUInt8 *)str;
		p = start;
		if (length >= 3 && sce_paf_memcmp(str, s_utf8Bom, 3) == 0)
			p = start + 3;
	} else {
		start = NULL;
		p = NULL;
	}
	if (start == NULL || length == 0) {
		if (key != NULL && keySize > 0)
			*key = 0xff;
		size = 1;
	} else {
		position = p - start;
		if (m_attribute[COLLATOR_ATTR_IGNORE_ARTICLES]) {
			p = SkipArticle(p, length - position);
			position = p - start;
		}
		while (position < length || next != 0) {
			int type;

			if (next != 0) {
				c = next;
				next = 0;
			} else {
				charLength = s_utf8Length[*p];
				c = DecodeUtf8(p, charLength);
				p += charLength;
			}
			c = ExpandLigature(&c, &next);
			c = Normalize(c);
			type = GetCharType(c);
			if (type == COLLATOR_CHAR_DIGIT && m_attribute[COLLATOR_ATTR_NUMERIC])
				p = ParseNumber(p - charLength, length, &c);
			position = p - (const SceUInt8 *)str;
			if (key != NULL && keySize > 0) {
				key[0] = GetGroup(type);
				*(SceUInt32 *)(key + 1) = ToBigEndian(GetWeight(type, c));
				key += 5;
			}
			size += 5;
		}
		if (size == 0) {
			if (key != NULL && keySize > 0)
				*key = 0xff;
			size = 1;
		}
	}
	if (keyLength != NULL)
		*keyLength = size;
	return 0;
}

int Collator::GetIndex(const char *str, int length)
{
	const SceUInt8 *p;
	int index = COLLATOR_INDEX_NONE;

	if (str != NULL) {
		p = (const SceUInt8 *)str;
		if (length >= 3 && sce_paf_memcmp(str, s_utf8Bom, 3) == 0)
			p += 3;
	} else {
		p = NULL;
	}
	if (m_attribute[COLLATOR_ATTR_IGNORE_ARTICLES])
		p = SkipArticle(p, length - (p - (const SceUInt8 *)str));
	if (length - (p - (const SceUInt8 *)str) > 0) {
		unsigned int c = DecodeUtf8(p, s_utf8Length[*p]);

		ExpandLigature(&c, NULL);
		c = Normalize(c);
		index = GetIndexOfLetter(c);
	}
	return index;
}

bool Collator::IsSymbol(unsigned int c)
{
	if ((c >= 0x21 && c <= 0x2f) || (c >= 0x3a && c <= 0x40) || (c >= 0x5b && c <= 0x60) ||
	    (c >= 0x70 && c <= 0x7e) || c == 0xd7 || c == 0xf7 || (c >= 0x2018 && c <= 0x201f) ||
	    (c >= 0x2039 && c <= 0x203a) || c == 0x2116 || (c >= 0x20a0 && c <= 0x20b1) ||
	    c == 0x3001 || c == 0x3002 || c == 0x300c || c == 0x300d || c == 0x30fb ||
	    c == 0x30fc || (c >= 0xff01 && c <= 0xff0f) || (c >= 0xff1a && c <= 0xff20) ||
	    (c >= 0xff3b && c <= 0xff40) || (c >= 0xffe0 && c <= 0xffe6) ||
	    (c >= 0xffe8 && c <= 0xffec))
		return true;
	return false;
}

bool Collator::IsDigit(unsigned int c)
{
	if ((c >= '0' && c <= '9') || (c >= 0xff10 && c <= 0xff19))
		return true;
	return false;
}

bool Collator::IsLatin(unsigned int c)
{
	if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
	    (c >= 0xc0 && c <= 0xff && c != 0xd7 && c != 0xf7) || (c >= 0x100 && c <= 0x17f))
		return true;
	return false;
}

int Collator::GetCharType(unsigned int c)
{
	if (IsLatin(c))
		return COLLATOR_CHAR_LATIN;
	if (IsDigit(c))
		return COLLATOR_CHAR_DIGIT;
	if (IsSymbol(c))
		return COLLATOR_CHAR_SYMBOL;
	return COLLATOR_CHAR_OTHER;
}

unsigned int Collator::ToLower(unsigned int c)
{
	if (c >= 'A' && c <= 'Z')
		return c + ('a' - 'A');
	return c;
}

unsigned int Collator::RemoveAccent(unsigned int c)
{
	unsigned int base = c;

	if (c >= 0x80 && c <= 0xff)
		base = g_latin1Base[c - 0xc0];
	else if (c >= 0x100 && c <= 0x17f)
		base = g_latinExtendedABase[c - 0x100];
	if (base != 0)
		return base;
	return c;
}

/* a ligature as its first letter, its second in *next */
unsigned int Collator::ExpandLigature(unsigned int *c, unsigned int *next)
{
	switch (*c) {
	case 0xc6:
		*c = 'A';
		if (next != NULL)
			*next = 'E';
		break;
	case 0xdf:
		*c = 's';
		if (next != NULL)
			*next = 's';
		break;
	case 0xe6:
		*c = 'a';
		if (next != NULL)
			*next = 'e';
		break;
	case 0x132:
		*c = 'I';
		if (next != NULL)
			*next = 'J';
		break;
	case 0x133:
		*c = 'i';
		if (next != NULL)
			*next = 'j';
		break;
	case 0x152:
		*c = 'O';
		if (next != NULL)
			*next = 'E';
		break;
	case 0x153:
		*c = 'o';
		if (next != NULL)
			*next = 'e';
		break;
	}
	return *c;
}

int Collator::GetIndexOfLetter(unsigned int c)
{
	int index = COLLATOR_INDEX_NONE;

	if (GetCharType(c) == COLLATOR_CHAR_LATIN) {
		switch (ToLower(c)) {
		case 'a': index = 0; break;
		case 'b': index = 1; break;
		case 'c': index = 2; break;
		case 'd': index = 3; break;
		case 'e': index = 4; break;
		case 'f': index = 5; break;
		case 'g': index = 6; break;
		case 'h': index = 7; break;
		case 'i': index = 8; break;
		case 'j': index = 9; break;
		case 'k': index = 10; break;
		case 'l': index = 11; break;
		case 'm': index = 12; break;
		case 'n': index = 13; break;
		case 'o': index = 14; break;
		case 'p': index = 15; break;
		case 'q': index = 16; break;
		case 'r': index = 17; break;
		case 's': index = 18; break;
		case 't': index = 19; break;
		case 'u': index = 20; break;
		case 'v': index = 21; break;
		case 'w': index = 22; break;
		case 'x': index = 23; break;
		case 'y': index = 24; break;
		case 'z': index = 25; break;
		}
	}
	return index;
}

unsigned int Collator::Normalize(unsigned int c)
{
	c = ToKatakana(RemoveAccent(ToHalfwidth(c)));
	if (!m_attribute[COLLATOR_ATTR_CASE_SENSITIVE]) {
		c = ToLower(c);
		/* Cyrillic capitals */
		if (c >= 0x410 && c <= 0x42f)
			c += 0x20;
	}
	return c;
}

unsigned int Collator::FoldCase(unsigned int c)
{
	return ToLower(ToHalfwidth(c));
}

bool Collator::IsCyrillic(unsigned int c)
{
	return c >= 0x410 && c <= 0x44f;
}

bool Collator::IsKana(unsigned int c)
{
	if ((c >= 0x3041 && c <= 0x3094) || (c >= 0x30a1 && c <= 0x30fa))
		return true;
	return false;
}

bool Collator::IsHan(unsigned int c)
{
	if ((c >= 0x4e00 && c <= 0x9fcb) || (c >= 0x3400 && c <= 0x4db5) ||
	    (c >= 0x20000 && c <= 0x2a6d6) || (c >= 0x2a700 && c <= 0x2b734) ||
	    (c >= 0xf900 && c <= 0xfaff) || (c >= 0x2f800 && c <= 0x2fa1f))
		return true;
	return false;
}

bool Collator::IsHangul(unsigned int c)
{
	if ((c >= 0x1100 && c <= 0x11ff) || (c >= 0x3130 && c <= 0x318f) ||
	    (c >= 0xa960 && c <= 0xa97f) || (c >= 0xac00 && c <= 0xd7af) ||
	    (c >= 0xd7b0 && c <= 0xd7ff))
		return true;
	return false;
}

/* letters by letter, the cases apart only when the collation is case sensitive */
unsigned int Collator::GetLatinWeight(unsigned int c)
{
	if (m_attribute[COLLATOR_ATTR_CASE_SENSITIVE]) {
		if (c >= 'A' && c <= 'Z')
			return (c - 'A') * 2 + !m_attribute[COLLATOR_ATTR_UPPER_FIRST];
		if (c >= 'a' && c <= 'z')
			return (c - 'a') * 2 + !m_attribute[COLLATOR_ATTR_LOWER_FIRST];
	} else if (c >= 'A' && c <= 'Z') {
		c -= 'A';
	} else if (c >= 'a' && c <= 'z') {
		c -= 'a';
	}
	return c;
}
