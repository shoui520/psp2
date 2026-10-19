/*
 * The AVC encoder's headers (0x862f28-0x86432c): the bit writer over the
 * stream's buffer (with emulation prevention), the NAL units' list, the
 * sequence and picture parameter sets, access unit delimiters, SEI
 * messages, VUI and HRD parameters, and slice headers. The unit writes the
 * slices' data; the functions on it hand it the stream and take it back.
 */
#include "service.h"

#define PICTURE_REFERENCE_SIZE	64
#define BLOCK_SIZE		512

#define JUMP_OFFSET		164	/* of the job's jump */

#define NAL_REF_IDC		2

#define EXTENDED_SAR		255

#define SLICE_TYPE_P		0
#define SLICE_TYPE_I		2
#define SLICE_TYPE_P_ALL	5

#define SEI_BUFFERING_PERIOD	0
#define SEI_PIC_TIMING		1
#define SEI_PAN_SCAN		2
#define SEI_RECOVERY_POINT	6

/* the unit's commands */
#define UNIT_FINISH		5
#define UNIT_FLUSH		18
#define UNIT_FLAG		0x40000	/* the unit's interrupt, on the helper */

/* the bit writer's other half (with the encoder's VLIW code) */
uint32_t avcencBitsAligned(VideoAvcWriter *writer);			/* 0x874784 */
void avcencBitsUe(VideoAvcWriter *writer, uint32_t value);		/* 0x87478c */
void avcencBitsSe(VideoAvcWriter *writer, int32_t value);		/* 0x8747d4 */

/* the slice header the unit writes, and CABAC's */

void avcencBitsPut(VideoAvcWriter *writer, int32_t count, uint32_t value);
void avcencBitsPutByte(VideoAvcWriter *writer, uint32_t byte);

/* the clock timestamps of each pic_struct */
static const int8_t numClockTs[9] = { 1, 1, 1, 2, 2, 3, 3, 2, 3 };

static void overflow(const VideoAvcWriter *writer)
{
	videoLongJump((uint32_t *)(writer->job + JUMP_OFFSET), WRITER_OVERFLOW);
}

/* a NAL unit starts here in the list, and ends */
static inline void nalBegin(VideoAvcEncodeJob *job, uint32_t type, uint32_t refIdc)
{
	VideoAvcNalList *nals = job->nals;
	VideoAvcWriter *writer = job->writer;

	nals->type[nals->count] = type;
	nals->refIdc[nals->count] = refIdc;
	nals->start[nals->count] = job->stream->offset + (writer->position - writer->start);
}

static inline void nalEnd(VideoAvcNalList *nals, const VideoAvcWriter *writer, uint32_t offset)
{
	nals->size[nals->count] = offset + (writer->position - writer->start) -
		nals->start[nals->count];
	nals->count++;
}

/* the writer at the stream's position; the bit position kept or not */
void avcencWriterBegin(const VideoAvcOptions *options, VideoAvcEncodeJob *job, uint32_t keepBits)
{
	VideoAvcWriter *stream = job->stream;
	VideoAvcWriter *writer = job->writer;

	writer->start = stream->buffer;
	writer->end = stream->buffer + stream->bufferSize - 1;
	writer->job = (uint32_t)job;
	writer->reserved = 0;
	writer->emulations = 0;
	writer->position = stream->buffer + (stream->publicPosition - stream->offset);
	writer->history = -1;
	if (!keepBits)
		writer->bitPosition = 0;
}

/* the unit's NAL unit ends */
void avcencNalEnd(const VideoAvcStreamInfo *info, VideoAvcWriter *writer)
{
	VideoAvcNalList *nals;

	writer->length = writer->publicPosition - writer->publicBase;
	nals = (VideoAvcNalList *)vnzPublicOpenPrivate(info->nals, NALS_SIZE);
	nalEnd(nals, writer, writer->offset);
	vnzPublicClosePrivate((uint32_t)nals, NALS_SIZE);
}

/* cabac_alignment_one_bit */
void avcencWriterAlignOnes(VideoAvcWriter *writer)
{
	while (writer->bitPosition & 7)
		avcencBitsPut(writer, 1, 1);
}

