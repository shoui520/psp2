/* RTL8152 USB Ethernet driver. Private function names are unknown. */
#include "rtl_internal.h"
#include "phy_patch.h"
#include <kernel/error.h>
#include <sysmem/sysevent_ids.h>

/* Sony's 4.00 build is fortified */
#if FW_SINCE(0x04000000)
#include <sysmem/fortify.h>
#endif

/* power.c's definition takes a third argument, passed back to the callback; Sony's USB
 * Ethernet drivers call it with two and leave that one unset */
int ScePowerForDriver_B24263F0(int kind, void (*handler)(int event, int value));

static unsigned int readValue __attribute__((aligned(64)));
static unsigned int writeValue __attribute__((aligned(64)));
static unsigned int wakeValue __attribute__((aligned(64)));
static unsigned char plaReadData[64] __attribute__((aligned(64)));
static unsigned char plaWriteData[512] __attribute__((aligned(64)));
static unsigned char usbReadData[32] __attribute__((aligned(64)));
static unsigned char usbWriteData[32] __attribute__((aligned(64)));
static SceUsbdDeviceRequest readRequest __attribute__((aligned(64)));
static SceUsbdDeviceRequest writeRequest __attribute__((aligned(64)));
static SceUsbdDeviceRequest wakeRequest __attribute__((aligned(64)));
static SceUsbdDeviceRequest plaReadRequest __attribute__((aligned(64)));
static SceUsbdDeviceRequest plaWriteRequest __attribute__((aligned(64)));
static SceUsbdDeviceRequest usbReadRequest __attribute__((aligned(64)));
static SceUsbdDeviceRequest usbWriteRequest __attribute__((aligned(64)));

void rtlControlDone(int result, int count, void *context)
{
	struct RtlAdapter *a = context;

	(void)count;
	sceKernelSetEventFlag(a->controlEvent, result < 0 ? 1 : 2);
}

/* name unknown */
static inline __attribute__((always_inline)) int transfer(struct RtlAdapter *a, SceUsbdDeviceRequest *request, void *data)
{
	unsigned int bits;
	int ret;

	ret = SceUsbdForDriver_2E05660F(a->controlPipe, request, data, rtlControlDone, a);
	if (ret < 0)
		return ret;
	ret = sceKernelWaitEventFlag(a->controlEvent, 3,
				      SCE_KERNEL_EVF_WAITMODE_OR |
				      SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &bits, NULL);
	if (ret >= 0 && (bits & 1))
		return RTL_ERROR(4);
	return ret;
}

/* name unknown */
static void request(SceUsbdDeviceRequest *r, int direction, unsigned short reg,
		    unsigned short mask, unsigned short length)
{
	r->bmRequestType = direction;
	r->bRequest = 5;
	r->wValue = reg;
	r->wIndex = mask;
	r->wLength = length;
}

static __attribute__((noinline, noclone)) int rtlWakeFeature(struct RtlAdapter *a)
{
	wakeValue = 0;
	wakeRequest.bmRequestType = 0;
	wakeRequest.bRequest = 3;
	wakeRequest.wValue = 1;
	wakeRequest.wIndex = 0;
	wakeRequest.wLength = 0;
	return transfer(a, &wakeRequest, &wakeValue);
}

int rtlReadRegister(struct RtlAdapter *a, unsigned short reg, unsigned short mask,
		    unsigned short length, unsigned int *value)
{
	int ret;

	readValue = 0;
	request(&readRequest, 0xc0, reg, mask, length);
	ret = transfer(a, &readRequest, &readValue);
	if (ret >= 0)
		*value = readValue;
	return ret;
}

int rtlWriteRegister(struct RtlAdapter *a, unsigned short reg, unsigned short mask,
		     unsigned short length, unsigned int value)
{
	writeValue = value;
	request(&writeRequest, 0x40, reg, mask, length);
	return transfer(a, &writeRequest, &writeValue);
}

int rtlReadPla(struct RtlAdapter *a, unsigned int reg, unsigned int length, void *data)
{
	unsigned int offset = 0, remaining = length;
	int ret;

	if ((length & 3) || !length || (reg & 3) || reg + length >= 0x10000)
		return RTL_ERROR(1);
	while ((int)remaining > 64) {
		request(&plaReadRequest, 0xc0, reg + offset, 0x100, 64);
		ret = transfer(a, &plaReadRequest, plaReadData);
		if (ret < 0)
			return ret;
		/* Sony uses inline word stores for the complete chunks. */
		for (unsigned int i = 0; i < 64; i += 4)
			*(unsigned int *)((unsigned char *)data + offset + i) =
				*(unsigned int *)(plaReadData + i);
		offset = (offset + 64) & 0xffff;
		remaining = length - offset;
	}
	request(&plaReadRequest, 0xc0, reg + offset, 0x100, remaining);
	ret = transfer(a, &plaReadRequest, plaReadData);
	if (ret >= 0)
		memcpy((unsigned char *)data + offset, plaReadData, remaining & 0xffff);
	return ret;
}

int rtlReadPla8(struct RtlAdapter *a, unsigned int reg, unsigned char *value)
{
	unsigned char data[4];
	int ret = rtlReadPla(a, reg & 0xfffc, 4, data);

	if (ret >= 0)
		*value = data[reg & 3];
	return ret;
}

int rtlReadPla16(struct RtlAdapter *a, unsigned int reg, unsigned short *value)
{
	unsigned char data[8];
	int ret = rtlReadPla(a, reg & 0xfffc, 8, data);

	if (ret >= 0)
		*value = *(unsigned short *)(data + (reg & 3));
	return ret;
}

int rtlReadPla32(struct RtlAdapter *a, unsigned int reg, unsigned int *value)
{
	unsigned char data[8];
	int ret = rtlReadPla(a, reg & 0xfffc, 8, data);

	if (ret >= 0)
		*value = *(unsigned int *)(data + (reg & 3));
	return ret;
}

int rtlWritePla(struct RtlAdapter *a, unsigned int reg, unsigned int mask,
		 unsigned int length, const void *data)
{
	unsigned int offset = 4, remaining;
	int ret;

	if ((length & 3) || !length || (reg & 3) || reg + length >= 0x10000)
		return RTL_ERROR(1);
	*(unsigned int *)plaWriteData = *(const unsigned int *)data;
	request(&plaWriteRequest, 0x40, reg, 0x100 | (mask & 15) * 17, 4);
	ret = transfer(a, &plaWriteRequest, plaWriteData);
	if (ret < 0 || length == 4)
		return ret;
	remaining = length - offset;
	while ((int)remaining > 512) {
		request(&plaWriteRequest, 0x40, reg + offset, 0x1ff, 512);
		memcpy(plaWriteData, (const unsigned char *)data + offset, 512);
		ret = transfer(a, &plaWriteRequest, plaWriteData);
		if (ret < 0)
			return ret;
		offset = (offset + 512) & 0xffff;
		remaining = length - offset;
	}
	request(&plaWriteRequest, 0x40, reg + offset, 0x1ff, remaining);
	memcpy(plaWriteData, (const unsigned char *)data + offset, remaining & 0xffff);
	ret = transfer(a, &plaWriteRequest, plaWriteData);
	if (ret < 0)
		return ret;
	request(&plaWriteRequest, 0x40, reg + offset,
		0x100 | (mask & 0xf0) | ((mask & 0xf0) >> 4), 4);
	/* Sony repeats the first word of the final chunk at this address. */
	*(unsigned int *)plaWriteData =
		*(const unsigned int *)((const unsigned char *)data + offset);
	return transfer(a, &plaWriteRequest, plaWriteData);
}

int rtlWritePla8(struct RtlAdapter *a, unsigned int reg, unsigned char value)
{
	unsigned int data = 0, mask = (0x11 << (reg & 3)) & 15;

	*((unsigned char *)&data + (reg & 3)) = value;
	if ((reg & 0xfffc) == 0xfffc)
		return RTL_ERROR(1);
	*(unsigned int *)plaWriteData = data;
	request(&plaWriteRequest, 0x40, reg & 0xfffc, 0x100 | mask * 17, 4);
	return transfer(a, &plaWriteRequest, plaWriteData);
}

