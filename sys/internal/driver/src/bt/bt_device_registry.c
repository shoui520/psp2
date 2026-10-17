/* Registered-device persistence; original source filename unavailable. */
#include <stdio.h>
#include <string.h>
#include <regmgr/regmgr.h>
#include "bt_internal.h"

uintptr_t BtNotifyEvent(unsigned int eventId, unsigned int status,
			unsigned int argument, uint64_t deviceAddress);

int BtWriteRegisteredDevices(BtStack *stack, int notifyChange)
{
	BtDevice *device = stack->devices;
	BtDevice *end = stack->devices + 32;
	BtDeviceRegistryPair *pair = &stack->deviceRegistry;
	char category[64];
	int index = 0, changed = 0;

	do {
		if (device[0].generation || device[1].generation) {
			device[0].flags &= 0x233c3;
			device[1].flags &= 0x233c3;
			snprintf(category, sizeof(category), "/CONFIG/BT/%02d", index);
			memset(pair, 0, sizeof(*pair));
			memcpy(pair->entries[0].serializedDevice, &device[0],
			       sizeof(pair->entries[0].serializedDevice));
			memcpy(pair->entries[1].serializedDevice, &device[1],
			       sizeof(pair->entries[1].serializedDevice));
			if (sceRegMgrSetKeyBin(category, "info", pair,
					       sizeof(*pair)) < 0)
				return 0x802f0103;
			changed++;
			device[1].generation = 0;
			device[0].generation = 0;
		}
		device += 2;
		index++;
	} while (device != end);
	if (changed && notifyChange)
		BtNotifyEvent(7, 0, 0, 0);
	return 0;
}

int BtReadRegisteredDevices(BtStack *stack)
{
	BtDeviceRegistryPair *pair = &stack->deviceRegistry;
	BtDevice *device = stack->devices;
	BtDevice *end = stack->devices + 32;
	BtDevice *next;
	unsigned int flags, sequence, profiles;
	char category[64];
	int index = 0, value, result;

	memset(stack->devices, 0, sizeof(stack->devices));
	do {
		snprintf(category, sizeof(category), "/CONFIG/BT/%02d", index);
		result = sceRegMgrGetKeyBin(category, "info", pair, sizeof(*pair));
		if (result < 0)
			goto failed;
		index++;
		memcpy(&device[0], pair->entries[0].serializedDevice,
		       sizeof(pair->entries[0].serializedDevice));
		memcpy(&device[1], pair->entries[1].serializedDevice,
		       sizeof(pair->entries[1].serializedDevice));
		device += 2;
	} while (index != 16);
	flags = stack->mode | 1;
	stack->mode = flags;
	if (!(flags & 0x10000)) {
		result = sceRegMgrGetKeyInt("/CONFIG/BT", "bt_enable", &value);
		if (result < 0)
			goto failed;
		if (!value)
			stack->mode &= ~1U;
		stack->mode &= ~2U;
		result = sceRegMgrGetKeyInt("/CONFIG/SYSTEM", "flight_mode", &value);
		if (result < 0)
			goto failed;
		if (value)
			stack->mode |= 2;
		stack->mode &= ~0x80U;
		result = sceRegMgrGetKeyInt("/CONFIG/BT", "bt_config", &value);
		if (result < 0)
			goto failed;
		flags = stack->mode;
		if (value & 1) {
			flags = (flags & ~1U) | 0x80;
			stack->mode = flags;
		}
	}
	stack->mode = (flags & ~8U) | ((flags & 1) ? 8 : 0);
	sceRegMgrGetKeyInt("/CONFIG/BT", "bt_mic", &value);
	stack->deviceTail = NULL;
	stack->registrationSequence = 0;
	stack->deviceHead = NULL;
	device = stack->devices;
	do {
		if (device->deviceAddress && !device->remoteNameStatus)
			device->remoteNameStatus = 0xf0;
		flags = device->flags;
		device->clockOffset &= 0x7fff;
		device->flags = flags & 0x233c3;
		if (flags & 0x1000) {
			sequence = device->registrationSequence;
			next = stack->deviceHead;
			if (stack->registrationSequence < sequence)
				stack->registrationSequence = sequence;
			while (next && sequence >= next->registrationSequence)
				next = next->next;
			if (next) {
				device->prev = next->prev;
				if (next->prev)
					next->prev->next = device;
				else
					stack->deviceHead = device;
				device->next = next;
				next->prev = device;
			} else {
				device->prev = stack->deviceTail;
				if (stack->deviceTail)
					stack->deviceTail->next = device;
				else
					stack->deviceHead = device;
				device->next = NULL;
				stack->deviceTail = device;
			}
		}
		device->lastSeen = BtGetSystemTimeWide();
		profiles = device->profiles;
		if (profiles & 0x10000)
			profiles |= 0x20000000;
		else
			profiles &= ~0x20000000U;
		device->profiles = profiles;
	} while (++device < end);
	result = 0;
	stack->deviceRegistryState = 1;
	goto notify;

failed:
	stack->deviceRegistryState = result;
notify:
	BtNotifyEvent(7, 0, 0, 0);
	return result;
}
