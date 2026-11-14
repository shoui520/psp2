/*
 * The AVC encoder's rate control (0x838788-0x838b48, 0x83c350-0x83e260):
 * the bits each picture may take from the bit rates and the CPB, the
 * quantizer it is coded with, and the buffering period the pictures report.
 * The jobs that give the client's output its macroblocks' modes sit between.
 */
#include "avcencstream.h"

#define Q15			32768
#define NINE_TENTHS		29491	/* in Q15 */
#define FOUR_FIFTHS		26214
#define SIX_FIFTHS		39321
#define INTRA_RATIO		0x18000	/* 3.0 in Q15 */
#define INTER_RATIO		10922	/* a third in Q15 */
#define WEIGHT_MIN		3276	/* a tenth in Q15 */
#define WEIGHT_MAX		0x18000

#define TICKS			90000	/* the HRD's delays' clock */
#define QP_MIN			16
#define QP_MAX			51
#define STEP_LIMIT		0x7f8000

/* the QP's steps of the six in an octave, in Q15 */
static const uint32_t quantizerSteps[6] = { 20480, 22528, 26624, 28672, 32768, 36864 };

/* the picture's result to the engine: done, an HRD overflowed, or to be replaced */
#define ENGINE_FLAG_RATE_DONE		0x01000000
#define ENGINE_FLAG_NAL_OVERFLOW	0x02000000
#define ENGINE_FLAG_VCL_OVERFLOW	0x04000000
#define ENGINE_FLAG_REPLACE		0x08000000

#define MODES_INFO_STRIDE	192	/* a core's, in the output info */

/* the quantizers a macroblock's bits give, by the picture's width (QCIF, CIF, larger) */
#define WIDTH_QCIF		176
#define WIDTH_CIF		352

static void rateInitialQp(AvcencPicture *picture, AvcencRate *rate);
static void ratePictureBits(AvcencPicture *picture, AvcencRate *rate);
static void rateRestart(AvcencPicture *picture, AvcencRate *rate);
static void modesPack(AvcencPicture *picture, AvcencJobInfo *jobInfo, AvcencModesInfo *info);
static void rateGroup(AvcencPicture *picture, AvcencRate *rate);
static void rateEstimate(AvcencPicture *picture, AvcencRate *rate);
static void rateDelays(AvcencPicture *picture, AvcencRate *rate);
static void rateTarget(AvcencPicture *picture, AvcencRate *rate);
static int32_t rateLimit(AvcencPicture *picture, AvcencRate *rate);
static void rateQuantizer(AvcencPicture *picture, AvcencRate *rate);
static void rateQuantizerMax(AvcencPicture *picture, AvcencRate *rate);
static void rateHrd(AvcencPicture *picture, AvcencRate *rate);
static void rateLearn(AvcencPicture *picture, AvcencRate *rate);
static void rateSkipped(AvcencPicture *picture, AvcencRate *rate);
static void rateRelax(AvcencPicture *picture, AvcencRate *rate);
static void rateSave(AvcencRate *rate);
static void rateRestore(AvcencRate *rate);
static void rateStatistics(AvcencPicture *picture, AvcencRate *rate);
static int32_t qpFromStep(int32_t step);
static uint32_t stepFromQp(int32_t qp);

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

	rate->idr = 0;
	rate->pictures = 0;
	rate->idrPending = 0;
	rate->targetBits = 0;
	rate->picturesLeft = 0;
	rate->pictureOverflow = 0;
	rate->oversized = 0;
	rate->vclOverflow = 0;
	rate->nalOverflow = 0;
	rate->fullness = 0;
	rate->groupBits = 0;
	rate->groupMaxBitsLeft = 0;
	rate->groupBitsLeft = 0;
	frameRate = ((uint64_t)picture->timeScale << 15) / (uint64_t)(picture->numUnitsInTick * 2);
	rate->frameRate = frameRate;
	frameRate = (uint32_t)frameRate;
	rate->maxFrameBits = ((uint64_t)picture->maxBitRate << 15) / frameRate;
	rate->frameBits = ((uint64_t)picture->bitRate << 15) / frameRate;
	rate->maxCpbLimit = ((uint64_t)picture->maxCpbSize * NINE_TENTHS) >> 15;
	rate->restart = 0;
	rate->skipNext = 0;
	rate->maxQp = 0;
	rate->interComplexity = 0;
	rate->intraComplexity = 0;
	rate->cpbLimit = ((uint64_t)picture->cpbSize * NINE_TENTHS) >> 15;
	rate->interRatio = INTER_RATIO;
	rate->intraRatio = INTRA_RATIO;
	rate->removal[1] = 0;
	rate->removal[0] = 0;
	rate->finalArrival[1] = 0;
	rate->finalArrival[0] = 0;
	rate->initialArrival[1] = 0;
	rate->initialArrival[0] = 0;
	ratePictureBits(picture, rate);
	rate->interWeight = Q15;
	rate->intraWeight = Q15;
	rate->weight = Q15;
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

/*
 * 0x83c350: the rate control before the picture: a skipped picture's bits,
 * else the quantizer from the motion job's statistics (an IDR period's start
 * restarts the estimates), or the settings' constant one with the HRD's
 * delays; the chroma QP, and an IDR's QP limit from its cost
 */