int rtlWritePla16(struct RtlAdapter *a, unsigned int reg, unsigned short value)
{
	unsigned int data[2] = { 0, 0 };
	unsigned int offset = reg & 3, mask, length;

	if (offset == 2) {
		mask = 0xcc;
		length = 4;
	} else if (offset == 3) {
		mask = 0x81;
		length = 8;
	} else {
		mask = offset == 1 ? 0x66 : 0x33;
		length = 4;
	}
	*(unsigned short *)((unsigned char *)data + offset) = value;
	return rtlWritePla(a, reg & 0xfffc, mask, length, data);
}

int rtlWritePla32(struct RtlAdapter *a, unsigned int reg, unsigned int value)
{
	unsigned int data[2] = { 0, 0 };
	unsigned int offset = reg & 3, mask, length;

	switch (offset) {
	case 1:
		mask = 0x1e;
		length = 8;
		break;
	case 2:
		mask = 0x3c;
		length = 8;
		break;
	case 3:
		mask = 0x78;
		length = 8;
		break;
	default:
		mask = 0xff;
		length = 4;
		break;
	}
	*(unsigned int *)((unsigned char *)data + offset) = value;
	return rtlWritePla(a, reg & 0xfffc, mask, length, data);
}

int rtlReadUsb(struct RtlAdapter *a, unsigned short reg, unsigned int length, void *data)
{
	int ret;

	if (length > 32)
		return RTL_ERROR(1);
	request(&usbReadRequest, 0xc0, reg, 0, length);
	ret = transfer(a, &usbReadRequest, usbReadData);
	if (ret >= 0)
		memcpy(data, usbReadData, length);
	return ret;
}

int rtlReadUsb8(struct RtlAdapter *a, unsigned int reg, unsigned char *value)
{
	unsigned int data;
	int ret;

	request(&usbReadRequest, 0xc0, reg & 0xfffc, 0, 4);
	ret = transfer(a, &usbReadRequest, usbReadData);
	if (ret >= 0) {
		data = *(unsigned int *)usbReadData;
		*value = *((unsigned char *)&data + (reg & 3));
	}
	return ret;
}

int rtlWriteUsb(struct RtlAdapter *a, unsigned short reg, unsigned short mask,
		 unsigned int length, const void *data)
{
	if (length > 32)
		return RTL_ERROR(1);
	request(&usbWriteRequest, 0x40, reg, mask, length);
	memcpy(usbWriteData, data, length);
	return transfer(a, &usbWriteRequest, usbWriteData);
}

int rtlWriteUsb8(struct RtlAdapter *a, unsigned int reg, unsigned char value)
{
	unsigned int data = 0;

	*((unsigned char *)&data + (reg & 3)) = value;
	*(unsigned int *)usbWriteData = data;
	request(&usbWriteRequest, 0x40, reg & 0xfffc, 0x11 << (reg & 3), 4);
	return transfer(a, &usbWriteRequest, usbWriteData);
}

int rtlWriteUsb16(struct RtlAdapter *a, unsigned int reg, unsigned short value)
{
	unsigned int data[2] = { 0, 0 };
	unsigned int offset = reg & 3, mask, length;

	if (offset == 2) {
		mask = 0x0c;
		length = 4;
	} else if (offset == 3) {
		mask = 0x81;
		length = 8;
	} else {
		mask = offset == 1 ? 0x66 : 0x33;
		length = 4;
	}

	*(unsigned short *)((unsigned char *)data + offset) = value;
	request(&usbWriteRequest, 0x40, reg & 0xfffc, mask, length);
	memcpy(usbWriteData, data, length);
	return transfer(a, &usbWriteRequest, usbWriteData);
}

int rtlWriteUsb32(struct RtlAdapter *a, unsigned int reg, unsigned int value)
{
	unsigned int data[2] = { 0, 0 };
	unsigned int offset = reg & 3, mask, length;

	switch (offset) {
	case 1:
		mask = 0x1e;
		length = 8;
		break;
	case 2:
		mask = 0x3c;
		length = 8;
		break;
	case 3:
		mask = 0x78;
		length = 8;
		break;
	default:
		mask = 0xff;
		length = 4;
		break;
	}

	*(unsigned int *)((unsigned char *)data + offset) = value;
	request(&usbWriteRequest, 0x40, reg & 0xfffc, mask, length);
	memcpy(usbWriteData, data, length);
	return transfer(a, &usbWriteRequest, usbWriteData);
}


struct RtlAdapter rtlAdapter;
static unsigned int suspendRequested, suspending;
static SceUsbdDriver usbDriver;
static const char usbDriverName[] = "SceUsbEtherRtl";

/* name unknown */
int rtlEnableReceive(struct RtlAdapter *a)
{
	unsigned char byte;
	unsigned int value, control;
	int ret;

	lockFastMutex(&a->mutex);
	ret = rtlReadPla8(a, 0xe813, &byte);
	if (ret < 0)
		goto out;
	byte |= 8;
	ret = rtlWritePla8(a, 0xe813, byte);
	if (ret < 0)
		goto out;
	ret = rtlReadRegister(a, 0xe858, 0x100, 4, &value);
	if (ret < 0)
		goto out;
	value &= ~0x80000;
	ret = rtlWriteRegister(a, 0xe858, 0x10f, 4, value);
	if (ret < 0)
		goto out;
	ret = rtlReadPla32(a, 0xc010, &control);
	if (ret < 0)
		goto out;
	if (a->macControl & 1)
		control |= 11;
	else
		control |= 10;
	ret = rtlWritePla32(a, 0xc010, control);
out:
	sceKernelUnlockFastMutex(&a->mutex);
	return ret;
}

/* name unknown */
static int kickTx(struct NetDriver *driver, int argument)
{
	struct RtlAdapter *a = driver->private;
	int old;

	(void)argument;

	if (!a->active)
		return RTL_ERROR(4);
	sceKernelAtomicAddAndGet32(&a->references, 1);
	old = __atomic_fetch_add(&a->pendingTx, 1, __ATOMIC_SEQ_CST);
	if (!old)
		sceKernelSetEventFlag(a->workerEvent, 8);
	sceKernelAtomicSubAndGet32(&a->references, 1);
	return 0;
}

/* name unknown */
static int start(struct NetDriver *driver, void *memoryBase)
{
	struct RtlAdapter *a = driver->private;

	(void)memoryBase;
	if (!a->active)
		return RTL_ERROR(4);
	sceKernelAtomicAddAndGet32(&a->references, 1);
	sceKernelAtomicSubAndGet32(&a->references, 1);
	return 0;
}

/* name unknown */
static int stop(struct NetDriver *driver, void *memoryBase, int memorySize)
{
	struct RtlAdapter *a = driver->private;

	(void)memoryBase;
	(void)memorySize;
	if (!a->active)
		return RTL_ERROR(4);
	sceKernelAtomicAddAndGet32(&a->references, 1);
	sceKernelAtomicSubAndGet32(&a->references, 1);
	return 0;
}

/* name unknown */
static int systemEvent(int resume, int event, void *parameter, void *context)
{
	struct RtlAdapter *a = context;

	(void)parameter;
	if (resume)
		return 0;
	switch (event) {
	case SYSEVENT_SUSPEND_IDLE:
		if (suspendRequested && !suspending) {
			if (!(a->flags & 4))
				return -2;
			sceKernelSetEventFlag(a->workerEvent, 0x40);
			suspending = 1;
		}
		break;
	case SYSEVENT_SUSPEND_IDLE_QUERY:
		if (suspendRequested)
			return a->flags & 4 ? -1 : 0;
		break;
	case SYSEVENT_SUSPEND_IDLE_ENTER:
		suspending = 0;
		if (a->flags & 1)
			suspendRequested = 1;
		break;
	}
	return 0;
}

/* name unknown */
static int interfaceClose(struct NetDriver *driver, int argument)
{
	struct RtlAdapter *a = driver->private;

	(void)argument;
	if (a->state & 1)
		sceKernelSetEventFlag(a->workerEvent, 4);
	else
		driver->notify(driver,
			NET_DRIVER_EVENT(NET_DRIVER_EVENT_STATUS,
					 NET_DRIVER_STATUS_STOPPED), NULL, 0);
	return 0;
}

