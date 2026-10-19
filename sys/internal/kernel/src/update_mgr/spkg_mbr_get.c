#include "update_internal.h"

extern int spkgMbrLoaded;
extern SceMbr spkgMbr;

const SceMbr *SpkgUtilGetMbr(void)
{
	UpdateUtilLockMbr();
	if (spkgMbrLoaded || StorageUtilReadMbr(&spkgMbr)) {
		UpdateUtilUnlockMbr();
		return NULL;
	}
	spkgMbrLoaded = 1;
	UpdateUtilUnlockMbr();
	return &spkgMbr;
}