/* cabac_zero_word, as many as the bins ask */
void avcencCabacZeroWords(const VideoAvcStreamInfo *info, VideoAvcEncodeJob *job2)
{
	VideoAvcWriter *writer = job2->writer;
	VideoAvcWriter *stream = job2->stream;
	VideoAvcNalList *nals;
	int32_t bytes = 0, words;
	uint32_t i;

	nals = (VideoAvcNalList *)vnzPublicOpenPrivate(info->nals, NALS_SIZE);
	for (i = 0; i < nals->count; i++) {
		if (nals->type[i] == NAL_SLICE || nals->type[i] == NAL_IDR)
			bytes += nals->size[i];
	}
	bytes += stream->offset + (writer->position - writer->start) - nals->start[nals->count];
	words = ((int32_t)(job2->binCount * 32 - stream->macroblocks * 3072) * 3 / 1024 - bytes) / 3;
	for (; words > 0; words--)
		avcencBitsPut(writer, 16, 0);
	vnzPublicClosePrivate((uint32_t)nals, NALS_SIZE);
}

/* the unit takes the stream at the writer's position, and its partial byte */
void avcencUnitLoad(VideoHelperShared *shared, const VideoAvcStreamInfo *info,
	VideoAvcWriter *writer)
{
	uint32_t descriptor[4];
	uint32_t *table;
	int32_t bitPosition = writer->bitPosition;
	int32_t i;

	descriptor[3] = info->size / 2 - (writer->offset - writer->publicBase) -
		(writer->position - writer->start);
	descriptor[1] = 0;
	descriptor[0] = 0;
	if (shared->publicStream)
		descriptor[2] = vnzToPublic(writer->position);
	else
		descriptor[2] = vnzToUncached(writer->position);
	table = (uint32_t *)shared->descriptors;
	for (i = 0; i < 4; i++)
		table[i] = descriptor[i];
	VIDEO_UNIT[VIDEO_UNIT_DESCRIPTORS] = shared->descriptors;
	VIDEO_UNIT[VIDEO_UNIT_COMMAND] = VIDEO_UNIT_LOAD;
	vnzThreadWaitFlags(UNIT_FLAG);
	if (bitPosition) {
		VIDEO_UNIT[VIDEO_UNIT_DESCRIPTORS] = writer->pendingBits >> (8 - bitPosition);
		VIDEO_UNIT[VIDEO_UNIT_COMMAND] = ((bitPosition - 1) & 0xff) << 8;
		vnzThreadWaitFlags(UNIT_FLAG);
	}
	VIDEO_UNIT[VIDEO_UNIT_BITS] = 0;
}

/* the unit is done with the stream: the writer after what it wrote */
void avcencUnitFinish(VideoHelperShared *shared, const VideoAvcStreamInfo *info,
	VideoAvcWriter *writer)
{
	VideoAvcNalList *nals;
	uint32_t bits;

	VIDEO_UNIT[VIDEO_UNIT_COMMAND] = UNIT_FLUSH;
	vnzThreadWaitFlags(UNIT_FLAG);
	VIDEO_UNIT[VIDEO_UNIT_COMMAND] = UNIT_FINISH;
	vnzThreadWaitFlags(UNIT_FLAG);
	bits = VIDEO_UNIT[VIDEO_UNIT_BITS];
	writer->bitPosition = 0;
	writer->position += (bits + 7) >> 3;
	writer->job = writer->end - writer->position;
	nals = (VideoAvcNalList *)vnzPublicOpenPrivate(info->nals, NALS_SIZE);
	nalEnd(nals, writer, writer->offset);
	vnzPublicClosePrivate((uint32_t)nals, NALS_SIZE);
	writer->publicPosition = vnzToPublic(writer->position);
	writer->length = writer->publicPosition - writer->publicBase;
}

/* a NAL unit the unit wrote ends */
void avcencUnitNalEnd(VideoHelperShared *shared, const VideoAvcStreamInfo *info,
	VideoAvcWriter *writer)
{
	VideoAvcNalList *nals;

	writer->length = writer->publicPosition - writer->publicBase;
	nals = (VideoAvcNalList *)vnzPublicOpenPrivate(info->nals, NALS_SIZE);
	nalEnd(nals, writer, writer->offset);
	vnzPublicClosePrivate((uint32_t)nals, NALS_SIZE);
}

/* the four-byte start code */
void avcencWriteStartCode(VideoAvcWriter *writer)
{
	uint8_t *position = (uint8_t *)writer->position;

	position[0] = 0;
	position[1] = 0;
	position[2] = 0;
	position[3] = 1;
	writer->history = 1;
	writer->position = (uint32_t)(position + 4);
}