/* name unknown */
static int interfaceOpen(struct NetDriver *driver, int argument)
{
	struct RtlAdapter *a = driver->private;
	int error = NET_DRIVER_ERROR_NOT_READY;

	(void)argument;
	if (a->state & 1)
		sceKernelSetEventFlag(a->workerEvent, 2);
	else
		driver->notify(driver,
			NET_DRIVER_EVENT(NET_DRIVER_EVENT_STATUS,
					 NET_DRIVER_STATUS_ERROR), &error, 4);
	return 0;
}

/* name unknown */
int rtlProcessEvent(int event, int type, void *parameter, void *context)
{
	struct RtlAdapter *a = context;

	(void)type;
	(void)parameter;
	if (event == 0x10)
		return sceKernelChangeThreadCpuAffinityMask(a->thread, 0x80000);
	if (event == 0x11)
		return sceKernelChangeThreadCpuAffinityMask(a->thread, 0xf0000);
	return 0;
}

/* name unknown */
static int multicast(struct NetDriver *driver, const unsigned char *addresses,
		     unsigned int length)
{
	struct RtlAdapter *a = driver->private;
	unsigned int value, hash[2];
	int ret;

	if (!a->active)
		return RTL_ERROR(4);
	sceKernelAtomicAddAndGet32(&a->references, 1);
	lockFastMutex(&a->mutex);
	if (!addresses) {
		if (length) {
			ret = RTL_ERROR(1);
			goto out;
		}
		ret = rtlReadRegister(a, 0xc010, 0x100, 4, &value);
		if (ret < 0)
			goto out;
		value &= ~4;
		ret = rtlWriteRegister(a, 0xc010, 0x10f, 4, value);
		goto out;
	}
	if (length % 6) {
		ret = RTL_ERROR(1);
		goto out;
	}
	ret = rtlReadRegister(a, 0xc010, 0x100, 4, &value);
	if (ret < 0)
		goto out;
	value |= 4;
	ret = rtlWriteRegister(a, 0xc010, 0x10f, 4, value);
	if (ret < 0)
		goto out;
	hash[0] = 0;
	hash[1] = 0;
	for (unsigned int n = 0; n < length / 6; n++) {
		unsigned int crc = ~0U;

		/* Sony repeatedly hashes the first address in the supplied list. */
		for (unsigned int i = 0; i < 6; i++) {
			for (unsigned int bit = 0; bit < 8; bit++) {
				unsigned int different = (crc >> 31) ^ ((addresses[i] >> bit) & 1);

				crc <<= 1;
				if (different)
					crc ^= 0x04c11db7;
			}
		}
		hash[crc >> 31] |= 1U << ((crc >> 26) & 31);
	}
	value = __builtin_bswap32(hash[1]);
	ret = rtlWriteRegister(a, 0xcd00, 0x10f, 4, value);
	if (ret < 0)
		goto out;
	value = __builtin_bswap32(hash[0]);
	ret = rtlWriteRegister(a, 0xcd04, 0x10f, 4, value);
out:
	sceKernelUnlockFastMutex(&a->mutex);
	sceKernelAtomicSubAndGet32(&a->references, 1);
	return ret;
}

/* name unknown */
void rtlPowerEvent(int event, int value)
{
	struct RtlAdapter *a = &rtlAdapter;

	if (event != 0x10021)
		return;
	if (!(value & 0x10)) {
		a->state |= 1;
		return;
	}
	a->state &= ~1;
	if (a->flags & 2)
		a->driver.notify(&a->driver, NET_DRIVER_EVENT_RESUMED, NULL, 0);
}

/* name unknown */
static inline __attribute__((always_inline)) int pollPhy(struct RtlAdapter *a, unsigned int *value, int read)
{
	int ret;

	for (;;) {
		ret = rtlReadRegister(a, 0xde00, 0x100, 4, value);
		if (ret < 0 || !!(*value & 0x80000000) == read)
			return ret;
		sceKernelDelayThread(100);
	}
}

/* name unknown */
static int setLink(struct RtlAdapter *a, unsigned int mode)
{
	unsigned int value, advertisement;
	int ret;

	switch (mode) {
	case 0:
		value = 0x80000800;
		advertisement = 0x80040000;
		break;
	case 0x11:
		value = 0x80000000;
		advertisement = 0x80040000;
		break;
	case 0x13:
		value = 0x80000100;
		advertisement = 0x80040000;
		break;
	case 0x15:
		value = 0x80001200;
		advertisement = 0x80040020;
		break;
	case 0x17:
		value = 0x80001300;
		advertisement = 0x80040040;
		break;
	case 0x21:
		value = 0x80002000;
		advertisement = 0x80040000;
		break;
	case 0x23:
		value = 0x80002100;
		advertisement = 0x80040000;
		break;
	case 0x25:
		value = 0x80003200;
		advertisement = 0x80040080;
		break;
	case 0x27:
		value = 0x80003300;
		advertisement = 0x80040100;
		break;
	case 0x77:
		value = 0x80003300;
		advertisement = 0x800401e1;
		break;
	default:
		return RTL_ERROR(8);
	}
	ret = rtlWriteRegister(a, 0xde00, 0x10f, 4, value);
	if (ret < 0)
		return ret;
	ret = pollPhy(a, &value, 0);
	if (ret < 0)
		return ret;
	if (mode & 4) {
		value = advertisement;
		ret = rtlWriteRegister(a, 0xde00, 0x10f, 4, value);
		if (ret < 0)
			return ret;
		ret = pollPhy(a, &value, 0);
		if (ret < 0)
			return ret;
	}
	a->link = 0;
	if (a->flags & 2)
		a->driver.notify(&a->driver, NET_DRIVER_EVENT_LINK_DOWN, NULL, 0);
	value = 0;
	ret = rtlWriteRegister(a, 0xde00, 0x10f, 4, value);
	if (ret < 0)
		return ret;
	ret = pollPhy(a, &value, 1);
	if (ret < 0)
		return ret;
	value = (value & 0xffff) | 0x80008000;
	ret = rtlWriteRegister(a, 0xde00, 0x10f, 4, value);
	if (ret < 0)
		return ret;
	ret = pollPhy(a, &value, 0);
	if (ret < 0)
		return ret;
	for (int count = 100; count; count--) {
		value = 0;
		ret = rtlWriteRegister(a, 0xde00, 0x10f, 4, value);
		if (ret < 0)
			return ret;
		ret = pollPhy(a, &value, 1);
		if (ret < 0 || !(value & 0x8000))
			return ret;
		sceKernelDelayThread(10000);
	}
	return RTL_ERROR(3);
}

/* name unknown */
static int interfaceControl(struct NetDriver *driver, unsigned int command,
			    int *value, unsigned int length)
{
	struct RtlAdapter *a = driver->private;
	unsigned int reg, mode, control;
	int ret;

	if (length != 4) {
		ret = RTL_ERROR(1);
		goto out;
	}
	if (command == NET_DRIVER_CTL_GET_LINK) {
		*value = a->link;
		ret = 0;
		goto out;
	}
	if (!a->active)
		return RTL_ERROR(4);
	sceKernelAtomicAddAndGet32(&a->references, 1);
	switch (command) {
	case NET_DRIVER_CTL_SET_LINK:
		mode = *value;
		lockFastMutex(&a->mutex);
		ret = setLink(a, mode);
		sceKernelUnlockFastMutex(&a->mutex);
		break;
	case NET_DRIVER_CTL_SET_PROMISCUOUS:
		mode = *value;
		lockFastMutex(&a->mutex);
		ret = rtlReadRegister(a, 0xc010, 0x100, 4, &reg);
		if (ret >= 0) {
			control = a->macControl;
			if (mode == 1) {
				control |= 1;
				reg |= 1;
			} else {
				control &= ~1;
				reg &= ~1;
			}
			a->macControl = control;
			ret = rtlWriteRegister(a, 0xc010, 0x10f, 4, reg);
		}
		sceKernelUnlockFastMutex(&a->mutex);
		break;
	case NET_DRIVER_CTL_GET_PROMISCUOUS:
		ret = a->macControl & 1;
		if (ret) {
			*value = 1;
			ret = 0;
		} else {
			*value = 0;
		}
		break;
	default:
		ret = RTL_ERROR(8);
	}
out:
	/* The original also decrements for GET_LINK and an invalid length. */
	sceKernelAtomicSubAndGet32(&a->references, 1);
	return ret;
}