void avcencJobRate(uint32_t pictureAddress, uint32_t rateAddress, uint32_t unused,
	uint32_t unused2)
{
	AvcencPicture *picture = (AvcencPicture *)pictureAddress;
	AvcencRate *rate = (AvcencRate *)vnzPublicOpenPrivate(rateAddress, RATE_STATE_SIZE);
	int8_t *quantizer;

	if (rate->skipNext) {
		rate->skipNext = 0;
		vnzPublicClosePrivate((uint32_t)rate, RATE_STATE_SIZE);
		return;
	}
	rate->wasIdr = rate->idr;
	if (!picture->constantQuantizer) {
		if (picture->predicted) {
			rateSkipped(picture, rate);
		} else {
			if (rate->idr)
				rateRelax(picture, rate);
			if (rate->idr || rate->periodicIdr)
				rateRestart(picture, rate);
			rateStatistics(picture, rate);
			if (picture->option2 && !rate->periodicIdr && rate->sceneKind) {
				rateRestart(picture, rate);
				rate->periodicIdr = 1;
			}
			rateTarget(picture, rate);
			rateQuantizer(picture, rate);
		}
	} else {
		quantizer = rate->idr ? &picture->quantizer : &picture->quantizer2;
		rate->qp = *quantizer;
		rate->codedQp = *quantizer;
		rateDelays(picture, rate);
	}
	rate->chromaQp = avcChromaQp[(uint8_t)rate->qp];
	if (rate->idr)
		rate->qpLimit = ((const AvcencMotionStats *)picture->rowsMotion)->cost /
			(uint32_t)(picture->widthInMbs * picture->heightInMbs * 4000 / 1200);
	vnzPublicClosePrivate((uint32_t)rate, RATE_STATE_SIZE);
}

/*
 * 0x83c46a: the rate control after the picture: its bits as the settings
 * answer them through the HRD, what they teach the estimates, and the
 * result to the engine; then the picture's last job, once it waits
 */
void avcencJobRateUpdate(uint32_t pictureAddress, uint32_t rateAddress, uint32_t unused,
	uint32_t unused2)
{
	AvcencPicture *picture = (AvcencPicture *)pictureAddress;
	AvcencRate *rate = (AvcencRate *)vnzPublicOpenPrivate(rateAddress, RATE_STATE_SIZE);
	int32_t nalOverflow, vclOverflow, oversized;
	VnzKernelVThreadContext *context;
	uint32_t index;

	rate->headerBits = picture->settings->headerBits;
	rate->sliceBits = picture->settings->sliceBits;
	rate->skipHeaderBits = picture->settings->skipHeaderBits;
	rate->skipSliceBits = picture->settings->skipSliceBits;
	rateHrd(picture, rate);
	if (!picture->constantQuantizer)
		rateLearn(picture, rate);
	oversized = (uint8_t)rate->oversized;
	vclOverflow = (uint8_t)rate->vclOverflow;
	nalOverflow = (uint8_t)rate->nalOverflow;
	rate->restart = 0;
	rate->pictures++;
	vnzPublicClosePrivate((uint32_t)rate, RATE_STATE_SIZE);
	if (vclOverflow)
		vnzThreadSetFlags(picture->engine, ENGINE_FLAG_VCL_OVERFLOW);
	else if (nalOverflow)
		vnzThreadSetFlags(picture->engine, ENGINE_FLAG_NAL_OVERFLOW);
	else if (oversized)
		vnzThreadSetFlags(picture->engine, ENGINE_FLAG_REPLACE);
	else
		vnzThreadSetFlags(picture->engine, ENGINE_FLAG_RATE_DONE);
	index = picture->jobs * 8 + picture->index * 8 + 9;
	context = vnzKernelVThreadContextLock(picture->context);
	if (vnzKernelVThreadJobStateUnlocked(context, index) == 1)
		vnzKernelVThreadJobSignal(context, index);
	vnzKernelVThreadContextUnlock(context);
}

/*
 * the modes jobs: the output's mode arrays opened for the pack, by this
 * core's info in the output info
 */
static inline void modesJob(uint32_t pictureAddress, uint32_t jobInfoAddress)
{
	AvcencPicture *picture = (AvcencPicture *)pictureAddress;
	uint32_t core = ((int32_t)vnzReadControl(VNZ_CR_ID) >> 16) & 7;
	uint32_t mbs, modesSize, chromaSize, differencesSize, modes, chroma, differences;
	AvcencModesInfo *info;
	AvcencJobInfo *jobInfo;
	AvcencOutput *output;

	jobInfo = (AvcencJobInfo *)vnzPublicOpenProtected(jobInfoAddress, 64);
	info = (AvcencModesInfo *)vnzPublicOpenPrivate(jobInfo->outputInfo +
		core * MODES_INFO_STRIDE, 64);
	vnzCacheInvalidateInside((uint32_t)info, 64);
	output = (AvcencOutput *)vnzPublicOpenProtected(jobInfo->output, RECORD_SIZE);
	mbs = output->modesMbs;
	chromaSize = (mbs + 63) & ~63;
	modesSize = (mbs * 16 + 63) & ~63;
	differencesSize = (mbs * 128 + 63) & ~63;
	modes = vnzPublicOpenPrivate(output->modes, modesSize);
	chroma = vnzPublicOpenPrivate(output->chromaModes, chromaSize);
	differences = vnzPublicOpenPrivate(output->differences, differencesSize);
	info->output = (uint32_t)output;
	info->modes = modes;
	info->chromaModes = chroma;
	info->differences = differences;
	modesPack(picture, jobInfo, info);
	vnzPublicClosePrivate(modes, modesSize);
	vnzPublicClosePrivate(chroma, chromaSize);
	vnzPublicClosePrivate(differences, differencesSize);
	vnzPublicCloseProtected((uint32_t)output, RECORD_SIZE);
	vnzCachePurge((uint32_t)info, 64);
	vnzPublicClosePrivate((uint32_t)info, 64);
	vnzPublicCloseProtected((uint32_t)jobInfo, 64);
}

