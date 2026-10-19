/* Descriptive filename; report helpers have no debug source records. */
#include "hid_internal.h"
#include <string.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/suspend.h>

HidReportRouting hidRouting[3];
HidCollection *hidCollections[24];
int hidLastCollectionId;

int hidClearCollectionReports(HidCollection *collection)
{
	HidReportCursor *cursor;
	unsigned int index;
	int intrState;

	for (index = 0; index != 24; index++) {
		if (hidCollections[index] == collection)
			break;
	}
	if (index == 24)
		index = (unsigned int)-1;
	cursor = hidGetProcessReportCursor(index);
	if (cursor == NULL)
		return 0;

	intrState = sceKernelSpinlockLowLockCpuSuspendIntr(&cursor->spinlock);
	while (cursor->pendingCount > 0) {
		SceSize size;

		switch (collection->type) {
		case 1:
			size = sizeof(SceHidMouseReport);
			break;
		case 2:
			size = sizeof(SceHidKeyboardReport);
			break;
		case 3:
			size = sizeof(SceHidConsumerControlReport);
			break;
		default:
			goto advance;
		}
		memset(&collection->reports[cursor->readIndex].report, 0, size);
advance:
		cursor->pendingCount--;
		cursor->readIndex = (cursor->readIndex + 1) % 16;
	}
	cursor->peekCount = cursor->readIndex;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&cursor->spinlock, intrState);
	return 0;
}

int hidIsSupportedConsumerUsage(unsigned int usage)
{
	return usage - 342 > 9 && usage - 363 > 4 &&
	       usage - 373 > 10 && usage != 443 &&
	       usage - 456 > 55 && usage - 522 > 15 && usage <= 668;
}

int hidReadCollectionReports(HidCollection *collection, void *buffer, int count)
{
	HidReportCursor *cursor;
	HidReportRouting *routing;
	unsigned int index, privileges = 0;
	SceUID pid;
	int intrState, copied = 0;

	for (index = 0; index != 24; index++) {
		if (hidCollections[index] == collection)
			break;
	}
	if (index == 24)
		index = (unsigned int)-1;
	cursor = hidGetProcessReportCursor(index);
	if (cursor == NULL)
		return 0;
	switch (collection->type) {
	case 1:
		routing = &hidRouting[0];
		break;
	case 2:
		routing = &hidRouting[1];
		break;
	case 3:
		routing = &hidRouting[2];
		break;
	default:
		routing = NULL;
		break;
	}
	pid = sceKernelGetProcessId();
	hidGetProcessPrivileges(pid, &privileges);
	intrState = sceKernelSpinlockLowLockCpuSuspendIntr(&cursor->spinlock);
	if (count <= 0 || cursor->pendingCount <= 0)
		goto unlock;
	do {
		HidReportEntry *entry = &collection->reports[cursor->readIndex];

		switch (collection->type) {
		case 1:
			((SceHidMouseReport *)buffer)[copied] = entry->report.mouse;
			break;
		case 2: {
			SceHidKeyboardReport *output = buffer;

			if ((privileges & 1) ||
			    (pid == routing->ownerPid && (int)entry->flags >= 0)) {
				output[copied] = entry->report.keyboard;
			} else {
				__builtin_prefetch(&output[copied + 3].modifiers[1]);
				memset(&output[copied], 0, sizeof(*output));
				output[copied].error = 0;
				output[copied].modifiers[0] = 0;
				output[copied].modifiers[1] = entry->report.keyboard.modifiers[1];
				output[copied].timestamp = entry->report.keyboard.timestamp;
			}
			break;
		}
		case 3:
			((SceHidConsumerControlReport *)buffer)[copied] =
				entry->report.consumer;
			break;
		}
		copied++;
		cursor->pendingCount--;
		cursor->readIndex = (cursor->readIndex + 1) % 16;
	} while (copied != count && cursor->pendingCount > 0);
unlock:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&cursor->spinlock, intrState);
	return copied;
}