/* name unknown */
int rtlUpdateLink(struct RtlAdapter *a)
{
	unsigned int value, partner;
	unsigned int full, half;
	int ret;

	lockFastMutex(&a->mutex);
	value = 0x10000;
	ret = rtlWriteRegister(a, 0xde00, 0x10f, 4, value);
	if (ret < 0)
		goto out;
	ret = pollPhy(a, &value, 1);
	if (ret < 0)
		goto out;
	if (a->link & 1) {
		if (!(value & 4)) {
			a->link = 0;
			a->driver.notify(&a->driver, NET_DRIVER_EVENT_LINK_DOWN, NULL, 0);
		}
		goto out;
	}
	if (!(value & 4))
		goto out;
	value = 0;
	ret = rtlWriteRegister(a, 0xde00, 0x10f, 4, value);
	if (ret < 0)
		goto out;
	ret = pollPhy(a, &value, 1);
	if (ret < 0)
		goto out;
	if (!(value & 0x1000)) {
		if (value & 0x2000) {
			full = 0x23;
			half = 0x21;
		} else {
			full = 0x13;
			half = 0x11;
		}
		a->link = value & 0x100 ? full : half;
	} else {
		value = 0x50000;
		ret = rtlWriteRegister(a, 0xde00, 0x10f, 4, value);
		if (ret < 0)
			goto out;
		ret = pollPhy(a, &value, 1);
		if (ret < 0)
			goto out;
		partner = value;
		value = 0x40000;
		ret = rtlWriteRegister(a, 0xde00, 0x10f, 4, value);
		if (ret < 0)
			goto out;
		ret = pollPhy(a, &value, 1);
		if (ret < 0)
			goto out;
		value &= partner & 0xffff;
		if (value & 0x100)
			a->link = 0x27;
		else if (value & 0x80)
			a->link = 0x25;
		else if (value & 0x40)
			a->link = 0x17;
		else if (value & 0x20)
			a->link = 0x15;
	}
	a->driver.notify(&a->driver, NET_DRIVER_EVENT_LINK_UP, NULL, 0);
out:
	sceKernelUnlockFastMutex(&a->mutex);
	return ret;
}

/* name unknown */
static int writeOtpWord(unsigned int index, unsigned int word)
{
	struct RtlAdapter *a = &rtlAdapter;
	unsigned int value, bit = index & 31;
	int ret;

	ret = rtlWriteRegister(a, 0xe81c, 0x101, 4, 0xc0);
	if (ret < 0)
		return ret;
	ret = rtlWriteRegister(a, 0xe858, 0x108, 4, 0xc4000000);
	if (ret < 0)
		return ret;
	if (bit > 16)
		bit = 32 - bit;
	value = ((word >> bit) & ((1U << (16 - bit)) - 1)) << (bit + 1);
	value |= word & ((1U << bit) - 1);
	value = (value & 0xffff) | (index | 0x4000 | ((value & 0x10000) >> 9)) << 16;
	ret = rtlWriteRegister(a, 0xdd00, 0x10f, 4, value);
	if (ret < 0)
		return ret;
	do {
		ret = rtlReadRegister(a, 0xdd00, 0x100, 4, &value);
		if (ret < 0)
			return ret;
	} while (value & 0x40000000);
	ret = rtlWriteRegister(a, 0xe858, 0x108, 4, 0);
	if (ret >= 0)
		ret = rtlWriteRegister(a, 0xe81c, 0x101, 4, 0);
	return ret;
}

int SceUsbEtherRtlForDriver_7473CB10(unsigned short reg, unsigned short mask,
				  unsigned int *value)
{
	int ret;

	if (!value)
		return -1;
	lockFastMutex(&rtlAdapter.mutex);
	ret = rtlReadRegister(&rtlAdapter, reg, mask, 4, value);
	sceKernelUnlockFastMutex(&rtlAdapter.mutex);
	return ret;
}

int SceUsbEtherRtlForDriver_1E0E3E59(unsigned short reg, unsigned short mask,
				  unsigned int value)
{
	int ret;

	lockFastMutex(&rtlAdapter.mutex);
	ret = rtlWriteRegister(&rtlAdapter, reg, mask, 4, value);
	sceKernelUnlockFastMutex(&rtlAdapter.mutex);
	return ret;
}

int SceUsbEtherRtlForDriver_BAF95A47(const unsigned char *mac)
{
	unsigned int offset = 0, value, bit, word, index;
	int ret;

	lockFastMutex(&rtlAdapter.mutex);
	while (offset < 0xb4) {
		value = 0xc0;
		ret = rtlWriteRegister(&rtlAdapter, 0xe81c, 0x101, 4, value);
		if (ret < 0)
			goto out;
		index = offset >> 1;
		value = index << 16 | 0x80000000;
		ret = rtlWriteRegister(&rtlAdapter, 0xdd00, 0x10f, 4, value);
		if (ret < 0)
			goto out;
		do {
			ret = rtlReadRegister(&rtlAdapter, 0xdd00, 0x100, 4, &value);
			if (ret < 0)
				goto out;
		} while (value & 0x80000000);
		bit = (value >> 16) & 31;
		if (bit > 16)
			bit = 32 - bit;
		word = (value & 0xffff) | ((value & 0x800000) >> 7);
		word = ((word >> (bit + 1)) & ((1U << (16 - bit)) - 1)) << bit |
			(word & ((1U << bit) - 1));
		value = 0;
		ret = rtlWriteRegister(&rtlAdapter, 0xe81c, 0x101, 4, value);
		if (ret < 0)
			goto out;
		if (offset & 1) {
			value = word & 0xff00;
			if (value == 0xff00) {
				ret = writeOtpWord(index & 255, 0x25ff);
				if (ret >= 0)
					ret = writeOtpWord((index + 1) & 255, 0xc0);
				if (ret >= 0)
					ret = writeOtpWord((index + 2) & 255, mac[0] | mac[1] << 8);
				if (ret >= 0)
					ret = writeOtpWord((index + 3) & 255, mac[2] | mac[3] << 8);
				if (ret >= 0)
					ret = writeOtpWord((index + 4) & 255, mac[4] | mac[5] << 8);
				goto out;
			}
			if ((word & 0xe000) == 0x2000) {
				offset += ((word >> 8) & 31) + 4;
				continue;
			}
		} else {
			value = word & 255;
			if (value == 255) {
				ret = writeOtpWord(index & 255, 0xc025);
				if (ret >= 0)
					ret = writeOtpWord((index + 1) & 255, mac[0] << 8);
				if (ret >= 0)
					ret = writeOtpWord((index + 2) & 255, mac[1] | mac[2] << 8);
				if (ret >= 0)
					ret = writeOtpWord((index + 3) & 255, mac[3] | mac[4] << 8);
				if (ret >= 0)
					ret = writeOtpWord((index + 4) & 255, mac[5] | 0xff00);
				goto out;
			}
			if ((word & 0xe0) == 0x20) {
				offset += (word & 31) + 4;
				continue;
			}
		}
		if (value) {
			SceDebugForDriver_391B74B7("Unknown Format Type!!\n");
			ret = RTL_ERROR(0x801);
			goto out;
		}
		offset++;
	}
	ret = RTL_ERROR(0x802);
out:
	sceKernelUnlockFastMutex(&rtlAdapter.mutex);
	return ret;
}

/* name unknown */
static __attribute__((noinline, noclone)) void txComplete(struct RtlAdapter *a, int result)
{
	if (result >= 0)
		sceKernelSetEventFlag(a->workerEvent, 8);
}

/* name unknown */
static __attribute__((noinline, noclone)) void rxComplete(struct RtlAdapter *a, int result, int count)
{
	if (result >= 0) {
		a->rxCount = count;
		sceKernelSetEventFlag(a->workerEvent, 0x10);
	}
}

/* name unknown */
static void txDone(int result, int count, void *context)
{
	(void)count;
	txComplete(context, result);
}

/* name unknown */
static void rxDone(int result, int count, void *context)
{
	rxComplete(context, result, count);
}

/* name unknown */
int rtlSendBuffer(struct RtlAdapter *a, void *data, unsigned int length)
{
	return SceUsbdForDriver_470D5289(a->txPipe, data, length, txDone, a);
}

/* name unknown */
int rtlReceiveBuffer(struct RtlAdapter *a, void *data, unsigned int length)
{
	return SceUsbdForDriver_470D5289(a->rxPipe, data, length, rxDone, a);
}

