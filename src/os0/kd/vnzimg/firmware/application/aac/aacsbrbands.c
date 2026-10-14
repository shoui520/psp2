/*
 * SBR's frequency band tables (0x853468-0x853a9e, 0x85406c-0x8540da):
 * the master table from the header, then the high, low and noise tables
 * (ISO/IEC 14496-3 4.6.18.3.2), and the Huffman decoder for the deltas
 * sent against them. Logarithms are Q24.
 */
#include "aac.h"

/* log2(i), Q24; 0 has -8 */
static const int32_t log2Table[65] = {
	-134217728, 0, 16777216, 26591258, 33554432, 38955489,
	43368474, 47099600, 50331648, 53182516, 55732705, 58039632,
	60145690, 62083076, 63876816, 65546747, 67108864, 68576247,
	69959732, 71268397, 72509921, 73690858, 74816848, 75892776,
	76922906, 77910978, 78860292, 79773775, 80654032, 81503396,
	82323963, 83117622, 83886080, 84630890, 85353463, 86055089,
	86736948, 87400124, 88045613, 88674335, 89287137, 89884807,
	90468074, 91037616, 91594064, 92138006, 92669992, 93190537,
	93700122, 94199200, 94688194, 95167505, 95637508, 96098559,
	96550991, 96995121, 97431248, 97859656, 98280612, 98694373,
	99101179, 99501261, 99894838, 100282116, 100663296,
};

/* the quotient of non-negative values, by shift and subtract */
int32_t aacSbrDivide(int32_t dividend, int32_t divisor)
{
	int32_t quotient = 0, shift;

	while (dividend >= divisor) {
		shift = 0;
		do
			shift++;
		while (dividend >= divisor << shift);
		dividend -= divisor << (shift - 1);
		quotient += 1 << (shift - 1);
	}
	return quotient;
}

int32_t aacSbrLog2Ratio(int32_t low, int32_t high)
{
	return log2Table[high] - log2Table[low];
}

/* Shell sort, ascending */
void aacSbrSort(uint8_t *values, int32_t count)
{
	int32_t gap = 1, i, j;
	uint8_t value;

	while (gap <= count)
		gap = gap * 3 + 1;
	do {
		gap = aacSbrDivide(gap, 3);
		for (i = gap; i < count; i++) {
			value = values[i];
			for (j = i; j >= gap && value < values[j - gap]; j -= gap)
				values[j] = values[j - gap];
			values[j] = value;
		}
	} while (gap >= 2);
}

/* the band borders from a start and the bands' widths */
static void cumulative(uint8_t start, const uint8_t *widths, int32_t count, uint8_t *borders)
{
	int32_t i;

	borders[0] = start;
	for (i = 0; i < count; i++)
		borders[i + 1] = borders[i] + widths[i];
}

/* evens the second region's first width with the first region's last */
static int32_t modifyBands(int32_t last, uint8_t *widths, int32_t count)
{
	uint8_t change = aacMin(last - widths[0], (widths[count - 1] - widths[0]) / 2);

	widths[0] += change;
	widths[count - 1] -= change;
	aacSbrSort(widths, count);
	return 0;
}

/* 2 * round(bands * log2(stop / start) / 2), warped by 1.3 for bs_alter_scale */
static uint8_t numberOfBands(int32_t halfBands, int32_t start, int32_t stop, int32_t warp)
{
	int64_t wide;
	int32_t value;

	aacFixedMultiply(&wide, aacSbrLog2Ratio(start, stop), halfBands, 56);
	value = aacFixedToInt(&wide, 56);
	if (warp) {
		aacFixedMultiply(&wide, value, 0x6270000, 56);	/* 1/1.3 */
		value = aacFixedToInt(&wide, 56);
	}
	aacFixedAdd(&wide, value, 0x100000, 56);
	return (aacFixedToInt(&wide, 56) >> 21) << 1;
}

/* the first band of bs_stop_freq's table, by output rate */
static int32_t stopMinimum(uint32_t rate)
{
	switch (rate) {
	case 64000:
		return 20;
	case 88200:
		return 15;
	case 96000:
		return 13;
	case 16000:
		return 48;
	case 22050:
		return 35;
	case 24000:
	case 32000:
		return 32;
	case 44100:
		return 23;
	case 48000:
		return 21;
	default:
		return -1;
	}
}