/* 0x83c556: the modes of the picture's first output */
void avcencJobModesFirst(uint32_t pictureAddress, uint32_t jobInfoAddress, uint32_t rate,
	uint32_t unused)
{
	modesJob(pictureAddress, jobInfoAddress);
}

/* 0x83c64c: and of the outputs after it */
void avcencJobModes(uint32_t pictureAddress, uint32_t jobInfoAddress, uint32_t rate,
	uint32_t unused)
{
	modesJob(pictureAddress, jobInfoAddress);
}

/* 0x83c742: the estimates again: the IDR period's bits, the IDR's QP, the HRD's delays */
static void rateRestart(AvcencPicture *picture, AvcencRate *rate)
{
	rateGroup(picture, rate);
	rateEstimate(picture, rate);
	rateDelays(picture, rate);
}

/*
 * 0x83c76e: the output's rows of macroblocks into its arrays: each Intra4x4
 * macroblock's sixteen mode codes and each intra one's chroma mode, each
 * coded inter macroblock's vector differences (one a partition)
 */
static void modesPack(AvcencPicture *picture, AvcencJobInfo *jobInfo, AvcencModesInfo *info)
{
	const AvcencOutput *output = (const AvcencOutput *)info->output;
	const AvcencLayout *layout;
	const AvcencMacroblock *record;
	uint32_t width = picture->widthInMbs, end, row, line, x, count, type, i;
	uint32_t *differences;
	uint8_t *modes, *chroma;

	layout = (const AvcencLayout *)((const VideoAvcPictureInfo *)output->info)->reference;
	end = (uint8_t)output->next;
	row = output->firstRow;
	record = (const AvcencMacroblock *)(picture->macroblocks + width * row * 64);
	for (line = 0; row < end; row++, line++) {
		modes = (uint8_t *)(info->modes + layout->modesPitch * line);
		chroma = (uint8_t *)(info->chromaModes + layout->chromaModesPitch * line);
		differences = (uint32_t *)(info->differences + layout->differencesPitch * line);
		for (x = 0; x < width; x++, record++) {
			type = record->type;
			if ((type & 0x300) == 0x200) {
				if (((type >> 6) & 0xb) == 8) {
					((uint64_t *)modes)[0] = record->modeWords[0];
					((uint64_t *)modes)[1] = record->modeWords[1];
					modes += 16;
				}
				*chroma++ = (type >> 14) & 3;
			} else if (!((type >> 11) & 1)) {
				count = 1 << ((type >> 2) & 3);
				for (i = 0; i < count; i++)
					*differences++ = record->differenceWords[i];
			}
		}
	}
}

/*
 * 0x83c870: the IDR period's bits: a new picture bit rate starts over (the
 * weights halfway back to 1.0), a new frame rate gives the bits per frame
 * again; the period's bits at both picture bit rates, the average picture's,
 * and four fifths of the picture size as its limits
 */
static void rateGroup(AvcencPicture *picture, AvcencRate *rate)
{
	uint32_t groupBits, groupMaxBits;
	uint64_t frameRate;
	int32_t fullness;

	if (rate->pictures && picture->pictureBitRate != rate->pictureBitRate) {
		rate->restart = 1;
		rate->fullness = 0;
		rate->maxQp = 0;
		rate->intraWeight = (rate->intraWeight + Q15) >> 1;
		rate->weight = (rate->weight + Q15) >> 1;
		rate->interWeight = (rate->interWeight + Q15) >> 1;
	}
	if (rate->pictures && (picture->numUnitsInTick != rate->numUnitsInTick ||
	    picture->timeScale != rate->timeScale)) {
		frameRate = ((uint64_t)picture->timeScale << 15) /
			(uint64_t)(picture->numUnitsInTick * 2);
		rate->frameRate = frameRate;
		rate->maxFrameBits = ((uint64_t)picture->maxBitRate << 15) / frameRate;
		rate->frameBits = ((uint64_t)picture->bitRate << 15) / frameRate;
	}
	fullness = rate->fullness;
	rate->pictureBitRate = picture->pictureBitRate;
	groupBits = ((uint64_t)picture->pictureBitRate * picture->idrInterval << 15) /
		(uint64_t)rate->frameRate;
	groupMaxBits = ((uint64_t)picture->pictureMaxBitRate * picture->idrInterval << 15) /
		(uint64_t)rate->frameRate;
	rate->averageBits = ((uint64_t)picture->pictureBitRate << 15) / (uint64_t)rate->frameRate;
	rate->sceneSizeLimit = ((uint64_t)picture->pictureInterval * FOUR_FIFTHS) >> 15;
	rate->groupMaxBitsLeft = groupMaxBits;
	rate->groupBitsLeft = groupBits - fullness;
	rate->sizeLimit = ((uint64_t)picture->pictureInterval * FOUR_FIFTHS) >> 15;
	rate->minimumBits = rate->averageBits >> 3;
	rate->picturesLeft = picture->idrInterval;
	rate->groupPictures = picture->idrInterval;
	rate->groupBits = groupBits;
}

