/* Descriptive file name: the original has no source-file debug records. */
#include "magicgate_private.h"

int typeOneInitialSlot = -1;
int typeZeroInitialSlot = -1;

int8_t typeTwoSlotIndex;
int8_t typeOneSlotIndex;
int8_t typeThreeSlotIndices[2];
int8_t typeZeroSlotIndices[4];
int8_t typeFourSlotIndices[32];
int8_t typeFiveSlotIndex;
int slotState;
MagicGateSlot magicGateSlots[41];

void initializeSlots(void)
{
	int index;
	int slot;
	int result;

	typeZeroSlotIndices[0] = 0;
	typeZeroSlotIndices[1] = 1;
	typeZeroSlotIndices[2] = 2;
	typeZeroSlotIndices[3] = 3;
	magicGateSlots[0].index = &typeZeroSlotIndices[0];
	magicGateSlots[0].type = 0;
	magicGateSlots[0].flags = 0;
	magicGateSlots[0].storageOffset = 0;
	magicGateSlots[1].index = &typeZeroSlotIndices[1];
	magicGateSlots[1].type = 0;
	magicGateSlots[1].flags = 0;
	magicGateSlots[1].storageOffset = 64;
	magicGateSlots[2].index = &typeZeroSlotIndices[2];
	magicGateSlots[2].type = 0;
	magicGateSlots[2].flags = 0;
	magicGateSlots[2].storageOffset = 128;
	magicGateSlots[3].index = &typeZeroSlotIndices[3];
	magicGateSlots[3].type = 0;
	magicGateSlots[3].flags = 0;
	magicGateSlots[3].storageOffset = 192;
	typeOneSlotIndex = 4;
	magicGateSlots[4].index = &typeOneSlotIndex;
	magicGateSlots[4].type = 1;
	magicGateSlots[4].flags = 0;
	magicGateSlots[4].storageOffset = 0;
	typeTwoSlotIndex = 5;
	magicGateSlots[5].index = &typeTwoSlotIndex;
	magicGateSlots[5].type = 2;
	magicGateSlots[5].flags = 0;
	magicGateSlots[5].storageOffset = 0;
	typeThreeSlotIndices[0] = 6;
	typeThreeSlotIndices[1] = 7;
	magicGateSlots[6].index = &typeThreeSlotIndices[0];
	magicGateSlots[6].type = 3;
	magicGateSlots[6].flags = 0;
	magicGateSlots[6].storageOffset = 0;
	magicGateSlots[7].index = &typeThreeSlotIndices[1];
	magicGateSlots[7].type = 3;
	magicGateSlots[7].flags = 0;
	magicGateSlots[7].storageOffset = 0;
	for (index = 0; index < 32; index += 4) {
		typeFourSlotIndices[index] = index + 8;
		magicGateSlots[index + 8].index = &typeFourSlotIndices[index];
		magicGateSlots[index + 8].type = 4;
		magicGateSlots[index + 8].flags = 0;
		magicGateSlots[index + 8].storageOffset = 1024 + index * 2048;
		typeFourSlotIndices[index + 1] = index + 9;
		magicGateSlots[index + 9].index = &typeFourSlotIndices[index + 1];
		magicGateSlots[index + 9].type = 4;
		magicGateSlots[index + 9].flags = 0;
		magicGateSlots[index + 9].storageOffset = 1024 + (index + 1) * 2048;
		typeFourSlotIndices[index + 2] = index + 10;
		magicGateSlots[index + 10].index = &typeFourSlotIndices[index + 2];
		magicGateSlots[index + 10].type = 4;
		magicGateSlots[index + 10].flags = 0;
		magicGateSlots[index + 10].storageOffset = 1024 + (index + 2) * 2048;
		typeFourSlotIndices[index + 3] = index + 11;
		magicGateSlots[index + 11].index = &typeFourSlotIndices[index + 3];
		magicGateSlots[index + 11].type = 4;
		magicGateSlots[index + 11].flags = 0;
		magicGateSlots[index + 11].storageOffset = 1024 + (index + 3) * 2048;
	}
	typeFiveSlotIndex = 40;
	magicGateSlots[40].index = &typeFiveSlotIndex;
	magicGateSlots[40].type = 5;
	magicGateSlots[40].flags = 0;
	magicGateSlots[40].storageOffset = 0;
	result = SceMagicGateForDriver_A49B9D70();
	if (result >= 0) {
		slot = typeZeroSlotIndices[0];
		if (!(magicGateSlots[slot].flags & 1))
			goto allocatedZero;
		slot = typeZeroSlotIndices[1];
		if (!(magicGateSlots[slot].flags & 1))
			goto allocatedZero;
		slot = typeZeroSlotIndices[2];
		if (!(magicGateSlots[slot].flags & 1))
			goto allocatedZero;
		slot = typeZeroSlotIndices[3];
		if (!(magicGateSlots[slot].flags & 1))
			goto allocatedZero;
		result = (int)0x80501006;
		SceMagicGateForDriver_492EE950();
		goto finishZero;
allocatedZero:
		magicGateSlots[slot].flags |= 1;
		result = slot;
		SceMagicGateForDriver_492EE950();
	}
finishZero:
	typeZeroInitialSlot = result;
	result = SceMagicGateForDriver_A49B9D70();
	if (result >= 0) {
		result = typeOneSlotIndex;
		if (magicGateSlots[result].flags & 1) {
			result = (int)0x80501006;
			SceMagicGateForDriver_492EE950();
		} else {
			magicGateSlots[result].flags |= 1;
			SceMagicGateForDriver_492EE950();
		}
	}
	typeOneInitialSlot = result;
	slotState = 0;
}

