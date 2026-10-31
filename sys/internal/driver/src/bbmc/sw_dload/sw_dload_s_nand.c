/*
 * The modem software download module (SceBbmcSwdlMod): the Qualcomm DLOAD/streaming
 * download over the modem's USB serial port, the Intel HEX images it sends, and the
 * thread that runs it.
 */
#include <stddef.h>
#include <string.h>
#include <sysmem/cpu.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysmem.h>
#include <iofilemgr/iofilemgr.h>
#include <sysmem/debug.h>
#include <threadmgr/threadmgr.h>
#include "sw_dload.h"
#include "../usb_serial.h"
#include "../updater/updater.h"

/* the 8 bytes an erased version reads as, and the version of an empty slot */
static const SceUInt8 swdlErasedVersion[8] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};
static const char swdlZeroVersion[] = "000000";

/* the HDLC frame check sequence (CRC-16/X.25, reflected 0x8408) */
static const SceUInt16 swdlCrcTable[256] = {
	0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
	0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
	0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
	0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
	0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
	0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
	0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
	0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
	0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
	0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
	0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
	0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
	0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
	0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
	0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
	0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
	0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
	0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
	0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
	0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
	0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
	0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
	0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
	0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
	0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
	0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
	0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
	0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
	0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
	0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
	0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
	0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78,
};

/* name unknown: the frame check sequence of a frame's bytes */
SceUInt16 swdlCalculateCrc(const SceUInt8 *data, SceSize size)
{
	const SceUInt8 *end;
	SceUInt32 crc;

	if (size == 0)
		return 0;
	end = data + size;
	crc = 0xffff;
	do
		crc = swdlCrcTable[(SceUInt8)(*data++ ^ crc)] ^ (crc >> 8);
	while (data != end);
	return (SceUInt16)~crc;
}

/* name unknown: make sure the frame buffer holds at least 140 bytes */
__attribute__((noinline)) int swdlAllocateFrameBuffer(SwdlContext *context)
{
	if (context->bufferCapacity > 139)
		return 0;
	if (context->buffer != NULL)
		sceKernelFreeHeapMemory(context->heapId, context->buffer);
	context->buffer = NULL;
	context->bufferLength = 0;
	context->bufferCapacity = 0;
	context->buffer = sceKernelAllocHeapMemory(context->heapId, 140);
	if (context->buffer == NULL) {
		context->bufferLength = 0;
		context->bufferCapacity = 0;
		return 0x80330005;
	}
	context->bufferCapacity = 140;
	return 0;
}

/* name unknown: free the frame buffer and the list */
__attribute__((noinline)) int swdlReleaseBuffers(SwdlContext *context)
{
	SwdlListNode *node;
	SwdlListNode *next;

	if (context->buffer != NULL) {
		sceKernelFreeHeapMemory(context->heapId, context->buffer);
		context->buffer = NULL;
		context->bufferLength = 0;
		context->bufferCapacity = 0;
	}
	sceKernelSpinlockLowLock(&context->lock);
	node = context->list;
	while (node != NULL) {
		next = node->next;
		if (node->data != NULL)
			sceKernelFreeHeapMemory(context->heapId, node->data);
		if (node->extra != NULL)
			sceKernelFreeHeapMemory(context->heapId, node->extra);
		sceKernelFreeHeapMemory(context->heapId, node);
		node = next;
	}
	context->listCount = 0;
	context->list = NULL;
	sceKernelSpinlockLowUnlock(&context->lock);
	return 0;
}

/* name unknown: make sure the frame buffer holds at least size bytes */
__attribute__((noinline)) int swdlReserveFrameBuffer(SwdlContext *context, SceSize size)
{
	if (context->bufferCapacity >= size)
		return 0;
	if (context->buffer != NULL)
		sceKernelFreeHeapMemory(context->heapId, context->buffer);
	context->buffer = NULL;
	context->bufferLength = 0;
	context->bufferCapacity = 0;
	context->buffer = sceKernelAllocHeapMemory(context->heapId, size);
	if (context->buffer == NULL) {
		context->bufferLength = 0;
		size = 0;
		context->bufferCapacity = size;
		return 0x80330005;
	}
	context->bufferCapacity = size;
	return 0;
}

/* name unknown: seek the image file to offset */
__attribute__((noinline)) int swdlSeekFile(SceUID fd, SceUInt32 offset)
{
	return udIoLseek(fd, offset, SCE_SEEK_SET);
}

/* name unknown: start reading the image file as Intel HEX, into the frame buffer after
 * its 8-byte header, size bytes at most */
__attribute__((noinline)) int swdlOpenHexReader(SwdlContext *context, SceSize size)
{
	SwdlHexReader *reader = &context->hexReader;
	SceSize capacity;
	SceSize needed;
	SceUInt8 *data;
	int result;

	if (size > 0xfffffffb) {
		result = 0x80330002;
		goto fail;
	}
	needed = size + 4;
	memset(reader, 0, sizeof(*reader));
	if (needed > context->bufferCapacity) {
		if (context->buffer != NULL)
			sceKernelFreeHeapMemory(context->heapId, context->buffer);
		context->buffer = NULL;
		context->bufferLength = 0;
		context->bufferCapacity = 0;
		context->buffer = sceKernelAllocHeapMemory(context->heapId, needed);
		if (context->buffer == NULL) {
			result = 0x80330005;
			context->bufferLength = 0;
			context->bufferCapacity = 0;
			goto fail;
		}
		context->bufferCapacity = needed;
	}
	reader->line = sceKernelAllocHeapMemory(context->heapId, 522);
	if (reader->line == NULL) {
		result = 0x80330005;
		goto fail;
	}
	capacity = context->bufferCapacity;
	data = context->buffer;
	reader->fd = context->update.fd;
	if (capacity > 6) {
		reader->data = data + 8;
		reader->capacity = capacity - 7;
		return 0;
	}
	result = 0x80330002;
	sceKernelFreeHeapMemory(context->heapId, reader->line);
	memset(reader, 0, sizeof(*reader));
fail:
	memset(reader, 0, sizeof(*reader));
	return result;
}

/* name unknown: stop reading the image file */
__attribute__((noinline)) int swdlCloseHexReader(SwdlContext *context)
{
	if (context->hexReader.line != NULL)
		sceKernelFreeHeapMemory(context->heapId, context->hexReader.line);
	memset(&context->hexReader, 0, sizeof(context->hexReader));
	return 0;
}

/* the value of a hex digit, or -1 */
static inline int swdlHexDigit(SceUInt8 c)
{
	if ((SceUInt8)(c - '0') <= 9)
		return c - '0';
	if ((SceUInt32)(c - 'A') <= 5)
		return c - 'A' + 10;
	if ((SceUInt32)(c - 'a') <= 5)
		return c - 'a' + 10;
	return -1;
}

/* name unknown: the next HEX records whose data follow each other, into the reader's
 * data; stops before a record that does not fit or changes the address */
__attribute__((noinline)) int swdlReadHexRecords(SwdlHexReader *reader)
{
	SceUInt8 *line = reader->line;
	SceSize length = 0;
	SceSize capacity;
	SceUInt32 count;
	SceUInt32 type;
	SceUInt32 sum;
	SceUInt32 i;
	int high;
	int low;
	int result;

	reader->length = 0;
	reader->consumed = 0;
	for (;;) {
		capacity = reader->capacity;
		do {
			result = udIoRead(reader->fd, line, 1);
			if (result < 0)
				return result;
			if (result == 0)
				return 0x80330701;
			line = reader->line;
			reader->consumed += result;
		} while (*line != ':');

		/* byte count, address and type */
		result = udIoRead(reader->fd, line, 8);
		if (result < 0)
			return result;
		if ((SceUInt32)result <= 7)
			return 0x80330701;
		line = reader->line;
		reader->consumed += result;
		for (i = 0; i < 4; i++) {
			high = swdlHexDigit(reader->line[2 * i]);
			if (high < 0)
				return 0x80330002;
			low = swdlHexDigit(reader->line[2 * i + 1]);
			if (low < 0)
				return 0x80330002;
			reader->line[i] = high << 4 | low;
		}
		line = reader->line;
		count = line[0];
		type = line[3];
		sum = line[0] + line[1] + line[2] + line[3];
		if (reader->length == 0)
			reader->address = line[1] << 8 | line[2];
		if ((type == 0 && capacity - length < count) ||
		    ((type == 4 || type == 5) && reader->length != 0)) {
			/* leave the record for the next call */
			result = udIoLseek(reader->fd, -9, SCE_SEEK_CUR);
			return result < 0 ? result : 0;
		}

		/* data and checksum */
		result = udIoRead(reader->fd, line, 2 * (count + 1));
		if (result < 0)
			return result;
		if ((SceUInt32)result < 2 * (count + 1))
			return 0x80330701;
		reader->consumed += result;
		for (i = 0; i < count + 1; i++) {
			line = reader->line;
			high = swdlHexDigit(line[2 * i]);
			if (high < 0)
				return 0x80330002;
			low = swdlHexDigit(line[2 * i + 1]);
			if (low < 0)
				return 0x80330002;
			line[i] = high << 4 | low;
			sum += high << 4 | low;
		}
		if ((SceUInt8)sum != 0)
			return 0x80330701;

		switch (type) {
		case 0:
			memcpy(reader->data + reader->length, reader->line, count);
			line = reader->line;
			length = reader->length + count;
			reader->length = length;
			break;
		case 1:
			reader->endOfFile = 1;
			/* read the file to its end */
			for (;;) {
				result = udIoRead(reader->fd, reader->line, 522);
				if (result < 0)
					return result;
				reader->consumed += result;
				if (result != 522)
					return 0;
			}
		case 4:
			length = reader->length;
			line = reader->line;
			reader->hasUpperAddress = 1;
			reader->upperAddress = line[0] << 8 | line[1];
			break;
		case 5:
			length = reader->length;
			line = reader->line;
			reader->startAddress = line[0] << 8;
			reader->startAddress = (reader->startAddress | line[1]) << 8;
			reader->startAddress = (reader->startAddress | line[2]) << 8;
			reader->hasStartAddress = 1;
			reader->startAddress |= line[3];
			break;
		default:
			return 0x80330701;
		}
	}
}

/* name unknown: read *size bytes at offset of the image file; *size becomes what was read */
__attribute__((noinline)) int swdlReadFileAt(SceUID fd, void *data, SceUInt32 offset, SceSize *size)
{
	int result;

	result = udIoLseek(fd, offset, SCE_SEEK_SET);
	if (result < 0)
		return result;
	result = udIoRead(fd, data, *size);
	if (result < 0)
		return result;
	*size = result;
	return 0;
}

/* name unknown: end an HDLC frame: its check sequence and the flag byte 0x7e */
__attribute__((noinline)) int swdlEndFrame(const SceUInt8 *data, SceSize *size, SceUInt8 *trailer)
{
	SceUInt16 crc = swdlCalculateCrc(data, *size);

	trailer[1] = crc >> 8;
	trailer[0] = crc;
	trailer[2] = 0x7e;
	*size += 3;
	return crc & 0xff;
}

/* name unknown: the version an empty slot reports */
__attribute__((noinline)) void swdlGetZeroVersion(const void **version, SceSize *size)
{
	*version = swdlZeroVersion;
	*size = 6;
}

/* name unknown: the version an erased slot reads as */
__attribute__((noinline)) void swdlGetErasedVersion(const void **version, SceSize *size)
{
	*version = swdlErasedVersion;
	*size = 8;
}

/* restart the frame timer (timerFlags 0x400, frameTime in ms) */
static inline int swdlStartFrameTimer(SwdlContext *context)
{
	SceUInt64 now;
	int result;

	result = SceThreadmgrForDriver_419E6736(&now);
	if (result < 0) {
		context->timerFlags &= ~0x400;
		return result;
	}
	context->timerFlags |= 0x400;
	context->frameTime = (SceUInt32)now / 1000;
	return result;
}

/*
 * name unknown: send what the sequence has to send: transmitState 1, 2, 5 and 7 ask the
 * step for a frame, 3 sends the last one again; 0, 4 and 6 wait for a reply.
 */
__attribute__((noinline)) int swdlSendFrames(SwdlContext *context)
{
	SwdlPortInterface *port = context->port;
	SceUInt32 state = context->transmitState;
	SceUInt32 stepped;
	int result = 0;
	int step;

	for (;;) {
		if (state > 7)
			return 0x80330002;
		if ((1 << state) & 0x51)
			goto out;
		step = (1 << state) & 0xa6;
		if (step) {
			context->transmitData = NULL;
			context->transmitLength = 0;
			step = context->steps[context->stepIndex].send(context);
			if (step < 0)
				return step;
			if (context->transmitState == 5) {
				context->bufferLength = 0;
				stepped = 1;
				goto stepped;
			}
			if (context->bufferLength == 0) {
				stepped = 1;
				goto stepped;
			}
			context->transmitData = context->buffer;
			context->transmitLength = context->bufferLength;
			stepped = 1;
		} else {
			if (!((1 << state) & 8) || context->transmitLength == 0)
				return 0x80330002;
			stepped = 0;
		}

		result = port->send(port, &context->transmitData, &context->transmitLength, 0);
		if (result < 0) {
			if (result == (int)0x80330005 && context->sendRetries <= 9) {
				if (context->sendRetries == 0)
					context->savedTransmitState = context->transmitState;
				context->sendRetries++;
				result = 0;
				context->transmitState = 3;
			}
			return result;
		}
		if (context->sendRetries > 0) {
			context->transmitState = context->savedTransmitState;
			context->sendRetries = 0;
		}
		context->bufferLength = 0;
		context->transmitData = NULL;
		context->transmitLength = 0;
		if (context->transmitState != 7) {
			result = swdlStartFrameTimer(context);
			if (result < 0)
				return result;
		}
		if (stepped != 1)
			goto reload;
stepped:
		if (step == 2) {
			context->transmitState = stepped;
			context->stepIndex--;
			state = stepped;
			continue;
		}
		if (step == 1) {
			context->transmitState = 1;
			context->stepIndex++;
			state = 1;
			continue;
		}
		result = step;
reload:
		state = context->transmitState;
out:
		if (state == 2)
			sceKernelSetEventFlag(context->eventFlagId, 256);
		return result;
	}
}

/* name unknown: take a frame from the port (waiting when received is given) and give it
 * to the step; a frame with a bad check sequence is asked for again twice */
__attribute__((noinline)) int swdlReceiveFrame(SwdlContext *context, int *received)
{
	SwdlPortInterface *port = context->port;
	int wait = received != NULL;
	SceUInt32 crc;
	SceUInt8 *data;
	SceUInt8 *end;
	int result;
	int timer;

	context->timerFlags &= ~0x400;
	context->timeouts = 0;
	context->receiveLength = sizeof(context->receiveBuffer);
	result = port->receive(port, context->receiveBuffer, &context->receiveLength, wait);
	if (result < 0)
		return result;
	if (wait)
		*received = result;
	result = context->receiveLength;
	if (result == 0)
		return result;

	data = context->receiveBuffer;
	end = data + result;
	crc = 0xffff;
	do
		crc = swdlCrcTable[(SceUInt8)(*data++ ^ crc)] ^ (crc >> 8);
	while (data != end);
	if (crc != 0xf0b8) {
		if (context->crcRetries > 1)
			return 0x80330700;
		context->crcRetries++;
		context->transmitState = 3;
		return 0;
	}

	context->crcRetries = 0;
	context->receiveEvent = 512;
	result = context->steps[context->stepIndex].receive(context);
	if (result < 0)
		return result;
	if (result == 2) {
		context->transmitState = 1;
		context->stepIndex--;
	} else if (result == 1) {
		context->transmitState = 1;
		context->stepIndex++;
	} else if (context->transmitState == 4) {
		timer = swdlStartFrameTimer(context);
		if (timer < 0)
			result = timer;
	}
	return result;
}

/* name unknown: no reply in time: tell the step, ten times at most */
__attribute__((noinline)) int swdlHandleFrameTimeout(SwdlContext *context)
{
	int result;

	context->receiveEvent = 1024;
	context->transmitState = 0;
	context->timeouts++;
	if (context->timeouts > 10)
		return 0x80330706;
	result = context->steps[context->stepIndex].receive(context);
	if (result < 0)
		return result;
	if (result == 2) {
		context->transmitState = 1;
		context->stepIndex--;
	} else if (result == 1) {
		context->transmitState = 1;
		context->stepIndex++;
	} else if (context->transmitState == 4) {
		result = swdlStartFrameTimer(context);
	}
	return result;
}

/* name unknown: the download was cancelled: tell the step */
__attribute__((noinline)) int swdlHandleCancel(SwdlContext *context)
{
	int result;

	context->receiveEvent = 2048;
	result = context->steps[context->stepIndex].receive(context);
	if (result < 0)
		return result;
	if (result == 2) {
		context->transmitState = 1;
		context->stepIndex--;
	} else if (result == 1) {
		context->transmitState = 1;
		context->stepIndex++;
	} else if (context->transmitState == 4) {
		result = swdlStartFrameTimer(context);
	}
	return result;
}

/* start sending the current step's frames */
__attribute__((noinline)) int swdlStartSend(SwdlContext *context)
{
	context->transmitState = 1;
	return swdlSendFrames(context);
}

/* take size bytes from *cursor into data (skip them when data is NULL) */
__attribute__((noinline)) void swdlUnpackBytes(SceUInt8 *data, const SceUInt8 **cursor, SceSize size)
{
	SceUInt8 *end;

	if (data == NULL) {
		*cursor += size;
		return;
	}
	if (size == 0)
		return;
	end = data + size;
	do
		*data++ = *(*cursor)++;
	while (data != end);
}

__attribute__((noinline)) int swdlSetResponseTimeout(SwdlContext *context, SceUInt32 timeout)
{
	context->responseTimeout = timeout;
	return 0;
}

/* the largest packet the port takes */
__attribute__((noinline)) int swdlGetPortPacketSize(SwdlContext *context, SceSize *size)
{
	SwdlPortInterface *port = context->port;

	if (size == NULL)
		return (int)0x80330002;
	return port->control(port, 0, size, sizeof(*size));
}

/* the packets the port can still queue */
__attribute__((noinline)) int swdlGetPortFreePackets(SwdlContext *context, SceUInt32 *count)
{
	SwdlPortInterface *port = context->port;

	if (count == NULL)
		return (int)0x80330002;
	return port->control(port, 1, count, sizeof(*count));
}

/* the time in ms */
__attribute__((noinline)) int swdlGetTime(SceUInt32 *time)
{
	SceUInt64 now;
	int result;

	if (time == NULL)
		return (int)0x80330002;
	result = SceThreadmgrForDriver_419E6736(&now);
	if (result >= 0)
		*time = (SceUInt32)now / 1000;
	return result;
}

/* 1 while no more than timeout ms (0: no limit) passed since start */
__attribute__((noinline)) int swdlIsWithinTimeout(SceUInt32 start, SceUInt32 timeout)
{
	SceUInt64 now;
	SceUInt64 elapsed;
	SceUInt32 time;
	int result;

	result = SceThreadmgrForDriver_419E6736(&now);
	if (result < 0)
		return result;
	time = (SceUInt32)now / 1000;
	if (start > time)
		elapsed = (SceUInt64)time + 0xffffffff - start;
	else
		elapsed = time - start;
	if (timeout == 0)
		return 1;
	return timeout >= elapsed;
}

/* the port's last error */
__attribute__((noinline)) int swdlGetPortError(SwdlPortInterface *port)
{
	int error;
	int result;

	result = port->control(port, 5, &error, sizeof(error));
	if (result >= 0)
		result = error;
	return result;
}

/* hand the attach event to the current step */
__attribute__((noinline)) int swdlHandleAttach(SwdlContext *context)
{
	int result;

	context->receiveEvent = 0x20000;
	result = context->steps[context->stepIndex].receive(context);
	if (result == 1) {
		context->transmitState = 1;
		context->stepIndex++;
	}
	return result;
}

/* hand the detach event to the current step */
__attribute__((noinline)) int swdlHandleDetach(SwdlContext *context)
{
	int result;

	context->receiveEvent = 0x10000;
	result = context->steps[context->stepIndex].receive(context);
	if (result == 1) {
		context->transmitState = 1;
		context->stepIndex++;
	}
	return result;
}

/* 1 when id is one of the IDs the target reports */
__attribute__((noinline)) int swdlHasId(SwdlContext *context, SceUInt16 id)
{
	SceUInt32 i;

	for (i = 0; i < context->idCount; i++) {
		if (context->ids[i] == id)
			return 1;
	}
	return 0;
}

/* append copies of data and extra to the context's list */
__attribute__((noinline)) int swdlAppendListNode(SwdlContext *context, const void *data, SceSize dataSize,
		       const void *extra, SceSize extraSize)
{
	SwdlListNode *last;
	SwdlListNode *node;
	int result;

	if (context == NULL || data == NULL || dataSize == 0 || extra == NULL ||
	    extraSize == 0) {
		sceKernelSpinlockLowUnlock(&context->lock);
		return (int)0x80330002;
	}
	sceKernelSpinlockLowLock(&context->lock);
	last = context->list;
	if (last != NULL) {
		while (last->next != NULL)
			last = last->next;
	}
	node = sceKernelAllocHeapMemory(context->heapId, sizeof(*node));
	if (node == NULL) {
		result = (int)0x80330005;
		goto unlock;
	}
	memset(node, 0, sizeof(*node));
	node->data = sceKernelAllocHeapMemory(context->heapId, dataSize);
	if (node->data == NULL) {
		result = (int)0x80330005;
		sceKernelFreeHeapMemory(context->heapId, node);
		goto unlock;
	}
	node->extra = sceKernelAllocHeapMemory(context->heapId, extraSize);
	if (node->extra == NULL) {
		result = (int)0x80330005;
		sceKernelFreeHeapMemory(context->heapId, node->data);
		sceKernelFreeHeapMemory(context->heapId, node);
		goto unlock;
	}
	memcpy(node->data, data, dataSize);
	node->dataSize = dataSize;
	memcpy(node->extra, extra, extraSize);
	node->extraSize = extraSize;
	if (last != NULL)
		last->next = node;
	else
		context->list = node;
	result = 0;
unlock:
	sceKernelSpinlockLowUnlock(&context->lock);
	return result;
}

/* name unknown: the thread's handler for each state (context->state) */
int swdlExecIdle(SwdlContext *context, SceUInt32 events);
int swdlExecBackup(SwdlContext *context, SceUInt32 events);
int swdlExecUpdate(SwdlContext *context, SceUInt32 events);
int swdlExecDownload(SwdlContext *context, SceUInt32 events);
int swdlExecRestore(SwdlContext *context, SceUInt32 events);
int swdlExecModeChange(SwdlContext *context, SceUInt32 events);

static int (*const swdlExecHandlers[6])(SwdlContext *context, SceUInt32 events) = {
	swdlExecIdle,
	swdlExecBackup,
	swdlExecUpdate,
	swdlExecDownload,
	swdlExecRestore,
	swdlExecModeChange,
};

/* the download thread: sets the state from the request events, runs the attach and
 * detach waits and the timers, and hands every event to the state's handler */
int swdlThread(SceSize args, void *argp)
{
	SwdlContext *context = *(SwdlContext **)argp;
	SceUInt32 timeout = 1000000;
	SceUInt32 events;
	SceUInt32 flags;
	SceUInt32 bit;
	SceUInt32 start;
	SceUInt32 limit;
	SceUInt32 now;
	int result = 0;
	int error;
	int within;

	for (;;) {
		events = 0;
		error = sceKernelWaitEventFlag(context->eventFlagId, 0xffffff, 1, &events,
					       &timeout);
		if (error < 0) {
			if (error != (int)0x80028005)
				return error;
			flags = context->timerFlags;
			if (flags != 0) {
				error = swdlGetTime(&now);
				while (flags != 0 && error == 0) {
					if (flags & 0x40000) {
						flags &= ~0x40000;
						bit = 0x40000;
						start = context->attachTime;
						limit = context->attachTimeout;
					} else if (flags & 0x80000) {
						flags &= ~0x80000;
						bit = 0x80000;
						start = context->attachTime;
						limit = context->attachTimeout;
					} else if (flags & 0x400) {
						flags &= ~0x400;
						bit = 0x400;
						start = context->frameTime;
						limit = 3000;
					} else {
						error = (int)0x80330002;
						goto fail;
					}
					within = swdlIsWithinTimeout(start, limit);
					if (within < 0) {
						error = within;
						goto fail;
					}
					if (within == 0) {
						sceKernelSetEventFlag(context->eventFlagId, bit);
						context->timerFlags &= ~bit;
					}
				}
				if (error < 0) {
fail:
					context->lastError = error;
					sceKernelSetEventFlag(context->eventFlagId, 0x100000);
				}
			}
			timeout = 1000000;
			continue;
		}
		error = sceKernelClearEventFlag(context->eventFlagId, ~events);
		if (error < 0)
			return error;
		if (events & 1)
			return result;

		if (events & 2)
			context->state = 1;
		if (events & 0x600004) {
			context->updateState = 0;
			context->abortState = 0;
			context->state = 2;
		}
		if (events & 8)
			context->state = 3;
		if (events & 0x20)
			context->state = 4;
		if (events & 0x800000)
			context->state = 5;
		if (events & 0x40)
			context->state = 0;
		if ((events & 0x80) && context->state != 2)
			context->state = 0;

		/* the attach and detach waits */
		if (events & 0x4000) {
			context->waiting = 1;
			if (context->attached == 1) {
				context->notify(256, &context->notification);
				context->waiting = 0;
			} else if (swdlGetTime(&context->attachTime) < 0) {
				context->attached = 0;
				context->waiting = 0;
				context->timerFlags &= ~0x40000;
				context->notify(1024, &context->notification);
			} else {
				context->timerFlags |= 0x40000;
			}
		}
		if (events & 0x8000) {
			context->waiting = 2;
			if (context->attached == 0) {
				context->notify(512, &context->notification);
				context->waiting = 0;
			} else if (swdlGetTime(&context->attachTime) < 0) {
				context->attached = 0;
				context->waiting = 0;
				context->timerFlags &= ~0x80000;
				context->notify(2048, &context->notification);
			} else {
				context->timerFlags |= 0x80000;
			}
		}
		if (events & 0x10000) {
			context->attached = 1;
			if (context->waiting == 1) {
				context->timerFlags &= ~0x40000;
				context->notify(256, &context->notification);
				context->waiting = 0;
			}
		}
		if (events & 0x20000) {
			context->attached = 0;
			if (context->waiting == 2) {
				context->timerFlags &= ~0x80000;
				context->notify(512, &context->notification);
				context->waiting = 0;
			}
		}
		if (events & 0x40000) {
			context->attached = 0;
			if (context->waiting == 1)
				context->notify(1024, &context->notification);
			context->waiting = 0;
		}
		if (events & 0x80000) {
			context->attached = 0;
			if (context->waiting == 2)
				context->notify(2048, &context->notification);
			context->waiting = 0;
		}

		result = swdlExecHandlers[context->state](context, events);
		if (result < 0)
			context->state = 0;
	}
}

/* name unknown: the module's state, and the pointer to it while it is initialized */
SwdlContext swdlContextStorage;
SwdlContext *swdlContext;

/* turn the port's events into the thread's */
int swdlPortEventHandler(SwdlPortInterface *port, SceUInt32 events, void *data, SceSize size)
{
	SceUInt32 pattern;

	if (swdlContext == NULL)
		return (int)0x80330003;
	if (events & 2)
		pattern = 512;
	else if (events & 8)
		pattern = 2048;
	else if (events & 1)
		pattern = 256;
	else if (events & 0x20)
		pattern = 0x2000;
	else if (events & 0x40)
		pattern = 0x10000;
	else if (events & 0x80)
		pattern = 0x20000;
	else if (events & 0x14)
		pattern = 4096;
	else
		return 0;
	sceKernelSetEventFlag(swdlContext->eventFlagId, pattern);
	return 0;
}

int swdlInitialize(SceUInt32 version, const SwdlClient *client)
{
	int (*notify)(SceUInt32 mask, const BbmcNotificationArgs *args);
	SwdlPortInterface *port;
	int result;

	if (version != 8 || client == NULL)
		return (int)0x80330002;
	if (swdlContext != NULL)
		return (int)0x80330004;
	notify = client->notify;
	memset(&swdlContextStorage, 0, sizeof(swdlContextStorage));
	swdlContextStorage.clientArgument = client->argument;
	port = swdlCreatePort();
	if (port == NULL)
		return (int)0x80330707;
	port->eventHandler = swdlPortEventHandler;
	swdlContextStorage.lock = 0;
	swdlContextStorage.port = port;
	result = sceKernelCreateHeap("SceBbmcSwdlMod", 0x4000, NULL);
	if (result < 0)
		return result;
	swdlContextStorage.heapId = result;
	result = sceKernelCreateEventFlag("SceBbmcSwdlMod", 4096, 0, NULL);
	if (result < 0)
		goto delete_heap;
	swdlContextStorage.eventFlagId = result;
	swdlContextStorage.notification.control = client->argument;
	swdlContextStorage.notify = notify;
	result = sceKernelCreateThread("SceBbmcSwdlMod", swdlThread, 96, 0x2000, 0, 0x80000,
				       NULL);
	if (result < 0)
		goto delete_event_flag;
	swdlContextStorage.threadId = result;
	swdlContext = &swdlContextStorage;
	result = sceKernelStartThread(result, sizeof(swdlContext), &swdlContext);
	if (result < 0) {
		sceKernelDeleteThread(swdlContextStorage.threadId);
		swdlContextStorage.threadId = 0;
		goto delete_event_flag;
	}
	return result;

delete_event_flag:
	sceKernelDeleteEventFlag(swdlContextStorage.eventFlagId);
	swdlContextStorage.eventFlagId = 0;
delete_heap:
	sceKernelDeleteHeap(swdlContextStorage.heapId);
	swdlContextStorage.heapId = 0;
	return result;
}

int swdlTerminate(void)
{
	SceUInt32 timeout = 10000000;
	int result;

	if (swdlContext == NULL)
		return (int)0x80330003;
	result = sceKernelSetEventFlag(swdlContext->eventFlagId, 1);
	if (result >= 0)
		result = sceKernelWaitThreadEnd(swdlContext->threadId, NULL, &timeout);
	sceKernelDeleteThread(swdlContext->threadId);
	sceKernelDeleteEventFlag(swdlContext->eventFlagId);
	sceKernelDeleteHeap(swdlContext->heapId);
	swdlDestroyPort(swdlContext->port);
	swdlContextStorage.port = NULL;
	swdlContext = NULL;
	return result;
}

/* the requests: each copies its arguments into the context and wakes the thread */
int swdlRequestBackup(const SwdlBackupRequest *request)
{
	SwdlContext *context;

	if (swdlContext == NULL)
		return (int)0x80330003;
	sceKernelSpinlockLowLock(&swdlContext->lock);
	context = swdlContext;
	context->backup = *request;
	sceKernelSpinlockLowUnlock(&context->lock);
	return sceKernelSetEventFlag(swdlContext->eventFlagId, 2);
}

int swdlRequestRestore(const SwdlRestoreRequest *request)
{
	SwdlContext *context;

	if (swdlContext == NULL)
		return (int)0x80330003;
	sceKernelSpinlockLowLock(&swdlContext->lock);
	context = swdlContext;
	context->restore = *request;
	sceKernelSpinlockLowUnlock(&context->lock);
	return sceKernelSetEventFlag(swdlContext->eventFlagId, 0x20);
}

int swdlRequestUpdate(const SwdlUpdateRequest *request)
{
	SwdlContext *context;

	if (swdlContext == NULL)
		return (int)0x80330003;
	sceKernelSpinlockLowLock(&swdlContext->lock);
	context = swdlContext;
	context->update = *request;
	sceKernelSpinlockLowUnlock(&context->lock);
	return sceKernelSetEventFlag(swdlContext->eventFlagId, 4);
}

int swdlRequestDownload(const SwdlDownloadRequest *request)
{
	SwdlContext *context;

	if (swdlContext == NULL)
		return (int)0x80330003;
	sceKernelSpinlockLowLock(&swdlContext->lock);
	context = swdlContext;
	context->download = *request;
	sceKernelSpinlockLowUnlock(&context->lock);
	return sceKernelSetEventFlag(swdlContext->eventFlagId, 8);
}

/* the update request of the carrier sequence */
int swdlRequestCarrierUpdate(const SwdlUpdateRequest *request)
{
	SwdlContext *context;

	if (swdlContext == NULL)
		return (int)0x80330003;
	sceKernelSpinlockLowLock(&swdlContext->lock);
	context = swdlContext;
	context->update = *request;
	sceKernelSpinlockLowUnlock(&context->lock);
	return sceKernelSetEventFlag(swdlContext->eventFlagId, 0x200000);
}

int swdlCancel(void)
{
	if (swdlContext == NULL)
		return (int)0x80330003;
	return sceKernelSetEventFlag(swdlContext->eventFlagId, 0x80);
}

int swdlRequestStop(void)
{
	if (swdlContext == NULL)
		return (int)0x80330003;
	return sceKernelSetEventFlag(swdlContext->eventFlagId, 16);
}

int swdlRequestDownloadMode(void)
{
	SceUInt8 command[4] = { 0x4b, 0xfa, 0x01, 0x00 };
	SceUInt8 argument[2] = { 0x01, 0x01 };
	int result;

	if (swdlContext == NULL)
		return (int)0x80330003;
	result = swdlAppendListNode(swdlContext, command, sizeof(command), argument,
				    sizeof(argument));
	if (result >= 0)
		result = sceKernelSetEventFlag(swdlContext->eventFlagId, 0x800000);
	return result;
}

int swdlWaitAttach(int extended)
{
	SwdlContext *context = swdlContext;

	if (context == NULL)
		return (int)0x80330003;
	context->attachTimeout = extended == 1 ? 35000 : 5000;
	return sceKernelSetEventFlag(context->eventFlagId, 0x4000);
}

int swdlWaitDetach(void)
{
	if (swdlContext == NULL)
		return (int)0x80330003;
	swdlContext->attachTimeout = 5000;
	return sceKernelSetEventFlag(swdlContext->eventFlagId, 0x8000);
}

/* name unknown: the backup file's header and the magic after it */
static const char swdlBackupHeader[20] = "COM Backup File\n";

