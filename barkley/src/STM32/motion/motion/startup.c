/*
 * Start-up: the vector table at 0x08003000 (initial sp, reset, 57 handlers in image 00
 * and 01) and __low_level_init, which IAR's __cmain calls before the data is
 * initialised (it returns 1: initialise).
 *
 * Skeleton. The handlers and the table's contents are not written yet.
 */

#include "motion.h"

int __low_level_init(void)
{
	return 1;
}
