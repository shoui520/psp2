/* RTL8152 USB Ethernet driver. Private function names are unknown. */
#include "rtl_internal.h"
#include <sysmem/sysevent_ids.h>

struct RtlAdapter rtlAdapter;
static unsigned int suspendRequested, suspending;
static SceUsbdDriver usbDriver;

/* name unknown */
int rtlEnableReceive(struct RtlAdapter *a)
{
	unsigned char byte;
	unsigned int value;
	int ret;

	ksceKernelLockFastMutex(&a->mutex);
	ret = rtlReadPla8(a, 0xe813, &byte);
	if (ret < 0)
		goto out;
	ret = rtlWritePla8(a, 0xe813, byte | 8);
	if (ret < 0)
		goto out;
	ret = rtlReadRegister(a, 0xe858, 0x100, 4, &value);
	if (ret < 0)
		goto out;
	ret = rtlWriteRegister(a, 0xe858, 0x10f, 4, value & ~0x80000);
	if (ret < 0)
		goto out;
	ret = rtlReadPla32(a, 0xc010, &value);
	if (ret >= 0)
		ret = rtlWritePla32(a, 0xc010, value | (a->macControl & 1 ? 11 : 10));
out:
	ksceKernelUnlockFastMutex(&a->mutex);
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
	ksceKernelAtomicAddAndGet32(&a->references, 1);
	old = __atomic_fetch_add(&a->pendingTx, 1, __ATOMIC_SEQ_CST);
	if (!old)
		ksceKernelSetEventFlag(a->workerEvent, 8);
	ksceKernelAtomicSubAndGet32(&a->references, 1);
	return 0;
}

/* name unknown */
static int start(struct NetDriver *driver, void *memoryBase)
{
	struct RtlAdapter *a = driver->private;

	(void)memoryBase;
	if (!a->active)
		return RTL_ERROR(4);
	ksceKernelAtomicAddAndGet32(&a->references, 1);
	ksceKernelAtomicSubAndGet32(&a->references, 1);
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
	ksceKernelAtomicAddAndGet32(&a->references, 1);
	ksceKernelAtomicSubAndGet32(&a->references, 1);
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
			ksceKernelSetEventFlag(a->workerEvent, 0x40);
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
		ksceKernelSetEventFlag(a->workerEvent, 4);
	else
		driver->notify(driver, driver->notify, NET_DRIVER_EVENT_STATUS, NET_DRIVER_STATUS_STOPPED, NULL, 0);
	return 0;
}

/* name unknown */
static int interfaceOpen(struct NetDriver *driver, int argument)
{
	struct RtlAdapter *a = driver->private;
	int error = NET_DRIVER_ERROR_NOT_READY;

	(void)argument;
	if (a->state & 1)
		ksceKernelSetEventFlag(a->workerEvent, 2);
	else
		driver->notify(driver, &error, NET_DRIVER_EVENT_STATUS, NET_DRIVER_STATUS_ERROR, &error, 4);
	return 0;
}

/* name unknown */
int rtlProcessEvent(int event, int type, void *parameter, void *context)
{
	struct RtlAdapter *a = context;

	(void)type;
	(void)parameter;
	if (event == 0x10)
		return ksceKernelChangeThreadCpuAffinityMask(a->thread, 0x80000);
	if (event == 0x11)
		return ksceKernelChangeThreadCpuAffinityMask(a->thread, 0xf0000);
	return 0;
}

/* name unknown */
static int multicast(struct NetDriver *driver, const unsigned char *addresses,
		     unsigned int length)
{
	struct RtlAdapter *a = driver->private;
	unsigned int value, hash[2] = { 0, 0 };
	int ret;

	if (!a->active)
		return RTL_ERROR(4);
	ksceKernelAtomicAddAndGet32(&a->references, 1);
	ksceKernelLockFastMutex(&a->mutex);
	if ((!addresses && length) || (addresses && length % 6)) {
		ret = RTL_ERROR(1);
		goto out;
	}
	ret = rtlReadRegister(a, 0xc010, 0x100, 4, &value);
	if (ret < 0)
		goto out;
	ret = rtlWriteRegister(a, 0xc010, 0x10f, 4,
		addresses ? value | 4 : value & ~4);
	if (ret < 0 || !addresses)
		goto out;
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
	ret = rtlWriteRegister(a, 0xcd00, 0x10f, 4, __builtin_bswap32(hash[1]));
	if (ret >= 0)
		ret = rtlWriteRegister(a, 0xcd04, 0x10f, 4, __builtin_bswap32(hash[0]));
out:
	ksceKernelUnlockFastMutex(&a->mutex);
	ksceKernelAtomicSubAndGet32(&a->references, 1);
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
		a->driver.notify(&a->driver, a->driver.notify, NET_DRIVER_EVENT_RESUMED, 0, NULL, 0);
}

