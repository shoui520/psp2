/* Registered-device queries; original source filename unavailable. */
#include <string.h>
#include <intrmgr/intrmgr.h>
#include <bt/bt_registered.h>
#include "bt_internal.h"
#include "bt_transport.h"

extern BtRegisteredOutput gBtRegisteredInfo;

int GetRegisteredInfo(uint64_t deviceAddress, void *buffer, int maxCount,
		      int userMode)
{
	SceBtRegisteredInfo *output = buffer;
	BtDevice *device;
	int count;
	int result;

	if (!sceKernelIsIntrContext()) {
		result = BtLockTransport();
		if (result < 0)
			return result;
	}
	count = gBtStack.deviceRegistryState;
	if (!count) {
		count = (int)0x802f0101;
	} else if (count > 0) {
		device = gBtStack.deviceHead;
		count = 0;
		while (device && count < maxCount) {
			if ((device->flags & 0x1000) &&
			    (!deviceAddress || device->deviceAddress == deviceAddress)) {
				if (output) {
					uint32_t profiles;
					uint32_t flags;
					uint64_t address;

					memset(&gBtRegisteredInfo, 0, sizeof(gBtRegisteredInfo));
					profiles = device->profiles;
					address = device->deviceAddress;
					flags = device->flags & 1;
					gBtRegisteredInfo.info.bt_class = device->deviceClass;
					gBtRegisteredInfo.info.profiles = profiles & 0x203fffff;
					gBtRegisteredInfo.info.flags = flags;
					gBtRegisteredInfo.deviceAddress = address;
					if ((profiles & 0x20) && (device->avrcpFeatures & 2)) {
						gBtRegisteredInfo.info.flags = flags | 0x100;
						if (device->avrcpVersion >= 0x104)
							gBtRegisteredInfo.info.flags = flags | 0x300;
					}
					if (device->eirIdentity.fields.vendorIdSource == 2) {
						gBtRegisteredInfo.info.vid = device->vendorId;
						gBtRegisteredInfo.info.pid = device->productId;
						gBtRegisteredInfo.info.version = device->version;
					}
					gBtRegisteredInfo.info.primary = device->primary;
					memcpy(gBtRegisteredInfo.info.name, device->name, 121);
					result = BtCopyOut(&gBtRegisteredInfo, output,
							  sizeof(gBtRegisteredInfo), userMode);
					if (result < 0) {
						count = result;
						break;
					}
					++output;
				}
				++count;
			}
			device = device->next;
		}
	}
	if (sceKernelIsIntrContext())
		return count;
	return BtUnlockTransport(count);
}

uint64_t BtRecordInquiryProgress(BtStack *stack)
{
	uint64_t time = BtGetSystemTimeWide();

	stack->inquiryProgressTime = time;
	return time;
}
