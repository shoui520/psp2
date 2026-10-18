/* Descriptive filename; Bluetooth device helpers have no debug source records. */
#include "hid_internal.h"
#include <string.h>

HidDevice *hidDevices[8];
HidDevice *hidParsingDevice;

#define HID_READ_UNSIGNED(value) do { \
	switch (payloadSize) { \
	case 1: (value) = item[1]; break; \
	case 2: (value) = item[1] | (item[2] << 8); break; \
	case 4: \
		(value) = item[1] | (item[2] << 8) | (item[3] << 16) | \
			  ((uint32_t)item[4] << 24); \
		break; \
	default: (value) = 0; break; \
	} \
} while (0)

#define HID_READ_SIGNED(value) do { \
	switch (payloadSize) { \
	case 1: (value) = (int8_t)item[1]; break; \
	case 2: (value) = (int16_t)(item[1] | (item[2] << 8)); break; \
	case 4: \
		(value) = item[1] | (item[2] << 8) | (item[3] << 16) | \
			  ((uint32_t)item[4] << 24); \
		break; \
	default: (value) = 0; break; \
	} \
} while (0)

#define HID_ITEM_AVAILABLE() \
	(payloadSize != 0 && (uintptr_t)item + payloadSize >= (uintptr_t)descriptor && \
	 (uintptr_t)item + payloadSize < (uintptr_t)descriptor + descriptorLength)

void hidConnectBluetoothDevice(uint64_t address)
{
	SceBtRegisteredInfo info;
	HidGlobalState global = {0};
	HidLocalState local;
	HidDevice *device;
	HidCollection *collection;
	SceBtHidRequest *request;
	uint8_t *descriptor, *item;
	unsigned int deviceId, payloadSize, itemSize, itemType, tag, value;
	int registeredCount;
	int slot = -1, index, current, depth = 0, descriptorLength;

	memset(&info, 0, sizeof(info));
	registeredCount = SceBtForDriver_F86D25E2(address, &info, 1);
	deviceId = info.vid | ((unsigned int)info.pid << 16);
	if ((registeredCount == 1 && (info.profiles & 0x40) == 0) ||
	    deviceId == 0x0268054c || deviceId == 0x03290e6f)
		return;
#define FIND_CONNECTION_SLOT(number) do { \
	device = hidDevices[number]; \
	if (device != NULL) { \
		if (device->deviceAddress == address) \
			return; \
	} else if (slot == -1) { \
		slot = (number); \
	} \
} while (0)
	FIND_CONNECTION_SLOT(0);
	FIND_CONNECTION_SLOT(1);
	FIND_CONNECTION_SLOT(2);
	FIND_CONNECTION_SLOT(3);
	FIND_CONNECTION_SLOT(4);
	FIND_CONNECTION_SLOT(5);
	FIND_CONNECTION_SLOT(6);
	FIND_CONNECTION_SLOT(7);