/* name unknown */
static int pollPhy(struct RtlAdapter *a, unsigned int *value, int read)
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
		ret = rtlWriteRegister(a, 0xde00, 0x10f, 4, advertisement);
		if (ret < 0)
			return ret;
		ret = pollPhy(a, &value, 0);
		if (ret < 0)
			return ret;
	}
	a->link = 0;
	if (a->flags & 2)
		a->driver.notify(&a->driver, a->driver.notify, NET_DRIVER_EVENT_LINK_DOWN, 0, NULL, 0);
	ret = rtlWriteRegister(a, 0xde00, 0x10f, 4, 0);
	if (ret < 0)
		return ret;
	ret = pollPhy(a, &value, 1);
	if (ret < 0)
		return ret;
	ret = rtlWriteRegister(a, 0xde00, 0x10f, 4, (value & 0xffff) | 0x80008000);
	if (ret < 0)
		return ret;
	ret = pollPhy(a, &value, 0);
	if (ret < 0)
		return ret;
	for (int count = 100; count; count--) {
		ret = rtlWriteRegister(a, 0xde00, 0x10f, 4, 0);
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
	unsigned int reg;
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
	ksceKernelAtomicAddAndGet32(&a->references, 1);
	switch (command) {
	case NET_DRIVER_CTL_SET_LINK:
		ksceKernelLockFastMutex(&a->mutex);
		ret = setLink(a, *value);
		ksceKernelUnlockFastMutex(&a->mutex);
		break;
	case NET_DRIVER_CTL_SET_PROMISCUOUS:
		ksceKernelLockFastMutex(&a->mutex);
		ret = rtlReadRegister(a, 0xc010, 0x100, 4, &reg);
		if (ret >= 0) {
			if (*value == 1) {
				a->macControl |= 1;
				reg |= 1;
			} else {
				a->macControl &= ~1;
				reg &= ~1;
			}
			ret = rtlWriteRegister(a, 0xc010, 0x10f, 4, reg);
		}
		ksceKernelUnlockFastMutex(&a->mutex);
		break;
	case NET_DRIVER_CTL_GET_PROMISCUOUS:
		*value = a->macControl & 1;
		ret = 0;
		break;
	default:
		ret = RTL_ERROR(8);
	}
out:
	/* The original also decrements for GET_LINK and an invalid length. */
	ksceKernelAtomicSubAndGet32(&a->references, 1);
	return ret;
}

