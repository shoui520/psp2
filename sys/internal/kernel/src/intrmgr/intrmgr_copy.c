/* SceKernelIntrMgr: fixed-size controller-state copy. */
#include "intrmgr.h"

/* Name unknown; sixteen snapshots preserve the original's overlap behavior. */
void *SceIntrmgrForKernel_71E08D71(void *destination, const void *source)
{
	SceUInt32 *dst = destination;
	const SceUInt32 *src = source;

	for (int i = 0; i < 0x200 / 32; i++, dst += 8, src += 8) {
		SceUInt32 word0 = src[0], word1 = src[1];
		SceUInt32 word2 = src[2], word3 = src[3];
		SceUInt32 word4 = src[4], word5 = src[5];
		SceUInt32 word6 = src[6], word7 = src[7];

		dst[0] = word0; dst[1] = word1;
		dst[2] = word2; dst[3] = word3;
		dst[4] = word4; dst[5] = word5;
		dst[6] = word6; dst[7] = word7;
	}
	return dst;
}
