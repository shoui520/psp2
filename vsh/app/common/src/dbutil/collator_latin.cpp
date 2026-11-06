/*
 * The Latin letters without their accents (Collator::RemoveAccent). The names are ours.
 */
#include <scetypes.h>
#include "collator.h"

/* Latin-1, U+00C0-U+00FF; 0 for none */
const SceUInt8 g_latin1Base[0x40] = {
	'A', 'A', 'A', 'A', 'A', 'A', 0, 'C', 'E', 'E', 'E', 'E', 'I', 'I', 'I', 'I',
	'D', 'N', 'O', 'O', 'O', 'O', 'O', 0, 'O', 'U', 'U', 'U', 'U', 'Y', 0, 0,
	'a', 'a', 'a', 'a', 'a', 'a', 0, 'c', 'e', 'e', 'e', 'e', 'i', 'i', 'i', 'i',
	'd', 'n', 'o', 'o', 'o', 'o', 'o', 0, 'o', 'u', 'u', 'u', 'u', 'y', 0, 'y',
};

/* Latin Extended-A, U+0100-U+017F; 0 for none */
const SceUInt8 g_latinExtendedABase[0x80] = {
	'A', 'a', 'A', 'a', 'A', 'a', 'C', 'c', 'C', 'c', 'C', 'c', 'C', 'c', 'D', 'd',
	'D', 'd', 'E', 'e', 'E', 'e', 'E', 'e', 'E', 'e', 'E', 'e', 'G', 'g', 'G', 'g',
	'G', 'g', 'G', 'g', 'H', 'h', 'H', 'h', 'I', 'i', 'I', 'i', 'I', 'i', 'I', 'i',
	'I', 'i', 0, 0, 'J', 'j', 'K', 'k', 'k', 'L', 'l', 'L', 'l', 'L', 'l', 'L',
	'l', 'L', 'l', 'N', 'n', 'N', 'n', 'N', 'n', 'n', 'N', 0, 'O', 'o', 'O', 'o',
	'O', 'o', 0, 0, 'R', 'r', 'R', 'r', 'R', 'r', 'S', 's', 'S', 's', 'S', 's',
	'S', 's', 'T', 't', 'T', 't', 'T', 't', 'U', 'u', 'U', 'u', 'U', 'u', 'U', 'u',
	'U', 'u', 'U', 'u', 'W', 'w', 'Y', 'y', 'Y', 'Z', 'z', 'Z', 'z', 'Z', 'z', 0,
};
