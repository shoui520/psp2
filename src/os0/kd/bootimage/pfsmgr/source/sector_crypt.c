/*
 * The sector ciphers of the crypt engine, each an encrypt/decrypt pair: AES
 * through the DMAC5 engine (ECB, CMAC) or ksceAesEncrypt2, with the XTS tweak
 * step in software. Names unknown; the modes in the names other than Xts are
 * provisional until the deferred routines are reversed.
 */

#include <psp2kern/kernel/sysclib.h>
#include "pfsmgr_internal.h"

/* PLACEHOLDER: cryptographic routine, deferred */
int pfsSectorCryptXtsEncrypt(void *dst, const void *src, const void *key, const void *tweakKey,
			     SceUInt32 keySize, SceUInt32 tweakKeySize, SceUInt32 sectorLow,
			     SceUInt32 sectorHigh, SceUInt32 sectorSize, SceSize size,
			     SceUInt16 flags)
{
	return 0;
}

/* PLACEHOLDER: cryptographic routine, deferred */
int pfsSectorCryptXtsDecrypt(void *dst, const void *src, const void *key, const void *tweakKey,
			     SceUInt32 keySize, SceUInt32 tweakKeySize, SceUInt32 sectorLow,
			     SceUInt32 sectorHigh, SceUInt32 sectorSize, SceSize size,
			     SceUInt16 flags)
{
	return 0;
}

/* PLACEHOLDER: cryptographic routine, deferred */
int pfsSectorCryptXtsSwEncrypt(void *dst, const void *src, SceUInt32 keySize,
			       SceUInt32 tweakKeySize, SceUInt32 sectorLow, SceUInt32 sectorHigh,
			       const void *key, const void *tweakKey, SceUInt16 flags)
{
	return 0;
}

/* PLACEHOLDER: cryptographic routine, deferred */
int pfsSectorCryptXtsSwDecrypt(void *dst, const void *src, SceUInt32 keySize,
			       SceUInt32 tweakKeySize, SceUInt32 sectorLow, SceUInt32 sectorHigh,
			       const void *key, const void *tweakKey, SceUInt16 flags)
{
	return 0;
}

/* PLACEHOLDER: cryptographic routine, deferred */
void pfsSectorTweakEncrypt(SceUInt32 *dst, const SceUInt32 *src, const SceUInt32 *tweak,
			   SceSize size)
{
}

/* PLACEHOLDER: cryptographic routine, deferred */
void pfsSectorTweakDecrypt(SceUInt32 *dst, const SceUInt32 *src, const SceUInt32 *tweak,
			   SceSize size)
{
}

/* PLACEHOLDER: cryptographic routine, deferred */
int pfsSectorCryptCbcEncrypt(void *dst, const void *src, const void *key, const void *iv,
			     SceUInt32 keySize, SceSize size, SceUInt32 flags)
{
	return 0;
}

/* PLACEHOLDER: cryptographic routine, deferred */
int pfsSectorCryptCbcDecrypt(void *dst, const void *src, const void *key, const void *iv,
			     SceUInt32 keySize, SceSize size, SceUInt32 flags)
{
	return 0;
}

/* PLACEHOLDER: cryptographic routine, deferred */
int pfsSectorCryptCbcSwEncrypt(void *dst, const void *src, const void *key, const void *iv,
			       SceUInt32 keySize, SceSize size, SceUInt32 flags)
{
	return 0;
}

/* PLACEHOLDER: cryptographic routine, deferred */
int pfsSectorCryptCbcSwDecrypt(void *dst, const void *src, const void *key, const void *iv,
			       SceUInt32 keySize, SceSize size, SceUInt32 flags)
{
	return 0;
}

/* PLACEHOLDER: cryptographic routine, deferred */
int pfsSectorCryptEcbEncrypt(void *dst, const void *src, SceSize size, const void *key,
			     SceUInt32 keySize)
{
	return 0;
}

/* PLACEHOLDER: cryptographic routine, deferred */
int pfsSectorCryptEcbDecrypt(void *dst, const void *src, SceSize size, const void *key,
			     SceUInt32 keySize)
{
	return 0;
}

/* PLACEHOLDER: cryptographic routine, deferred */
int pfsSectorCryptEcbSlotEncrypt(void *dst, const void *src, SceSize size, const void *key,
				 SceUInt32 keySize, SceUInt16 slot)
{
	return 0;
}

/* PLACEHOLDER: cryptographic routine, deferred */
int pfsSectorCryptEcbSlotDecrypt(void *dst, const void *src, SceSize size, const void *key,
				 SceUInt32 keySize, SceUInt16 slot)
{
	return 0;
}

/* PLACEHOLDER: cryptographic routine, deferred */
int pfsSectorCryptCtrEncrypt(void *dst, const void *src, SceSize size, const void *key,
			     SceUInt32 keySize)
{
	return 0;
}

/* PLACEHOLDER: cryptographic routine, deferred */
int pfsSectorCryptCtrDecrypt(void *dst, const void *src, SceSize size, const void *key,
			     SceUInt32 keySize)
{
	return 0;
}

/* PLACEHOLDER: cryptographic routine, deferred */
int pfsSectorCryptCtrSlotEncrypt(void *dst, const void *src, SceSize size, const void *key,
				 SceUInt32 keySize, SceUInt16 slot)
{
	return 0;
}

/* PLACEHOLDER: cryptographic routine, deferred */
int pfsSectorCryptCtrSlotDecrypt(void *dst, const void *src, SceSize size, const void *key,
				 SceUInt32 keySize, SceUInt16 slot)
{
	return 0;
}