#undef FIND_CONNECTION_SLOT
	if (slot == -1)
		return;
	device = hidAllocateMemory(sizeof(*device));
	if (device == NULL)
		return;
	memset(device, 0, sizeof(*device));
	device->deviceAddress = address;
	device->vendorId = info.vid;
	device->productId = info.pid;
	device->version = info.version;
	device->primary = info.primary;
	device->transportType = 2;
	memcpy(device->name, info.name, 120);
	device->name[120] = '\0';
	descriptor = device->descriptor;
	descriptorLength = SceBtForDriver_06CB4ABA(address, descriptor, 512);
	device->descriptorLength = descriptorLength;
	if (descriptorLength <= 0)
		goto free_device;
	hidParsingDevice = device;
	item = descriptor;
	memset(&local, 0, sizeof(local));
	do {
		value = *item;
		if (value == 0xfe) {
			if ((uintptr_t)item + 1 < (uintptr_t)descriptor ||
			    (uintptr_t)item + 1 >= (uintptr_t)descriptor + descriptorLength)
				break;
			item = (uint8_t *)((uintptr_t)item + item[1] + 3);
			goto next_item;
		}
		payloadSize = value & 3;
		if (payloadSize == 3)
			payloadSize = 4;
		itemSize = payloadSize + 1;
		itemType = (value >> 2) & 3;
		tag = value >> 4;
		if (itemType == 0) {
			switch (tag) {
			case 8:
			case 9:
				if (HID_ITEM_AVAILABLE()) {
#define FIND_INPUT_COLLECTION(number) do { \
	collection = device->collections[number]; \
	if (collection != NULL) \
		goto parse_input_item; \
} while (0)
					FIND_INPUT_COLLECTION(7);
					FIND_INPUT_COLLECTION(6);
					FIND_INPUT_COLLECTION(5);
					FIND_INPUT_COLLECTION(4);
					FIND_INPUT_COLLECTION(3);
					FIND_INPUT_COLLECTION(2);
					FIND_INPUT_COLLECTION(1);
					FIND_INPUT_COLLECTION(0);
#undef FIND_INPUT_COLLECTION
					break;
parse_input_item:
					HID_READ_UNSIGNED(value);
					if (tag == 8)
						hidParseInputItem(collection, &global, &local, value);
					else
						hidParseOutputItem(collection, &global, &local, value);
					descriptor = hidParsingDevice->descriptor;
					descriptorLength = hidParsingDevice->descriptorLength;
				}
				break;
			case 10:
				if (depth != 0) {
					++depth;
					break;
				}
				depth = 1;
				if (!HID_ITEM_AVAILABLE())
					break;
				HID_READ_UNSIGNED(value);
				if (value != 1)
					break;
				if (device->collections[0] == NULL) {
					collection = hidCreateCollection(&global, &local);
					device->collections[0] = collection;
					if (collection == NULL)
						goto free_device;
					goto collection_created;
				}
#define FIND_FREE_COLLECTION(number) do { \
	if (device->collections[number] == NULL) { \
		current = (number); \
		goto create_collection; \
	} \
} while (0)
				FIND_FREE_COLLECTION(1);
				FIND_FREE_COLLECTION(2);
				FIND_FREE_COLLECTION(3);
				FIND_FREE_COLLECTION(4);
				FIND_FREE_COLLECTION(5);
				FIND_FREE_COLLECTION(6);
				FIND_FREE_COLLECTION(7);
#undef FIND_FREE_COLLECTION
				break;
create_collection:
				collection = hidCreateCollection(&global, &local);
				device->collections[current] = collection;
				if (collection == NULL) {
					for (index = 0; index < current; ++index)
						if (hidIsSupportedCollection(device->collections[index]))
							break;
					if (index == current)
						goto free_device;
				}
collection_created:
				descriptor = hidParsingDevice->descriptor;
				descriptorLength = hidParsingDevice->descriptorLength;
				break;
			case 12:
				if (depth-- == 0)
					goto finish_descriptor;
				if (depth != 0)
					break;
#define FIND_FINISHED_COLLECTION(number) do { \
	collection = device->collections[number]; \
	if (collection != NULL) { \
		current = (number); \
		goto attach_collection; \
	} \
} while (0)
				FIND_FINISHED_COLLECTION(7);
				FIND_FINISHED_COLLECTION(6);
				FIND_FINISHED_COLLECTION(5);
				FIND_FINISHED_COLLECTION(4);
				FIND_FINISHED_COLLECTION(3);
				FIND_FINISHED_COLLECTION(2);
				FIND_FINISHED_COLLECTION(1);
				FIND_FINISHED_COLLECTION(0);
#undef FIND_FINISHED_COLLECTION
				break;
