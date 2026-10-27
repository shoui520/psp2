/* RTL8152 worker; all private function names are unknown. */
#include "rtl_internal.h"
#include "phy_patch.h"
#include <kernel/error.h>

/* power.c's definition takes a third argument, passed back to the callback; Sony's USB
 * Ethernet drivers call it with two and leave that one unset */
int ScePowerForDriver_B24263F0(int kind, void (*handler)(int event, int value));

/* name unknown */
static void drainReferences(struct RtlAdapter *a)
{
	a->active = 0;
	for (int count = 3000; count && a->references; count--)
		sceKernelDelayThread(1000);
}

/* name unknown */
static int resetReceive(struct RtlAdapter *a)
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
	static const struct {
		unsigned short reg, mask;
		unsigned int value;
	} settings[] = {
		{ 0xfc24, 0xcc, 0x80000000 },
		{ 0xfc28, 0xff, 0x06e9170b },
		{ 0xfc2c, 0xff, 0x13870989 },
		{ 0xfc30, 0xff, 0x061d01b7 },
		{ 0xe420, 0xcc, 0x00200000 },
		{ 0xe420, 0x33, 0x00000018 },
		{ 0xfc34, 0xcc, 0x047b0000 },
	};

	ret = rtlWritePla(a, 0xf800, 0xff, sizeof(plaPhyPatch), plaPhyPatch);
	if (ret < 0)
		return ret;
	for (unsigned int i = 0; i < sizeof(settings) / sizeof(settings[0]); i++) {
		value = settings[i].value;
		ret = rtlWritePla(a, settings[i].reg, settings[i].mask, 4, &value);
		if (ret < 0)
			return ret;
	}
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
	static const struct {
		unsigned short reg, mask;
		unsigned int value;
	} settings[] = {
		{ 0xc0a0, 0x10f, 0x00080002 },
		{ 0xc0a4, 0x103, 0x00000038 },
		{ 0xc0a8, 0x103, 0x00000048 },
		{ 0xe618, 0x10f, 0x00400008 },
		{ 0xe610, 0x10f, 0x4f000880 },
		{ 0xd408, 0x004, 0x00030000 },
		{ 0xd40c, 0x00f, 0xfde80600 },
		{ 0xd404, 0x00f, 0x80001e11 },
	};

	if (SceSblAIMgrForDriver_C6E83F34() && !SceSysrootForDriver_8AA268D6(0xc)) {
		SceGpioForDriver_372022A4(1, 2, 0);
		sceGpioPortClear(1, 2);
	}
	for (unsigned int reg = 0xfc28; reg <= 0xfc34; reg += 4) {
		ret = rtlWritePla(a, reg, 0xff, 4, &value);
		if (ret < 0)
			return ret;
	}
	for (unsigned int reg = 0xfc28; reg <= 0xfc34; reg += 4) {
		ret = rtlWriteUsb(a, reg, 0xff, 4, &value);
		if (ret < 0)
			return ret;
	}
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
	for (unsigned int i = 0; i < sizeof(settings) / sizeof(settings[0]); i++) {
		ret = rtlWriteRegister(a, settings[i].reg, settings[i].mask, 4, settings[i].value);
		if (ret < 0)
			return ret;
	}
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
static int allocateBuffer(const char *name, unsigned int length, void **base)
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
	unsigned char byte;
	int ret;

	for (int count = 10; !(a->flags & 1); count--) {
		if (!count) {
			ret = RTL_ERROR(7);
			goto error;
		}
		sceKernelDelayThread(1000000);
	}
	if ((a->flags & 4) && (a->link & 1)) {
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

int rtlWorker(SceSize length, void *arg)
{
	struct RtlAdapter *a = *(struct RtlAdapter **)arg;
	SceUInt64 lastPoll = 0, elapsed = 0, now;
	unsigned int bits, timeout = 0;
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
		if (a->flags & 4)
			timeout = 3000000;
		ret = sceKernelWaitEventFlag(a->workerEvent, 0x7f,
					      SCE_KERNEL_EVF_WAITMODE_OR |
					      SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &bits,
					    a->flags & 4 ? &timeout : NULL);
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
