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

uint32_t vkPhysicalToPublic(uint32_t address)
{
	if (inRange(address, 0x18000000, 0x08000000) || inRange(address, 0x20000000, 0x08000000))
		return address + 0xa8000000;
	if (inRange(address, 0x40000000, 0x08000000) || inRange(address, 0x48000000, 0x08000000))
		return address + 0x90000000;
	return 0;
}

uint32_t vkPhysicalToUncached(uint32_t address)
{
	if (inRange(address, 0x1e000000, 0x02000000) || inRange(address, 0x20000000, 0x02000000) ||
	    inRange(address, 0x22000000, 0x02000000))
		return address + 0xd2000000;
	if (inRange(address, 0x40000000, 0x02000000))
		return address + 0xb6000000;
	return 0;
}

uint32_t vkPublicToPhysical(uint32_t address)
{
	if (inRange(address, 0xc0000000, 0x08000000) || inRange(address, 0xc8000000, 0x08000000))
		return address - 0xa8000000;
	if (inRange(address, 0xd0000000, 0x08000000) || inRange(address, 0xd8000000, 0x08000000))
		return address - 0x90000000;
	return 0;
}

uint32_t vkPublicToUncached(uint32_t address)
{
	uint32_t physical = vkPublicToPhysical(address);

	return physical ? vkPhysicalToUncached(physical) : 0;
}

uint32_t vkUncachedToPhysical(uint32_t address)
{
	if (inRange(address, 0xf0000000, 0x02000000) || inRange(address, 0xf2000000, 0x02000000) ||
	    inRange(address, 0xf4000000, 0x02000000))
		return address - 0xd2000000;
	if (inRange(address, 0xf6000000, 0x02000000))
		return address - 0xb6000000;
	return 0;
}

uint32_t vkUncachedToPublic(uint32_t address)
{
	uint32_t physical = vkUncachedToPhysical(address);

	return physical ? vkPhysicalToPublic(physical) : 0;
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
uint32_t vkToPhysical(uint32_t address)
{
	if (isPublic(address))
		return vkPublicToPhysical(address);
	if (isUncached(address))
		return vkUncachedToPhysical(address);
	if (isPhysical(address))
		return address;
	return 0;
}

uint32_t vkToPublic(uint32_t address)
{
	if (isPhysical(address))
		return vkPhysicalToPublic(address);
	if (isUncached(address))
		return vkUncachedToPublic(address);
	if (isPublic(address))
		return address;
	return 0;
}

uint32_t vkToUncached(uint32_t address)
{
	if (isPhysical(address))
		return vkPhysicalToUncached(address);
	if (isPublic(address))
		return vkPublicToUncached(address);
	if (isUncached(address))
		return address;
	return 0;
}
