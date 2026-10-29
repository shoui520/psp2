/* Registered devices; the original source filename is unavailable. */
#include "bt_internal.h"
#include <string.h>

BtDevice *BtGetOrCreateDevice(BtStack *stack, uint64_t deviceAddress,
			    unsigned int create);

BtDevice gBtUncachedInquiryDevice;
BtDeviceHistory gBtDeviceHistory[8];

int BtFlushRegisteredDevices(BtStack *stack)
{
	return BtWriteRegisteredDevices(stack, 1);
}

int BtCountRegisteredDevices(BtStack *stack)
{
	BtDevice *device;
	int count = 0;

	for (device = stack->devices; device < stack->devices + 32; device++) {
		if (device->deviceAddress && (device->flags & 0x1000))
			count++;
	}
	return count;
}

static inline void BtCacheDeviceHistory(uint64_t address, uint32_t deviceClass,
				      const uint8_t *name, uint16_t vendorId,
				      uint16_t productId)
{
	BtDeviceHistory *history, *selected = NULL;
	uint64_t oldestTime = 0;

	for (history = gBtDeviceHistory; history < gBtDeviceHistory + 8;
	     history++) {
		if (!history->deviceAddress || history->deviceAddress == address) {
			selected = history;
			break;
		}
		if (!oldestTime || history->lastSeen < oldestTime) {
			oldestTime = history->lastSeen;
			selected = history;
		}
	}
	if (selected) {
		selected->deviceAddress = address;
		selected->lastSeen = BtGetSystemTimeWide();
		selected->deviceClass = deviceClass;
		BtUpdateUtf8String(selected->name, name, 120);
		selected->vendorId = vendorId;
		selected->productId = productId;
	}
}

BtDevice *BtGetOrCreateDevice(BtStack *stack, uint64_t deviceAddress,
			    unsigned int create)
{
	BtDevice *device, *selected;
	uint64_t oldestTime;
	SceInt64 now;
	uint32_t generation;

	for (device = stack->devices; device < stack->devices + 32; device++) {
		if (device->deviceAddress == deviceAddress) {
			device->lastSeen = BtGetSystemTimeWide();
			return device;
		}
	}
	if (!create)
		return NULL;
	for (device = stack->devices; device < stack->devices + 32; device++) {
		if (!device->deviceAddress)
			goto initialize;
	}
	selected = NULL;
	oldestTime = 0;
	for (device = stack->devices; device < stack->devices + 32; device++) {
		if (!(device->flags & 0x1000) &&
		    (!oldestTime || device->lastSeen < oldestTime)) {
			oldestTime = device->lastSeen;
			selected = device;
		}
	}
	device = selected;
	if (!device)
		return NULL;
	if (device->deviceAddress && device->name[0])
		BtCacheDeviceHistory(device->deviceAddress, device->deviceClass,
				     (const uint8_t *)device->name,
				     device->vendorId, device->productId);
initialize:
	memset(device, 0, sizeof(*device));
	now = BtGetSystemTimeWide();
	generation = device->generation;
	device->lastSeen = now;
	stack->inquiryProgressTime = now;
	device->deviceAddress = deviceAddress;
	device->generation = generation + 1;
	device->remoteNameStatus = 0xd0;
	return device;
}

BtDevice *BtRestoreDeviceFromHistory(BtStack *stack, uint64_t deviceAddress)
{
	BtDeviceHistory *history = gBtDeviceHistory;
	BtDevice *device;
	uint32_t deviceClass;

	while (history->deviceAddress != deviceAddress) {
		if (++history >= gBtDeviceHistory + 8)
			return NULL;
	}
	history->lastSeen = BtGetSystemTimeWide();
	deviceClass = history->deviceClass;
	device = BtGetOrCreateDevice(stack, deviceAddress, 1);
	if (device) {
		device->deviceClass = deviceClass;
		device->pageScanRepetitionMode = 2;
		device->clockOffset = 0;
		memcpy(device->name, history->name, 121);
	}
	return device;
}