int hidGetCollectionWriteIndex(unsigned int index)
{
	HidCollection *collection;

	if (index > 23)
		return 0;
	collection = hidCollections[index];
	if (collection != NULL)
		return collection->writeIndex;
	return 0;
}

void hidInitializeReportLists(void)
{
	hidRouting[0].pendingOwner = 0;
	hidRouting[0].ownerPid = 0;
	hidRouting[0].pendingOwnerPid = 0;
	hidRouting[0].allKeysReleased = 0;
	hidRouting[1].pendingOwner = 0;
	hidRouting[1].ownerPid = 0;
	hidRouting[1].pendingOwnerPid = 0;
	hidRouting[1].allKeysReleased = 0;
	hidRouting[2].pendingOwner = 0;
	hidRouting[2].ownerPid = 0;
	hidRouting[2].pendingOwnerPid = 0;
	hidRouting[2].allKeysReleased = 0;
}

HidCollection *hidCreateCollection(const HidGlobalState *global,
				   const HidLocalState *local)
{
	HidCollection **slot, *collection;
	HidReportCursor *cursor;
	unsigned int index;
	int type, id, intrState;

	if (global->usagePage == 1) {
		if (local->usages[0] == 2) {
			slot = &hidCollections[0];
			type = 1;
		} else if (local->usages[0] == 6) {
			slot = &hidCollections[8];
			type = 2;
		} else {
			return NULL;
		}
	} else if (global->usagePage == 12 && local->usages[0] == 1) {
		slot = &hidCollections[16];
		type = 3;
	} else {
		return NULL;
	}
	if (slot[0] == NULL)
		slot += 0;
	else if (slot[1] == NULL)
		slot += 1;
	else if (slot[2] == NULL)
		slot += 2;
	else if (slot[3] == NULL)
		slot += 3;
	else if (slot[4] == NULL)
		slot += 4;
	else if (slot[5] == NULL)
		slot += 5;
	else if (slot[6] == NULL)
		slot += 6;
	else if (slot[7] == NULL)
		slot += 7;
	else
		return NULL;

	collection = hidAllocateMemory(sizeof(*collection));
	if (collection == NULL)
		return NULL;
	memset(collection, 0, sizeof(*collection));
	id = hidLastCollectionId;
nextId:
	id = (int)((unsigned int)id + 1);
	if (id < 1)
		id = 1;
	for (index = 0; index != 24; index++) {
		if (hidCollections[index] != NULL && hidCollections[index]->id == id)
			goto nextId;
	}
	collection->id = id;
	collection->type = type;
	*slot = collection;
	hidLastCollectionId = id;
	hidResetProcessIteration();
	for (;;) {
		for (index = 0; index != 24; index++) {
			if (hidCollections[index] == collection)
				break;
		}
		if (index == 24)
			index = (unsigned int)-1;
		cursor = hidNextProcessReportCursor(index);
		if (cursor == NULL)
			return collection;
		intrState = sceKernelSpinlockLowLockCpuSuspendIntr(&cursor->spinlock);
		cursor->pendingCount = 0;
		cursor->readIndex = 0;
		cursor->peekCount = 0;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&cursor->spinlock, intrState);
	}
}

void hidReleaseCollection(HidCollection *collection, int notify)
{
	HidCollection **slot, **candidate;
	int type = collection->type;

	if (type == 1) {
		slot = &hidCollections[0];
	} else if (type == 2) {
		slot = &hidCollections[8];
	} else if (type == 3) {
		slot = &hidCollections[16];
	} else {
		slot = NULL;
	}
	if (slot[0] == collection)
		goto release;
#define FIND_COLLECTION_SLOT(number) do { \
	candidate = slot + number; \
	if (*candidate == collection) \
		goto selected; \
} while (0)
	FIND_COLLECTION_SLOT(1);
	FIND_COLLECTION_SLOT(2);
	FIND_COLLECTION_SLOT(3);
	FIND_COLLECTION_SLOT(4);
	FIND_COLLECTION_SLOT(5);
	FIND_COLLECTION_SLOT(6);
	FIND_COLLECTION_SLOT(7);