/* the NV items the backup saves, by ID, with how many instances follow each */
static const SwdlNvItem swdlNvItems[6577] = {
	{ 0, 0 }, { 5, 0 }, { 10, 1 }, { 11, 1 }, { 12, 1 }, { 13, 1 }, { 15, 1 }, { 17, 1 },
	{ 18, 1 }, { 19, 1 }, { 20, 1 }, { 21, 1 }, { 24, 1 }, { 31, 1 }, { 32, 1 },
	{ 33, 1 }, { 34, 1 }, { 35, 1 }, { 36, 1 }, { 37, 1 }, { 38, 1 }, { 39, 1 },
	{ 40, 0 }, { 42, 0 }, { 43, 1 }, { 53, 1 }, { 54, 1 }, { 56, 0 }, { 57, 108 },
	{ 62, 0 }, { 63, 0 }, { 64, 0 }, { 65, 0 }, { 66, 0 }, { 67, 0 }, { 69, 0 },
	{ 70, 0 }, { 71, 0 }, { 72, 0 }, { 74, 0 }, { 75, 0 }, { 76, 0 }, { 77, 0 },
	{ 81, 0 }, { 82, 0 }, { 83, 0 }, { 84, 0 }, { 85, 0 }, { 90, 0 }, { 92, 0 },
	{ 95, 0 }, { 107, 0 }, { 108, 0 }, { 120, 0 }, { 121, 0 }, { 122, 0 }, { 123, 0 },
	{ 124, 0 }, { 125, 0 }, { 126, 0 }, { 127, 0 }, { 128, 0 }, { 129, 0 }, { 130, 0 },
	{ 132, 0 }, { 133, 0 }, { 134, 0 }, { 135, 0 }, { 136, 0 }, { 137, 0 }, { 138, 0 },
	{ 139, 0 }, { 140, 0 }, { 141, 0 }, { 142, 0 }, { 143, 0 }, { 144, 0 }, { 145, 0 },
	{ 146, 0 }, { 148, 0 }, { 149, 0 }, { 150, 0 }, { 151, 0 }, { 152, 0 }, { 153, 0 },
	{ 154, 0 }, { 155, 0 }, { 156, 0 }, { 157, 0 }, { 158, 0 }, { 159, 0 }, { 161, 0 },
	{ 162, 0 }, { 163, 0 }, { 164, 0 }, { 165, 0 }, { 166, 0 }, { 168, 1 }, { 169, 1 },
	{ 170, 0 }, { 171, 0 }, { 174, 0 }, { 176, 1 }, { 177, 1 }, { 178, 1 }, { 179, 0 },
	{ 184, 0 }, { 190, 0 }, { 191, 0 }, { 192, 0 }, { 193, 0 }, { 194, 0 }, { 197, 0 },
	{ 198, 0 }, { 199, 0 }, { 200, 0 }, { 202, 0 }, { 203, 0 }, { 204, 0 }, { 206, 0 },
	{ 207, 0 }, { 209, 1 }, { 210, 1 }, { 211, 1 }, { 212, 1 }, { 213, 1 }, { 215, 1 },
	{ 219, 0 }, { 220, 0 }, { 221, 0 }, { 222, 0 }, { 223, 0 }, { 225, 0 }, { 226, 0 },
	{ 227, 0 }, { 228, 0 }, { 229, 0 }, { 230, 0 }, { 231, 1 }, { 232, 0 }, { 233, 0 },
	{ 237, 0 }, { 240, 0 }, { 241, 0 }, { 243, 0 }, { 246, 0 }, { 247, 0 }, { 248, 0 },
	{ 249, 0 }, { 250, 0 }, { 251, 0 }, { 255, 1 }, { 256, 1 }, { 258, 1 }, { 259, 1 },
	{ 260, 1 }, { 261, 1 }, { 262, 1 }, { 263, 1 }, { 264, 1 }, { 265, 1 }, { 266, 1 },
	{ 267, 0 }, { 268, 0 }, { 269, 0 }, { 270, 0 }, { 275, 0 }, { 284, 0 }, { 285, 1 },
	{ 286, 0 }, { 287, 0 }, { 290, 0 }, { 291, 0 }, { 292, 0 }, { 293, 0 }, { 296, 0 },
	{ 297, 0 }, { 298, 0 }, { 300, 0 }, { 301, 0 }, { 303, 5 }, { 304, 0 }, { 305, 0 },
	{ 306, 0 }, { 309, 0 }, { 310, 0 }, { 311, 0 }, { 312, 0 }, { 325, 0 }, { 326, 0 },
	{ 327, 0 }, { 328, 0 }, { 329, 0 }, { 330, 0 }, { 331, 0 }, { 332, 0 }, { 333, 0 },
	{ 334, 0 }, { 335, 0 }, { 336, 0 }, { 337, 0 }, { 338, 0 }, { 339, 0 }, { 340, 0 },
	{ 341, 0 }, { 342, 0 }, { 343, 0 }, { 344, 0 }, { 345, 0 }, { 346, 0 }, { 347, 0 },
	{ 348, 0 }, { 349, 0 }, { 350, 0 }, { 351, 0 }, { 352, 0 }, { 353, 0 }, { 354, 0 },
	{ 355, 0 }, { 356, 0 }, { 357, 0 }, { 358, 0 }, { 367, 0 }, { 368, 0 }, { 369, 0 },
	{ 370, 0 }, { 376, 0 }, { 377, 0 }, { 378, 0 }, { 379, 0 }, { 380, 0 }, { 381, 0 },
	{ 382, 0 }, { 383, 0 }, { 384, 0 }, { 385, 0 }, { 386, 0 }, { 387, 0 }, { 388, 0 },
	{ 389, 0 }, { 390, 0 }, { 391, 0 }, { 392, 0 }, { 393, 0 }, { 394, 0 }, { 395, 0 },
	{ 396, 0 }, { 397, 0 }, { 398, 0 }, { 399, 0 }, { 400, 0 }, { 401, 0 }, { 402, 0 },
	{ 403, 0 }, { 404, 0 }, { 405, 0 }, { 409, 0 }, { 410, 0 }, { 411, 0 }, { 412, 0 },
	{ 413, 0 }, { 414, 0 }, { 415, 0 }, { 416, 0 }, { 417, 0 }, { 418, 0 }, { 419, 0 },
	{ 420, 0 }, { 421, 0 }, { 422, 0 }, { 423, 0 }, { 424, 0 }, { 426, 0 }, { 427, 0 },
	{ 428, 0 }, { 429, 0 }, { 430, 0 }, { 431, 0 }, { 432, 0 }, { 433, 0 }, { 434, 0 },
	{ 435, 0 }, { 436, 0 }, { 437, 0 }, { 438, 0 }, { 439, 0 }, { 441, 1 }, { 442, 1 },
	{ 443, 0 }, { 444, 0 }, { 445, 0 }, { 446, 0 }, { 448, 0 }, { 449, 0 }, { 450, 0 },
	{ 451, 1 }, { 452, 0 }, { 453, 0 }, { 454, 0 }, { 455, 0 }, { 459, 0 }, { 460, 0 },
	{ 461, 0 }, { 462, 0 }, { 463, 0 }, { 464, 0 }, { 465, 5 }, { 467, 0 }, { 468, 0 },
	{ 469, 0 }, { 470, 0 }, { 471, 0 }, { 472, 0 }, { 473, 0 }, { 474, 0 }, { 475, 0 },
	{ 476, 0 }, { 477, 0 }, { 478, 0 }, { 479, 0 }, { 480, 0 }, { 481, 0 }, { 482, 0 },
	{ 483, 0 }, { 484, 0 }, { 485, 0 }, { 486, 0 }, { 487, 0 }, { 488, 0 }, { 489, 0 },
	{ 494, 5 }, { 495, 0 }, { 496, 0 }, { 497, 0 }, { 498, 0 }, { 499, 0 }, { 500, 0 },
	{ 501, 0 }, { 502, 0 }, { 503, 0 }, { 504, 0 }, { 505, 0 }, { 506, 0 }, { 507, 0 },
	{ 508, 0 }, { 509, 0 }, { 510, 0 }, { 511, 0 }, { 512, 0 }, { 513, 0 }, { 514, 0 },
	{ 515, 0 }, { 516, 0 }, { 517, 0 }, { 518, 0 }, { 519, 0 }, { 520, 0 }, { 521, 0 },
	{ 522, 0 }, { 523, 0 }, { 524, 0 }, { 525, 0 }, { 526, 0 }, { 527, 0 }, { 528, 0 },
	{ 529, 0 }, { 530, 0 }, { 531, 0 }, { 532, 0 }, { 533, 0 }, { 534, 0 }, { 535, 0 },
	{ 536, 0 }, { 537, 0 }, { 538, 0 }, { 539, 0 }, { 540, 0 }, { 541, 0 }, { 542, 0 },
	{ 543, 0 }, { 544, 0 }, { 545, 0 }, { 546, 0 }, { 547, 0 }, { 548, 0 }, { 549, 0 },
	{ 550, 0 }, { 551, 0 }, { 552, 0 }, { 553, 0 }, { 554, 0 }, { 555, 0 }, { 556, 0 },
	{ 557, 0 }, { 558, 0 }, { 559, 0 }, { 561, 0 }, { 562, 0 }, { 563, 0 }, { 564, 0 },
	{ 565, 0 }, { 566, 0 }, { 567, 0 }, { 568, 0 }, { 569, 0 }, { 570, 0 }, { 571, 0 },
	{ 572, 0 }, { 573, 0 }, { 574, 0 }, { 577, 0 }, { 579, 0 }, { 583, 0 }, { 584, 0 },
	{ 585, 0 }, { 586, 0 }, { 587, 0 }, { 588, 0 }, { 589, 0 }, { 590, 0 }, { 591, 0 },
	{ 592, 0 }, { 593, 0 }, { 594, 0 }, { 595, 0 }, { 596, 0 }, { 597, 0 }, { 598, 0 },
	{ 599, 0 }, { 600, 0 }, { 601, 0 }, { 602, 0 }, { 603, 0 }, { 604, 0 }, { 605, 0 },
	{ 606, 0 }, { 607, 0 }, { 608, 0 }, { 609, 0 }, { 610, 0 }, { 611, 0 }, { 612, 0 },
	{ 613, 0 }, { 614, 0 }, { 615, 0 }, { 616, 0 }, { 617, 0 }, { 618, 0 }, { 619, 0 },
	{ 620, 0 }, { 621, 0 }, { 622, 0 }, { 623, 0 }, { 624, 0 }, { 625, 0 }, { 626, 0 },
	{ 627, 0 }, { 628, 0 }, { 629, 0 }, { 630, 0 }, { 631, 0 }, { 632, 0 }, { 633, 0 },
	{ 634, 0 }, { 635, 0 }, { 636, 0 }, { 637, 0 }, { 638, 0 }, { 639, 0 }, { 640, 0 },
	{ 641, 0 }, { 642, 0 }, { 643, 0 }, { 644, 0 }, { 645, 0 }, { 646, 0 }, { 647, 0 },
	{ 648, 0 }, { 649, 0 }, { 650, 0 }, { 651, 0 }, { 652, 0 }, { 653, 0 }, { 654, 0 },
	{ 655, 0 }, { 656, 0 }, { 657, 0 }, { 658, 0 }, { 659, 0 }, { 660, 0 }, { 661, 0 },
	{ 662, 0 }, { 663, 0 }, { 664, 0 }, { 665, 0 }, { 666, 0 }, { 667, 0 }, { 668, 0 },
	{ 669, 0 }, { 670, 0 }, { 671, 0 }, { 672, 0 }, { 673, 0 }, { 674, 0 }, { 675, 0 },
	{ 676, 0 }, { 677, 0 }, { 678, 0 }, { 679, 0 }, { 680, 0 }, { 681, 0 }, { 682, 0 },
	{ 683, 0 }, { 684, 0 }, { 685, 0 }, { 686, 0 }, { 687, 0 }, { 688, 0 }, { 689, 0 },
	{ 690, 0 }, { 691, 0 }, { 692, 0 }, { 693, 0 }, { 694, 0 }, { 695, 0 }, { 696, 0 },
	{ 697, 0 }, { 698, 0 }, { 699, 0 }, { 700, 0 }, { 701, 0 }, { 702, 0 }, { 703, 0 },
	{ 704, 0 }, { 705, 0 }, { 706, 0 }, { 707, 0 }, { 708, 0 }, { 709, 0 }, { 710, 0 },
	{ 712, 0 }, { 713, 0 }, { 714, 0 }, { 715, 0 }, { 716, 0 }, { 717, 0 }, { 718, 0 },
	{ 719, 0 }, { 720, 0 }, { 722, 0 }, { 723, 0 }, { 724, 0 }, { 725, 0 }, { 726, 0 },
	{ 727, 0 }, { 728, 0 }, { 729, 0 }, { 730, 0 }, { 731, 0 }, { 732, 0 }, { 733, 0 },
	{ 734, 0 }, { 735, 0 }, { 736, 0 }, { 737, 0 }, { 738, 0 }, { 739, 0 }, { 740, 0 },
	{ 741, 0 }, { 742, 0 }, { 743, 0 }, { 744, 0 }, { 745, 0 }, { 746, 0 }, { 747, 0 },
	{ 748, 0 }, { 749, 0 }, { 750, 0 }, { 751, 0 }, { 752, 0 }, { 753, 0 }, { 754, 0 },
	{ 755, 0 }, { 756, 0 }, { 757, 0 }, { 758, 0 }, { 759, 0 }, { 760, 0 }, { 761, 0 },
	{ 762, 0 }, { 763, 0 }, { 764, 0 }, { 765, 0 }, { 766, 0 }, { 767, 0 }, { 768, 0 },
	{ 769, 0 }, { 770, 0 }, { 771, 0 }, { 772, 0 }, { 773, 0 }, { 774, 0 }, { 775, 0 },
	{ 776, 0 }, { 777, 0 }, { 778, 0 }, { 779, 0 }, { 780, 0 }, { 781, 0 }, { 782, 0 },
	{ 783, 0 }, { 784, 0 }, { 785, 0 }, { 786, 0 }, { 787, 0 }, { 788, 0 }, { 789, 0 },
	{ 790, 0 }, { 791, 0 }, { 792, 0 }, { 793, 0 }, { 794, 0 }, { 795, 0 }, { 796, 0 },
	{ 797, 0 }, { 798, 0 }, { 799, 0 }, { 800, 0 }, { 801, 0 }, { 802, 0 }, { 803, 0 },
	{ 804, 0 }, { 805, 0 }, { 806, 0 }, { 807, 0 }, { 808, 0 }, { 809, 0 }, { 810, 0 },
	{ 811, 0 }, { 812, 0 }, { 813, 0 }, { 814, 0 }, { 815, 0 }, { 816, 0 }, { 817, 0 },
	{ 818, 0 }, { 819, 0 }, { 820, 0 }, { 821, 0 }, { 822, 0 }, { 823, 0 }, { 824, 0 },
	{ 825, 0 }, { 826, 0 }, { 827, 0 }, { 828, 0 }, { 829, 0 }, { 830, 0 }, { 831, 0 },
	{ 832, 0 }, { 833, 0 }, { 834, 0 }, { 835, 0 }, { 836, 0 }, { 837, 0 }, { 838, 0 },
	{ 839, 0 }, { 840, 0 }, { 841, 0 }, { 842, 0 }, { 843, 0 }, { 844, 0 }, { 845, 0 },
	{ 846, 0 }, { 847, 0 }, { 848, 1 }, { 849, 1 }, { 850, 1 }, { 851, 0 }, { 852, 0 },
	{ 853, 0 }, { 854, 5 }, { 855, 0 }, { 856, 0 }, { 857, 0 }, { 858, 0 }, { 859, 0 },
	{ 860, 0 }, { 861, 0 }, { 862, 0 }, { 863, 0 }, { 864, 0 }, { 865, 0 }, { 866, 0 },
	{ 867, 0 }, { 868, 0 }, { 869, 0 }, { 870, 0 }, { 871, 0 }, { 872, 0 }, { 873, 0 },
	{ 874, 0 }, { 875, 0 }, { 876, 0 }, { 877, 0 }, { 878, 0 }, { 879, 0 }, { 880, 1 },
	{ 881, 1 }, { 882, 1 }, { 884, 0 }, { 885, 0 }, { 886, 0 }, { 887, 0 }, { 888, 0 },
	{ 890, 0 }, { 896, 0 }, { 897, 0 }, { 901, 0 }, { 902, 0 }, { 903, 0 }, { 904, 0 },
	{ 905, 0 }, { 907, 0 }, { 908, 0 }, { 909, 0 }, { 910, 0 }, { 911, 0 }, { 912, 0 },
	{ 913, 0 }, { 914, 0 }, { 915, 0 }, { 916, 0 }, { 917, 0 }, { 919, 0 }, { 921, 1 },
	{ 922, 1 }, { 923, 0 }, { 924, 0 }, { 925, 0 }, { 926, 0 }, { 927, 0 }, { 928, 0 },
	{ 929, 0 }, { 930, 0 }, { 931, 0 }, { 932, 0 }, { 933, 0 }, { 934, 0 }, { 935, 0 },
	{ 938, 0 }, { 939, 0 }, { 940, 0 }, { 941, 0 }, { 942, 0 }, { 943, 0 }, { 944, 0 },
	{ 945, 1 }, { 946, 1 }, { 947, 0 }, { 948, 0 }, { 949, 0 }, { 950, 0 }, { 951, 0 },
	{ 952, 0 }, { 953, 0 }, { 954, 0 }, { 955, 0 }, { 956, 0 }, { 957, 0 }, { 958, 0 },
	{ 959, 0 }, { 960, 0 }, { 961, 0 }, { 962, 0 }, { 963, 0 }, { 964, 0 }, { 965, 0 },
	{ 966, 0 }, { 967, 0 }, { 968, 0 }, { 969, 0 }, { 970, 0 }, { 971, 0 }, { 972, 0 },
	{ 973, 0 }, { 974, 0 }, { 975, 0 }, { 976, 0 }, { 977, 0 }, { 978, 0 }, { 979, 0 },
	{ 980, 0 }, { 981, 0 }, { 982, 0 }, { 983, 0 }, { 984, 0 }, { 985, 0 }, { 986, 0 },
	{ 987, 0 }, { 988, 0 }, { 989, 0 }, { 990, 0 }, { 991, 0 }, { 992, 0 }, { 993, 0 },
	{ 994, 0 }, { 995, 0 }, { 996, 0 }, { 997, 0 }, { 998, 0 }, { 999, 0 }, { 1000, 0 },
	{ 1001, 0 }, { 1002, 0 }, { 1003, 0 }, { 1004, 0 }, { 1005, 0 }, { 1006, 0 },
	{ 1007, 0 }, { 1008, 0 }, { 1009, 0 }, { 1010, 0 }, { 1011, 0 }, { 1012, 0 },
	{ 1013, 0 }, { 1014, 1 }, { 1015, 0 }, { 1016, 0 }, { 1017, 0 }, { 1018, 0 },
	{ 1019, 0 }, { 1020, 0 }, { 1021, 0 }, { 1022, 0 }, { 1023, 0 }, { 1024, 0 },
	{ 1025, 0 }, { 1026, 0 }, { 1027, 0 }, { 1028, 0 }, { 1029, 0 }, { 1030, 0 },
	{ 1031, 0 }, { 1032, 0 }, { 1033, 0 }, { 1034, 0 }, { 1035, 0 }, { 1036, 0 },
	{ 1037, 0 }, { 1038, 0 }, { 1039, 0 }, { 1040, 0 }, { 1041, 0 }, { 1042, 0 },
	{ 1043, 0 }, { 1044, 0 }, { 1045, 0 }, { 1046, 0 }, { 1047, 0 }, { 1048, 0 },
	{ 1049, 0 }, { 1050, 0 }, { 1051, 0 }, { 1052, 0 }, { 1053, 0 }, { 1054, 0 },
	{ 1055, 0 }, { 1056, 0 }, { 1057, 0 }, { 1058, 0 }, { 1059, 0 }, { 1060, 0 },
	{ 1061, 0 }, { 1062, 0 }, { 1063, 0 }, { 1064, 0 }, { 1065, 0 }, { 1066, 0 },
	{ 1067, 0 }, { 1068, 0 }, { 1069, 0 }, { 1070, 0 }, { 1071, 0 }, { 1072, 0 },
	{ 1073, 0 }, { 1074, 0 }, { 1075, 0 }, { 1076, 0 }, { 1077, 0 }, { 1078, 0 },
	{ 1079, 0 }, { 1080, 0 }, { 1081, 0 }, { 1082, 0 }, { 1083, 0 }, { 1084, 0 },
	{ 1085, 0 }, { 1086, 0 }, { 1087, 0 }, { 1088, 0 }, { 1089, 0 }, { 1090, 0 },
	{ 1091, 0 }, { 1092, 0 }, { 1093, 0 }, { 1094, 0 }, { 1095, 0 }, { 1096, 0 },
	{ 1097, 0 }, { 1098, 0 }, { 1099, 0 }, { 1100, 0 }, { 1101, 0 }, { 1102, 0 },
	{ 1103, 0 }, { 1104, 0 }, { 1105, 0 }, { 1106, 0 }, { 1107, 0 }, { 1108, 0 },
	{ 1109, 0 }, { 1110, 0 }, { 1111, 0 }, { 1112, 0 }, { 1113, 0 }, { 1114, 0 },
	{ 1115, 0 }, { 1116, 0 }, { 1117, 0 }, { 1118, 0 }, { 1119, 0 }, { 1120, 0 },
	{ 1121, 0 }, { 1122, 0 }, { 1123, 0 }, { 1124, 0 }, { 1125, 0 }, { 1126, 0 },
	{ 1127, 0 }, { 1128, 0 }, { 1129, 0 }, { 1130, 0 }, { 1131, 0 }, { 1132, 0 },
	{ 1133, 0 }, { 1134, 0 }, { 1135, 0 }, { 1136, 0 }, { 1137, 0 }, { 1138, 0 },
	{ 1139, 0 }, { 1140, 0 }, { 1141, 0 }, { 1142, 0 }, { 1143, 0 }, { 1144, 0 },
	{ 1145, 0 }, { 1146, 0 }, { 1147, 0 }, { 1148, 0 }, { 1149, 0 }, { 1150, 0 },
	{ 1151, 0 }, { 1152, 0 }, { 1153, 0 }, { 1154, 0 }, { 1155, 0 }, { 1156, 0 },
	{ 1157, 0 }, { 1158, 0 }, { 1159, 0 }, { 1160, 0 }, { 1161, 0 }, { 1162, 0 },
	{ 1163, 0 }, { 1164, 0 }, { 1165, 0 }, { 1166, 0 }, { 1167, 0 }, { 1168, 0 },
	{ 1169, 0 }, { 1170, 0 }, { 1171, 0 }, { 1172, 0 }, { 1173, 0 }, { 1174, 0 },
	{ 1175, 0 }, { 1176, 0 }, { 1177, 0 }, { 1178, 0 }, { 1179, 0 }, { 1180, 0 },
	{ 1181, 0 }, { 1182, 0 }, { 1183, 0 }, { 1184, 0 }, { 1185, 0 }, { 1186, 0 },
	{ 1187, 0 }, { 1188, 0 }, { 1189, 0 }, { 1190, 0 }, { 1191, 0 }, { 1193, 0 },
	{ 1194, 0 }, { 1195, 0 }, { 1196, 0 }, { 1197, 0 }, { 1198, 0 }, { 1199, 0 },
	{ 1200, 0 }, { 1201, 0 }, { 1202, 0 }, { 1203, 0 }, { 1204, 0 }, { 1205, 0 },
	{ 1206, 2 }, { 1207, 0 }, { 1208, 0 }, { 1209, 0 }, { 1210, 0 }, { 1211, 0 },
	{ 1212, 0 }, { 1213, 0 }, { 1214, 0 }, { 1215, 0 }, { 1216, 0 }, { 1217, 0 },
	{ 1218, 0 }, { 1219, 0 }, { 1220, 0 }, { 1221, 0 }, { 1222, 0 }, { 1223, 0 },
	{ 1224, 0 }, { 1225, 0 }, { 1226, 0 }, { 1227, 0 }, { 1228, 0 }, { 1229, 0 },
	{ 1230, 0 }, { 1231, 0 }, { 1232, 0 }, { 1233, 0 }, { 1234, 0 }, { 1235, 0 },
	{ 1236, 0 }, { 1237, 0 }, { 1238, 0 }, { 1239, 0 }, { 1240, 0 }, { 1241, 0 },
	{ 1242, 0 }, { 1243, 0 }, { 1244, 0 }, { 1245, 0 }, { 1246, 0 }, { 1247, 0 },
	{ 1248, 0 }, { 1249, 0 }, { 1250, 0 }, { 1251, 0 }, { 1252, 0 }, { 1253, 0 },
	{ 1254, 0 }, { 1255, 0 }, { 1256, 0 }, { 1257, 0 }, { 1258, 0 }, { 1259, 0 },
	{ 1260, 0 }, { 1261, 0 }, { 1262, 0 }, { 1263, 0 }, { 1264, 0 }, { 1265, 0 },
	{ 1266, 0 }, { 1267, 0 }, { 1268, 0 }, { 1269, 0 }, { 1270, 0 }, { 1271, 0 },
	{ 1272, 0 }, { 1273, 0 }, { 1274, 0 }, { 1275, 0 }, { 1276, 0 }, { 1277, 0 },
	{ 1278, 0 }, { 1279, 0 }, { 1280, 0 }, { 1281, 0 }, { 1282, 0 }, { 1283, 0 },
	{ 1284, 0 }, { 1285, 0 }, { 1286, 0 }, { 1287, 0 }, { 1288, 0 }, { 1289, 0 },
	{ 1290, 0 }, { 1291, 0 }, { 1292, 0 }, { 1293, 0 }, { 1294, 0 }, { 1295, 0 },
	{ 1296, 0 }, { 1297, 0 }, { 1298, 0 }, { 1299, 0 }, { 1300, 0 }, { 1301, 0 },
	{ 1302, 0 }, { 1303, 0 }, { 1304, 0 }, { 1305, 0 }, { 1306, 0 }, { 1307, 0 },
	{ 1308, 0 }, { 1309, 0 }, { 1310, 0 }, { 1312, 0 }, { 1313, 0 }, { 1314, 0 },
	{ 1315, 0 }, { 1316, 0 }, { 1317, 0 }, { 1318, 0 }, { 1319, 0 }, { 1320, 0 },
	{ 1321, 0 }, { 1322, 0 }, { 1323, 0 }, { 1324, 0 }, { 1325, 0 }, { 1326, 0 },
	{ 1327, 0 }, { 1328, 0 }, { 1329, 0 }, { 1330, 0 }, { 1331, 0 }, { 1332, 0 },
	{ 1333, 0 }, { 1334, 0 }, { 1335, 0 }, { 1336, 0 }, { 1337, 0 }, { 1338, 0 },
	{ 1339, 0 }, { 1340, 0 }, { 1341, 0 }, { 1342, 0 }, { 1343, 0 }, { 1344, 0 },
	{ 1345, 0 }, { 1346, 0 }, { 1347, 0 }, { 1348, 0 }, { 1349, 0 }, { 1351, 0 },
	{ 1352, 0 }, { 1353, 0 }, { 1354, 0 }, { 1355, 0 }, { 1356, 0 }, { 1357, 0 },
	{ 1358, 0 }, { 1359, 0 }, { 1360, 0 }, { 1361, 0 }, { 1362, 0 }, { 1363, 0 },
	{ 1364, 0 }, { 1365, 0 }, { 1366, 0 }, { 1367, 0 }, { 1368, 0 }, { 1369, 0 },
	{ 1370, 0 }, { 1371, 0 }, { 1372, 0 }, { 1373, 0 }, { 1374, 0 }, { 1375, 0 },
	{ 1376, 0 }, { 1377, 0 }, { 1378, 0 }, { 1379, 0 }, { 1380, 0 }, { 1381, 0 },
	{ 1382, 0 }, { 1383, 0 }, { 1384, 0 }, { 1385, 0 }, { 1386, 0 }, { 1387, 0 },
	{ 1388, 0 }, { 1389, 0 }, { 1391, 0 }, { 1392, 0 }, { 1393, 0 }, { 1394, 0 },
	{ 1395, 0 }, { 1396, 0 }, { 1397, 0 }, { 1398, 0 }, { 1399, 0 }, { 1400, 0 },
	{ 1401, 0 }, { 1402, 0 }, { 1403, 0 }, { 1404, 0 }, { 1405, 0 }, { 1406, 0 },
	{ 1407, 0 }, { 1408, 0 }, { 1409, 0 }, { 1410, 0 }, { 1411, 0 }, { 1412, 0 },
	{ 1413, 0 }, { 1414, 0 }, { 1415, 0 }, { 1416, 0 }, { 1417, 0 }, { 1418, 0 },
	{ 1419, 0 }, { 1420, 0 }, { 1421, 0 }, { 1422, 0 }, { 1423, 0 }, { 1424, 0 },
	{ 1425, 0 }, { 1426, 0 }, { 1427, 0 }, { 1428, 0 }, { 1429, 0 }, { 1430, 0 },
	{ 1431, 0 }, { 1432, 0 }, { 1433, 0 }, { 1434, 0 }, { 1435, 0 }, { 1436, 0 },
	{ 1437, 0 }, { 1438, 0 }, { 1439, 0 }, { 1440, 0 }, { 1441, 0 }, { 1442, 0 },
	{ 1443, 0 }, { 1444, 0 }, { 1445, 0 }, { 1446, 0 }, { 1447, 0 }, { 1448, 0 },
	{ 1449, 0 }, { 1450, 0 }, { 1451, 0 }, { 1452, 0 }, { 1453, 0 }, { 1454, 0 },
	{ 1455, 0 }, { 1456, 0 }, { 1457, 0 }, { 1458, 0 }, { 1459, 0 }, { 1460, 0 },
	{ 1461, 0 }, { 1462, 0 }, { 1463, 0 }, { 1464, 0 }, { 1465, 0 }, { 1466, 0 },
	{ 1467, 0 }, { 1469, 0 }, { 1470, 0 }, { 1471, 0 }, { 1472, 0 }, { 1473, 0 },
	{ 1474, 0 }, { 1475, 0 }, { 1476, 0 }, { 1477, 0 }, { 1478, 0 }, { 1479, 0 },
	{ 1480, 0 }, { 1481, 0 }, { 1482, 0 }, { 1483, 0 }, { 1484, 0 }, { 1485, 0 },
	{ 1486, 0 }, { 1487, 0 }, { 1488, 0 }, { 1489, 0 }, { 1490, 0 }, { 1491, 0 },
	{ 1492, 0 }, { 1493, 0 }, { 1494, 0 }, { 1495, 0 }, { 1496, 0 }, { 1497, 0 },
	{ 1498, 0 }, { 1499, 0 }, { 1500, 0 }, { 1501, 0 }, { 1502, 0 }, { 1503, 0 },
	{ 1504, 0 }, { 1505, 0 }, { 1506, 0 }, { 1507, 0 }, { 1508, 0 }, { 1509, 0 },
	{ 1510, 0 }, { 1511, 0 }, { 1512, 0 }, { 1513, 0 }, { 1514, 0 }, { 1515, 0 },
	{ 1516, 0 }, { 1517, 0 }, { 1518, 0 }, { 1519, 0 }, { 1520, 0 }, { 1521, 0 },
	{ 1522, 0 }, { 1523, 0 }, { 1524, 0 }, { 1525, 0 }, { 1526, 0 }, { 1527, 0 },
	{ 1528, 0 }, { 1529, 0 }, { 1530, 0 }, { 1531, 0 }, { 1532, 0 }, { 1533, 0 },
	{ 1534, 0 }, { 1535, 0 }, { 1536, 0 }, { 1537, 0 }, { 1538, 0 }, { 1539, 0 },
	{ 1540, 0 }, { 1541, 0 }, { 1542, 0 }, { 1543, 0 }, { 1544, 0 }, { 1545, 0 },
	{ 1547, 0 }, { 1548, 0 }, { 1549, 0 }, { 1550, 0 }, { 1551, 0 }, { 1552, 0 },
	{ 1553, 0 }, { 1554, 0 }, { 1555, 0 }, { 1557, 0 }, { 1558, 0 }, { 1559, 0 },
	{ 1560, 0 }, { 1561, 0 }, { 1562, 0 }, { 1563, 0 }, { 1564, 0 }, { 1565, 0 },
	{ 1566, 0 }, { 1567, 0 }, { 1568, 0 }, { 1569, 0 }, { 1570, 0 }, { 1571, 0 },
	{ 1572, 0 }, { 1573, 0 }, { 1574, 0 }, { 1575, 0 }, { 1576, 0 }, { 1577, 0 },
	{ 1578, 0 }, { 1579, 0 }, { 1580, 0 }, { 1581, 0 }, { 1582, 0 }, { 1583, 0 },
	{ 1584, 0 }, { 1585, 0 }, { 1586, 0 }, { 1587, 0 }, { 1588, 0 }, { 1589, 0 },
	{ 1590, 0 }, { 1591, 0 }, { 1592, 0 }, { 1593, 0 }, { 1594, 0 }, { 1595, 0 },
	{ 1596, 0 }, { 1597, 0 }, { 1598, 0 }, { 1599, 0 }, { 1600, 0 }, { 1601, 0 },
	{ 1602, 0 }, { 1603, 0 }, { 1604, 0 }, { 1605, 0 }, { 1606, 0 }, { 1607, 0 },
	{ 1608, 0 }, { 1609, 0 }, { 1610, 0 }, { 1611, 0 }, { 1612, 0 }, { 1613, 0 },
	{ 1614, 0 }, { 1615, 0 }, { 1616, 0 }, { 1617, 0 }, { 1618, 0 }, { 1619, 0 },
	{ 1620, 0 }, { 1621, 0 }, { 1622, 0 }, { 1623, 0 }, { 1624, 0 }, { 1626, 0 },
	{ 1627, 0 }, { 1628, 0 }, { 1629, 0 }, { 1630, 0 }, { 1631, 0 }, { 1632, 0 },
	{ 1633, 0 }, { 1634, 0 }, { 1635, 0 }, { 1636, 0 }, { 1637, 0 }, { 1638, 0 },
	{ 1639, 0 }, { 1640, 0 }, { 1641, 0 }, { 1642, 0 }, { 1643, 0 }, { 1644, 0 },
	{ 1645, 0 }, { 1646, 0 }, { 1647, 0 }, { 1648, 0 }, { 1649, 0 }, { 1650, 0 },
	{ 1651, 0 }, { 1652, 0 }, { 1653, 0 }, { 1654, 0 }, { 1655, 0 }, { 1666, 0 },
	{ 1667, 0 }, { 1668, 0 }, { 1669, 0 }, { 1670, 0 }, { 1671, 0 }, { 1672, 0 },
	{ 1673, 0 }, { 1674, 0 }, { 1675, 0 }, { 1676, 0 }, { 1677, 0 }, { 1678, 0 },
	{ 1679, 0 }, { 1680, 0 }, { 1681, 0 }, { 1682, 0 }, { 1683, 0 }, { 1684, 0 },
	{ 1685, 0 }, { 1686, 0 }, { 1687, 0 }, { 1689, 0 }, { 1690, 0 }, { 1691, 0 },
	{ 1692, 0 }, { 1693, 0 }, { 1694, 0 }, { 1695, 0 }, { 1696, 0 }, { 1697, 0 },
	{ 1698, 0 }, { 1699, 0 }, { 1700, 0 }, { 1701, 0 }, { 1702, 0 }, { 1703, 0 },
	{ 1704, 0 }, { 1705, 0 }, { 1706, 0 }, { 1707, 0 }, { 1708, 0 }, { 1709, 0 },
	{ 1710, 0 }, { 1711, 0 }, { 1712, 0 }, { 1713, 0 }, { 1715, 0 }, { 1716, 0 },
	{ 1717, 0 }, { 1718, 0 }, { 1719, 0 }, { 1720, 0 }, { 1721, 0 }, { 1722, 0 },
	{ 1723, 0 }, { 1724, 0 }, { 1725, 0 }, { 1726, 0 }, { 1727, 0 }, { 1728, 0 },
	{ 1729, 0 }, { 1730, 0 }, { 1731, 0 }, { 1732, 0 }, { 1733, 0 }, { 1734, 0 },
	{ 1735, 0 }, { 1736, 0 }, { 1737, 0 }, { 1738, 0 }, { 1739, 0 }, { 1740, 0 },
	{ 1741, 0 }, { 1742, 0 }, { 1743, 0 }, { 1744, 0 }, { 1745, 0 }, { 1746, 0 },
	{ 1747, 0 }, { 1748, 0 }, { 1749, 0 }, { 1750, 0 }, { 1751, 0 }, { 1752, 0 },
	{ 1753, 0 }, { 1754, 0 }, { 1755, 0 }, { 1756, 0 }, { 1757, 0 }, { 1758, 0 },
	{ 1759, 0 }, { 1760, 0 }, { 1761, 0 }, { 1762, 0 }, { 1763, 0 }, { 1764, 0 },
	{ 1765, 0 }, { 1766, 0 }, { 1767, 0 }, { 1768, 0 }, { 1769, 0 }, { 1770, 0 },
	{ 1771, 0 }, { 1772, 0 }, { 1773, 0 }, { 1774, 0 }, { 1775, 0 }, { 1776, 0 },
	{ 1777, 0 }, { 1778, 0 }, { 1779, 0 }, { 1780, 0 }, { 1781, 0 }, { 1782, 0 },
	{ 1783, 0 }, { 1784, 0 }, { 1785, 0 }, { 1786, 0 }, { 1787, 0 }, { 1788, 0 },
	{ 1789, 0 }, { 1790, 0 }, { 1791, 0 }, { 1792, 0 }, { 1794, 0 }, { 1795, 0 },
	{ 1796, 0 }, { 1797, 0 }, { 1798, 0 }, { 1799, 0 }, { 1800, 0 }, { 1801, 0 },
	{ 1802, 0 }, { 1803, 0 }, { 1805, 0 }, { 1806, 0 }, { 1807, 0 }, { 1808, 0 },
	{ 1809, 0 }, { 1810, 0 }, { 1811, 0 }, { 1812, 0 }, { 1813, 0 }, { 1814, 0 },
	{ 1815, 0 }, { 1816, 0 }, { 1817, 0 }, { 1818, 0 }, { 1819, 0 }, { 1820, 0 },
	{ 1821, 0 }, { 1822, 0 }, { 1823, 0 }, { 1824, 0 }, { 1825, 0 }, { 1826, 0 },
	{ 1827, 0 }, { 1828, 0 }, { 1829, 0 }, { 1830, 0 }, { 1831, 0 }, { 1832, 0 },
	{ 1833, 0 }, { 1834, 0 }, { 1835, 0 }, { 1836, 0 }, { 1837, 0 }, { 1838, 0 },
	{ 1839, 0 }, { 1840, 0 }, { 1841, 0 }, { 1842, 0 }, { 1843, 0 }, { 1844, 0 },
	{ 1845, 0 }, { 1846, 0 }, { 1847, 0 }, { 1848, 0 }, { 1849, 0 }, { 1850, 0 },
	{ 1860, 0 }, { 1861, 0 }, { 1862, 0 }, { 1863, 0 }, { 1864, 0 }, { 1865, 0 },
	{ 1866, 0 }, { 1867, 0 }, { 1868, 0 }, { 1869, 0 }, { 1870, 0 }, { 1871, 0 },
	{ 1872, 0 }, { 1873, 0 }, { 1874, 0 }, { 1875, 0 }, { 1876, 0 }, { 1877, 0 },
	{ 1878, 0 }, { 1879, 0 }, { 1880, 0 }, { 1881, 0 }, { 1882, 0 }, { 1883, 0 },
	{ 1884, 0 }, { 1885, 0 }, { 1886, 0 }, { 1887, 0 }, { 1888, 0 }, { 1889, 0 },
	{ 1890, 0 }, { 1891, 0 }, { 1892, 0 }, { 1893, 0 }, { 1894, 1 }, { 1895, 0 },
	{ 1896, 0 }, { 1897, 0 }, { 1898, 0 }, { 1899, 0 }, { 1900, 0 }, { 1901, 0 },
	{ 1902, 0 }, { 1903, 0 }, { 1904, 0 }, { 1905, 0 }, { 1906, 0 }, { 1907, 0 },
	{ 1908, 0 }, { 1909, 0 }, { 1910, 0 }, { 1911, 0 }, { 1912, 0 }, { 1913, 0 },
	{ 1914, 0 }, { 1915, 0 }, { 1916, 0 }, { 1917, 0 }, { 1918, 0 }, { 1919, 0 },
	{ 1920, 0 }, { 1921, 0 }, { 1922, 0 }, { 1923, 0 }, { 1924, 0 }, { 1925, 0 },
	{ 1926, 0 }, { 1927, 0 }, { 1929, 0 }, { 1930, 0 }, { 1931, 0 }, { 1932, 0 },
	{ 1933, 0 }, { 1934, 0 }, { 1935, 0 }, { 1936, 0 }, { 1937, 0 }, { 1940, 0 },
	{ 1941, 0 }, { 1942, 0 }, { 1943, 0 }, { 1944, 0 }, { 1945, 0 }, { 1946, 0 },
	{ 1947, 0 }, { 1948, 0 }, { 1949, 0 }, { 1950, 0 }, { 1951, 0 }, { 1952, 0 },
	{ 1953, 0 }, { 1954, 0 }, { 1955, 0 }, { 1956, 0 }, { 1957, 0 }, { 1958, 1 },
	{ 1959, 0 }, { 1960, 0 }, { 1961, 0 }, { 1962, 0 }, { 1963, 0 }, { 1964, 0 },
	{ 1965, 0 }, { 1966, 0 }, { 1967, 0 }, { 1968, 0 }, { 1969, 0 }, { 1970, 0 },
	{ 1971, 0 }, { 1972, 0 }, { 1973, 0 }, { 1974, 0 }, { 1975, 0 }, { 1976, 0 },
	{ 1977, 0 }, { 1978, 0 }, { 1979, 0 }, { 1980, 0 }, { 1981, 0 }, { 1982, 0 },
	{ 1983, 0 }, { 1984, 0 }, { 1985, 0 }, { 1986, 0 }, { 1987, 0 }, { 1988, 0 },
	{ 1989, 0 }, { 1990, 0 }, { 1991, 0 }, { 1992, 0 }, { 1993, 0 }, { 1994, 0 },
	{ 1995, 0 }, { 1996, 0 }, { 1997, 0 }, { 1998, 0 }, { 1999, 0 }, { 2000, 1 },
	{ 2001, 0 }, { 2002, 0 }, { 2003, 1 }, { 2004, 0 }, { 2005, 0 }, { 2006, 0 },
	{ 2007, 0 }, { 2008, 0 }, { 2009, 0 }, { 2010, 0 }, { 2011, 0 }, { 2012, 0 },
	{ 2013, 0 }, { 2014, 0 }, { 2015, 0 }, { 2016, 0 }, { 2017, 0 }, { 2018, 0 },
	{ 2019, 0 }, { 2020, 0 }, { 2021, 0 }, { 2022, 0 }, { 2023, 0 }, { 2024, 0 },
	{ 2025, 0 }, { 2026, 0 }, { 2027, 0 }, { 2028, 0 }, { 2029, 0 }, { 2030, 0 },
	{ 2031, 0 }, { 2032, 0 }, { 2033, 0 }, { 2034, 0 }, { 2035, 0 }, { 2036, 0 },
	{ 2037, 0 }, { 2038, 0 }, { 2039, 0 }, { 2040, 0 }, { 2041, 0 }, { 2042, 0 },
	{ 2043, 0 }, { 2044, 0 }, { 2045, 0 }, { 2046, 0 }, { 2047, 0 }, { 2048, 0 },
	{ 2049, 0 }, { 2050, 0 }, { 2051, 0 }, { 2052, 0 }, { 2053, 0 }, { 2054, 0 },
	{ 2055, 0 }, { 2056, 0 }, { 2057, 0 }, { 2058, 0 }, { 2059, 0 }, { 2060, 0 },
	{ 2061, 0 }, { 2062, 0 }, { 2063, 0 }, { 2064, 0 }, { 2065, 0 }, { 2066, 0 },
	{ 2067, 0 }, { 2068, 0 }, { 2069, 0 }, { 2070, 0 }, { 2071, 0 }, { 2072, 0 },
	{ 2073, 0 }, { 2074, 0 }, { 2075, 0 }, { 2076, 0 }, { 2077, 0 }, { 2078, 0 },
	{ 2079, 0 }, { 2080, 0 }, { 2081, 0 }, { 2082, 0 }, { 2083, 0 }, { 2084, 0 },
	{ 2085, 0 }, { 2497, 0 }, { 2498, 0 }, { 2499, 0 }, { 2500, 0 }, { 2506, 0 },
	{ 2507, 3 }, { 2508, 0 }, { 2509, 0 }, { 2510, 0 }, { 2511, 0 }, { 2512, 0 },
	{ 2513, 0 }, { 2514, 0 }, { 2515, 0 }, { 2516, 0 }, { 2517, 0 }, { 2518, 0 },
	{ 2519, 0 }, { 2520, 0 }, { 2521, 0 }, { 2522, 0 }, { 2523, 0 }, { 2524, 0 },
	{ 2525, 0 }, { 2526, 0 }, { 2527, 0 }, { 2528, 0 }, { 2529, 0 }, { 2530, 0 },
	{ 2531, 0 }, { 2532, 0 }, { 2533, 0 }, { 2534, 0 }, { 2535, 0 }, { 2536, 0 },
	{ 2537, 0 }, { 2538, 0 }, { 2539, 0 }, { 2540, 0 }, { 2541, 0 }, { 2542, 0 },
	{ 2543, 0 }, { 2544, 0 }, { 2545, 0 }, { 2546, 0 }, { 2547, 0 }, { 2548, 0 },
	{ 2549, 0 }, { 2550, 0 }, { 2551, 0 }, { 2552, 0 }, { 2553, 0 }, { 2554, 0 },
	{ 2555, 0 }, { 2556, 0 }, { 2557, 0 }, { 2558, 0 }, { 2559, 0 }, { 2560, 0 },
	{ 2561, 0 }, { 2562, 0 }, { 2563, 0 }, { 2564, 0 }, { 2565, 0 }, { 2566, 0 },
	{ 2567, 0 }, { 2568, 0 }, { 2569, 0 }, { 2570, 0 }, { 2571, 0 }, { 2572, 0 },
	{ 2573, 0 }, { 2574, 0 }, { 2575, 0 }, { 2576, 0 }, { 2577, 0 }, { 2578, 0 },
	{ 2579, 0 }, { 2580, 0 }, { 2581, 0 }, { 2582, 0 }, { 2583, 0 }, { 2584, 0 },
	{ 2585, 0 }, { 2586, 0 }, { 2587, 0 }, { 2588, 0 }, { 2589, 0 }, { 2590, 0 },
	{ 2591, 0 }, { 2592, 0 }, { 2593, 0 }, { 2594, 0 }, { 2595, 0 }, { 2596, 0 },
	{ 2597, 0 }, { 2598, 0 }, { 2599, 0 }, { 2600, 0 }, { 2601, 0 }, { 2602, 0 },
	{ 2603, 0 }, { 2604, 0 }, { 2605, 0 }, { 2606, 0 }, { 2607, 0 }, { 2608, 0 },
	{ 2609, 0 }, { 2610, 0 }, { 2611, 0 }, { 2612, 0 }, { 2613, 0 }, { 2614, 0 },
	{ 2615, 0 }, { 2616, 0 }, { 2617, 0 }, { 2618, 0 }, { 2619, 0 }, { 2620, 0 },
	{ 2621, 0 }, { 2622, 0 }, { 2623, 0 }, { 2624, 0 }, { 2625, 0 }, { 2626, 0 },
	{ 2627, 0 }, { 2628, 0 }, { 2629, 0 }, { 2630, 0 }, { 2631, 0 }, { 2632, 0 },
	{ 2633, 0 }, { 2634, 0 }, { 2635, 0 }, { 2636, 0 }, { 2637, 0 }, { 2638, 0 },
	{ 2639, 0 }, { 2640, 0 }, { 2641, 0 }, { 2642, 0 }, { 2643, 0 }, { 2644, 0 },
	{ 2645, 0 }, { 2646, 0 }, { 2647, 0 }, { 2648, 0 }, { 2649, 0 }, { 2650, 0 },
	{ 2651, 0 }, { 2652, 0 }, { 2653, 0 }, { 2654, 0 }, { 2655, 0 }, { 2656, 0 },
	{ 2657, 0 }, { 2658, 0 }, { 2659, 0 }, { 2660, 0 }, { 2661, 0 }, { 2662, 0 },
	{ 2663, 0 }, { 2664, 0 }, { 2665, 0 }, { 2666, 0 }, { 2667, 0 }, { 2668, 0 },
	{ 2669, 0 }, { 2670, 0 }, { 2671, 0 }, { 2672, 0 }, { 2673, 0 }, { 2674, 0 },
	{ 2675, 0 }, { 2676, 0 }, { 2677, 0 }, { 2678, 0 }, { 2679, 0 }, { 2680, 0 },
	{ 2681, 0 }, { 2682, 0 }, { 2683, 0 }, { 2684, 0 }, { 2685, 0 }, { 2686, 0 },
	{ 2687, 0 }, { 2688, 0 }, { 2689, 0 }, { 2690, 0 }, { 2691, 0 }, { 2692, 0 },
	{ 2693, 0 }, { 2694, 0 }, { 2695, 0 }, { 2696, 0 }, { 2697, 0 }, { 2698, 0 },
	{ 2699, 0 }, { 2700, 0 }, { 2701, 0 }, { 2702, 0 }, { 2703, 0 }, { 2704, 0 },
	{ 2705, 0 }, { 2706, 0 }, { 2707, 0 }, { 2708, 0 }, { 2709, 0 }, { 2710, 0 },
	{ 2711, 0 }, { 2712, 0 }, { 2713, 0 }, { 2714, 0 }, { 2715, 0 }, { 2716, 0 },
	{ 2717, 0 }, { 2718, 0 }, { 2719, 0 }, { 2720, 0 }, { 2721, 0 }, { 2722, 0 },
	{ 2723, 0 }, { 2724, 0 }, { 2725, 0 }, { 2726, 0 }, { 2727, 0 }, { 2728, 0 },
	{ 2729, 0 }, { 2730, 0 }, { 2731, 0 }, { 2732, 0 }, { 2733, 0 }, { 2734, 0 },
	{ 2735, 0 }, { 2736, 0 }, { 2737, 0 }, { 2738, 0 }, { 2739, 0 }, { 2740, 0 },
	{ 2741, 0 }, { 2742, 0 }, { 2743, 0 }, { 2744, 0 }, { 2745, 0 }, { 2746, 0 },
	{ 2747, 0 }, { 2748, 0 }, { 2749, 0 }, { 2750, 0 }, { 2751, 0 }, { 2752, 0 },
	{ 2753, 0 }, { 2754, 0 }, { 2755, 0 }, { 2756, 0 }, { 2757, 0 }, { 2758, 0 },
	{ 2759, 0 }, { 2760, 0 }, { 2761, 0 }, { 2762, 0 }, { 2763, 0 }, { 2764, 0 },
	{ 2765, 0 }, { 2766, 0 }, { 2767, 0 }, { 2768, 0 }, { 2769, 0 }, { 2770, 0 },
	{ 2771, 0 }, { 2772, 0 }, { 2773, 0 }, { 2774, 0 }, { 2775, 0 }, { 2776, 0 },
	{ 2777, 0 }, { 2778, 0 }, { 2779, 0 }, { 2780, 0 }, { 2781, 0 }, { 2782, 0 },
	{ 2783, 0 }, { 2784, 0 }, { 2785, 0 }, { 2786, 0 }, { 2787, 0 }, { 2788, 0 },
	{ 2789, 0 }, { 2790, 0 }, { 2791, 0 }, { 2792, 0 }, { 2793, 0 }, { 2794, 0 },
	{ 2795, 0 }, { 2796, 0 }, { 2797, 0 }, { 2798, 0 }, { 2799, 0 }, { 2800, 0 },
	{ 2801, 0 }, { 2802, 0 }, { 2803, 0 }, { 2804, 0 }, { 2805, 0 }, { 2806, 0 },
	{ 2807, 0 }, { 2808, 0 }, { 2809, 0 }, { 2810, 0 }, { 2811, 0 }, { 2812, 0 },
	{ 2813, 0 }, { 2814, 0 }, { 2815, 0 }, { 2816, 0 }, { 2817, 0 }, { 2818, 0 },
	{ 2819, 0 }, { 2820, 0 }, { 2821, 0 }, { 2822, 0 }, { 2823, 0 }, { 2824, 0 },
	{ 2825, 5 }, { 2826, 0 }, { 2827, 0 }, { 2828, 0 }, { 2829, 0 }, { 2830, 0 },
	{ 2831, 0 }, { 2832, 0 }, { 2833, 0 }, { 2834, 0 }, { 2835, 0 }, { 2836, 0 },
	{ 2837, 0 }, { 2838, 0 }, { 2839, 0 }, { 2840, 0 }, { 2841, 0 }, { 2842, 0 },
	{ 2843, 0 }, { 2844, 0 }, { 2845, 0 }, { 2846, 0 }, { 2847, 0 }, { 2848, 0 },
	{ 2849, 0 }, { 2850, 0 }, { 2851, 0 }, { 2852, 0 }, { 2853, 0 }, { 2854, 0 },
	{ 2855, 0 }, { 2856, 0 }, { 2857, 0 }, { 2858, 0 }, { 2859, 0 }, { 2860, 0 },
	{ 2861, 0 }, { 2862, 0 }, { 2863, 0 }, { 2864, 0 }, { 2865, 0 }, { 2866, 0 },
	{ 2867, 0 }, { 2868, 0 }, { 2869, 0 }, { 2870, 0 }, { 2871, 0 }, { 2872, 0 },
	{ 2873, 0 }, { 2874, 0 }, { 2875, 0 }, { 2876, 0 }, { 2877, 0 }, { 2878, 0 },
	{ 2879, 0 }, { 2880, 0 }, { 2881, 0 }, { 2882, 0 }, { 2883, 0 }, { 2884, 0 },
	{ 2885, 0 }, { 2886, 0 }, { 2887, 0 }, { 2888, 0 }, { 2889, 0 }, { 2890, 0 },
	{ 2891, 0 }, { 2892, 0 }, { 2893, 0 }, { 2894, 0 }, { 2895, 0 }, { 2896, 0 },
	{ 2897, 0 }, { 2898, 0 }, { 2899, 0 }, { 2900, 0 }, { 2901, 0 }, { 2902, 0 },
	{ 2903, 0 }, { 2904, 0 }, { 2905, 0 }, { 2906, 0 }, { 2907, 0 }, { 2908, 0 },
	{ 2909, 0 }, { 2910, 0 }, { 2911, 0 }, { 2912, 0 }, { 2913, 0 }, { 2914, 0 },
	{ 2915, 0 }, { 2916, 0 }, { 2917, 0 }, { 2918, 0 }, { 2919, 0 }, { 2920, 0 },
	{ 2921, 0 }, { 2922, 0 }, { 2923, 0 }, { 2924, 0 }, { 2925, 0 }, { 2926, 0 },
	{ 2927, 0 }, { 2928, 0 }, { 2929, 0 }, { 2930, 0 }, { 2931, 0 }, { 2932, 0 },
	{ 2933, 0 }, { 2934, 0 }, { 2935, 0 }, { 2936, 0 }, { 2937, 0 }, { 2938, 0 },
	{ 2939, 0 }, { 2940, 0 }, { 2941, 0 }, { 2942, 0 }, { 2943, 0 }, { 2944, 0 },
	{ 2945, 0 }, { 2946, 0 }, { 2947, 0 }, { 2948, 0 }, { 2949, 0 }, { 2950, 0 },
	{ 2951, 0 }, { 2952, 0 }, { 2953, 0 }, { 2954, 1 }, { 2955, 0 }, { 2956, 0 },
	{ 2957, 0 }, { 2958, 0 }, { 2959, 0 }, { 2960, 0 }, { 2961, 0 }, { 2962, 0 },
	{ 2963, 0 }, { 2964, 0 }, { 2965, 0 }, { 2966, 0 }, { 2967, 0 }, { 2968, 0 },
	{ 2969, 0 }, { 2970, 0 }, { 2971, 0 }, { 2972, 0 }, { 2973, 0 }, { 2974, 0 },
	{ 2975, 0 }, { 2976, 0 }, { 2977, 0 }, { 2978, 0 }, { 2979, 0 }, { 2980, 0 },
	{ 2981, 0 }, { 2982, 0 }, { 2983, 0 }, { 2984, 0 }, { 2985, 0 }, { 2986, 0 },
	{ 2987, 0 }, { 2988, 0 }, { 2989, 0 }, { 2990, 0 }, { 2991, 0 }, { 2992, 0 },
	{ 2993, 0 }, { 2994, 0 }, { 2995, 0 }, { 2996, 0 }, { 2997, 0 }, { 2998, 0 },
	{ 2999, 0 }, { 3000, 0 }, { 3001, 0 }, { 3002, 0 }, { 3003, 0 }, { 3004, 0 },
	{ 3005, 0 }, { 3006, 0 }, { 3007, 0 }, { 3008, 0 }, { 3009, 0 }, { 3010, 0 },
	{ 3011, 0 }, { 3012, 0 }, { 3013, 0 }, { 3014, 0 }, { 3015, 0 }, { 3016, 0 },
	{ 3017, 0 }, { 3018, 0 }, { 3019, 0 }, { 3020, 0 }, { 3021, 0 }, { 3022, 0 },
	{ 3023, 0 }, { 3024, 0 }, { 3025, 0 }, { 3026, 0 }, { 3027, 0 }, { 3028, 0 },
	{ 3029, 0 }, { 3030, 0 }, { 3031, 0 }, { 3032, 0 }, { 3033, 0 }, { 3034, 0 },
	{ 3035, 0 }, { 3036, 0 }, { 3037, 0 }, { 3038, 0 }, { 3039, 0 }, { 3040, 0 },
	{ 3041, 0 }, { 3042, 0 }, { 3043, 0 }, { 3044, 0 }, { 3045, 0 }, { 3046, 0 },
	{ 3047, 0 }, { 3048, 0 }, { 3049, 0 }, { 3050, 0 }, { 3051, 0 }, { 3052, 0 },
	{ 3053, 0 }, { 3054, 0 }, { 3059, 0 }, { 3060, 0 }, { 3061, 0 }, { 3062, 0 },
	{ 3063, 0 }, { 3064, 0 }, { 3065, 0 }, { 3066, 0 }, { 3067, 0 }, { 3068, 0 },
	{ 3069, 0 }, { 3070, 0 }, { 3071, 0 }, { 3072, 0 }, { 3073, 0 }, { 3074, 0 },
	{ 3075, 0 }, { 3076, 0 }, { 3077, 0 }, { 3078, 0 }, { 3079, 0 }, { 3080, 0 },
	{ 3081, 0 }, { 3082, 0 }, { 3083, 0 }, { 3084, 0 }, { 3085, 0 }, { 3086, 0 },
	{ 3087, 0 }, { 3088, 0 }, { 3089, 0 }, { 3090, 0 }, { 3091, 0 }, { 3092, 0 },
	{ 3093, 0 }, { 3094, 0 }, { 3095, 0 }, { 3096, 0 }, { 3097, 0 }, { 3098, 0 },
	{ 3099, 0 }, { 3100, 0 }, { 3101, 0 }, { 3102, 0 }, { 3103, 0 }, { 3104, 0 },
	{ 3105, 0 }, { 3106, 0 }, { 3107, 0 }, { 3108, 0 }, { 3109, 0 }, { 3110, 0 },
	{ 3111, 0 }, { 3112, 0 }, { 3113, 0 }, { 3114, 0 }, { 3115, 0 }, { 3116, 0 },
	{ 3117, 0 }, { 3118, 0 }, { 3119, 0 }, { 3120, 0 }, { 3121, 0 }, { 3122, 0 },
	{ 3123, 0 }, { 3124, 0 }, { 3125, 0 }, { 3126, 0 }, { 3127, 0 }, { 3128, 0 },
	{ 3129, 0 }, { 3130, 0 }, { 3131, 0 }, { 3132, 0 }, { 3133, 0 }, { 3134, 0 },
	{ 3135, 0 }, { 3136, 0 }, { 3137, 0 }, { 3138, 0 }, { 3139, 0 }, { 3140, 0 },
	{ 3141, 0 }, { 3142, 0 }, { 3143, 0 }, { 3144, 0 }, { 3145, 0 }, { 3146, 0 },
	{ 3147, 0 }, { 3148, 0 }, { 3149, 0 }, { 3150, 0 }, { 3151, 0 }, { 3152, 0 },
	{ 3153, 0 }, { 3154, 0 }, { 3155, 0 }, { 3156, 0 }, { 3157, 0 }, { 3158, 0 },
	{ 3159, 0 }, { 3160, 0 }, { 3161, 0 }, { 3162, 0 }, { 3163, 0 }, { 3164, 0 },
	{ 3165, 0 }, { 3166, 0 }, { 3167, 0 }, { 3168, 0 }, { 3169, 0 }, { 3170, 0 },
	{ 3171, 0 }, { 3172, 0 }, { 3173, 0 }, { 3174, 0 }, { 3175, 0 }, { 3176, 0 },
	{ 3177, 0 }, { 3178, 0 }, { 3179, 0 }, { 3180, 0 }, { 3181, 0 }, { 3182, 0 },
	{ 3183, 0 }, { 3184, 0 }, { 3185, 0 }, { 3186, 0 }, { 3187, 0 }, { 3188, 0 },
	{ 3189, 0 }, { 3190, 0 }, { 3191, 0 }, { 3192, 0 }, { 3193, 0 }, { 3194, 0 },
	{ 3195, 0 }, { 3196, 0 }, { 3197, 0 }, { 3198, 0 }, { 3199, 0 }, { 3200, 0 },
	{ 3201, 0 }, { 3202, 0 }, { 3203, 0 }, { 3204, 0 }, { 3205, 0 }, { 3206, 0 },
	{ 3207, 0 }, { 3208, 0 }, { 3209, 0 }, { 3210, 0 }, { 3211, 0 }, { 3212, 0 },
	{ 3213, 0 }, { 3214, 0 }, { 3215, 0 }, { 3216, 0 }, { 3217, 0 }, { 3218, 0 },
	{ 3219, 0 }, { 3220, 0 }, { 3221, 0 }, { 3222, 0 }, { 3223, 0 }, { 3224, 0 },
	{ 3225, 0 }, { 3226, 0 }, { 3227, 0 }, { 3228, 0 }, { 3229, 0 }, { 3230, 0 },
	{ 3231, 0 }, { 3232, 0 }, { 3233, 0 }, { 3234, 0 }, { 3235, 0 }, { 3236, 0 },
	{ 3237, 0 }, { 3238, 0 }, { 3239, 0 }, { 3240, 0 }, { 3241, 0 }, { 3242, 0 },
	{ 3243, 0 }, { 3244, 0 }, { 3245, 0 }, { 3246, 0 }, { 3247, 0 }, { 3248, 0 },
	{ 3249, 0 }, { 3250, 0 }, { 3251, 0 }, { 3253, 0 }, { 3254, 0 }, { 3255, 0 },
	{ 3256, 0 }, { 3257, 0 }, { 3258, 0 }, { 3259, 0 }, { 3260, 0 }, { 3261, 0 },
	{ 3262, 0 }, { 3263, 0 }, { 3264, 0 }, { 3265, 0 }, { 3266, 0 }, { 3267, 0 },
	{ 3268, 0 }, { 3269, 0 }, { 3270, 0 }, { 3271, 0 }, { 3272, 0 }, { 3273, 0 },
	{ 3274, 0 }, { 3275, 0 }, { 3276, 0 }, { 3277, 0 }, { 3278, 0 }, { 3279, 0 },
	{ 3280, 0 }, { 3281, 0 }, { 3282, 0 }, { 3283, 0 }, { 3284, 0 }, { 3285, 0 },
	{ 3286, 0 }, { 3287, 0 }, { 3288, 0 }, { 3289, 0 }, { 3290, 0 }, { 3291, 0 },
	{ 3292, 0 }, { 3293, 0 }, { 3294, 0 }, { 3295, 0 }, { 3296, 0 }, { 3297, 0 },
	{ 3298, 0 }, { 3299, 0 }, { 3300, 0 }, { 3301, 0 }, { 3302, 0 }, { 3303, 0 },
	{ 3304, 0 }, { 3305, 0 }, { 3306, 0 }, { 3307, 0 }, { 3308, 0 }, { 3309, 0 },
	{ 3310, 0 }, { 3311, 0 }, { 3312, 0 }, { 3313, 0 }, { 3314, 0 }, { 3315, 0 },
	{ 3316, 0 }, { 3317, 0 }, { 3318, 0 }, { 3319, 0 }, { 3320, 0 }, { 3321, 0 },
	{ 3322, 0 }, { 3323, 0 }, { 3324, 0 }, { 3325, 0 }, { 3326, 0 }, { 3327, 0 },
	{ 3328, 0 }, { 3329, 0 }, { 3330, 0 }, { 3331, 0 }, { 3332, 0 }, { 3333, 0 },
	{ 3334, 0 }, { 3335, 0 }, { 3336, 0 }, { 3337, 0 }, { 3338, 0 }, { 3339, 0 },
	{ 3340, 0 }, { 3342, 0 }, { 3343, 0 }, { 3344, 0 }, { 3345, 0 }, { 3346, 0 },
	{ 3347, 0 }, { 3348, 0 }, { 3349, 0 }, { 3350, 0 }, { 3351, 0 }, { 3352, 0 },
	{ 3353, 0 }, { 3354, 0 }, { 3355, 0 }, { 3356, 0 }, { 3357, 0 }, { 3358, 0 },
	{ 3359, 0 }, { 3360, 0 }, { 3361, 0 }, { 3362, 0 }, { 3363, 3 }, { 3364, 0 },
	{ 3365, 0 }, { 3366, 0 }, { 3367, 0 }, { 3368, 0 }, { 3369, 0 }, { 3370, 0 },
	{ 3371, 0 }, { 3372, 0 }, { 3373, 0 }, { 3374, 0 }, { 3375, 0 }, { 3376, 0 },
	{ 3377, 0 }, { 3378, 0 }, { 3379, 0 }, { 3380, 0 }, { 3381, 0 }, { 3383, 0 },
	{ 3384, 0 }, { 3385, 0 }, { 3386, 0 }, { 3387, 0 }, { 3388, 0 }, { 3389, 0 },
	{ 3390, 0 }, { 3391, 0 }, { 3392, 0 }, { 3393, 0 }, { 3394, 0 }, { 3395, 0 },
	{ 3396, 0 }, { 3397, 0 }, { 3398, 0 }, { 3399, 0 }, { 3400, 0 }, { 3401, 0 },
	{ 3402, 0 }, { 3403, 0 }, { 3404, 0 }, { 3405, 0 }, { 3406, 0 }, { 3407, 0 },
	{ 3408, 0 }, { 3409, 0 }, { 3410, 0 }, { 3411, 0 }, { 3412, 0 }, { 3413, 0 },
	{ 3414, 0 }, { 3415, 0 }, { 3416, 0 }, { 3417, 0 }, { 3418, 0 }, { 3419, 0 },
	{ 3420, 0 }, { 3421, 0 }, { 3422, 0 }, { 3423, 0 }, { 3424, 0 }, { 3425, 0 },
	{ 3426, 0 }, { 3427, 0 }, { 3428, 0 }, { 3429, 0 }, { 3430, 0 }, { 3431, 0 },
	{ 3432, 1 }, { 3433, 1 }, { 3434, 1 }, { 3435, 1 }, { 3436, 1 }, { 3437, 1 },
	{ 3438, 1 }, { 3439, 1 }, { 3440, 0 }, { 3441, 0 }, { 3442, 0 }, { 3443, 0 },
	{ 3444, 0 }, { 3445, 0 }, { 3446, 0 }, { 3447, 0 }, { 3448, 0 }, { 3449, 0 },
	{ 3450, 0 }, { 3451, 0 }, { 3452, 0 }, { 3453, 0 }, { 3454, 0 }, { 3455, 0 },
	{ 3456, 0 }, { 3457, 0 }, { 3458, 0 }, { 3459, 0 }, { 3460, 0 }, { 3461, 0 },
	{ 3463, 0 }, { 3464, 0 }, { 3465, 0 }, { 3466, 0 }, { 3467, 0 }, { 3468, 0 },
	{ 3469, 0 }, { 3470, 0 }, { 3471, 0 }, { 3472, 0 }, { 3473, 0 }, { 3474, 0 },
	{ 3475, 0 }, { 3476, 0 }, { 3477, 0 }, { 3478, 0 }, { 3479, 0 }, { 3480, 0 },
	{ 3481, 0 }, { 3482, 0 }, { 3483, 0 }, { 3484, 0 }, { 3485, 0 }, { 3486, 0 },
	{ 3487, 0 }, { 3488, 0 }, { 3489, 0 }, { 3490, 0 }, { 3491, 0 }, { 3492, 0 },
	{ 3493, 0 }, { 3494, 0 }, { 3495, 0 }, { 3496, 0 }, { 3497, 0 }, { 3498, 0 },
	{ 3499, 0 }, { 3500, 0 }, { 3501, 0 }, { 3502, 0 }, { 3503, 0 }, { 3504, 0 },
	{ 3505, 0 }, { 3506, 0 }, { 3507, 0 }, { 3508, 0 }, { 3509, 0 }, { 3510, 0 },
	{ 3511, 0 }, { 3512, 0 }, { 3513, 0 }, { 3514, 0 }, { 3515, 0 }, { 3516, 0 },
	{ 3517, 0 }, { 3518, 0 }, { 3519, 0 }, { 3520, 0 }, { 3521, 0 }, { 3522, 0 },
	{ 3523, 0 }, { 3524, 0 }, { 3525, 0 }, { 3526, 0 }, { 3527, 0 }, { 3528, 0 },
	{ 3529, 0 }, { 3530, 0 }, { 3531, 0 }, { 3532, 0 }, { 3533, 0 }, { 3534, 0 },
	{ 3535, 0 }, { 3536, 0 }, { 3537, 0 }, { 3538, 0 }, { 3539, 0 }, { 3540, 0 },
	{ 3541, 0 }, { 3542, 0 }, { 3543, 0 }, { 3544, 0 }, { 3545, 0 }, { 3546, 0 },
	{ 3547, 0 }, { 3548, 0 }, { 3549, 0 }, { 3550, 0 }, { 3551, 0 }, { 3552, 0 },
	{ 3553, 0 }, { 3554, 0 }, { 3555, 0 }, { 3556, 0 }, { 3557, 0 }, { 3558, 0 },
	{ 3559, 0 }, { 3560, 0 }, { 3561, 0 }, { 3562, 0 }, { 3563, 0 }, { 3564, 0 },
	{ 3565, 0 }, { 3566, 0 }, { 3567, 0 }, { 3568, 0 }, { 3569, 0 }, { 3570, 0 },
	{ 3571, 0 }, { 3572, 0 }, { 3573, 0 }, { 3574, 0 }, { 3575, 0 }, { 3576, 0 },
	{ 3577, 0 }, { 3578, 0 }, { 3579, 0 }, { 3580, 0 }, { 3581, 0 }, { 3582, 0 },
	{ 3583, 0 }, { 3584, 0 }, { 3585, 0 }, { 3586, 0 }, { 3587, 0 }, { 3588, 0 },
	{ 3589, 0 }, { 3590, 0 }, { 3591, 0 }, { 3592, 0 }, { 3593, 0 }, { 3594, 0 },
	{ 3595, 0 }, { 3596, 0 }, { 3597, 0 }, { 3598, 0 }, { 3599, 0 }, { 3600, 0 },
	{ 3601, 0 }, { 3602, 0 }, { 3603, 0 }, { 3604, 0 }, { 3605, 0 }, { 3606, 0 },
	{ 3607, 0 }, { 3608, 0 }, { 3609, 0 }, { 3610, 0 }, { 3611, 0 }, { 3612, 0 },
	{ 3613, 0 }, { 3614, 0 }, { 3615, 0 }, { 3616, 0 }, { 3617, 0 }, { 3618, 0 },
	{ 3619, 0 }, { 3620, 0 }, { 3621, 0 }, { 3622, 0 }, { 3623, 0 }, { 3624, 0 },
	{ 3625, 0 }, { 3626, 0 }, { 3627, 0 }, { 3628, 0 }, { 3629, 0 }, { 3630, 0 },
	{ 3631, 0 }, { 3632, 0 }, { 3633, 0 }, { 3634, 0 }, { 3635, 0 }, { 3636, 0 },
	{ 3637, 0 }, { 3638, 0 }, { 3639, 0 }, { 3640, 0 }, { 3641, 0 }, { 3642, 0 },
	{ 3643, 0 }, { 3644, 0 }, { 3645, 0 }, { 3646, 0 }, { 3647, 0 }, { 3648, 0 },
	{ 3649, 0 }, { 3650, 0 }, { 3651, 0 }, { 3652, 0 }, { 3653, 0 }, { 3654, 0 },
	{ 3655, 0 }, { 3656, 0 }, { 3657, 0 }, { 3658, 0 }, { 3659, 0 }, { 3660, 0 },
	{ 3661, 0 }, { 3662, 0 }, { 3663, 0 }, { 3664, 0 }, { 3665, 0 }, { 3666, 0 },
	{ 3667, 0 }, { 3668, 0 }, { 3669, 0 }, { 3670, 0 }, { 3671, 0 }, { 3672, 0 },
	{ 3673, 0 }, { 3674, 0 }, { 3675, 0 }, { 3676, 0 }, { 3677, 0 }, { 3678, 0 },
	{ 3679, 0 }, { 3680, 0 }, { 3681, 0 }, { 3682, 0 }, { 3683, 0 }, { 3684, 0 },
	{ 3685, 0 }, { 3686, 0 }, { 3687, 0 }, { 3688, 0 }, { 3689, 0 }, { 3690, 0 },
	{ 3691, 0 }, { 3692, 0 }, { 3693, 0 }, { 3694, 0 }, { 3695, 0 }, { 3696, 0 },
	{ 3697, 0 }, { 3698, 0 }, { 3699, 0 }, { 3700, 0 }, { 3701, 0 }, { 3702, 0 },
	{ 3703, 0 }, { 3704, 0 }, { 3705, 0 }, { 3706, 0 }, { 3707, 0 }, { 3708, 0 },
	{ 3709, 0 }, { 3710, 0 }, { 3711, 0 }, { 3712, 0 }, { 3713, 0 }, { 3714, 0 },
	{ 3715, 0 }, { 3716, 0 }, { 3717, 0 }, { 3718, 0 }, { 3719, 0 }, { 3720, 0 },
	{ 3721, 0 }, { 3722, 0 }, { 3723, 0 }, { 3724, 0 }, { 3725, 0 }, { 3726, 0 },
	{ 3727, 0 }, { 3728, 0 }, { 3729, 0 }, { 3730, 0 }, { 3731, 0 }, { 3732, 0 },
	{ 3733, 0 }, { 3734, 0 }, { 3735, 0 }, { 3736, 0 }, { 3737, 0 }, { 3738, 0 },
	{ 3739, 0 }, { 3740, 0 }, { 3741, 0 }, { 3742, 0 }, { 3743, 0 }, { 3744, 0 },
	{ 3745, 0 }, { 3746, 0 }, { 3747, 0 }, { 3748, 0 }, { 3749, 0 }, { 3750, 0 },
	{ 3751, 0 }, { 3752, 0 }, { 3753, 0 }, { 3754, 0 }, { 3755, 0 }, { 3756, 0 },
	{ 3758, 0 }, { 3759, 0 }, { 3760, 0 }, { 3761, 0 }, { 3762, 0 }, { 3763, 0 },
	{ 3764, 0 }, { 3765, 0 }, { 3766, 0 }, { 3767, 0 }, { 3768, 0 }, { 3769, 0 },
	{ 3770, 0 }, { 3771, 0 }, { 3772, 0 }, { 3773, 0 }, { 3774, 0 }, { 3775, 0 },
	{ 3776, 0 }, { 3777, 0 }, { 3778, 0 }, { 3779, 0 }, { 3780, 0 }, { 3781, 0 },
	{ 3782, 0 }, { 3783, 0 }, { 3784, 0 }, { 3785, 0 }, { 3786, 0 }, { 3787, 0 },
	{ 3788, 0 }, { 3789, 0 }, { 3790, 0 }, { 3791, 0 }, { 3792, 0 }, { 3793, 0 },
	{ 3794, 0 }, { 3795, 0 }, { 3796, 0 }, { 3797, 0 }, { 3798, 0 }, { 3799, 0 },
	{ 3800, 0 }, { 3801, 0 }, { 3802, 0 }, { 3803, 0 }, { 3804, 0 }, { 3805, 0 },
	{ 3806, 0 }, { 3807, 0 }, { 3808, 0 }, { 3809, 0 }, { 3810, 0 }, { 3811, 0 },
	{ 3812, 0 }, { 3813, 0 }, { 3814, 0 }, { 3815, 0 }, { 3816, 0 }, { 3817, 0 },
	{ 3818, 0 }, { 3819, 0 }, { 3820, 0 }, { 3821, 0 }, { 3822, 0 }, { 3823, 0 },
	{ 3824, 0 }, { 3825, 0 }, { 3826, 0 }, { 3827, 0 }, { 3828, 0 }, { 3829, 0 },
	{ 3830, 0 }, { 3831, 0 }, { 3832, 0 }, { 3833, 0 }, { 3834, 0 }, { 3835, 0 },
	{ 3836, 0 }, { 3837, 0 }, { 3838, 0 }, { 3839, 0 }, { 3840, 0 }, { 3841, 0 },
	{ 3842, 0 }, { 3843, 0 }, { 3844, 0 }, { 3845, 0 }, { 3846, 0 }, { 3847, 0 },
	{ 3848, 0 }, { 3849, 0 }, { 3850, 0 }, { 3851, 0 }, { 3852, 0 }, { 3853, 0 },
	{ 3854, 0 }, { 3855, 0 }, { 3856, 0 }, { 3857, 0 }, { 3858, 0 }, { 3859, 0 },
	{ 3860, 0 }, { 3861, 0 }, { 3862, 0 }, { 3863, 0 }, { 3864, 0 }, { 3865, 0 },
	{ 3866, 0 }, { 3867, 0 }, { 3868, 0 }, { 3869, 0 }, { 3870, 0 }, { 3871, 0 },
	{ 3872, 0 }, { 3873, 0 }, { 3874, 0 }, { 3875, 0 }, { 3876, 0 }, { 3877, 0 },
	{ 3878, 0 }, { 3879, 0 }, { 3880, 0 }, { 3881, 0 }, { 3882, 0 }, { 3883, 0 },
	{ 3884, 0 }, { 3885, 0 }, { 3886, 0 }, { 3887, 0 }, { 3888, 0 }, { 3889, 0 },
	{ 3890, 0 }, { 3891, 0 }, { 3892, 0 }, { 3893, 0 }, { 3894, 0 }, { 3895, 0 },
	{ 3896, 0 }, { 3897, 0 }, { 3898, 0 }, { 3899, 0 }, { 3900, 0 }, { 3901, 0 },
	{ 3902, 0 }, { 3903, 0 }, { 3904, 0 }, { 3905, 0 }, { 3906, 0 }, { 3907, 0 },
	{ 3908, 0 }, { 3909, 0 }, { 3910, 0 }, { 3911, 0 }, { 3912, 0 }, { 3913, 0 },
	{ 3914, 0 }, { 3915, 0 }, { 3916, 0 }, { 3917, 0 }, { 3918, 0 }, { 3919, 0 },
	{ 3920, 0 }, { 3921, 0 }, { 3922, 0 }, { 3923, 0 }, { 3924, 0 }, { 3925, 0 },
	{ 3926, 0 }, { 3927, 0 }, { 3928, 0 }, { 3929, 0 }, { 3930, 0 }, { 3931, 0 },
	{ 3932, 0 }, { 3933, 0 }, { 3934, 0 }, { 3935, 0 }, { 3936, 0 }, { 3937, 0 },
	{ 3938, 0 }, { 3939, 0 }, { 3940, 0 }, { 3941, 0 }, { 3942, 0 }, { 3943, 0 },
	{ 3944, 0 }, { 3945, 0 }, { 3946, 0 }, { 3947, 0 }, { 3948, 0 }, { 3949, 0 },
	{ 3950, 0 }, { 3951, 0 }, { 3952, 0 }, { 3953, 0 }, { 3954, 0 }, { 3955, 0 },
	{ 3956, 0 }, { 3957, 0 }, { 3958, 0 }, { 3959, 0 }, { 3960, 0 }, { 3961, 0 },
	{ 3962, 0 }, { 3963, 0 }, { 3964, 0 }, { 3965, 0 }, { 3966, 0 }, { 3967, 0 },
	{ 3968, 0 }, { 3969, 0 }, { 3970, 0 }, { 3971, 0 }, { 3972, 0 }, { 3973, 0 },
	{ 3974, 0 }, { 3975, 0 }, { 3976, 0 }, { 3977, 0 }, { 3978, 0 }, { 3979, 0 },
	{ 3980, 0 }, { 3981, 0 }, { 3982, 0 }, { 3983, 0 }, { 3984, 0 }, { 3985, 0 },
	{ 3986, 0 }, { 3987, 0 }, { 3988, 0 }, { 3989, 0 }, { 3990, 0 }, { 3991, 0 },
	{ 3992, 0 }, { 3993, 0 }, { 3994, 0 }, { 3995, 0 }, { 3996, 0 }, { 3997, 0 },
	{ 3998, 0 }, { 3999, 0 }, { 4000, 0 }, { 4001, 0 }, { 4002, 0 }, { 4003, 0 },
	{ 4004, 0 }, { 4005, 0 }, { 4006, 0 }, { 4007, 0 }, { 4008, 0 }, { 4009, 0 },
	{ 4010, 0 }, { 4011, 0 }, { 4012, 0 }, { 4013, 0 }, { 4014, 0 }, { 4015, 0 },
	{ 4016, 0 }, { 4017, 0 }, { 4018, 0 }, { 4019, 0 }, { 4020, 0 }, { 4021, 0 },
	{ 4022, 0 }, { 4023, 0 }, { 4024, 0 }, { 4025, 0 }, { 4026, 0 }, { 4027, 0 },
	{ 4028, 0 }, { 4029, 0 }, { 4030, 0 }, { 4031, 0 }, { 4032, 0 }, { 4033, 0 },
	{ 4034, 0 }, { 4035, 0 }, { 4036, 0 }, { 4037, 0 }, { 4038, 0 }, { 4039, 0 },
	{ 4040, 0 }, { 4041, 0 }, { 4042, 0 }, { 4043, 0 }, { 4044, 0 }, { 4045, 0 },
	{ 4046, 0 }, { 4047, 0 }, { 4048, 0 }, { 4049, 0 }, { 4050, 0 }, { 4051, 0 },
	{ 4052, 0 }, { 4053, 0 }, { 4054, 0 }, { 4055, 0 }, { 4056, 0 }, { 4057, 0 },
	{ 4058, 0 }, { 4059, 0 }, { 4060, 0 }, { 4061, 0 }, { 4062, 0 }, { 4063, 0 },
	{ 4064, 0 }, { 4065, 0 }, { 4066, 0 }, { 4067, 0 }, { 4068, 0 }, { 4069, 0 },
	{ 4070, 0 }, { 4071, 0 }, { 4072, 0 }, { 4073, 0 }, { 4074, 0 }, { 4075, 0 },
	{ 4076, 0 }, { 4077, 0 }, { 4078, 0 }, { 4079, 0 }, { 4080, 0 }, { 4081, 0 },
	{ 4082, 0 }, { 4083, 0 }, { 4084, 0 }, { 4085, 0 }, { 4086, 0 }, { 4087, 0 },
	{ 4088, 0 }, { 4089, 0 }, { 4090, 0 }, { 4091, 0 }, { 4092, 0 }, { 4093, 0 },
	{ 4094, 0 }, { 4095, 0 }, { 4096, 0 }, { 4097, 0 }, { 4098, 0 }, { 4099, 0 },
	{ 4100, 0 }, { 4101, 0 }, { 4102, 0 }, { 4103, 0 }, { 4104, 0 }, { 4105, 0 },
	{ 4106, 0 }, { 4107, 0 }, { 4108, 0 }, { 4109, 0 }, { 4110, 0 }, { 4111, 0 },
	{ 4112, 0 }, { 4113, 0 }, { 4114, 0 }, { 4115, 0 }, { 4116, 0 }, { 4117, 0 },
	{ 4118, 0 }, { 4119, 0 }, { 4120, 0 }, { 4121, 0 }, { 4122, 3 }, { 4123, 0 },
	{ 4124, 0 }, { 4125, 0 }, { 4126, 0 }, { 4127, 0 }, { 4128, 0 }, { 4129, 0 },
	{ 4130, 0 }, { 4131, 0 }, { 4132, 0 }, { 4133, 0 }, { 4134, 0 }, { 4135, 0 },
	{ 4136, 0 }, { 4137, 0 }, { 4138, 0 }, { 4139, 0 }, { 4140, 0 }, { 4141, 0 },
	{ 4142, 0 }, { 4143, 0 }, { 4144, 0 }, { 4145, 0 }, { 4146, 0 }, { 4147, 0 },
	{ 4148, 0 }, { 4149, 0 }, { 4150, 0 }, { 4151, 0 }, { 4152, 0 }, { 4153, 0 },
	{ 4154, 0 }, { 4155, 0 }, { 4156, 0 }, { 4157, 0 }, { 4158, 0 }, { 4159, 0 },
	{ 4160, 0 }, { 4161, 0 }, { 4162, 0 }, { 4163, 0 }, { 4164, 0 }, { 4165, 0 },
	{ 4166, 0 }, { 4167, 0 }, { 4168, 0 }, { 4169, 0 }, { 4170, 0 }, { 4171, 0 },
	{ 4172, 0 }, { 4173, 0 }, { 4174, 0 }, { 4175, 0 }, { 4176, 0 }, { 4177, 0 },
	{ 4178, 0 }, { 4179, 0 }, { 4180, 0 }, { 4181, 0 }, { 4182, 0 }, { 4183, 0 },
	{ 4184, 0 }, { 4185, 0 }, { 4186, 0 }, { 4187, 0 }, { 4188, 0 }, { 4189, 0 },
	{ 4190, 0 }, { 4191, 0 }, { 4192, 0 }, { 4193, 0 }, { 4194, 0 }, { 4195, 0 },
	{ 4196, 0 }, { 4197, 0 }, { 4198, 0 }, { 4199, 0 }, { 4200, 0 }, { 4201, 0 },
	{ 4202, 3 }, { 4203, 0 }, { 4204, 0 }, { 4205, 0 }, { 4206, 0 }, { 4207, 0 },
	{ 4208, 0 }, { 4209, 0 }, { 4210, 0 }, { 4211, 0 }, { 4212, 0 }, { 4213, 0 },
	{ 4214, 0 }, { 4215, 0 }, { 4216, 0 }, { 4217, 0 }, { 4218, 0 }, { 4219, 0 },
	{ 4220, 0 }, { 4221, 0 }, { 4222, 0 }, { 4223, 0 }, { 4224, 0 }, { 4225, 0 },
	{ 4226, 0 }, { 4227, 0 }, { 4228, 0 }, { 4229, 0 }, { 4230, 0 }, { 4231, 0 },
	{ 4232, 0 }, { 4233, 0 }, { 4234, 0 }, { 4235, 0 }, { 4236, 0 }, { 4237, 0 },
	{ 4238, 0 }, { 4239, 0 }, { 4240, 0 }, { 4241, 0 }, { 4242, 0 }, { 4243, 0 },
	{ 4244, 0 }, { 4245, 0 }, { 4246, 0 }, { 4247, 0 }, { 4248, 0 }, { 4249, 0 },
	{ 4250, 0 }, { 4251, 0 }, { 4252, 0 }, { 4253, 0 }, { 4254, 0 }, { 4255, 0 },
	{ 4256, 0 }, { 4257, 0 }, { 4258, 0 }, { 4259, 1 }, { 4260, 0 }, { 4261, 0 },
	{ 4262, 0 }, { 4263, 0 }, { 4264, 0 }, { 4265, 0 }, { 4266, 0 }, { 4267, 0 },
	{ 4268, 0 }, { 4269, 0 }, { 4270, 0 }, { 4271, 0 }, { 4272, 0 }, { 4273, 0 },
	{ 4274, 0 }, { 4275, 0 }, { 4276, 0 }, { 4277, 0 }, { 4278, 0 }, { 4279, 0 },
	{ 4280, 0 }, { 4281, 0 }, { 4282, 0 }, { 4283, 0 }, { 4284, 0 }, { 4285, 0 },
	{ 4286, 0 }, { 4287, 0 }, { 4288, 0 }, { 4289, 0 }, { 4290, 0 }, { 4291, 0 },
	{ 4292, 0 }, { 4293, 0 }, { 4294, 0 }, { 4295, 0 }, { 4296, 0 }, { 4297, 0 },
	{ 4298, 0 }, { 4299, 0 }, { 4300, 0 }, { 4301, 0 }, { 4302, 0 }, { 4303, 0 },
	{ 4304, 0 }, { 4305, 0 }, { 4306, 0 }, { 4307, 0 }, { 4308, 0 }, { 4309, 0 },
	{ 4310, 0 }, { 4311, 0 }, { 4312, 0 }, { 4313, 0 }, { 4314, 0 }, { 4315, 0 },
	{ 4316, 0 }, { 4317, 0 }, { 4318, 0 }, { 4319, 0 }, { 4320, 0 }, { 4321, 0 },
	{ 4322, 0 }, { 4323, 0 }, { 4324, 0 }, { 4325, 0 }, { 4326, 0 }, { 4327, 0 },
	{ 4328, 0 }, { 4329, 0 }, { 4330, 0 }, { 4331, 0 }, { 4332, 0 }, { 4333, 0 },
	{ 4334, 0 }, { 4335, 0 }, { 4336, 0 }, { 4337, 0 }, { 4338, 0 }, { 4339, 0 },
	{ 4340, 0 }, { 4341, 0 }, { 4342, 0 }, { 4343, 0 }, { 4344, 0 }, { 4345, 0 },
	{ 4346, 0 }, { 4347, 0 }, { 4348, 0 }, { 4349, 0 }, { 4350, 0 }, { 4351, 0 },
	{ 4352, 0 }, { 4353, 0 }, { 4354, 0 }, { 4355, 0 }, { 4356, 0 }, { 4357, 0 },
	{ 4358, 0 }, { 4360, 0 }, { 4361, 0 }, { 4362, 0 }, { 4363, 0 }, { 4364, 0 },
	{ 4365, 0 }, { 4366, 0 }, { 4367, 0 }, { 4368, 0 }, { 4369, 0 }, { 4370, 0 },
	{ 4371, 0 }, { 4372, 0 }, { 4373, 0 }, { 4374, 0 }, { 4375, 0 }, { 4376, 0 },
	{ 4377, 0 }, { 4378, 0 }, { 4379, 0 }, { 4380, 0 }, { 4381, 0 }, { 4382, 0 },
	{ 4383, 0 }, { 4384, 0 }, { 4385, 0 }, { 4386, 0 }, { 4387, 0 }, { 4388, 0 },
	{ 4389, 0 }, { 4390, 0 }, { 4391, 0 }, { 4392, 0 }, { 4393, 0 }, { 4395, 0 },
	{ 4396, 0 }, { 4397, 0 }, { 4398, 0 }, { 4399, 0 }, { 4400, 0 }, { 4401, 0 },
	{ 4402, 0 }, { 4403, 0 }, { 4404, 0 }, { 4405, 0 }, { 4406, 0 }, { 4407, 0 },
	{ 4408, 0 }, { 4409, 0 }, { 4410, 0 }, { 4411, 0 }, { 4412, 0 }, { 4413, 0 },
	{ 4414, 0 }, { 4415, 0 }, { 4416, 0 }, { 4417, 0 }, { 4418, 0 }, { 4419, 0 },
	{ 4420, 0 }, { 4421, 0 }, { 4422, 0 }, { 4423, 0 }, { 4424, 0 }, { 4425, 0 },
	{ 4426, 0 }, { 4427, 0 }, { 4428, 0 }, { 4429, 0 }, { 4430, 0 }, { 4431, 0 },
	{ 4432, 0 }, { 4433, 0 }, { 4434, 0 }, { 4435, 0 }, { 4436, 0 }, { 4437, 0 },
	{ 4438, 0 }, { 4439, 0 }, { 4440, 0 }, { 4441, 0 }, { 4442, 0 }, { 4443, 0 },
	{ 4444, 0 }, { 4445, 0 }, { 4446, 0 }, { 4447, 0 }, { 4448, 0 }, { 4449, 0 },
	{ 4450, 0 }, { 4451, 0 }, { 4452, 0 }, { 4453, 0 }, { 4454, 0 }, { 4455, 0 },
	{ 4456, 0 }, { 4457, 0 }, { 4458, 0 }, { 4459, 0 }, { 4460, 0 }, { 4461, 0 },
	{ 4462, 0 }, { 4463, 0 }, { 4464, 0 }, { 4465, 0 }, { 4466, 0 }, { 4467, 0 },
	{ 4468, 0 }, { 4469, 0 }, { 4470, 0 }, { 4471, 0 }, { 4472, 0 }, { 4473, 0 },
	{ 4474, 0 }, { 4475, 0 }, { 4476, 0 }, { 4477, 0 }, { 4478, 0 }, { 4479, 0 },
	{ 4480, 0 }, { 4481, 0 }, { 4482, 0 }, { 4483, 0 }, { 4484, 0 }, { 4485, 0 },
	{ 4486, 0 }, { 4487, 0 }, { 4488, 0 }, { 4489, 0 }, { 4490, 0 }, { 4491, 0 },
	{ 4492, 0 }, { 4493, 0 }, { 4494, 0 }, { 4495, 0 }, { 4496, 0 }, { 4497, 0 },
	{ 4498, 0 }, { 4499, 0 }, { 4500, 0 }, { 4501, 0 }, { 4502, 0 }, { 4503, 0 },
	{ 4504, 0 }, { 4505, 0 }, { 4506, 0 }, { 4507, 0 }, { 4508, 0 }, { 4509, 0 },
	{ 4510, 0 }, { 4511, 0 }, { 4512, 0 }, { 4513, 0 }, { 4514, 0 }, { 4515, 0 },
	{ 4516, 0 }, { 4517, 0 }, { 4518, 0 }, { 4519, 0 }, { 4520, 0 }, { 4521, 0 },
	{ 4522, 0 }, { 4523, 0 }, { 4524, 0 }, { 4525, 0 }, { 4526, 0 }, { 4527, 3 },
	{ 4528, 0 }, { 4529, 0 }, { 4530, 0 }, { 4531, 0 }, { 4532, 0 }, { 4533, 0 },
	{ 4534, 0 }, { 4535, 0 }, { 4536, 0 }, { 4537, 0 }, { 4538, 0 }, { 4539, 0 },
	{ 4540, 0 }, { 4541, 0 }, { 4542, 0 }, { 4543, 0 }, { 4544, 0 }, { 4545, 0 },
	{ 4546, 0 }, { 4547, 0 }, { 4548, 0 }, { 4549, 0 }, { 4551, 0 }, { 4552, 0 },
	{ 4553, 0 }, { 4554, 0 }, { 4555, 0 }, { 4556, 0 }, { 4557, 0 }, { 4558, 0 },
	{ 4559, 0 }, { 4560, 0 }, { 4561, 0 }, { 4562, 0 }, { 4563, 0 }, { 4564, 0 },
	{ 4565, 0 }, { 4566, 0 }, { 4567, 0 }, { 4568, 0 }, { 4569, 0 }, { 4570, 0 },
	{ 4571, 0 }, { 4572, 0 }, { 4573, 0 }, { 4574, 0 }, { 4575, 0 }, { 4576, 0 },
	{ 4577, 0 }, { 4578, 0 }, { 4579, 0 }, { 4580, 0 }, { 4581, 0 }, { 4582, 0 },
	{ 4583, 0 }, { 4584, 0 }, { 4585, 0 }, { 4586, 0 }, { 4587, 0 }, { 4588, 0 },
	{ 4589, 0 }, { 4590, 0 }, { 4591, 0 }, { 4592, 0 }, { 4593, 0 }, { 4594, 0 },
	{ 4595, 0 }, { 4596, 0 }, { 4597, 0 }, { 4598, 0 }, { 4599, 0 }, { 4600, 0 },
	{ 4601, 0 }, { 4602, 0 }, { 4603, 0 }, { 4604, 0 }, { 4605, 0 }, { 4606, 0 },
	{ 4607, 0 }, { 4608, 0 }, { 4609, 0 }, { 4610, 0 }, { 4611, 0 }, { 4612, 0 },
	{ 4613, 0 }, { 4614, 0 }, { 4615, 0 }, { 4616, 0 }, { 4617, 0 }, { 4618, 0 },
	{ 4619, 0 }, { 4620, 0 }, { 4621, 0 }, { 4622, 0 }, { 4623, 0 }, { 4624, 0 },
	{ 4625, 0 }, { 4626, 0 }, { 4627, 0 }, { 4628, 0 }, { 4629, 0 }, { 4630, 0 },
	{ 4631, 0 }, { 4632, 0 }, { 4633, 0 }, { 4634, 0 }, { 4635, 0 }, { 4636, 0 },
	{ 4637, 0 }, { 4638, 0 }, { 4639, 0 }, { 4640, 0 }, { 4641, 0 }, { 4642, 0 },
	{ 4643, 0 }, { 4644, 0 }, { 4645, 0 }, { 4646, 0 }, { 4647, 0 }, { 4648, 0 },
	{ 4649, 0 }, { 4650, 0 }, { 4651, 0 }, { 4652, 0 }, { 4653, 0 }, { 4654, 0 },
	{ 4655, 0 }, { 4656, 0 }, { 4657, 0 }, { 4658, 0 }, { 4659, 0 }, { 4660, 0 },
	{ 4661, 0 }, { 4662, 0 }, { 4663, 0 }, { 4664, 0 }, { 4665, 0 }, { 4666, 0 },
	{ 4667, 0 }, { 4668, 0 }, { 4669, 0 }, { 4670, 0 }, { 4671, 0 }, { 4672, 0 },
	{ 4673, 0 }, { 4674, 0 }, { 4675, 0 }, { 4676, 0 }, { 4677, 0 }, { 4678, 3 },
	{ 4679, 0 }, { 4680, 0 }, { 4681, 0 }, { 4682, 0 }, { 4683, 0 }, { 4684, 0 },
	{ 4685, 0 }, { 4686, 0 }, { 4687, 0 }, { 4688, 0 }, { 4689, 0 }, { 4690, 0 },
	{ 4691, 0 }, { 4692, 0 }, { 4693, 0 }, { 4694, 0 }, { 4695, 0 }, { 4696, 0 },
	{ 4697, 0 }, { 4698, 0 }, { 4699, 0 }, { 4700, 0 }, { 4701, 0 }, { 4702, 0 },
	{ 4703, 0 }, { 4704, 0 }, { 4705, 0 }, { 4706, 0 }, { 4707, 0 }, { 4708, 0 },
	{ 4709, 0 }, { 4710, 0 }, { 4711, 0 }, { 4712, 0 }, { 4713, 0 }, { 4714, 0 },
	{ 4715, 0 }, { 4716, 0 }, { 4717, 0 }, { 4718, 0 }, { 4719, 0 }, { 4720, 0 },
	{ 4721, 0 }, { 4722, 0 }, { 4723, 0 }, { 4724, 0 }, { 4725, 0 }, { 4726, 0 },
	{ 4727, 0 }, { 4728, 0 }, { 4729, 0 }, { 4730, 0 }, { 4731, 0 }, { 4732, 0 },
	{ 4733, 0 }, { 4734, 0 }, { 4735, 0 }, { 4736, 0 }, { 4737, 0 }, { 4738, 0 },
	{ 4739, 0 }, { 4740, 0 }, { 4741, 0 }, { 4742, 0 }, { 4743, 0 }, { 4744, 0 },
	{ 4745, 0 }, { 4746, 0 }, { 4747, 0 }, { 4748, 0 }, { 4749, 0 }, { 4750, 0 },
	{ 4751, 0 }, { 4752, 0 }, { 4753, 0 }, { 4754, 0 }, { 4755, 0 }, { 4756, 0 },
	{ 4757, 0 }, { 4758, 0 }, { 4759, 0 }, { 4760, 0 }, { 4761, 0 }, { 4762, 0 },
	{ 4763, 0 }, { 4764, 0 }, { 4765, 0 }, { 4766, 0 }, { 4767, 0 }, { 4768, 0 },
	{ 4769, 0 }, { 4770, 0 }, { 4771, 0 }, { 4772, 0 }, { 4773, 0 }, { 4774, 0 },
	{ 4775, 0 }, { 4776, 0 }, { 4777, 0 }, { 4778, 0 }, { 4779, 0 }, { 4780, 0 },
	{ 4781, 0 }, { 4782, 0 }, { 4783, 0 }, { 4784, 0 }, { 4785, 0 }, { 4786, 0 },
	{ 4787, 0 }, { 4788, 0 }, { 4789, 0 }, { 4790, 0 }, { 4791, 0 }, { 4792, 0 },
	{ 4793, 0 }, { 4794, 0 }, { 4795, 0 }, { 4796, 0 }, { 4797, 0 }, { 4798, 0 },
	{ 4799, 0 }, { 4800, 0 }, { 4801, 0 }, { 4802, 0 }, { 4803, 0 }, { 4804, 0 },
	{ 4805, 0 }, { 4806, 0 }, { 4807, 0 }, { 4808, 0 }, { 4809, 0 }, { 4810, 0 },
	{ 4811, 0 }, { 4812, 0 }, { 4813, 0 }, { 4814, 0 }, { 4815, 0 }, { 4816, 0 },
	{ 4817, 0 }, { 4818, 0 }, { 4819, 0 }, { 4820, 0 }, { 4821, 0 }, { 4822, 0 },
	{ 4823, 0 }, { 4824, 0 }, { 4825, 0 }, { 4826, 0 }, { 4827, 0 }, { 4828, 0 },
	{ 4829, 0 }, { 4830, 0 }, { 4831, 0 }, { 4832, 0 }, { 4833, 0 }, { 4834, 0 },
	{ 4835, 0 }, { 4836, 0 }, { 4837, 0 }, { 4838, 0 }, { 4839, 0 }, { 4840, 0 },
	{ 4841, 0 }, { 4842, 0 }, { 4843, 0 }, { 4844, 0 }, { 4845, 0 }, { 4846, 0 },
	{ 4847, 0 }, { 4848, 0 }, { 4849, 0 }, { 4850, 0 }, { 4851, 0 }, { 4852, 0 },
	{ 4853, 0 }, { 4854, 0 }, { 4855, 0 }, { 4856, 0 }, { 4857, 0 }, { 4858, 0 },
	{ 4859, 0 }, { 4860, 0 }, { 4861, 0 }, { 4862, 0 }, { 4863, 0 }, { 4864, 0 },
	{ 4865, 0 }, { 4866, 0 }, { 4867, 0 }, { 4868, 0 }, { 4869, 0 }, { 4870, 0 },
	{ 4871, 0 }, { 4872, 0 }, { 4873, 0 }, { 4874, 0 }, { 4875, 0 }, { 4876, 0 },
	{ 4877, 0 }, { 4878, 0 }, { 4879, 0 }, { 4880, 0 }, { 4881, 0 }, { 4882, 0 },
	{ 4883, 0 }, { 4884, 0 }, { 4885, 0 }, { 4886, 0 }, { 4887, 0 }, { 4888, 0 },
	{ 4889, 0 }, { 4890, 0 }, { 4891, 0 }, { 4892, 0 }, { 4893, 0 }, { 4894, 0 },
	{ 4895, 0 }, { 4896, 0 }, { 4897, 0 }, { 4898, 0 }, { 4899, 0 }, { 4900, 0 },
	{ 4901, 0 }, { 4902, 0 }, { 4903, 0 }, { 4904, 0 }, { 4905, 0 }, { 4906, 0 },
	{ 4907, 0 }, { 4908, 0 }, { 4909, 0 }, { 4910, 0 }, { 4911, 0 }, { 4912, 0 },
	{ 4913, 0 }, { 4914, 0 }, { 4915, 0 }, { 4916, 0 }, { 4917, 0 }, { 4918, 0 },
	{ 4919, 0 }, { 4920, 0 }, { 4921, 0 }, { 4922, 0 }, { 4923, 0 }, { 4924, 0 },
	{ 4925, 0 }, { 4926, 0 }, { 4927, 0 }, { 4928, 0 }, { 4929, 0 }, { 4930, 0 },
	{ 4931, 0 }, { 4932, 0 }, { 4933, 0 }, { 4934, 0 }, { 4935, 0 }, { 4936, 0 },
	{ 4937, 0 }, { 4938, 0 }, { 4939, 0 }, { 4940, 0 }, { 4941, 0 }, { 4942, 0 },
	{ 4943, 0 }, { 4944, 0 }, { 4945, 0 }, { 4946, 0 }, { 4947, 0 }, { 4948, 0 },
	{ 4949, 0 }, { 4950, 0 }, { 4951, 0 }, { 4952, 0 }, { 4953, 0 }, { 4954, 0 },
	{ 4955, 0 }, { 4956, 0 }, { 4957, 0 }, { 4958, 0 }, { 4959, 0 }, { 4960, 0 },
	{ 4961, 0 }, { 4962, 0 }, { 4963, 0 }, { 4964, 0 }, { 4965, 0 }, { 4966, 0 },
	{ 4967, 0 }, { 4968, 0 }, { 4969, 0 }, { 4970, 0 }, { 4971, 0 }, { 4972, 0 },
	{ 4973, 0 }, { 4974, 0 }, { 4975, 0 }, { 4976, 0 }, { 4977, 0 }, { 4978, 0 },
	{ 4979, 0 }, { 4980, 0 }, { 4981, 0 }, { 4982, 0 }, { 4983, 0 }, { 4984, 0 },
	{ 4985, 0 }, { 4986, 0 }, { 4987, 0 }, { 4988, 0 }, { 4989, 0 }, { 4990, 0 },
	{ 4991, 0 }, { 4992, 0 }, { 4993, 0 }, { 4994, 0 }, { 4995, 0 }, { 4996, 0 },
	{ 4997, 0 }, { 4998, 0 }, { 4999, 0 }, { 5000, 0 }, { 5001, 0 }, { 5002, 0 },
	{ 5003, 0 }, { 5004, 0 }, { 5005, 0 }, { 5006, 0 }, { 5007, 0 }, { 5008, 0 },
	{ 5009, 0 }, { 5010, 0 }, { 5011, 0 }, { 5012, 0 }, { 5013, 0 }, { 5014, 0 },
	{ 5015, 0 }, { 5016, 0 }, { 5017, 0 }, { 5018, 0 }, { 5019, 0 }, { 5020, 0 },
	{ 5021, 0 }, { 5022, 0 }, { 5023, 0 }, { 5024, 0 }, { 5025, 0 }, { 5026, 0 },
	{ 5027, 0 }, { 5028, 0 }, { 5029, 0 }, { 5030, 0 }, { 5031, 0 }, { 5032, 0 },
	{ 5033, 0 }, { 5034, 0 }, { 5035, 0 }, { 5036, 0 }, { 5037, 0 }, { 5038, 0 },
	{ 5039, 0 }, { 5040, 0 }, { 5041, 0 }, { 5042, 0 }, { 5043, 0 }, { 5044, 0 },
	{ 5045, 0 }, { 5046, 0 }, { 5047, 0 }, { 5048, 0 }, { 5049, 0 }, { 5050, 0 },
	{ 5051, 0 }, { 5052, 0 }, { 5053, 0 }, { 5054, 0 }, { 5055, 0 }, { 5056, 0 },
	{ 5057, 0 }, { 5058, 0 }, { 5059, 0 }, { 5060, 0 }, { 5061, 0 }, { 5062, 0 },
	{ 5063, 0 }, { 5064, 0 }, { 5065, 0 }, { 5066, 0 }, { 5067, 0 }, { 5068, 0 },
	{ 5069, 0 }, { 5070, 0 }, { 5071, 0 }, { 5072, 0 }, { 5073, 0 }, { 5074, 0 },
	{ 5075, 0 }, { 5076, 0 }, { 5077, 0 }, { 5078, 0 }, { 5079, 0 }, { 5080, 1 },
	{ 5081, 1 }, { 5082, 1 }, { 5083, 1 }, { 5084, 1 }, { 5085, 1 }, { 5086, 1 },
	{ 5087, 1 }, { 5088, 0 }, { 5089, 0 }, { 5090, 0 }, { 5091, 0 }, { 5092, 0 },
	{ 5093, 0 }, { 5094, 0 }, { 5095, 0 }, { 5096, 0 }, { 5097, 0 }, { 5098, 0 },
	{ 5099, 0 }, { 5100, 0 }, { 5101, 0 }, { 5102, 0 }, { 5103, 0 }, { 5104, 0 },
	{ 5105, 0 }, { 5106, 0 }, { 5107, 0 }, { 5108, 0 }, { 5109, 0 }, { 5110, 0 },
	{ 5111, 0 }, { 5112, 0 }, { 5113, 0 }, { 5114, 0 }, { 5115, 0 }, { 5116, 0 },
	{ 5117, 0 }, { 5118, 0 }, { 5119, 0 }, { 5120, 0 }, { 5121, 0 }, { 5122, 0 },
	{ 5123, 0 }, { 5124, 0 }, { 5125, 0 }, { 5126, 0 }, { 5127, 0 }, { 5128, 0 },
	{ 5129, 0 }, { 5130, 0 }, { 5131, 0 }, { 5132, 0 }, { 5133, 0 }, { 5134, 0 },
	{ 5135, 0 }, { 5136, 0 }, { 5137, 0 }, { 5138, 0 }, { 5139, 0 }, { 5140, 0 },
	{ 5141, 0 }, { 5142, 0 }, { 5143, 0 }, { 5144, 0 }, { 5145, 0 }, { 5146, 0 },
	{ 5147, 0 }, { 5148, 0 }, { 5149, 0 }, { 5150, 0 }, { 5151, 0 }, { 5152, 0 },
	{ 5153, 0 }, { 5154, 0 }, { 5155, 0 }, { 5156, 0 }, { 5157, 0 }, { 5158, 0 },
	{ 5159, 0 }, { 5160, 0 }, { 5161, 0 }, { 5162, 0 }, { 5163, 0 }, { 5164, 0 },
	{ 5165, 0 }, { 5166, 0 }, { 5167, 0 }, { 5168, 0 }, { 5169, 0 }, { 5170, 0 },
	{ 5171, 0 }, { 5172, 0 }, { 5173, 0 }, { 5174, 0 }, { 5175, 0 }, { 5176, 0 },
	{ 5177, 0 }, { 5178, 0 }, { 5179, 0 }, { 5180, 0 }, { 5181, 0 }, { 5182, 0 },
	{ 5183, 0 }, { 5184, 0 }, { 5185, 0 }, { 5186, 0 }, { 5187, 0 }, { 5188, 0 },
	{ 5189, 0 }, { 5190, 0 }, { 5191, 0 }, { 5192, 0 }, { 5193, 0 }, { 5194, 0 },
	{ 5195, 0 }, { 5196, 0 }, { 5197, 0 }, { 5198, 0 }, { 5199, 0 }, { 5200, 0 },
	{ 5201, 0 }, { 5202, 0 }, { 5203, 0 }, { 5204, 0 }, { 5205, 0 }, { 5206, 0 },
	{ 5207, 0 }, { 5208, 0 }, { 5209, 0 }, { 5210, 0 }, { 5211, 0 }, { 5212, 0 },
	{ 5213, 0 }, { 5214, 0 }, { 5215, 0 }, { 5216, 0 }, { 5217, 0 }, { 5218, 0 },
	{ 5219, 0 }, { 5220, 0 }, { 5221, 0 }, { 5222, 0 }, { 5223, 0 }, { 5224, 0 },
	{ 5225, 0 }, { 5226, 0 }, { 5227, 0 }, { 5228, 0 }, { 5229, 0 }, { 5230, 0 },
	{ 5231, 0 }, { 5232, 0 }, { 5233, 0 }, { 5234, 0 }, { 5235, 0 }, { 5236, 0 },
	{ 5237, 0 }, { 5238, 0 }, { 5239, 0 }, { 5240, 0 }, { 5241, 0 }, { 5242, 0 },
	{ 5243, 0 }, { 5244, 0 }, { 5245, 0 }, { 5246, 0 }, { 5247, 0 }, { 5248, 0 },
	{ 5249, 0 }, { 5250, 0 }, { 5251, 0 }, { 5252, 0 }, { 5253, 0 }, { 5254, 0 },
	{ 5255, 0 }, { 5256, 0 }, { 5257, 0 }, { 5258, 0 }, { 5259, 0 }, { 5260, 0 },
	{ 5261, 0 }, { 5262, 0 }, { 5263, 0 }, { 5264, 0 }, { 5265, 0 }, { 5266, 0 },
	{ 5267, 0 }, { 5268, 0 }, { 5269, 0 }, { 5270, 0 }, { 5271, 0 }, { 5272, 0 },
	{ 5273, 0 }, { 5274, 0 }, { 5275, 0 }, { 5276, 0 }, { 5277, 0 }, { 5278, 0 },
	{ 5279, 0 }, { 5280, 0 }, { 5281, 3 }, { 5282, 3 }, { 5283, 3 }, { 5284, 0 },
	{ 5285, 0 }, { 5286, 0 }, { 5287, 0 }, { 5288, 0 }, { 5289, 0 }, { 5290, 0 },
	{ 5291, 0 }, { 5292, 0 }, { 5293, 0 }, { 5294, 0 }, { 5295, 0 }, { 5296, 0 },
	{ 5297, 0 }, { 5298, 0 }, { 5299, 0 }, { 5300, 0 }, { 5301, 0 }, { 5302, 0 },
	{ 5303, 0 }, { 5304, 0 }, { 5305, 0 }, { 5306, 0 }, { 5307, 0 }, { 5308, 0 },
	{ 5309, 0 }, { 5310, 0 }, { 5311, 0 }, { 5312, 0 }, { 5313, 0 }, { 5314, 0 },
	{ 5315, 0 }, { 5316, 0 }, { 5317, 0 }, { 5318, 0 }, { 5319, 0 }, { 5320, 0 },
	{ 5321, 0 }, { 5322, 0 }, { 5323, 0 }, { 5324, 0 }, { 5325, 0 }, { 5326, 0 },
	{ 5327, 0 }, { 5328, 0 }, { 5329, 0 }, { 5330, 0 }, { 5331, 0 }, { 5332, 0 },
	{ 5333, 0 }, { 5334, 0 }, { 5335, 0 }, { 5336, 0 }, { 5337, 0 }, { 5338, 0 },
	{ 5339, 0 }, { 5340, 0 }, { 5341, 0 }, { 5342, 0 }, { 5343, 0 }, { 5344, 0 },
	{ 5345, 0 }, { 5346, 0 }, { 5347, 0 }, { 5348, 0 }, { 5349, 0 }, { 5350, 0 },
	{ 5351, 0 }, { 5352, 0 }, { 5353, 0 }, { 5354, 0 }, { 5355, 0 }, { 5356, 0 },
	{ 5357, 0 }, { 5358, 0 }, { 5359, 0 }, { 5360, 0 }, { 5361, 0 }, { 5362, 0 },
	{ 5363, 0 }, { 5364, 0 }, { 5365, 0 }, { 5366, 0 }, { 5367, 0 }, { 5368, 0 },
	{ 5369, 0 }, { 5370, 0 }, { 5371, 0 }, { 5372, 0 }, { 5373, 0 }, { 5374, 0 },
	{ 5375, 0 }, { 5376, 0 }, { 5377, 0 }, { 5378, 0 }, { 5379, 0 }, { 5380, 0 },
	{ 5381, 0 }, { 5382, 0 }, { 5383, 0 }, { 5384, 0 }, { 5385, 0 }, { 5386, 0 },
	{ 5387, 0 }, { 5388, 0 }, { 5389, 0 }, { 5390, 0 }, { 5391, 0 }, { 5392, 0 },
	{ 5393, 0 }, { 5394, 0 }, { 5395, 0 }, { 5396, 0 }, { 5397, 0 }, { 5398, 0 },
	{ 5399, 0 }, { 5400, 0 }, { 5401, 0 }, { 5402, 0 }, { 5403, 0 }, { 5404, 0 },
	{ 5405, 0 }, { 5406, 0 }, { 5407, 0 }, { 5408, 0 }, { 5409, 0 }, { 5410, 0 },
	{ 5411, 0 }, { 5412, 0 }, { 5413, 0 }, { 5414, 0 }, { 5415, 0 }, { 5416, 0 },
	{ 5417, 0 }, { 5418, 0 }, { 5419, 0 }, { 5420, 0 }, { 5421, 0 }, { 5422, 0 },
	{ 5423, 0 }, { 5424, 0 }, { 5425, 0 }, { 5426, 0 }, { 5427, 0 }, { 5428, 0 },
	{ 5429, 0 }, { 5430, 0 }, { 5431, 0 }, { 5432, 0 }, { 5433, 0 }, { 5434, 0 },
	{ 5435, 0 }, { 5436, 0 }, { 5437, 0 }, { 5438, 0 }, { 5439, 0 }, { 5440, 0 },
	{ 5441, 0 }, { 5442, 0 }, { 5443, 0 }, { 5444, 0 }, { 5445, 0 }, { 5446, 0 },
	{ 5447, 0 }, { 5448, 0 }, { 5449, 0 }, { 5450, 0 }, { 5451, 3 }, { 5452, 0 },
	{ 5453, 0 }, { 5454, 3 }, { 5455, 3 }, { 5456, 0 }, { 5457, 0 }, { 5458, 0 },
	{ 5459, 0 }, { 5460, 0 }, { 5461, 0 }, { 5462, 0 }, { 5463, 0 }, { 5464, 0 },
	{ 5465, 0 }, { 5466, 0 }, { 5467, 0 }, { 5468, 0 }, { 5469, 0 }, { 5470, 0 },
	{ 5471, 0 }, { 5472, 0 }, { 5473, 0 }, { 5474, 0 }, { 5475, 0 }, { 5476, 0 },
	{ 5477, 0 }, { 5478, 0 }, { 5479, 0 }, { 5480, 0 }, { 5481, 0 }, { 5482, 0 },
	{ 5483, 0 }, { 5484, 0 }, { 5485, 0 }, { 5486, 0 }, { 5487, 0 }, { 5488, 0 },
	{ 5489, 0 }, { 5490, 0 }, { 5491, 0 }, { 5492, 0 }, { 5493, 0 }, { 5494, 0 },
	{ 5495, 0 }, { 5496, 0 }, { 5497, 0 }, { 5498, 0 }, { 5499, 0 }, { 5500, 0 },
	{ 5501, 0 }, { 5502, 0 }, { 5503, 0 }, { 5504, 0 }, { 5505, 0 }, { 5506, 0 },
	{ 5507, 0 }, { 5508, 0 }, { 5509, 0 }, { 5510, 0 }, { 5511, 0 }, { 5512, 0 },
	{ 5513, 0 }, { 5514, 0 }, { 5515, 0 }, { 5516, 0 }, { 5517, 0 }, { 5518, 0 },
	{ 5519, 0 }, { 5520, 0 }, { 5521, 0 }, { 5522, 0 }, { 5523, 0 }, { 5524, 0 },
	{ 5525, 0 }, { 5526, 0 }, { 5527, 0 }, { 5528, 0 }, { 5529, 0 }, { 5530, 0 },
	{ 5531, 0 }, { 5532, 0 }, { 5533, 0 }, { 5534, 0 }, { 5535, 0 }, { 5536, 0 },
	{ 5537, 0 }, { 5538, 0 }, { 5539, 0 }, { 5540, 0 }, { 5541, 0 }, { 5542, 0 },
	{ 5543, 0 }, { 5544, 0 }, { 5545, 0 }, { 5546, 0 }, { 5547, 0 }, { 5548, 0 },
	{ 5549, 0 }, { 5550, 0 }, { 5551, 0 }, { 5552, 0 }, { 5553, 0 }, { 5554, 0 },
	{ 5555, 0 }, { 5556, 0 }, { 5557, 0 }, { 5558, 0 }, { 5559, 0 }, { 5560, 0 },
	{ 5561, 0 }, { 5562, 0 }, { 5563, 0 }, { 5564, 0 }, { 5565, 0 }, { 5566, 0 },
	{ 5567, 0 }, { 5568, 0 }, { 5569, 0 }, { 5570, 0 }, { 5571, 0 }, { 5572, 0 },
	{ 5573, 0 }, { 5574, 0 }, { 5575, 0 }, { 5576, 0 }, { 5577, 0 }, { 5578, 0 },
	{ 5579, 0 }, { 5580, 0 }, { 5581, 0 }, { 5582, 0 }, { 5583, 0 }, { 5584, 0 },
	{ 5585, 0 }, { 5586, 0 }, { 5587, 0 }, { 5588, 0 }, { 5589, 0 }, { 5590, 0 },
	{ 5591, 0 }, { 5592, 0 }, { 5593, 0 }, { 5594, 0 }, { 5595, 0 }, { 5596, 0 },
	{ 5597, 0 }, { 5598, 0 }, { 5599, 0 }, { 5600, 0 }, { 5601, 0 }, { 5602, 0 },
	{ 5603, 0 }, { 5604, 0 }, { 5605, 0 }, { 5606, 0 }, { 5607, 0 }, { 5608, 0 },
	{ 5609, 0 }, { 5610, 0 }, { 5611, 0 }, { 5612, 0 }, { 5613, 0 }, { 5614, 0 },
	{ 5615, 0 }, { 5616, 0 }, { 5617, 0 }, { 5618, 0 }, { 5619, 0 }, { 5620, 0 },
	{ 5621, 0 }, { 5622, 0 }, { 5623, 0 }, { 5624, 0 }, { 5625, 0 }, { 5626, 0 },
	{ 5627, 0 }, { 5628, 0 }, { 5629, 0 }, { 5630, 0 }, { 5631, 0 }, { 5632, 0 },
	{ 5633, 0 }, { 5634, 0 }, { 5635, 0 }, { 5636, 0 }, { 5637, 0 }, { 5638, 0 },
	{ 5639, 0 }, { 5640, 0 }, { 5641, 0 }, { 5642, 0 }, { 5643, 0 }, { 5644, 0 },
	{ 5645, 0 }, { 5646, 0 }, { 5647, 0 }, { 5648, 0 }, { 5649, 0 }, { 5650, 0 },
	{ 5651, 0 }, { 5652, 0 }, { 5653, 0 }, { 5654, 0 }, { 5655, 0 }, { 5656, 0 },
	{ 5657, 0 }, { 5658, 0 }, { 5659, 0 }, { 5660, 0 }, { 5661, 0 }, { 5662, 0 },
	{ 5663, 0 }, { 5664, 0 }, { 5665, 0 }, { 5666, 0 }, { 5667, 0 }, { 5668, 0 },
	{ 5669, 0 }, { 5670, 0 }, { 5671, 0 }, { 5672, 0 }, { 5673, 0 }, { 5674, 0 },
	{ 5675, 0 }, { 5676, 0 }, { 5677, 0 }, { 5678, 0 }, { 5679, 0 }, { 5680, 0 },
	{ 5681, 0 }, { 5682, 0 }, { 5683, 0 }, { 5684, 0 }, { 5685, 0 }, { 5686, 0 },
	{ 5687, 0 }, { 5688, 0 }, { 5689, 0 }, { 5690, 0 }, { 5691, 0 }, { 5692, 0 },
	{ 5693, 0 }, { 5694, 0 }, { 5695, 0 }, { 5696, 0 }, { 5697, 0 }, { 5698, 0 },
	{ 5699, 0 }, { 5700, 0 }, { 5701, 0 }, { 5702, 0 }, { 5703, 4 }, { 5704, 0 },
	{ 5705, 0 }, { 5706, 0 }, { 5707, 0 }, { 5708, 0 }, { 5709, 4 }, { 5710, 4 },
	{ 5711, 4 }, { 5712, 4 }, { 5713, 4 }, { 5714, 0 }, { 5715, 0 }, { 5716, 0 },
	{ 5717, 0 }, { 5718, 0 }, { 5719, 4 }, { 5720, 4 }, { 5721, 4 }, { 5722, 0 },
	{ 5723, 0 }, { 5724, 0 }, { 5725, 0 }, { 5726, 0 }, { 5727, 0 }, { 5728, 0 },
	{ 5729, 0 }, { 5730, 0 }, { 5731, 0 }, { 5732, 0 }, { 5733, 0 }, { 5734, 0 },
	{ 5735, 0 }, { 5736, 0 }, { 5737, 0 }, { 5738, 0 }, { 5739, 0 }, { 5740, 0 },
	{ 5741, 0 }, { 5742, 0 }, { 5743, 0 }, { 5744, 0 }, { 5745, 0 }, { 5746, 0 },
	{ 5747, 0 }, { 5748, 0 }, { 5749, 0 }, { 5750, 0 }, { 5751, 0 }, { 5752, 0 },
	{ 5753, 0 }, { 5754, 0 }, { 5755, 0 }, { 5756, 0 }, { 5757, 0 }, { 5758, 0 },
	{ 5759, 0 }, { 5760, 0 }, { 5761, 0 }, { 5762, 0 }, { 5763, 0 }, { 5764, 0 },
	{ 5765, 0 }, { 5766, 0 }, { 5767, 0 }, { 5768, 0 }, { 5769, 0 }, { 5770, 0 },
	{ 5771, 0 }, { 5772, 0 }, { 5773, 0 }, { 5774, 0 }, { 5775, 0 }, { 5776, 0 },
	{ 5777, 0 }, { 5778, 0 }, { 5779, 0 }, { 5780, 0 }, { 5781, 0 }, { 5782, 0 },
	{ 5783, 0 }, { 5784, 0 }, { 5785, 0 }, { 5786, 0 }, { 5787, 0 }, { 5788, 0 },
	{ 5789, 0 }, { 5790, 0 }, { 5791, 0 }, { 5792, 0 }, { 5793, 0 }, { 5794, 0 },
	{ 5795, 0 }, { 5796, 0 }, { 5797, 0 }, { 5798, 0 }, { 5799, 0 }, { 5800, 0 },
	{ 5801, 0 }, { 5802, 0 }, { 5803, 0 }, { 5804, 0 }, { 5805, 0 }, { 5806, 0 },
	{ 5807, 0 }, { 5808, 0 }, { 5809, 0 }, { 5810, 0 }, { 5811, 0 }, { 5812, 0 },
	{ 5813, 0 }, { 5814, 0 }, { 5815, 0 }, { 5816, 0 }, { 5817, 0 }, { 5818, 0 },
	{ 5819, 0 }, { 5820, 0 }, { 5821, 0 }, { 5822, 0 }, { 5823, 0 }, { 5824, 0 },
	{ 5825, 0 }, { 5826, 0 }, { 5827, 0 }, { 5828, 0 }, { 5829, 0 }, { 5830, 0 },
	{ 5831, 0 }, { 5832, 0 }, { 5833, 0 }, { 5834, 0 }, { 5835, 0 }, { 5836, 0 },
	{ 5837, 0 }, { 5838, 0 }, { 5839, 0 }, { 5840, 0 }, { 5841, 0 }, { 5842, 0 },
	{ 5843, 0 }, { 5844, 0 }, { 5845, 0 }, { 5846, 0 }, { 5847, 0 }, { 5848, 0 },
	{ 5849, 0 }, { 5850, 0 }, { 5851, 0 }, { 5852, 0 }, { 5853, 0 }, { 5854, 0 },
	{ 5855, 0 }, { 5856, 0 }, { 5857, 0 }, { 5858, 0 }, { 5859, 0 }, { 5860, 0 },
	{ 5861, 0 }, { 5862, 0 }, { 5863, 0 }, { 5864, 0 }, { 5865, 0 }, { 5866, 0 },
	{ 5867, 0 }, { 5868, 0 }, { 5869, 0 }, { 5870, 0 }, { 5871, 0 }, { 5872, 0 },
	{ 5873, 0 }, { 5874, 0 }, { 5875, 0 }, { 5876, 0 }, { 5877, 0 }, { 5878, 0 },
	{ 5879, 0 }, { 5880, 0 }, { 5881, 0 }, { 5882, 0 }, { 5883, 0 }, { 5884, 0 },
	{ 5885, 0 }, { 5886, 0 }, { 5887, 0 }, { 5888, 0 }, { 5889, 0 }, { 5890, 0 },
	{ 5891, 0 }, { 5892, 0 }, { 5893, 0 }, { 5894, 0 }, { 5895, 0 }, { 5896, 0 },
	{ 5897, 0 }, { 5898, 0 }, { 5899, 0 }, { 5900, 0 }, { 5901, 0 }, { 5902, 0 },
	{ 5903, 0 }, { 5904, 0 }, { 5905, 0 }, { 5906, 0 }, { 5907, 0 }, { 5908, 0 },
	{ 5909, 0 }, { 5910, 0 }, { 5911, 0 }, { 5912, 0 }, { 5913, 0 }, { 5914, 0 },
	{ 5915, 0 }, { 5916, 0 }, { 5917, 0 }, { 5918, 0 }, { 5919, 0 }, { 5920, 0 },
	{ 5921, 0 }, { 5922, 0 }, { 5923, 0 }, { 5924, 0 }, { 5925, 0 }, { 5926, 0 },
	{ 5927, 0 }, { 5928, 0 }, { 5929, 0 }, { 5930, 0 }, { 5931, 0 }, { 5932, 0 },
	{ 5933, 0 }, { 5934, 0 }, { 5935, 0 }, { 5936, 0 }, { 5937, 0 }, { 5938, 0 },
	{ 5939, 0 }, { 5940, 0 }, { 5941, 0 }, { 5942, 0 }, { 5943, 0 }, { 5944, 0 },
	{ 5945, 0 }, { 5946, 0 }, { 5947, 0 }, { 5948, 0 }, { 5949, 0 }, { 5950, 0 },
	{ 5951, 0 }, { 5952, 0 }, { 5953, 0 }, { 5954, 0 }, { 5955, 0 }, { 5956, 0 },
	{ 5957, 0 }, { 5958, 0 }, { 5959, 0 }, { 5960, 0 }, { 5961, 0 }, { 5962, 0 },
	{ 5963, 0 }, { 5964, 0 }, { 5965, 0 }, { 5966, 0 }, { 5967, 0 }, { 5968, 0 },
	{ 5969, 0 }, { 5970, 0 }, { 5971, 0 }, { 5972, 0 }, { 5973, 0 }, { 5974, 0 },
	{ 5975, 0 }, { 5976, 0 }, { 5977, 0 }, { 5978, 0 }, { 5979, 0 }, { 5980, 0 },
	{ 5981, 0 }, { 5982, 0 }, { 5983, 0 }, { 5984, 0 }, { 5985, 0 }, { 5986, 0 },
	{ 5987, 0 }, { 5988, 0 }, { 5989, 0 }, { 5990, 0 }, { 5991, 0 }, { 5992, 0 },
	{ 5993, 0 }, { 5994, 0 }, { 5995, 0 }, { 5996, 0 }, { 5997, 0 }, { 5998, 0 },
	{ 5999, 0 }, { 6000, 0 }, { 6001, 0 }, { 6002, 0 }, { 6003, 0 }, { 6004, 0 },
	{ 6005, 0 }, { 6006, 0 }, { 6007, 0 }, { 6008, 0 }, { 6009, 0 }, { 6010, 0 },
	{ 6011, 0 }, { 6012, 0 }, { 6013, 0 }, { 6014, 0 }, { 6015, 0 }, { 6016, 0 },
	{ 6017, 0 }, { 6018, 0 }, { 6019, 0 }, { 6020, 0 }, { 6021, 0 }, { 6022, 0 },
	{ 6023, 0 }, { 6024, 0 }, { 6025, 0 }, { 6026, 0 }, { 6027, 0 }, { 6028, 0 },
	{ 6029, 0 }, { 6030, 0 }, { 6031, 0 }, { 6032, 0 }, { 6033, 0 }, { 6034, 0 },
	{ 6035, 0 }, { 6036, 0 }, { 6037, 0 }, { 6038, 0 }, { 6039, 0 }, { 6040, 0 },
	{ 6041, 0 }, { 6042, 0 }, { 6043, 0 }, { 6044, 0 }, { 6045, 0 }, { 6046, 0 },
	{ 6047, 0 }, { 6048, 0 }, { 6049, 0 }, { 6050, 0 }, { 6051, 0 }, { 6052, 0 },
	{ 6053, 0 }, { 6054, 0 }, { 6055, 0 }, { 6056, 0 }, { 6057, 0 }, { 6058, 0 },
	{ 6059, 0 }, { 6060, 0 }, { 6061, 0 }, { 6062, 0 }, { 6063, 0 }, { 6064, 0 },
	{ 6065, 0 }, { 6066, 0 }, { 6067, 0 }, { 6068, 0 }, { 6069, 0 }, { 6070, 0 },
	{ 6071, 0 }, { 6072, 0 }, { 6073, 0 }, { 6074, 0 }, { 6075, 0 }, { 6076, 0 },
	{ 6077, 0 }, { 6078, 0 }, { 6079, 0 }, { 6080, 0 }, { 6081, 0 }, { 6082, 0 },
	{ 6083, 0 }, { 6084, 0 }, { 6085, 0 }, { 6086, 0 }, { 6087, 0 }, { 6088, 0 },
	{ 6089, 0 }, { 6090, 0 }, { 6091, 0 }, { 6092, 0 }, { 6093, 0 }, { 6094, 0 },
	{ 6095, 0 }, { 6096, 0 }, { 6097, 0 }, { 6098, 0 }, { 6099, 0 }, { 6100, 0 },
	{ 6101, 0 }, { 6102, 0 }, { 6103, 0 }, { 6104, 0 }, { 6105, 0 }, { 6106, 0 },
	{ 6107, 0 }, { 6108, 0 }, { 6109, 0 }, { 6110, 0 }, { 6111, 0 }, { 6112, 0 },
	{ 6113, 0 }, { 6114, 0 }, { 6115, 0 }, { 6116, 0 }, { 6117, 0 }, { 6118, 0 },
	{ 6119, 0 }, { 6120, 0 }, { 6121, 0 }, { 6122, 0 }, { 6123, 0 }, { 6124, 0 },
	{ 6125, 0 }, { 6126, 0 }, { 6127, 0 }, { 6128, 0 }, { 6129, 0 }, { 6130, 0 },
	{ 6131, 0 }, { 6132, 0 }, { 6133, 0 }, { 6134, 0 }, { 6135, 0 }, { 6136, 0 },
	{ 6137, 0 }, { 6138, 0 }, { 6139, 0 }, { 6140, 0 }, { 6141, 0 }, { 6142, 0 },
	{ 6143, 0 }, { 6144, 0 }, { 6145, 0 }, { 6146, 0 }, { 6147, 0 }, { 6148, 0 },
	{ 6149, 0 }, { 6150, 0 }, { 6151, 0 }, { 6152, 0 }, { 6153, 0 }, { 6154, 0 },
	{ 6155, 0 }, { 6156, 0 }, { 6157, 0 }, { 6158, 0 }, { 6159, 0 }, { 6160, 0 },
	{ 6161, 0 }, { 6162, 0 }, { 6163, 0 }, { 6164, 0 }, { 6165, 0 }, { 6166, 0 },
	{ 6167, 0 }, { 6168, 0 }, { 6169, 0 }, { 6170, 0 }, { 6171, 0 }, { 6172, 0 },
	{ 6173, 0 }, { 6174, 0 }, { 6175, 0 }, { 6176, 0 }, { 6177, 0 }, { 6178, 0 },
	{ 6179, 0 }, { 6180, 0 }, { 6181, 0 }, { 6182, 0 }, { 6183, 0 }, { 6184, 0 },
	{ 6185, 0 }, { 6186, 0 }, { 6187, 0 }, { 6188, 0 }, { 6189, 0 }, { 6190, 0 },
	{ 6191, 0 }, { 6192, 0 }, { 6193, 0 }, { 6194, 0 }, { 6195, 0 }, { 6196, 0 },
	{ 6197, 0 }, { 6198, 0 }, { 6199, 0 }, { 6200, 0 }, { 6201, 0 }, { 6202, 0 },
	{ 6203, 0 }, { 6204, 0 }, { 6205, 0 }, { 6206, 0 }, { 6207, 0 }, { 6208, 0 },
	{ 6209, 0 }, { 6210, 0 }, { 6211, 0 }, { 6212, 0 }, { 6213, 0 }, { 6214, 0 },
	{ 6215, 0 }, { 6216, 0 }, { 6217, 0 }, { 6218, 0 }, { 6219, 0 }, { 6220, 0 },
	{ 6221, 0 }, { 6222, 0 }, { 6223, 0 }, { 6224, 0 }, { 6225, 0 }, { 6226, 0 },
	{ 6227, 0 }, { 6228, 0 }, { 6229, 0 }, { 6230, 0 }, { 6231, 0 }, { 6232, 0 },
	{ 6233, 0 }, { 6234, 0 }, { 6235, 0 }, { 6236, 0 }, { 6237, 0 }, { 6238, 0 },
	{ 6239, 0 }, { 6240, 0 }, { 6241, 0 }, { 6242, 0 }, { 6243, 0 }, { 6244, 0 },
	{ 6246, 0 }, { 6247, 0 }, { 6248, 0 }, { 6249, 0 }, { 6250, 0 }, { 6251, 0 },
	{ 6252, 0 }, { 6253, 0 }, { 6254, 0 }, { 6255, 0 }, { 6256, 0 }, { 6257, 0 },
	{ 6258, 0 }, { 6259, 0 }, { 6260, 0 }, { 6261, 0 }, { 6262, 0 }, { 6263, 0 },
	{ 6264, 0 }, { 6265, 0 }, { 6266, 0 }, { 6267, 0 }, { 6268, 0 }, { 6269, 0 },
	{ 6270, 0 }, { 6271, 0 }, { 6272, 0 }, { 6273, 0 }, { 6274, 0 }, { 6275, 0 },
	{ 6276, 0 }, { 6277, 0 }, { 6278, 0 }, { 6279, 0 }, { 6280, 0 }, { 6281, 0 },
	{ 6282, 0 }, { 6283, 0 }, { 6284, 0 }, { 6285, 0 }, { 6286, 0 }, { 6287, 0 },
	{ 6288, 0 }, { 6289, 0 }, { 6290, 0 }, { 6291, 0 }, { 6292, 0 }, { 6293, 0 },
	{ 6294, 0 }, { 6295, 0 }, { 6296, 0 }, { 6297, 0 }, { 6298, 0 }, { 6299, 0 },
	{ 6300, 0 }, { 6301, 0 }, { 6302, 0 }, { 6303, 0 }, { 6304, 0 }, { 6305, 0 },
	{ 6306, 0 }, { 6307, 0 }, { 6308, 0 }, { 6309, 0 }, { 6310, 0 }, { 6311, 0 },
	{ 6312, 0 }, { 6313, 0 }, { 6314, 0 }, { 6315, 0 }, { 6316, 0 }, { 6317, 0 },
	{ 6318, 0 }, { 6319, 0 }, { 6320, 0 }, { 6321, 0 }, { 6322, 0 }, { 6323, 0 },
	{ 6324, 0 }, { 6325, 0 }, { 6326, 0 }, { 6327, 0 }, { 6328, 0 }, { 6329, 0 },
	{ 6330, 0 }, { 6331, 0 }, { 6332, 0 }, { 6333, 0 }, { 6334, 0 }, { 6335, 0 },
	{ 6336, 0 }, { 6337, 0 }, { 6338, 0 }, { 6339, 0 }, { 6340, 0 }, { 6341, 0 },
	{ 6342, 0 }, { 6343, 0 }, { 6344, 0 }, { 6345, 0 }, { 6346, 0 }, { 6347, 0 },
	{ 6348, 0 }, { 6349, 0 }, { 6350, 0 }, { 6351, 0 }, { 6352, 0 }, { 6353, 0 },
	{ 6354, 0 }, { 6355, 0 }, { 6356, 0 }, { 6357, 0 }, { 6358, 0 }, { 6359, 0 },
	{ 6360, 0 }, { 6361, 0 }, { 6362, 0 }, { 6363, 0 }, { 6364, 0 }, { 6365, 0 },
	{ 6366, 0 }, { 6367, 0 }, { 6368, 0 }, { 6369, 0 }, { 6370, 0 }, { 6371, 0 },
	{ 6372, 0 }, { 6373, 0 }, { 6374, 0 }, { 6375, 0 }, { 6376, 0 }, { 6377, 0 },
	{ 6378, 0 }, { 6379, 0 }, { 6380, 0 }, { 6381, 0 }, { 6382, 0 }, { 6383, 0 },
	{ 6384, 0 }, { 6385, 0 }, { 6386, 0 }, { 6387, 0 }, { 6388, 0 }, { 6389, 0 },
	{ 6390, 0 }, { 6391, 0 }, { 6392, 0 }, { 6393, 0 }, { 6394, 0 }, { 6395, 0 },
	{ 6396, 0 }, { 6397, 0 }, { 6398, 0 }, { 6399, 0 }, { 6400, 0 }, { 6401, 0 },
	{ 6402, 0 }, { 6403, 0 }, { 6404, 0 }, { 6405, 0 }, { 6406, 0 }, { 6407, 0 },
	{ 6408, 0 }, { 6409, 0 }, { 6410, 0 }, { 6411, 0 }, { 6412, 0 }, { 6413, 0 },
	{ 6414, 0 }, { 6415, 0 }, { 6416, 0 }, { 6417, 0 }, { 6418, 0 }, { 6419, 0 },
	{ 6420, 0 }, { 6421, 0 }, { 6422, 0 }, { 6423, 0 }, { 6424, 0 }, { 6425, 0 },
	{ 6426, 0 }, { 6427, 0 }, { 6428, 0 }, { 6429, 1 }, { 6430, 0 }, { 6431, 0 },
	{ 6432, 0 }, { 6433, 0 }, { 6434, 0 }, { 6435, 0 }, { 6436, 0 }, { 6437, 0 },
	{ 6438, 0 }, { 6439, 0 }, { 6440, 0 }, { 6441, 0 }, { 6442, 0 }, { 6443, 0 },
	{ 6444, 0 }, { 6445, 0 }, { 6446, 0 }, { 6447, 0 }, { 6448, 0 }, { 6449, 0 },
	{ 6450, 0 }, { 6451, 0 }, { 6452, 0 }, { 6453, 0 }, { 6454, 0 }, { 6455, 0 },
	{ 6456, 0 }, { 6457, 0 }, { 6458, 0 }, { 6459, 0 }, { 6460, 0 }, { 6461, 0 },
	{ 6462, 0 }, { 6463, 0 }, { 6464, 0 }, { 6465, 0 }, { 6466, 0 }, { 6467, 0 },
	{ 6468, 0 }, { 6469, 0 }, { 6470, 0 }, { 6471, 0 }, { 6472, 0 }, { 6473, 0 },
	{ 6474, 0 }, { 6475, 0 }, { 6476, 0 }, { 6477, 0 }, { 6478, 0 }, { 6479, 0 },
	{ 6480, 0 }, { 6481, 0 }, { 6482, 0 }, { 6483, 0 }, { 6484, 0 }, { 6485, 0 },
	{ 6486, 0 }, { 6487, 0 }, { 6488, 0 }, { 6489, 0 }, { 6490, 0 }, { 6491, 0 },
	{ 6492, 0 }, { 6493, 0 }, { 6494, 0 }, { 6495, 0 }, { 6496, 0 }, { 6497, 0 },
	{ 6498, 0 }, { 6499, 0 }, { 6500, 0 }, { 6501, 0 }, { 6502, 0 }, { 6503, 0 },
	{ 6504, 0 }, { 6505, 0 }, { 6506, 0 }, { 6507, 0 }, { 6508, 0 }, { 6509, 0 },
	{ 6510, 0 }, { 6511, 0 }, { 6512, 0 }, { 6513, 0 }, { 6514, 0 }, { 6515, 0 },
	{ 6516, 0 }, { 6517, 0 }, { 6518, 0 }, { 6519, 0 }, { 6520, 0 }, { 6521, 0 },
	{ 6522, 0 }, { 6523, 0 }, { 6524, 0 }, { 6525, 0 }, { 6526, 0 }, { 6527, 0 },
	{ 6528, 0 }, { 6529, 0 }, { 6530, 0 }, { 6531, 0 }, { 6532, 0 }, { 6533, 0 },
	{ 6534, 0 }, { 6535, 0 }, { 6536, 0 }, { 6537, 0 }, { 6538, 0 }, { 6539, 0 },
	{ 6540, 0 }, { 6541, 0 }, { 6542, 0 }, { 6543, 0 }, { 6544, 0 }, { 6545, 0 },
	{ 6546, 0 }, { 6547, 0 }, { 6548, 0 }, { 6549, 0 }, { 6550, 0 }, { 6551, 0 },
	{ 6552, 0 }, { 6553, 0 }, { 6554, 0 }, { 6555, 0 }, { 6556, 0 }, { 6557, 0 },
	{ 6558, 0 }, { 6559, 0 }, { 6560, 0 }, { 6561, 0 }, { 6562, 0 }, { 6563, 0 },
	{ 6564, 0 }, { 6565, 0 }, { 6566, 0 }, { 6567, 0 }, { 6568, 0 }, { 6569, 0 },
	{ 6570, 0 }, { 6571, 0 }, { 6572, 0 }, { 6573, 0 }, { 6574, 0 }, { 6575, 0 },
	{ 6576, 0 }, { 6577, 0 }, { 6578, 0 }, { 6579, 0 }, { 6580, 0 }, { 6581, 0 },
	{ 6582, 0 }, { 6583, 0 }, { 6584, 0 }, { 6585, 0 }, { 6586, 0 }, { 6587, 0 },
	{ 6588, 0 }, { 6589, 0 }, { 6590, 0 }, { 6591, 0 }, { 6592, 0 }, { 6593, 0 },
	{ 6594, 0 }, { 6595, 0 }, { 6596, 0 }, { 6597, 0 }, { 6598, 0 }, { 6599, 0 },
	{ 6600, 0 }, { 6601, 0 }, { 6602, 0 }, { 6603, 0 }, { 6604, 0 }, { 6605, 0 },
	{ 6606, 0 }, { 6607, 0 }, { 6608, 0 }, { 6609, 0 }, { 6610, 0 }, { 6611, 0 },
	{ 6612, 0 }, { 6613, 0 }, { 6614, 0 }, { 6615, 0 }, { 6616, 0 }, { 6617, 0 },
	{ 6618, 0 }, { 6619, 0 }, { 6620, 0 }, { 6621, 0 }, { 6622, 0 }, { 6623, 0 },
	{ 6624, 0 }, { 6625, 0 }, { 6626, 0 }, { 6627, 0 }, { 6628, 0 }, { 6629, 0 },
	{ 6630, 0 }, { 6631, 0 }, { 6632, 0 }, { 6633, 0 }, { 6634, 0 }, { 6635, 0 },
	{ 6636, 0 }, { 6637, 0 }, { 6638, 0 }, { 6639, 0 }, { 6640, 0 }, { 6641, 0 },
	{ 6642, 0 }, { 6643, 0 }, { 6644, 0 }, { 6645, 0 }, { 6646, 0 }, { 6647, 0 },
	{ 6648, 0 }, { 6649, 0 }, { 6650, 0 }, { 6651, 0 }, { 6652, 0 }, { 6653, 0 },
	{ 6654, 0 }, { 6655, 0 }, { 6656, 0 }, { 6657, 0 }, { 6658, 0 }, { 6659, 0 },
	{ 6660, 0 }, { 6661, 0 }, { 6662, 0 }, { 6663, 0 }, { 6664, 0 }, { 6665, 0 },
	{ 6666, 0 }, { 6667, 0 }, { 6668, 0 }, { 6669, 0 }, { 6670, 0 }, { 6671, 0 },
	{ 6672, 0 }, { 6673, 0 }, { 6674, 0 }, { 6675, 0 }, { 6676, 0 }, { 6677, 0 },
	{ 6678, 0 }, { 6679, 0 }, { 6680, 0 }, { 6681, 0 }, { 6682, 0 }, { 6683, 0 },
	{ 6684, 0 }, { 6685, 0 }, { 6686, 0 }, { 6687, 0 }, { 6688, 0 }, { 6689, 0 },
	{ 6690, 0 }, { 6691, 0 }, { 6692, 0 }, { 6693, 0 }, { 6694, 0 }, { 6695, 0 },
	{ 6696, 0 }, { 6697, 0 }, { 6698, 0 }, { 6699, 0 }, { 6700, 0 }, { 6701, 0 },
	{ 6702, 0 }, { 6703, 0 }, { 6704, 0 }, { 6705, 0 }, { 6706, 0 }, { 6707, 0 },
	{ 6708, 0 }, { 6709, 0 }, { 6710, 0 }, { 6711, 0 }, { 6712, 0 }, { 6713, 0 },
	{ 6714, 0 }, { 6715, 0 }, { 6716, 0 }, { 6717, 0 }, { 6718, 0 }, { 6719, 0 },
	{ 6720, 0 }, { 6721, 0 }, { 6722, 0 }, { 6723, 0 }, { 6724, 0 }, { 6725, 0 },
	{ 6726, 0 }, { 6727, 0 }, { 6728, 0 }, { 6729, 0 }, { 6730, 0 }, { 6731, 0 },
	{ 6732, 0 }, { 6733, 0 }, { 6734, 0 }, { 6735, 0 }, { 6736, 0 }, { 6737, 0 },
	{ 6738, 0 }, { 6739, 0 }, { 6740, 0 }, { 6741, 0 }, { 6742, 0 }, { 6743, 0 },
	{ 6744, 0 }, { 6745, 0 }, { 6746, 0 }, { 6747, 0 }, { 6748, 0 }, { 6749, 0 },
	{ 6750, 0 }, { 6751, 0 }, { 6752, 0 }, { 6753, 0 }, { 6754, 0 }, { 6755, 0 },
	{ 6756, 0 }, { 6757, 0 }, { 6758, 0 }, { 6759, 0 }, { 6760, 0 }, { 6761, 0 },
	{ 6762, 0 }, { 6763, 0 }, { 6764, 0 }, { 6765, 0 }, { 6766, 0 }, { 6767, 0 },
	{ 6768, 0 }, { 6769, 0 }, { 6770, 0 }, { 6771, 0 }, { 6772, 0 }, { 6773, 0 },
	{ 6774, 0 }, { 6775, 0 }, { 6776, 0 }, { 6777, 0 }, { 6778, 0 }, { 6779, 0 },
	{ 6780, 0 }, { 6781, 0 }, { 6782, 0 }, { 6783, 0 }, { 6784, 0 }, { 6785, 0 },
	{ 6786, 0 }, { 6787, 0 }, { 6788, 0 }, { 6789, 0 }, { 6790, 0 }, { 6791, 0 },
	{ 6792, 0 }, { 6793, 0 }, { 6794, 0 }, { 6795, 0 }, { 6796, 0 }, { 6797, 0 },
	{ 6798, 0 }, { 6799, 0 }, { 6800, 0 }, { 6801, 0 }, { 6802, 0 }, { 6803, 0 },
	{ 6804, 0 }, { 6805, 0 }, { 6806, 0 }, { 6807, 0 }, { 6808, 0 }, { 6809, 0 },
	{ 6810, 0 }, { 6811, 0 }, { 6812, 0 }, { 6813, 0 }, { 6814, 0 }, { 6815, 0 },
	{ 6816, 0 }, { 6817, 0 }, { 6818, 0 }, { 6819, 0 }, { 6820, 0 }, { 6821, 0 },
	{ 6822, 0 }, { 6823, 0 }, { 6824, 0 }, { 6825, 0 }, { 6826, 0 }, { 6827, 0 },
	{ 6828, 0 }, { 6829, 0 }, { 6830, 0 }, { 6831, 0 }, { 6832, 0 }, { 6833, 0 },
	{ 6834, 0 }, { 6835, 0 }, { 6836, 0 }, { 6837, 0 }, { 6838, 0 }, { 6839, 0 },
	{ 6840, 0 }, { 6841, 0 }, { 6842, 0 }, { 6843, 0 }, { 6844, 0 }, { 6845, 0 },
	{ 6846, 0 }, { 6847, 0 }, { 6848, 0 }, { 6849, 0 }, { 6850, 0 }, { 6851, 0 },
	{ 6852, 0 }, { 6853, 0 }, { 6854, 0 }, { 6855, 0 }, { 6856, 0 }, { 6857, 0 },
	{ 6858, 0 }, { 6859, 0 }, { 6860, 0 }, { 6861, 0 }, { 6862, 0 }, { 6863, 0 },
	{ 6864, 0 }, { 6865, 0 }, { 6866, 0 }, { 6867, 0 }, { 6868, 0 }, { 6869, 0 },
	{ 6870, 0 }, { 6871, 0 }, { 6872, 0 }, { 6873, 0 }, { 6874, 0 }, { 6875, 0 },
	{ 6876, 0 }, { 6877, 0 }, { 6878, 0 }, { 6879, 0 }, { 6880, 0 }, { 6881, 0 },
	{ 6882, 0 }, { 6883, 0 }, { 6884, 0 }, { 6885, 0 }, { 6886, 0 }, { 6887, 0 },
	{ 6888, 0 }, { 6889, 0 }, { 6890, 0 }, { 6891, 0 }, { 6892, 0 }, { 6893, 0 },
	{ 6894, 0 }, { 6895, 0 }, { 6896, 0 }, { 6897, 0 }, { 6898, 0 }, { 6899, 0 },
	{ 6900, 0 }, { 6901, 0 }, { 6902, 0 }, { 6903, 0 }, { 6904, 0 }, { 6905, 0 },
	{ 6906, 0 }, { 6907, 0 }, { 6908, 0 }, { 6909, 0 }, { 6910, 0 }, { 6911, 0 },
	{ 6912, 0 }, { 6913, 0 }, { 6914, 0 }, { 6915, 0 }, { 6916, 0 }, { 6917, 0 },
	{ 6918, 0 }, { 6919, 0 }, { 6920, 0 }, { 6921, 0 }, { 6922, 0 }, { 6923, 0 },
	{ 6924, 0 }, { 6925, 0 }, { 6926, 0 }, { 6927, 0 }, { 6928, 0 }, { 6929, 0 },
	{ 6930, 0 }, { 6931, 0 }, { 6932, 0 }, { 6933, 0 }, { 6934, 0 }, { 6935, 0 },
	{ 6936, 0 }, { 6937, 0 }, { 6938, 0 }, { 6939, 0 }, { 6940, 0 }, { 6941, 0 },
	{ 6942, 0 }, { 6943, 0 }, { 6944, 0 }, { 6945, 0 }, { 6946, 0 }, { 6947, 0 },
	{ 6948, 0 }, { 6949, 0 }, { 6950, 0 }, { 6951, 0 }, { 6952, 0 }, { 6953, 0 },
	{ 6954, 0 }, { 6955, 0 }, { 6956, 0 }, { 6957, 0 }, { 6958, 0 }, { 6959, 0 },
	{ 6960, 0 }, { 6961, 0 }, { 6962, 0 }, { 6963, 0 }, { 6964, 0 }, { 6965, 0 },
	{ 6966, 0 }, { 6967, 0 }, { 6968, 0 }, { 6969, 0 }, { 6970, 0 }, { 6971, 0 },
	{ 6972, 0 }, { 6973, 0 }, { 6974, 0 }, { 6975, 0 }, { 6976, 0 }, { 6977, 0 },
	{ 6978, 0 }, { 6979, 0 }, { 6980, 0 }, { 6981, 0 }, { 6982, 0 }, { 6983, 0 },
	{ 6984, 0 }, { 6985, 0 }, { 6986, 0 }, { 6987, 0 }, { 6988, 0 }, { 6989, 0 },
	{ 6990, 0 }, { 6991, 0 }, { 6992, 0 }, { 6993, 0 }, { 6994, 0 }, { 6995, 0 },
	{ 6996, 0 }, { 6997, 0 }, { 6998, 0 }, { 6999, 0 }, { 7000, 0 }, { 7001, 0 },
	{ 7002, 0 }, { 7003, 0 }, { 7004, 0 }, { 7005, 0 }, { 7006, 0 }, { 7007, 0 },
	{ 7008, 0 }, { 7009, 0 }, { 7010, 0 }, { 7011, 0 }, { 7012, 0 }, { 7013, 0 },
	{ 7014, 0 }, { 7015, 0 }, { 7016, 0 }, { 7017, 0 }, { 7018, 0 }, { 7019, 0 },
	{ 7020, 0 }, { 7021, 0 }, { 7022, 0 }, { 7023, 0 }, { 7024, 0 }, { 7025, 0 },
	{ 7026, 0 }, { 7027, 0 }, { 7028, 0 }, { 7029, 0 }, { 7030, 0 }, { 7031, 0 },
	{ 7032, 0 }, { 7033, 0 }, { 7034, 0 }, { 7035, 0 }, { 7036, 0 }, { 7037, 0 },
	{ 7038, 0 }, { 7039, 0 }, { 7040, 0 }, { 7041, 0 }, { 7042, 0 }, { 7043, 0 },
	{ 7044, 0 }, { 7045, 0 }, { 7046, 0 }, { 7047, 0 }, { 7048, 0 }, { 7049, 0 },
	{ 7050, 0 }, { 7051, 0 }, { 7052, 0 }, { 7053, 0 }, { 7054, 0 }, { 7055, 0 },
	{ 7056, 0 }, { 7057, 0 }, { 7058, 0 }, { 7059, 0 }, { 7060, 0 }, { 7061, 0 },
	{ 7062, 0 }, { 7063, 0 }, { 7064, 0 }, { 7065, 0 }, { 7066, 0 }, { 7067, 0 },
	{ 7068, 0 }, { 7069, 0 }, { 7070, 0 }, { 7071, 0 }, { 7072, 0 }, { 7073, 0 },
	{ 7074, 0 }, { 7075, 0 }, { 7076, 0 }, { 7077, 0 }, { 7078, 0 }, { 7079, 0 },
	{ 7080, 0 }, { 7081, 0 }, { 7082, 0 }, { 7083, 0 }, { 7084, 0 }, { 7085, 0 },
	{ 7086, 0 }, { 7087, 0 }, { 7088, 0 }, { 7089, 0 }, { 7090, 0 }, { 7091, 0 },
	{ 7092, 0 }, { 7093, 0 }, { 7094, 0 }, { 7095, 0 }, { 7096, 0 }, { 7097, 0 },
	{ 7098, 0 }, { 7099, 0 }, { 7100, 0 }, { 7101, 0 }, { 7102, 0 }, { 7103, 0 },
	{ 7104, 0 }, { 7105, 0 }, { 7106, 0 }, { 7107, 0 }, { 7108, 0 }, { 7109, 0 },
	{ 7110, 0 }, { 7111, 0 }, { 7112, 0 }, { 7113, 0 }, { 7114, 0 }, { 7115, 0 },
	{ 7116, 0 }, { 7117, 0 }, { 7118, 0 }, { 7119, 0 }, { 7120, 0 }, { 7121, 0 },
	{ 7122, 0 }, { 7123, 0 }, { 7124, 0 }, { 7125, 0 }, { 7126, 0 }, { 7127, 0 },
	{ 7128, 0 }, { 7129, 0 }, { 7130, 0 }, { 7131, 0 }, { 7132, 0 }, { 7133, 0 },
	{ 7134, 0 }, { 7135, 0 }, { 7136, 0 }, { 7137, 0 }, { 7138, 0 }, { 7139, 0 },
	{ 7140, 0 }, { 7141, 0 }, { 7142, 0 }, { 7143, 0 }, { 7144, 0 }, { 7145, 0 },
	{ 7146, 0 }, { 7147, 0 }, { 7148, 0 }, { 7149, 0 }, { 7150, 0 }, { 7151, 0 },
	{ 7152, 0 }, { 7153, 0 }, { 7154, 0 }, { 7155, 0 }, { 7156, 3 }, { 7157, 0 },
	{ 7158, 0 }, { 7159, 0 }, { 7160, 0 }, { 7161, 0 }, { 7162, 0 }, { 7163, 0 },
	{ 7164, 0 }, { 7165, 0 }, { 7166, 0 }, { 7167, 0 }, { 7168, 0 }, { 7169, 0 },
	{ 7170, 0 }, { 7171, 0 }, { 7172, 0 }, { 7173, 0 }, { 7174, 0 }, { 7175, 0 },
	{ 7176, 0 }, { 7177, 0 }, { 7178, 0 }, { 7179, 0 }, { 7180, 0 }, { 7181, 0 },
	{ 7182, 0 }, { 7183, 0 }, { 7184, 0 }, { 7185, 0 }, { 7186, 0 }, { 7187, 0 },
	{ 7188, 0 }, { 7189, 0 }, { 7190, 0 }, { 7191, 0 }, { 7192, 0 }, { 7193, 0 },
	{ 7194, 0 }, { 7195, 0 }, { 7196, 0 }, { 7197, 0 }, { 7198, 0 }, { 7199, 0 },
	{ 7200, 0 }, { 7201, 0 }, { 7202, 0 }, { 7203, 0 }, { 7204, 0 }, { 7205, 0 },
	{ 7206, 0 }, { 7207, 0 }, { 7208, 0 }, { 7209, 0 }, { 7210, 0 }, { 7211, 0 },
	{ 7212, 0 }, { 7213, 0 }, { 7214, 0 }, { 7215, 0 }, { 7216, 0 }, { 7217, 0 },
	{ 7218, 0 }, { 7219, 0 }, { 7220, 0 }, { 7221, 0 }, { 7222, 0 }, { 7223, 0 },
	{ 7224, 0 }, { 50000, 0 }, { 50001, 0 }, { 50002, 0 },
};