int BtUpdateDeviceClass(BtDevice *device, uint32_t deviceClass)
{
	int result = memcmp(&device->deviceClass, &deviceClass,
			    sizeof(deviceClass));

	if (result) {
		device->deviceClass = deviceClass;
		device->generation++;
		result = deviceClass;
	}
	return result;
}

int BtProcessInquiryResult(BtStack *stack, uint64_t deviceAddress,
			  uint8_t pageScanRepetitionMode, uint32_t deviceClass,
			  uint16_t clockOffset, int8_t rssi,
			  const uint8_t *extendedInquiryResponse)
{
	BtDevice *device = BtGetOrCreateDevice(stack, deviceAddress, 1);
	const uint8_t *cursor = extendedInquiryResponse;
	int nameFound = 0;
	uint64_t now;

	if (!device) {
		device = &gBtUncachedInquiryDevice;
		memset(device, 0, sizeof(*device));
		device->deviceAddress = deviceAddress;
		device->remoteNameStatus = 0xd0;
	}
	if (memcmp(&device->deviceClass, &deviceClass, sizeof(deviceClass))) {
		device->deviceClass = deviceClass;
		device->generation++;
	}
	device->clockOffset = clockOffset;
	if (device->pageScanRepetitionMode != pageScanRepetitionMode) {
		device->pageScanRepetitionMode = pageScanRepetitionMode;
		device->generation++;
	}
	if (cursor) {
		while (cursor < extendedInquiryResponse + 240) {
			const BtEirRecord *record = (const BtEirRecord *)cursor;

			if (!record->length)
				break;
			if ((unsigned int)(record->type - 8) <= 1) {
				int length = record->length - 1;

				if (length >= 120)
					length = 120;
				if (BtUpdateUtf8String(device->name, record->payload,
						      length) > 0)
					device->generation++;
				nameFound = 1;
			} else if (record->length == 9 && record->type == 16) {
				const BtEirDeviceId *identity = (const BtEirDeviceId *)cursor;
				uint16_t source = BtReadLe16(identity->sourceLe);

				if (source == 2) {
					uint16_t vendor = BtReadLe16(identity->vendorLe);
					uint16_t product = BtReadLe16(identity->productLe);
					uint16_t version = BtReadLe16(identity->versionLe);

					if (product == 0x9cc && vendor == 0x54c)
						product = 0x5c4;
					if (memcmp(&device->eirIdentity, &source, 2)) {
						memcpy(&device->eirIdentity, &source, 2);
						device->generation++;
					}
					if (memcmp(&device->vendorId, &vendor, 2)) {
						device->generation++;
						device->vendorId = vendor;
					}
					if (memcmp(&device->productId, &product, 2)) {
						device->generation++;
						device->productId = product;
					}
					if (memcmp(&device->version, &version, 2)) {
						device->generation++;
						device->version = version;
					}
				}
			}
			cursor += record->length + 1;
		}
	}
	if (nameFound) {
		if (device == &gBtUncachedInquiryDevice)
			BtCacheDeviceHistory(device->deviceAddress, device->deviceClass,
					     (const uint8_t *)device->name,
					     device->vendorId, device->productId);
	} else {
		unsigned int status;
		uint64_t timeout;

		now = BtGetSystemTimeWide();
		status = device->remoteNameStatus;
		if (status == 0xf0 || status == 4) {
			timeout = status == 0xf0 ? 10000000 : 30000000;
			if (now >= timeout + (uint64_t)stack->inquiryProgressTime) {
				device->remoteNameStatus = 0xe0;
				stack->inquiryProgressTime = now;
				return 1;
			}
		} else if (status) {
			stack->inquiryProgressTime = now;
			return 1;
		}
	}
	if (device->flags & 0x1000) {
		if (device->generation)
			BtNotifyEvent(7, 0, 0, 0);
	} else {
		BtNotifyEvent(1, (uint8_t)rssi, deviceClass, deviceAddress);
	}
	return 0;
}