attach_collection:
				if (hidAttachCollection(collection, device) == 0) {
					hidReleaseCollection(collection, 1);
					device->collections[current] = NULL;
				}
				descriptor = hidParsingDevice->descriptor;
				descriptorLength = hidParsingDevice->descriptorLength;
				break;
			default:
				break;
			}
			memset(&local, 0, sizeof(local));
		} else if (itemType == 1) {
			switch (tag) {
			case 0:
				if (HID_ITEM_AVAILABLE()) {
					HID_READ_UNSIGNED(value);
					global.usagePage = value;
				}
				break;
			case 1:
				if (HID_ITEM_AVAILABLE())
					HID_READ_SIGNED(global.logicalMinimum);
				break;
			case 2:
				if (HID_ITEM_AVAILABLE())
					HID_READ_SIGNED(global.logicalMaximum);
				break;
			case 3:
				if (HID_ITEM_AVAILABLE())
					HID_READ_SIGNED(global.physicalMinimum);
				break;
			case 4:
				if (HID_ITEM_AVAILABLE())
					HID_READ_SIGNED(global.physicalMaximum);
				break;
			case 7:
				if (HID_ITEM_AVAILABLE())
					HID_READ_UNSIGNED(global.reportSize);
				break;
			case 8:
				if (HID_ITEM_AVAILABLE()) {
					global.reportId = item[1];
					device->usesReportIds = 1;
				}
				break;
			case 9:
				if (HID_ITEM_AVAILABLE())
					HID_READ_UNSIGNED(global.reportCount);
				break;
			default:
				break;
			}
		} else if (itemType == 2 && HID_ITEM_AVAILABLE()) {
			switch (tag) {
			case 0:
				for (index = 0; index < 32; ++index)
					if (local.usages[index] == 0)
						break;
				if (index < 32) {
					HID_READ_UNSIGNED(value);
					local.usages[index] = value;
				}
				break;
			case 1:
				HID_READ_UNSIGNED(value);
				local.usageMinimum = value;
				break;
			case 2:
				HID_READ_UNSIGNED(value);
				local.usageMaximum = value;
				break;
			default: break;
			}
		}
		item = (uint8_t *)((uintptr_t)item + itemSize);
next_item:
		;
	} while ((uintptr_t)item >= (uintptr_t)descriptor &&
		 (uintptr_t)item < (uintptr_t)descriptor + descriptorLength && item != NULL);
finish_descriptor:
	hidDevices[slot] = device;
	if (device->transportType == 2) {
		for (index = 0; index < 2; ++index) {
			request = hidAllocateMemory(sizeof(*request));
			if (request == NULL) {
				device->readRequests[index] = NULL;
				continue;
			}
			memset(request, 0, sizeof(*request));
			request->type = 0;
			request->reportId = 0;
			request->buffer = hidAllocateMemory(58);
			if (request->buffer == NULL) {
				hidFreeMemory(request);
				device->readRequests[index] = NULL;
				continue;
			}
			memset(request->buffer, 0, 58);
			device->readRequests[index] = request;
			request->length = 58;
			request->argument = (uint32_t)request;
			SceBtForDriver_F9DCEC77((uint32_t)device->deviceAddress,
					   (uint32_t)(device->deviceAddress >> 32), request);
		}
	}
	return;
free_device:
	hidFreeMemory(device);
}

#undef HID_ITEM_AVAILABLE
#undef HID_READ_SIGNED
#undef HID_READ_UNSIGNED

int hidDisconnectBluetoothDevice(uint64_t address)
{
	HidDevice *device;
	int slot;
	int index;
	int result;
	SceBtHidRequest *request;

	/* The original checks each fixed slot before entering shared cleanup. */
#define HID_FIND_BLUETOOTH_SLOT(number) \
	do { \
		device = hidDevices[number]; \
		if (device != NULL && device->transportType == 2 && \
		    device->deviceAddress == address) { \
			slot = number; \
			goto found; \
		} \
	} while (0)
	HID_FIND_BLUETOOTH_SLOT(0);
	HID_FIND_BLUETOOTH_SLOT(1);
	HID_FIND_BLUETOOTH_SLOT(2);
	HID_FIND_BLUETOOTH_SLOT(3);
	HID_FIND_BLUETOOTH_SLOT(4);
	HID_FIND_BLUETOOTH_SLOT(5);
	HID_FIND_BLUETOOTH_SLOT(6);
	HID_FIND_BLUETOOTH_SLOT(7);
#undef HID_FIND_BLUETOOTH_SLOT
	return (int)address;
found:
	for (index = 0; index < 8; index++) {
		HidCollection *collection = device->collections[index];

		if (collection == NULL)
			break;
		hidReleaseCollection(collection, 0);
		device->collections[index] = NULL;
	}
	if (device->transportType == 2) {
		request = device->readRequests[0];
		if (request != NULL) {
			hidFreeMemory(request->buffer);
			request->buffer = NULL;
			hidFreeMemory(request);
		}
		request = device->readRequests[1];
		device->readRequests[0] = NULL;
		if (request != NULL) {
			hidFreeMemory(request->buffer);
			request->buffer = NULL;
			hidFreeMemory(request);
		}
		device->readRequests[1] = NULL;
	}
	result = hidFreeMemory(device);
	hidDevices[slot] = NULL;
	return result;
}