#undef FIND_COLLECTION_SLOT
	return;
selected:
	slot = candidate;
release:
	hidFreeMemory(collection);
	*slot = NULL;
	if (notify == 0) {
		if (type == 1)
			hidNotifyMouseEnumeration();
		else if (type == 2)
			hidNotifyKeyboardEnumeration();
		else if (type == 3)
			hidNotifyConsumerEnumeration();
		SceKernelSuspendForDriver_E0489831(0);
	}
}

void hidParseInputItem(HidCollection *collection, const HidGlobalState *global,
		       const HidLocalState *local, unsigned int flags)
{
	HidReportOffset *offset = &collection->reportOffsets[0];
	HidBitField *field;
	HidMouseAxis *axis;
	unsigned int usage, index;
	int position, count;

	if (collection->device != NULL)
		return;
	if (global->reportId != 0) {
#define FIND_REPORT_OFFSET(number) \
		offset = &collection->reportOffsets[number]; \
		if (offset->reportId == global->reportId) \
			goto foundOffset; \
		if (offset->reportId == 0) { \
			offset->reportId = global->reportId; \
			goto foundOffset; \
		}
		FIND_REPORT_OFFSET(0);
		FIND_REPORT_OFFSET(1);
		FIND_REPORT_OFFSET(2);
		FIND_REPORT_OFFSET(3);
		FIND_REPORT_OFFSET(4);
		FIND_REPORT_OFFSET(5);
		FIND_REPORT_OFFSET(6);
		FIND_REPORT_OFFSET(7);
#undef FIND_REPORT_OFFSET
		return;
	}
foundOffset:
	position = offset->bitOffset;
	count = global->reportCount;
	switch (collection->type) {
	case 1:
		if (global->usagePage == 9) {
			if ((flags & 7) != 2 || global->logicalMinimum != 0 ||
			    global->logicalMaximum != 1 || global->reportSize != 1)
				break;
			usage = local->usageMinimum;
			for (index = 0; usage <= local->usageMaximum &&
			     (int)index < global->reportCount; usage++, index++) {
				switch (usage) {
				case 1:
					field = &collection->descriptor.mouse.buttons[0];
					break;
				case 2:
					field = &collection->descriptor.mouse.buttons[1];
					break;
				case 3:
					field = &collection->descriptor.mouse.buttons[2];
					break;
				case 4:
					field = &collection->descriptor.mouse.buttons[3];
					break;
				case 5:
					field = &collection->descriptor.mouse.buttons[4];
					break;
				default:
					continue;
				}
				field->present = 1;
				field->bitOffset = position + index;
				field->reportId = global->reportId;
			}
		} else if (global->usagePage == 1) {
			if ((flags & 7) != 6) {
				for (index = 0; index != 32 &&
				     (int)index < global->reportCount; index++) {
					if (local->usages[index] == 0)
						break;
				}
				break;
			}
			for (index = 0; index != 32 && (int)index < global->reportCount;
			     index++) {
				usage = local->usages[index];
				if (usage == 0)
					break;
				if (usage == 48)
					axis = &collection->descriptor.mouse.xAxis;
				else if (usage == 49)
					axis = &collection->descriptor.mouse.yAxis;
				else if (usage == 56)
					axis = &collection->descriptor.mouse.wheel;
				else
					continue;
				axis->present = 1;
				axis->reportId = global->reportId;
				axis->bitSize = global->reportSize;
				axis->signExtend = 1;
				axis->bitOffset = position + global->reportSize * index;
			}
		} else if (global->usagePage == 12 && local->usages[0] == 568 &&
			   (flags & 7) == 6) {
			axis = &collection->descriptor.mouse.pan;
			axis->present = 1;
			axis->bitOffset = position;
			axis->reportId = global->reportId;
			axis->bitSize = global->reportSize;
			axis->signExtend = 1;
		}
		break;
	case 2:
		if (global->usagePage != 7)
			break;
		if ((flags & 7) == 0) {
			HidArrayField *keys = &collection->descriptor.keyboard.keys;

			keys->present = 1;
			keys->bitOffset = position;
			keys->reportId = global->reportId;
			keys->bitSize = global->reportSize;
			keys->count = global->reportCount;
		} else if ((flags & 7) == 2 && global->logicalMinimum == 0 &&
			   global->logicalMaximum == 1 && global->reportSize == 1) {
			usage = local->usageMinimum;
			for (index = 0; usage <= local->usageMaximum &&
			     (int)index < global->reportCount; usage++, index++) {
				switch (usage) {
				case 224:
					field = &collection->descriptor.keyboard.modifiers[0];
					break;
				case 225:
					field = &collection->descriptor.keyboard.modifiers[1];
					break;
				case 226:
					field = &collection->descriptor.keyboard.modifiers[2];
					break;
				case 227:
					field = &collection->descriptor.keyboard.modifiers[3];
					break;
				case 228:
					field = &collection->descriptor.keyboard.modifiers[4];
					break;
				case 229:
					field = &collection->descriptor.keyboard.modifiers[5];
					break;
				case 230:
					field = &collection->descriptor.keyboard.modifiers[6];
					break;
				case 231:
					field = &collection->descriptor.keyboard.modifiers[7];
					break;
				default:
					continue;
				}
				field->present = 1;
				field->bitOffset = position + index;
				field->reportId = global->reportId;
			}
		}
		break;
	case 3:
		if (global->usagePage != 12)
			break;
		if ((flags & 7) == 0) {
			HidArrayField *array = &collection->descriptor.consumer.fields.array;

			collection->descriptor.consumer.mode = 1;
			array->present = 1;
			array->bitOffset = position;
			array->reportId = global->reportId;
			array->bitSize = global->reportSize;
			array->count = global->reportCount;
		} else if ((flags & 7) == 2) {
			HidConsumerBitFields *group;

			collection->descriptor.consumer.mode = 2;
			if (!collection->descriptor.consumer.fields.groups[0].present)
				index = 0;
			else if (!collection->descriptor.consumer.fields.groups[1].present)
				index = 1;
			else if (!collection->descriptor.consumer.fields.groups[2].present)
				index = 2;
			else if (!collection->descriptor.consumer.fields.groups[3].present)
				index = 3;
			else if (!collection->descriptor.consumer.fields.groups[4].present)
				index = 4;
			else if (!collection->descriptor.consumer.fields.groups[5].present)
				index = 5;
			else if (!collection->descriptor.consumer.fields.groups[6].present)
				index = 6;
			else if (!collection->descriptor.consumer.fields.groups[7].present)
				index = 7;
			else
				break;
			group = &collection->descriptor.consumer.fields.groups[index];
			group->present = 1;
			for (index = 0; index != 32; index++) {
				usage = local->usages[index];
				if (usage == 0)
					break;
				group->usages[index] = usage;
			}
			group->reportId = global->reportId;
			group->bitOffset = position;
			group->count = global->reportCount;
		}
		break;
	}
	count = global->reportCount;
	offset->bitOffset += global->reportSize * count;
}

