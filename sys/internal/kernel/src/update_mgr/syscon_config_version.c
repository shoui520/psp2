#include "spkg_peripherals.h"
#include <syscon/syscon.h>

int SysconUtilGetConfigVersion(uint32_t *version)
{
	uint16_t information[4];
	int result;

	result = SceSysconForDriver_CE48E8EB(information);
	if (result)
		return (int)0x800f0202;
	*version = (uint16_t)((information[0] << 8) | (information[0] >> 8));
	return result;
}
