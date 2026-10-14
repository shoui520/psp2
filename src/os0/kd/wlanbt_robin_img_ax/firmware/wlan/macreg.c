/*
 * MAC register sequences: operation polling, indexed-table access and
 * small bit changes. The filename is inferred; register names are positional
 * (see robin/mac.h).
 */
#include <stddef.h>
#include "robin/mac.h"
#include "robin/rom.h"

/*
 * Starts a MAC operation and polls its progress field until it reads 3,
 * calling step(1) between polls; limit 0 means poll forever. Returns whether
 * it finished and stores the progress bits in *status.
 */
int wlanMacPollOperation(unsigned int limit, uint32_t *status, void (*step)(int))
{
	unsigned int remaining = limit;
	int done;

	MAC_650_OPERATION &= ~4u;
	MAC_650_OPERATION |= 3;
	while (((MAC_658_OPERATION_STATUS << 26) >> 30) != 3 && (limit == 0 || remaining != 0)) {
		step(1);
		if (remaining != 0)
			--remaining;
	}
	*status = MAC_658_OPERATION_STATUS & 0x30;
	done = 1;
	if (limit != 0)
		done = remaining != 0;
	return done;
}

/* Sets the value in bits 3-12 of the configuration word and the nibble b in bits 16-19 and 28-31. */
uint32_t wlanMacProgramConfig(uint32_t a, uint8_t b)
{
	uint32_t value = (MAC_13C_CONFIG & 0x0ff0fc07) | ((uint32_t)b << 28) | ((8 * a) & 0x3ff) |
			 ((b & 0xf) << 16);

	MAC_13C_CONFIG = value;
	return value;
}

/* code bit 7 sets and clears the bit given by code bits 0-2 in *mask and the register. */
void wlanMacSetPolicyBit(uint32_t *mask, uint32_t code)
{
	uint32_t bit = 1u << (code & 7);
	uint32_t value;

	if (!(code & 0x80)) {
		if (!(*mask & bit))
			return;
		value = *mask & ~bit;
	} else {
		if (*mask & bit)
			return;
		value = *mask | bit;
	}
	*mask = value;
	MAC_090_POLICY_MASK = value;
}

/* Checks two indexed entries; the second value is offset by 0x2000 when upper is set. */
int wlanMacEntryCheckPair(unsigned int index, const uint32_t *entry, int upper)
{
	if (romMacEntryCheck(index, entry[0]) == 0)
		return 0;
	return romMacEntryCheck(index + 1, entry[2] + (upper != 0 ? 0x2000 : 0)) != 0;
}

int wlanMacEntryWritePair(unsigned int index, const uint32_t *entry)
{
	return romMacEntryWrite(index, entry[0], entry[1]) != 0 &&
	       romMacEntryWrite(index + 1, entry[2], entry[3]) != 0;
}

/* Writes count pairs (a lone pair when count is 0), two indexes each. */
int wlanMacEntryWriteTable(unsigned int index, unsigned int count, const uint32_t *entries)
{
	unsigned int i;

	if (count == 0)
		return wlanMacEntryWritePair(index, entries) != 0;
	for (i = 0; i < count; ++i) {
		if (!wlanMacEntryWritePair(index, entries))
			return 0;
		index += 2;
		entries += 4;
	}
	return 1;
}

int wlanMacEntryCheckTable(unsigned int index, unsigned int count, const uint32_t *entries, int upper)
{
	unsigned int i;

	if (count == 0)
		return wlanMacEntryCheckPair(index, entries, upper) != 0;
	for (i = 0; i < count; ++i) {
		if (!wlanMacEntryCheckPair(index, entries, upper))
			return 0;
		index += 2;
		entries += 4;
	}
	return 1;
}

void wlanMacClear46cBits(void)
{
	MAC_46C_FILTER &= 0xbffdffff;
}

void wlanMacSet46cBits(void)
{
	MAC_46C_FILTER |= 0x40020000;
}

void wlanMacWrite474(uint32_t value)
{
	MAC_474_VALUE = value;
}

uint32_t wlanMacRead5f8(void)
{
	return MAC_5F8_STATUS;
}

void wlanMacSetE80Bits(void)
{
	MAC_E80_CONTROL |= 0xc0;
}

/* Writes a 6-byte entry (four and two bytes) into table entry index; 1 if the index is out of range. */
int wlanMacTableWrite(uint32_t low, uint32_t high, unsigned int index)
{
	if (index - MAC_TABLE_FIRST >= MAC_TABLE_COUNT)
		return 1;
	while (MAC_310_TABLE_COMMAND & MAC_TABLE_BUSY)
		;
	MAC_314_TABLE_LOW = low;
	MAC_318_TABLE_HIGH = high;
	MAC_310_TABLE_COMMAND = index | MAC_TABLE_WRITE;
	return 0;
}