void hidParseOutputItem(HidCollection *collection, const HidGlobalState *global,
			const HidLocalState *local, unsigned int flags)
{
	if (collection->device != NULL || collection->type != 2 ||
	    global->usagePage != 8 || (flags & 7) != 2 ||
	    global->logicalMinimum != 0 || global->logicalMaximum != 1 ||
	    global->reportSize != 1)
		return;
	if (local->usageMinimum <= local->usageMaximum &&
	    local->usageMinimum != 0 && local->usageMaximum <= 5) {
		collection->descriptor.keyboard.leds.present = 1;
		collection->descriptor.keyboard.leds.reportId = global->reportId;
	}
}

int hidAttachCollection(HidCollection *collection, HidDevice *device)
{
	unsigned int groupIndex, index, usage;
	int result;

	if (collection->device != NULL)
		return 1;
	if (collection->type == 3) {
		if (collection->descriptor.consumer.mode == 1) {
			result = collection->descriptor.consumer.fields.array.present;
			if (result == 0)
				goto attached;
		} else {
			result = 0;
			for (groupIndex = 0; groupIndex != 8; groupIndex++) {
				HidConsumerBitFields *group =
					&collection->descriptor.consumer.fields.groups[groupIndex];

				if (group->present == 0)
					goto attached;
				for (index = 0; index != 32; index++) {
					usage = group->usages[index];
					if ((uint16_t)(usage - 1) <= 667 &&
					    usage - 7 > 24 && usage - 35 > 12 &&
					    usage - 55 > 8 && usage - 73 > 22 &&
					    usage - 103 > 24 && usage != 159 &&
					    usage - 165 > 10 && usage - 207 > 16 &&
					    usage - 235 > 4 && usage - 246 > 9 &&
					    usage - 270 > 65 &&
					    usage - 342 > 9 && usage - 363 > 4 &&
					    usage - 373 > 10 && usage != 443 &&
					    usage - 456 > 55 && usage - 522 > 15) {
						result = 1;
						goto consumer;
					}
				}
			}
			goto attached;
		}
consumer:
		hidNotifyConsumerEnumeration();
	} else {
		result = 1;
		if (collection->type == 1)
			hidNotifyMouseEnumeration();
		else if (collection->type == 2)
			hidNotifyKeyboardEnumeration();
	}
	SceKernelSuspendForDriver_E0489831(0);
attached:
	collection->device = device;
	return result;
}

