/* Descriptive filename; no Sony debug source record names this service. */
#include "hid_internal.h"
#include <string.h>

extern HidDevice *hidDevices[8];

typedef struct HidDeviceInfo {
	SceSize size;
	unsigned int transportType;
	char name[256];
	/* These spans are untouched by this producer, including failed copies. */
	uint8_t untouchedAfterName[4];
	uint32_t mac0;
	uint32_t mac1;
	uint16_t vendorId;
	uint16_t productId;
	uint16_t version;
	uint8_t primary;
	uint8_t untouchedAfterPrimary;
	HidCollectionInfo collections[8];
	uint8_t untouchedTail[192];
} HidDeviceInfo;

_Static_assert(sizeof(HidDeviceInfo) == 572, "HID device diagnostic stride");
_Static_assert(offsetof(HidDeviceInfo, mac0) == 268, "HID diagnostic address");
_Static_assert(offsetof(HidDeviceInfo, collections) == 284,
	       "HID diagnostic collections");

int hidDumpDevices(void *buffer, SceSize size, SceUInt32 *count)
{
	HidDeviceInfo *output = buffer;
	HidDevice *device;
	unsigned int copied = 0;
	int total = 0, index, collectionIndex, outputIndex;

	if (size >= sizeof(*output) && buffer == NULL) {
		total = (int)0x80020005;
		goto out;
	}
#define COUNT_DEVICE(number) do { \
	if (hidDevices[number] != NULL) \
		++total; \
} while (0)
	COUNT_DEVICE(0);
	COUNT_DEVICE(1);
	COUNT_DEVICE(2);
	COUNT_DEVICE(3);
	COUNT_DEVICE(4);
	COUNT_DEVICE(5);
	COUNT_DEVICE(6);
	COUNT_DEVICE(7);
#undef COUNT_DEVICE
	for (index = 0; index < 8; ++index,
	     output = (HidDeviceInfo *)((uintptr_t)output + sizeof(*output))) {
		device = hidDevices[index];
		if (device != NULL) {
			if (size < sizeof(*output) * (copied + 1))
				break;
			output->size = sizeof(*output);
			output->transportType = device->transportType;
			output->vendorId = device->vendorId;
			output->productId = device->productId;
			output->version = device->version;
			output->primary = device->primary;
			outputIndex = 0;
			for (collectionIndex = 0; collectionIndex < 8; ++collectionIndex) {
				HidCollection *collection = device->collections[collectionIndex];

				if (collection != NULL) {
					hidGetCollectionInfo(collection, &output->collections[outputIndex]);
					device = hidDevices[index];
					++outputIndex;
				}
			}
			output->mac0 = (uint32_t)device->deviceAddress;
			output->mac1 = (uint32_t)(device->deviceAddress >> 32);
			memcpy(output->name, hidDevices[index]->name, sizeof(output->name));
			++copied;
		}
	}
out:
	if (count != NULL)
		*count = copied;
	return total;
}
