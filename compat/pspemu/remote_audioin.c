/*
 * remote_audioin.c: Kermit microphone audio input server (channel 11)
 * capturing 16 kHz mono voice samples and resampling to 44.1 kHz stereo
 * for the PSP guest.
 */
#include "host_common.h"

#include <string.h>

#define AUDIOIN_CHANNEL_ID        11
#define AUDIOIN_CMD_STOP_FIRST    0x770U
#define AUDIOIN_CMD_STOP_LAST     0x771U
#define AUDIOIN_CMD_READ_SAMPLES  0x772U
#define AUDIOIN_COMPAT_INTR_ID    3
#define AUDIOIN_ERROR_INTR_BUSY   ((int)0x80010086)

int16_t              g_audioInPcmBuf[256][2];
PspemuAudioResampler g_audioInResampler;

static int audioInThread(SceSize args, void *argp)
{
	PspemuKermitRequest *req;
	int16_t *guestOut = NULL;
	int pendingInputFrames = 0;
	int port = -1;

	(void)args;
	(void)argp;

	audioResamplerInit(&g_audioInResampler, 16000, 44100);

	for (;;) {
		unsigned int cmd = (unsigned int)remotefsWaitAndGetRequest(AUDIOIN_CHANNEL_ID, &req);

		if (cmd >= AUDIOIN_CMD_STOP_FIRST && cmd <= AUDIOIN_CMD_STOP_LAST) {
			remotefsReturnValue(AUDIOIN_CHANNEL_ID, req, 0);
			guestOut = NULL;
		} else {
			if (cmd == AUDIOIN_CMD_READ_SAMPLES) {
				guestOut = (int16_t *)remotefsTranslateGuestAddr((uint32_t)req->args[0], 2, 256U);
			} else {
				pspemuRecordTitleListFlags(16U);
			}
			remotefsReturnValue(AUDIOIN_CHANNEL_ID, req, 0);
		}

		if (guestOut != NULL) {
			if (port < 0) {
				port = sceAudioInOpenPort(
					SCE_AUDIO_IN_PORT_TYPE_VOICE,
					256,
					16000,
					SCE_AUDIO_IN_PARAM_FORMAT_S16_MONO);
				pendingInputFrames = 0;
				audioResamplerInit(&g_audioInResampler, 16000, 44100);
			}

			if (port >= 0) {
				int produced = 0;
				do {
					const int16_t *srcSamples;
					if (pendingInputFrames > 0) {
						srcSamples = &g_audioInPcmBuf[256 - pendingInputFrames][0];
					} else {
						int16_t *monoSamples = &g_audioInPcmBuf[0][0];
						sceAudioInInput(port, monoSamples);
						for (int i = 255; i >= 0; i--) {
							int16_t sample = monoSamples[i];
							g_audioInPcmBuf[i][0] = sample;
							g_audioInPcmBuf[i][1] = sample;
						}
						srcSamples = &g_audioInPcmBuf[0][0];
						pendingInputFrames = 256;
					}

					audioResamplerProcess(
						&g_audioInResampler,
						srcSamples,
						pendingInputFrames,
						&guestOut[produced * 2],
						64 - produced,
						0x8000U);
					produced += g_audioInResampler.producedFrames;
					pendingInputFrames -= g_audioInResampler.consumedFrames;
				} while (produced < 64);
			} else {
				memset(guestOut, 0, 256U);
			}

			remotefsFlushGuestAddr(guestOut, 256U);

			int delayUs = pspemuCompatGetParam(6);
			if (delayUs == -1)
				delayUs = 500;
			sceKernelDelayThread((SceUInt)delayUs);

			int intrRet;
			do {
				intrRet = sceCompatInterrupt(AUDIOIN_COMPAT_INTR_ID);
				if (intrRet == AUDIOIN_ERROR_INTR_BUSY)
					sceKernelDelayThread(10000U);
			} while (intrRet == AUDIOIN_ERROR_INTR_BUSY);

			if (intrRet != 0)
				pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
		} else {
			if (port >= 0) {
				if (sceAudioInReleasePort(port) >= 0)
					port = -1;
			}
		}
	}
}

int remoteAudioInInit(void)
{
	SceUID thid = sceKernelCreateThread(
		"ScePspemuRemoteAudioIn",
		audioInThread,
		96,
		0x2000U,
		0,
		0x10000,
		NULL);
	if (thid >= 0)
		return sceKernelStartThread(thid, 0, NULL);
	return thid;
}