/* f_Master; scratch holds the widths, scratch + 29 the second region's */
static int32_t masterTable(uint8_t *master, uint8_t *numMaster, AacSbrHeader *header,
	uint8_t *scratch)
{
	uint32_t rate = header->outputRate;
	uint8_t *second = scratch + 29, numBands0, numBands1, numBands;
	int32_t k0, k1, k2, minimum, halfBands, dk, diff, step, k, i;

	k0 = aacSbrStartBand(rate, header->startFreq);
	if (header->stopFreq < 14) {
		minimum = stopMinimum(rate);
		if (minimum < 0)
			return -1;
		aacSbrCalcBands(scratch, minimum, 64, 13);
		aacSbrSort(scratch, 13);
		cumulative(minimum, scratch, 13, second);
		k2 = second[header->stopFreq];
	} else if (header->stopFreq == 14) {
		k2 = (uint8_t)(2 * k0);
	} else {
		k2 = (uint8_t)(3 * k0);
	}
	k2 = aacMin(k2, 64);
	if (k2 - k0 >= 49 || k0 >= k2)
		return -1;
	if (rate == 44100 && k2 - k0 >= 36)
		return -1;
	if (rate >= 48000 && k2 - k0 >= 33)
		return -1;

	if (header->freqScale == 0) {
		/* linear: widths of 1 or 2 */
		if (!header->alterScale) {
			dk = 1;
			numBands = (k2 - k0) & 0xfe;
		} else {
			dk = 2;
			numBands = (((k2 - k0) >> 1) + 1) & 0xfe;
		}
		if (numBands < 1)
			return -1;
		for (i = 0; i < numBands; i++)
			scratch[i] = dk;
		diff = k2 - (k0 + numBands * dk);
		step = 0;
		k = 0;
		if (diff < 0) {
			k = 0;
			step = 1;
		}
		if (diff >= 1) {
			step = -1;
			k = numBands - 1;
		}
		while (diff) {
			diff += step;
			scratch[k] -= step;
			k += step;
		}
		cumulative(k0, scratch, numBands, master);
		*numMaster = numBands;
		return 0;
	}

	/* bands per octave: 12, 10 or 8, halved */
	if (header->freqScale == 1)
		halfBands = 0x6000000;
	else if (header->freqScale == 2)
		halfBands = 0x5000000;
	else
		halfBands = 0x4000000;

	if (k0 * 2245 < k2 * 1000) {
		/* two regions, the second from 2 k0 */
		k1 = (uint8_t)(2 * k0);
		numBands0 = numberOfBands(halfBands, k0, k1, 0);
		numBands1 = numberOfBands(halfBands, k1, k2, header->alterScale);
		if (numBands0 < 1 || numBands1 < 1)
			return -1;
		aacSbrCalcBands(scratch, k0, k1, numBands0);
		aacSbrSort(scratch, numBands0);
		if ((int8_t)scratch[0] == 0)
			return -1;
		cumulative(k0, scratch, numBands0, master);
		aacSbrCalcBands(second, k1, k2, numBands1);
		aacSbrSort(second, numBands1);
		if (second[0] < scratch[numBands0 - 1] &&
		    modifyBands(scratch[numBands0 - 1], second, numBands1))
			return -1;
		cumulative(k1, second, numBands1, master + numBands0);
		numBands = numBands0 + numBands1;
	} else {
		numBands = numberOfBands(halfBands, k0, k2, 0);
		if (numBands < 1)
			return -1;
		aacSbrCalcBands(scratch, k0, k2, numBands);
		aacSbrSort(scratch, numBands);
		if ((int8_t)scratch[0] == 0)
			return -1;
		cumulative(k0, scratch, numBands, master);
	}
	*numMaster = numBands;
	return numBands < 1 ? -1 : 0;
}

/* f_TableHigh: the master table from the crossover band */
static void highTable(uint8_t *high, uint8_t *numHigh, const uint8_t *master,
	int32_t numMaster, int32_t xover)
{
	int32_t i;

	*numHigh = numMaster - xover;
	for (i = 0; xover + i <= numMaster; i++)
		high[i] = master[xover + i];
}

/* f_TableLow: every other high band */
static void lowTable(uint8_t *low, uint8_t *numLow, const uint8_t *high, int32_t numHigh)
{
	int32_t i;

	if (!(numHigh & 1)) {
		*numLow = numHigh >> 1;
		for (i = 0; i <= *numLow; i++)
			low[i] = high[2 * i];
	} else {
		*numLow = (numHigh + 1) >> 1;
		low[0] = high[0];
		for (i = 1; i <= *numLow; i++)
			low[i] = high[2 * i - 1];
	}
}

/* f_TableNoise: N_Q bands over the low table */
static void noiseTable(uint8_t *noise, int32_t numNoise, const uint8_t *low, int32_t numLow,
	int32_t *index)
{
	int32_t k = 0, width, i;

	index[0] = 0;
	while (numLow >= 1) {
		k++;
		width = aacSbrDivide(numLow, numNoise);
		numNoise--;
		numLow -= width;
		index[k] = index[k - 1] + width;
	}
	for (i = 0; i <= k; i++)
		noise[i] = low[index[i]];
}