/* count bits of value, the last bit last; 0x03 before 0x000000-0x000003 */
void avcencBitsPut(VideoAvcWriter *writer, int32_t count, uint32_t value)
{
	uint8_t *position = (uint8_t *)writer->position;
	int32_t bitPosition = writer->bitPosition;
	int32_t free = 8 - bitPosition;
	uint32_t byte = (*position >> free) << free;
	uint32_t history, last;
	int32_t left;

	if (count < free) {
		*position = byte | (value << (free - count));
		writer->bitPosition = count + bitPosition;
		return;
	}
	history = writer->history;
	last = (value << ((free - count) & 7)) & 0xff;
	left = count - free;
	byte |= value >> left;
	for (;;) {
		history = ((history & 0xffff) << 8) | byte;
		if (history <= 3) {
			history |= 0x300;
			*position++ = 3;
			writer->emulations++;
		}
		*position++ = byte;
		count -= 8;
		left -= 8;
		if (left < 0)
			break;
		byte = value >> left;
	}
	*position = last;
	if (writer->end < (uint32_t)position + 4)
		overflow(writer);
	writer->position = (uint32_t)position;
	writer->history = history;
	writer->bitPosition = count + bitPosition;
}

/* a word's bytes, the highest first (the coprocessor's byte swap) */
static inline uint8_t *putWord(uint8_t *position, uint32_t word)
{
	position[0] = word >> 24;
	position[1] = word >> 16;
	position[2] = word >> 8;
	position[3] = word;
	return position + 4;
}

/* 512 bytes of words, as they are */
void avcencBitsCopyBlock(VideoAvcWriter *writer, const uint32_t *source)
{
	uint8_t *position;
	int32_t i;

	if (writer->end < writer->position + BLOCK_SIZE)
		overflow(writer);
	position = (uint8_t *)writer->position;
	for (i = 0; i < BLOCK_SIZE / 4; i++)
		position = putWord(position, source[i]);
	writer->position = (uint32_t)position;
}

/* size bytes of words, as they are */
void avcencBitsCopy(VideoAvcWriter *writer, const uint32_t *source, int32_t size)
{
	uint32_t word;
	int32_t i;

	if (writer->end < writer->position + size)
		overflow(writer);
	while (size >= 8) {
		writer->position = (uint32_t)putWord((uint8_t *)writer->position, source[0]);
		writer->position = (uint32_t)putWord((uint8_t *)writer->position, source[1]);
		source += 2;
		writer->job -= 8;
		size -= 8;
	}
	if (size >= 4) {
		writer->position = (uint32_t)putWord((uint8_t *)writer->position, *source++);
		size -= 4;
		writer->job -= 4;
	}
	if (size >= 1) {
		word = *source;
		for (i = 3; i >= 4 - size; i--)
			avcencBitsPutByte(writer, (word >> (i * 8)) & 0xff);
	}
}

void avcencBitsPutByte(VideoAvcWriter *writer, uint32_t byte)
{
	uint8_t *position = (uint8_t *)writer->position;

	*position++ = byte;
	if (writer->end < (uint32_t)position)
		overflow(writer);
	writer->position = (uint32_t)position;
	writer->job--;
}

/* zero bits to the byte's end */
void avcencBitsAlign(VideoAvcWriter *writer)
{
	if (writer->bitPosition)
		avcencBitsPut(writer, 8 - writer->bitPosition, 0);
}

uint32_t avcencBitsEmulations(const VideoAvcWriter *writer)
{
	return writer->emulations;
}

/* rbsp_trailing_bits, and the unit ends */
static void nalTrailing(VideoAvcEncodeJob *job, VideoAvcWriter *writer, uint32_t offset)
{
	avcencBitsPut(writer, 1, 1);
	avcencBitsAlign(writer);
	nalEnd(job->nals, writer, offset);
}

static void nalHeader(VideoAvcWriter *writer, uint32_t refIdc, uint32_t type)
{
	avcencWriteStartCode(writer);
	avcencBitsPut(writer, 3, refIdc);
	avcencBitsPut(writer, 5, type);
}

void avcencWriteVui(const VideoAvcSequence *sequence, const VideoAvcOptions *options,
	VideoAvcEncodeJob *job);

