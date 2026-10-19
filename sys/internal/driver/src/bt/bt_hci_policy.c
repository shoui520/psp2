/* Controller device policy; original source filename unavailable. */
#include "bt_internal.h"

int BtConfigureAutoAcceptDevices(BtStack *stack)
{
	BtDevice *device = stack->devices;
	BtDevice *end = stack->devices + 32;
	unsigned int flags, identifier;

	if (stack->mode & 0x10000) {
		do {
			flags = device->flags & ~0x4000u;
			identifier = device->vendorId |
				     ((unsigned int)device->productId << 16);
			device->flags = flags;
			if (identifier == 0x0306054c ||
			    identifier == 0x0268054c ||
			    identifier == 0x03d5054c ||
			    identifier == 0x05c4054c) {
				flags |= 0x4000;
				device->flags = flags;
			}
			if ((flags & 0xd000) == 0x5000)
				goto accept;
		} while (++device < end);
	} else if (stack->mode & 0x800) {
		do {
			flags = device->flags | 0x4000;
			device->flags = flags;
			if ((flags & 0xd000) == 0x5000)
				goto accept;
		} while (++device < end);
	} else {
		do {
			device->flags &= ~0x4000u;
		} while (++device < end);
	}
	stack->state = 72;
	return BtSendHciCommand(stack, 0x0c1c, "22", 360, 18);

accept:
	device->flags = flags | 0x8000;
	stack->autoAcceptDeviceCount++;
	stack->state = 71;
	BtSendHciCommand(stack, 0x0c05, "1161", 2, 2,
			 device->deviceAddress, 1);
	return 0;
}
