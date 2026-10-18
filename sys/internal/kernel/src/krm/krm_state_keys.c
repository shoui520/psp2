#include "krm_internal.h"

#define seedForIV g_krmSeedForIv
#define seedForKey g_krmSeedForKey

int KrmDriverDeriveStateKeys(void)
{
	SceKernelOpenPsId id;
	unsigned int count;
	int ret;

	ret = KrmDriverGetOpenPsId(&id);
	if (ret < 0)
		return ret;
	__builtin_memcpy(g_krmStateKey.seed.label, seedForIV, 16);
	g_krmStateKey.seed.id = id;
	count = 16;
	do {
		ret = KrmDriverSha256(g_krmStateKey.bytes, g_krmStateKey.bytes, 32);
		if (ret < 0)
			goto error;
	} while (--count);
	__builtin_memcpy(g_krmStateIv, g_krmStateKey.bytes, 16);
	__builtin_memcpy(&g_krmStateKey, seedForKey, sizeof(seedForKey));
	g_krmStateKey.seed.id = id;
	count = 16;
	do {
		ret = KrmDriverSha256(g_krmStateKey.bytes, g_krmStateKey.bytes, 32);
		if (ret < 0)
			goto error;
	} while (--count);
	__builtin_memset(&id, 0, sizeof(id));
	if (ret == 0)
		return ret;
	goto clearKeys;
error:
	__builtin_memset(&id, 0, sizeof(id));
clearKeys:
	__builtin_memset(g_krmStateIv, 0, sizeof(g_krmStateIv));
	__builtin_memset(&g_krmStateKey, 0, sizeof(g_krmStateKey));
	return ret;
}