/*
 * 0x83caf6: the IDR's QP: at the start (or after a new bit rate) from the
 * bits a macroblock gets, else from the last period's (the IDR's, or the
 * inter pictures' average less a little, no more than two below the IDR's);
 * the ratio of an IDR's complexity to an inter picture's, from 1.5 to 8
 */
static void rateEstimate(AvcencPicture *picture, AvcencRate *rate)
{
	uint32_t pictures, intra, ratio, inter;
	int32_t qp, average;

	if (!rate->pictures || rate->restart) {
		rateInitialQp(picture, rate);
		rate->historyIndex = 0;
		rate->historyCount = 0;
		rate->sinceScene = 0;
		rate->sceneKind = 0;
		rate->previousSceneKind = 0;
	} else {
		pictures = rate->interPictures;
		if ((int32_t)pictures >= 2)
			average = rate->interQpSum / (pictures - 1);
		else
			average = rate->intraQp;
		average -= ((int32_t)pictures / 15 < 2) ? (int32_t)pictures / 15 : 2;
		if (average >= rate->intraQp + 1)
			average = rate->intraQp + 1;
		qp = rate->intraQp - 2;
		if (qp < average)
			qp = average;
		if (qp > QP_MAX)
			qp = QP_MAX;
		if (qp < QP_MIN)
			qp = QP_MIN;
		rate->initialQp = qp;
		if ((int32_t)pictures >= 2) {
			intra = ((uint64_t)rate->intraComplexity * SIX_FIFTHS) >> 15;
			inter = rate->interComplexity / pictures;
			ratio = inter * 8 < intra ? inter * 8 : intra;
			if (ratio < inter + (inter >> 1))
				ratio = inter + (inter >> 1);
			if (ratio && inter) {
				rate->intraRatio = ((uint64_t)ratio << 15) / inter;
				rate->interRatio = ((uint64_t)inter << 15) / ratio;
			}
		}
	}
	rate->interComplexity = 0;
	rate->intraComplexity = 0;
	rate->interPictures = 0;
	rate->interQpSum = 0;
}

/*
 * 0x83cd62: the HRD's delays (in 90 kHz ticks) for the next IDR: the CPB
 * limits at the bit rates, a second's at most, and once pictures are coded
 * no more than the time to the next removal; the offsets keep the totals
 * of the first picture's
 */
static void rateDelays(AvcencPicture *picture, AvcencRate *rate)
{
	uint32_t maxBitRate = picture->maxBitRate, bitRate = picture->bitRate;
	uint32_t delay, vclDelay, wait, vclWait;
	uint64_t scale = (uint64_t)picture->timeScale << 8;
	uint64_t removal = rate->removal[0], vclRemoval = rate->removal[1];
	uint64_t arrival = rate->finalArrival[0], vclArrival = rate->finalArrival[1];

	delay = rate->maxCpbLimit;
	vclDelay = rate->cpbLimit;
	rateSave(rate);
	delay = (int64_t)((uint64_t)delay * TICKS) / (int64_t)maxBitRate;
	vclDelay = (int64_t)((uint64_t)vclDelay * TICKS) / (int64_t)bitRate;
	if (delay > TICKS) {
		delay = TICKS;
		rate->maxCpbLimit = ((uint64_t)maxBitRate * Q15) >> 15;
	}
	if (vclDelay > TICKS) {
		vclDelay = TICKS;
		rate->cpbLimit = ((uint64_t)bitRate * Q15) >> 15;
	}
	rate->initialCpbRemovalDelay = delay;
	rate->vclInitialCpbRemovalDelay = vclDelay;
	if (!rate->pictures) {
		rate->delayTotal[1] = vclDelay;
		rate->delayTotal[0] = delay;
	}
	if (rate->pictures) {
		wait = (removal - arrival) * TICKS / scale;
		vclWait = (vclRemoval - vclArrival) * TICKS / scale;
		if (wait < delay) {
			delay = wait;
			rate->maxCpbLimit = (((uint64_t)wait << 15) / TICKS * maxBitRate) >> 15;
		}
		if (vclWait < vclDelay) {
			vclDelay = vclWait;
			rate->cpbLimit = (((uint64_t)vclWait << 15) / TICKS * bitRate) >> 15;
		}
		rate->vclInitialCpbRemovalDelay = vclDelay;
		rate->initialCpbRemovalDelay = delay;
		rate->delay[0] = (uint64_t)delay * scale / TICKS;
		rate->delay[1] = (uint64_t)vclDelay * scale / TICKS;
	} else {
		rate->removal[0] = (uint64_t)delay * scale / TICKS;
		rate->removal[1] = (uint64_t)vclDelay * scale / TICKS;
		rate->delay[0] = rate->removal[0];
		rate->delay[1] = rate->removal[1];
	}
	rate->initialCpbRemovalDelayOffset = 0;
	if (delay < rate->delayTotal[0])
		rate->initialCpbRemovalDelayOffset = rate->delayTotal[0] - delay;
	rate->vclInitialCpbRemovalDelayOffset = 0;
	if (vclDelay < rate->delayTotal[1])
		rate->vclInitialCpbRemovalDelayOffset = rate->delayTotal[1] - vclDelay;
	rate->delayOffset[0] = (uint64_t)rate->initialCpbRemovalDelayOffset * scale / TICKS;
	rate->delayOffset[1] = (uint64_t)rate->vclInitialCpbRemovalDelayOffset * scale / TICKS;
}