int hidCollectionHasReportId(HidCollection *collection, unsigned int reportId)
{
	if (collection->device == NULL)
		return 0;
#define HAS_REPORT(field) ((field).present != 0 && (field).reportId == reportId)
	switch (collection->type) {
	case 1:
		return HAS_REPORT(collection->descriptor.mouse.buttons[0]) ||
		       HAS_REPORT(collection->descriptor.mouse.buttons[1]) ||
		       HAS_REPORT(collection->descriptor.mouse.buttons[2]) ||
		       HAS_REPORT(collection->descriptor.mouse.buttons[3]) ||
		       HAS_REPORT(collection->descriptor.mouse.buttons[4]) ||
		       HAS_REPORT(collection->descriptor.mouse.xAxis) ||
		       HAS_REPORT(collection->descriptor.mouse.yAxis) ||
		       HAS_REPORT(collection->descriptor.mouse.wheel) ||
		       HAS_REPORT(collection->descriptor.mouse.pan);
	case 2:
		return HAS_REPORT(collection->descriptor.keyboard.modifiers[0]) ||
		       HAS_REPORT(collection->descriptor.keyboard.modifiers[1]) ||
		       HAS_REPORT(collection->descriptor.keyboard.modifiers[2]) ||
		       HAS_REPORT(collection->descriptor.keyboard.modifiers[3]) ||
		       HAS_REPORT(collection->descriptor.keyboard.modifiers[4]) ||
		       HAS_REPORT(collection->descriptor.keyboard.modifiers[5]) ||
		       HAS_REPORT(collection->descriptor.keyboard.modifiers[6]) ||
		       HAS_REPORT(collection->descriptor.keyboard.modifiers[7]) ||
		       HAS_REPORT(collection->descriptor.keyboard.keys);
	case 3:
		if (collection->descriptor.consumer.mode == 1)
			return HAS_REPORT(collection->descriptor.consumer.fields.array);
		if (collection->descriptor.consumer.mode != 2)
			return 0;
		return collection->descriptor.consumer.fields.groups[0].reportId == reportId ||
		       collection->descriptor.consumer.fields.groups[1].reportId == reportId ||
		       collection->descriptor.consumer.fields.groups[2].reportId == reportId ||
		       collection->descriptor.consumer.fields.groups[3].reportId == reportId ||
		       collection->descriptor.consumer.fields.groups[4].reportId == reportId ||
		       collection->descriptor.consumer.fields.groups[5].reportId == reportId ||
		       collection->descriptor.consumer.fields.groups[6].reportId == reportId ||
		       collection->descriptor.consumer.fields.groups[7].reportId == reportId;
	default:
		return 0;
	}
#undef HAS_REPORT
}

