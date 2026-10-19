/* PC-2 selections are one-based because the original consumes shift carry. */
#include "des_internal.h"

const uint16_t desKeySelection[24] = {
	KEY_SELECTION(2, 4), KEY_SELECTION(13, 1),
	KEY_SELECTION(20, 8), KEY_SELECTION(27, 22),
	KEY_SELECTION(7, 14), KEY_SELECTION(16, 18),
	KEY_SELECTION(8, 25), KEY_SELECTION(26, 6),
	KEY_SELECTION(4, 28), KEY_SELECTION(12, 11),
	KEY_SELECTION(19, 21), KEY_SELECTION(23, 16),
	KEY_SELECTION(10, 20), KEY_SELECTION(21, 5),
	KEY_SELECTION(6, 17), KEY_SELECTION(15, 23),
	KEY_SELECTION(28, 12), KEY_SELECTION(3, 2),
	KEY_SELECTION(5, 27), KEY_SELECTION(1, 19),
	KEY_SELECTION(24, 9), KEY_SELECTION(11, 3),
	KEY_SELECTION(17, 24), KEY_SELECTION(14, 13),
};

/* Two bits per round, least significant round first: 1,1,2,...,2,1. */
const uint32_t desKeyRotationSequence = 0x6aa9aaa5;
