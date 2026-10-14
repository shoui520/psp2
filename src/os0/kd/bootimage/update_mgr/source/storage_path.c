#include <psp2kern/kernel/sysclib.h>
#include "update_internal.h"

int StorageUtilGetPartitionPath(uint32_t partition, uint8_t active,
		char *path, SceSize size)
{
	const char *device, *bank, *name;

	if (!path || size < 64)
		return (int)0x800f0216;
	memset(path, 0, size);
	bank = active & 1 ? "act-" : "ina-";
	switch (partition) {
	case 1:
		device = "int";
		name = "idstor";
		bank = "ign-";
		break;
	case 2:
		device = "int";
		name = "sloader";
		break;
	case 3:
		device = "int";
		name = "os";
		break;
	case 4:
		device = "int";
		name = "vsh";
		bank = "ign-";
		break;
	case 5:
		device = "int";
		name = "vshdata";
		bank = "ign-";
		break;
	case 6:
		device = "int";
		name = "vtrm";
		bank = "ign-";
		break;
	case 7:
		device = "int";
		name = "user";
		bank = "ign-";
		break;
	case 8:
		device = "xmc";
		name = "userext";
		bank = "ign-";
		break;
	case 9:
		device = "gcd";
		name = "gamero";
		bank = "ign-";
		break;
	case 10:
		device = "gcd";
		name = "gamerw";
		bank = "ign-";
		break;
	case 11:
		device = "int";
		name = "updater";
		bank = "ign-";
		break;
	case 12:
		device = "int";
		name = "sysdata";
		bank = "ign-";
		break;
	case 14:
		device = "int";
		name = "pidata";
		bank = "ign-";
		break;
	case 255:
		device = "int";
		name = "entire";
		break;
	default:
		return (int)0x800f0216;
	}
	snprintf(path, size - 1, "sdstor0:%s-lp-%s%s", device, bank, name);
	return 0;
}