void BtProcessRemoteNameResponse(BtStack *stack, int status,
				 uint64_t deviceAddress, uint32_t deviceClass,
				 int8_t rssi, const uint8_t *name)
{
	BtDevice *device;
	BtDeviceHistory *history;
	uint16_t vendorId = 0, productId = 0;

	for (device = stack->devices; device < stack->devices + 32; device++) {
		if (device->deviceAddress == deviceAddress) {
			device->lastSeen = BtGetSystemTimeWide();
			if (device->remoteNameStatus != status) {
				device->remoteNameStatus = status;
				device->generation++;
			}
			if (status)
				return;
			if (BtUpdateUtf8String(device->name, name, 120) > 0)
				device->generation++;
			if (device->flags & 0x1000) {
				if (device->generation)
					BtNotifyEvent(7, 0, 0, 0);
				return;
			}
			goto notify;
		}
	}
	if (status)
		return;
	for (history = gBtDeviceHistory; history < gBtDeviceHistory + 8;
	     history++) {
		if (history->deviceAddress == deviceAddress) {
			vendorId = history->vendorId;
			productId = history->productId;
			break;
		}
	}
	BtCacheDeviceHistory(deviceAddress, deviceClass, name, vendorId, productId);
notify:
	BtNotifyEvent(1, (uint8_t)rssi, deviceClass, deviceAddress);
}

int BtRegisterDevice(BtStack *stack, uint64_t deviceAddress)
{
	BtDevice *device, *tail;
	uint32_t sequence;

	for (device = stack->devices; device < stack->devices + 32; device++) {
		if (device->deviceAddress == deviceAddress)
			goto found;
	}
	return -1;
found:
	device->lastSeen = BtGetSystemTimeWide();
	if (device->flags & 0x1000)
		return 0;
	tail = stack->deviceTail;
	sequence = stack->registrationSequence + 1;
	device->flags |= 0x1000;
	device->generation++;
	stack->registrationSequence = sequence;
	device->registrationSequence = sequence;
	device->prev = tail;
	if (tail)
		tail->next = device;
	else
		stack->deviceHead = device;
	device->next = NULL;
	stack->deviceTail = device;
	BtWriteRegisteredDevices(stack, 1);
	return 0;
}

int BtFlushConnectionDevice(BtConnection *connection)
{
	BtFlushRegisteredDevices(connection->stack);
	return 0;
}

void BtSetDeviceAuthenticationFailed(BtStack *stack, BtConnection *connection,
				   int failed)
{
	BtDevice *device = connection->device;
	uint32_t flags;

	if (!device)
		return;
	flags = device->flags;
	if (!(flags & 0x20000)) {
		if (!failed)
			return;
		device->flags = flags | 0x20000;
		device->generation++;
	} else {
		if (failed)
			return;
		device->flags = flags & ~0x20000;
		device->generation++;
	}
	BtWriteRegisteredDevices(stack, 0);
}

static inline void BtCopyLinkKey(uint8_t *destination, const uint8_t *source)
{
	uint32_t first, second, third, fourth;

	__builtin_memcpy(&first, source, 4);
	__builtin_memcpy(&second, source + 4, 4);
	__builtin_memcpy(&third, source + 8, 4);
	__builtin_memcpy(&fourth, source + 12, 4);
	__builtin_memcpy(destination, &first, 4);
	__builtin_memcpy(destination + 4, &second, 4);
	__builtin_memcpy(destination + 8, &third, 4);
	__builtin_memcpy(destination + 12, &fourth, 4);
}

int BtReadDeviceLinkKey(BtStack *stack, uint64_t deviceAddress, uint8_t *key)
{
	BtDevice *device;

	for (device = stack->devices; device < stack->devices + 32; device++) {
		if (device->deviceAddress == deviceAddress)
			goto found;
	}
	return -1;
found:
	device->lastSeen = BtGetSystemTimeWide();
	if (!(device->flags & 0x2000))
		return -1;
	BtCopyLinkKey(key, device->linkKey);
	return 0;
}

int BtWriteDeviceLinkKey(BtStack *stack, uint64_t deviceAddress, const uint8_t *key)
{
	BtDevice *device;

	for (device = stack->devices; device < stack->devices + 32; device++) {
		if (device->deviceAddress == deviceAddress)
			goto found;
	}
	return -1;
found:
	device->lastSeen = BtGetSystemTimeWide();
	BtCopyLinkKey(device->linkKey, key);
	device->flags |= 0x2000;
	device->generation++;
	BtWriteRegisteredDevices(stack, 1);
	return 0;
}

