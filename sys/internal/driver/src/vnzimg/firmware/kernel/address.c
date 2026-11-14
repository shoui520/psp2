/*
 * The address windows (see include/vnz.h): an address of one window in
 * another. Each returns 0 for an address outside what the window maps.
 */
#include "kernel.h"

#define VNZ_PHYSICAL_START	0x18000000
#define VNZ_PUBLIC_START	0xc0000000
#define VNZ_PUBLIC_END		0xe0000000
#define VNZ_UNCACHED_START	0xf0000000
#define VNZ_UNCACHED_END	0xf8000000

static int inRange(uint32_t address, uint32_t start, uint32_t size)
{
	return address - start < size;
}

uint32_t vnzKernelPhysicalToPublic(uint32_t address)
{
	if (inRange(address, 0x18000000, 0x08000000) || inRange(address, 0x20000000, 0x08000000))
		return address + 0xa8000000;
	if (inRange(address, 0x40000000, 0x08000000) || inRange(address, 0x48000000, 0x08000000))
		return address + 0x90000000;
	return 0;
}

uint32_t vnzKernelPhysicalToUncached(uint32_t address)
{
	if (inRange(address, 0x1e000000, 0x02000000) || inRange(address, 0x20000000, 0x02000000) ||
	    inRange(address, 0x22000000, 0x02000000))
		return address + 0xd2000000;
	if (inRange(address, 0x40000000, 0x02000000))
		return address + 0xb6000000;
	return 0;
}

uint32_t vnzKernelPublicToPhysical(uint32_t address)
{
	if (inRange(address, 0xc0000000, 0x08000000) || inRange(address, 0xc8000000, 0x08000000))
		return address - 0xa8000000;
	if (inRange(address, 0xd0000000, 0x08000000) || inRange(address, 0xd8000000, 0x08000000))
		return address - 0x90000000;
	return 0;
}

uint32_t vnzKernelPublicToUncached(uint32_t address)
{
	uint32_t physical = vnzKernelPublicToPhysical(address);

	return physical ? vnzKernelPhysicalToUncached(physical) : 0;
}

uint32_t vnzKernelUncachedToPhysical(uint32_t address)
{
	if (inRange(address, 0xf0000000, 0x02000000) || inRange(address, 0xf2000000, 0x02000000) ||
	    inRange(address, 0xf4000000, 0x02000000))
		return address - 0xd2000000;
	if (inRange(address, 0xf6000000, 0x02000000))
		return address - 0xb6000000;
	return 0;
}

uint32_t vnzKernelUncachedToPublic(uint32_t address)
{
	uint32_t physical = vnzKernelUncachedToPhysical(address);

	return physical ? vnzKernelPhysicalToPublic(physical) : 0;
}

static int isPublic(uint32_t address)
{
	return address >= VNZ_PUBLIC_START && address < VNZ_PUBLIC_END;
}

static int isUncached(uint32_t address)
{
	return address >= VNZ_UNCACHED_START && address < VNZ_UNCACHED_END;
}

static int isPhysical(uint32_t address)
{
	return address >= VNZ_PHYSICAL_START && address < VNZ_PUBLIC_START;
}

/* an address of any window, physically */
uint32_t vnzKernelToPhysical(uint32_t address)
{
	if (isPublic(address))
		return vnzKernelPublicToPhysical(address);
	if (isUncached(address))
		return vnzKernelUncachedToPhysical(address);
	if (isPhysical(address))
		return address;
	return 0;
}

uint32_t vnzKernelToPublic(uint32_t address)
{
	if (isPhysical(address))
		return vnzKernelPhysicalToPublic(address);
	if (isUncached(address))
		return vnzKernelUncachedToPublic(address);
	if (isPublic(address))
		return address;
	return 0;
}

uint32_t vnzKernelToUncached(uint32_t address)
{
	if (isPhysical(address))
		return vnzKernelPhysicalToUncached(address);
	if (isPublic(address))
		return vnzKernelPublicToUncached(address);
	if (isUncached(address))
		return address;
	return 0;
}
