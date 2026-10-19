#include "spkg_peripherals.h"
#include <deci4p_cpup/cpup.h>

int CpUtilCheckFirmwareType(uint64_t hardwareInfo)
{
	unsigned int protocol = 0;
	unsigned int packageVersion = 0;
	unsigned int boardVersion = 0;

	if (!updateCpupConnected)
		return (int)0x800f0202;
	if (sceDeci4pCpupInquireVersionForDriver(&protocol, &packageVersion,
		&boardVersion))
		return (int)0x800f0205;
	if (boardVersion - 3 <= 1)
		boardVersion = 2;
	return boardVersion == (uint32_t)(hardwareInfo >> 32) ?
		0 : (int)0x800f0202;
}