#define READ_BITS(offset, width, value) do { \
	const uint8_t *bytes = data + (offset) / 8; \
	int start = (offset) % 8, bitPosition; \
	(value) = 0; \
	if ((width) > 0) { \
		for (bitPosition = start; bitPosition != start + (width); bitPosition++) \
			(value) |= (uint32_t)((bytes[bitPosition / 8] >> \
					     (bitPosition % 8)) & 1) << (bitPosition - start); \
	} \
} while (0)

#define REPORT_BIT(field, mask) \
	(((field).present && (field).reportId == reportId) ? \
	 (((data[(field).bitOffset / 8] >> ((field).bitOffset % 8 % 8)) & 1) * (mask)) : 0)

#define READ_AXIS(field, output) do { \
	uint32_t axisValue = 0; \
	int width = (field).bitSize; \
	if ((field).present && (field).reportId == reportId && width > 0) { \
		READ_BITS((field).bitOffset, width, axisValue); \
		if ((field).signExtend && width <= 31 && ((axisValue >> (width - 1)) & 1)) { \
			while (width <= 31) \
				axisValue |= UINT32_C(1) << width++; \
		} \
	} \
	(output) = axisValue; \
} while (0)

#define LOWER_CONSUMER_USAGE(usage) \
	((usage) != 0 && (usage) - 7 > 24 && (usage) - 35 > 12 && \
	 (usage) - 55 > 8 && (usage) - 73 > 22 && (usage) - 103 > 24 && \
	 (usage) != 159 && (usage) - 165 > 10 && (usage) - 207 > 16 && \
	 (usage) - 235 > 4 && (usage) - 246 > 9 && (usage) - 270 > 65)