void avcencWriteSps(const VideoAvcSequence *sequence, const VideoAvcOptions *options,
	VideoAvcEncodeJob *job)
{
	VideoAvcWriter *stream = job->stream;
	VideoAvcWriter *writer = job->writer;
	uint32_t profile = sequence->profileIdc;
	uint32_t set0, set1, set2, vui;

	if (profile == PROFILE_BASELINE) {
		set0 = 1;
		set1 = 1;
		set2 = 1;
	} else {
		set1 = 1;
		set0 = 0;
		set2 = 0;
	}
	nalBegin(job, NAL_SPS, NAL_REF_IDC);
	nalHeader(writer, NAL_REF_IDC, NAL_SPS);
	avcencBitsPut(writer, 8, profile);
	avcencBitsPut(writer, 1, set0);
	avcencBitsPut(writer, 1, set1);
	avcencBitsPut(writer, 1, set2);
	avcencBitsPut(writer, 1, 0);		/* constraint_set3_flag */
	avcencBitsPut(writer, 4, 0);		/* reserved_zero_4bits */
	avcencBitsPut(writer, 8, sequence->levelIdc);
	avcencBitsUe(writer, 0);		/* seq_parameter_set_id */
	avcencBitsUe(writer, stream->log2MaxFrameNum - 4);
	avcencBitsUe(writer, 2);		/* pic_order_cnt_type */
	avcencBitsUe(writer, 1);		/* num_ref_frames */
	avcencBitsPut(writer, 1, 0);		/* gaps_in_frame_num_value_allowed_flag */
	avcencBitsUe(writer, stream->widthInMacroblocks - 1);
	avcencBitsUe(writer, stream->heightInMacroblocks - 1);
	avcencBitsPut(writer, 1, 1);		/* frame_mbs_only_flag */
	avcencBitsPut(writer, 1, 1);		/* direct_8x8_inference_flag */
	avcencBitsPut(writer, 1, sequence->frameCropping);
	if (sequence->frameCropping) {
		avcencBitsUe(writer, 0);
		avcencBitsUe(writer, sequence->cropRight);
		avcencBitsUe(writer, 0);
		avcencBitsUe(writer, sequence->cropBottom);
	}
	vui = !options->noVui;
	avcencBitsPut(writer, 1, vui);
	if (vui)
		avcencWriteVui(sequence, options, job);
	nalTrailing(job, writer, stream->offset);
}

void avcencWritePps(const VideoAvcSequence *sequence, const VideoAvcOptions *options,
	VideoAvcEncodeJob *job)
{
	VideoAvcWriter *stream = job->stream;
	VideoAvcWriter *writer = job->writer;
	uint32_t cabac = (uint8_t)options->cabac;

	nalBegin(job, NAL_PPS, NAL_REF_IDC);
	nalHeader(writer, NAL_REF_IDC, NAL_PPS);
	avcencBitsUe(writer, 0);		/* pic_parameter_set_id */
	avcencBitsUe(writer, 0);		/* seq_parameter_set_id */
	avcencBitsPut(writer, 1, cabac);	/* entropy_coding_mode_flag */
	avcencBitsPut(writer, 1, 0);		/* pic_order_present_flag */
	avcencBitsUe(writer, 0);		/* num_slice_groups_minus1 */
	avcencBitsUe(writer, 0);		/* num_ref_idx_l0_active_minus1 */
	avcencBitsUe(writer, 0);		/* num_ref_idx_l1_active_minus1 */
	avcencBitsPut(writer, 1, 0);		/* weighted_pred_flag */
	avcencBitsPut(writer, 2, 0);		/* weighted_bipred_idc */
	avcencBitsSe(writer, 0);		/* pic_init_qp_minus26 */
	avcencBitsSe(writer, 0);		/* pic_init_qs_minus26 */
	avcencBitsSe(writer, 0);		/* chroma_qp_index_offset */
	avcencBitsPut(writer, 1, 1);		/* deblocking_filter_control_present_flag */
	avcencBitsPut(writer, 1, 0);		/* constrained_intra_pred_flag */
	avcencBitsPut(writer, 1, 0);		/* redundant_pic_cnt_present_flag */
	nalTrailing(job, writer, stream->offset);
}

/* primary_pic_type: I only for an IDR picture not forced to I, P */
void avcencWriteAud(const VideoAvcOptions *options, VideoAvcEncodeJob *job, uint32_t force)
{
	VideoAvcWriter *stream = job->stream;
	VideoAvcWriter *writer = job->writer;
	uint32_t primaryPicType;

	primaryPicType = (!options->idr || force) ? 1 : 0;
	nalBegin(job, NAL_AUD, 0);
	nalHeader(writer, 0, NAL_AUD);
	avcencBitsPut(writer, 3, primaryPicType);
	nalTrailing(job, writer, stream->offset);
}

