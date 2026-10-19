#include "hid_internal.h"
#include <hid/hid.h>

int SceHidForDriver_480D225F(SceUID *handles, int maxCount)
{
	return (int)0x803B0004;
}

int sceHidControllerEnumerate(SceUID *handles, int maxCount)
{
	return (int)0x803B0004;
}

int SceHidForDriver_F7470359(SceUID handle, void *reports, int count)
{
	return (int)0x803B0004;
}

int sceHidControllerRead(SceUID handle, void *reports, int count)
{
	return (int)0x803B0004;
}
