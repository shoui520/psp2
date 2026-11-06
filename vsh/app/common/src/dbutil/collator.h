/*
 * dbutil's collators: the sort keys behind the database's dbutil_CollationEN/JP collations and
 * SortKeyEN/JP functions, one class per system language (CreateCollator).
 *
 * A sort key is five bytes per character: the character's group (digits, symbols, Latin,
 * Cyrillic, kana, Hangul, Han, ...), in the language's order of groups, then its weight inside
 * the group, big-endian. An empty string's key is the single byte 0xFF.
 *
 * Sony's names for these classes are not known; the names are ours.
 */
#ifndef DBUTIL_COLLATOR_H
#define DBUTIL_COLLATOR_H

#include <scetypes.h>

/* the character types GetCharType returns, the index of a key's group */
#define COLLATOR_CHAR_DIGIT		0
#define COLLATOR_CHAR_SYMBOL		1
#define COLLATOR_CHAR_LATIN		2
#define COLLATOR_CHAR_CYRILLIC		3
#define COLLATOR_CHAR_KANA		4
#define COLLATOR_CHAR_HANGUL		5
#define COLLATOR_CHAR_HAN		6
#define COLLATOR_CHAR_HAN_READING	7	/* a Han character the reading dictionary knows */
#define COLLATOR_CHAR_OTHER		8
#define COLLATOR_CHAR_TYPE_COUNT	9

/* GetIndex's result for a string that starts with no letter */
#define COLLATOR_INDEX_NONE		44

/* SetAttribute's attributes */
#define COLLATOR_ATTR_IGNORE_ARTICLES	0	/* skip a leading "the", "a" or "an" */
#define COLLATOR_ATTR_CASE_SENSITIVE	1
#define COLLATOR_ATTR_UPPER_FIRST	2	/* with case: upper case sorts first */
#define COLLATOR_ATTR_LOWER_FIRST	3	/* with case: lower case sorts first */
#define COLLATOR_ATTR_NUMERIC		4	/* a run of digits sorts by its value */
#define COLLATOR_ATTR_COUNT		5

class Collator {
public:
	Collator();
	virtual ~Collator()
	{
	}

	virtual int Compare(const char *left, int leftLength, const char *right, int rightLength);
	virtual int GetSortKey(const char *str, int length, SceUInt8 *key, int keySize,
			       int *keyLength);
	virtual int GetIndex(const char *str, int length);
	/* where a letter index (GetIndex) sorts among the language's letters */
	virtual int GetIndexOrder(int index)
	{
		return index;
	}
	virtual int GetCharType(unsigned int c);
	virtual int GetGroup(int type) = 0;
	virtual unsigned int ToLower(unsigned int c);
	virtual unsigned int RemoveAccent(unsigned int c);
	virtual unsigned int ExpandLigature(unsigned int *c, unsigned int *next);
	virtual int GetIndexOfLetter(unsigned int c);
	virtual unsigned int Normalize(unsigned int c);
	virtual unsigned int FoldCase(unsigned int c);
	virtual unsigned int GetDigitWeight(unsigned int c)
	{
		return c;
	}
	virtual unsigned int GetSymbolWeight(unsigned int c)
	{
		return c;
	}
	virtual unsigned int GetLatinWeight(unsigned int c);
	virtual unsigned int GetCyrillicWeight(unsigned int c)
	{
		return c;
	}
	virtual unsigned int GetKanaWeight(unsigned int c)
	{
		return c;
	}
	virtual unsigned int GetHangulWeight(unsigned int c)
	{
		return c;
	}
	virtual unsigned int GetHanWeight(unsigned int c)
	{
		return c;
	}
	virtual unsigned int GetHanReadingWeight(unsigned int c)
	{
		return c;
	}

	void SetAttribute(int attribute, bool value);
	unsigned int GetWeight(int type, unsigned int c);
	const SceUInt8 *ParseNumber(const SceUInt8 *str, int length, unsigned int *value);
	const SceUInt8 *SkipArticle(const SceUInt8 *str, int length);
	bool IsSymbol(unsigned int c);
	bool IsDigit(unsigned int c);
	bool IsLatin(unsigned int c);
	bool IsCyrillic(unsigned int c);
	bool IsKana(unsigned int c);
	bool IsHan(unsigned int c);
	bool IsHangul(unsigned int c);

protected:
	bool m_attribute[COLLATOR_ATTR_COUNT];
};

class CollatorEnglish : public Collator {
public:
	virtual int GetGroup(int type)
	{
		return s_groupOrder[type];
	}

private:
	static const SceUInt8 s_groupOrder[COLLATOR_CHAR_TYPE_COUNT];
};

class CollatorFrench : public Collator {
public:
	virtual int GetGroup(int type)
	{
		return s_groupOrder[type];
	}

private:
	static const SceUInt8 s_groupOrder[COLLATOR_CHAR_TYPE_COUNT];
};

class CollatorGerman : public Collator {
public:
	virtual int GetGroup(int type)
	{
		return s_groupOrder[type];
	}

private:
	static const SceUInt8 s_groupOrder[COLLATOR_CHAR_TYPE_COUNT];
};

class CollatorItalian : public Collator {
public:
	virtual int GetGroup(int type)
	{
		return s_groupOrder[type];
	}

private:
	static const SceUInt8 s_groupOrder[COLLATOR_CHAR_TYPE_COUNT];
};

class CollatorDutch : public Collator {
public:
	virtual int GetGroup(int type)
	{
		return s_groupOrder[type];
	}

private:
	static const SceUInt8 s_groupOrder[COLLATOR_CHAR_TYPE_COUNT];
};