void hidProcessReport(HidCollection *collection, unsigned int reportId,
		      const uint8_t *data, SceSize length, uint64_t timestamp)
{
	HidReportEntry *entry;
	HidReportCursor *cursor;
	unsigned int index, usage, value;
	int intrState;

	if (hidGetFlags(0x1000) != 0)
		return;
	entry = &collection->reports[collection->writeIndex];
	entry->inputTimestamp = timestamp;
	switch (collection->type) {
	case 1:
		entry->report.mouse.buttons =
			REPORT_BIT(collection->descriptor.mouse.buttons[0], 1) |
			REPORT_BIT(collection->descriptor.mouse.buttons[1], 2) |
			REPORT_BIT(collection->descriptor.mouse.buttons[2], 4) |
			REPORT_BIT(collection->descriptor.mouse.buttons[3], 8) |
			REPORT_BIT(collection->descriptor.mouse.buttons[4], 16);
		READ_AXIS(collection->descriptor.mouse.xAxis, entry->report.mouse.rel_x);
		READ_AXIS(collection->descriptor.mouse.yAxis, entry->report.mouse.rel_y);
		READ_AXIS(collection->descriptor.mouse.wheel, entry->report.mouse.wheel);
		READ_AXIS(collection->descriptor.mouse.pan, entry->report.mouse.tilt);
		entry->report.mouse.timestamp = timestamp;
		break;
	case 2: {
		SceHidKeyboardReport *report = &entry->report.keyboard;
		HidArrayField *keys = &collection->descriptor.keyboard.keys;
		HidKeyboardIntercept intercept = {0};
		unsigned int keyIndex, searchIndex;
		int keyCount = 0, capsLock = 0, numLock = 0, scrollLock = 0;
		int ledChanged = 0, anyKeys = 0, width, position;
		uint8_t ledData[1];

		memset(report, 0, sizeof(*report));
		report->modifiers[0] =
			REPORT_BIT(collection->descriptor.keyboard.modifiers[0], 1) |
			REPORT_BIT(collection->descriptor.keyboard.modifiers[1], 2) |
			REPORT_BIT(collection->descriptor.keyboard.modifiers[2], 4) |
			REPORT_BIT(collection->descriptor.keyboard.modifiers[3], 8) |
			REPORT_BIT(collection->descriptor.keyboard.modifiers[4], 16) |
			REPORT_BIT(collection->descriptor.keyboard.modifiers[5], 32) |
			REPORT_BIT(collection->descriptor.keyboard.modifiers[6], 64) |
			REPORT_BIT(collection->descriptor.keyboard.modifiers[7], 128);
		for (keyIndex = 0; (int)keyIndex < keys->count; keyIndex++) {
			if (!keys->present || keys->reportId != reportId)
				continue;
			width = keys->bitSize < 16 ? keys->bitSize : 16;
			if (width <= 0)
				continue;
			position = keys->bitOffset + keys->bitSize * keyIndex;
			READ_BITS(position, width, value);
			usage = (uint16_t)value;
			if ((uint16_t)(usage - 1) <= 2) {
				memset(report, 0, sizeof(*report));
				report->error = usage;
				break;
			}
			if ((uint16_t)(usage - 1) > 230 || usage - 165 <= 42 ||
			    usage - 222 <= 1)
				continue;
			switch (usage) {
			case 57:
				capsLock = 1;
				break;
			case 71:
				numLock = 1;
				break;
			case 83:
				scrollLock = 1;
				break;
			default:
				for (searchIndex = keyCount > 12; searchIndex < keyCount;
				     searchIndex++) {
					if (report->keycodes[searchIndex] == usage)
						break;
				}
				if (searchIndex != keyCount)
					break;
				if (keyCount <= 12) {
					report->keycodes[keyCount++] = usage;
				} else {
					for (index = 0; index != 12; index++)
						report->keycodes[index] = report->keycodes[index + 1];
					report->keycodes[12] = usage;
				}
				break;
			}
		}
		if (!collection->capsLockPressed && capsLock) {
			collection->ledState ^= 2;
			ledChanged = 1;
		}
		if (!collection->numLockPressed && numLock) {
			collection->ledState ^= 4;
			ledChanged = 1;
		}
		if (!collection->scrollLockPressed && scrollLock) {
			collection->ledState ^= 1;
			ledChanged = 1;
		}
		collection->capsLockPressed = capsLock;
		collection->numLockPressed = numLock;
		collection->scrollLockPressed = scrollLock;
		report->modifiers[1] = collection->ledState;
		report->timestamp = timestamp;
		if (ledChanged) {
			ledData[0] = collection->ledState;
			if (collection->descriptor.keyboard.leds.present)
				hidWriteBluetoothReport(collection->device,
					collection->descriptor.keyboard.leds.reportId, ledData, 1);
		}
		for (index = 0; index != 13; index++)
			anyKeys |= report->keycodes[index] != 0;
		hidGetKeyboardIntercept(hidRouting[1].ownerPid, &intercept);
		if (anyKeys) {
			hidRouting[1].allKeysReleased = 0;
		} else {
			hidRouting[1].allKeysReleased = 1;
			if (intercept.pending) {
				intercept.pending = 0;
				intercept.enabled = intercept.pendingEnabled;
				hidSetKeyboardIntercept(hidRouting[1].ownerPid, intercept);
			}
			if (hidRouting[1].pendingOwner) {
				hidRouting[1].pendingOwner = 0;
				hidRouting[1].ownerPid = hidRouting[1].pendingOwnerPid;
			}
		}
		if (intercept.enabled == 1)
			entry->flags |= UINT32_C(0x80000000);
		else
			entry->flags &= UINT32_C(0x7fffffff);
		break;
	}
	case 3: {
		SceHidConsumerControlReport *report = &entry->report.consumer;
		int copied = 0;

		if (collection->descriptor.consumer.mode == 1) {
			HidArrayField *array = &collection->descriptor.consumer.fields.array;
			int position = array->bitOffset;
			int width = array->bitSize < 16 ? array->bitSize : 16;

			memset(report, 0, sizeof(*report));
			for (index = 0; (int)index < array->count;
			     index++, position += array->bitSize) {
				if (!array->present || array->reportId != reportId || width <= 0)
					continue;
				READ_BITS(position, width, value);
				usage = (uint16_t)value;
				if (LOWER_CONSUMER_USAGE(usage) &&
				    hidIsSupportedConsumerUsage(usage)) {
					if (copied > 3)
						break;
					report->usages[copied++] = usage;
				}
			}
		} else if (collection->descriptor.consumer.mode == 2) {
			unsigned int groupIndex, bit;

			memset(report, 0, sizeof(*report));
			for (groupIndex = 0; groupIndex != 8; groupIndex++) {
				HidConsumerBitFields *group =
					&collection->descriptor.consumer.fields.groups[groupIndex];

				if (!group->present)
					break;
				if (group->reportId != reportId || group->count <= 0)
					continue;
				READ_BITS(group->bitOffset, group->count, value);
				for (bit = 0; (int)bit != group->count; bit++) {
					if (((value >> bit) & 1) == 0 || !group->present)
						continue;
					usage = group->usages[bit];
					if (LOWER_CONSUMER_USAGE(usage) &&
					    hidIsSupportedConsumerUsage(usage))
						break;
				}
				if ((int)bit == group->count)
					continue;
				if (copied > 3)
					break;
				report->usages[copied++] = usage;
			}
		}
		report->timestamp = timestamp;
		break;
	}
	}
	collection->writeIndex = (collection->writeIndex + 1) % 16;
	hidResetProcessIteration();
	for (;;) {
		for (index = 0; index != 24; index++) {
			if (hidCollections[index] == collection)
				break;
		}
		if (index == 24)
			index = (unsigned int)-1;
		cursor = hidNextProcessReportCursor(index);
		if (cursor == NULL)
			break;
		intrState = sceKernelSpinlockLowLockCpuSuspendIntr(&cursor->spinlock);
		if (cursor->pendingCount <= 15)
			cursor->pendingCount++;
		else
			cursor->readIndex = collection->writeIndex;
		if (cursor->peekCount != 16)
			cursor->peekCount++;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&cursor->spinlock, intrState);
	}
	switch (collection->type) {
	case 1:
		hidNotifyMouseRead();
		break;
	case 2:
		hidNotifyKeyboardRead();
		break;
	case 3:
		hidNotifyConsumerRead();
		break;
	}
	SceKernelSuspendForDriver_E0489831(0);
}

#undef READ_BITS
#undef REPORT_BIT
#undef READ_AXIS
#undef LOWER_CONSUMER_USAGE

int hidIsSupportedCollection(HidCollection *collection)
{
	if (collection != NULL)
		return (unsigned int)collection->type - 1 <= 2;
	return 0;
}

int hidGetCollectionInfo(const HidCollection *collection, HidCollectionInfo *info)
{
	int id, type;

	if (collection == NULL)
		return (int)UINT32_C(0x803b0001);
	id = collection->id;
	type = collection->type;
	info->size = sizeof(*info);
	info->id = id;
	info->type = type;
	return 0;
}
