/*
 * The default handler of the ROM-patch override slots (0x000282a0; see
 * robin/patch.h): "not handled", so the firmware runs its own code.
 */
#include "robin/patch.h"

int robinNoOverride(void)
{
	return 0;
}