int clearSlotState(void)
{
	MagicGateSlot *slot;
	int result;

	if (magicGateSlots[0].flags & 1)
		magicGateSlots[0].flags &= ~1;
	for (slot = magicGateSlots + 1; slot != magicGateSlots + 41; slot += 4) {
		if (slot[0].flags & 1)
			slot[0].flags &= ~1;
		if (slot[1].flags & 1)
			slot[1].flags &= ~1;
		if (slot[2].flags & 1)
			slot[2].flags &= ~1;
		result = slot[3].flags;
		if (result & 1)
			slot[3].flags = result & ~1;
	}
	slotState = 0;
	return result;
}

int SceMagicGateForDriver_FEF11D84(int type)
{
	int8_t *indices;
	MagicGateSlot *entry;
	uint8_t flags;
	int count;
	int index;
	int remainder;
	int slot;
	int result;

	switch (type) {
	case 0:
		indices = typeZeroSlotIndices;
		count = 4;
		break;
	case 1:
		indices = &typeOneSlotIndex;
		count = 1;
		break;
	case 2:
		indices = &typeTwoSlotIndex;
		count = 1;
		break;
	case 3:
		indices = typeThreeSlotIndices;
		count = 2;
		break;
	case 4:
		indices = typeFourSlotIndices;
		count = 32;
		break;
	case 5:
		indices = &typeFiveSlotIndex;
		count = 1;
		break;
	default:
		return (int)0x80501005;
	}
	result = SceMagicGateForDriver_A49B9D70();
	if (result < 0)
		return result;
	slot = indices[0];
	entry = &magicGateSlots[slot];
	flags = entry->flags;
	if (!(flags & 1))
		goto allocated;
	index = 1;
	if (count <= 1)
		goto exhausted;
	remainder = (count - 1) & 3;
	if (remainder) {
		if (remainder != 1) {
			if (remainder != 2) {
				slot = indices[index];
				entry = &magicGateSlots[slot];
				flags = entry->flags;
				if (!(flags & 1))
					goto allocated;
				index = 2;
			}
			slot = indices[index];
			entry = &magicGateSlots[slot];
			flags = entry->flags;
			if (!(flags & 1))
				goto allocated;
			++index;
		}
		slot = indices[index];
		entry = &magicGateSlots[slot];
		flags = entry->flags;
		if (!(flags & 1))
			goto allocated;
		if (count <= ++index)
			goto exhausted;
	}
	do {
		slot = indices[index];
		entry = &magicGateSlots[slot];
		flags = entry->flags;
		if (!(flags & 1))
			goto allocated;
		slot = indices[index + 1];
		entry = &magicGateSlots[slot];
		flags = entry->flags;
		if (!(flags & 1))
			goto allocated;
		slot = indices[index + 2];
		entry = &magicGateSlots[slot];
		flags = entry->flags;
		if (!(flags & 1))
			goto allocated;
		slot = indices[index + 3];
		entry = &magicGateSlots[slot];
		flags = entry->flags;
		if (!(flags & 1))
			goto allocated;
		index += 4;
	} while (count > index);
exhausted:
	SceMagicGateForDriver_492EE950();
	return (int)0x80501006;
allocated:
	entry->flags = flags | 1;
	SceMagicGateForDriver_492EE950();
	return slot;
}

int SceMagicGateForDriver_144BFA76(int index)
{
	MagicGateSlot *slot;

	if (index > 40)
		return (int)0x80501006;
	slot = &magicGateSlots[index];
	if (!(slot->flags & 1))
		return (int)0x80501006;
	slot->flags &= ~1;
	return 0;
}

unsigned int getInitialSlot(int type)
{
	if (type == 0) {
		slotState = 0;
		return typeZeroInitialSlot;
	}
	if (type == 1)
		return typeOneInitialSlot;
	return (int)0x80501005;
}