static const SceUInt32 swdlBackupMagic = 0x12345670;

/* write one item record to the backup file */
int swdlWriteNvItem(SwdlContext *context, const void *data, SceSize size)
{
	struct {
		SwdlNvRecordHeader header;
		SceUInt8 data[256];
	} record;
	int result;

	if (size > sizeof(record.data))
		return (int)0x80330002;
	record.header.magic = 0x4d49564e;	/* "NVIM" */
#if FW_VERSION >= 0x04000000
	__memcpy_chk(record.data, data, size, sizeof(record.data));
#else
	memcpy(record.data, data, size);
#endif
	record.header.size = size;
	result = udIoWrite(context->backup.fd, &record, size + sizeof(record.header));
	if (result >= 0)
		context->backupSize += result;
	return result;
}

/* save the item a read reply carries (none on an error status); 1 when all are done */
__attribute__((noinline)) int swdlBackupNvItem(SwdlContext *context, const SceUInt8 *reply, SceSize length)
{
	SwdlNvRecordHeader table;
	int result;

	context->nvItemsDone++;
	if ((reply[131] | reply[132] << 8) == 0) {
		if (context->backupSize == 0) {
			result = udIoWrite(context->backup.fd, swdlBackupHeader,
					   sizeof(swdlBackupHeader));
			if (result < 0)
				return result;
			context->backupSize += result;
			table.magic = 0x4c54564e;	/* "NVTL" */
			table.size = 0;
			result = udIoWrite(context->backup.fd, &table, sizeof(table));
			if (result < 0)
				return result;
			context->backupSize += result;
		}
		result = swdlWriteNvItem(context, reply + 1, length - 3);
		if (result < 0)
			return result;
		context->nvItemsWritten++;
	}
	return context->nvItemTotal <= context->nvItemsDone;
}

