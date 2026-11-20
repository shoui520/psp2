/*
 * The packet loss concealment of mSBC frames (include/robin/sbc.h): a good
 * frame goes into the history; a lost frame is made from the history,
 * repeating the stretch after the best match of the newest 64 samples,
 * scaled to the newest samples' amplitude, overlapped with the frame before
 * and faded out from the fourth lost frame. Marvell published no source;
 * names are inferred from use.
 */
#include <stdint.h>
#include "robin/rom.h"
#include "robin/sbc.h"

struct SbcPlcState sbcPlc;				/* 0x5a998 */

/* The overlap's fade, Q15: the repeated stretch takes these, the other side the reverse. */
const int16_t sbcPlcOverlap[SBC_PLC_OVERLAP] = {	/* 0x5459c */
	0x7ee9, 0x7bae, 0x766a, 0x6f4c, 0x6692, 0x5c87, 0x5184, 0x45e8,
	0x3a18, 0x2e7c, 0x2379, 0x196e, 0x10b4, 0x0996, 0x0453, 0x0117,
};

void btSbcReleaseBuffer(struct SbcPlcState *state)
{
	__aeabi_memclr4(state, sizeof(*state));
	state->attenuation = 0x7fff;
}

/* A register shift right: 0 from 32 places, or for a negative count. */
static inline uint32_t plcShiftRight(uint32_t value, int count)
{
	return count >= 0 && count < 32 ? value >> count : 0;
}

/*
 * The lag 0..255 at which the newest 64 samples correlate best, by the
 * correlation squared over the energy, both kept to 16 bits.
 */
int16_t btSbcSetupBuffer(const struct SbcPlcState *state)
{
	int16_t scaled[SBC_PLC_HISTORY];
	int32_t correlations[4];
	uint32_t peak = 0, block, energy, product, bestProduct = 0, bestEnergy = 0;
	int32_t correlation;
	int i, lag, scale, bestScale = 0, bestLag = 0;
	const int16_t *signal = state->history;

	for (i = 0; i < 5; i++) {
		block = btSbcResetPacket(signal);
		if (block > peak)
			peak = block;
		signal += SBC_PLC_TEMPLATE;
	}
	btSbcComputePacket(state->history, scaled, 4 - ((unsigned int)(robinClz16(peak) + 1) >> 1),
			   SBC_PLC_HISTORY);
	energy = btSbcSyncPacket(scaled);
	signal = scaled;
	lag = 0;
	do {
		btSbcBuildPacket(signal, &scaled[SBC_PLC_HISTORY - SBC_PLC_TEMPLATE], correlations,
				 SBC_PLC_TEMPLATE);
		for (i = 0; i < 4; i++) {
			correlation = correlations[i];
			if (correlation > 0) {
				scale = (int16_t)(15 - ((unsigned int)robinClz16(correlation) >> 1));
				if (scale < 0)
					scale = 0;
				product = (uint32_t)((correlation >> scale) * (correlation >> scale));
				if (plcShiftRight(bestProduct, 16 - 2 * (bestScale - scale)) * (energy >> 16)
				    <= (product >> 16) * (bestEnergy >> 16)) {
					bestScale = scale;
					bestLag = lag;
					bestEnergy = energy;
					bestProduct = product;
				}
			}
			lag++;
			energy += (uint32_t)(signal[SBC_PLC_TEMPLATE] * signal[SBC_PLC_TEMPLATE] -
					     signal[0] * signal[0]);
			signal++;
		}
	} while (lag < SBC_PLC_LAGS);
	return bestLag;
}

/* The ratio of the newest samples' amplitude to the stretch's, Q9, 0.75..1.2. */
int btSbcClearBuffer(const struct SbcPlcState *state)
{
	int32_t newest = 0, stretch = 0;
	int count, scale;

	count = state->lag < SBC_PLC_FRAME ? state->lag : SBC_PLC_FRAME;
	btSbcApplyPacket(&state->history[SBC_PLC_HISTORY - 1], &state->history[state->lag], count,
			 &newest, &stretch);
	scale = (unsigned int)(newest << 9) / (unsigned int)(stretch + 1);
	if (scale < 384)
		return 384;
	if (scale > 614)
		return 614;
	return scale;
}

/* Overlaps 16 samples: first fading out, second fading in. */
void btSbcConvertBuffer(const int16_t *first, const int16_t *second, int16_t *output)
{
	const int16_t *fadeOut = sbcPlcOverlap;
	const int16_t *fadeIn = &sbcPlcOverlap[SBC_PLC_OVERLAP - 1];
	int i;

	for (i = 0; i < SBC_PLC_OVERLAP / 2; i++) {
		*output++ = btSbcFilterBuffer((*first++ * *fadeOut++ + *second++ * *fadeIn-- + 0x4000) >> 15);
		*output++ = btSbcFilterBuffer((*first++ * *fadeOut++ + *second++ * *fadeIn-- + 0x4000) >> 15);
	}
}