/*
 * 0x83d22a: the picture's target bits: the period's left at both bit rates
 * shared by the pictures left (an IDR weighs the ratio), averaged with the
 * share the whole period gives less the buffer's fullness, at least an
 * eighth of the average; more after a scene change, no more than the limit
 */
static void rateTarget(AvcencPicture *picture, AvcencRate *rate)
{
	int32_t bits = rate->groupBitsLeft, maxBits = rate->groupMaxBitsLeft;
	int32_t minimum = rate->minimumBits, buffer = rate->fullness / 64;
	int64_t groupBits = (int64_t)rate->groupBits * Q15, pictures;
	int32_t target, group, limit;

	if (rate->idr) {
		pictures = (int32_t)((rate->picturesLeft - 1) * rate->interRatio + Q15);
		target = ((int64_t)bits * Q15) / pictures;
		maxBits = ((int64_t)maxBits * Q15) / pictures;
		group = groupBits / pictures - buffer;
	} else {
		target = bits / (int32_t)rate->picturesLeft;
		maxBits = maxBits / (int32_t)rate->picturesLeft;
		if (picture->option2)
			group = groupBits / (int32_t)((uint32_t)picture->idrInterval << 15) - buffer;
		else
			group = groupBits / (int32_t)((((uint32_t)picture->idrInterval - 1) << 15) +
				rate->intraRatio);
	}
	target = ((int64_t)target + group + 1) >> 1;
	if (target > maxBits)
		target = maxBits;
	if (target < minimum)
		target = minimum;
	if (picture->option2) {
		if ((uint8_t)rate->sceneKind == 1)
			target <<= 2;
		else if ((uint8_t)rate->sceneKind == 2)
			target <<= 1;
	}
	limit = rateLimit(picture, rate);
	if (limit < target) {
		target = limit;
		rate->limited = 1;
	} else {
		rate->limited = 0;
	}
	rate->targetBits = target;
}

/*
 * 0x83d3a4: the most bits a picture may take: four fifths of the smaller
 * CPB limit, the picture bits' limit and the size limit (four fifths again
 * at the start or after a new bit rate)
 */
static int32_t rateLimit(AvcencPicture *picture, AvcencRate *rate)
{
	uint32_t cpb = rate->cpbLimit, size;
	int32_t limit;

	if (rate->maxCpbLimit < cpb)
		cpb = rate->maxCpbLimit;
	limit = ((int64_t)(int32_t)cpb * FOUR_FIFTHS) >> 15;
	if (limit > (int32_t)rate->pictureBitsLimit)
		limit = rate->pictureBitsLimit;
	if (rate->pictures && rate->sceneChange)
		size = rate->sceneSizeLimit;
	else
		size = rate->sizeLimit;
	if (size && (int32_t)size < limit)
		limit = size;
	if (!rate->pictures || rate->restart)
		limit = ((int64_t)limit * FOUR_FIFTHS) >> 15;
	return limit;
}

/* the weight of the picture's cost: an IDR's, else its macroblocks' (skipped ones aside) */
static inline uint32_t rateWeight(AvcencRate *rate)
{
	int32_t intra = rate->intraMbs, inter = rate->interMbs, mbs = intra + inter;

	if (rate->idr) {
		rate->weight = rate->intraWeight;
		return rate->weight;
	}
	rate->weight = rate->interWeight;
	inter -= rate->skipCount;
	mbs -= rate->skipCount;
	if (mbs >= 1)
		rate->weight = (rate->interWeight * inter + intra * rate->intraWeight) / mbs;
	return rate->weight;
}

/* a QP within 16 to 51 */
static inline int32_t clampQp(int32_t qp)
{
	if (qp > QP_MAX)
		return QP_MAX;
	if (qp < QP_MIN + 1)
		return QP_MIN;
	return qp;
}

/*
 * 0x83d42c: the picture's QP: the step its cost takes to the target's bits
 * (past the modes' bits), no lower than the period's inter average late in
 * a long period, one higher where the limit is passed, and within a few
 * of the last picture's (an IDR's: the estimate's); the bits it gives
 */