/* name unknown */
static __attribute__((noinline, noclone)) struct RtlAdapter *getAdapter(void)
{
	return &rtlAdapter;
}

/* name unknown */
static __attribute__((noinline, noclone)) int attachAdapter(struct RtlAdapter *a)
{
	__atomic_fetch_or(&a->flags, 1, __ATOMIC_SEQ_CST);
	return sceKernelSetEventFlag(a->workerEvent, 1);
}

/* name unknown */
static __attribute__((noinline, noclone)) int detachAdapter(struct RtlAdapter *a)
{
	__atomic_fetch_and(&a->flags, ~1U, __ATOMIC_SEQ_CST);
	sceKernelSetEventFlag(a->workerEvent, 0x20);
	a->link = 0;
	return 0;
}

/* name unknown */
static int detach(int device)
{
	struct RtlAdapter *a = getAdapter();

	(void)device;
	if (a == NULL || sceKernelSetEventFlag(a->controlEvent, 1) < 0)
		return -1;
	return detachAdapter(a) < 0 ? -1 : 0;
}

/* name unknown */
static int probe(int device)
{
	SceUsbdDeviceDescriptor *descriptor;
	SceUsbdInterfaceDescriptor *interface;
	unsigned int vendor, product;
	void *previous;

	descriptor = SceUsbdForDriver_BC3EF82B(device, NULL, 1);
	if (!descriptor)
		return -1;
	vendor = descriptor->idVendor;
	product = descriptor->idProduct;
	if (vendor == 0x0bda) {
		if (product != 0x8152)
			return -1;
	} else if (vendor != 0x054c || product != 0x08a5) {
		return -1;
	}
	previous = descriptor;
	while ((interface = SceUsbdForDriver_BC3EF82B(device, previous, 4))) {
		if (interface->bInterfaceClass == 255)
			return 0;
		previous = interface;
	}
	return -1;
}

/* name unknown */
static int attach(int device)
{
	struct RtlAdapter *a = getAdapter();
	SceUsbdInterfaceDescriptor *interface;
	SceUsbdEndpointDescriptor *endpoint;
	SceUsbdDeviceRequest request;
	unsigned int bits;
	void *previous;
	int ret;

	if (a == NULL)
		return -1;
	a->controlPipe = a->rxPipe = a->txPipe = -1;
	ret = sceKernelClearEventFlag(a->controlEvent, 0);
	if (ret < 0)
		return ret;
	previous = SceUsbdForDriver_BC3EF82B(device, NULL, 1);
	if (!previous)
		return -1;
	a->controlPipe = SceUsbdForDriver_1CDBFF9F(device, NULL);
	if (a->controlPipe < 0)
		return -1;
	while ((interface = SceUsbdForDriver_BC3EF82B(device, previous, 4))) {
		previous = interface;
		if (interface->bInterfaceClass != 255 || !interface->bNumEndpoints)
			continue;
		for (unsigned int i = 0; i < interface->bNumEndpoints; i++) {
			endpoint = SceUsbdForDriver_BC3EF82B(device, previous, 5);
			previous = endpoint;
			/* The original assumes the descriptor exists. */
			if ((endpoint->bEndpointAddress & 31) == 1) {
				a->rxPipe = SceUsbdForDriver_1CDBFF9F(device, endpoint);
				if (a->rxPipe < 0)
					return -1;
				a->rxMaxPacket = endpoint->wMaxPacketSize;
			} else if ((endpoint->bEndpointAddress & 31) == 2) {
				a->txPipe = SceUsbdForDriver_1CDBFF9F(device, endpoint);
				if (a->txPipe < 0)
					return -1;
				a->txMaxPacket = endpoint->wMaxPacketSize;
			}
		}
		previous = interface;
	}
	if (a->controlPipe < 0 || a->rxPipe < 0 || a->txPipe < 0)
		return -1;
	request.bmRequestType = 0;
	request.bRequest = 9;
	request.wValue = 1;
	request.wIndex = 0;
	request.wLength = 0;
	ret = SceUsbdForDriver_2E05660F(a->controlPipe, &request, NULL, rtlControlDone, a);
	if (ret < 0)
		return ret;
	ret = sceKernelWaitEventFlag(a->controlEvent, 3,
				      SCE_KERNEL_EVF_WAITMODE_OR |
				      SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &bits, NULL);
	if (ret < 0)
		return ret;
	request.bmRequestType = 1;
	request.bRequest = 11;
	request.wValue = 0;
	request.wIndex = 0;
	request.wLength = 0;
	ret = SceUsbdForDriver_2E05660F(a->controlPipe, &request, NULL, rtlControlDone, a);
	if (ret < 0)
		return ret;
	ret = sceKernelWaitEventFlag(a->controlEvent, 3,
				      SCE_KERNEL_EVF_WAITMODE_OR |
				      SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &bits, NULL);
	if (ret < 0)
		return ret;
	a->device = device;
	a->txBufferSize = a->rxBufferSize = 0x4000;
	ret = attachAdapter(a);
	return ret < 0 ? ret : 0;
}

/* name unknown */
static inline __attribute__((always_inline)) void rtlPopulateInterface(struct RtlAdapter *a)
{
	a->driver.stop = stop;
	a->driver.start = start;
	a->driver.open = interfaceOpen;
	a->driver.close = interfaceClose;
	a->driver.kickTx = kickTx;
	a->driver.multicast = multicast;
	a->driver.control = interfaceControl;
	a->driver.capabilities = NET_DRIVER_CAPABILITIES_ETHER;
	a->driver.type = NET_DRIVER_TYPE_ETHER;
	a->driver.mtu = 1500;
	a->driver.prefix = 8;
	a->driver.private = a;
}

/* name unknown */
static __attribute__((noinline, noclone)) int registerDriver(struct RtlAdapter *a)
{
	int ret;

	ret = sceKernelCreateEventFlag("SceRtlControlTransfer", 0, 0, NULL);
	if (ret < 0)
		return ret;
	a->controlEvent = ret;
	usbDriver.name = usbDriverName;
	usbDriver.probe = probe;
	usbDriver.attach = attach;
	usbDriver.detach = detach;
	return SceUsbdForDriver_1EC94F18(&usbDriver);
}

int module_start(SceSize length, void *arg)
{
	struct RtlAdapter *a = &rtlAdapter;
	unsigned int hardware;
	int ret;

	(void)length;
	(void)arg;
	hardware = SceSysconForDriver_CBD6D8BC();
	if (SceSblAIMgrForDriver_D78B04A2() && SceSblAIMgrForDriver_4273B97B() && !(hardware & 0x80))
		return 0;
	memset(a, 0, sizeof(*a));
	ret = sceKernelInitializeFastMutex(&a->mutex, "SceUsbEtherRtlControl", 0x2000, 0);
	if (ret < 0)
		return ret;
	ret = sceKernelCreateEventFlag("SceUsbEtherRtlWorker", 0, 0, NULL);
	if (ret < 0)
		return ret;
	a->workerEvent = ret;
	ret = sceKernelCreateThread("SceNetDrvEther", workerEntry, 0x3e, 0x2000, 0, 0xf0000, NULL);
	if (ret < 0)
		return ret;
	a->thread = ret;
	ret = sceKernelStartThread(ret, 4, &a);
	if (ret < 0)
		return ret;
	ret = sceKernelRegisterSysEventHandler("SceUsbEtherRtl", systemEvent, a);
	if (ret < 0)
		return ret;
	ret = registerDriver(a);
	return ret < 0 ? ret : 0;
}

/* name unknown */
static void drainReferences(struct RtlAdapter *a)
{
	a->active = 0;
	for (int count = 3000; count && a->references; count--)
		sceKernelDelayThread(1000);
}

/* name unknown */
static inline __attribute__((always_inline)) int resetReceive(struct RtlAdapter *a)
{
	unsigned char byte;
	int ret;

	ret = rtlWritePla8(a, 0xe813, 0x10);
	if (ret < 0)
		return ret;
	for (int count = 1000; count; count--) {
		ret = rtlReadPla8(a, 0xe813, &byte);
		if (ret < 0 || !(byte & 0x10))
			return ret;
		sceKernelDelayThread(1000);
	}
	return RTL_ERROR(3);
}