void hidReleaseBluetoothDevices(void)
{
	HidDevice **slot;
	HidDevice *device;
	SceBtHidRequest *request;
	int index;

	slot = hidDevices;
	do {
		device = *slot++;
		if (device == NULL || device->transportType != 2)
			continue;
		for (index = 0; index < 8; index++) {
			HidCollection *collection = device->collections[index];

			if (collection == NULL)
				break;
			hidReleaseCollection(collection, 0);
			device->collections[index] = NULL;
		}
		if (device->transportType == 2) {
			request = device->readRequests[0];
			if (request != NULL) {
				hidFreeMemory(request->buffer);
				request->buffer = NULL;
				hidFreeMemory(request);
			}
			request = device->readRequests[1];
			device->readRequests[0] = NULL;
			if (request != NULL) {
				hidFreeMemory(request->buffer);
				request->buffer = NULL;
				hidFreeMemory(request);
			}
			device->readRequests[1] = NULL;
		}
		hidFreeMemory(device);
		slot[-1] = NULL;
	} while (slot != hidDevices + 8);
}

void hidReceiveBluetoothReport(uint64_t address, int status,
			       SceBtHidRequest *request, uint64_t timestamp)
{
	HidDevice *device;
	HidCollection *collection;
	uint8_t *buffer;
	SceSize length = request != NULL ? request->length : 0;
	int slot;

	if (status != 0)
		return;
#define HID_FIND_INPUT_DEVICE(number) \
	do { \
		device = hidDevices[number]; \
		if (device != NULL && device->transportType == 2 && \
		    device->deviceAddress == address) \
			goto found; \
	} while (0)
	HID_FIND_INPUT_DEVICE(0);
	HID_FIND_INPUT_DEVICE(1);
	HID_FIND_INPUT_DEVICE(2);
	HID_FIND_INPUT_DEVICE(3);
	HID_FIND_INPUT_DEVICE(4);
	HID_FIND_INPUT_DEVICE(5);
	HID_FIND_INPUT_DEVICE(6);
	HID_FIND_INPUT_DEVICE(7);
#undef HID_FIND_INPUT_DEVICE
	return;
found:
	buffer = request->buffer;
	collection = device->collections[0];
	if (device->usesReportIds == 0)
		goto process;
	if (collection == NULL)
		goto submit;
#define HID_FIND_REPORT_COLLECTION(number) \
	do { \
		collection = device->collections[number]; \
		if (collection == NULL) \
			goto queue; \
		if (*buffer != 0 && hidCollectionHasReportId(collection, *buffer)) { \
			slot = number; \
			goto selected; \
		} \
	} while (0)
	HID_FIND_REPORT_COLLECTION(0);
	HID_FIND_REPORT_COLLECTION(1);
	HID_FIND_REPORT_COLLECTION(2);
	HID_FIND_REPORT_COLLECTION(3);
	HID_FIND_REPORT_COLLECTION(4);
	HID_FIND_REPORT_COLLECTION(5);
	HID_FIND_REPORT_COLLECTION(6);
	HID_FIND_REPORT_COLLECTION(7);
#undef HID_FIND_REPORT_COLLECTION
	goto queue;
selected:
	collection = device->collections[slot];
process:
	if (collection != NULL) {
		if (device->usesReportIds != 0) {
			if (*buffer != 0) {
				hidProcessReport(collection, *buffer, buffer + 1,
						 length - 1, timestamp);
				buffer = request->buffer;
				goto submit;
			}
		} else {
			hidProcessReport(collection, 0, buffer, length, timestamp);
		}
	}
queue:
	buffer = request->buffer;
submit:
	request->length = 58;
	memset(buffer, 0, 58);
	SceBtForDriver_F9DCEC77((uint32_t)address, (uint32_t)(address >> 32), request);
}

