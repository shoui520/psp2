/*
 * How the system runs a PSP title (SceLsdb_BE72A404), and the LiveArea template's filter of an
 * element by its attributes.
 */
#include "livespace_db_internal.h"
#include <regmgr/regmgr.h>

extern "C" {

#if defined(FW_TYPE_CEX)
/*
 * 1, or for a title the compatibility layer runs where the game plugin's mode is 0 or the
 * account's country is Japan, 3 (2 when the mode is 0)
 */
int SceLsdb_BE72A404(const char *titleId)
{
	LsdbGamePluginInterface *game = LsdbGamePlugin();
	int result = 1;
	int mode = -1;
	char country[3];

	if ((game != NULL && (mode = game->getMode()) == 0) ||
	    (country[2] = 0, country[1] = 0, country[0] = 0,
	     sceRegMgrGetKeyStr("/CONFIG/NP", "country", country, 3) == 0 &&
	     sce_paf_strcmp("jp", country) == 0)) {
		if (SceCompatForVsh_70997F92(titleId) == 1) {
			result = 3;
			if (mode == 0)
				result = 2;
		}
	}
	return result;
}
#else
/* 1, or for a title the compatibility layer runs, 3 (2 when the game plugin's mode is 0) */
int SceLsdb_BE72A404(const char *titleId)
{
	LsdbGamePluginInterface *game = LsdbGamePlugin();
	int mode = -1;

	if (game != NULL)
		mode = game->getMode();
	if (SceCompatForVsh_70997F92(titleId) == 1) {
		if (mode == 0)
			return 2;
		return 3;
	}
	return 1;
}
#endif

}