/* name unknown */
static int setupWake(struct RtlAdapter *a)
{
	unsigned char byte, control;
	unsigned short word;
	int ret;

	ret = rtlEnableReceive(a);
	/* The error path also unlocks if enableReceive failed before this lock. */
	if (ret < 0)
		goto out;
	lockFastMutex(&a->mutex);
	ret = resetReceive(a);
	if (ret < 0)
		goto out;
	ret = rtlReadPla16(a, 0xc0b6, &word);
	if (ret < 0)
		goto out;
	ret = rtlWritePla16(a, 0xc0b6, word | 1);
	if (ret < 0)
		goto out;
	ret = rtlReadPla8(a, 0xe822, &byte);
	if (ret < 0)
		goto out;
	ret = rtlReadPla8(a, 0xe81c, &control);
	if (ret < 0)
		goto out;
	ret = rtlWritePla8(a, 0xe81c, control | 0xc0);
	if (ret < 0)
		goto out;
	ret = rtlReadPla8(a, 0xe81c, &control);
	if (ret < 0)
		goto out;
	ret = rtlWritePla8(a, 0xe822, (byte & 0xf7) | 2);
	if (ret < 0)
		goto out;
	ret = rtlReadPla8(a, 0xe81c, &control);
	if (ret < 0)
		goto out;
	ret = rtlWritePla8(a, 0xe81c, control & 0x3f);
	if (ret < 0)
		goto out;
	ret = rtlReadPla8(a, 0xe81c, &control);
	if (ret < 0)
		goto out;
	ret = rtlReadPla8(a, 0xe84f, &byte);
	if (ret < 0)
		goto out;
	ret = rtlWritePla8(a, 0xe84f, byte | 0x80);
	if (ret >= 0)
		ret = rtlWakeFeature(a);
out:
	sceKernelUnlockFastMutex(&a->mutex);
	return ret;
}

/* name unknown */
static int lowPower(struct RtlAdapter *a)
{
	unsigned char byte;
	int ret;

	SceGpioForDriver_372022A4(1, 2, 0);
	sceGpioPortSet(1, 2);
	lockFastMutex(&a->mutex);
	ret = rtlWriteUsb16(a, 0xe436, 4000);
	if (ret < 0)
		goto out;
	ret = rtlWriteUsb8(a, 0xe434, 0x19);
	if (ret < 0)
		goto out;
	ret = rtlReadUsb8(a, 0xd429, &byte);
	if (ret < 0)
		goto out;
	ret = rtlWriteUsb8(a, 0xd429, byte | 0x80);
	if (ret < 0)
		goto out;
	ret = rtlReadUsb8(a, 0xc2f8, &byte);
	if (ret >= 0)
		ret = rtlWriteUsb8(a, 0xc2f8, byte | 1);
out:
	sceKernelUnlockFastMutex(&a->mutex);
	return ret;
}

/* name unknown */
static void suspendAdapter(struct RtlAdapter *a)
{
	int value, ret;

	drainReferences(a);
	if (!(a->flags & 4))
		return;
	if (SceSblAIMgrForDriver_C6E83F34() &&
	    sceRegMgrGetKeyInt("/CONFIG/NET/", "wol_config", &value) >= 0 && value == 1) {
		ret = setupWake(a);
		if (ret < 0) {
			SceDebugForDriver_391B74B7("Set Wake On LAN : NG (ret=0x%08x)\n", ret);
		} else if (!SceSysrootForDriver_8AA268D6(0xc)) {
			ret = lowPower(a);
			if (ret < 0) {
				SceDebugForDriver_391B74B7("Set Low Power Mode : NG (ret=0x%08x)\n", ret);
			} else {
				SceDebugForDriver_391B74B7("Set Low Power Mode : OK\n");
				SceUsbServForDriver_6D738018();
			}
		} else {
			SceUsbServForDriver_6D738018();
		}
	}
	__atomic_fetch_and(&a->flags, ~12U, __ATOMIC_SEQ_CST);
}

/* name unknown */
static int plaPatch(struct RtlAdapter *a)
{
	unsigned int value;
	unsigned char byte;
	int ret;

	ret = rtlWritePla(a, 0xf800, 0xff, sizeof(plaPhyPatch), plaPhyPatch);
	if (ret < 0)
		return ret;
	value = 0x80000000;
	ret = rtlWritePla(a, 0xfc24, 0xcc, 4, &value);
	if (ret < 0)
		return ret;
	value = 0x06e9170b;
	ret = rtlWritePla(a, 0xfc28, 0xff, 4, &value);
	if (ret < 0)
		return ret;
	value = 0x13870989;
	ret = rtlWritePla(a, 0xfc2c, 0xff, 4, &value);
	if (ret < 0)
		return ret;
	value = 0x061d01b7;
	ret = rtlWritePla(a, 0xfc30, 0xff, 4, &value);
	if (ret < 0)
		return ret;
	value = 0x00200000;
	ret = rtlWritePla(a, 0xe420, 0xcc, 4, &value);
	if (ret < 0)
		return ret;
	value = 0x00000018;
	ret = rtlWritePla(a, 0xe420, 0x33, 4, &value);
	if (ret < 0)
		return ret;
	value = 0x047b0000;
	ret = rtlWritePla(a, 0xfc34, 0xcc, 4, &value);
	if (ret < 0)
		return ret;
	ret = rtlReadUsb(a, 0xd428, 4, &value);
	if (ret < 0)
		return ret;
	value |= 0x8000;
	ret = rtlWriteUsb(a, 0xd428, 0x22, 4, &value);
	if (ret < 0)
		return ret;
	value = 0x02100000;
	ret = rtlWriteUsb(a, 0xc0cc, 0xcc, 4, &value);
	if (ret < 0)
		return ret;
	ret = rtlReadUsb(a, 0xd428, 4, &value);
	if (ret < 0)
		return ret;
	value &= ~0x8000;
	ret = rtlWriteUsb(a, 0xd428, 0x22, 4, &value);
	if (ret < 0)
		return ret;
	ret = rtlReadPla8(a, 0xdd93, &byte);
	if (ret >= 0)
		ret = rtlWritePla8(a, 0xdd93, byte & 0xf8);
	return ret;
}