void avcencWriteSeiMessage(const VideoAvcOptions *options, VideoAvcEncodeJob *job,
	uint32_t type);

static const uint8_t seiTypes[VIDEO_AVC_SEIS] = {
	SEI_BUFFERING_PERIOD, SEI_PIC_TIMING, SEI_PAN_SCAN, SEI_RECOVERY_POINT
};

/* the SEI messages asked: one NAL unit each with CABAC, otherwise one for all */
void avcencWriteSei(const VideoAvcOptions *options, VideoAvcEncodeJob *job)
{
	VideoAvcWriter *stream = job->stream;
	VideoAvcWriter *writer = job->writer;
	int32_t i;

	if (!options->cabac) {
		nalBegin(job, NAL_SEI, 0);
		nalHeader(writer, 0, NAL_SEI);
		for (i = 0; i < VIDEO_AVC_SEIS; i++) {
			if (job->sei[i])
				avcencWriteSeiMessage(options, job, seiTypes[i]);
		}
		nalTrailing(job, writer, stream->offset);
		return;
	}
	for (i = 0; i < VIDEO_AVC_SEIS; i++) {
		if (!job->sei[i])
			continue;
		nalBegin(job, NAL_SEI, 0);
		nalHeader(writer, 0, NAL_SEI);
		avcencWriteSeiMessage(options, job, seiTypes[i]);
		nalTrailing(job, writer, stream->offset);
	}
}

void avcencWriteHrd(const VideoAvcSequence *sequence, const VideoAvcOptions *options,
	VideoAvcEncodeJob *job, uint32_t vcl);

void avcencWriteVui(const VideoAvcSequence *sequence, const VideoAvcOptions *options,
	VideoAvcEncodeJob *job)
{
	VideoAvcWriter *stream = job->stream;
	VideoAvcWriter *writer = job->writer;
	uint32_t aspect, idc, sarWidth, sarHeight;
	uint32_t signal, format, fullRange, colour, primaries, transfer, matrix;
	uint32_t numUnitsInTick, timeScale;

	if (sequence->aspectRatioIdc) {
		sarWidth = sequence->sarWidth;
		idc = sequence->aspectRatioIdc;
		sarHeight = sequence->sarHeight;
		aspect = 1;
	} else {
		aspect = 0;
		sarHeight = 0;
		idc = 0;
		sarWidth = 0;
	}
	avcencBitsPut(writer, 1, aspect);
	if (aspect) {
		avcencBitsPut(writer, 8, idc);
		if (idc == EXTENDED_SAR) {
			avcencBitsPut(writer, 16, sarWidth);
			avcencBitsPut(writer, 16, sarHeight);
		}
	}
	avcencBitsPut(writer, 1, 0);		/* overscan_info_present_flag */
	if (sequence->videoSignalPresent) {
		matrix = sequence->matrixCoefficients;
		transfer = sequence->transferCharacteristics;
		colour = sequence->colourDescriptionPresent;
		primaries = sequence->colourPrimaries;
		fullRange = sequence->videoFullRange;
		format = sequence->videoFormat;
		signal = 1;
	} else {
		signal = 0;
		format = 5;			/* unspecified */
		matrix = 2;
		transfer = 2;
		primaries = 2;
		fullRange = 0;
		colour = 0;
	}
	avcencBitsPut(writer, 1, signal);
	if (signal) {
		avcencBitsPut(writer, 3, format);
		avcencBitsPut(writer, 1, fullRange);
		avcencBitsPut(writer, 1, colour);
		if (colour) {
			avcencBitsPut(writer, 8, primaries);
			avcencBitsPut(writer, 8, transfer);
			avcencBitsPut(writer, 8, matrix);
		}
	}
	avcencBitsPut(writer, 1, 0);		/* chroma_loc_info_present_flag */
	timeScale = sequence->timeScale;
	numUnitsInTick = sequence->numUnitsInTick;
	avcencBitsPut(writer, 1, 1);		/* timing_info_present_flag */
	avcencBitsPut(writer, 32, numUnitsInTick);
	avcencBitsPut(writer, 32, timeScale);
	avcencBitsPut(writer, 1, 1);		/* fixed_frame_rate_flag */
	avcencBitsPut(writer, 1, 1);		/* nal_hrd_parameters_present_flag */
	avcencWriteHrd(sequence, options, job, 0);
	avcencBitsPut(writer, 1, 1);		/* vcl_hrd_parameters_present_flag */
	avcencWriteHrd(sequence, options, job, 1);
	avcencBitsPut(writer, 1, 0);		/* low_delay_hrd_flag */
	stream->picStructPresent = (uint8_t)options->cabac != 0;
	avcencBitsPut(writer, 1, stream->picStructPresent);
	avcencBitsPut(writer, 1, 1);		/* bitstream_restriction_flag */
	avcencBitsPut(writer, 1, 1);		/* motion_vectors_over_pic_boundaries_flag */
	avcencBitsUe(writer, 2);		/* max_bytes_per_pic_denom */
	avcencBitsUe(writer, 1);		/* max_bits_per_mb_denom */
	avcencBitsUe(writer, 7);		/* log2_max_mv_length_horizontal */
	avcencBitsUe(writer, 6);		/* log2_max_mv_length_vertical */
	avcencBitsUe(writer, 0);		/* num_reorder_frames */
	avcencBitsUe(writer, 1);		/* max_dec_frame_buffering */
}

