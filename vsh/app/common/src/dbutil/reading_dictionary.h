/*
 * The reading dictionary the Japanese collator sorts Han characters by (collator_japanese.cpp):
 * a library dbutil links, written with its own code. The names are ours.
 */
#ifndef DBUTIL_READING_DICTIONARY_H
#define DBUTIL_READING_DICTIONARY_H

#include <scetypes.h>

/* the dictionary's state, set up by HanReadingDictionaryOpen; its layout is the library's */
typedef struct HanReadingDictionary {
	SceUInt32 state[0xec / 4];
} HanReadingDictionary;

HanReadingDictionary *HanReadingDictionaryOpen(HanReadingDictionary *dictionary);
/* the reading of c in reading, its length in *length; 0 on success */
int HanReadingDictionaryLookup(unsigned int c, HanReadingDictionary *dictionary,
			       SceUInt8 *reading, int size, int *length);

#endif