static void rateQuantizer(AvcencPicture *picture, AvcencRate *rate)
{
	uint32_t weight = rateWeight(rate), modeBits, target, cost, bits, step, higherStep, limit;
	int32_t qp, higher, lower, reference, up, down;
	uint64_t complexity, estimate;

	modeBits = rate->modeBits;
	target = rate->targetBits;
	rate->maxQp = 0;
	cost = rate->cost;
	if (modeBits < target && !rate->idr && picture->option2 && !rate->sceneKind) {
		bits = modeBits + (uint32_t)((uint64_t)weight * cost / rate->quantizerStep);
		if (bits >= target + (target >> 1))
			target += target >> 1;
		else if (bits > target >> 1)
			target = bits;
		else
			target >>= 1;
	}
	if (modeBits >= target) {
		rateQuantizerMax(picture, rate);
		return;
	}
	complexity = (uint64_t)weight * cost;
	estimate = complexity / (target - modeBits);
	qp = qpFromStep(estimate > STEP_LIMIT ? STEP_LIMIT : (uint32_t)estimate);
	if (rate->groupPictures >= 15 && !picture->option2 &&
	    rate->groupPictures >> 2 >= rate->picturesLeft) {
		reference = rate->interQpSum / (uint32_t)(rate->groupPictures - rate->picturesLeft - 1);
		if (qp < reference)
			qp = reference;
	}
	step = stepFromQp(qp);
	limit = rateLimit(picture, rate);
	bits = modeBits + (uint32_t)(complexity / step);
	if (limit < bits) {
		qp = clampQp(qp + 1);
		step = stepFromQp(qp);
		bits = modeBits + (uint32_t)(complexity / step);
		rate->limited = 1;
	}
	rate->targetBits = bits;

	if (!rate->pictures) {
		up = 2;
		down = 6;
	} else if (rate->idr) {
		up = 2;
		down = 3;
	} else if (rate->sceneKind) {
		up = 2;
		down = rate->previousQp < 40 ? 4 : 6;
	} else {
		up = 2;
		down = 1;
		if (rate->picturesLeft == picture->idrInterval - 1) {
			if (!picture->option2) {
				down = 2;
				up = 5;
			}
		}
	}
	reference = rate->idr ? (uint8_t)rate->initialQp : rate->previousQp;
	lower = reference - down;
	if (qp < lower) {
		qp = clampQp(lower);
		step = stepFromQp(qp);
		rate->targetBits = modeBits + (uint32_t)(complexity / step);
	} else if (!rate->limited) {
		higher = reference + up;
		if (higher < qp) {
			higher = clampQp(higher);
			higherStep = stepFromQp(higher);
			bits = modeBits + (uint32_t)(complexity / higherStep);
			if (bits <= limit) {
				qp = higher;
				step = higherStep;
				rate->targetBits = bits;
			}
		}
	}
	qp = clampQp(qp);
	rate->quantizerStep = step;
	rate->qp = qp;
	rate->maxQp = 0;
	rate->codedQp = (uint8_t)rate->qp;
	rate->chosenQp = (uint8_t)rate->qp;
	if (rate->idr)
		rate->intraQp = qp;
	else
		rate->interQpSum += qp;
	rate->picturesLeft--;
	rate->interPictures++;
}

/* 0x83d786: QP 51, when the modes' bits leave none for the cost */
static void rateQuantizerMax(AvcencPicture *picture, AvcencRate *rate)
{
	uint32_t weight = rateWeight(rate), modeBits = rate->modeBits, cost = rate->cost, step;
	uint64_t estimate;

	step = stepFromQp(QP_MAX);
	estimate = (uint64_t)weight * cost / step;
	rate->quantizerStep = step;
	rate->qp = QP_MAX;
	rate->maxQp = 1;
	rate->targetBits = modeBits + (uint32_t)estimate;
	rate->codedQp = (uint8_t)rate->qp;
	rate->chosenQp = (uint8_t)rate->qp;
	if (rate->idr)
		rate->intraQp = QP_MAX;
	else
		rate->interQpSum += QP_MAX;
	rate->picturesLeft--;
	rate->interPictures++;
}

/* a skipped picture's bits in place of the picture's, and the times before it */
static inline void rateSkipBits(AvcencRate *rate, uint32_t *bits, uint32_t *sliceBits)
{
	*bits = rate->skipHeaderBits + rate->skipSliceBits;
	*sliceBits = rate->skipSliceBits;
	rate->headerBits = rate->skipHeaderBits;
	rate->sliceBits = rate->skipSliceBits;
	if (rate->wasIdr)
		rateRestore(rate);
}

/*
 * 0x83d86e: the picture through the HRDs: which limits its bits passed (a
 * too large picture is replaced by a skipped one, unless the option allows
 * it), its bits' arrival, the next removal and the next CPB limits; an
 * overflow makes the next picture an IDR. With option2, an inter picture's
 * bits and cost go to the history the scene check reads
 */