#define DELAY_LENGTH	24

void avcencWriteHrd(const VideoAvcSequence *sequence, const VideoAvcOptions *options,
	VideoAvcEncodeJob *job, uint32_t vcl)
{
	VideoAvcWriter *writer = job->writer;
	VideoAvcWriter *stream = job->stream;
	uint32_t bitRate, cpbSize;

	if (!vcl) {
		bitRate = ((sequence->nalBitRate + 63) >> 6) - 1;
		cpbSize = (sequence->nalCpbSize >> 4) - 1;
	} else {
		bitRate = ((sequence->vclBitRate + 63) >> 6) - 1;
		cpbSize = (sequence->vclCpbSize >> 4) - 1;
	}
	avcencBitsUe(writer, 0);		/* cpb_cnt_minus1 */
	avcencBitsPut(writer, 4, 0);		/* bit_rate_scale */
	avcencBitsPut(writer, 4, 0);		/* cpb_size_scale */
	avcencBitsUe(writer, bitRate);
	avcencBitsUe(writer, cpbSize);
	avcencBitsPut(writer, 1, 0);		/* cbr_flag */
	avcencBitsPut(writer, 5, DELAY_LENGTH - 1);
	avcencBitsPut(writer, 5, DELAY_LENGTH - 1);
	avcencBitsPut(writer, 5, DELAY_LENGTH - 1);
	avcencBitsPut(writer, 5, 0);		/* time_offset_length */
	stream->dpbOutputDelayLength = DELAY_LENGTH;
	stream->cpbRemovalDelayLength = DELAY_LENGTH;
	stream->initialCpbRemovalDelayLength = DELAY_LENGTH;
}

void avcencSeiBufferingPeriod(const VideoAvcOptions *options, VideoAvcEncodeJob *job);
void avcencSeiPicTiming(const VideoAvcOptions *options, VideoAvcEncodeJob *job);
void avcencSeiPanScan(const VideoAvcOptions *options, VideoAvcEncodeJob *job);
void avcencSeiRecoveryPoint(const VideoAvcOptions *options, VideoAvcEncodeJob *job);

/* a message: its type, its size (set once written), its payload */
void avcencWriteSeiMessage(const VideoAvcOptions *options, VideoAvcEncodeJob *job,
	uint32_t type)
{
	VideoAvcWriter *writer = job->writer;
	uint32_t emulations, size;
	uint8_t *sizeByte;

	emulations = avcencBitsEmulations(writer);
	avcencBitsPut(writer, 8, type);
	sizeByte = (uint8_t *)writer->position;
	avcencBitsPut(writer, 8, 255);
	if (type == SEI_BUFFERING_PERIOD)
		avcencSeiBufferingPeriod(options, job);
	else if (type == SEI_PIC_TIMING)
		avcencSeiPicTiming(options, job);
	else if (type == SEI_PAN_SCAN)
		avcencSeiPanScan(options, job);
	else if (type == SEI_RECOVERY_POINT)
		avcencSeiRecoveryPoint(options, job);
	if (!avcencBitsAligned(writer)) {
		avcencBitsPut(writer, 1, 1);	/* bit_equal_to_one */
		while (!avcencBitsAligned(writer))
			avcencBitsPut(writer, 1, 0);
	}
	size = (writer->position - (uint32_t)sizeByte - 1) -
		(avcencBitsEmulations(writer) - emulations);
	*sizeByte = size;
}

