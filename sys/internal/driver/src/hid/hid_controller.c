#include "hid_internal.h"

int ksceHidControllerEnumerate(SceUID *handles, int maxCount)
{
	return (int)0x803B0004;
}

int sceHidControllerEnumerate(SceUID *handles, int maxCount)
{
	return (int)0x803B0004;
}

int ksceHidControllerRead(SceUID handle, void *reports, int count)
{
	return (int)0x803B0004;
}

int sceHidControllerRead(SceUID handle, void *reports, int count)
{
	return (int)0x803B0004;
}