/* Repeats the stretch into count samples; the next frame starts the stretch again. */
int btSbcVerifyBuffer(struct SbcPlcState *state, int16_t *output, int count)
{
	int part;

	while (count != 0) {
		part = (int16_t)(state->length - state->position);
		if (part > count)
			part = count;
		btSbcQueryPacket(&state->history[state->lag + state->position], output, part);
		state->position = state->position + part;
		if (state->position == state->length)
			state->position = 0;
		output += part;
		count = (int16_t)(count - part);
	}
	state->position = 0;
	return 0;
}

/*
 * The first lost frame: scales the stretch in the history, and the frame,
 * whose first 16 samples ramp from 1 to the scale (Q9).
 */
void btSbcSelectBuffer(struct SbcPlcState *state, int16_t *output, int scale)
{
	int16_t *sample = &state->history[state->lag];
	int count = SBC_PLC_HISTORY - state->lag;
	int i, gain, step;

	for (i = 0; i < count >> 2; i++) {
		*sample = btSbcFilterBuffer((*sample * scale + 256) >> 9);
		sample++;
		*sample = btSbcFilterBuffer((*sample * scale + 256) >> 9);
		sample++;
		*sample = btSbcFilterBuffer((*sample * scale + 256) >> 9);
		sample++;
		*sample = btSbcFilterBuffer((*sample * scale + 256) >> 9);
		sample++;
	}
	for (i = 4 * i; i < count; i++) {
		*sample = btSbcFilterBuffer((*sample * scale + 256) >> 9);
		sample++;
	}
	gain = 512;
	step = (scale - 504) >> 4;
	for (i = 0; i < SBC_PLC_OVERLAP / 2; i++) {
		*output = btSbcFilterBuffer((*output * gain + 256) >> 9);
		output++;
		gain = (int16_t)(gain + step);
		*output = btSbcFilterBuffer((*output * gain + 256) >> 9);
		output++;
		gain = (int16_t)(gain + step);
	}
	for (; i < SBC_PLC_FRAME / 2; i++) {
		*output = btSbcFilterBuffer((*output * scale + 256) >> 9);
		output++;
		*output = btSbcFilterBuffer((*output * scale + 256) >> 9);
		output++;
	}
}

/*
 * A lost frame. The first finds the stretch (sbcPlc.length holds the match
 * of the last good frame) and overlaps the decoder's output into it; from
 * the ninth the frame is silent.
 */
void btSbcAdjustBuffer(struct SbcPlcState *state, int16_t *input, int16_t *output)
{
	int scale = 0, lag, gain, i;

	state->badFrames++;
	if (state->badFrames == 1) {
		lag = state->length + SBC_PLC_TEMPLATE;
		state->lag = lag;
		state->length = SBC_PLC_HISTORY - lag;
		state->position = 0;
		scale = btSbcClearBuffer(state);
		btSbcQueryPacket(input, &state->history[state->lag], 8);
		btSbcConvertBuffer(input + 8, &state->history[state->lag + 8], &state->history[state->lag + 8]);
	} else if (state->badFrames > 8) {
		for (i = 0; i < SBC_PLC_FRAME; i++)
			output[i] = 0;
		goto shift;
	}
	btSbcVerifyBuffer(state, output, SBC_PLC_FRAME);
shift:
	btSbcHandlePacket(state, output, output);
	if (state->badFrames == 1) {
		btSbcSelectBuffer(state, output, (int16_t)scale);
	} else if ((unsigned int)(state->badFrames - 4) <= 4) {
		gain = state->attenuation;
		for (i = 0; i < SBC_PLC_FRAME; i++) {
			output[i] = (output[i] * gain) >> 15;
			gain -= SBC_PLC_FADE;
		}
		state->attenuation = gain;
	}
}

/* A good frame; after lost ones, its first 52 samples overlap the stretch. */
int btSbcRecordBuffer(struct SbcPlcState *state, int16_t *input, int16_t *output)
{
	int16_t overlap[SBC_PLC_OVERLAP];

	if (state->badFrames <= 0) {
		btSbcHandlePacket(state, input, output);
	} else {
		__aeabi_memcpy(overlap, input + 36, sizeof(overlap));
		btSbcVerifyBuffer(state, output, 52);
		btSbcConvertBuffer(output + 36, overlap, output + 36);
		btSbcQueryPacket(input + 52, output + 52, 68);
		btSbcHandlePacket(state, output, output);
		state->attenuation = 0x7fff;
	}
	state->badFrames = 0;
	return 0;
}

__attribute__((noinline)) int16_t btSbcFilterBuffer(int value)
{
	if (value > 32767)
		value = 32767;
	else if (value < -32768)
		value = -32768;
	return value;
}

/* Moves the history on by a frame, appends input and gives the frame back in output. */
void btSbcHandlePacket(struct SbcPlcState *state, const int16_t *input, int16_t *output)
{
	btSbcQueryPacket(&state->history[SBC_PLC_FRAME], state->history, SBC_PLC_HISTORY - SBC_PLC_FRAME);
	btSbcQueryPacket(input, &state->history[SBC_PLC_HISTORY - SBC_PLC_FRAME], SBC_PLC_FRAME);
	btSbcQueryPacket(&state->history[SBC_PLC_HISTORY - SBC_PLC_FRAME], output, SBC_PLC_FRAME);
}
