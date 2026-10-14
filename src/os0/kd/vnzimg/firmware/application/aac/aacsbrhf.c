/*
 * The SBR HF generator's patches and the QMF banks' setup
 * (0x853aca-0x853dba, 0x853f6a-0x85406a): ISO/IEC 14496-3 4.6.18.6.3.
 */
#include "aac.h"

#define Q27(x)	((int32_t)((x) * (1 << 27)))

/*
 * The master table's entry nearest `value`: the first not below it going
 * up, the last not above it going down.
 */
static uint8_t findBand(uint8_t value, const uint8_t *master, uint8_t numMaster, int32_t up)
{
	int32_t i;

	if (!(master[0] < value))
		return master[0];
	if (!(value < master[numMaster]))
		return master[numMaster];
	if (up) {
		for (i = 0; master[i] < value; i++)
			;
	} else {
		for (i = numMaster; value < master[i]; i--)
			;
	}
	return master[i];
}

/* patch construction; -1 if the patches cannot reach the stop band */
int32_t aacSbrBuildPatches(AacSbrHf *hf, uint8_t kx, const uint8_t *master, uint8_t numMaster,
	const uint8_t *noise, uint8_t numNoise, uint8_t stop, uint32_t rate)
{
	AacSbrPatches *patches = hf->patches;
	int32_t k0 = master[0], msb = kx - k0 + 1, usb = kx;
	int32_t goal, numPatches = 0, numBands, offset, last, i;
	uint8_t goalBand;

	if (k0 - 1 < 4)
		return -1;
	switch (rate) {	/* NINT(2.048E6 / Fs) */
	case 96000:
		goal = 21;
		break;
	case 88200:
		goal = 23;
		break;
	case 64000:
		goal = 32;
		break;
	case 48000:
		goal = 43;
		break;
	case 44100:
		goal = 46;
		break;
	case 32000:
	case 24000:
	case 22050:
	case 16000:
		goal = 64;
		break;
	default:
		return -1;
	}
	goalBand = findBand(goal, master, numMaster, 1);

	while (usb < stop && numPatches < 6) {
		AacSbrPatch *patch = &patches->patch[numPatches];

		patch->target = usb;
		patch->targetStart = usb;
		if (goalBand - usb < k0 - msb) {
			numBands = goalBand - usb;
		} else {
			int32_t odd = usb - ((usb - msb) & ~1);

			numBands = findBand(usb + k0 - odd, master, numMaster, 0) - usb;
		}
		offset = (usb + numBands - k0 + 1) & ~1;
		if (numBands >= 1) {
			patch->source = usb - offset;
			numPatches++;
			patch->numBands = numBands;
			patch->sourceEnd = patch->source + numBands;
			patch->offset = offset;
			usb += numBands;
		}
		if (goalBand - usb < 3)
			goalBand = stop;
		msb = 1;
	}

	/* a last patch of fewer than 3 bands is dropped */
	last = numPatches - 1;
	if (last >= 1 && patches->patch[last].numBands < 3) {
		last--;
		usb = patches->patch[last].targetStart + patches->patch[last].numBands;
	}
	if (usb < stop - 2)
		return -1;

	patches->sourceLow = usb;
	patches->numPatches = last + 1;
	patches->sourceHigh = 0;
	for (i = 0; i < patches->numPatches; i++) {
		patches->sourceLow = aacMin(patches->sourceLow, patches->patch[i].source);
		patches->sourceHigh = aacMax(patches->sourceHigh, patches->patch[i].sourceEnd);
	}
	for (i = 0; i < numNoise; i++)
		patches->noise[i] = noise[i + 1];

	patches->bandwidth[0] = 0;
	patches->bandwidth[1] = Q27(0.6);
	patches->bandwidth[2] = Q27(0.75);
	patches->bandwidth[3] = Q27(0.9);
	patches->bandwidth[4] = Q27(0.98);
	return 0;
}

/*
 * The HF generator of the element's index'th channel; its LPC history is
 * cleared. Returns nonzero if the bands cannot be patched.
 */
int32_t aacSbrHfInit(AacSbrHf *hf, uint8_t kx, const uint8_t *master, uint8_t numMaster,
	uint8_t stop, int32_t timeSlots, uint8_t numSlots, const uint8_t *noise,
	uint8_t numNoise, uint32_t rate, uint8_t index, int32_t channel,
	struct AacDecoder *decoder)
{
	int32_t n = index + channel, i, j;

	hf->patches = &decoder->sbrPatches[n];
	hf->patches->numSlots = numSlots;
	if (timeSlots != 16)
		return 1;

	hf->lpcReal[0] = (int32_t *)decoder->channelBuffer[AAC_BUFFER_LPC_REAL][n];
	hf->lpcReal[1] = hf->lpcReal[0] + 64;
	hf->lpcImag[0] = (int32_t *)decoder->channelBuffer[AAC_BUFFER_LPC_IMAG][n];
	hf->lpcImag[1] = hf->lpcImag[0] + 64;
	for (i = 0; i < 2; i++) {
		for (j = 0; j < 32; j++) {
			hf->lpcReal[i][j] = 0;
			hf->lpcImag[i][j] = 0;
		}
	}
	return aacSbrBuildPatches(hf, kx, master, numMaster, noise, numNoise, stop, rate);
}

/* the analysis bank: 32 bands, its 320-sample delay line cleared */
void aacSbrAnalysisInit(AacSbrQmf *qmf, int32_t *timing, int32_t numSlots, int32_t kx,
	int32_t stop, int32_t status, int32_t index, int32_t downsampled, int32_t channel,
	struct AacDecoder *decoder)
{
	aacSbrFill((int8_t *)qmf, 0, sizeof(*qmf));
	qmf->window = aacSbrQmfWindow;
	if (downsampled) {
		qmf->cos = aacSbrQmfCos32;
		qmf->sin = aacSbrQmfSin32;
	} else {
		qmf->cos = aacSbrQmfCos64;
		qmf->sin = aacSbrQmfSin64;
	}
	qmf->numBands = 32;
	qmf->kx = kx;
	qmf->numSlots = numSlots;
	qmf->bufferSize = 320;
	qmf->status = status;
	qmf->stop = stop;
	qmf->analysisBuffer = (int32_t *)decoder->channelBuffer[AAC_BUFFER_ANALYSIS][index + channel];
	aacSbrFill((int8_t *)qmf->analysisBuffer, 0, 1280);
	timing[1] = 0;
}

/*
 * The synthesis bank: 64 bands, or 32 when the output stays at the core's
 * rate; its delay line cleared.
 */
void aacSbrSynthesisInit(AacSbrQmf *qmf, int32_t *timing, int32_t numSlots, int32_t kx,
	int32_t stop, int32_t status, uint8_t created, int32_t index, int32_t downsampled,
	int32_t channel, struct AacDecoder *decoder)
{
	aacSbrFill((int8_t *)qmf, 0, sizeof(*qmf));
	qmf->kx = kx;
	qmf->numSlots = numSlots;
	qmf->window = aacSbrQmfWindow;
	qmf->created = created;
	if (downsampled) {
		qmf->bufferSize = 320;
		qmf->stop = 32;
		qmf->numBands = 32;
	} else {
		qmf->bufferSize = 640;
		qmf->numBands = 64;
		qmf->stop = stop;
	}
	qmf->status = status;
	qmf->synthesisBuffer = (int32_t *)decoder->channelBuffer[AAC_BUFFER_SYNTHESIS][index + channel];
	aacSbrFill((int8_t *)qmf->synthesisBuffer, 0, 5120);
	timing[5] = -6;
}