int hidCompleteBluetoothWrite(uint64_t address, int status,
			      SceBtHidRequest *completedRequest)
{
	HidDevice *device;
	SceBtHidRequest *request;
	int result = (int)address;

#define HID_FIND_WRITE_DEVICE(number) \
	do { \
		device = hidDevices[number]; \
		if (device != NULL && device->transportType == 2 && \
		    device->deviceAddress == address) \
			goto found; \
	} while (0)
	HID_FIND_WRITE_DEVICE(0);
	HID_FIND_WRITE_DEVICE(1);
	HID_FIND_WRITE_DEVICE(2);
	HID_FIND_WRITE_DEVICE(3);
	HID_FIND_WRITE_DEVICE(4);
	HID_FIND_WRITE_DEVICE(5);
	HID_FIND_WRITE_DEVICE(6);
	HID_FIND_WRITE_DEVICE(7);
#undef HID_FIND_WRITE_DEVICE
	return result;
found:
	request = device->writeRequest;
	if (request != NULL) {
		hidFreeMemory(request->buffer);
		request->buffer = NULL;
		result = hidFreeMemory(request);
	}
	request = device->pendingWriteRequest;
	device->writeRequest = NULL;
	if (request != NULL) {
		result = SceBtForDriver_F9DCEC77((uint32_t)address, (uint32_t)(address >> 32),
					  request);
		if (result != 0) {
			request = device->pendingWriteRequest;
			if (request != NULL) {
				hidFreeMemory(request->buffer);
				request->buffer = NULL;
				result = hidFreeMemory(request);
				device->pendingWriteRequest = NULL;
				return result;
			}
		} else {
			device->writeRequest = device->pendingWriteRequest;
		}
		device->pendingWriteRequest = NULL;
	}
	return result;
}

uintptr_t hidWriteBluetoothReport(HidDevice *device, int reportId,
				 const void *data, SceSize size)
{
	SceBtHidRequest *request;
	SceBtHidRequest *pending;
	uint8_t *buffer;
	SceSize length;
	int prefix;
	uintptr_t result = device->transportType;

	if (result != 2)
		return result;
	prefix = reportId != 0;
	length = prefix + size;
	request = hidAllocateMemory(sizeof(*request));
	if (request == NULL)
		return 0;
	memset(request, 0, sizeof(*request));
	request->reportId = 0;
	request->type = 1;
	buffer = hidAllocateMemory(length);
	request->buffer = buffer;
	if (buffer == NULL)
		return hidFreeMemory(request);
	memset(buffer, 0, length);
	request->length = length;
	request->argument = (uintptr_t)request;
	if (prefix != 0) {
		*buffer = reportId;
		result = (uintptr_t)memcpy(buffer + 1, data, size);
	} else {
		result = (uintptr_t)memcpy(buffer, data, size);
	}
	if (device->writeRequest != NULL) {
		pending = device->pendingWriteRequest;
		if (pending != NULL) {
			hidFreeMemory(pending->buffer);
			pending->buffer = NULL;
			result = hidFreeMemory(pending);
		}
		device->pendingWriteRequest = request;
		return result;
	}
	result = SceBtForDriver_F9DCEC77((uint32_t)device->deviceAddress,
				 (uint32_t)(device->deviceAddress >> 32), request);
	if (result == 0) {
		device->writeRequest = request;
		return result;
	}
	hidFreeMemory(request->buffer);
	request->buffer = NULL;
	return hidFreeMemory(request);
}
