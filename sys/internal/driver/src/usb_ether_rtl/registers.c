/* RTL8152 register-access helpers; all private function names are unknown. */
#include "rtl_internal.h"

/* Sony's 4.00 build is fortified */
#if FW_SINCE(0x04000000)
#include <sysmem/fortify.h>
#endif

static unsigned int readValue __attribute__((aligned(64)));
static unsigned int writeValue __attribute__((aligned(64)));
static unsigned int wakeValue __attribute__((aligned(64)));
static unsigned char plaReadData[64] __attribute__((aligned(64)));
static unsigned char plaWriteData[512] __attribute__((aligned(64)));
static unsigned char usbData[32] __attribute__((aligned(64)));
static SceUsbdDeviceRequest readRequest __attribute__((aligned(64)));
static SceUsbdDeviceRequest writeRequest __attribute__((aligned(64)));
static SceUsbdDeviceRequest wakeRequest __attribute__((aligned(64)));
static SceUsbdDeviceRequest plaReadRequest __attribute__((aligned(64)));
static SceUsbdDeviceRequest plaWriteRequest __attribute__((aligned(64)));
static SceUsbdDeviceRequest usbRequest __attribute__((aligned(64)));

void rtlControlDone(int result, int count, void *context)
{
	struct RtlAdapter *a = context;

	(void)count;
	sceKernelSetEventFlag(a->controlEvent, result < 0 ? 1 : 2);
}

/* name unknown */
static int transfer(struct RtlAdapter *a, SceUsbdDeviceRequest *request, void *data)
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

int rtlWakeFeature(struct RtlAdapter *a)
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
	unsigned int offset = reg & 3;
	unsigned int mask = offset == 3 ? 0x81 : 0x33 << offset;

	*(unsigned short *)((unsigned char *)data + offset) = value;
	return rtlWritePla(a, reg & 0xfffc, mask, offset == 3 ? 8 : 4, data);
}

int rtlWritePla32(struct RtlAdapter *a, unsigned int reg, unsigned int value)
{
	unsigned int data[2] = { 0, 0 };
	unsigned int offset = reg & 3;
	unsigned int mask = offset ? 15 << offset : 0xff;

	*(unsigned int *)((unsigned char *)data + offset) = value;
	return rtlWritePla(a, reg & 0xfffc, mask, offset ? 8 : 4, data);
}

int rtlReadUsb(struct RtlAdapter *a, unsigned short reg, unsigned int length, void *data)
{
	int ret;

	if (length > 32)
		return RTL_ERROR(1);
	request(&usbRequest, 0xc0, reg, 0, length);
	ret = transfer(a, &usbRequest, usbData);
	if (ret >= 0)
		memcpy(data, usbData, length);
	return ret;
}

int rtlReadUsb8(struct RtlAdapter *a, unsigned int reg, unsigned char *value)
{
	unsigned int data;
	int ret;

	request(&usbRequest, 0xc0, reg & 0xfffc, 0, 4);
	ret = transfer(a, &usbRequest, usbData);
	if (ret >= 0) {
		data = *(unsigned int *)usbData;
		*value = *((unsigned char *)&data + (reg & 3));
	}
	return ret;
}

int rtlWriteUsb(struct RtlAdapter *a, unsigned short reg, unsigned short mask,
		 unsigned int length, const void *data)
{
	if (length > 32)
		return RTL_ERROR(1);
	request(&usbRequest, 0x40, reg, mask, length);
	memcpy(usbData, data, length);
	return transfer(a, &usbRequest, usbData);
}

int rtlWriteUsb8(struct RtlAdapter *a, unsigned int reg, unsigned char value)
{
	unsigned int data = 0;

	*((unsigned char *)&data + (reg & 3)) = value;
	*(unsigned int *)usbData = data;
	request(&usbRequest, 0x40, reg & 0xfffc, 0x11 << (reg & 3), 4);
	return transfer(a, &usbRequest, usbData);
}

int rtlWriteUsb16(struct RtlAdapter *a, unsigned int reg, unsigned short value)
{
	unsigned int data[2] = { 0, 0 };
	unsigned int offset = reg & 3;
	unsigned int mask = offset == 3 ? 0x81 : offset == 2 ? 0x0c : 0x33 << offset;
	unsigned int length = offset == 3 ? 8 : 4;

	*(unsigned short *)((unsigned char *)data + offset) = value;
	request(&usbRequest, 0x40, reg & 0xfffc, mask, length);
	memcpy(usbData, data, length);
	return transfer(a, &usbRequest, usbData);
}

int rtlWriteUsb32(struct RtlAdapter *a, unsigned int reg, unsigned int value)
{
	unsigned int data[2] = { 0, 0 };
	unsigned int offset = reg & 3;
	unsigned int mask = offset ? 15 << offset : 0xff;
	unsigned int length = offset ? 8 : 4;

	*(unsigned int *)((unsigned char *)data + offset) = value;
	request(&usbRequest, 0x40, reg & 0xfffc, mask, length);
	memcpy(usbData, data, length);
	return transfer(a, &usbRequest, usbData);
}