static void rateHrd(AvcencPicture *picture, AvcencRate *rate)
{
	uint32_t maxCpbSize = picture->maxCpbSize, cpbSize = picture->cpbSize;
	uint32_t maxLimit = rate->maxCpbLimit, limit = rate->cpbLimit;
	uint32_t sliceBits = rate->sliceBits, bits = sliceBits + rate->headerBits, index, count;
	uint64_t scale = (uint64_t)picture->timeScale << 8, tick, arrival, removal, start;

	rate->vclOverflow = 0;
	rate->nalOverflow = 0;
	rate->oversized = 0;
	rate->pictureOverflow = 0;
	if (rate->pictureBits < bits)
		rate->pictureOverflow = 1;
	if (maxLimit < bits || rate->pictureOverflow) {
		rate->nalOverflow = 1;
		if (!picture->option || picture->option2)
			bits = maxLimit;
	}
	if (limit < sliceBits) {
		rate->vclOverflow = 1;
		if (!picture->option || picture->option2)
			sliceBits = limit;
	}
	if (picture->pictureInterval < bits)
		rate->oversized = 1;
	if (!picture->option2) {
		if (rate->oversized)
			rateSkipBits(rate, &bits, &sliceBits);
		if ((rate->nalOverflow || rate->vclOverflow || rate->oversized) &&
		    (picture->option || rate->pictureOverflow))
			rateSkipBits(rate, &bits, &sliceBits);
	}

	tick = (uint64_t)picture->numUnitsInTick << 9;
	arrival = rate->initialArrival[0] + (uint64_t)bits * scale / picture->maxBitRate;
	removal = rate->removal[0] + tick;
	start = removal - rate->delayOffset[0] - rate->delay[0];
	if (start < arrival)
		start = arrival;
	maxLimit = (removal - start) * picture->maxBitRate / scale;
	if (maxLimit > maxCpbSize)
		maxLimit = maxCpbSize;
	rate->finalArrival[0] = arrival;
	rate->initialArrival[0] = start;
	rate->removal[0] = removal;

	arrival = rate->initialArrival[1] + (uint64_t)sliceBits * scale / picture->bitRate;
	removal = rate->removal[1] + tick;
	start = removal - rate->delayOffset[1] - rate->delay[1];
	if (start < arrival)
		start = arrival;
	limit = (removal - start) * picture->bitRate / scale;
	if (limit > cpbSize)
		limit = cpbSize;
	rate->finalArrival[1] = arrival;
	rate->initialArrival[1] = start;
	rate->removal[1] = removal;
	rate->cpbLimit = limit;
	rate->maxCpbLimit = maxLimit;

	if (((picture->option || rate->pictureOverflow) && !rate->idr &&
	    (rate->nalOverflow || rate->vclOverflow || rate->oversized) && picture->option2 != 1) ||
	    rate->oversized) {
		rate->skipNext = 1;
		rate->idr = 1;
	}
	if (picture->option2 && !rate->wasIdr) {
		index = rate->historyIndex;
		count = rate->historyCount;
		if ((uint64_t)rate->quantizerStep * sliceBits /
		    (uint64_t)(int64_t)(picture->widthInMbs * picture->heightInMbs) > 300) {
			rate->historyBits[index] = sliceBits;
			rate->historyCost[index] = rate->cost;
			rate->historyIndex = (index + 1) & 15;
			if ((int32_t)count < 16)
				rate->historyCount++;
		}
		if (!rate->sceneKind)
			rate->sinceScene++;
	}
}

/*
 * 0x83dd44: what the picture's bits teach: the period's bits less them, the
 * fullness within half the CPB; unless it was skipped, the weight its bits
 * give (an inter picture's, its inter macroblocks', from 0.1 to 3.0) into
 * the IDR's or the inter one, and its complexity at a step of 1.0
 */
static void rateLearn(AvcencPicture *picture, AvcencRate *rate)
{
	uint32_t modeBits = rate->modeBits, skip = rate->skipCount, header = rate->headerBits;
	uint32_t slice = rate->sliceBits, bits = rate->headerBits + slice, cost, step, weight;
	int32_t intra = rate->intraMbs, inter, mbs, half, fullness, coded;
	uint32_t complexity;

	rate->groupBitsLeft -= bits;
	rate->groupMaxBitsLeft -= bits;
	rate->previousQp = rate->qp;
	cost = rate->cost;
	rate->fullness += bits - rate->averageBits;
	inter = rate->interMbs;
	mbs = intra + inter;
	half = picture->cpbSize >> 1;
	fullness = rate->fullness;
	if (fullness < -half)
		fullness = -half;
	if (fullness > half)
		fullness = half;
	rate->fullness = fullness;
	if (rate->skipHeaderBits == header && rate->skipSliceBits == rate->sliceBits)
		return;
	if ((uint8_t)rate->qp != (uint8_t)rate->chosenQp)
		step = stepFromQp((uint8_t)rate->chosenQp);
	else
		step = rate->quantizerStep;
	weight = rate->weight;
	if (cost && modeBits < slice)
		weight = (uint64_t)(slice - modeBits) * step / cost;
	if (!rate->wasIdr) {
		coded = inter - skip;
		if (rate->intraMbs * rate->intraWeight < (mbs - skip) * weight && coded >= 1)
			weight = ((mbs - skip) * weight - rate->intraMbs * rate->intraWeight) /
				(uint32_t)coded;
	}
	if (weight < WEIGHT_MIN)
		weight = WEIGHT_MIN;
	if (weight > WEIGHT_MAX)
		weight = WEIGHT_MAX;
	if ((int32_t)(mbs - skip) >= 9) {
		if (!rate->pictures) {
			rate->interWeight = weight;
			rate->intraWeight = weight;
		} else if (rate->wasIdr) {
			rate->intraWeight = (rate->intraWeight + weight) >> 1;
		} else {
			rate->interWeight = (rate->interWeight + weight) >> 1;
		}
	}
	if (!mbs)
		return;
	complexity = ((uint64_t)slice * step / (uint64_t)(int64_t)mbs) >> 15;
	if (rate->wasIdr)
		rate->intraComplexity = complexity;
	else
		rate->interComplexity += complexity;
}

/* 0x83dee6: a skipped picture: its bits through the HRDs and into the fullness */
static void rateSkipped(AvcencPicture *picture, AvcencRate *rate)
{
	rate->headerBits = picture->settings->skipHeaderBits;
	rate->sliceBits = picture->settings->skipSliceBits;
	rateHrd(picture, rate);
	rate->fullness += rate->headerBits + rate->sliceBits - rate->averageBits;
	rate->pictures++;
}

