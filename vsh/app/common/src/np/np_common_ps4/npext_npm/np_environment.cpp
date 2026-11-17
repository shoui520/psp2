/*
 * SceNpCommonPs4: the NP environment name (npext_npm/np_environment.cpp), the "np" or "sp-int" part of the
 * hosts of the NP servers.
 */

#include "../np_common_ps4_internal.h"

extern "C" {

int npCommonPs4NpEnvironmentGet(char *environment)
{
	int result;

	result = sceNpCommonGetNpEnviroment(environment);
	if (result >= 0) {
		environment[16] = 0;
		return 0;
	}
	return result;
}

}