/* all the tables, after a header that changed them */
int32_t aacSbrResetBands(AacSbrHeader *header, int32_t *scratch)
{
	AacSbrBands *bands = header->bands;
	uint8_t numHigh, numLow, numNoise, kx, stop;
	int32_t value;

	if (masterTable(bands->master, &bands->numMaster, header, (uint8_t *)scratch) ||
	    bands->numMaster < header->xoverBand)
		return -1;
	highTable(bands->table[1], &numHigh, bands->master, bands->numMaster,
		header->xoverBand);
	lowTable(bands->table[0], &numLow, bands->table[1], numHigh);
	bands->numLow = numLow;
	bands->numHigh = numHigh;
	if (numLow < 1 || numLow >= 25)
		return -1;
	kx = bands->table[0][0];
	stop = bands->table[0][numLow];
	if (kx >= 33 || kx >= stop)
		return -1;

	if (!header->noiseBands) {
		numNoise = 1;
	} else {
		value = (aacSbrLog2Ratio(bands->table[1][0], bands->table[1][numHigh]) >> 2) *
			header->noiseBands;
		numNoise = (value + 0x200000) >> 22;
		if (!numNoise)
			numNoise = 1;
	}
	bands->numNoise = numNoise;
	if (numNoise >= 6)
		return -1;
	bands->noiseBands = numNoise;
	noiseTable(bands->noise, numNoise, bands->table[0], numLow, scratch);
	bands->stop = stop;
	bands->kx = kx;
	return 0;
}

/* one envelope or noise delta, walking a tree from aacsbrtables.c */
int32_t aacSbrHuffman(const int8_t (*tree)[2], AacSbrBits *bits)
{
	int32_t node = 0;

	do
		node = tree[node][aacSbrReadBits(bits, 1)];
	while (node >= 0);
	return node + 64;
}

/* the limiter bands per octave, Q25 (0x9008b4), and their merging threshold */
static const int32_t limiterBandsPerOctave[4] = { 33554432, 40265320, 67108864, 100663296 };
#define LIMITER_THRESHOLD	0x1f5c28

static int32_t isPatchBorder(uint8_t value, const int32_t *borders, int32_t numPatches)
{
	int32_t i;

	for (i = 0; i <= numPatches; i++)
		if (value == borders[i])
			return 1;
	return 0;
}

/*
 * The limiter band table (ISO/IEC 14496-3 4.6.18.3.3, 0x8bd400): the low
 * band borders and the patch borders, sorted, with borders closer than
 * the octave threshold merged (a removed border becomes the top band,
 * and sorts to the end). Returns -1 for more than 12 bands or none.
 */
int32_t aacSbrLimiterBands(uint8_t *limiter, uint8_t *numLimiter, const uint8_t *low,
	int32_t numLow, const AacSbrPatch *patches, int32_t numPatches, int32_t limiterBands,
	int32_t *work)
{
	int32_t *borders = work + 2;
	uint8_t *table = (uint8_t *)(work + 9);
	uint8_t k0 = low[0], top = low[numLow];
	int32_t count, last, k, previous, i;

	if (limiterBands == 0) {
		limiter[1] = top - k0;
		limiter[0] = 0;
		*numLimiter = 1;
		return 0;
	}

	for (i = 0; i < numPatches; i++)
		borders[i] = patches[i].target - k0;
	borders[numPatches] = top - k0;

	for (i = 0; i <= numLow; i++)
		table[i] = low[i] - k0;
	for (i = 1; i < numPatches; i++)
		table[numLow + i] = borders[i];

	last = numLow + numPatches - 1;
	count = last;
	aacSbrSort(table, (uint8_t)(numLow + numPatches));

	for (k = 1, previous = 0; k <= last; k++) {
		int32_t octaves = aacSbrLog2Ratio(table[previous] + k0, table[k] + k0);

		octaves = (int32_t)(((int64_t)limiterBandsPerOctave[limiterBands] * octaves) >> 27);
		if (octaves < LIMITER_THRESHOLD) {
			if (table[k] != table[previous]) {
				work[0] = 0;
				work[1] = isPatchBorder(table[k], borders, numPatches);
				if (work[1]) {
					work[0] = isPatchBorder(table[previous], borders, numPatches);
					if (!work[0]) {
						table[previous] = top;
						count--;
					}
					previous = k;
					continue;
				}
			}
			table[k] = top;
			count--;
			continue;
		}
		previous = k;
	}

	aacSbrSort(table, (uint8_t)(last + 1));
	if (count >= 13 || count < 1)
		return -1;
	for (i = 0; i <= count; i++)
		limiter[i] = table[i];
	*numLimiter = count;
	return 0;
}