void avcencSeiBufferingPeriod(const VideoAvcOptions *options, VideoAvcEncodeJob *job)
{
	const VideoAvcPictureInfo *picture = job->picture;
	VideoAvcWriter *writer = job->writer;
	uint32_t length = job->stream->initialCpbRemovalDelayLength;
	uint32_t vclDelay = picture->vclInitialCpbRemovalDelay;
	uint32_t offset = picture->initialCpbRemovalDelayOffset;
	uint32_t delay = picture->initialCpbRemovalDelay;
	uint32_t vclOffset = picture->vclInitialCpbRemovalDelayOffset;

	avcencBitsUe(writer, 0);		/* seq_parameter_set_id */
	avcencBitsPut(writer, length, delay);
	avcencBitsPut(writer, length, offset);
	avcencBitsPut(writer, length, vclDelay);
	avcencBitsPut(writer, length, vclOffset);
}

void avcencSeiPicTiming(const VideoAvcOptions *options, VideoAvcEncodeJob *job)
{
	VideoAvcWriter *stream = job->stream;
	VideoAvcWriter *writer = job->writer;
	uint32_t dpbLength = stream->dpbOutputDelayLength;
	int32_t i;

	avcencBitsPut(writer, stream->cpbRemovalDelayLength, stream->frameNum * 2);
	avcencBitsPut(writer, dpbLength, 0);	/* dpb_output_delay */
	if (!stream->picStructPresent)
		return;
	avcencBitsPut(writer, 4, 0);		/* pic_struct: a frame */
	for (i = 0; i < numClockTs[0]; i++) {
		avcencBitsPut(writer, 1, 1);	/* clock_timestamp_flag */
		avcencBitsPut(writer, 2, 0);	/* ct_type */
		avcencBitsPut(writer, 1, 0);	/* nuit_field_based_flag */
		avcencBitsPut(writer, 5, 0);	/* counting_type */
		avcencBitsPut(writer, 1, 0);	/* full_timestamp_flag */
		avcencBitsPut(writer, 1, 0);	/* discontinuity_flag */
		avcencBitsPut(writer, 1, 0);	/* cnt_dropped_flag */
		avcencBitsPut(writer, 8, 0);	/* n_frames */
		avcencBitsPut(writer, 1, 0);	/* seconds_flag */
	}
}

void avcencSeiPanScan(const VideoAvcOptions *options, VideoAvcEncodeJob *job)
{
	VideoAvcWriter *writer = job->writer;
	int32_t i;

	avcencBitsUe(writer, 0);		/* pan_scan_rect_id */
	avcencBitsPut(writer, 1, 0);		/* pan_scan_rect_cancel_flag */
	avcencBitsUe(writer, 0);		/* pan_scan_cnt_minus1 */
	for (i = 0; i < 1; i++) {
		avcencBitsSe(writer, 0);	/* the rectangle's offsets */
		avcencBitsSe(writer, 0);
		avcencBitsSe(writer, 0);
		avcencBitsSe(writer, 0);
	}
	avcencBitsUe(writer, 0);		/* pan_scan_rect_repetition_period */
}

void avcencSeiRecoveryPoint(const VideoAvcOptions *options, VideoAvcEncodeJob *job)
{
	VideoAvcWriter *writer = job->writer;

	avcencBitsUe(writer, 0);		/* recovery_frame_cnt */
	avcencBitsPut(writer, 1, 0);		/* exact_match_flag */
	avcencBitsPut(writer, 1, 0);		/* broken_link_flag */
	avcencBitsPut(writer, 2, 0);		/* changing_slice_group_idc */
}