/* the reads the backup makes: every instance of every item (0xffff without a file) */
__attribute__((noinline)) int swdlCountNvItems(SwdlContext *context)
{
	SceUInt32 total;
	int i;

	if (context->backup.fd == 0) {
		context->nvItemTotal = 0xffff;
		return 0;
	}
	context->nvItemTotal = 6577;
	total = 6577;
	for (i = 0; i < 6577; i++)
		total += swdlNvItems[i].instances;
	context->nvItemTotal = total;
	return 0;
}

/* the item after item (0xfffe: the first), 0xffff after the last */
__attribute__((noinline)) int swdlNextNvItem(SwdlContext *context, SceUInt16 item, SceUInt16 *next)
{
	SceUInt16 id = 0xffff;

	if (item == 0xffff || context == NULL || next == NULL)
		return (int)0x80330002;
	if (item == 0xfffe) {
		context->nvTableIndex = 0;
		id = 0;
		context->nvTableInstance = 0;
	} else if (swdlNvItems[context->nvTableIndex].instances > context->nvTableInstance) {
		id = swdlNvItems[context->nvTableIndex].id;
		context->nvTableInstance++;
	} else {
		context->nvTableInstance = 0;
		context->nvTableIndex++;
		if (context->nvTableIndex <= 6576)
			id = swdlNvItems[context->nvTableIndex].id;
	}
	*next = id;
	return 0;
}