/* name unknown */
int rtlUpdateLink(struct RtlAdapter *a)
{
	unsigned int value, partner;
	int ret;

	ksceKernelLockFastMutex(&a->mutex);
	ret = rtlWriteRegister(a, 0xde00, 0x10f, 4, 0x10000);
	if (ret < 0)
		goto out;
	ret = pollPhy(a, &value, 1);
	if (ret < 0)
		goto out;
	if (a->link & 1) {
		if (!(value & 4)) {
			a->link = 0;
			a->driver.notify(&a->driver, a->driver.notify, NET_DRIVER_EVENT_LINK_DOWN, 0, NULL, 0);
		}
		goto out;
	}
	if (!(value & 4))
		goto out;
	ret = rtlWriteRegister(a, 0xde00, 0x10f, 4, 0);
	if (ret < 0)
		goto out;
	ret = pollPhy(a, &value, 1);
	if (ret < 0)
		goto out;
	if (!(value & 0x1000)) {
		a->link = value & 0x2000 ? 0x23 : 0x13;
		if (!(value & 0x100))
			a->link -= 2;
	} else {
		ret = rtlWriteRegister(a, 0xde00, 0x10f, 4, 0x50000);
		if (ret < 0)
			goto out;
		ret = pollPhy(a, &partner, 1);
		if (ret < 0)
			goto out;
		ret = rtlWriteRegister(a, 0xde00, 0x10f, 4, 0x40000);
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
	a->driver.notify(&a->driver, a->driver.notify, NET_DRIVER_EVENT_LINK_UP, 0, NULL, 0);
out:
	ksceKernelUnlockFastMutex(&a->mutex);
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
	ksceKernelLockFastMutex(&rtlAdapter.mutex);
	ret = rtlReadRegister(&rtlAdapter, reg, mask, 4, value);
	ksceKernelUnlockFastMutex(&rtlAdapter.mutex);
	return ret;
}

int SceUsbEtherRtlForDriver_1E0E3E59(unsigned short reg, unsigned short mask,
				  unsigned int value)
{
	int ret;

	ksceKernelLockFastMutex(&rtlAdapter.mutex);
	ret = rtlWriteRegister(&rtlAdapter, reg, mask, 4, value);
	ksceKernelUnlockFastMutex(&rtlAdapter.mutex);
	return ret;
}

int SceUsbEtherRtlForDriver_BAF95A47(const unsigned char *mac)
{
	unsigned int offset = 0, value, bit, word, index;
	int ret;

	ksceKernelLockFastMutex(&rtlAdapter.mutex);
	while (offset < 0xb4) {
		ret = rtlWriteRegister(&rtlAdapter, 0xe81c, 0x101, 4, 0xc0);
		if (ret < 0)
			goto out;
		index = offset >> 1;
		ret = rtlWriteRegister(&rtlAdapter, 0xdd00, 0x10f, 4,
				       index << 16 | 0x80000000);
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
		ret = rtlWriteRegister(&rtlAdapter, 0xe81c, 0x101, 4, 0);
		if (ret < 0)
			goto out;
		if (offset & 1) {
			value = word & 0xff00;
			if (value == 0xff00) {
				ret = writeOtpWord(index & 255, 0x25ff);
				if (ret >= 0)
					ret = writeOtpWord((index + 1) & 255, 0xc0);
				for (unsigned int i = 0; ret >= 0 && i < 3; i++)
					ret = writeOtpWord((index + 2 + i) & 255,
						*(const unsigned short *)(mac + 2 * i));
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
					ret = writeOtpWord((index + 2) & 255, *(const unsigned short *)(mac + 1));
				if (ret >= 0)
					ret = writeOtpWord((index + 3) & 255, *(const unsigned short *)(mac + 3));
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
			ksceKernelPrintf("Unknown Format Type!!\n");
			ret = RTL_ERROR(0x801);
			goto out;
		}
		offset++;
	}
	ret = RTL_ERROR(0x802);
out:
	ksceKernelUnlockFastMutex(&rtlAdapter.mutex);
	return ret;
}

/* name unknown */
static void txDone(int result, int count, void *context)
{
	struct RtlAdapter *a = context;

	(void)count;
	if (result >= 0)
		ksceKernelSetEventFlag(a->workerEvent, 8);
}

/* name unknown */
static void rxDone(int result, int count, void *context)
{
	struct RtlAdapter *a = context;

	if (result >= 0) {
		a->rxCount = count;
		ksceKernelSetEventFlag(a->workerEvent, 0x10);
	}
}

/* name unknown */
int rtlSendBuffer(struct RtlAdapter *a, void *data, unsigned int length)
{
	return ksceUsbdBulkTransfer2(a->txPipe, data, length, txDone, a);
}

/* name unknown */
int rtlReceiveBuffer(struct RtlAdapter *a, void *data, unsigned int length)
{
	return ksceUsbdBulkTransfer2(a->rxPipe, data, length, rxDone, a);
}

/* name unknown */
static int detach(int device)
{
	struct RtlAdapter *a = &rtlAdapter;
	int ret;

	(void)device;
	ret = ksceKernelSetEventFlag(a->controlEvent, 1);
	if (ret < 0)
		return -1;
	__atomic_fetch_and(&a->flags, ~1U, __ATOMIC_SEQ_CST);
	ksceKernelSetEventFlag(a->workerEvent, 0x20);
	a->link = 0;
	return 0;
}

/* name unknown */
static int probe(int device)
{
	SceUsbdDeviceDescriptor *descriptor;
	SceUsbdInterfaceDescriptor *interface;
	void *previous;

	descriptor = ksceUsbdScanStaticDescriptor(device, NULL, 1);
	if (!descriptor)
		return -1;
	if (!((descriptor->idVendor == 0x0bda && descriptor->idProduct == 0x8152) ||
	      (descriptor->idVendor == 0x054c && descriptor->idProduct == 0x08a5)))
		return -1;
	previous = descriptor;
	while ((interface = ksceUsbdScanStaticDescriptor(device, previous, 4))) {
		if (interface->bInterfaceClass == 255)
			return 0;
		previous = interface;
	}
	return -1;
}

/* name unknown */
static int attach(int device)
{
	struct RtlAdapter *a = &rtlAdapter;
	SceUsbdInterfaceDescriptor *interface;
	SceUsbdEndpointDescriptor *endpoint;
	SceUsbdDeviceRequest request;
	unsigned int bits;
	void *previous;
	int ret;

	a->controlPipe = a->rxPipe = a->txPipe = -1;
	ret = ksceKernelClearEventFlag(a->controlEvent, 0);
	if (ret < 0)
		return ret;
	previous = ksceUsbdScanStaticDescriptor(device, NULL, 1);
	if (!previous)
		return -1;
	a->controlPipe = ksceUsbdOpenPipe(device, NULL);
	if (a->controlPipe < 0)
		return -1;
	while ((interface = ksceUsbdScanStaticDescriptor(device, previous, 4))) {
		previous = interface;
		if (interface->bInterfaceClass != 255 || !interface->bNumEndpoints)
			continue;
		for (unsigned int i = 0; i < interface->bNumEndpoints; i++) {
			endpoint = ksceUsbdScanStaticDescriptor(device, previous, 5);
			previous = endpoint;
			/* The original assumes the descriptor exists. */
			if ((endpoint->bEndpointAddress & 31) == 1) {
				a->rxPipe = ksceUsbdOpenPipe(device, endpoint);
				if (a->rxPipe < 0)
					return -1;
				a->rxMaxPacket = endpoint->wMaxPacketSize;
			} else if ((endpoint->bEndpointAddress & 31) == 2) {
				a->txPipe = ksceUsbdOpenPipe(device, endpoint);
				if (a->txPipe < 0)
					return -1;
				a->txMaxPacket = endpoint->wMaxPacketSize;
			}
		}
		previous = interface;
	}
	if (a->controlPipe < 0 || a->rxPipe < 0 || a->txPipe < 0)
		return -1;
	request = (SceUsbdDeviceRequest){ 0, 9, 1, 0, 0 };
	ret = ksceUsbdControlTransfer(a->controlPipe, &request, NULL, rtlControlDone, a);
	if (ret < 0)
		return ret;
	ret = ksceKernelWaitEventFlag(a->controlEvent, 3, SCE_EVENT_WAITOR | SCE_EVENT_WAITCLEAR_PAT, &bits, NULL);
	if (ret < 0)
		return ret;
	request = (SceUsbdDeviceRequest){ 1, 11, 0, 0, 0 };
	ret = ksceUsbdControlTransfer(a->controlPipe, &request, NULL, rtlControlDone, a);
	if (ret < 0)
		return ret;
	ret = ksceKernelWaitEventFlag(a->controlEvent, 3, SCE_EVENT_WAITOR | SCE_EVENT_WAITCLEAR_PAT, &bits, NULL);
	if (ret < 0)
		return ret;
	a->device = device;
	a->txBufferSize = a->rxBufferSize = 0x4000;
	__atomic_fetch_or(&a->flags, 1, __ATOMIC_SEQ_CST);
	ret = ksceKernelSetEventFlag(a->workerEvent, 1);
	return ret < 0 ? ret : 0;
}

/* name unknown */
void rtlPopulateInterface(struct RtlAdapter *a)
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

int module_start(SceSize length, void *arg)
{
	struct RtlAdapter *a = &rtlAdapter;
	unsigned int hardware;
	int ret;

	(void)length;
	(void)arg;
	hardware = ksceSysconGetHardwareInfo();
	if (ksceSblAimgrIsCEX() && ksceSblAimgrIsVITA() && !(hardware & 0x80))
		return 0;
	memset(a, 0, sizeof(*a));
	ret = ksceKernelInitializeFastMutex(&a->mutex, "SceUsbEtherRtlControl", 0x2000, 0);
	if (ret < 0)
		return ret;
	ret = ksceKernelCreateEventFlag("SceUsbEtherRtlWorker", 0, 0, NULL);
	if (ret < 0)
		return ret;
	a->workerEvent = ret;
	ret = ksceKernelCreateThread("SceNetDrvEther", rtlWorker, 0x3e, 0x2000, 0, 0xf0000, NULL);
	if (ret < 0)
		return ret;
	a->thread = ret;
	ret = ksceKernelStartThread(ret, 4, &a);
	if (ret < 0)
		return ret;
	ret = ksceKernelRegisterSysEventHandler("SceUsbEtherRtl", systemEvent, a);
	if (ret < 0)
		return ret;
	ret = ksceKernelCreateEventFlag("SceRtlControlTransfer", 0, 0, NULL);
	if (ret < 0)
		return ret;
	a->controlEvent = ret;
	usbDriver.name = "SceUsbEtherRtl";
	usbDriver.probe = probe;
	usbDriver.attach = attach;
	usbDriver.detach = detach;
	ret = ksceUsbdRegisterDriver(&usbDriver);
	return ret < 0 ? ret : 0;
}