/* the slice header: written here, or by the unit with CABAC */
void avcencWriteSliceHeader(const VideoAvcOptions *options, VideoAvcEncodeJob *job,
	uint32_t firstMb)
{
	const VideoAvcPictureInfo *picture = job->picture;
	VideoAvcWriter *stream = job->stream;
	VideoAvcWriter *writer = job->writer;
	uint32_t reference, type, refIdc, sliceType, log2, frameNum, idrPicId;
	int32_t alpha, beta, qpDelta;

	reference = vnzPublicOpenProtected(picture->reference, PICTURE_REFERENCE_SIZE);
	alpha = picture->alphaOffset;
	qpDelta = picture->qp;
	beta = picture->betaOffset;
	if (options->idr) {
		type = NAL_IDR;
		stream->sliceType = SLICE_TYPE_I;
		refIdc = NAL_REF_IDC;
	} else {
		type = NAL_SLICE;
		refIdc = NAL_REF_IDC;
		stream->sliceType = SLICE_TYPE_P;
	}
	beta >>= 1;
	alpha >>= 1;
	qpDelta -= 26;
	sliceType = stream->sliceType & 7;
	if (type == NAL_IDR && firstMb == 0) {
		stream->idrPicId++;
		stream->frameNum = 0;
	}
	log2 = stream->log2MaxFrameNum;
	frameNum = stream->frameNum;
	idrPicId = stream->idrPicId;
	vnzPublicCloseProtected(reference, PICTURE_REFERENCE_SIZE);
	nalBegin(job, type, refIdc);
	if (options->cabac) {
		avcencCabacBegin(job);
		avcencCabacAlign(job);
		avcencCabacNalHeader(job, ((refIdc << 5) | type) & 0xff);
		avcencCabacSliceHeader(options, job, firstMb);
		avcencCabacSliceInit(job, sliceType);
		return;
	}
	nalHeader(writer, refIdc, type);
	avcencBitsUe(writer, firstMb);
	avcencBitsUe(writer, sliceType);
	avcencBitsUe(writer, 0);		/* pic_parameter_set_id */
	avcencBitsPut(writer, log2, frameNum);
	if (type == NAL_IDR)
		avcencBitsUe(writer, idrPicId);
	if (sliceType == SLICE_TYPE_P) {
		avcencBitsPut(writer, 1, 0);	/* num_ref_idx_active_override_flag */
		avcencBitsPut(writer, 1, 0);	/* ref_pic_list_reordering_flag_l0 */
	}
	if (type == NAL_IDR)
		avcencBitsPut(writer, 1, 0);	/* no_output_of_prior_pics_flag */
	avcencBitsPut(writer, 1, 0);		/* long_term_reference_flag, adaptive_ref_pic_marking_mode_flag */
	avcencBitsSe(writer, qpDelta);
	avcencBitsUe(writer, 0);		/* disable_deblocking_filter_idc */
	avcencBitsSe(writer, alpha);
	avcencBitsSe(writer, beta);
}

/* a P slice of skipped macroblocks from firstMb, whole */
void avcencWriteSkipSlice(const VideoAvcOptions *options, VideoAvcEncodeJob *job,
	uint32_t firstMb)
{
	VideoAvcNalList *nals = job->nals;
	VideoAvcWriter *stream = job->stream;
	VideoAvcWriter *writer = job->writer;
	uint32_t log2 = stream->log2MaxFrameNum;
	uint32_t frameNum = stream->frameNum;
	uint32_t sliceType;

	nalBegin(job, NAL_SLICE, NAL_REF_IDC);
	nalHeader(writer, NAL_REF_IDC, NAL_SLICE);
	avcencBitsUe(writer, firstMb);
	sliceType = options->cabac == 1 ? SLICE_TYPE_P_ALL : SLICE_TYPE_P;
	avcencBitsUe(writer, sliceType);
	avcencBitsUe(writer, 0);		/* pic_parameter_set_id */
	avcencBitsPut(writer, log2, frameNum);
	avcencBitsPut(writer, 1, 0);		/* num_ref_idx_active_override_flag */
	avcencBitsPut(writer, 1, 0);		/* ref_pic_list_reordering_flag_l0 */
	avcencBitsPut(writer, 1, 0);		/* adaptive_ref_pic_marking_mode_flag */
	if (options->cabac)
		avcencBitsUe(writer, 0);	/* cabac_init_idc */
	avcencBitsSe(writer, 0);		/* slice_qp_delta */
	avcencBitsUe(writer, 1);		/* disable_deblocking_filter_idc */
	if (!options->cabac) {
		avcencBitsUe(writer, job->skipRun);
		avcencBitsPut(writer, 1, 1);	/* rbsp_stop_one_bit */
	} else {
		if (sliceType == SLICE_TYPE_P || sliceType == SLICE_TYPE_P_ALL) {
			avcencCabacReset(job, 0);
			avcencCabacInit(job, sliceType);
		}
		while (!avcencBitsAligned(writer))
			avcencBitsPut(writer, 1, 1);	/* cabac_alignment_one_bit */
		avcencCabacSkipped(job, sliceType);
	}
	avcencBitsAlign(writer);
	nalEnd(nals, writer, stream->offset);
	stream->length = writer->position - stream->buffer;
}