/* write the table header with the count of items saved */
__attribute__((noinline)) int swdlWriteNvTable(SwdlContext *context)
{
	SwdlNvRecordHeader table;
	SceUInt16 count = context->nvItemsWritten;
	int result;

	result = swdlSeekFile(context->backup.fd, sizeof(swdlBackupHeader));
	if (result < 0)
		return result;
	table.magic = 0x4c54564e;	/* "NVTL" */
	table.size = count;
	result = udIoWrite(context->backup.fd, &table, sizeof(table));
	return result < 0 ? result : 0;
}

/* mark the backup file complete */
__attribute__((noinline)) int swdlWriteBackupMagic(SwdlContext *context)
{
	int result;

	result = swdlSeekFile(context->backup.fd, 16);
	if (result < 0)
		return result;
	result = udIoWrite(context->backup.fd, &swdlBackupMagic, sizeof(swdlBackupMagic));
	return result < 0 ? result : 0;
}

/* check the backup file to restore and take its item count (0xffff: no file) */
int swdlOpenBackup(SwdlContext *context)
{
	SwdlNvRecordHeader table;
	SceUInt32 magic;
	int result;

	if (context->restore.fd == 0) {
		context->nvItemTotal = 0xffff;
		return 0;
	}
	context->nvItemTotal = 0;
	result = swdlSeekFile(context->restore.fd, 16);
	if (result < 0)
		return result;
	result = udIoRead(context->restore.fd, &magic, sizeof(magic));
	if (result < 0)
		return result;
	if (memcmp(&magic, &swdlBackupMagic, sizeof(magic)) != 0)
		return (int)0x80330704;
	result = swdlSeekFile(context->restore.fd, sizeof(swdlBackupHeader));
	if (result < 0)
		return result;
	result = udIoRead(context->restore.fd, &table, sizeof(table));
	if (result < 0)
		return result;
	if (memcmp(&table.magic, "NVTL", 4) != 0)
		return (int)0x80330702;
	result = swdlSeekFile(context->restore.fd, sizeof(swdlBackupHeader) + sizeof(table));
	if (result < 0)
		return result;
	context->nvItemTotal = table.size;
	return 0;
}

