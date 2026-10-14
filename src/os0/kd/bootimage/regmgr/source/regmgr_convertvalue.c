/*
 * regmgr_convertvalue.c: values whose meaning changed between versions.
 */

#include "regmgr_internal.h"

/* 1.50 (0x9415) numbered the languages anew: the new number of each old one */
static const int g_regMgrLanguage[16] = {	/* name unknown */
	14, 4, 3, 12, 2, 5, 0, 9, 6, 15, 16, 7, 8, 13, 10, 11,
};

void regMgrConvertLanguage(int version, int newVersion)
{
	int language, value;

	if (version < 0)
		version &= ~REGMGR_VERSION_DIRTY;
	if (regMgrGetVolatileMode() == 1 || version == 0)
		return;
	if (newVersion > 0x9414 && version <= 0x9414) {
		if (regMgrGetKeyInt("/CONFIG/SYSTEM", "language", &language) != 0) {
			value = 1;
			language = 0;
		} else if ((SceUInt32)(language - 1) < 16) {
			value = g_regMgrLanguage[language - 1];
		} else {
			value = 1;
		}
		regMgrSetKeyInt("/CONFIG/SYSTEM", "language", value);
	}
}