/* 0x83df36: at an IDR, the weights and ratios halfway back to their start */
static void rateRelax(AvcencPicture *picture, AvcencRate *rate)
{
	rate->intraWeight = (rate->intraWeight + Q15) >> 1;
	rate->weight = (rate->weight + Q15) >> 1;
	rate->intraRatio = (rate->intraRatio + INTRA_RATIO) >> 1;
	rate->interWeight = (rate->interWeight + Q15) >> 1;
	rate->interRatio = (rate->interRatio + INTER_RATIO) >> 1;
}

/* 0x83df7e: the limits, delays and times kept before an IDR's restart */
static void rateSave(AvcencRate *rate)
{
	rate->savedLimits[0] = rate->maxCpbLimit;
	rate->savedLimits[1] = rate->cpbLimit;
	rate->savedDelays[0] = rate->initialCpbRemovalDelay;
	rate->savedDelays[1] = rate->vclInitialCpbRemovalDelay;
	rate->savedDelays[2] = rate->initialCpbRemovalDelayOffset;
	rate->savedDelays[3] = rate->vclInitialCpbRemovalDelayOffset;
	rate->savedRemoval[0] = rate->removal[0];
	rate->savedRemoval[1] = rate->removal[1];
	rate->savedDelay[0] = rate->delay[0];
	rate->savedDelay[1] = rate->delay[1];
	rate->savedDelayOffset[0] = rate->delayOffset[0];
	rate->savedDelayOffset[1] = rate->delayOffset[1];
}

/* 0x83e02a: and back, when the IDR was replaced by a skipped picture */
static void rateRestore(AvcencRate *rate)
{
	rate->maxCpbLimit = rate->savedLimits[0];
	rate->cpbLimit = rate->savedLimits[1];
	rate->initialCpbRemovalDelay = rate->savedDelays[0];
	rate->vclInitialCpbRemovalDelay = rate->savedDelays[1];
	rate->initialCpbRemovalDelayOffset = rate->savedDelays[2];
	rate->vclInitialCpbRemovalDelayOffset = rate->savedDelays[3];
	rate->removal[0] = rate->savedRemoval[0];
	rate->removal[1] = rate->savedRemoval[1];
	rate->delay[0] = rate->savedDelay[0];
	rate->delay[1] = rate->savedDelay[1];
	rate->delayOffset[0] = rate->savedDelayOffset[0];
	rate->delayOffset[1] = rate->savedDelayOffset[1];
}

/*
 * 0x83e0d6: the motion job's statistics; with option2 and a full history, a
 * cost well over the history's mean (and over 1000 a macroblock) is a scene
 * change, a smaller rise long after the last (over 800) a smaller one; the
 * history then holds the mean of the two
 */
static void rateStatistics(AvcencPicture *picture, AvcencRate *rate)
{
	const AvcencMotionStats *stats = (const AvcencMotionStats *)picture->rowsMotion;
	uint32_t cost, perMb, sum, mean, middle, i;
	int8_t kind;

	rate->cost = stats->cost;
	rate->modeBits = stats->bits;
	rate->interMbs = picture->widthInMbs * picture->heightInMbs - stats->intraCount;
	rate->intraMbs = stats->intraCount;
	kind = rate->sceneKind;
	rate->interCount = stats->interCount;
	rate->sceneKind = 0;
	rate->previousSceneKind = kind;
	rate->skipCount = stats->skipCount;
	if (!picture->option2 || (int8_t)rate->historyCount != 16)
		return;
	cost = rate->cost;
	perMb = cost / (uint32_t)(picture->widthInMbs * picture->heightInMbs);
	for (sum = 0, i = 0; i < 16; i++)
		sum += rate->historyCost[i];
	mean = sum >> 4;
	middle = (mean + cost) >> 1;
	if (mean + (mean >> 1) < cost && perMb >= 1001) {
		rate->sceneKind = 1;
		rate->sinceScene = 0;
		for (i = 0; i < 16; i++)
			rate->historyCost[i] = middle;
		return;
	}
	if (rate->sinceScene >= 64 && mean + (mean >> 2) < cost && perMb >= 801) {
		rate->sceneKind = 2;
		rate->sinceScene = 0;
		for (i = 0; i < 16; i++)
			rate->historyCost[i] = middle;
	}
}

/* 0x83e1c4: the QP of a step (in Q15): six an octave, the nearest of the six */
static int32_t qpFromStep(int32_t step)
{
	int32_t octave = 0, index, qp;

	if (step > STEP_LIMIT)
		step = STEP_LIMIT;
	if (step < 0)
		step = 0;
	while (step >= 0x9001) {
		octave++;
		step >>= 1;
	}
	if (step < 21505)
		index = 0;
	else if (step < 24577)
		index = 1;
	else if (step < 27649)
		index = 2;
	else if (step < 30721)
		index = 3;
	else
		index = step < 0x8801 ? 4 : 5;
	qp = octave * 6 + index;
	if (qp > QP_MAX)
		qp = QP_MAX;
	if (qp < QP_MIN)
		qp = QP_MIN;
	return qp;
}

/* 0x83e230: a QP's step, in Q15 */
static uint32_t stepFromQp(int32_t qp)
{
	uint32_t step;
	int32_t i;

	if (qp > QP_MAX)
		qp = QP_MAX;
	if (qp < QP_MIN)
		qp = QP_MIN;
	step = quantizerSteps[qp % 6];
	for (i = 0; i < qp / 6; i++)
		step <<= 1;
	return step;
}