/* read the item record index of the backup file (records are 138 bytes apart) */
int swdlReadNvItem(SwdlContext *context, void *data, SceSize *size, SceUInt32 index)
{
	SwdlNvRecordHeader header;
	int result;

	result = swdlSeekFile(context->restore.fd, index * 138 + 28);
	if (result < 0)
		return result;
	result = udIoRead(context->restore.fd, &header, sizeof(header));
	if (result < 0)
		return result;
	if (memcmp(&header.magic, "NVIM", 4) != 0)
		return (int)0x80330702;
	if (*size < header.size)
		return (int)0x80330002;
	result = udIoRead(context->restore.fd, data, header.size);
	if (result < 0)
		return result;
	*size = result;
	return 0;
}

/* the port the download runs over: one, over the modem's USB serial channel */
SwdlPort swdlPortStorage;

/* name unknown: the length of the frame the receive ring holds, 0 while there is none */
int swdlPortGetFrameLength(SwdlPortInterface *interface, SceSize *length)
{
	SwdlPort *port;

	if (length == NULL || interface == NULL)
		return (int)0x80330002;
	port = interface->owner;
	if (port == NULL)
		return (int)0x80330002;
	*length = port->frameLength;
	return 0;
}

/* name unknown: look for the end of a frame (0x7e) in the receive ring; returns its
 * length, 0 while the ring holds none */
int swdlPortFindFrame(SwdlPort *port)
{
	SceSize length;
	SceUInt32 writePos;
	SceUInt32 pos;

	lockFastMutex(&port->rxLock);
	length = port->frameLength;
	if (length != 0)
		goto unlock;
	writePos = port->writePos;
	for (;;) {
		pos = port->scanPos;
		if (pos == writePos)
			break;
		length = port->frameScan;
		if (port->ring[pos] != 0x7e) {
			do {
				length++;
				if (pos + 1 < 1024) {
					pos++;
					port->scanPos = pos;
				} else {
					pos -= 1023;
					port->scanPos = pos;
				}
				if (pos == writePos) {
					port->frameScan = length;
					goto none;
				}
			} while (port->ring[pos] != 0x7e);
			port->frameScan = length;
		}
		if (length != 0) {
			port->frameScan = 0;
			port->frameLength = length;
			goto unlock;
		}
		/* a flag with nothing before it: drop it */
		if (port->scanPos + 1 < 1024)
			port->scanPos++;
		else
			port->scanPos -= 1023;
		if (port->readPos + 1 < 1024)
			port->readPos++;
		else
			port->readPos -= 1023;
		port->rxCount--;
	}
none:
	length = 0;
unlock:
	sceKernelUnlockFastMutex(&port->rxLock);
	return length;
}

/* name unknown: stop reading from the modem */
int swdlPortStopReceive(SwdlPortInterface *interface, int flags)
{
	SwdlPort *port;

	if (interface == NULL)
		return (int)0x80330002;
	port = interface->owner;
	if (port == NULL)
		return (int)0x80330002;
	lockFastMutex(&port->rxLock);
	port->rxActive = 0;
	sceKernelUnlockFastMutex(&port->rxLock);
	return sceKernelSetEventFlag(port->eventFlagId, 8);
}

/* a control argument: one word */
static inline int swdlPortIsWord(const void *data, SceSize size)
{
	int valid = size == 4;

	if (data == NULL)
		valid = 0;
	return valid;
}

/* name unknown: 0 packet size, 1 free packets, 2 abort, 5 last error, 6 raw mode */
int swdlPortControl(SwdlPortInterface *interface, int command, void *data, SceSize size)
{
	SwdlPort *port;
	int rawMode;

	if (interface == NULL)
		return (int)0x80330002;
	port = interface->owner;
	if (port == NULL)
		return (int)0x80330002;
	if (port->connected == 0)
		return (int)0x80330006;

	switch (command) {
	case 0:
		if (!swdlPortIsWord(data, size))
			return (int)0x80330002;
		*(SceSize *)data = 2048;
		return 0;
	case 1:
		if (!swdlPortIsWord(data, size))
			return (int)0x80330002;
		lockFastMutex(&port->txLock);
		*(SceUInt32 *)data = 16 - port->txCount;
		sceKernelUnlockFastMutex(&port->txLock);
		return 0;
	case 2:
		return sceKernelSetEventFlag(port->eventFlagId, 16);
	case 5:
		if (!swdlPortIsWord(data, size))
			return (int)0x80330002;
		lockFastMutex(&port->errorLock);
		*(int *)data = port->error;
		sceKernelUnlockFastMutex(&port->errorLock);
		return 0;
	case 6:
		if (!swdlPortIsWord(data, size))
			return 0;
		lockFastMutex(&port->errorLock);
		rawMode = *(int *)data;
		port->rawMode = rawMode;
		sceKernelUnlockFastMutex(&port->errorLock);
		if (rawMode != 0)
			return 0;
		/* framing again: scan what the ring holds from its start */
		lockFastMutex(&port->rxLock);
		port->frameScan = 0;
		port->frameLength = 0;
		port->scanPos = port->readPos;
		sceKernelUnlockFastMutex(&port->rxLock);
		if (swdlPortFindFrame(port) != 0)
			interface->eventHandler(interface, 2, NULL, 0);
		return 0;
	default:
		return (int)0x80330002;
	}
}

/* name unknown: a write finished */
int swdlPortTransmitDone(int forwardedValue, BbmcUsbSerialRequest *request, int result)
{
	SwdlPortRequestContext *context;
	SwdlPort *port;
	SceUInt32 events;

	if (request == NULL)
		return (int)0x80330002;
	context = request->context;
	if (context == NULL)
		return (int)0x80330002;
	port = context->port;
	if (port == NULL)
		return (int)0x80330002;
	if (result <= 0) {
		context->result = result;
		return sceKernelSetEventFlag(port->eventFlagId, 256);
	}
	events = context->last == 1 ? 32 : 64;
	return sceKernelSetEventFlag(port->eventFlagId, events);
}

/* name unknown: the abort timer ran out */
SceUInt32 swdlPortAbortTimer(void *argument)
{
	SwdlPort *port = argument;

	if (port->abortEvent != 0)
		sceKernelSetEventFlag(port->eventFlagId, port->abortEvent);
	return 0;
}

/* name unknown: the USB serial device came or went */
void swdlPortUsbStateChanged(SceUInt32 events, void *context)
{
	SwdlPort *port = context;

	if (port == NULL)
		return;
	if (events == 64 || events == 1 || events == 16) {
		port->connected = 1;
		sceKernelSetEventFlag(port->eventFlagId, 2048);
	} else if (events == 128 || events == 2 || events == 32) {
		port->connected = 0;
		sceKernelSetEventFlag(port->eventFlagId, 4096);
	}
}

/* name unknown: a read finished */
int swdlPortReceiveDone(int forwardedValue, BbmcUsbSerialRequest *request, int result)
{
	SwdlPortRequestContext *context;
	SwdlPort *port;

	if (request == NULL)
		return (int)0x80330002;
	context = request->context;
	if (context == NULL)
		return (int)0x80330002;
	port = context->port;
	if (port == NULL)
		return (int)0x80330002;
	if (result < 0) {
		context->result = result;
		return sceKernelSetEventFlag(port->eventFlagId, 512);
	}
	request->arguments.bulk.length = result;
	return sceKernelSetEventFlag(port->eventFlagId, 128);
}

/* name unknown: take what the ring holds: the bytes as they are in raw mode, else the
 * frame, unescaped; wait 1 does not report the next frame */
int swdlPortReceive(SwdlPortInterface *interface, void *data, SceSize *size, int wait)
{
	SwdlPort *port;
	SwdlPortEscape *escape;
	SceSize capacity;
	SceSize available;
	SceSize contiguous;
	SceSize chunk;
	SceSize rest;
	SceSize used;
	SceSize i;
	const SceUInt8 *source;
	SceUInt8 *dest;
	int rawMode;
	int result;

	if (data == NULL || interface == NULL || size == NULL || *size == 0)
		return (int)0x80330002;
	port = interface->owner;
	if (port == NULL)
		return (int)0x80330002;

	lockFastMutex(&port->errorLock);
	rawMode = port->rawMode;
	sceKernelUnlockFastMutex(&port->errorLock);
	lockFastMutex(&port->rxLock);

	if (rawMode == 1) {
		available = port->rxCount;
		capacity = *size;
		contiguous = 1024 - port->readPos;
		source = &port->ring[port->readPos];
		*size = 0;
		do {
			chunk = available < capacity ? available : capacity;
			if (chunk >= contiguous)
				chunk = contiguous;
			memcpy(data, source, chunk);
			if (port->readPos + chunk < 1024)
				port->readPos += chunk;
			else
				port->readPos += chunk - 1024;
			available -= chunk;
			capacity -= chunk;
			port->rxCount -= chunk;
			*size += chunk;
			if (capacity == 0 && available != 0) {
				result = (int)0x80330005;
				goto unlock;
			}
		} while (available != 0);
		result = 0;
unlock:
		sceKernelUnlockFastMutex(&port->rxLock);
		return result;
	}

	/* the frame may wrap around the end of the ring */
	chunk = port->frameLength;
	contiguous = 1024 - port->readPos;
	source = &port->ring[port->readPos];
	if (chunk > contiguous) {
		rest = chunk - contiguous;
		chunk = contiguous;
	} else {
		rest = 0;
	}
	escape = &port->receive;
	capacity = *size;
	memset(escape, 0, sizeof(*escape));
	escape->capacity = capacity;
	dest = data;
	escape->dest = dest;
	escape->source = source;
	escape->sourceLength = chunk;
	used = 0;
	for (;;) {
		for (i = chunk; i != 0; i--) {
			if (escape->length >= escape->capacity)
				break;
			if (source[used] == 0x7d) {
				escape->sourcePos = ++used;
				escape->escaped = 1;
			} else if (source[used] == 0x7e) {
				escape->sourcePos = ++used;
			} else {
				escape->sourcePos = used + 1;
				if (escape->escaped == 1) {
					dest[escape->length++] = source[used] ^ 0x20;
					escape->escaped = 0;
				} else {
					dest[escape->length++] = source[used];
				}
				used = escape->sourcePos;
			}
		}
		port->readPos = port->readPos + used < 1024 ? port->readPos + used :
			port->readPos + used - 1024;
		port->rxCount -= used;
		port->frameLength -= used;
		if (escape->sourceLength != used) {
			sceKernelUnlockFastMutex(&port->rxLock);
			return (int)0x80330005;
		}
		if (rest == 0)
			break;
		escape->sourceLength = rest;
		source = &port->ring[port->readPos];
		chunk = rest;
		rest = 0;
		escape->source = source;
		escape->sourcePos = 0;
		used = 0;
		dest = escape->dest;
	}
	sceKernelUnlockFastMutex(&port->rxLock);
	*size = escape->length;
	result = swdlPortFindFrame(port);
	if (result == 0)
		return 0;
	if (wait != 1)
		return interface->eventHandler(interface, 2, NULL, 0);
	return result;
}

/* name unknown: close the channel open() opened */
int swdlPortClose(SwdlPortInterface *interface, int flags)
{
	SwdlPort *port;
	int result;

	if (interface == NULL)
		return (int)0x80330002;
	port = interface->owner;
	if (port == NULL)
		return (int)0x80330002;
	lockFastMutex(&port->rxLock);
	port->rxActive = 0;
	sceKernelUnlockFastMutex(&port->rxLock);
	switch (port->mode) {
	case 1:
		result = bbmcCloseUsbSerialQcom(port->channel);
		break;
	case 2:
		result = bbmcCloseUsbSerialQedl(port->channel);
		break;
	case 3:
		result = bbmcCloseUsbSerialQdl(port->channel);
		break;
	default:
		return (int)0x80330002;
	}
	if (result < 0)
		return result;
	return sceKernelSetEventFlag(port->eventFlagId, 8);
}

/* name unknown: open the modem's channel: 1 QCOM, 2 QEDL, 3 QDL */
int swdlPortOpen(SwdlPortInterface *interface, int mode)
{
	SwdlPort *port;
	int result;

	if (interface == NULL)
		return (int)0x80330002;
	port = interface->owner;
	if (port == NULL)
		return (int)0x80330002;
	if (port->connected == 0)
		return (int)0x80330006;
	switch (mode) {
	case 1:
		result = bbmcOpenUsbSerialQcom(&port->channel);
		break;
	case 2:
		result = bbmcOpenUsbSerialQedl(&port->channel);
		break;
	case 3:
		result = bbmcOpenUsbSerialQdl(&port->channel);
		break;
	default:
		return (int)0x80330002;
	}
	if (result < 0)
		return result;
	port->mode = mode;
	return sceKernelSetEventFlag(port->eventFlagId, 4);
}

/* name unknown: a bulk request with its buffer (64-byte aligned) and its context */
BbmcUsbSerialRequest *swdlPortAllocRequest(SceUID *heapId, SceSize size)
{
	BbmcUsbSerialRequest *request;
	SceAllocOpt opt = {
		.size = sizeof(opt),
		.align = 64,
	};

	request = sceKernelAllocHeapMemory(*heapId, sizeof(*request));
	if (request == NULL)
		return NULL;
	memset(request, 0, sizeof(*request));
	request->transferType = 1;
	request->arguments.bulk.data = sceKernelAllocHeapMemoryWithOption(*heapId, size, &opt);
	if (request->arguments.bulk.data == NULL) {
		sceKernelFreeHeapMemory(*heapId, request);
		return NULL;
	}
	request->context = sceKernelAllocHeapMemory(*heapId, sizeof(SwdlPortRequestContext));
	if (request->context == NULL) {
		sceKernelFreeHeapMemory(*heapId, request->arguments.bulk.data);
		sceKernelFreeHeapMemory(*heapId, request);
		return NULL;
	}
	memset(request->context, 0, sizeof(SwdlPortRequestContext));
	return request;
}

static inline void swdlPortFreeRequest(SwdlPort *port, BbmcUsbSerialRequest *request)
{
	sceKernelFreeHeapMemory(port->heapId, request->context);
	sceKernelFreeHeapMemory(port->heapId, request->arguments.bulk.data);
	sceKernelFreeHeapMemory(port->heapId, request);
}

/* the one read request, created with the port */
static inline int swdlPortCreateReceiveRequest(SwdlPort *port)
{
	BbmcUsbSerialRequest *request;
	SwdlPortRequestContext *context;

	request = swdlPortAllocRequest(&port->heapId, 1024);
	if (request == NULL)
		return (int)0x80330005;
	context = request->context;
	request->completed = swdlPortReceiveDone;
	request->transferType = 1;
	request->arguments.bulk.length = 1024;
	context->port = port;
	lockFastMutex(&port->rxLock);
	port->rxRequest = request;
	sceKernelUnlockFastMutex(&port->rxLock);
	return 0;
}

static inline void swdlPortDeleteReceiveRequest(SwdlPort *port)
{
	BbmcUsbSerialRequest *request;

	lockFastMutex(&port->rxLock);
	request = port->rxRequest;
	port->rxRequest = NULL;
	sceKernelUnlockFastMutex(&port->rxLock);
	if (request != NULL)
		swdlPortFreeRequest(port, request);
}

/* the transmit queue, under txLock */
static inline void swdlPortQueueTransmit(SwdlPort *port, BbmcUsbSerialRequest *request)
{
	SwdlPortRequestContext *tail;

	lockFastMutex(&port->txLock);
	if (port->txHead == NULL) {
		port->txHead = request;
		port->txTail = request;
	} else {
		tail = port->txTail->context;
		tail->next = request;
		port->txTail = request;
	}
	port->txCount++;
	sceKernelUnlockFastMutex(&port->txLock);
	sceKernelSetEventFlag(port->eventFlagId, 2);
}

static inline BbmcUsbSerialRequest *swdlPortDequeueTransmit(SwdlPort *port)
{
	BbmcUsbSerialRequest *request;
	SwdlPortRequestContext *context;

	lockFastMutex(&port->txLock);
	request = port->txHead;
	if (request != NULL) {
		context = request->context;
		if (request == port->txTail)
			port->txTail = NULL;
		port->txHead = context->next;
		context->next = NULL;
		port->txCount--;
	}
	sceKernelUnlockFastMutex(&port->txLock);
	return request;
}

/* name unknown: queue data for the modem, HDLC-escaped into 2048-byte packets, or as it
 * is in raw mode; *data and *size come back past what was queued */
int swdlPortSend(SwdlPortInterface *interface, const void **data, SceSize *size, int flags)
{
	SwdlPort *port;
	SwdlPortEscape *escape;
	SwdlPortRequestContext *context;
	BbmcUsbSerialRequest *request;
	const SceUInt8 *source;
	const SceUInt8 *packet;
	SceUInt8 *dest;
	SceSize remaining;
	SceSize chunk;
	SceUInt32 free;
	int rawMode;
	int more;
	SceUInt8 c;

	if (data == NULL || interface == NULL || size == NULL)
		return (int)0x80330002;
	port = interface->owner;
	if (port == NULL)
		return (int)0x80330002;
	if (port->connected == 0)
		return (int)0x80330006;
	source = *data;
	remaining = *size;
	lockFastMutex(&port->errorLock);
	rawMode = port->rawMode;
	sceKernelUnlockFastMutex(&port->errorLock);

	if (rawMode != 1) {
		escape = &port->transmit;
		memset(escape, 0, sizeof(*escape));
		escape->dest = NULL;
		escape->source = source;
		escape->capacity = 0;
		escape->sourceLength = remaining;
		do {
			request = swdlPortAllocRequest(&port->heapId, 2048);
			if (request == NULL)
				goto no_memory;
			dest = request->arguments.bulk.data;
			packet = escape->source;
			escape->capacity = 2048;
			escape->length = 0;
			escape->dest = dest;
			for (;;) {
				if (escape->escaped == 1) {
					dest[escape->length++] =
						packet[escape->sourcePos++] ^ 0x20;
					escape->escaped = 0;
				} else {
					c = packet[escape->sourcePos];
					if (c == 0x7d || (c == 0x7e && escape->sourcePos != 0 &&
							  escape->sourcePos != escape->sourceLength - 1)) {
						/* escaped: 0x7d, then the byte ^ 0x20 */
						dest[escape->length++] = 0x7d;
						escape->escaped = 1;
					} else {
						dest[escape->length++] =
							packet[escape->sourcePos++];
					}
				}
				if (escape->sourcePos == escape->sourceLength) {
					more = 0;
					break;
				}
				if (escape->length >= escape->capacity) {
					more = 1;
					break;
				}
			}
			request->arguments.bulk.length = escape->length;
			request->transferType = 1;
			remaining = escape->sourceLength - escape->sourcePos;
			source = escape->source + escape->sourcePos;
			context = request->context;
			request->completed = swdlPortTransmitDone;
			context->port = port;
			if (!more)
				context->last = 1;
			swdlPortQueueTransmit(port, request);
		} while (more);
		goto done;
	}

	lockFastMutex(&port->txLock);
	free = 16 - port->txCount;
	sceKernelUnlockFastMutex(&port->txLock);
	if (remaining > free << 11)
		goto no_memory;
	do {
		chunk = remaining < 2048 ? remaining : 2048;
		request = swdlPortAllocRequest(&port->heapId, 2048);
		if (request == NULL)
			goto no_memory;
		request->transferType = 1;
		memcpy(request->arguments.bulk.data, source, chunk);
		source += chunk;
		remaining -= chunk;
		context = request->context;
		request->arguments.bulk.length = chunk;
		request->completed = swdlPortTransmitDone;
		context->port = port;
		if (remaining == 0)
			context->last = 1;
		swdlPortQueueTransmit(port, request);
	} while (remaining != 0);
done:
	*data = source;
	*size = remaining;
	return 0;
no_memory:
	*data = source;
	*size = remaining;
	return (int)0x80330005;
}

/* name unknown: a write failed: record it, report it (4) and drop every queued write */
void swdlPortFailTransmit(SwdlPort *port, int error, BbmcUsbSerialRequest *request)
{
	if (port != NULL) {
		lockFastMutex(&port->errorLock);
		port->error = error;
		sceKernelUnlockFastMutex(&port->errorLock);
	}
	port->interface.eventHandler(&port->interface, 4, NULL, 0);
	if (request != NULL)
		swdlPortFreeRequest(port, request);
	while ((request = swdlPortDequeueTransmit(port)) != NULL)
		swdlPortFreeRequest(port, request);
	port->txCurrent = NULL;
	port->txPending = 0;
}

/* name unknown: drop the queued writes and stop the read in flight (a 1 s timer ends
 * the wait for it); 32 reports a port already idle */
int swdlPortAbort(SwdlPort *port)
{
	BbmcUsbSerialRequest *request;
	int result;

	if (port->txPending == 1) {
		while ((request = swdlPortDequeueTransmit(port)) != NULL)
			swdlPortFreeRequest(port, request);
	}
	if (port->rxPending == 1) {
		lockFastMutex(&port->rxLock);
		port->rxActive = 0;
		sceKernelUnlockFastMutex(&port->rxLock);
		port->abortEvent = 1024;
		result = SceThreadmgrForDriver_C58DF384("SceBbmcSwdlAbort", 1000,
							swdlPortAbortTimer, port);
		if (result < 0) {
			lockFastMutex(&port->errorLock);
			port->error = result;
			sceKernelUnlockFastMutex(&port->errorLock);
			port->interface.eventHandler(&port->interface, 16, NULL, 0);
		} else {
			port->abortTimerId = result;
		}
	} else {
		result = 0;
	}
	if (port->txPending == 1 || port->rxPending == 1)
		port->stopping = 1;
	else if (port->terminating == 0)
		port->interface.eventHandler(&port->interface, 32, NULL, 0);
	return result;
}

static inline void swdlPortStartTransmit(SwdlPort *port)
{
	BbmcUsbSerialRequest *request;
	int result;

	request = swdlPortDequeueTransmit(port);
	if (request == NULL) {
		port->txPending = 0;
		return;
	}
	port->txCurrent = request;
	port->txPending = 1;
	result = bbmcWriteUsbSerial(port->channel, request);
	if (result < 0)
		swdlPortFailTransmit(port, result, request);
}

static inline int swdlPortStartReceive(SwdlPort *port)
{
	BbmcUsbSerialRequest *request;
	SwdlPortRequestContext *context;

	lockFastMutex(&port->rxLock);
	request = port->rxRequest;
	context = request->context;
	request->transferType = 1;
	request->arguments.bulk.length = 1024;
	context->result = 0;
	sceKernelUnlockFastMutex(&port->rxLock);
	port->rxPending = 1;
	return bbmcReadUsbSerial(port->channel, request);
}

static inline int swdlPortIsReceiving(SwdlPort *port)
{
	int active;

	lockFastMutex(&port->rxLock);
	active = port->rxActive;
	sceKernelUnlockFastMutex(&port->rxLock);
	return active;
}

/* a read failed: record it, and report it (8) while the reading goes on */
static inline void swdlPortFailReceive(SwdlPort *port, int error)
{
	lockFastMutex(&port->errorLock);
	port->error = error;
	sceKernelUnlockFastMutex(&port->errorLock);
	port->rxPending = 0;
	if (swdlPortIsReceiving(port) == 1)
		port->interface.eventHandler(&port->interface, 8, NULL, 0);
}

/* name unknown: the port's thread (SceBbmcSwdlPort): it submits the writes and reads
 * and reports to the event handler (1 sent, 2 frame, 4 write error, 8 read error,
 * 16 abort error, 32 stopped, 64/128 device attached/detached) */
int swdlPortThread(SceSize args, void *argp)
{
	SwdlPort *port;
	SwdlPortInterface *interface;
	BbmcUsbSerialRequest *request;
	SwdlPortRequestContext *context;
	SceUInt32 events;
	const SceUInt8 *source;
	SceSize length;
	SceSize chunk;
	SceSize rest;
	int rawMode;
	int active;
	int result;

	if (argp == NULL)
		return 0;
	port = *(SwdlPort **)argp;
	interface = &port->interface;
	port->terminating = 0;
	for (;;) {
		events = 0;
		result = sceKernelWaitEventFlag(port->eventFlagId, 0x1fff, 1, &events, NULL);
		if (result < 0)
			return result;
		result = sceKernelClearEventFlag(port->eventFlagId, ~events);
		if (result < 0)
			return result;

		if (port->stopping != 1) {
			if (events & 1) {
				port->terminating = 1;
				swdlPortAbort(port);
				if (port->stopping != 1) {
					port->terminating = 0;
					return 0;
				}
				continue;
			}
			if ((events & 2) && port->txPending != 1) {
				request = swdlPortDequeueTransmit(port);
				if (request != NULL) {
					port->txCurrent = request;
					port->txPending = 1;
					result = bbmcWriteUsbSerial(port->channel, request);
					if (result < 0)
						swdlPortFailTransmit(port, result, request);
				}
			}
			if ((events & 4) && swdlPortIsReceiving(port) != 1) {
				lockFastMutex(&port->rxLock);
				port->rxActive = 1;
				sceKernelUnlockFastMutex(&port->rxLock);
				if (port->rxPending != 1) {
					result = swdlPortStartReceive(port);
					if (result < 0)
						swdlPortFailReceive(port, result);
				}
			}
			if (events & 0x10)
				swdlPortAbort(port);
			if (events & 0x20) {
				request = port->txCurrent;
				if (request != NULL)
					swdlPortFreeRequest(port, request);
				port->txCurrent = NULL;
				interface->eventHandler(interface, 1, NULL, 0);
				swdlPortStartTransmit(port);
			}
			if (events & 0x40) {
				request = port->txCurrent;
				if (request != NULL)
					swdlPortFreeRequest(port, request);
				port->txCurrent = NULL;
				swdlPortStartTransmit(port);
			}
			if ((events & 0x80) && swdlPortIsReceiving(port) != 0) {
				/* into the ring, wrapping at its end */
				lockFastMutex(&port->rxLock);
				request = port->rxRequest;
				length = request->arguments.bulk.length;
				source = request->arguments.bulk.data;
				for (chunk = length; chunk != 0; chunk = rest) {
					if (chunk > 1024 - port->writePos) {
						rest = chunk - (1024 - port->writePos);
						chunk = 1024 - port->writePos;
					} else {
						rest = 0;
					}
					memcpy(&port->ring[port->writePos], source, chunk);
					source += chunk;
					port->writePos = port->writePos + chunk < 1024 ?
						port->writePos + chunk :
						port->writePos + chunk - 1024;
					port->rxCount += chunk;
				}
				sceKernelUnlockFastMutex(&port->rxLock);
				if (request->arguments.bulk.length != 0) {
					if (port->rxCount > 1024) {
						swdlPortFailReceive(port, (int)0x8033070A);
						goto received;
					}
					lockFastMutex(&port->errorLock);
					rawMode = port->rawMode;
					sceKernelUnlockFastMutex(&port->errorLock);
					if (rawMode == 1 || swdlPortFindFrame(port) != 0)
						interface->eventHandler(interface, 2, NULL, 0);
				}
				result = swdlPortStartReceive(port);
				if (result < 0)
					swdlPortFailReceive(port, result);
			}
received:
			if (events & 0x100) {
				request = port->txCurrent;
				context = request->context;
				swdlPortFailTransmit(port, context->result, request);
			}
			if (events & 0x200) {
				lockFastMutex(&port->rxLock);
				context = port->rxRequest->context;
				result = context->result;
				sceKernelUnlockFastMutex(&port->rxLock);
				swdlPortFailReceive(port, result);
			}
			if (events & 0x800)
				interface->eventHandler(interface, 64, NULL, 0);
			if (events & 0x1000)
				interface->eventHandler(interface, 128, NULL, 0);
			continue;
		}

		/* stopping: wait for the write and the read in flight to end */
		result = 0;
		if (events & 0x160) {
			request = port->txCurrent;
			if (request != NULL)
				swdlPortFreeRequest(port, request);
			port->txCurrent = NULL;
			port->txPending = 0;
		}
		if (events & 0x80) {
			if (port->abortEvent == 0)
				port->rxPending = 0;
			else
				result = swdlPortStartReceive(port);
		}
		if (events & 0x200) {
			sceKernelCancelAlarm(port->abortTimerId);
			port->rxPending = 0;
			port->abortTimerId = 0;
			port->abortEvent = 0;
		}
		if (events & 0x400) {
			port->rxPending = 0;
			port->abortTimerId = 0;
			port->abortEvent = 0;
		}
		if (events & 0x800) {
			result = 0;
			interface->eventHandler(interface, 64, NULL, 0);
		}
		if (events & 0x1000) {
			result = 0;
			interface->eventHandler(interface, 128, NULL, 0);
		}
		active = swdlPortIsReceiving(port);
		if (result < 0) {
			lockFastMutex(&port->errorLock);
			port->error = result;
			sceKernelUnlockFastMutex(&port->errorLock);
			interface->eventHandler(interface, 8, NULL, 0);
			interface->eventHandler(interface, 32, NULL, 0);
			port->stopping = 0;
		} else if (port->txPending == 0 && (port->rxPending | active) == 0 &&
			   port->terminating == 0) {
			interface->eventHandler(interface, 32, NULL, 0);
			port->stopping = 0;
		}
		if (port->terminating == 1 && port->stopping == 0) {
			port->terminating = 0;
			return 0;
		}
	}
}