int BtDeleteRegisteredDevice(BtStack *stack, uint64_t deviceAddress)
{
	BtDevice *device;

	for (device = stack->devices; device < stack->devices + 32; device++) {
		if (device->deviceAddress == deviceAddress)
			goto found;
	}
	return (int)0x802f0102;
found:
	device->lastSeen = BtGetSystemTimeWide();
	device->flags &= ~0x3000;
	device->generation++;
	device->profiles = 0;
	if (device->next)
		device->next->prev = device->prev;
	else
		stack->deviceTail = device->prev;
	if (device->prev)
		device->prev->next = device->next;
	else
		stack->deviceHead = device->next;
	device->next = NULL;
	device->prev = NULL;
	BtWriteRegisteredDevices(stack, 1);
	return 0;
}

int BtSetDeviceHidDescriptor(BtConnection *connection,
			     const uint8_t *descriptor, unsigned int length)
{
	uint16_t descriptorLength = length;
	BtDevice *device;

	if (!descriptor || !descriptor[0] ||
	    (unsigned int)descriptorLength - 1 >= 512)
		return (int)0x802f0105;
	device = connection->device;
	if (!device)
		return (int)0x802f0106;
	if (memcmp(device->hidDescriptor, descriptor, descriptorLength)) {
		memcpy(device->hidDescriptor, descriptor, descriptorLength);
		device->generation++;
	}
	if (memcmp(&device->hidDescriptorLength, &descriptorLength, 2)) {
		device->generation++;
		device->hidDescriptorLength = descriptorLength;
	}
	return 0;
}

int BtGetHidReportDescriptor(BtStack *stack, uint64_t deviceAddress,
			     void *buffer, int capacity)
{
	BtDevice *device;

	for (device = stack->devices; device < stack->devices + 32; device++) {
		if (device->deviceAddress == deviceAddress)
			goto found;
	}
	return (int)0x802f0107;
found:
	device->lastSeen = BtGetSystemTimeWide();
	if (device->hidDescriptorLength > capacity)
		return (int)0x802f0108;
	memcpy(buffer, device->hidDescriptor, device->hidDescriptorLength);
	return device->hidDescriptorLength;
}

const char *BtLookupDeviceName(BtStack *stack, uint64_t deviceAddress)
{
	BtDevice *device;
	BtDeviceHistory *history;

	for (device = stack->devices; device < stack->devices + 32; device++) {
		if (device->deviceAddress == deviceAddress) {
			device->lastSeen = BtGetSystemTimeWide();
			return device->name;
		}
	}
	for (history = gBtDeviceHistory; history < gBtDeviceHistory + 8; history++) {
		if (history->deviceAddress == deviceAddress) {
			history->lastSeen = BtGetSystemTimeWide();
			return history->name;
		}
	}
	return NULL;
}

BtStack *BtResetAutoAcceptDevices(BtStack *stack)
{
	BtDevice *device;

	for (device = stack->devices; device < stack->devices + 32; device++)
		device->flags &= ~0x8000;
	stack->autoAcceptDeviceCount = 0;
	return stack;
}

int BtGetDeviceId(BtStack *stack, uint64_t deviceAddress, void *buffer,
		  int userMode)
{
	uint16_t identity[2] = { 0, 0 };
	BtDevice *device;
	BtDeviceHistory *history;

	for (device = stack->devices; device < stack->devices + 32; device++) {
		if (device->deviceAddress == deviceAddress) {
			device->lastSeen = BtGetSystemTimeWide();
			identity[0] = device->vendorId;
			identity[1] = device->productId;
			goto copy;
		}
	}
	for (history = gBtDeviceHistory; history < gBtDeviceHistory + 8; history++) {
		if (history->deviceAddress == deviceAddress) {
			history->lastSeen = BtGetSystemTimeWide();
			identity[0] = history->vendorId;
			identity[1] = history->productId;
			goto copy;
		}
	}
	return (int)0x802f5001;
copy:
	return BtCopyOut(identity, buffer, sizeof(identity), userMode);
}