class CollatorPortuguese : public Collator {
public:
	virtual int GetGroup(int type)
	{
		return s_groupOrder[type];
	}

private:
	static const SceUInt8 s_groupOrder[COLLATOR_CHAR_TYPE_COUNT];
};

class CollatorDanish : public Collator {
public:
	virtual int GetGroup(int type)
	{
		return s_groupOrder[type];
	}
	virtual unsigned int RemoveAccent(unsigned int c);
	virtual unsigned int ExpandLigature(unsigned int *c, unsigned int *next);
	virtual int GetIndexOfLetter(unsigned int c);
	virtual unsigned int GetLatinWeight(unsigned int c);

private:
	static const SceUInt8 s_groupOrder[COLLATOR_CHAR_TYPE_COUNT];
};

class CollatorFinnish : public Collator {
public:
	virtual int GetGroup(int type)
	{
		return s_groupOrder[type];
	}
	virtual unsigned int RemoveAccent(unsigned int c);
	virtual int GetIndexOfLetter(unsigned int c);
	virtual unsigned int GetLatinWeight(unsigned int c);

private:
	static const SceUInt8 s_groupOrder[COLLATOR_CHAR_TYPE_COUNT];
};

class CollatorJapanese : public Collator {
public:
	CollatorJapanese();
	virtual ~CollatorJapanese();

	virtual int GetIndexOrder(int index);
	virtual int GetCharType(unsigned int c);
	virtual int GetGroup(int type)
	{
		return s_groupOrder[type];
	}
	virtual int GetIndexOfLetter(unsigned int c);
	virtual unsigned int GetHanReadingWeight(unsigned int c);

private:
	static const SceUInt8 s_groupOrder[COLLATOR_CHAR_TYPE_COUNT];
};

class CollatorKorean : public Collator {
public:
	virtual int GetCharType(unsigned int c);
	virtual int GetGroup(int type)
	{
		return s_groupOrder[type];
	}

private:
	static const SceUInt8 s_groupOrder[COLLATOR_CHAR_TYPE_COUNT];
};

class CollatorNorwegian : public Collator {
public:
	virtual int GetGroup(int type)
	{
		return s_groupOrder[type];
	}
	virtual unsigned int RemoveAccent(unsigned int c);
	virtual unsigned int ExpandLigature(unsigned int *c, unsigned int *next);
	virtual int GetIndexOfLetter(unsigned int c);
	virtual unsigned int GetLatinWeight(unsigned int c);

private:
	static const SceUInt8 s_groupOrder[COLLATOR_CHAR_TYPE_COUNT];
};

class CollatorPolish : public Collator {
public:
	virtual int GetGroup(int type)
	{
		return s_groupOrder[type];
	}
	virtual unsigned int RemoveAccent(unsigned int c);
	virtual int GetIndexOfLetter(unsigned int c);
	virtual unsigned int GetLatinWeight(unsigned int c);

private:
	static const SceUInt8 s_groupOrder[COLLATOR_CHAR_TYPE_COUNT];
};

class CollatorRussian : public Collator {
public:
	virtual int GetCharType(unsigned int c);
	virtual int GetGroup(int type)
	{
		return s_groupOrder[type];
	}
	virtual unsigned int GetCyrillicWeight(unsigned int c);

private:
	static const SceUInt8 s_groupOrder[COLLATOR_CHAR_TYPE_COUNT];
};

class CollatorSimplifiedChinese : public Collator {
public:
	virtual int GetCharType(unsigned int c);
	virtual int GetGroup(int type)
	{
		return s_groupOrder[type];
	}

private:
	static const SceUInt8 s_groupOrder[COLLATOR_CHAR_TYPE_COUNT];
};

class CollatorSpanish : public Collator {
public:
	virtual int GetGroup(int type)
	{
		return s_groupOrder[type];
	}
	virtual unsigned int RemoveAccent(unsigned int c);
	virtual int GetIndexOfLetter(unsigned int c);
	virtual unsigned int GetLatinWeight(unsigned int c);

private:
	static const SceUInt8 s_groupOrder[COLLATOR_CHAR_TYPE_COUNT];
};

class CollatorSwedish : public Collator {
public:
	virtual int GetGroup(int type)
	{
		return s_groupOrder[type];
	}
	virtual unsigned int RemoveAccent(unsigned int c);
	virtual int GetIndexOfLetter(unsigned int c);
	virtual unsigned int GetLatinWeight(unsigned int c);

private:
	static const SceUInt8 s_groupOrder[COLLATOR_CHAR_TYPE_COUNT];
};

class CollatorTraditionalChinese : public Collator {
public:
	virtual int GetCharType(unsigned int c);
	virtual int GetGroup(int type)
	{
		return s_groupOrder[type];
	}

private:
	static const SceUInt8 s_groupOrder[COLLATOR_CHAR_TYPE_COUNT];
};

class CollatorTurkish : public Collator {
public:
	virtual int GetIndexOrder(int index);
	virtual int GetGroup(int type)
	{
		return s_groupOrder[type];
	}
	virtual unsigned int ToLower(unsigned int c);
	virtual unsigned int RemoveAccent(unsigned int c);
	virtual int GetIndexOfLetter(unsigned int c);
	virtual unsigned int GetLatinWeight(unsigned int c);

private:
	static const SceUInt8 s_groupOrder[COLLATOR_CHAR_TYPE_COUNT];
};

/* shared by the collators' files */
extern const SceUInt8 g_latin1Base[0x40];
extern const SceUInt8 g_latinExtendedABase[0x80];
unsigned int ToHalfwidth(unsigned int c);
unsigned int ToKatakana(unsigned int c);

Collator *CreateCollator(int language);
void DestroyCollator(Collator *collator);

#endif