/* name unknown: set the port up: its locks, event flag, heap, read request and thread */
SwdlPortInterface *swdlCreatePort(void)
{
	SwdlPort *port;
	int result;

	memset(&swdlPortStorage, 0, sizeof(swdlPortStorage));
	port = &swdlPortStorage;
	swdlPortStorage.initialized = 1;
	if (sceKernelInitializeFastMutex(&swdlPortStorage.txLock, "SwdlPortTxLock", 0, NULL) < 0)
		goto failed;
	if (sceKernelInitializeFastMutex(&port->rxLock, "SwdlPortRxLock", 0, NULL) < 0)
		goto finalize_tx;
	if (sceKernelInitializeFastMutex(&port->errorLock, "SwdlPortErrCodeLock", 0, NULL) < 0)
		goto finalize_rx;
	result = sceKernelCreateEventFlag("SceBbmcSwdlPort", 4096, 0, NULL);
	if (result < 0)
		goto finalize_error;
	port->eventFlagId = result;
	result = sceKernelCreateHeap("SceBbmcSwdlPort", 68144, NULL);
	if (result < 0)
		goto delete_event_flag;
	port->heapId = result;
	if (bbmcRegisterUsbSerialState(swdlPortUsbStateChanged, port) < 0)
		goto delete_heap;
	if (swdlPortCreateReceiveRequest(port) < 0)
		goto unregister;
	result = sceKernelCreateThread("SceBbmcSwdlPort", swdlPortThread, 96, 0x2000, 0, 0x80000,
				       NULL);
	if (result < 0)
		goto free_request;
	port->threadId = result;
	if (sceKernelStartThread(result, sizeof(port), &port) < 0) {
		sceKernelDeleteThread(port->threadId);
		goto free_request;
	}
	port->interface.open = swdlPortOpen;
	port->interface.owner = port;
	port->interface.close = swdlPortClose;
	port->interface.send = swdlPortSend;
	port->interface.control = swdlPortControl;
	port->interface.receive = swdlPortReceive;
	port->interface.getFrameLength = swdlPortGetFrameLength;
	port->interface.stopReceive = swdlPortStopReceive;
	return &port->interface;

free_request:
	swdlPortDeleteReceiveRequest(port);
unregister:
	bbmcUnregisterUsbSerialState(swdlPortUsbStateChanged);
delete_heap:
	sceKernelDeleteHeap(port->heapId);
delete_event_flag:
	sceKernelDeleteEventFlag(port->eventFlagId);
finalize_error:
	sceKernelFinalizeFastMutex(&port->errorLock);
finalize_rx:
	sceKernelFinalizeFastMutex(&port->rxLock);
finalize_tx:
	sceKernelFinalizeFastMutex(&port->txLock);
failed:
	port->initialized = 0;
	return NULL;
}

/* name unknown: end the port's thread (10 s at most) and release what create set up */
void swdlDestroyPort(SwdlPortInterface *interface)
{
	SwdlPort *port;
	SceUInt32 timeout = 10000000;

	if (interface == NULL)
		return;
	port = interface->owner;
	if (port == NULL || port->initialized == 0)
		return;
	if (sceKernelSetEventFlag(port->eventFlagId, 1) >= 0)
		sceKernelWaitThreadEnd(port->threadId, NULL, &timeout);
	sceKernelDeleteThread(port->threadId);
	swdlPortDeleteReceiveRequest(port);
	bbmcUnregisterUsbSerialState(swdlPortUsbStateChanged);
	sceKernelDeleteHeap(port->heapId);
	sceKernelDeleteEventFlag(port->eventFlagId);
	sceKernelFinalizeFastMutex(&port->errorLock);
	sceKernelFinalizeFastMutex(&port->rxLock);
	sceKernelFinalizeFastMutex(&port->txLock);
	port->initialized = 0;
}

/* the last step of each sequence: the sequence is over */
int swdlBackupEndSend(SwdlContext *context)
{
	return 3;
}

int swdlDownloadEndSend(SwdlContext *context)
{
	return 3;
}

int swdlModeChangeEndSend(SwdlContext *context)
{
	return 3;
}

int swdlUpdateEndSend(SwdlContext *context)
{
	return 3;
}

int swdlRestoreEndSend(SwdlContext *context)
{
	return 3;
}

int swdlBackupEndReceive(SwdlContext *context)
{
	return 3;
}

int swdlDownloadEndReceive(SwdlContext *context)
{
	return 3;
}

int swdlModeChangeEndReceive(SwdlContext *context)
{
	return 3;
}

int swdlUpdateEndReceive(SwdlContext *context)
{
	return 3;
}

int swdlRestoreEndReceive(SwdlContext *context)
{
	return 3;
}

/* name unknown: DIAG command 0x30, argument 0 */
int swdlSendCommand30Start(SwdlContext *context)
{
	SceUInt8 *buffer = context->buffer;

	buffer[0] = 0x30;
	buffer[1] = 0;
	buffer[2] = 0;
	context->bufferLength = 3;
	swdlEndFrame(buffer, &context->bufferLength, buffer + 3);
	context->transmitState = 4;
	return 0;
}

/* name unknown: DIAG command 0x30, argument 1 */
int swdlSendCommand30End(SwdlContext *context)
{
	SceUInt8 *buffer = context->buffer;

	buffer[0] = 0x30;
	buffer[1] = 0;
	buffer[2] = 1;
	context->bufferLength = 3;
	swdlEndFrame(buffer, &context->bufferLength, buffer + 3);
	context->transmitState = 4;
	return 0;
}

/* name unknown: DIAG command 0x2f */
int swdlSendCommand2f(SwdlContext *context)
{
	SceUInt8 *buffer = context->buffer;

	buffer[0] = 0x2f;
	buffer[1] = 0;
	buffer[2] = 0;
	context->bufferLength = 3;
	swdlEndFrame(buffer, &context->bufferLength, buffer + 3);
	context->transmitState = 4;
	return 0;
}

/* name unknown: the status request: DIAG 0x0c in mode 1, subsystem 4 command 0x0e in
 * mode 2 */
static inline int swdlSendStatus(SwdlContext *context)
{
	SceUInt8 *buffer;

	if (context->mode == 1) {
		buffer = context->buffer;
		buffer[0] = 0x0c;
		context->bufferLength = 1;
		swdlEndFrame(buffer, &context->bufferLength, buffer + 1);
		context->transmitState = 4;
		return 0;
	}
	if (context->mode == 2) {
		buffer = context->buffer;
		buffer[0] = 0x4b;
		buffer[2] = 0x0e;
		buffer[3] = 0;
		buffer[1] = 4;
		context->bufferLength = 4;
		swdlEndFrame(buffer, &context->bufferLength, buffer + 4);
		context->transmitState = 4;
		return 0;
	}
	return (int)0x80330002;
}

int swdlBackupSendStatus(SwdlContext *context)
{
	return swdlSendStatus(context);
}

int swdlModeChangeSendStatus(SwdlContext *context)
{
	return swdlSendStatus(context);
}

int swdlRestoreSendStatus(SwdlContext *context)
{
	return swdlSendStatus(context);
}

/* name unknown: the service programming code (DIAG 0x41), the empty slot's "000000" */
typedef struct __attribute__((packed)) SwdlSpc {
	SceUInt32 low;
	SceUInt16 high;
} SwdlSpc;

static inline void swdlSendSpc(SwdlContext *context)
{
	const void *version;
	SceSize size;
	SceUInt8 *buffer;
	SceUInt8 *cursor;

	swdlGetZeroVersion(&version, &size);
	buffer = context->buffer;
	cursor = buffer;
	*cursor++ = 0x41;
	((SwdlSpc *)cursor)->low = ((const SceUInt32 *)version)[0];
	((SwdlSpc *)cursor)->high = ((const SceUInt16 *)version)[2];
	context->bufferLength = 7;
	swdlEndFrame(buffer, &context->bufferLength, buffer + 7);
	context->transmitState = 4;
}

int swdlBackupSendSpc(SwdlContext *context)
{
	swdlSendSpc(context);
	return 0;
}

int swdlRestoreSendSpc(SwdlContext *context)
{
	if (context->restore.fd == 0)
		return 1;
	swdlSendSpc(context);
	return 0;
}

/* name unknown: the replies to commands 0x30 and 0x2f */
int swdlReceiveCommand30Start(SwdlContext *context)
{
	switch (context->receiveEvent) {
	case 0x200:
		if (context->receiveBuffer[0] == 0x30)
			return 1;
		return (int)0x80330708;
	case 0x400:
		context->transmitState = 4;
		return 0;
	case 0x800:
		return swdlGetPortError(context->port);
	default:
		return (int)0x80330002;
	}
}

int swdlReceiveCommand30End(SwdlContext *context)
{
	switch (context->receiveEvent) {
	case 0x200:
		if (context->receiveBuffer[0] == 0x30)
			return 1;
		return (int)0x80330708;
	case 0x400:
		context->transmitState = 4;
		return 0;
	case 0x800:
		return swdlGetPortError(context->port);
	default:
		return (int)0x80330002;
	}
}

int swdlReceiveCommand2f(SwdlContext *context)
{
	switch (context->receiveEvent) {
	case 0x200:
		if (context->receiveBuffer[0] == 0x2f)
			return 1;
		return (int)0x80330708;
	case 0x400:
		context->transmitState = 4;
		return 0;
	case 0x800:
		return swdlGetPortError(context->port);
	default:
		return (int)0x80330002;
	}
}

/* name unknown: the SPC reply: 0x41 with 1 when the code was accepted */
static inline int swdlReceiveSpc(SwdlContext *context)
{
	switch (context->receiveEvent) {
	case 0x200:
		if (context->receiveBuffer[0] != 0x41)
			return (int)0x80330708;
		if (context->receiveBuffer[1] == 1)
			return 1;
		return (int)0x80330709;
	case 0x400:
		context->transmitState = 4;
		return 0;
	case 0x800:
		return swdlGetPortError(context->port);
	default:
		return (int)0x80330002;
	}
}

int swdlBackupReceiveSpc(SwdlContext *context)
{
	return swdlReceiveSpc(context);
}

int swdlRestoreReceiveSpc(SwdlContext *context)
{
	return swdlReceiveSpc(context);
}

/* name unknown: the status reply; a mode 1 request the target rejects (0x13) is sent
 * again in mode 2 */
static inline int swdlReceiveStatus(SwdlContext *context)
{
	switch (context->receiveEvent) {
	case 0x200:
		if (context->mode == 1) {
			if (context->receiveBuffer[0] == 0x0c)
				return 1;
			if (context->receiveBuffer[0] != 0x13)
				return (int)0x80330708;
			context->transmitState = 1;
			context->mode = 2;
			return 0;
		}
		if (context->mode != 2)
			return (int)0x80330708;
		if (context->receiveBuffer[0] == 0x4b)
			return 1;
		return (int)0x80330708;
	case 0x400:
		context->transmitState = 4;
		return 0;
	case 0x800:
		return swdlGetPortError(context->port);
	default:
		return (int)0x80330002;
	}
}

int swdlBackupReceiveStatus(SwdlContext *context)
{
	return swdlReceiveStatus(context);
}

int swdlModeChangeReceiveStatus(SwdlContext *context)
{
	return swdlReceiveStatus(context);
}

int swdlRestoreReceiveStatus(SwdlContext *context)
{
	return swdlReceiveStatus(context);
}

/* name unknown: an NV read reply (0x26) goes to the backup file; a rejected item
 * (0x13 to 0x15) is skipped */
int swdlBackupReceiveNvItem(SwdlContext *context)
{
	SceUInt32 command;
	int result;

	switch (context->receiveEvent) {
	case 0x200:
		command = context->receiveBuffer[0];
		if (command == 0x26) {
			result = swdlBackupNvItem(context, context->receiveBuffer,
						  context->receiveLength - 2);
			if (result == 0)
				context->transmitState = 1;
			return result;
		}
		if (command - 0x13 > 2)
			return (int)0x80330708;
		context->transmitState = 1;
		context->nvItemsDone++;
		return 0;
	case 0x400:
		context->transmitState = 4;
		return 0;
	case 0x800:
		return swdlGetPortError(context->port);
	default:
		return (int)0x80330002;
	}
}

/* name unknown: read the next NV item (DIAG 0x26) */
int swdlBackupSendNvItem(SwdlContext *context)
{
	SceUInt8 *buffer;
	int result;

	if (context->backup.fd == 0)
		return 1;
	result = swdlNextNvItem(context, context->nvItem, &context->nvItem);
	if (result < 0)
		return result;
	if (context->nvItem == 0xffff)
		return 1;
	buffer = context->buffer;
	buffer[0] = 0x26;
	buffer[1] = context->nvItem;
	buffer[131] = 0;
	buffer[132] = 0;
	buffer[2] = context->nvItem >> 8;
	memset(buffer + 3, 0, 0x80);
	buffer[3] = context->nvTableInstance;
	context->bufferLength = 133;
	swdlEndFrame(buffer, &context->bufferLength, buffer + 133);
	context->transmitState = 4;
	return result;
}

/* name unknown: the backup sequence */
static const SwdlStep swdlBackupSteps[7] = {
	{ swdlBackupSendStatus, swdlBackupReceiveStatus },
	{ swdlBackupSendSpc, swdlBackupReceiveSpc },
	{ swdlSendCommand2f, swdlReceiveCommand2f },
	{ swdlSendCommand30End, swdlReceiveCommand30End },
	{ swdlBackupSendNvItem, swdlBackupReceiveNvItem },
	{ swdlSendCommand30Start, swdlReceiveCommand30Start },
	{ swdlBackupEndSend, swdlBackupEndReceive },
};

/* the backup is over: close the port and complete the file */
static inline void swdlFinishBackup(SwdlContext *context)
{
	context->port->close(context->port, 0);
	swdlReleaseBuffers(context);
	swdlWriteNvTable(context);
	swdlWriteBackupMagic(context);
	context->notify(1, &context->notification);
	context->state = 0;
}

/* name unknown: state 1, the NV backup */
int swdlExecBackup(SwdlContext *context, SceUInt32 events)
{
	SwdlPortInterface *port = context->port;
	SceUInt32 total;
	SceUInt32 progress;
	int result = events & 2;

	if (events & 2) {
		context->nvItemsDone = 0;
		context->reserved528 = 0;
		context->nvItem = 0xfffe;
		context->backupSize = 0;
		context->reserved520 = 0;
		context->nvItemsWritten = 0;
		context->progress = 0;
		context->reportedProgress = 0;
		memset(&context->stepIndex, 0, 0x468);
		context->mode = 2;
		context->steps = swdlBackupSteps;
		result = swdlAllocateFrameBuffer(context);
		if (result < 0)
			goto error;
		result = swdlCountNvItems(context);
		if (result < 0)
			goto error;
		result = swdlSetResponseTimeout(context, 3000);
		if (result < 0)
			goto error;
		result = port->open(port, 1);
		if (result < 0)
			goto error;
		result = swdlStartSend(context);
		if (result < 0)
			goto error;
		if (result == 3)
			swdlFinishBackup(context);
	}
	if (events & 0x200) {
		result = swdlReceiveFrame(context, NULL);
		if (result < 0)
			goto error;
		total = context->nvItemTotal;
		if (total != 0 && total != 0xffff) {
			progress = 100 * context->nvItemsDone / total;
			context->progress = progress;
			if (progress > context->reportedProgress + 1) {
				context->reportedProgress = progress;
				context->notification.forwardedValue1 = (SceUInt8)progress;
				context->notify(64, &context->notification);
			}
		}
		result = swdlSendFrames(context);
		if (result < 0)
			goto error;
		if (result == 3)
			swdlFinishBackup(context);
	}
	if (events & 0x1800) {
		result = swdlHandleCancel(context);
		if (result < 0)
			goto error;
		result = swdlSendFrames(context);
		if (result < 0)
			goto error;
		if (result == 3)
			swdlFinishBackup(context);
	}
	if (events & 0x400) {
		result = swdlHandleFrameTimeout(context);
		if (result < 0)
			goto error;
		result = swdlSendFrames(context);
		if (result < 0)
			goto error;
		if (result == 3)
			swdlFinishBackup(context);
	}
	if (events & 0x100000) {
		result = context->lastError;
error:
		port->close(port, 0);
		swdlReleaseBuffers(context);
		context->notification.forwardedValue2 = result;
		context->notify(128, &context->notification);
	}
	return result;
}

/* name unknown: start at the first image's open */
int swdlDownloadStartImages(SwdlContext *context)
{
	context->phase = 1;
	return 1;
}

int swdlUpdateStartImages(SwdlContext *context)
{
	context->phase = 1;
	return 1;
}

/* name unknown: a step that waits for no reply */
int swdlDownloadSkipReply(SwdlContext *context)
{
	return 1;
}

int swdlUpdateSkipReply(SwdlContext *context)
{
	return 1;
}

/* name unknown: send nothing, wait for the device to come back */
int swdlDownloadWaitAttach(SwdlContext *context)
{
	context->transmitState = 4;
	context->bufferLength = 0;
	return 0;
}

int swdlDownloadReceiveAttach(SwdlContext *context)
{
	switch (context->receiveEvent) {
	case 0x200:
	case 0x400:
	case 0x800:
		return 0;
	case 0x20000:
		return 1;
	default:
		return (int)0x80330708;
	}
}

/* name unknown: the streaming reset (0x2d), sent without waiting for a reply */
int swdlDownloadSendReset(SwdlContext *context)
{
	SceUInt8 *buffer = context->buffer;

	buffer[0] = 0x7e;
	buffer[1] = 0x2d;
	context->bufferLength = 1;
	swdlEndFrame(buffer + 1, &context->bufferLength, buffer + 2);
	context->transmitState = 7;
	context->bufferLength++;
	return 1;
}

/* name unknown: the streaming close (0x29) */
int swdlDownloadSendClose(SwdlContext *context)
{
	SceUInt8 *buffer = context->buffer;

	buffer[0] = 0x7e;
	buffer[1] = 0x29;
	context->bufferLength = 1;
	swdlEndFrame(buffer + 1, &context->bufferLength, buffer + 2);
	context->transmitState = 4;
	context->bufferLength++;
	return 0;
}

/* name unknown: the target's log text (0x0e), ended at the frame check sequence */
static inline void swdlReceiveLog(SwdlContext *context)
{
	context->receiveBuffer[context->receiveLength - 3] = 0;
	context->transmitState = 4;
}

/* name unknown: the open (0x26) and write (0x28) replies of the current image */
int swdlDownloadReceiveImage(SwdlContext *context)
{
	SwdlPortInterface *port;
	const SceUInt8 *cursor;
	int index = context->download.index;
	SceUInt8 code;
	SceUInt8 skip[2];
	SceUInt16 status;
	SceInt8 mode;
	SceUInt32 address;
	SceSize size;
	int raw;
	int result;

	switch (context->phase) {
	case 1:
		raw = 1;
		port = context->port;
		switch (context->receiveEvent) {
		case 0x200:
			cursor = context->receiveBuffer + 1;
			if (context->receiveBuffer[0] != 0x26) {
				if (context->receiveBuffer[0] != 0x0e)
					return (int)0x80330708;
				swdlReceiveLog(context);
				return 0;
			}
			swdlUnpackBytes((SceUInt8 *)&status, &cursor, 2);
			swdlUnpackBytes((SceUInt8 *)&mode, &cursor, 1);
			if (status != 0) {
				if (status != 7)
					return (int)0x80330708;
				context->transmitState = 1;
				context->phase = 3;
				return 0;
			}
			if (mode != 1)
				return (int)0x80330708;
			result = swdlReserveFrameBuffer(context, 0x2000);
			if (result < 0)
				return result;
			result = port->control(port, 6, &raw, sizeof(raw));
			if (result < 0)
				return result;
			swdlUnpackBytes((SceUInt8 *)&context->windowSize, &cursor, 4);
			context->fileOffset = 0;
			context->imageWritten = 0;
			context->sequence = 0;
			size = context->windowSize;
			if (size > context->imageSize)
				size = context->imageSize;
			context->transmitState = 1;
			context->windowRemaining = size;
			context->windowSize = size;
			context->phase = 2;
			return result;
		case 0x400:
			context->transmitState = 4;
			return 0;
		case 0x800:
			return swdlGetPortError(context->port);
		default:
			return (int)0x80330002;
		}
	case 2:
		port = context->port;
		size = context->download.images[index].size;
		switch (context->receiveEvent) {
		case 0x200:
			cursor = context->receiveBuffer;
			swdlUnpackBytes(&code, &cursor, 1);
			swdlUnpackBytes(skip, &cursor, 2);
			swdlUnpackBytes((SceUInt8 *)&address, &cursor, 4);
			swdlUnpackBytes((SceUInt8 *)&status, &cursor, 2);
			if (code != 0x28) {
				if (code != 0x0e)
					return (int)0x80330708;
				swdlReceiveLog(context);
				return 0;
			}
			if (status != 0)
				return (int)0x80330708;
			context->windowRemaining = context->windowSize;
			if (size > context->imageWritten) {
				raw = 1;
				result = port->control(port, 6, &raw, sizeof(raw));
				if (result >= 0)
					context->transmitState = 2;
				return result;
			}
			context->notification.forwardedValue1 = context->progressBytes;
			context->notify(64, &context->notification);
			context->progressBytes = 0;
			context->phase = 3;
			context->transmitState = 1;
			return 0;
		case 0x400:
			context->transmitState = 4;
			return 0;
		case 0x800:
			return swdlGetPortError(context->port);
		default:
			return (int)0x80330002;
		}
	default:
		return (int)0x80330002;
	}
}

/* name unknown: open the current image (0x25), then write it a window at a time:
 * each window starts with a write header (0x27), raw, without HDLC framing */
int swdlDownloadSendImage(SwdlContext *context)
{
	SwdlPortInterface *port;
	SwdlDownloadImage *image;
	SceUInt8 *buffer;
	SceUInt8 *data;
	int index = context->download.index;
	int count = context->download.count;
	SceUInt32 offset;
	SceSize remaining;
	SceSize window;
	SceSize capacity;
	SceSize size;
	SceUInt32 freePackets;
	int raw;
	int result;

	if (index >= count)
		return 1;
	image = &context->download.images[index];
	if (image->fd == 0)
		return (int)0x80330002;
	switch (context->phase) {
	case 1:
		buffer = context->buffer;
		size = image->size;
		buffer[2] = image->type;
		*(SceUInt32 *)(buffer + 3) = size;
		buffer[7] = 1;
		buffer[0] = 0x7e;
		buffer[1] = 0x25;
		buffer[8] = 0;
		buffer[9] = 0;
		buffer[10] = 0;
		buffer[12] = 0;
		buffer[13] = 0;
		buffer[11] = 4;
		context->imageSize = size;
		context->bufferLength = 13;
		swdlEndFrame(buffer + 1, &context->bufferLength, buffer + 14);
		context->transmitState = 4;
		context->bufferLength++;
		return 0;
	case 2:
		raw = 0;
		port = context->port;
		result = swdlGetPortFreePackets(context, &freePackets);
		if (result < 0)
			return result;
		if (freePackets == 0) {
			context->transmitState = 5;
			return result;
		}
		remaining = context->windowRemaining;
		window = context->windowSize;
		if (window == remaining) {
			capacity = (context->bufferCapacity - 13) & ~3;
			data = context->buffer + 13;
		} else {
			if (remaining == 0)
				goto end;
			data = context->buffer;
			capacity = context->bufferCapacity;
		}
		offset = context->fileOffset;
		size = capacity;
		if (remaining < capacity)
			size = remaining;
		result = swdlReadFileAt(image->fd, data, offset, &size);
		if (result < 0)
			return result;
		if (size == 0)
			goto end;
		context->bufferLength = size;
		if (window == remaining) {
			buffer = context->buffer;
			buffer[0] = 0x27;
			*(SceUInt16 *)(buffer + 1) = context->sequence;
			*(SceUInt32 *)(buffer + 3) = offset;
			*(SceUInt32 *)(buffer + 7) = window;
			*(SceUInt16 *)(buffer + 11) = swdlCalculateCrc(buffer, 11);
			context->sequence++;
			context->bufferLength += 13;
		}
		context->fileOffset += size;
		context->windowRemaining -= size;
		context->imageWritten += size;
		context->progressBytes += size;
		if (context->progressBytes > 0xc800) {
			context->notification.forwardedValue1 = context->progressBytes;
			context->progressBytes = 0;
			context->notify(64, &context->notification);
		}
		context->transmitState = 2;
		return result;
end:
		result = port->control(port, 6, &raw, sizeof(raw));
		if (result >= 0) {
			context->bufferLength = 0;
			context->transmitState = 4;
		}
		return result;
	case 3:
		context->download.index = ++index;
		return count > index ? 2 : 1;
	default:
		return (int)0x80330002;
	}
}

/* name unknown: the high speed hello (0x01) */
int swdlDownloadSendHello(SwdlContext *context)
{
	SceUInt8 *buffer = context->buffer;

	buffer[1] = 1;
	buffer[0] = 0x7e;
	memset(buffer + 2, 0, 32);
	strncpy((char *)buffer + 2, "QCOM high speed protocol hst", 32);
	buffer[36] = 0x30;
	buffer[34] = 6;
	buffer[35] = 6;
	buffer[37] = 0;
	buffer[38] = 0;
	buffer[39] = 0;
	context->bufferLength = 39;
	swdlEndFrame(buffer + 1, &context->bufferLength, buffer + 40);
	context->transmitState = 4;
	context->bufferLength++;
	return 0;
}

/* name unknown: the fast download hello (0x01) */
int swdlUpdateSendHello(SwdlContext *context)
{
	SceUInt8 *buffer = context->buffer;

	buffer[0] = 0x7e;
	buffer[1] = 1;
	__builtin_memcpy(buffer + 2, "QCOM fast download protocol host", 32);
	buffer[34] = 3;
	buffer[35] = 2;
	buffer[36] = 8;
	buffer[37] = 0;
	buffer[38] = 0;
	buffer[39] = 0;
	context->bufferLength = 39;
	swdlEndFrame(buffer + 1, &context->bufferLength, buffer + 40);
	context->transmitState = 4;
	context->bufferLength++;
	return 0;
}

/* name unknown: the close reply (0x2a) */
int swdlDownloadReceiveClose(SwdlContext *context)
{
	const SceUInt8 *cursor;
	SceUInt16 status;
	SceUInt8 error;

	switch (context->receiveEvent) {
	case 0x400:
		context->transmitState = 4;
		return 0;
	case 0x800:
		return swdlGetPortError(context->port);
	case 0x200:
		cursor = context->receiveBuffer + 1;
		if (context->receiveBuffer[0] == 0x2a) {
			swdlUnpackBytes((SceUInt8 *)&status, &cursor, 2);
			if (status == 0)
				return 1;
			swdlUnpackBytes(&error, &cursor, 1);
			return (int)0x80330708;
		}
		if (context->receiveBuffer[0] != 0x0e)
			return (int)0x80330708;
		swdlReceiveLog(context);
		return 0;
	default:
		return (int)0x80330002;
	}
}

/* name unknown: the high speed hello reply (0x02): the protocol, and versions 6 */
int swdlDownloadReceiveHello(SwdlContext *context)
{
	const SceUInt8 *cursor;
	char magic[24];
	SceUInt32 reserved;
	SceUInt16 version[2];
	SceUInt8 level[2];
	SceUInt32 features;
	SceUInt8 skip;
	SceUInt16 count;
	SceSize left;

	switch (context->receiveEvent) {
	case 0x200:
		if (context->receiveBuffer[0] == 0x02) {
			cursor = context->receiveBuffer + 1;
			__builtin_memset(magic, 0, sizeof(magic));
			reserved = 0;
			__builtin_memset(version, 0, sizeof(version));
			__builtin_memset(level, 0, sizeof(level));
			features = 0;
			swdlUnpackBytes((SceUInt8 *)magic, &cursor, 24);
			swdlUnpackBytes((SceUInt8 *)&reserved, &cursor, 4);
			swdlUnpackBytes((SceUInt8 *)&version[0], &cursor, 2);
			swdlUnpackBytes((SceUInt8 *)&version[1], &cursor, 2);
			swdlUnpackBytes(&level[0], &cursor, 1);
			swdlUnpackBytes(&level[1], &cursor, 1);
			swdlUnpackBytes(NULL, &cursor, 4);
			swdlUnpackBytes(NULL, &cursor, 4);
			swdlUnpackBytes(&skip, &cursor, 1);
			swdlUnpackBytes(NULL, &cursor, skip);
			swdlUnpackBytes(NULL, &cursor, 2);
			swdlUnpackBytes((SceUInt8 *)&count, &cursor, 2);
			swdlUnpackBytes(NULL, &cursor, 4 * count);
			if (context->receiveLength < (SceSize)(cursor - context->receiveBuffer) + 2)
				return (int)0x80330002;
			left = context->receiveLength - 2 - (cursor - context->receiveBuffer);
			if (left <= 4)
				swdlUnpackBytes((SceUInt8 *)&features, &cursor, left);
			if (memcmp(magic, "QCOM high speed protocol", 24) == 0 && level[0] > 5 &&
			    level[1] > 5) {
				if ((features & 0x30) == 0x30)
					return 1;
				return (int)0x80330708;
			}
		} else if (context->receiveBuffer[0] == 0x0e) {
			swdlReceiveLog(context);
			return 0;
		}
		return (int)0x80330708;
	case 0x400:
		context->transmitState = 4;
		return 0;
	case 0x800:
		return swdlGetPortError(context->port);
	default:
		return (int)0x80330002;
	}
}

/* name unknown: the fast download hello reply (0x02) */
typedef struct SwdlFastHello {
	char magic[32];
	SceUInt8 version;
	SceUInt8 compatibleVersion;
	SwdlHelloWindow window;
	SceUInt16 windowSize;
	SceUInt16 sectorCount;
	SceUInt32 sectorSizes[16];
} SwdlFastHello;

int swdlUpdateReceiveHello(SwdlContext *context)
{
	const SceUInt8 *cursor;
	const SceUInt8 *start;
	SwdlFastHello hello;
	SceUInt8 skip;
	SceUInt8 trailer[4];
	SceSize packetSize;
	SceSize size;
	SceSize left;
	int result;
	int i;

	switch (context->receiveEvent) {
	case 0x200:
		if (context->receiveBuffer[0] != 0x02) {
			if (context->receiveBuffer[0] != 0x0e)
				return (int)0x80330708;
			swdlReceiveLog(context);
			return 0;
		}
		start = context->receiveBuffer + 1;
		cursor = start;
		swdlUnpackBytes((SceUInt8 *)hello.magic, &cursor, 32);
		swdlUnpackBytes(&hello.version, &cursor, 1);
		swdlUnpackBytes(&hello.compatibleVersion, &cursor, 1);
		swdlUnpackBytes((SceUInt8 *)&hello.window.blockSize, &cursor, 4);
		swdlUnpackBytes((SceUInt8 *)&hello.window.baseAddress, &cursor, 4);
		swdlUnpackBytes(&skip, &cursor, 1);
		swdlUnpackBytes(NULL, &cursor, skip);
		swdlUnpackBytes((SceUInt8 *)&hello.windowSize, &cursor, 2);
		swdlUnpackBytes((SceUInt8 *)&hello.sectorCount, &cursor, 2);
		for (i = 0; i < hello.sectorCount; i++)
			swdlUnpackBytes(i > 15 ? NULL : (SceUInt8 *)&hello.sectorSizes[i], &cursor, 4);
		if (context->receiveLength < (SceSize)(cursor - start) - 2)
			return (int)0x80330002;
		left = context->receiveLength - 2 - (cursor - start);
		if (left <= 4)
			swdlUnpackBytes(trailer, &cursor, left);
		result = swdlGetPortPacketSize(context, &packetSize);
		if (result < 0)
			return result;
		size = hello.window.blockSize + 9;
		if (size > packetSize) {
			hello.window.blockSize = (packetSize - 9) & ~3;
			size = hello.window.blockSize + 9;
		}
		result = swdlReserveFrameBuffer(context, size > 517 ? size : 518);
		if (result == 0) {
			context->hello.window = hello.window;
			context->hello.windowSize = hello.windowSize;
			result = 1;
		}
		return result;
	case 0x400:
		context->transmitState = 4;
		return 0;
	case 0x800:
		return swdlGetPortError(context->port);
	default:
		return (int)0x80330002;
	}
}

/* name unknown: the streaming download of the requested images */
static const SwdlStep swdlDownloadSteps[7] = {
	{ swdlDownloadSendHello, swdlDownloadReceiveHello },
	{ swdlDownloadStartImages, swdlDownloadSkipReply },
	{ swdlDownloadSendImage, swdlDownloadReceiveImage },
	{ swdlDownloadSendClose, swdlDownloadReceiveClose },
	{ swdlDownloadSendReset, swdlDownloadSkipReply },
	{ swdlDownloadWaitAttach, swdlDownloadReceiveAttach },
	{ swdlDownloadEndSend, swdlDownloadEndReceive },
};

/* the download is over: close the port */
static inline void swdlFinishDownload(SwdlContext *context)
{
	context->port->close(context->port, 0);
	swdlReleaseBuffers(context);
	context->notify(4, &context->notification);
	context->state = 0;
}

/* name unknown: state 3, the streaming download */
int swdlExecDownload(SwdlContext *context, SceUInt32 events)
{
	SwdlPortInterface *port = context->port;
	int result = events & 8;

	if (events & 8) {
		memset(&context->stepIndex, 0, 0x468);
		__builtin_memset(&context->hello, 0, sizeof(context->hello));
		context->steps = swdlDownloadSteps;
		context->progressBytes = 0;
		context->download.index = 0;
		result = swdlAllocateFrameBuffer(context);
		if (result < 0)
			goto error;
		result = swdlSetResponseTimeout(context, 3000);
		if (result < 0)
			goto error;
		result = port->open(port, 3);
		if (result < 0)
			goto error;
		result = swdlStartSend(context);
		if (result < 0)
			goto error;
		if (result == 3)
			swdlFinishDownload(context);
	}
	if (events & 0x200) {
		result = swdlReceiveFrame(context, NULL);
		if (result < 0)
			goto error;
		if (result == 3)
			goto finish;
		result = swdlSendFrames(context);
		if (result < 0)
			goto error;
		if (result == 3)
finish:
			swdlFinishDownload(context);
	}
	if (events & 0x100) {
		result = swdlSendFrames(context);
		if (result == 3)
			swdlFinishDownload(context);
	}
	if (events & 0x1800) {
		result = swdlHandleCancel(context);
		if (result < 0)
			goto error;
		result = swdlSendFrames(context);
		if (result < 0)
			goto error;
		if (result == 3)
			swdlFinishDownload(context);
	}
	if (events & 0x400) {
		result = swdlHandleFrameTimeout(context);
		if (result < 0)
			goto error;
		result = swdlSendFrames(context);
		if (result < 0)
			goto error;
		if (result == 3)
			swdlFinishDownload(context);
	}
	if (events & 0x20000) {
		result = swdlHandleAttach(context);
		if (result < 0)
			goto error;
		result = swdlSendFrames(context);
		if (result < 0)
			goto error;
		if (result == 3)
			swdlFinishDownload(context);
	}
	if (events & 0x100000) {
		result = context->lastError;
error:
		port->close(port, 0);
		swdlReleaseBuffers(context);
		context->notification.forwardedValue2 = result;
		context->notify(128, &context->notification);
	}
	return result;
}

/* name unknown: the reply to the queued command must be the one queued with it; then
 * the next command goes */
int swdlModeChangeReceiveCommand(SwdlContext *context)
{
	SwdlListNode *node = context->list;
	void *extra;

	if (node == NULL)
		return (int)0x80330002;
	switch (context->receiveEvent) {
	case 0x200:
		extra = node->extra;
		if (memcmp(context->receiveBuffer, extra, node->extraSize) != 0)
			return (int)0x80330708;
		context->listCount--;
		context->list = node->next;
		if (node->data != NULL) {
			sceKernelFreeHeapMemory(context->heapId, node->data);
			extra = node->extra;
		}
		if (extra != NULL)
			sceKernelFreeHeapMemory(context->heapId, extra);
		sceKernelFreeHeapMemory(context->heapId, node);
		context->transmitState = 1;
		return 0;
	case 0x400:
		context->transmitState = 4;
		return 0;
	case 0x800:
		return swdlGetPortError(context->port);
	default:
		return (int)0x80330002;
	}
}

/* name unknown: send the first queued command */
int swdlModeChangeSendCommand(SwdlContext *context)
{
	SwdlListNode *node = context->list;

	if (node == NULL)
		return 1;
	memcpy(context->buffer, node->data, node->dataSize);
	context->bufferLength = node->dataSize;
	swdlEndFrame(context->buffer, &context->bufferLength, context->buffer + node->dataSize);
	context->transmitState = 4;
	return 0;
}

/* name unknown: the mode change: the status, then the queued commands */
static const SwdlStep swdlModeChangeSteps[3] = {
	{ swdlModeChangeSendStatus, swdlModeChangeReceiveStatus },
	{ swdlModeChangeSendCommand, swdlModeChangeReceiveCommand },
	{ swdlModeChangeEndSend, swdlModeChangeEndReceive },
};

/* the mode change is over: close the port */
static inline void swdlFinishModeChange(SwdlContext *context)
{
	context->port->close(context->port, 0);
	swdlReleaseBuffers(context);
	context->notify(0x4000, &context->notification);
	context->state = 0;
}

/* name unknown: state 5, the mode change */
int swdlExecModeChange(SwdlContext *context, SceUInt32 events)
{
	SwdlPortInterface *port = context->port;
	int result = events & 0x800000;

	if (events & 0x800000) {
		memset(&context->stepIndex, 0, 0x468);
		context->mode = 2;
		context->steps = swdlModeChangeSteps;
		result = swdlAllocateFrameBuffer(context);
		if (result < 0)
			goto error;
		result = swdlSetResponseTimeout(context, 3000);
		if (result < 0)
			goto error;
		result = port->open(port, 1);
		if (result < 0)
			goto error;
		result = swdlStartSend(context);
		if (result < 0)
			goto error;
		if (result == 3)
			swdlFinishModeChange(context);
	}
	if (events & 0x200) {
		result = swdlReceiveFrame(context, NULL);
		if (result < 0)
			goto error;
		result = swdlSendFrames(context);
		if (result < 0)
			goto error;
		if (result == 3)
			swdlFinishModeChange(context);
	}
	if (events & 0x1800) {
		result = swdlHandleCancel(context);
		if (result < 0)
			goto error;
		result = swdlSendFrames(context);
		if (result < 0)
			goto error;
		if (result == 3)
			swdlFinishModeChange(context);
	}
	if (events & 0x400) {
		result = swdlHandleFrameTimeout(context);
		if (result < 0)
			goto error;
		result = swdlSendFrames(context);
		if (result < 0)
			goto error;
		if (result == 3)
			swdlFinishModeChange(context);
	}
	if (events & 0x100000) {
		result = context->lastError;
error:
		port->close(port, 0);
		swdlReleaseBuffers(context);
		context->notification.forwardedValue2 = result;
		context->notify(128, &context->notification);
	}
	return result;
}

/* name unknown: state 0, nothing in work: a device that attaches is checked (port
 * control 2), a stop closes the port */
int swdlExecIdle(SwdlContext *context, SceUInt32 events)
{
	SwdlPortInterface *port = context->port;

	if (events & 0x80) {
		if (port->control(port, 2, NULL, 0) < 0)
			sceKernelSetEventFlag(context->eventFlagId, 0x2000);
	}
	if (events & 0x2000) {
		port->close(port, 0);
		swdlReleaseBuffers(context);
		context->notify(16, &context->notification);
	}
	if (events & 0x100000) {
		swdlReleaseBuffers(context);
		context->notification.forwardedValue2 = context->lastError;
		context->notify(128, &context->notification);
	}
	return 0;
}

/* name unknown: move stepIndex to the step that sends or receives with handler; 0 when
 * the sequence ends first */
