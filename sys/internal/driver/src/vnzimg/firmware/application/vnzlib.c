/*
 * The application's library (0x85fc14-): memory taken through the
 * protocol in one call each way (VVNZRunner's VnzMemory) and a job run on a
 * VThread context and waited for.
 */
#include "application.h"

#define DONE_FLAG			0x80000000

void vnzMemoryCreate(VnzMemory *memory, uint32_t paddr, uint32_t size)
{
	memory->paddr = paddr;
	memory->size = size;
	memory->vaddrVnz = 0;
}

uint32_t vnzMemoryOpenPublic(VnzMemory *memory)
{
	uint32_t size = memory->size, address;

	address = enterMemoryAccessProtocol(memory->paddr, size);
	if (address) {
		address = openPublicMemory(address, size);
		if (address)
			memory->vaddrVnz = address;
	}
	return address;
}

uint32_t vnzMemoryClosePublic(VnzMemory *memory)
{
	uint32_t size = memory->size, address;

	address = closePublicMemory(memory->vaddrVnz, size);
	if (address) {
		memory->vaddrVnz = 0;
		address = leaveMemoryAccessProtocol(address, size);
	}
	return address;
}

uint32_t vnzMemoryOpenPrivate(VnzMemory *memory)
{
	uint32_t size = memory->size, address;

	address = enterMemoryAccessProtocol(memory->paddr, size);
	if (address) {
		address = openPublicMemory(address, size);
		if (address) {
			address = openPrivateMemory(address, size);
			if (address)
				memory->vaddrVnz = address;
		}
	}
	return address;
}

uint32_t vnzMemoryClosePrivate(VnzMemory *memory)
{
	uint32_t size = memory->size, address;

	address = closePrivateMemory(memory->vaddrVnz, size);
	if (address) {
		address = closePublicMemory(address, size);
		if (address) {
			memory->vaddrVnz = 0;
			address = leaveMemoryAccessProtocol(address, size);
		}
	}
	return address;
}

uint32_t vnzMemoryOpenProtected(VnzMemory *memory)
{
	uint32_t size = memory->size, address;

	address = enterMemoryAccessProtocol(memory->paddr, size);
	if (address) {
		address = openPublicMemory(address, size);
		if (address) {
			address = openProtectedMemory(address, size);
			if (address)
				memory->vaddrVnz = address;
		}
	}
	return address;
}

uint32_t vnzMemoryCloseProtected(VnzMemory *memory)
{
	uint32_t size = memory->size, address;

	address = closeProtectedMemory(memory->vaddrVnz, size);
	if (address) {
		address = closePublicMemory(address, size);
		if (address) {
			memory->vaddrVnz = 0;
			address = leaveMemoryAccessProtocol(address, size);
		}
	}
	return address;
}

/* the job, on the context, waited for */
int32_t vnzVThreadExecute(VkVThreadContext *context, uint32_t index, VkJobFunction entry,
	uint32_t argument, uint32_t argument2, uint32_t argument3, uint32_t argument4)
{
	VkVThreadContext *locked = vkVThreadContextLock(context);
	int32_t result;

	result = vkVThreadJobSetNotify(locked, index, 0, entry, argument, argument2, argument3,
		argument4, vnzThreadCurrent(), DONE_FLAG);
	result |= vkVThreadContextUnlock(locked);
	if (result)
		return -10001;
	vnzThreadWaitFlags(DONE_FLAG);
	return 0;
}
