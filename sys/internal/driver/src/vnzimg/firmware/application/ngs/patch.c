/*
 * Patches (0x877d48): a voice's output, after its module ran, is added into
 * each patched voice's input mixer, through a free port (each port has its
 * own lock; the ports are tried in turn) and the patch's volume matrix. A
 * changed volume ramps over the first 16 samples, in 17ths.
 */
#include "ngs.h"

#define RAMP_SAMPLES	16
#define MIX_BLOCK	8	/* samples the IVC2 adds at once */
#define MIXER_SIZE	128	/* the settings' cache lines */

static uint32_t lockPort(NgsInputMixerSettings *mixer)
{
	uint32_t port = 0;

	while (vnzSpinTry(mixer->locks[port]))
		port = (port + 1) % mixer->ports;
	return port;
}

/* from: the output's channel; to, the mixer's */
static void mixChannel(NgsBuffer *buffer, int32_t *destination, const int32_t *source,
	int32_t previous, int32_t volume)
{
	int32_t step, i;

	cachePurgeRange((uint32_t)destination, buffer->samples * NGS_SAMPLE_SIZE);
	step = (volume - previous) / (RAMP_SAMPLES + 1);
	for (i = 0; i < RAMP_SAMPLES; i++)
		destination[i] += ngsMultiply(source[i], previous + (i + 1) * step);
	if (!volume) {
		cacheFlushRange((uint32_t)destination, RAMP_SAMPLES * NGS_SAMPLE_SIZE);
		return;
	}
	for (i = RAMP_SAMPLES;
	     i < RAMP_SAMPLES + ((int32_t)buffer->samples - RAMP_SAMPLES) / MIX_BLOCK * MIX_BLOCK; i++)
		destination[i] += ngsMultiply(source[i], volume);
	cacheFlushRange((uint32_t)destination, buffer->samples * NGS_SAMPLE_SIZE);
}

void ngsRouteChannel(NgsBuffer *buffer, NgsVoice *voice, int32_t output)
{
	NgsRack *rack = voice->rack;
	uint32_t count = rack->patchesPerOutput, channels = rack->channels, p, from, to;
	int32_t *applied = (int32_t *)(voice->settings + rack->volumesOffset) +
		output * count * channels * 2;

	for (p = 0; p < count; p++) {
		NgsPatch *patch = &voice->patches[output * count + p];
		NgsInputMixerSettings *mixer = patch->mixer;
		int32_t *volumes = applied + p * channels * 2;
		uint32_t port;

		if (!mixer) {
			for (from = 0; from < (uint32_t)buffer->channels; from++) {
				volumes[2 * from] = 0;
				volumes[2 * from + 1] = 0;
			}
			continue;
		}
		if (!(buffer->flags & NGS_BUFFER_VALID))
			continue;

		cachePurgeRange((uint32_t)mixer, MIXER_SIZE);
		port = lockPort(mixer);
		if (mixer->reset[port] == 1) {
			for (to = 0; to < mixer->channels; to++)
				ngsInputMixerClear(mixer->buffers[port][to], buffer->channelSize);
			mixer->reset[port] = 0;
			cacheFlushRange((uint32_t)mixer, MIXER_SIZE);
		}

		for (from = 0; from < (uint32_t)buffer->channels; from++)
			for (to = 0; to < mixer->channels; to++) {
				int32_t volume = patch->volumes[2 * from + to];
				int32_t previous = volumes[2 * from + to];

				/* a voice that starts takes its volumes at once */
				if (voice->state & SCE_NGS_VOICE_STATE_ACTIVE)
					volumes[2 * from + to] = previous = volume;
				if (!((buffer->channelMask >> from) & 1))
					continue;
				if (!volume && !previous)
					continue;
				volumes[2 * from + to] = volume;
				mixChannel(buffer, mixer->buffers[port][to],
					(const int32_t *)buffer->channelData[from], previous, volume);
			}
		vnzSpinUnlock(mixer->locks[port]);
	}
}