/* Reads table entry index into entry[0..2]; 1 if the index is out of range. */
int wlanMacTableRead(uint16_t *entry, unsigned int index)
{
	uint32_t low;
	uint16_t high;

	if (index - MAC_TABLE_FIRST >= MAC_TABLE_COUNT)
		return 1;
	while (MAC_310_TABLE_COMMAND & MAC_TABLE_BUSY)
		;
	MAC_310_TABLE_COMMAND = index | MAC_TABLE_READ;
	low = MAC_314_TABLE_LOW;
	high = MAC_318_TABLE_HIGH;
	__aeabi_memcpy(entry, &low, 4);
	entry[2] = high;
	return 0;
}

/* Loads the bases of regions 0, 1 and 6 from the provider, then waits for the MAC to settle. */
int wlanMacLoadRegions(const struct MacRegionProvider *provider)
{
	uint32_t base;
	unsigned int i;

	base = provider->lookup(0);
	if (base == 0)
		return 0;
	MAC_000_REGION_A = base;
	base = provider->lookup(1);
	if (base == 0)
		return 0;
	MAC_004_REGION_B = base;
	base = provider->lookup(6);
	if (base == 0)
		return 0;
	MAC_008_REGION_C = base;
	MAC_254_PENDING_REQUEST = 0;
	i = 0;
	do {
		if ((MAC_258_PENDING_STATUS << 22) == 0)
			break;
		++i;
	} while (i < 100);
	return 1;
}

/* True when the halfwords the two pointer arrays point to are equal. */
int wlanHalfwordsEqual(const uint16_t *const *first, const uint16_t *const *second, unsigned int count)
{
	unsigned int i;

	for (i = 0; i < count; ++i) {
		if (*first[i] != *second[i])
			return 0;
	}
	return 1;
}

/*
 * Writes a MAC register given by page and low byte, unless the byte lies
 * within the guarded window around from.
 */
int wlanMacWriteChecked(unsigned int from, unsigned int to, unsigned int page, uint32_t value)
{
	from &= 0xff;
	to &= 0xff;
	if ((from < to && from + 248 <= to) || (from > to && from <= to + 8))
		return 0;
	REG32(SOC_MAC_REGISTER_BASE | (page << 8) | to) = value;
	return 1;
}

void wlanMacClear868Enable(void)
{
	MAC_868_ENABLE &= ~0x80u;
}

/*
 * Highest set bit of mask at or below start that is at or above low; start is
 * clamped to high, and 0 is returned when none is set.
 */
unsigned int wlanFindSetBitDown(uint32_t mask, unsigned int start, unsigned int high, unsigned int low)
{
	if (start >= high)
		return high;
	if (start <= low)
		return start;
	while (start >= low) {
		if ((1u << start) & mask)
			return (uint8_t)start;
		--start;
	}
	return 0;
}

void wlanBaWindowEnable(int window)
{
	MAC_E00_BA_CONTROL |= 1 << (window + 8);
}

void wlanBaWindowDisable(int window)
{
	MAC_E00_BA_CONTROL &= ~(1 << (window + 8));
}

/* Marks a hardware slot active for a window and enables the window. */
int wlanBaSlotApply(int slot, int window)
{
	int control;

	MAC_AC0_BA_ACTIVE |= 1 << slot;
	MAC_ACC_BA_WINDOWS |= (slot | 8) << (4 * window);
	control = MAC_E00_BA_CONTROL;
	MAC_E00_BA_CONTROL |= 1 << (window + 8);
	return control;
}

/* Undoes wlanBaSlotApply(). */
int wlanBaSlotRelease(int slot, int window)
{
	wlanBaWindowDisable(window);
	MAC_AC0_BA_ACTIVE &= ~(0x11000001 << slot);
	MAC_ACC_BA_WINDOWS &= ~((slot | 8) << (4 * window));
	return 1;
}

void wlanBaWindowCounterReset(int window)
{
	MAC_EC0_BA_COUNTER(window) = 0x2000000;
}

/* Keeps bits 0, 1, 5-8 and above 11 of the E80 register and sets bit 0. */
void wlanMacE80Select(void)
{
	MAC_E80_CONTROL = (MAC_E80_CONTROL & 0xfffff89c) | 1;
}
