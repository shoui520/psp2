/*
 * libSceCes's UCS.c (Sony's internal/usermode/src/ces/libces/src/UCS/UCS.c, the SDK's static
 * library, built with SNC): UCS-2 and UCS-4 code checks and conversions. SNC's linker keeps only
 * the routines a module calls; the tree has the one SceClipboard calls.
 */
#include <stdint.h>
#include <ces/libces_ucs.h>

/* a UCS-2 code is any UTF-16 code unit but a surrogate */
int sceCesUcs2CheckCode(uint16_t ucs2)
{
	if (ucs2 >= 0xd800 && ucs2 <= 0xdfff)
		return SCE_CES_ERROR_ILLEGAL_CODE;
	return SCE_CES_OK;
}