int swdlSeekStep(SwdlContext *context, int (*handler)(SwdlContext *context))
{
	SceUInt32 index = context->stepIndex;
	const SwdlStep *step = &context->steps[index];
	int (*send)(SwdlContext *context) = step->send;
	int (*receive)(SwdlContext *context) = step->receive;

	if (send == swdlUpdateEndSend)
		return 0;
	if (send == handler || receive == handler)
		return 1;
	do {
		index++;
		send = step[1].send;
		receive = step[1].receive;
		step++;
		if (send == swdlUpdateEndSend) {
			context->stepIndex = index;
			return 0;
		}
	} while (handler != send && handler != receive);
	context->stepIndex = index;
	return 1;
}

/* name unknown: after the go the device leaves (detach) and comes back in streaming
 * download mode (attach) */
int swdlUpdateReceiveRestart(SwdlContext *context)
{
	SwdlPortInterface *port = context->port;
	int result;

	switch (context->receiveEvent) {
	case 0x200:
		if (context->receiveBuffer[0] == 0x02) {
			result = port->stopReceive(port, 0);
			if (result < 0)
				return result;
		} else {
			result = (int)0x80330708;
		}
		context->acknowledged = 1;
		return result;
	case 0x400:
		if (context->acknowledged == 1)
			return 0;
		return (int)0x80330706;
	case 0x800:
		return 0;
	case 0x10000:
		result = port->open(port, 2);
		if (result < 0)
			return result;
		context->updateState = 3;
		return 1;
	case 0x20000:
		if (context->acknowledged != 0) {
			result = 0;
		} else {
			result = port->stopReceive(port, 0);
			if (result < 0)
				return result;
		}
		context->acknowledged = 1;
		return result;
	default:
		return (int)0x80330708;
	}
}

/* name unknown: the update is over: close the port and the image, report how it ended */
int swdlFinishUpdate(SwdlContext *context)
{
	int result;

	context->port->close(context->port, 0);
	swdlReleaseBuffers(context);
	result = swdlCloseHexReader(context);
	if (context->abortState != 0) {
		result = context->notify(16, &context->notification);
	} else {
		switch (context->updateKind) {
		case 0:
			result = context->notify(2, &context->notification);
			break;
		case 1:
			result = context->notify(0x1000, &context->notification);
			break;
		case 2:
			result = context->notify(0x2000, &context->notification);
			break;
		}
	}
	context->state = 0;
	context->updateState = 0;
	context->abortState = 0;
	return result;
}

/* name unknown: the streaming reset (0x0b), or the power down (0x0a) before the image
 * download started */
int swdlUpdateSendReset(SwdlContext *context)
{
	SceUInt8 *buffer = context->buffer;
	SceUInt8 command;

	if (context->updateState > 1)
		command = 0x0b;
	else
		command = 0x0a;
	buffer[0] = 0x7e;
	buffer[1] = command;
	context->bufferLength = 1;
	swdlEndFrame(buffer + 1, &context->bufferLength, buffer + 2);
	context->transmitState = 4;
	context->bufferLength++;
	context->updateState = 6;
	return 0;
}

/* name unknown: the streaming security mode (0x17), trusted */
int swdlUpdateSendSecurityMode(SwdlContext *context)
{
	SceUInt8 *buffer = context->buffer;

	buffer[0] = 0x7e;
	buffer[1] = 0x17;
	buffer[2] = 1;
	context->bufferLength = 2;
	swdlEndFrame(buffer + 1, &context->bufferLength, buffer + 3);
	context->transmitState = 4;
	context->updateState = 4;
	context->bufferLength++;
	return 0;
}

/* name unknown: the DMSS go (0x05) to the boot loader's start address */
int swdlUpdateSendGo(SwdlContext *context)
{
	SceUInt8 *buffer;

	if (context->hexReader.hasStartAddress == 0)
		return (int)0x80330002;
	buffer = context->buffer;
	buffer[0] = 0x7e;
	buffer[1] = 0x05;
	buffer[2] = context->hexReader.startAddress >> 24;
	buffer[3] = context->hexReader.startAddress >> 16;
	buffer[4] = context->hexReader.startAddress >> 8;
	buffer[5] = context->hexReader.startAddress;
	context->bufferLength = 5;
	swdlEndFrame(buffer + 1, &context->bufferLength, buffer + 6);
	context->transmitState = 4;
	context->acknowledged = 0;
	context->bufferLength++;
	context->updateState = 2;
	return 0;
}

/* name unknown: the DMSS parameter request (0x07) */
int swdlUpdateSendParameterRequest(SwdlContext *context)
{
	SceUInt8 *buffer = context->buffer;

	buffer[0] = 0x7e;
	buffer[1] = 0x07;
	context->bufferLength = 1;
	swdlEndFrame(buffer + 1, &context->bufferLength, buffer + 2);
	context->transmitState = 4;
	context->bufferLength++;
	return 0;
}

/* name unknown: the DMSS no-operation (0x06) */
int swdlUpdateSendNop(SwdlContext *context)
{
	SceUInt8 *buffer = context->buffer;

	buffer[0] = 0x7e;
	buffer[1] = 0x06;
	context->bufferLength = 1;
	swdlEndFrame(buffer + 1, &context->bufferLength, buffer + 2);
	context->transmitState = 4;
	context->bufferLength++;
	return 0;
}

/* name unknown: the boot loader download (DMSS), the target's flash and its partition
 * table, then the three images (streaming download) */
static const SwdlStep swdlUpdateSteps[17];
/* name unknown: the same with the module's own partition table, without the images */
static const SwdlStep swdlCarrierUpdateSteps[11];
/* name unknown: the same with the partition table of the request, without the images */
static const SwdlStep swdlPartitionTableSteps[10];

/* name unknown: state 2's start: the sequence of the update kind */
int swdlStartUpdate(SwdlContext *context)
{
	SwdlPortInterface *port = context->port;
	int result;

	memset(&context->stepIndex, 0, 0x468);
	context->updateState = 1;
	__builtin_memset(&context->hello, 0, sizeof(context->hello));
	switch (context->updateKind) {
	case 0:
		context->steps = swdlUpdateSteps;
		break;
	case 1:
		context->steps = swdlCarrierUpdateSteps;
		break;
	case 2:
		context->steps = swdlPartitionTableSteps;
		break;
	default:
		return (int)0x80330002;
	}
	result = swdlAllocateFrameBuffer(context);
	if (result < 0)
		return result;
	context->progressBytes = 0;
	result = swdlSetResponseTimeout(context, 3000);
	if (result < 0)
		return result;
	result = port->open(port, 2);
	if (result < 0)
		return result;
	context->updateState = 1;
	return swdlStartSend(context);
}

/* name unknown: the request's partition table (0x19), forced when the request or the
 * target asks for it */
int swdlUpdateSendPartitionTable(SwdlContext *context)
{
	SceUInt8 *buffer = context->buffer;
	int result;
	int size;

	memset(buffer, 0, 0x206);
	buffer[0] = 0x7e;
	buffer[1] = 0x19;
	buffer[2] = 0;
	if (context->update.forcePartitionTable == 1 ||
	    (context->hello.partitionTableStatus == 1 && context->update.partitionTableForced == 1))
		buffer[2] = 1;
	result = swdlSeekFile(context->update.partitionTableFd, 0);
	if (result != 0)
		return result;
	size = udIoRead(context->update.partitionTableFd, buffer + 3, 512);
	if (size < 0)
		return size;
	if (size == 0)
		return (int)0x80330703;
	context->progressBytes += size;
	context->bufferLength = size + 2;
	swdlEndFrame(buffer + 1, &context->bufferLength, buffer + size + 3);
	context->transmitState = 4;
	context->bufferLength++;
	return size;
}

/* name unknown: the partition table the module carries */
typedef struct SwdlPartitionEntry {
	char name[16];
	SceUInt32 start;
	SceUInt32 length;
	SceUInt8 attributes[4];
} SwdlPartitionEntry;

typedef struct SwdlPartitionTable {
	SceUInt32 magic[2];
	SceUInt32 version;
	SceUInt32 count;
	SwdlPartitionEntry entries[6];
	SceUInt8 padding[280];
} SwdlPartitionTable;

static const SwdlPartitionTable swdlDefaultPartitionTable = {
	.magic = { 0xaa7d1b9a, 0x1f7d48bc },
	.version = 3,
	.count = 6,
	.entries = {
		{ "0:MIBIB", 6, 4, { 0xff, 0xff, 0xff, 0xfe } },
		{ "0:SIM_SECURE", 4, 1, { 0xff, 0xff, 0xff, 0xfe } },
		{ "0:FSBL", 0x40, 0x40, { 0xff, 0xff, 0xff, 0xff } },
		{ "0:OSBL", 0x40, 0x40, { 0xff, 0xff, 0xff, 0xff } },
		{ "0:AMSS", 0x40, 0x40, { 0xff, 0xff, 0xff, 0xff } },
		{ "0:EFS2", 0xffffffff, 0xffff, { 0xff, 0xff, 0xff, 0xff } },
	},
};

_Static_assert(sizeof(SwdlPartitionTable) == 464, "swdl partition table");

/* name unknown: the module's partition table (0x19), forced */
int swdlUpdateSendDefaultPartitionTable(SwdlContext *context)
{
	SceUInt8 *buffer;

	if (context->update.forcePartitionTable == 0) {
		context->bufferLength = 0;
		return 1;
	}
	buffer = context->buffer;
	memset(buffer, 0, 0x206);
	context->hello.partitionTableStatus = 0;
	buffer[0] = 0x7e;
	buffer[1] = 0x19;
	buffer[2] = 1;
	memcpy(buffer + 3, &swdlDefaultPartitionTable, sizeof(swdlDefaultPartitionTable));
	context->progressBytes += sizeof(swdlDefaultPartitionTable);
	context->bufferLength = sizeof(swdlDefaultPartitionTable) + 2;
	swdlEndFrame(buffer + 1, &context->bufferLength, buffer + 467);
	context->transmitState = 4;
	context->bufferLength++;
	return 0;
}

/* name unknown: erase the flash (0x1d) before a forced partition table */
int swdlUpdateSendErase(SwdlContext *context)
{
	SceUInt8 *buffer;

	if (context->update.forcePartitionTable == 0) {
		context->bufferLength = 0;
		return 1;
	}
	buffer = context->buffer;
	buffer[2] = 0;
	buffer[3] = 0;
	buffer[4] = 0;
	buffer[0] = 0x7e;
	buffer[1] = 0x1d;
	context->bufferLength = 1;
	swdlEndFrame(buffer + 1, &context->bufferLength, buffer + 2);
	context->transmitState = 4;
	context->bufferLength++;
	return 0;
}

/* name unknown: the reset acknowledge (0x0c); a reset the abort sent may go unanswered */
int swdlUpdateReceiveReset(SwdlContext *context)
{
	SceUInt8 command;

	switch (context->receiveEvent) {
	case 0x200:
		command = context->receiveBuffer[0];
		if (command == 0x0c || command == 0x02)
			return 1;
		if (context->abortState != 7)
			return (int)0x80330708;
		context->transmitState = 4;
		return 0;
	case 0x400:
		context->transmitState = 4;
		return 0;
	case 0x800:
		return swdlGetPortError(context->port);
	default:
		return (int)0x80330002;
	}
}

/* the target's error (0x0d) or log (0x0e) text */
static inline int swdlIsErrorOrLog(SceUInt32 command)
{
	return command - 0x0d <= 1;
}

/* name unknown: the partition table reply (0x1a) to the module's table */
int swdlUpdateReceiveDefaultPartitionTable(SwdlContext *context)
{
	SceUInt32 command;
	SceUInt32 status;

	switch (context->receiveEvent) {
	case 0x200:
		command = context->receiveBuffer[0];
		if (command == 0x1a) {
			status = context->receiveBuffer[1];
			context->hello.partitionTableStatus = status;
			if (status == 0)
				return 1;
			if (status == 1)
				return (int)0x80330708;
			return (int)0x80330703;
		}
		if (!swdlIsErrorOrLog(command))
			return (int)0x80330708;
		swdlReceiveLog(context);
		return 0;
	case 0x400:
		context->transmitState = 4;
		return 0;
	case 0x800:
		return swdlGetPortError(context->port);
	default:
		return (int)0x80330002;
	}
}

/* name unknown: the erase reply (0x1e): the partition table goes forced */
int swdlUpdateReceiveErase(SwdlContext *context)
{
	SceUInt32 command;

	switch (context->receiveEvent) {
	case 0x200:
		command = context->receiveBuffer[0];
		if (command == 0x1e) {
			context->update.forcePartitionTable = 0;
			context->update.partitionTableForced = 1;
			return 1;
		}
		if (command == 0x0e) {
			swdlReceiveLog(context);
			return 0;
		}
		if (command == 0x0d)
			return 1;
		return (int)0x80330708;
	case 0x400:
		context->transmitState = 4;
		return 0;
	case 0x800:
		return swdlGetPortError(context->port);
	default:
		return (int)0x80330002;
	}
}

/* name unknown: the security mode reply (0x18) */
int swdlUpdateReceiveSecurityMode(SwdlContext *context)
{
	SceUInt32 command;

	switch (context->receiveEvent) {
	case 0x200:
		command = context->receiveBuffer[0];
		if (command == 0x18)
			return 1;
		if (command == 0x0e) {
			swdlReceiveLog(context);
			return 0;
		}
		return (int)0x80330708;
	case 0x400:
		context->transmitState = 4;
		return 0;
	case 0x800:
		return swdlGetPortError(context->port);
	default:
		return (int)0x80330002;
	}
}

/* name unknown: the DMSS acknowledge (0x02) of a HEX record: the next one goes */
int swdlUpdateReceiveWriteAck(SwdlContext *context)
{
	switch (context->receiveEvent) {
	case 0x200:
		if (context->receiveBuffer[0] != 0x02)
			return (int)0x80330708;
		context->transmitState = 1;
		context->progressBytes += context->hexReader.consumed;
		return 0;
	case 0x400:
		context->transmitState = 4;
		return 0;
	case 0x800:
		return swdlGetPortError(context->port);
	default:
		return (int)0x80330002;
	}
}

/* name unknown: the DMSS acknowledge (0x02) of the unlock and the no-operation */
int swdlUpdateReceiveUnlockAck(SwdlContext *context)
{
	switch (context->receiveEvent) {
	case 0x200:
		if (context->receiveBuffer[0] == 0x02)
			return 1;
		return (int)0x80330708;
	case 0x400:
		context->transmitState = 4;
		return 0;
	case 0x800:
		return swdlGetPortError(context->port);
	default:
		return (int)0x80330002;
	}
}

int swdlUpdateReceiveNopAck(SwdlContext *context)
{
	switch (context->receiveEvent) {
	case 0x200:
		if (context->receiveBuffer[0] == 0x02)
			return 1;
		return (int)0x80330708;
	case 0x400:
		context->transmitState = 4;
		return 0;
	case 0x800:
		return swdlGetPortError(context->port);
	default:
		return (int)0x80330002;
	}
}

/* name unknown: the reply to the request's partition table; a table that differs from
 * the target's is reported (32), and sent again forced when allowed */
int swdlUpdateReceivePartitionTable(SwdlContext *context)
{
	SceUInt32 command;
	SceUInt32 status;

	switch (context->receiveEvent) {
	case 0x200:
		command = context->receiveBuffer[0];
		if (command == 0x1a) {
			status = context->receiveBuffer[1];
			context->hello.partitionTableStatus = status;
			if (status == 0)
				return 1;
			if (status != 1)
				return (int)0x80330703;
			if (context->update.partitionTableForced != 0) {
				context->transmitState = 1;
				return 0;
			}
			context->notify(32, &context->notification);
			if (context->updateKind == 2)
				return 1;
			context->transmitState = 6;
			return 0;
		}
		if (!swdlIsErrorOrLog(command))
			return (int)0x80330708;
		swdlReceiveLog(context);
		return 0;
	case 0x400:
		context->transmitState = 4;
		return 0;
	case 0x800:
		return swdlGetPortError(context->port);
	default:
		return (int)0x80330002;
	}
}

/* name unknown: the next record of the boot loader as a DMSS 32-bit write (0x0f) */
int swdlUpdateSendHexRecord(SwdlContext *context)
{
	SwdlHexReader *reader = &context->hexReader;
	SceUInt8 *buffer;
	SceSize length;
	int result;

	result = swdlReadHexRecords(reader);
	if (result != 0)
		return result;
	if (reader->endOfFile == 1)
		return 1;
	buffer = context->buffer;
	buffer[0] = 0x7e;
	buffer[1] = 0x0f;
	buffer[2] = reader->upperAddress >> 8;
	buffer[3] = reader->upperAddress;
	buffer[4] = reader->address >> 8;
	buffer[5] = reader->address;
	buffer[6] = reader->length >> 8;
	buffer[7] = reader->length;
	length = reader->length + 7;
	context->bufferLength = length;
	swdlEndFrame(buffer + 1, &context->bufferLength, buffer + length + 1);
	context->transmitState = 4;
	context->bufferLength++;
	return 0;
}

/* name unknown: the DMSS unlock (0x0b) with the erased slot's code */
int swdlUpdateSendUnlock(SwdlContext *context)
{
	const void *code;
	SceSize size;
	SceUInt8 *buffer;

	if (context->update.unlock == 0)
		return 1;
	swdlGetErasedVersion(&code, &size);
	buffer = context->buffer;
	buffer[1] = 0x0b;
	buffer[0] = 0x7e;
	memcpy(buffer + 2, code, size);
	context->bufferLength = 9;
	swdlEndFrame(buffer + 1, &context->bufferLength, buffer + 10);
	context->transmitState = 4;
	context->bufferLength++;
	return 0;
}

/* name unknown: the DMSS parameters (0x08): the HEX records are read as large as the
 * target and the port take */
int swdlUpdateReceiveParameters(SwdlContext *context)
{
	SceSize packetSize;
	SceSize size;
	int result;

	switch (context->receiveEvent) {
	case 0x400:
		context->transmitState = 4;
		return 0;
	case 0x800:
		return swdlGetPortError(context->port);
	case 0x200:
		if (context->receiveBuffer[0] != 0x08)
			return (int)0x80330708;
		size = context->receiveBuffer[4] | context->receiveBuffer[3] << 8;
		result = swdlGetPortPacketSize(context, &packetSize);
		if (result < 0)
			return result;
		if (size + 4 > packetSize)
			size = (packetSize - 4) & ~3;
		result = swdlOpenHexReader(context, size);
		if (result < 0)
			return result;
		return 1;
	default:
		return (int)0x80330002;
	}
}

/* name unknown: open the image (0x1b), write it (0x07) as far as the window allows, close
 * it (0x15) */
static inline int swdlUpdateSendImage(SwdlContext *context, SceUID fd, SceUInt8 type)
{
	SceUInt8 *buffer;
	SceUInt32 freePackets;
	SceUInt32 address;
	SceSize size;
	int result;

	if (fd == 0)
		return 1;
	switch (context->phase) {
	case 2:
		context->streaming = 1;
		result = context->windowCredits;
		if (result == 0)
			goto wait;
		result = swdlGetPortFreePackets(context, &freePackets);
		if (result < 0)
			return result;
		if (freePackets == 0) {
wait:
			context->transmitState = 5;
			return result;
		}
		buffer = context->buffer;
		buffer[0] = 0x7e;
		buffer[1] = 0x07;
		size = context->hello.window.blockSize;
		address = context->fileOffset;
		*(SceUInt32 *)(buffer + 2) = address;
		result = swdlReadFileAt(fd, buffer + 6, address - context->hello.window.baseAddress,
					&size);
		if (result < 0)
			return result;
		if (size == 0) {
			context->bufferLength = 0;
			context->transmitState = 4;
			return result;
		}
		context->windowCredits--;
		context->fileOffset += size;
		context->bufferLength = size + 5;
		swdlEndFrame(buffer + 1, &context->bufferLength, buffer + 1 + size + 5);
		context->bufferLength++;
		context->transmitState = context->streaming == 1 ? 2 : 4;
		return result;
	case 3:
		buffer = context->buffer;
		buffer[0] = 0x7e;
		buffer[1] = 0x15;
		context->bufferLength = 1;
		swdlEndFrame(buffer + 1, &context->bufferLength, buffer + 2);
		context->transmitState = 4;
		context->bufferLength++;
		return 0;
	case 1:
		buffer = context->buffer;
		buffer[0] = 0x7e;
		buffer[1] = 0x1b;
		buffer[2] = type;
		context->bufferLength = 2;
		swdlEndFrame(buffer + 1, &context->bufferLength, buffer + 3);
		context->transmitState = 4;
		context->updateState = 5;
		context->bufferLength++;
		return 0;
	default:
		return (int)0x80330002;
	}
}

int swdlUpdateSendImage1(SwdlContext *context)
{
	return swdlUpdateSendImage(context, context->update.imageFds[1], 0x10);
}

int swdlUpdateSendImage0(SwdlContext *context)
{
	return swdlUpdateSendImage(context, context->update.imageFds[0], 0x0f);
}

int swdlUpdateSendImage2(SwdlContext *context)
{
	return swdlUpdateSendImage(context, context->update.imageFds[2], 0x05);
}

/* name unknown: the open (0x1c), write (0x08) and close (0x16) replies of an image */
static inline int swdlUpdateReceiveImage(SwdlContext *context, const SceSize *imageSize)
{
	const SceUInt8 *cursor;
	SceUInt8 command;
	SceUInt8 status;
	SceUInt8 address[4];
	SceSize size;
	SceSize written;
	SceSize progress;

	switch (context->phase) {
	case 1:
		switch (context->receiveEvent) {
		case 0x200:
			command = context->receiveBuffer[0];
			status = context->receiveBuffer[1];
			if (command == 0x1c) {
				if (status != 0)
					return (int)0x80330708;
				context->imageWritten = 0;
				context->transmitState = 1;
				context->windowCredits = context->hello.windowSize;
				context->fileOffset = context->hello.window.baseAddress;
				context->phase = 2;
				return 0;
			}
			if (command != 0x0e)
				return (int)0x80330708;
			swdlReceiveLog(context);
			return 0;
		case 0x400:
			context->transmitState = 4;
			return 0;
		case 0x800:
			return swdlGetPortError(context->port);
		default:
			return (int)0x80330002;
		}
	case 2:
		size = *imageSize;
		switch (context->receiveEvent) {
		case 0x200:
			cursor = context->receiveBuffer;
			swdlUnpackBytes(&command, &cursor, 1);
			swdlUnpackBytes(address, &cursor, 4);
			if (command != 0x08) {
				if (command != 0x0e)
					return (int)0x80330708;
				swdlReceiveLog(context);
				return 0;
			}
			written = context->imageWritten + context->hello.window.blockSize;
			context->imageWritten = written;
			context->windowCredits++;
			if (size <= written) {
				progress = context->progressBytes;
				context->phase = 3;
				context->transmitState = 1;
				context->streaming = 0;
				context->progressBytes = 0;
				context->notification.forwardedValue1 = progress + written - size;
				context->notify(64, &context->notification);
				return 0;
			}
			context->progressBytes += context->hello.window.blockSize;
			if (context->transmitState != 3) {
				if (context->streaming == 1)
					context->transmitState = 2;
				else
					context->transmitState = 1;
			}
			return 0;
		case 0x400:
			context->transmitState = 4;
			return 0;
		case 0x800:
			return swdlGetPortError(context->port);
		default:
			return (int)0x80330002;
		}
	case 3:
		switch (context->receiveEvent) {
		case 0x200:
			if (context->receiveBuffer[0] == 0x16)
				return 1;
			if (context->receiveBuffer[0] != 0x0e)
				return (int)0x80330708;
			swdlReceiveLog(context);
			return 0;
		case 0x400:
			context->transmitState = 4;
			return 0;
		case 0x800:
			return swdlGetPortError(context->port);
		default:
			return (int)0x80330002;
		}
	default:
		return (int)0x80330002;
	}
}

int swdlUpdateReceiveImage0(SwdlContext *context)
{
	return swdlUpdateReceiveImage(context, &context->update.imageSizes[0]);
}

int swdlUpdateReceiveImage2(SwdlContext *context)
{
	return swdlUpdateReceiveImage(context, &context->update.imageSizes[2]);
}

int swdlUpdateReceiveImage1(SwdlContext *context)
{
	return swdlUpdateReceiveImage(context, &context->update.imageSizes[1]);
}

static const SwdlStep swdlUpdateSteps[17] = {
	{ swdlUpdateSendNop, swdlUpdateReceiveNopAck },
	{ swdlUpdateSendParameterRequest, swdlUpdateReceiveParameters },
	{ swdlUpdateSendUnlock, swdlUpdateReceiveUnlockAck },
	{ swdlUpdateSendHexRecord, swdlUpdateReceiveWriteAck },
	{ swdlUpdateSendGo, swdlUpdateReceiveRestart },
	{ swdlUpdateSendHello, swdlUpdateReceiveHello },
	{ swdlUpdateSendSecurityMode, swdlUpdateReceiveSecurityMode },
	{ swdlUpdateSendErase, swdlUpdateReceiveErase },
	{ swdlUpdateSendPartitionTable, swdlUpdateReceivePartitionTable },
	{ swdlUpdateStartImages, swdlUpdateSkipReply },
	{ swdlUpdateSendImage0, swdlUpdateReceiveImage0 },
	{ swdlUpdateStartImages, swdlUpdateSkipReply },
	{ swdlUpdateSendImage1, swdlUpdateReceiveImage1 },
	{ swdlUpdateStartImages, swdlUpdateSkipReply },
	{ swdlUpdateSendImage2, swdlUpdateReceiveImage2 },
	{ swdlUpdateSendReset, swdlUpdateReceiveReset },
	{ swdlUpdateEndSend, swdlUpdateEndReceive },
};

static const SwdlStep swdlCarrierUpdateSteps[11] = {
	{ swdlUpdateSendNop, swdlUpdateReceiveNopAck },
	{ swdlUpdateSendParameterRequest, swdlUpdateReceiveParameters },
	{ swdlUpdateSendUnlock, swdlUpdateReceiveUnlockAck },
	{ swdlUpdateSendHexRecord, swdlUpdateReceiveWriteAck },
	{ swdlUpdateSendGo, swdlUpdateReceiveRestart },
	{ swdlUpdateSendHello, swdlUpdateReceiveHello },
	{ swdlUpdateSendSecurityMode, swdlUpdateReceiveSecurityMode },
	{ swdlUpdateSendErase, swdlUpdateReceiveErase },
	{ swdlUpdateSendDefaultPartitionTable, swdlUpdateReceiveDefaultPartitionTable },
	{ swdlUpdateSendReset, swdlUpdateReceiveReset },
	{ swdlUpdateEndSend, swdlUpdateEndReceive },
};

static const SwdlStep swdlPartitionTableSteps[10] = {
	{ swdlUpdateSendNop, swdlUpdateReceiveNopAck },
	{ swdlUpdateSendParameterRequest, swdlUpdateReceiveParameters },
	{ swdlUpdateSendUnlock, swdlUpdateReceiveUnlockAck },
	{ swdlUpdateSendHexRecord, swdlUpdateReceiveWriteAck },
	{ swdlUpdateSendGo, swdlUpdateReceiveRestart },
	{ swdlUpdateSendHello, swdlUpdateReceiveHello },
	{ swdlUpdateSendSecurityMode, swdlUpdateReceiveSecurityMode },
	{ swdlUpdateSendPartitionTable, swdlUpdateReceivePartitionTable },
	{ swdlUpdateSendReset, swdlUpdateReceiveReset },
	{ swdlUpdateEndSend, swdlUpdateEndReceive },
};

#if FW_DEBUG
static const SceKernelDebugInfo dbg__abortExec_L2139
	__attribute__((section(".sce_dbginfo"), used)) = {
	.file = "sw_dload/sw_dload_s_nand.c",
	.line = 2139,
	.func = "_abortExec",
};

static const SceKernelDebugInfoSource dbg__abortExec_L2139Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg__abortExec_L2139, "sw_dload/sw_dload_s_nand.c", 2139,
	"_abortExec",
};

static const SceKernelDebugInfo dbg__abortExec_L2156
	__attribute__((section(".sce_dbginfo"), used)) = {
	.file = "sw_dload/sw_dload_s_nand.c",
	.line = 2156,
	.func = "_abortExec",
};

static const SceKernelDebugInfoSource dbg__abortExec_L2156Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg__abortExec_L2156, "sw_dload/sw_dload_s_nand.c", 2156,
	"_abortExec",
};

static const SceKernelDebugInfo dbg__abortExec_L2212
	__attribute__((section(".sce_dbginfo"), used)) = {
	.file = "sw_dload/sw_dload_s_nand.c",
	.line = 2212,
	.func = "_abortExec",
};

static const SceKernelDebugInfoSource dbg__abortExec_L2212Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg__abortExec_L2212, "sw_dload/sw_dload_s_nand.c", 2212,
	"_abortExec",
};

static const SceKernelDebugInfo dbg__abortExec_L2260
	__attribute__((section(".sce_dbginfo"), used)) = {
	.file = "sw_dload/sw_dload_s_nand.c",
	.line = 2260,
	.func = "_abortExec",
};

static const SceKernelDebugInfoSource dbg__abortExec_L2260Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg__abortExec_L2260, "sw_dload/sw_dload_s_nand.c", 2260,
	"_abortExec",
};

static const SceKernelDebugInfo dbg__abortExec_L2341
	__attribute__((section(".sce_dbginfo"), used)) = {
	.file = "sw_dload/sw_dload_s_nand.c",
	.line = 2341,
	.func = "_abortExec",
};

static const SceKernelDebugInfoSource dbg__abortExec_L2341Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg__abortExec_L2341, "sw_dload/sw_dload_s_nand.c", 2341,
	"_abortExec",
};

static const SceKernelDebugInfo dbg__abortExec_L2404
	__attribute__((section(".sce_dbginfo"), used)) = {
	.file = "sw_dload/sw_dload_s_nand.c",
	.line = 2404,
	.func = "_abortExec",
};

static const SceKernelDebugInfoSource dbg__abortExec_L2404Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg__abortExec_L2404, "sw_dload/sw_dload_s_nand.c", 2404,
	"_abortExec",
};
#endif

/* state 2 stopped (event 0x80): end the update where it stands, by the step it is in
 * (abortState: 2 restart the boot loader, 3 wait for the streaming mode, 4 wait for
 * the security mode reply, 5 for the image's close, 6 reset, 7 wait for the reset's
 * reply) */
static inline int _abortExec(SwdlContext *context, SceUInt32 events)
{
	SwdlPortInterface *port = context->port;
	SceUInt32 state;
	int received;
	int result = 0;

	if (context->abortState == 1) {
		state = context->updateState;
		if (state == 0)
			context->abortState = 2;
		else if (state == 1 || state == 4)
			context->abortState = 6;
		else if (state == 2)
			context->abortState = 3;
		else if (state == 3)
			context->abortState = 4;
		else if (state == 5)
			context->abortState = 5;
		else if (state == 6)
			context->abortState = 7;
		else
			return (int)0x80330002;
	}
	state = context->abortState;
	if (state == 2) {
		memset(&context->stepIndex, 0, 0x468);
		__builtin_memset(&context->hello, 0, sizeof(context->hello));
		context->steps = swdlCarrierUpdateSteps;
		context->updateState = 1;
		result = swdlAllocateFrameBuffer(context);
		if (result < 0)
			return result;
		context->progressBytes = 0;
		result = swdlSetResponseTimeout(context, 3000);
		if (result < 0)
			return result;
		result = port->open(port, 2);
		if (result < 0)
			return result;
		if (!swdlSeekStep(context, swdlUpdateSendReset)) {
#if FW_DEBUG
			sceKernelAssert(0, &dbg__abortExec_L2139, __builtin_return_address(0));
#endif
		}
		result = swdlStartSend(context);
		if (result < 0)
			return result;
		context->abortState = 7;
		return result;
	} else if (state == 3) {
		if (!swdlSeekStep(context, swdlUpdateReceiveRestart)) {
#if FW_DEBUG
			sceKernelAssert(0, &dbg__abortExec_L2156, __builtin_return_address(0));
#endif
		}
		if (events & 0x200) {
			do {
				result = swdlReceiveFrame(context, &received);
				if (result != 0)
					return result;
			} while (received > 0);
		}
		if (events & 0x1800) {
			result = swdlHandleCancel(context);
			if (result < 0)
				return result;
		}
		if (events & 0x400) {
			result = swdlHandleFrameTimeout(context);
			if (result < 0)
				return result;
		}
		if (events & 0x10000) {
			result = swdlHandleDetach(context);
			if (result < 0)
				return result;
			if (result == 1) {
				result = swdlSendFrames(context);
				if (result < 0)
					return result;
				context->abortState = 4;
			} else {
#if FW_DEBUG
				sceKernelAssert(0, &dbg__abortExec_L2212, __builtin_return_address(0));
#endif
			}
		}
		if (events & 0x20000) {
			result = swdlHandleAttach(context);
			if (result != 0)
				return result;
		}
	} else if (state == 4) {
		if (events & 0x200) {
			for (;;) {
				result = swdlReceiveFrame(context, &received);
				if (result < 0)
					return result;
				if (result == 1) {
					if (context->receiveBuffer[0] != 0x18) {
						result = swdlSendFrames(context);
						if (result < 0)
							return result;
						break;
					}
					if (!swdlSeekStep(context, swdlUpdateSendReset)) {
#if FW_DEBUG
						sceKernelAssert(0, &dbg__abortExec_L2260, __builtin_return_address(0));
#endif
					}
					result = swdlStartSend(context);
					if (result < 0)
						return result;
					context->abortState = 7;
					break;
				}
				if (received <= 0)
					break;
			}
		}
		goto cancel;
	} else if (state == 5) {
		if (events & 0x200) {
			for (;;) {
				result = swdlReceiveFrame(context, &received);
				if (result < 0)
					return result;
				if (result != 1) {
					if (context->sendRetries == 0) {
						result = swdlSendFrames(context);
						if (result < 0)
							return result;
					}
					if (received > 0)
						continue;
					if (result != 1)
						break;
				}
				if (context->receiveBuffer[0] == 0x16) {
					if (!swdlSeekStep(context, swdlUpdateSendReset)) {
#if FW_DEBUG
						sceKernelAssert(0, &dbg__abortExec_L2341, __builtin_return_address(0));
#endif
					}
					result = swdlStartSend(context);
					if (result < 0)
						return result;
					context->abortState = 7;
				} else {
					result = swdlSendFrames(context);
					if (result < 0)
						return result;
				}
				break;
			}
		}
		if (events & 0x100) {
			result = swdlSendFrames(context);
			if (result < 0)
				return result;
		}
		goto cancel;
	} else if (state == 6) {
		if (!swdlSeekStep(context, swdlUpdateSendReset)) {
#if FW_DEBUG
			sceKernelAssert(0, &dbg__abortExec_L2404, __builtin_return_address(0));
#endif
		}
		result = swdlStartSend(context);
		if (result < 0)
			return result;
		context->abortState = 7;
		return result;
	} else if (state == 7) {
		if (events & 0x200) {
			do {
				result = swdlReceiveFrame(context, &received);
				if (result < 0)
					return result;
				if (result == 1) {
					context->abortState = 8;
					swdlFinishUpdate(context);
					return result;
				}
			} while (received > 0);
		}
cancel:
		if (events & 0x1800) {
			result = swdlHandleCancel(context);
			if (result < 0)
				return result;
		}
		if (events & 0x400) {
			result = swdlHandleFrameTimeout(context);
			if (result < 0)
				return result;
		}
	} else {
		return (int)0x80330002;
	}
	if (events & 0x100000)
		result = context->lastError;
	return result;
}

/* name unknown: state 2, the modem update: the boot loader (DMSS), then the partition
 * table and the images (streaming download); 4, 0x200000 and 0x400000 start the
 * sequence of each update kind */
int swdlExecUpdate(SwdlContext *context, SceUInt32 events)
{
	SwdlPortInterface *port = context->port;
	int received;
	int result = 0;

	if ((events & 0x80) && context->abortState == 0)
		context->abortState = 1;
	if (context->abortState != 0) {
		result = _abortExec(context, events);
		if (result < 0)
			goto error;
		return result;
	}
	if (events & 4) {
		context->updateKind = 0;
		result = swdlStartUpdate(context);
		if (result < 0)
			goto error;
		if (result == 3)
			swdlFinishUpdate(context);
	}
	if (events & 0x200000) {
		context->updateKind = 1;
		result = swdlStartUpdate(context);
		if (result < 0)
			goto error;
		if (result == 3)
			swdlFinishUpdate(context);
	}
	if (events & 0x400000) {
		context->updateKind = 2;
		result = swdlStartUpdate(context);
		if (result < 0)
			goto error;
		if (result == 3)
			swdlFinishUpdate(context);
	}
	if (events & 0x10) {
		if (context->hello.partitionTableStatus == 1)
			context->update.partitionTableForced = 1;
		result = swdlStartSend(context);
		if (result < 0)
			goto error;
	}
	if (events & 0x200) {
		do {
			result = swdlReceiveFrame(context, &received);
			if (result < 0)
				goto error;
			if (context->progressBytes > 0xc800) {
				context->notification.forwardedValue1 = context->progressBytes;
				context->progressBytes = 0;
				context->notify(64, &context->notification);
			}
			if (result == 3) {
				swdlFinishUpdate(context);
				break;
			}
			if (context->sendRetries == 0) {
				result = swdlSendFrames(context);
				if (result < 0)
					goto error;
				if (result == 3) {
					swdlFinishUpdate(context);
					break;
				}
			}
		} while (received > 0);
	}
	if (events & 0x100) {
		result = swdlSendFrames(context);
		if (result == 3)
			swdlFinishUpdate(context);
	}
	if (events & 0x1800) {
		result = swdlHandleCancel(context);
		if (result < 0)
			goto error;
		result = swdlSendFrames(context);
		if (result < 0)
			goto error;
		if (result == 3)
			swdlFinishUpdate(context);
	}
	if (events & 0x400) {
		result = swdlHandleFrameTimeout(context);
		if (result < 0)
			goto error;
		result = swdlSendFrames(context);
		if (result < 0)
			goto error;
		if (result == 3)
			swdlFinishUpdate(context);
	}
	if (events & 0x10000) {
		result = swdlHandleDetach(context);
		if (result < 0)
			goto error;
		result = swdlSendFrames(context);
		if (result < 0)
			goto error;
		if (result == 3)
			swdlFinishUpdate(context);
	}
	if (events & 0x20000) {
		result = swdlHandleAttach(context);
		if (result < 0)
			goto error;
		result = swdlSendFrames(context);
		if (result < 0)
			goto error;
		if (result == 3)
			swdlFinishUpdate(context);
	}
	if (events & 0x100000) {
		result = context->lastError;
		goto error;
	}
	return result;

error:
	port->close(port, 0);
	swdlReleaseBuffers(context);
	swdlCloseHexReader(context);
	context->notification.forwardedValue2 = result;
	context->updateState = 0;
	context->abortState = 0;
	context->notify(128, &context->notification);
	return result;
}