/* name unknown */
static int initializeHardware(struct RtlAdapter *a)
{
	unsigned int value = 0;
	unsigned short revision;
	int ret;

	if (SceSblAIMgrForDriver_C6E83F34() && !SceSysrootForDriver_8AA268D6(0xc)) {
		SceGpioForDriver_372022A4(1, 2, 0);
		sceGpioPortClear(1, 2);
	}
	ret = rtlWritePla(a, 0xfc28, 0xff, 4, &value);
	if (ret < 0)
		return ret;
	ret = rtlWritePla(a, 0xfc2c, 0xff, 4, &value);
	if (ret < 0)
		return ret;
	ret = rtlWritePla(a, 0xfc30, 0xff, 4, &value);
	if (ret < 0)
		return ret;
	ret = rtlWritePla(a, 0xfc34, 0xff, 4, &value);
	if (ret < 0)
		return ret;
	ret = rtlWriteUsb(a, 0xfc28, 0xff, 4, &value);
	if (ret < 0)
		return ret;
	ret = rtlWriteUsb(a, 0xfc2c, 0xff, 4, &value);
	if (ret < 0)
		return ret;
	ret = rtlWriteUsb(a, 0xfc30, 0xff, 4, &value);
	if (ret < 0)
		return ret;
	ret = rtlWriteUsb(a, 0xfc34, 0xff, 4, &value);
	if (ret < 0)
		return ret;
	sceKernelDelayThread(3000);
	ret = rtlWritePla(a, 0xfc24, 0xcc, 4, &value);
	if (ret < 0)
		return ret;
	ret = rtlWriteUsb(a, 0xfc24, 0xcc, 4, &value);
	if (ret < 0)
		return ret;
	ret = rtlReadPla16(a, 0xe612, &revision);
	if (ret < 0)
		return ret;
	if ((revision & 0x7cf0) == 0x4c00) {
		a->revision = 1;
		ret = plaPatch(a);
		if (ret < 0)
			return ret;
	} else {
		a->revision = (revision & 0x7cf0) == 0x4c10 ? 2 : 0;
	}
	if (SceSblAIMgrForDriver_C6E83F34() && !SceSysrootForDriver_8AA268D6(0xc)) {
		for (unsigned int i = 0; i < sizeof(usbPhyPatch); i += 2) {
			ret = rtlWriteUsb16(a, 0xf800 + i, *(const unsigned short *)(usbPhyPatch + i));
			if (ret < 0)
				return ret;
		}
		ret = rtlWriteUsb16(a, 0xfc26, 0xa000);
		if (ret < 0)
			return ret;
		ret = rtlWriteUsb32(a, 0xfc28, 0x3019);
		if (ret < 0)
			return ret;
		ret = rtlWriteUsb32(a, 0xfc2c, 0x024f085f);
		if (ret < 0)
			return ret;
	}
	ret = resetReceive(a);
	if (ret < 0)
		return ret;
	ret = rtlWritePla16(a, 0xc016, 0x05f2);
	if (ret < 0)
		return ret;
	ret = rtlWriteRegister(a, 0xc0a0, 0x10f, 4, 0x00080002);
	if (ret < 0)
		return ret;
	ret = rtlWriteRegister(a, 0xc0a4, 0x103, 4, 0x00000038);
	if (ret < 0)
		return ret;
	ret = rtlWriteRegister(a, 0xc0a8, 0x103, 4, 0x00000048);
	if (ret < 0)
		return ret;
	ret = rtlWriteRegister(a, 0xe618, 0x10f, 4, 0x00400008);
	if (ret < 0)
		return ret;
	ret = rtlWriteRegister(a, 0xe610, 0x10f, 4, 0x4f000880);
	if (ret < 0)
		return ret;
	ret = rtlWriteRegister(a, 0xd408, 0x004, 4, 0x00030000);
	if (ret < 0)
		return ret;
	ret = rtlWriteRegister(a, 0xd40c, 0x00f, 4, 0xfde80600);
	if (ret < 0)
		return ret;
	ret = rtlWriteRegister(a, 0xd404, 0x00f, 4, 0x80001e11);
	if (ret < 0)
		return ret;
	ret = rtlWriteRegister(a, 0xde00, 0x10f, 4, 0x1d0000);
	if (ret < 0)
		return ret;
	for (;;) {
		ret = rtlReadRegister(a, 0xde00, 0x100, 4, &value);
		if (ret < 0)
			return ret;
		if (value & 0x80000000)
			break;
		sceKernelDelayThread(100);
	}
	ret = rtlWriteRegister(a, 0xde00, 0x10f, 4, 0x801e0050);
	if (ret < 0)
		return ret;
	for (;;) {
		ret = rtlReadRegister(a, 0xde00, 0x100, 4, &value);
		if (ret < 0 || !(value & 0x80000000))
			return ret;
		sceKernelDelayThread(100);
	}
}

/* name unknown */
static inline __attribute__((always_inline)) int allocateBuffer(const char *name, unsigned int length, void **base)
{
	unsigned int options[22];
	int ret;

	memset(options, 0, sizeof(options));
	options[0] = sizeof(options);
	options[2] = 0x200004;
	options[5] = 0x1000;
	ret = sceKernelAllocMemBlockForKernel(name, SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_NC_RW, (length + 0xfff) & ~0xfff,
				    (SceKernelAllocMemBlockKernelOpt *)options);
	if (ret >= 0)
		ret = sceKernelGetMemBlockBase(ret, base);
	return ret;
}

/* name unknown */
static int initializeAdapter(struct RtlAdapter *a)
{
	unsigned int value;
	unsigned short word;
	unsigned char byte;
	void *base;
	int ret;

	lockFastMutex(&a->mutex);
	ret = initializeHardware(a);
	sceKernelUnlockFastMutex(&a->mutex);
	if (ret < 0)
		return 0;
	ret = rtlReadRegister(a, 0xc000, 0x100, 4, &value);
	if (ret < 0)
		return 0;
	for (unsigned int i = 0; i < 4; i++)
		a->driver.mac.data[i] = value >> (8 * i);
	ret = rtlReadRegister(a, 0xc004, 0x100, 4, &value);
	if (ret < 0)
		return 0;
	a->driver.mac.data[4] = value;
	a->driver.mac.data[5] = value >> 8;
	a->references = 0;
	a->active = 1;
	if (!(a->flags & 2)) {
		rtlPopulateInterface(a);
		ret = SceNetPsForDriver_1ABF937D(&a->driver);
		if (ret < 0)
			return 0;
		__atomic_fetch_or(&a->flags, 2, __ATOMIC_SEQ_CST);
		ret = allocateBuffer("SceNetDrvEtherTx", a->txBufferSize, &base);
		if (ret < 0 || !base)
			return 0;
		a->txBuffer = base;
		ret = allocateBuffer("SceNetDrvEtherRx", a->rxBufferSize, &base);
		if (ret < 0 || !base)
			return 0;
		a->rxBuffer = base;
	}
	ret = rtlReceiveBuffer(a, a->rxBuffer, a->rxBufferSize);
	if (ret < 0)
		return 0;
	lockFastMutex(&a->mutex);
	ret = rtlReadPla16(a, 0xc0b6, &word);
	if (ret < 0)
		goto out;
	ret = rtlWritePla16(a, 0xc0b6, word & ~1);
	if (ret < 0)
		goto out;
	ret = rtlReadPla8(a, 0xe822, &byte);
	if (ret < 0)
		goto out;
	ret = rtlWritePla8(a, 0xe822, byte & ~2);
	if (ret < 0)
		goto out;
	ret = rtlReadPla8(a, 0xe84f, &byte);
	if (ret >= 0)
		rtlWritePla8(a, 0xe84f, byte & ~0x80);
out:
	sceKernelUnlockFastMutex(&a->mutex);
	__atomic_fetch_or(&a->flags, 4, __ATOMIC_SEQ_CST);
	a->state |= 1;
	ScePowerForDriver_B24263F0(2, rtlPowerEvent);
	for (int count = 30; count; count--) {
		rtlUpdateLink(a);
		sceKernelDelayThread(100000);
		if (a->link & 1)
			break;
	}
	return 1;
}

/* name unknown */
static void startReceive(struct RtlAdapter *a, int *retries)
{
	unsigned int flags;
	unsigned char byte;
	int ret;

	for (int count = 10;; ) {
		flags = a->flags;
		if (flags & 1)
			break;
		sceKernelDelayThread(1000000);
		if (--count == 0) {
			ret = RTL_ERROR(7);
			goto error;
		}
	}
	if ((flags & 4) && (a->link & 1)) {
		lockFastMutex(&a->mutex);
		ret = rtlReadPla8(a, 0xe813, &byte);
		if (ret >= 0)
			ret = rtlWritePla8(a, 0xe813, byte | 4);
		sceKernelUnlockFastMutex(&a->mutex);
		if (ret < 0)
			goto errorReset;
		ret = rtlEnableReceive(a);
		if (ret < 0)
			goto errorReset;
		a->driver.notify(&a->driver,
			NET_DRIVER_EVENT(NET_DRIVER_EVENT_STATUS,
					 NET_DRIVER_STATUS_STARTED), NULL, 0);
		__atomic_fetch_or(&a->flags, 8, __ATOMIC_SEQ_CST);
		*retries = 0;
	} else if (*retries == 10) {
		ret = RTL_ERROR(3);
		*retries = 0;
		a->driver.notify(&a->driver,
			NET_DRIVER_EVENT(NET_DRIVER_EVENT_STATUS,
					 NET_DRIVER_STATUS_ERROR), &ret, 4);
		rtlUpdateLink(a);
	} else {
		sceKernelSetEventFlag(a->workerEvent, 2);
		(*retries)++;
		sceKernelDelayThread(1000000);
	}
	return;
errorReset:
	*retries = 0;
error:
	a->driver.notify(&a->driver,
		NET_DRIVER_EVENT(NET_DRIVER_EVENT_STATUS,
				 NET_DRIVER_STATUS_ERROR), &ret, 4);
}

