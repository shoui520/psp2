/* Bluetooth audio transfer workers and lifecycle. */

#include "av_config_internal.h"
#include <audio/audio.h>
#include <audioin/audioin.h>
#include <bt/bt_audio.h>
#include <threadmgr/threadmgr.h>
#include <string.h>

SceUID avConfigBtSendThreadId = 0;
unsigned int avConfigBtReceiveFlags = 0;
unsigned int avConfigBtSendFlags = 0;
SceUID avConfigBtReceiveThreadId = 0;
SceUInt64 avConfigBtSendAddress = 0;
SceUID avConfigBtReceiveEvent = 0;
SceUInt64 avConfigBtReceiveAddress = 0;
SceUID avConfigBtSendEvent = 0;
int avConfigBtSendRate = 0;
int avConfigBtReceiveActive = 0;
SceInt16 avConfigBtReceiveSamples[1024] = {0};
int avConfigBtSendActive = 0;
int avConfigBtReceiveRate = 0;
SceInt16 avConfigBtSendSamples[2][1024] = {{0}};

static const char avConfigBtSendName[] = "SceAVConfigDataSend";
static const char avConfigBtReceiveName[] = "SceAVConfigDataRcev";

int avConfigBtReceiveThread(SceSize arglen, void *argp)
{
	SceUInt64 deadline;
	SceUInt64 now;
	SceUInt64 interval;
	unsigned int bits;
	int size;
	int received;

	(void)arglen;
	(void)argp;
	for (;;) {
		memset(avConfigBtReceiveSamples, 0, sizeof(avConfigBtReceiveSamples));
		sceKernelWaitEventFlag(avConfigBtReceiveEvent, 16, 5, NULL, NULL);
		if (avConfigBtReceiveFlags & 4) {
			size = 2048;
			interval = 10667;
		} else {
			size = 480;
			interval = 15000;
		}
		received = SceBtForDriver_CD7C0492(avConfigBtReceiveAddress,
					 avConfigBtReceiveSamples, size);
		deadline = (SceUInt64)sceKernelGetSystemTimeWide() + interval;
		for (;;) {
			SceAudioInForDriver_3D164387(avConfigBtReceiveSamples,
				(received < 0 ? 0 : received) >> 1);
			sceKernelPollEventFlag(avConfigBtReceiveEvent, 32, 5, &bits);
			if (bits == 32)
				break;
			now = sceKernelGetSystemTimeWide();
			if (now < deadline)
				sceKernelDelayThread((unsigned int)(deadline - now));
			deadline += interval;
			received = SceBtForDriver_CD7C0492(avConfigBtReceiveAddress,
						 avConfigBtReceiveSamples, size);
		}
	}
}

int avConfigBtSendThread(SceSize arglen, void *argp)
{
	SceInt16 *buffer;
	SceInt16 *previous;
	unsigned int bits;
	unsigned int words;
	unsigned int size;
	int bank;

	(void)arglen;
	(void)argp;
	for (;;) {
		memset(avConfigBtSendSamples[0], 0, sizeof(avConfigBtSendSamples[0]));
		sceKernelWaitEventFlag(avConfigBtSendEvent, 1, 5, NULL, NULL);
		if (avConfigBtSendFlags & 8) {
			words = 512;
			SceAudioForDriver_134C96C1(48000);
			size = 2048;
			SceAudioForDriver_FC375BAC(2);
		} else {
			words = 120;
			SceAudioForDriver_134C96C1(8000);
			size = 240;
			SceAudioForDriver_FC375BAC(1);
		}
		buffer = avConfigBtSendSamples[1];
		bank = 1;
		do {
			bank ^= 1;
			previous = buffer;
			buffer = avConfigBtSendSamples[bank];
			SceAudioForDriver_81EB0AE5(0, previous, words);
			SceBtForDriver_47F19727(avConfigBtSendAddress, buffer, size);
			sceKernelPollEventFlag(avConfigBtSendEvent, 2, 5, &bits);
		} while (bits != 2);
		SceAudioForDriver_134C96C1(48000);
		SceAudioForDriver_FC375BAC(2);
	}
}

int avConfigInitializeBtTransfer(void)
{
	int result;

	avConfigBtSendActive = 0;
	avConfigBtReceiveActive = 0;
	result = sceKernelCreateEventFlag(avConfigBtSendName, 0, 0, NULL);
	avConfigBtSendEvent = result;
	if (result < 0)
		return result;
	result = sceKernelCreateEventFlag(avConfigBtReceiveName, 0, 0, NULL);
	avConfigBtReceiveEvent = result;
	if (result < 0)
		return result;
	result = sceKernelCreateThread(avConfigBtSendName, avConfigBtSendThread,
				       18, 0x2000, 0, 0x80000, NULL);
	avConfigBtSendThreadId = result;
	if (result < 0)
		return result;
	result = sceKernelCreateThread(avConfigBtReceiveName, avConfigBtReceiveThread,
				       19, 0x2000, 0, 0x80000, NULL);
	avConfigBtReceiveThreadId = result;
	if (result < 0)
		return result;
	result = sceKernelStartThread(avConfigBtSendThreadId, 0, NULL);
	if (result < 0)
		return result;
	result = sceKernelStartThread(avConfigBtReceiveThreadId, 0, NULL);
	return result < 0 ? result : 0;
}

int avConfigStartBtSend(SceUInt64 address, unsigned int flags)
{
	int sampleRate = 48000;

	avConfigBtSendAddress = address;
	if ((flags & 8) == 0)
		sampleRate = 8000;
	avConfigBtSendFlags = flags;
	avConfigBtSendRate = sampleRate;
	avConfigBtSendActive = 1;
	sceKernelSetEventFlag(avConfigBtSendEvent, 1);
	return 0;
}

int avConfigStartBtReceive(SceUInt64 address, unsigned int flags, int sampleRate)
{
	avConfigBtReceiveAddress = address;
	if ((flags & 4) == 0)
		sampleRate = 8000;
	avConfigBtReceiveRate = sampleRate;
	avConfigBtReceiveFlags = flags;
	avConfigBtReceiveActive = 1;
	sceKernelSetEventFlag(avConfigBtReceiveEvent, 16);
	return 0;
}

int avConfigStopBtSend(void)
{
	if (avConfigBtSendActive == 1) {
		avConfigBtSendActive = 0;
		sceKernelSetEventFlag(avConfigBtSendEvent, 2);
	}
	return 0;
}

int avConfigStopBtReceive(void)
{
	if (avConfigBtReceiveActive == 1) {
		avConfigBtReceiveActive = 0;
		sceKernelSetEventFlag(avConfigBtReceiveEvent, 32);
	}
	return 0;
}
