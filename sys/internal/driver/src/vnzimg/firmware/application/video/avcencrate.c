/*
 * The AVC encoder's rate control (0x838788-0x838b48, 0x83c350-0x83e260):
 * the bits each picture may take from the bit rates and the CPB, the
 * quantizer it is coded with, and the buffering period the pictures report.
 */
#include "avcencstream.h"

#define Q15			32768
#define NINE_TENTHS		29491	/* in Q15 */
#define GAIN			0x18000	/* 1.5 in Q16 */
#define THIRD			10922	/* in Q15 */

/* the quantizers a macroblock's bits give, by the picture's width (QCIF, CIF, larger) */
#define WIDTH_QCIF		176
#define WIDTH_CIF		352

static void rateInitialQp(AvcencPicture *picture, AvcencRate *rate);
static void ratePictureBits(AvcencPicture *picture, AvcencRate *rate);

/*
 * 0x838788: the rate control's start with the stream's first frame: the
 * frame rate and the bits per frame from the settings, nine tenths of the
 * CPB sizes, the sums cleared, and the first quantizer (the settings' when
 * it is constant)
 */
void avcencRateInitialize(AvcencPicture *picture, uint32_t rateAddress)
{
	AvcencRate *rate = (AvcencRate *)vnzPublicOpenPrivate(rateAddress, RATE_STATE_SIZE);
	uint64_t frameRate;
	int32_t i;

	rate->idrTaken = 0;
	rate->intraCount = 0;
	rate->idrPending = 0;
	rate->intraBits = 0;
	rate->pictures = 0;
	rate->sceneIdr = 0;
	rate->intraRequested = 0;
	rate->idrRequested = 0;
	rate->sceneChange = 0;
	rate->counts[3] = 0;
	rate->counts[2] = 0;
	rate->counts[1] = 0;
	rate->counts[0] = 0;
	frameRate = ((uint64_t)picture->timeScale << 15) / (uint64_t)(picture->numUnitsInTick * 2);
	rate->frameRate = frameRate;
	frameRate = (uint32_t)frameRate;
	rate->maxFrameBits = ((uint64_t)picture->maxBitRate << 15) / frameRate;
	rate->frameBits = ((uint64_t)picture->bitRate << 15) / frameRate;
	rate->maxCpbLimit = ((uint64_t)picture->maxCpbSize * NINE_TENTHS) >> 15;
	rate->overflow[0] = 0;
	rate->overflow[2] = 0;
	rate->overflow[1] = 0;
	rate->error[1] = 0;
	rate->error[0] = 0;
	rate->cpbLimit = ((uint64_t)picture->cpbSize * NINE_TENTHS) >> 15;
	rate->gain2 = THIRD;
	rate->gain = GAIN;
	for (i = 5; i >= 0; i--)
		rate->sums[i] = 0;
	ratePictureBits(picture, rate);
	rate->weights[2] = Q15;
	rate->weights[1] = Q15;
	rate->weights[0] = Q15;
	if (picture->constantQuantizer) {
		picture->rateQp = picture->quantizer;
	} else {
		rateInitialQp(picture, rate);
		picture->rateQp = rate->initialQp;
	}
	vnzPublicClosePrivate((uint32_t)rate, RATE_STATE_SIZE);
}

/* the bits a macroblock gets, and the most bits that give each quantizer */
static const uint16_t qpBitsOption2[3][11] = {
	{ 1, 2, 2, 3, 3, 4, 5, 20, 26, 77, 154 },
	{ 2, 3, 4, 6, 7, 8, 10, 40, 51, 154, 307 },
	{ 3, 6, 9, 12, 14, 17, 20, 100, 154, 358, 614 },
};
static const uint8_t qpOption2[11] = { 43, 38, 36, 34, 33, 31, 30, 28, 25, 20, 15 };
static const uint8_t qpPlain[3] = { 35, 25, 20 };

/*
 * 0x838966: the first quantizer, from the bits each macroblock gets at the
 * picture bit rate, by thresholds that grow with the picture's width
 */
static void rateInitialQp(AvcencPicture *picture, AvcencRate *rate)
{
	uint32_t widthInMbs = picture->widthInMbs, width = widthInMbs << 4, mbsPerSecond, bits;
	const uint16_t *thresholds;
	int32_t i;

	mbsPerSecond = ((uint64_t)rate->frameRate * (picture->heightInMbs * widthInMbs)) >> 15;
	bits = picture->pictureBitRate / mbsPerSecond;
	thresholds = qpBitsOption2[width <= WIDTH_QCIF ? 0 : width <= WIDTH_CIF ? 1 : 2];
	if (picture->option2) {
		for (i = 0; i < 11; i++) {
			if (bits <= thresholds[i]) {
				rate->initialQp = qpOption2[i];
				return;
			}
		}
		rate->initialQp = 10;
		return;
	}
	for (i = 0; i < 3; i++) {
		if (bits <= thresholds[8 + i]) {
			rate->initialQp = qpPlain[i];
			return;
		}
	}
	rate->initialQp = 10;
}

/*
 * 0x838aa0: a picture's bits: 96 per macroblock (48 at levels 3.1 to 4),
 * rounded up to 256 bytes in bits, and the limit at 5.6 of them
 */
static void ratePictureBits(AvcencPicture *picture, AvcencRate *rate)
{
	uint32_t level = (uint8_t)picture->level, mbs = picture->widthInMbs * picture->heightInMbs;
	uint64_t bits;
	int32_t shift = 2;

	if (level >= 31 && level <= 40)
		shift = 4;
	bits = (uint32_t)(mbs * 3 << 7) >> 1;
	if (shift == 4)
		bits >>= 1;
	rate->pictureBits = (((uint32_t)bits + 255) & ~255) << 3;
	rate->pictureBitsLimit = (bits * 22937) >> 12;
}