/* name unknown */
static void stopReceive(struct RtlAdapter *a)
{
	unsigned int value;
	int ret;

	if ((a->flags & 12) != 12) {
		a->driver.notify(&a->driver,
			NET_DRIVER_EVENT(NET_DRIVER_EVENT_STATUS,
					 NET_DRIVER_STATUS_STOPPED), NULL, 0);
		return;
	}
	__atomic_fetch_and(&a->flags, ~8U, __ATOMIC_SEQ_CST);
	lockFastMutex(&a->mutex);
	ret = rtlReadRegister(a, 0xe810, 0x100, 4, &value);
	if (ret >= 0)
		ret = rtlWriteRegister(a, 0xe810, 0x10f, 4, value & ~0x04000000);
	sceKernelUnlockFastMutex(&a->mutex);
	if (ret < 0)
		goto out;
	lockFastMutex(&a->mutex);
	ret = rtlReadRegister(a, 0xe810, 0x100, 4, &value);
	if (ret < 0)
		goto unlock;
	ret = rtlWriteRegister(a, 0xe810, 0x10f, 4, value & ~0x08000000);
	if (ret < 0)
		goto unlock;
	ret = rtlReadRegister(a, 0xe858, 0x100, 4, &value);
	if (ret < 0)
		goto unlock;
	ret = rtlWriteRegister(a, 0xe858, 0x10f, 4, value | 0x80000);
	if (ret < 0)
		goto unlock;
	ret = rtlReadRegister(a, 0xc010, 0x100, 4, &value);
	if (ret >= 0)
		ret = rtlWriteRegister(a, 0xc010, 0x10f, 4, value & ~11U);
unlock:
	sceKernelUnlockFastMutex(&a->mutex);
out:
	if (ret < 0)
		a->driver.notify(&a->driver,
			NET_DRIVER_EVENT(NET_DRIVER_EVENT_STATUS,
					 NET_DRIVER_STATUS_ERROR), &ret, 4);
	else
		a->driver.notify(&a->driver,
			NET_DRIVER_EVENT(NET_DRIVER_EVENT_STATUS,
					 NET_DRIVER_STATUS_STOPPED), NULL, 0);
}

/* name unknown */
static void transmit(struct RtlAdapter *a)
{
	unsigned char *cursor = a->txBuffer, *end = cursor + a->txBufferSize;
	unsigned int remaining = a->txBufferSize, prefix = a->driver.prefix;
	unsigned int bytes = 0, packets = 0, broadcastBytes = 0, broadcastPackets = 0;
	unsigned int multicastBytes = 0, multicastPackets = 0;
	struct NetDriverPacket *packet;
	int ret;

	__atomic_exchange_n(&a->pendingTx, 1, __ATOMIC_SEQ_CST);
	while (prefix + 1514 <= remaining) {
		unsigned int length;
		unsigned char *payload;
		unsigned int *header;

		packet = a->driver.dequeueTx(&a->driver);
		if (!packet)
			break;
		length = packet->length;
		if (remaining < prefix + length) {
			a->driver.releaseTx(&a->driver, packet);
			break;
		}
		bytes += length;
		packets++;
		payload = packet->buffer->data;
		header = (unsigned int *)(payload - prefix);
		header[0] = length | 0xc0000000;
		header[1] = 0;
		memcpy(cursor, header, prefix + length);
		if (payload[0] == 255 && payload[1] == 255 && payload[2] == 255 &&
		    payload[3] == 255 && payload[4] == 255 && payload[5] == 255) {
			broadcastBytes += length;
			broadcastPackets++;
		} else if (payload[0] & 1) {
			multicastBytes += length;
			multicastPackets++;
		}
		a->driver.releaseTx(&a->driver, packet);
		cursor = (unsigned char *)(((unsigned int)cursor + prefix + length + 3) & ~3U);
		remaining = end - cursor;
	}
	if (packets) {
		ret = rtlSendBuffer(a, a->txBuffer, cursor - a->txBuffer);
		if (ret >= 0) {
			a->driver.txBytes += bytes;
			a->driver.txPackets += packets;
			a->driver.txBroadcastBytes += broadcastBytes;
			a->driver.txBroadcastPackets += broadcastPackets;
			a->driver.txMulticastBytes += multicastBytes;
			a->driver.txMulticastPackets += multicastPackets;
			return;
		}
		a->driver.txErrors += packets;
	}
	ret = __atomic_fetch_sub(&a->pendingTx, 1, __ATOMIC_SEQ_CST);
	if (ret > 1)
		sceKernelSetEventFlag(a->workerEvent, 8);
}

/* name unknown */
static void receive(struct RtlAdapter *a)
{
	unsigned char *header = a->rxBuffer;
	unsigned char *end = header + a->rxCount;
	unsigned int delivered = 0;

	if (a->rxCount) {
		while (header + 24 < end) {
			unsigned int value = *(unsigned int *)header;
			unsigned int length = (value & 0x7fff) - 4;
			unsigned char *payload = header + 24;
			struct NetDriverPacket *packet;

			if ((value & 0x7fff) < 4 || !length)
				break;
			if (value & 0x8000) {
				a->driver.rxErrors++;
			} else if (payload + length > end || !(packet = a->driver.allocateRx(&a->driver, length, 0))) {
				a->driver.rxDrops++;
			} else {
				memcpy(packet->buffer->end, payload, length);
				packet->buffer->end += length;
				packet->length += length;
				value = *(unsigned int *)header;
				a->driver.rxBytes += length;
				a->driver.rxPackets++;
				if (value & 0x40000) {
					a->driver.rxBroadcastBytes += length;
					a->driver.rxBroadcastPackets++;
				}
				if (value & 0x100000) {
					a->driver.rxMulticastBytes += length;
					a->driver.rxMulticastPackets++;
				}
				a->driver.deliverRx(&a->driver, packet);
				delivered++;
			}
			header = (unsigned char *)(((unsigned int)header + length + 0x23) & ~7U);
		}
		if (delivered)
			a->driver.notify(&a->driver,
				NET_DRIVER_EVENT(NET_DRIVER_EVENT_STATUS,
						 NET_DRIVER_STATUS_RECEIVED), NULL, 0);
	}
	rtlReceiveBuffer(a, a->rxBuffer, a->rxBufferSize);
	sceKernelDelayThread(500);
}

int workerEntry(SceSize length, void *arg)
{
	struct RtlAdapter *a = *(struct RtlAdapter **)arg;
	SceUInt64 lastPoll = 0, elapsed = 0, now;
	unsigned int bits, timeout = 0, *pointer;
	int retries = 0, ret;

	(void)length;
	a->proc.size = sizeof(a->proc);
	a->proc.create = NULL;
	a->proc.exit = NULL;
	a->proc.kill = NULL;
	a->proc.suspend = NULL;
	a->proc.resume = NULL;
	a->proc.notify = rtlProcessEvent;
	ret = sceKernelRegisterProcEventHandler("SceNetDrvEther", &a->proc, a);
	if (ret < 0) {
		if (a->procId > 0) {
			sceKernelUnregisterProcEventHandler(a->procId);
			a->procId = 0;
		}
		SceDebugForDriver_391B74B7("UsbEtherRtl workerEntry Exit. ret=0x%08x\n", ret);
		sceKernelExitThread(0);
		return ret;
	}
	a->procId = ret;
	for (;;) {
		bits = 0;
		pointer = NULL;
		if (a->flags & 4) {
			pointer = &timeout;
			timeout = 3000000;
		}
		ret = sceKernelWaitEventFlag(a->workerEvent, 0x7f,
					      SCE_KERNEL_EVF_WAITMODE_OR |
					      SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &bits, pointer);
		now = sceKernelGetSystemTimeWide();
		if (ret) {
			if (ret == SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
				elapsed += 3000000U - timeout;
				if (elapsed >= 3000000) {
					rtlUpdateLink(a);
					elapsed = 0;
					lastPoll = now;
				}
			}
			continue;
		}
		if (bits & 0x20) {
			drainReferences(a);
			if (a->flags & 4)
				__atomic_fetch_and(&a->flags, ~12U, __ATOMIC_SEQ_CST);
			retries = 0;
		}
		if (bits & 0x40) {
			suspendAdapter(a);
			retries = 0;
		}
		if ((bits & 1) && initializeAdapter(a))
			lastPoll = 0;
		if (bits & 2)
			startReceive(a, &retries);
		if (bits & 4)
			stopReceive(a);
		if (bits & 8)
			transmit(a);
		if (bits & 0x10)
			receive(a);
		if (now < lastPoll || now > lastPoll + 3000000) {
			rtlUpdateLink(a);
			lastPoll = now;
		}
	}
}
