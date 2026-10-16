/*
 * FW2: the decode's start, and the concealment of a field whose pair did not
 * come, PU1's (fw1/pu1/decode.c) changed. A restart drops what is unread of both
 * rings PU0 writes. FW2 keeps after takeMotionData a function nothing calls (it
 * counts a drop and skips both rings to their writers); it is not written.
 */
#include "codec.h"

/* concealPicture's units: the picture's writes, by fields */
#define CONCEAL_LAYOUT		2
#define CONCEAL_STRIDE		192
#define CONCEAL_TOP_FIELD	0x3fffb8	/* setupChannels' PICTURE_FIELD */
#define CONCEAL_BOTTOM_FIELD	(CONCEAL_TOP_FIELD + VIP_BOTTOM_FIELD_OFFSET)
#define CONCEAL_COLOUR		0x808080	/* grey: Y << 16 | Cb << 8 | Cr */

/*
 * A decode begins: take the decoder's rings, work memory and frame stores. When
 * PU0 passed a restart on (the records ring's reader state 0), the stream starts
 * over: the decoding is reset and what is unread of both rings is dropped.
 * Nonzero when it did.
 */
uint32_t beginDecode(VipCodecDecoder *decoder)
{
	uint32_t restarted, i;

	g_ringHandles[VIP_RING_SLICES] = &decoder->rings[VIP_RING_SLICES];
	g_ringHandles[VIP_RING_OUTPUT] = &decoder->rings[VIP_RING_OUTPUT];
	g_ringHandles[VIP_RING_RECORDS] = &decoder->rings[VIP_RING_RECORDS];
	g_workBase = decoder->workMemory;
	for (i = 0; i < VIP_CODEC_FRAMES; i++)
		g_frameHandles[i] = &decoder->frames[i];
	restarted = getRingReaderState0(&decoder->rings[VIP_RING_RECORDS]);
	if (restarted) {
		g_generation++;
		resetDecoding();
		while (ringSkipToWriter(&decoder->rings[VIP_RING_RECORDS]))
			;
		while (ringSkipToWriter(&decoder->rings[VIP_RING_SLICES]))
			;
		setRingReaderState0(&decoder->rings[VIP_RING_RECORDS], 0);
	}
	return restarted;
}

/*
 * Conceal the field of the current picture that did not come, the one the last
 * slice was not of: all its macroblocks in grey.
 */
void concealPicture(void)
{
	uint32_t base;

	vipWriteControlBus(VIP_CB_CHANNEL_WIDTH, g_widthMbsMinus1);
	vipWriteControlBus(VIP_CB_PICTURE_LAYOUT, CONCEAL_LAYOUT);
	vipWriteControlBus(VIP_CB_PICTURE_STRIDE, CONCEAL_STRIDE);
	base = g_sliceHeader.bottomField ? CONCEAL_TOP_FIELD : CONCEAL_BOTTOM_FIELD;
	vipWriteControlBus(VIP_CB_PICTURE_BASE, g_currentBuffer->picture + base);
	vipWriteControlBus(VIP_CB_PICTURE_X, 0);
	vipWriteControlBus(VIP_CB_PICTURE_Y, 0);
	vipWriteControlBus(VIP_CB_PICTURE_COMMAND, VIP_UNIT_START);
	fillMacroblocks(g_pictureMbs, CONCEAL_COLOUR);
}
