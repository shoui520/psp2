/* Sysmem-specific counters use the shared exclusive-access primitives. */
#include "sce/atomic.h"

uint32_t ksceKernelAtomicDecIfLowPositive32(uint32_t *address)
{
	uint32_t next;

	AtomicFullBarrier();
	do {
		next = AtomicLoadReserved32(address);
		if (!(next & 0xffff))
			break;
		next--;
	} while (AtomicStoreReserved32(address, next));
	AtomicFullBarrier();
	return next;
}

int ksceKernelAtomicAddAndGetPositive32InRange(uint32_t *address, uint32_t value,
					     int32_t limit)
{
	uint32_t next;
	int reached;

	AtomicFullBarrier();
	do {
		next = AtomicLoadReserved32(address);
		/* Native negative-counter exit has no trailing barrier or store. */
		if ((int32_t)next < 0)
			return 2;
		next += value;
		reached = (int32_t)next >= limit;
		if ((int32_t)next > limit)
			next = (uint32_t)limit;
	} while (AtomicStoreReserved32(address, next));
	AtomicFullBarrier();
	return reached;
}

uint32_t ksceKernelAtomicIncrementHighwaterCounter(uint32_t *address)
{
	uint32_t next;

	AtomicFullBarrier();
	do {
		next = AtomicLoadReserved32(address) + 1;
		if ((next & 0xffff) > (next >> 16))
			next += 0x10000;
	} while (AtomicStoreReserved32(address, next));
	AtomicFullBarrier();
	return next;
}

int32_t ksceKernelAtomicSetIfGreaterGet32(uint32_t *address, int32_t value)
{
	int32_t previous;

	AtomicFullBarrier();
	do {
		previous = (int32_t)AtomicLoadReserved32(address);
		if (value <= previous)
			break;
	} while (AtomicStoreReserved32(address, (uint32_t)value));
	AtomicFullBarrier();
	return value > previous ? value : previous;
}

int32_t ksceKernelAtomicSetIfGreaterGet16(uint16_t *address, int32_t value)
{
	int32_t previous;

	AtomicFullBarrier();
	do {
		previous = (int16_t)AtomicLoadReserved16(address);
		if (value <= previous)
			break;
	} while (AtomicStoreReserved16(address, (uint32_t)value));
	AtomicFullBarrier();
	/* Native returns the full replacement word, not its stored halfword. */
	return value > previous ? value : previous;
}

int32_t ksceKernelAtomicSetIfGreaterGet8(uint8_t *address, int32_t value)
{
	int32_t previous;

	AtomicFullBarrier();
	do {
		previous = (int8_t)AtomicLoadReserved8(address);
		if (value <= previous)
			break;
	} while (AtomicStoreReserved8(address, (uint32_t)value));
	AtomicFullBarrier();
	return value > previous ? value : previous;
}

int32_t ksceKernelAtomicGet32AndSet64(uint64_t *address, int32_t value)
{
	AtomicSet64(address, ((uint64_t)(uint32_t)value << 32) | (uint32_t)value);
	/* Native leaves the destination address in its declared result register. */
	return (int32_t)(uintptr_t)address;
}

int32_t ksceKernelAtomicGet32AndSet64_2(uint64_t *address, int32_t value)
{
	AtomicSet64(address, ((uint64_t)(uint32_t)value << 32) | (uint32_t)value);
	return (int32_t)(uintptr_t)address;
}

int ksceKernelAtomicSubIfGreater64(uint64_t *address, int32_t value)
{
	uint64_t previous, next;
	uint32_t low;
	int insufficient;

	AtomicFullBarrier();
	do {
		previous = AtomicLoadReserved64(address);
		low = (uint32_t)previous;
		insufficient = value > (int32_t)low;
		if (!insufficient)
			low -= (uint32_t)value;
		next = (previous & UINT64_C(0xffffffff00000000)) | low;
	} while (AtomicStoreReserved64(address, next));
	AtomicFullBarrier();
	return insufficient;
}

int ksceKernelAtomicAdd32AndGet64InRange(uint64_t *address, uint32_t value,
				      int32_t limit)
{
	uint64_t previous, next;
	uint32_t low, sum;
	int reached;

	AtomicFullBarrier();
	do {
		previous = AtomicLoadReserved64(address);
		low = (uint32_t)previous;
		sum = low + value;
		reached = (int32_t)sum >= limit;
		if (!reached)
			low = sum;
		next = (previous & UINT64_C(0xffffffff00000000)) | low;
	} while (AtomicStoreReserved64(address, next));
	AtomicFullBarrier();
	return reached;
}

int ksceKernelAtomicAdd32AndGet64InHiLoRange(uint64_t *address, uint32_t value,
					  int32_t limit)
{
	uint64_t previous, next;
	uint32_t low, high, sum;
	int reached;

	AtomicFullBarrier();
	do {
		previous = AtomicLoadReserved64(address);
		low = (uint32_t)previous;
		high = previous >> 32;
		sum = low + value;
		reached = (int32_t)sum >= limit;
		if (!reached)
			low = sum;
		/* Native may select sum for high even when low was retained. */
		if ((int32_t)low > (int32_t)high)
			high = sum;
		next = ((uint64_t)high << 32) | low;
	} while (AtomicStoreReserved64(address, next));
	AtomicFullBarrier();
	return reached;
}

int ksceKernelAtomicLimit64(uint64_t *address, int32_t value)
{
	uint64_t previous, next;
	uint32_t low, high;
	int insufficient;

	AtomicFullBarrier();
	do {
		previous = AtomicLoadReserved64(address);
		low = (uint32_t)previous;
		high = previous >> 32;
		insufficient = value > (int32_t)low;
		if (!insufficient)
			low -= (uint32_t)value;
		if ((int32_t)high >= (int32_t)low)
			high = low;
		next = ((uint64_t)high << 32) | low;
	} while (AtomicStoreReserved64(address, next));
	AtomicFullBarrier();
	return insufficient;
}
