/*
 * Parametric stereo (ISO/IEC 14496-3 8.6.4): the indices' decoding
 * (0x86bacc-0x86bf4a), ps_data() (0x86bf4c-) and the mixing's parameters.
 */
#include "aac.h"

/* the bands of each IID or ICC resolution */
static const int32_t numBins[3] = { 10, 20, 34 };

/* x / 3, towards zero */
static int32_t divide3(int32_t value)
{
	int32_t negative = value < 0;
	int64_t product;

	if (negative)
		value = -value;
	aacFixedMultiply(&product, value * 2, 0x02ab0000, 56);
	value = aacFixedToInt(&product, 56) >> 1;
	return negative ? -value : value;
}

/* undoes the delta coding of one envelope's indices, doubled out at the coarse resolution */
static void deltaDecode(int32_t enable, int8_t *index, const int8_t *previous, int32_t dt,
	int32_t count, int32_t stride, int32_t minimum, int32_t maximum)
{
	int32_t i;

	if (enable == 1) {
		if (!dt) {
			index[0] = aacSbrClamp(index[0], minimum, maximum);
			for (i = 1; i < count; i++)
				index[i] = aacSbrClamp((int8_t)(index[i] + index[i - 1]),
					minimum, maximum);
		} else {
			for (i = 0; i < count; i++)
				index[i] = aacSbrClamp((int8_t)(index[i] + previous[i * stride]),
					minimum, maximum);
		}
	} else {
		for (i = 0; i < count; i++)
			index[i] = 0;
	}
	if (stride == 2)
		for (i = 2 * count - 1; i >= 1; i--)
			index[i] = index[i >> 1];
}

/* 34 bands' indices folded onto 20 */
static void map34To20(int8_t *index)
{
	index[0] = divide3(2 * index[0] + index[1]);
	index[1] = divide3(2 * index[2] + index[1]);
	index[2] = divide3(2 * index[3] + index[4]);
	index[3] = divide3(2 * index[5] + index[4]);
	index[4] = aacSbrHalve(index[6] + index[7]);
	index[5] = aacSbrHalve(index[8] + index[9]);
	index[6] = index[10];
	index[7] = index[11];
	index[8] = aacSbrHalve(index[12] + index[13]);
	index[9] = aacSbrHalve(index[14] + index[15]);
	index[10] = index[16];
	index[11] = index[17];
	index[12] = index[18];
	index[13] = index[19];
	index[14] = aacSbrHalve(index[20] + index[21]);
	index[15] = aacSbrHalve(index[22] + index[23]);
	index[16] = aacSbrHalve(index[24] + index[25]);
	index[17] = aacSbrHalve(index[26] + index[27]);
	index[18] = aacSbrHalve(aacSbrHalve(index[28] + index[29] + index[30] + index[31]));
	index[19] = aacSbrHalve(index[32] + index[33]);
}

/*
 * The frame's IID and ICC indices: delta decoding, the envelope borders
 * of a fixed frame, an envelope added up to the frame's end otherwise.
 */
void aacPsDecode(AacPs *ps)
{
	const int8_t *iidPrevious, *iccPrevious;
	int32_t steps, shift, env, i;

	if (!ps->dataAvailable)
		ps->numEnvelopes = 0;

	for (env = 0; env < ps->numEnvelopes; env++) {
		steps = ps->fineIid ? 15 : 7;
		if (env == 0) {
			iidPrevious = ps->iidPrevious;
			iccPrevious = ps->iccPrevious;
		} else {
			iidPrevious = ps->iid[env - 1];
			iccPrevious = ps->icc[env - 1];
		}
		deltaDecode(ps->iidEnable, ps->iid[env], iidPrevious, ps->iidDt[env],
			numBins[ps->iidMode], ps->iidMode ? 1 : 2, -steps, steps);
		deltaDecode(ps->iccEnable, ps->icc[env], iccPrevious, ps->iccDt[env],
			numBins[ps->iccMode], ps->iccMode ? 1 : 2, 0, 7);
	}

	/* no data: the last frame's parameters hold */
	if (ps->numEnvelopes == 0) {
		ps->numEnvelopes = 1;
		for (i = 0; i < 34; i++)
			ps->iid[0][i] = ps->iidEnable ? ps->iidPrevious[i] : 0;
		for (i = 0; i < 34; i++)
			ps->icc[0][i] = ps->iccEnable ? ps->iccPrevious[i] : 0;
	}
	for (i = 0; i < 34; i++)
		ps->iidPrevious[i] = ps->iid[ps->numEnvelopes - 1][i];
	for (i = 0; i < 34; i++)
		ps->iccPrevious[i] = ps->icc[ps->numEnvelopes - 1][i];
	ps->dataAvailable = 0;

	if (!ps->frameClass) {
		switch (ps->numEnvelopes) {
		case 4:
			shift = 2;
			break;
		case 2:
			shift = 1;
			break;
		default:
			shift = 0;
			break;
		}
		ps->borders[0] = 0;
		for (i = 1; i < ps->numEnvelopes; i++)
			ps->borders[i] = (32 * i) >> shift;
		ps->borders[ps->numEnvelopes] = 32;
	} else {
		ps->borders[0] = 0;
		if (ps->borders[ps->numEnvelopes] < 32) {
			ps->numEnvelopes++;
			ps->borders[ps->numEnvelopes] = 32;
			for (i = 0; i < 34; i++)
				ps->iid[ps->numEnvelopes - 1][i] = ps->iid[ps->numEnvelopes - 2][i];
			for (i = 0; i < 34; i++)
				ps->icc[ps->numEnvelopes - 1][i] = ps->icc[ps->numEnvelopes - 2][i];
		}
		for (i = 1; i < ps->numEnvelopes; i++) {
			int32_t latest = i - ps->numEnvelopes + 32;

			if (latest < ps->borders[i])
				ps->borders[i] = latest;
			else if (ps->borders[i] < ps->borders[i - 1] + 1)
				ps->borders[i] = ps->borders[i - 1] + 1;
		}
	}

	for (env = 0; env < ps->numEnvelopes; env++) {
		if (ps->iidMode == 2)
			map34To20(ps->iid[env]);
		if (ps->iccMode == 2)
			map34To20(ps->icc[env]);
	}
}

/* the envelopes of a fixed frame, by bs_num_env */
static const int32_t fixedEnvelopes[4] = { 0, 1, 2, 4 };

/*
 * ps_data() of an SBR extension; `count` bits are available. Returns the
 * bits read.
 */
int32_t aacPsParse(AacPs *ps, AacSbrBits *bits, int32_t count)
{
	int32_t start, dt, env, i, n;
	const int8_t (*tree)[2];

	if (!ps)
		return 0;
	start = bits->remaining;

	if (aacSbrReadBits(bits, 1)) {		/* enable_ps_header */
		ps->iidEnable = aacSbrReadBits(bits, 1);
		if (ps->iidEnable)
			ps->iidMode = aacSbrReadBits(bits, 3);
		ps->iccEnable = aacSbrReadBits(bits, 1);
		if (ps->iccEnable)
			ps->iccMode = aacSbrReadBits(bits, 3);
		ps->extEnable = aacSbrReadBits(bits, 1);
		if (ps->iidMode >= 6 || ps->iccMode >= 6) {
			ps->headerValid = 0;
		} else {
			/* modes 3-5: the same resolutions, fine IID quantisation */
			if (ps->iidMode >= 3) {
				ps->fineIid = 1;
				ps->iidMode -= 3;
			} else {
				ps->fineIid = 0;
			}
			if (ps->iccMode >= 3)
				ps->iccMode -= 3;
			ps->headerValid = 1;
		}
	}

	if (!ps->headerValid) {
		/* nothing usable: the rest is skipped */
		ps->dataAvailable = 0;
		count -= start - bits->remaining;
		while (count >= 1) {
			n = aacMin(count, 8);
			aacSbrReadBits(bits, n);
			count -= n;
		}
		return start - bits->remaining;
	}

	ps->frameClass = aacSbrReadBits(bits, 1);
	n = aacSbrReadBits(bits, 2);
	if (!ps->frameClass) {
		ps->numEnvelopes = fixedEnvelopes[n];
	} else {
		ps->numEnvelopes = n + 1;
		for (i = 1; i < ps->numEnvelopes + 1; i++)
			ps->borders[i] = aacSbrReadBits(bits, 5) + 1;
	}

	if (ps->iidEnable) {
		for (env = 0; env < ps->numEnvelopes; env++) {
			dt = aacSbrReadBits(bits, 1);
			if (dt)
				tree = ps->fineIid ? aacPsIidTimeFine : aacPsIidTimeCoarse;
			else
				tree = ps->fineIid ? aacPsIidFreqFine : aacPsIidFreqCoarse;
			for (i = 0; i < numBins[ps->iidMode]; i++)
				ps->iid[env][i] = aacSbrHuffman(tree, bits);
			ps->iidDt[env] = dt;
		}
	}
	if (ps->iccEnable) {
		for (env = 0; env < ps->numEnvelopes; env++) {
			dt = aacSbrReadBits(bits, 1);
			tree = dt ? aacPsIccTime : aacPsIccFreq;
			for (i = 0; i < numBins[ps->iccMode]; i++)
				ps->icc[env][i] = aacSbrHuffman(tree, bits);
			ps->iccDt[env] = dt;
		}
	}
	if (ps->extEnable) {
		n = aacSbrReadBits(bits, 4);
		if (n == 15)
			n += aacSbrReadBits(bits, 8);
		while (n--)
			aacSbrReadBits(bits, 8);
	}
	ps->dataAvailable = 1;
	return start - bits->remaining;
}

/* 0x86c17c: the bits every delay line could be shifted left by, at most 27 */
int32_t aacPsHeadroom(AacPs *ps)
{
	int32_t headroom = 27, i;

	for (i = 0; i < 12; i++) {
		headroom = aacMin(headroom, aacSbrBufferHeadroom(ps->qmfDelayReal[i], 14));
		headroom = aacMin(headroom, aacSbrBufferHeadroom(ps->qmfDelayImag[i], 14));
	}
	for (; i < 41; i++) {
		headroom = aacMin(headroom, aacSbrBufferHeadroom(ps->qmfDelayReal[i], 1));
		headroom = aacMin(headroom, aacSbrBufferHeadroom(ps->qmfDelayImag[i], 1));
	}
	for (i = 0; i < 20; i++)
		headroom = aacMin(headroom, aacSbrBufferHeadroom(ps->subbandDelay[i], 32));
	for (i = 0; i < 10; i++)
		headroom = aacMin(headroom, aacSbrBufferHeadroom(ps->subbandDelay2[i], 32));
	for (i = 0; i < 3; i++)
		headroom = aacMin(headroom, aacSbrBufferHeadroom(ps->hybrid.delayReal[i],
			ps->hybrid.length));
	for (i = 0; i < 3; i++)
		headroom = aacMin(headroom, aacSbrBufferHeadroom(ps->hybrid.delayImag[i],
			ps->hybrid.length));
	return headroom;
}

/* 0x86c3de: every delay line shifted by `shift`, the powers by twice that */
void aacPsRescale(AacPs *ps, int32_t shift)
{
	int32_t i;

	for (i = 0; i < 12; i++) {
		aacSbrScale(ps->qmfDelayReal[i], 14, shift);
		aacSbrScale(ps->qmfDelayImag[i], 14, shift);
	}
	for (; i < 41; i++) {
		aacSbrScale(ps->qmfDelayReal[i], 1, shift);
		aacSbrScale(ps->qmfDelayImag[i], 1, shift);
	}
	for (i = 0; i < 20; i++)
		aacSbrScale(ps->subbandDelay[i], 32, shift);
	for (i = 0; i < 10; i++)
		aacSbrScale(ps->subbandDelay2[i], 32, shift);
	for (i = 0; i < 3; i++)
		aacSbrScale(ps->hybrid.delayReal[i], ps->hybrid.length, shift);
	for (i = 0; i < 3; i++)
		aacSbrScale(ps->hybrid.delayImag[i], ps->hybrid.length, shift);
	shift *= 2;
	for (i = 0; i < 3; i++)
		aacSbrScale(ps->power[i], 20, shift);
}

/*
 * 0x86c55a: Before a frame's synthesis: the delay lines take as much headroom as
 * they have, then all go to the scale the frame's QMF matrix allows.
 */
void aacPsApply(AacPs *ps, int32_t *timing)
{
	int32_t headroom = aacPsHeadroom(ps);

	ps->scale += headroom;
	timing[6] = aacMin(timing[0], timing[2]);
	timing[6] = aacMin(timing[6], timing[3]);
	timing[6] = aacMin(timing[6], ps->scale) - 1;
	aacPsRescale(ps, timing[6] - ps->scale + headroom);
	ps->scale = timing[6];
}

/* sqrt(2) / (1 + 10^(-iid / 20)) and its mirror, Q27, by quantised IID */
static const int32_t iidScaleCoarse[15] = {
	94756576, 94163000, 93071736, 90489592, 86654256, 80264688, 74314480, 67108864,
	59030096, 50643636, 38707052, 28615286, 18570230, 11854445, 5328578,
};

static const int32_t iidScaleFine[31] = {
	94905792, 94904768, 94901520, 94891264, 94858848, 94756560, 94608264, 94314448,
	93736296, 92613792, 90489576, 88175688, 84846416, 80264712, 74314504, 67108864,
	59030108, 50643608, 42523940, 35103376, 28615316, 20733646, 14856201, 10582255,
	7515002, 5328553, 2999700, 1687432, 949015, 533689, 300118,
};

/* acos(icc) / 2, Q27, by quantised ICC */
static const int32_t iccAlpha[8] = {
	0, 11974100, 19170788, 31076236, 40074932, 52707168, 73840488, 105414336,
};

/* the parameter band of each of the 22 mixing bands */
static const int8_t parameterBand[22] = {
	1, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
};

/* 1 / n, Q27, by envelope length in slots */
static const int32_t reciprocal[33] = {
	134217727, 134217727, 67108864, 44739244, 33554432, 26843546, 22369622, 19173962,
	16777216, 14913081, 13421773, 12201612, 11184811, 10324441, 9586981, 8947849,
	8388608, 7895161, 7456541, 7064091, 6710887, 6391321, 6100806, 5835554,
	5592406, 5368709, 5162221, 4971027, 4793491, 4628198, 4473925, 4329604,
	4194304,
};

static int32_t mulQ27(int32_t a, int32_t b)
{
	return (int32_t)(((int64_t)a * b) >> 27);
}

/*
 * 0x86c5c0: At the start of an envelope: the mixing matrix it ends at, and the steps
 * from the last one. The first envelope of a frame also clears the delay
 * lines of the bands SBR newly covers.
 */
void aacPsUpdate(AacPs *ps, int32_t envelope, int32_t stop)
{
	const int32_t *scale;
	int32_t steps, invLength, band, i, k;

	if (envelope == 0) {
		int32_t old = ps->stop;

		ps->stop = stop;
		if (old < stop && old != 0) {
			stop -= 3;
			for (k = aacMax(old, 3) - 3; k < 20 && k < stop; k++)
				for (i = 0; i < 32; i++)
					ps->subbandDelay[k][i] = 0;
			for (; k < 35 && k < stop; k++) {
				for (i = 0; i < 14; i++) {
					ps->qmfDelayReal[k - 20][i] = 0;
					ps->qmfDelayImag[k - 20][i] = 0;
				}
			}
			for (; k < stop; k++) {
				ps->qmfDelayReal[k - 23][0] = 0;
				ps->qmfDelayImag[k - 23][0] = 0;
			}
		}
	}

	if (ps->fineIid) {
		scale = iidScaleFine;
		steps = 15;
	} else {
		scale = iidScaleCoarse;
		steps = 7;
	}
	invLength = reciprocal[ps->borders[envelope + 1] - ps->borders[envelope]];

	for (band = 0; band < 22; band++) {
		int32_t bin = parameterBand[band];
		int32_t iid = ps->iid[envelope][bin];
		int32_t c1 = scale[iid + steps], c2 = scale[steps - iid];
		int32_t alpha = iccAlpha[ps->icc[envelope][bin]];
		int32_t beta = mulQ27(mulQ27(c1 - c2, alpha), 0x05a8279a);	/* 1 / sqrt(2) */
		int32_t h[4];

		h[0] = mulQ27(c2, aacPsCos(beta + (alpha >> 1)));
		h[1] = mulQ27(c1, aacPsCos(beta - (alpha >> 1)));
		h[2] = mulQ27(c2, aacPsSin(beta + (alpha >> 1)));
		h[3] = mulQ27(c1, aacPsSin(beta - (alpha >> 1)));
		for (i = 0; i < 4; i++) {
			ps->hDelta[i][band] = mulQ27(h[i] - ps->h[i][band], invLength);
			ps->hStart[i][band] = ps->h[i][band];
			ps->h[i][band] = h[i];
		}
	}
}

/* sin(k pi / 1024), Q27, k = 0..512 (0x924720) */
static const int32_t quarterSine[513] = {
	0, 411774, 823544, 1235307, 1647058, 2058793, 2470509, 2882202,
	3293868, 3705503, 4117102, 4528663, 4940182, 5351654, 5763075, 6174443,
	6585752, 6996999, 7408180, 7819292, 8230330, 8641290, 9052170, 9462964,
	9873669, 10284281, 10694796, 11105210, 11515520, 11925722, 12335811, 12745785,
	13155638, 13565367, 13974969, 14384439, 14793774, 15202970, 15612022, 16020928,
	16429683, 16838283, 17246724, 17655004, 18063117, 18471060, 18878829, 19286421,
	19693831, 20101056, 20508091, 20914933, 21321579, 21728024, 22134264, 22540296,
	22946116, 23351720, 23757104, 24162265, 24567198, 24971900, 25376367, 25780594,
	26184580, 26588319, 26991807, 27395042, 27798018, 28200733, 28603183, 29005363,
	29407271, 29808901, 30210251, 30611317, 31012094, 31412580, 31812770, 32212660,
	32612248, 33011528, 33410498, 33809153, 34207490, 34605505, 35003194, 35400554,
	35797580, 36194270, 36590619, 36986623, 37382280, 37777584, 38172533, 38567123,
	38961350, 39355210, 39748699, 40141815, 40534552, 40926908, 41318879, 41710461,
	42101651, 42492444, 42882837, 43272826, 43662408, 44051580, 44440336, 44828674,
	45216591, 45604081, 45991143, 46377772, 46763964, 47149716, 47535024, 47919884,
	48304294, 48688249, 49071746, 49454781, 49837350, 50219450, 50601078, 50982229,
	51362901, 51743089, 52122790, 52502000, 52880716, 53258935, 53636652, 54013865,
	54390568, 54766760, 55142437, 55517594, 55892229, 56266338, 56639918, 57012964,
	57385473, 57757443, 58128868, 58499747, 58870075, 59239849, 59609065, 59977720,
	60345811, 60713334, 61080285, 61446661, 61812459, 62177676, 62542307, 62906349,
	63269799, 63632654, 63994909, 64356563, 64717610, 65078049, 65437875, 65797085,
	66155675, 66513644, 66870985, 67227698, 67583778, 67939221, 68294026, 68648187,
	69001702, 69354568, 69706781, 70058338, 70409235, 70759470, 71109039, 71457938,
	71806165, 72153716, 72500588, 72846777, 73192281, 73537096, 73881218, 74224646,
	74567374, 74909401, 75250723, 75591336, 75931238, 76270426, 76608895, 76946643,
	77283667, 77619964, 77955530, 78290362, 78624457, 78957813, 79290425, 79622291,
	79953407, 80283771, 80613379, 80942229, 81270316, 81597639, 81924193, 82249977,
	82574986, 82899218, 83222670, 83545339, 83867221, 84188314, 84508614, 84828119,
	85146825, 85464730, 85781831, 86098124, 86413607, 86728276, 87042129, 87355163,
	87667375, 87978761, 88289320, 88599047, 88907941, 89215997, 89523214, 89829588,
	90135117, 90439797, 90743627, 91046602, 91348720, 91649978, 91950373, 92249903,
	92548565, 92846356, 93143273, 93439313, 93734473, 94028752, 94322145, 94614651,
	94906266, 95196987, 95486813, 95775740, 96063765, 96350886, 96637101, 96922406,
	97206798, 97490276, 97772835, 98054475, 98335192, 98614983, 98893846, 99171778,
	99448777, 99724839, 99999963, 100274146, 100547385, 100819677, 101091021, 101361413,
	101630851, 101899332, 102166855, 102433415, 102699012, 102963642, 103227303, 103489992,
	103751707, 104012445, 104272205, 104530983, 104788777, 105045585, 105301404, 105556232,
	105810066, 106062905, 106314745, 106565585, 106815421, 107064252, 107312076, 107558889,
	107804690, 108049476, 108293245, 108535995, 108777724, 109018428, 109258107, 109496757,
	109734376, 109970963, 110206514, 110441028, 110674503, 110906936, 111138325, 111368668,
	111597962, 111826207, 112053398, 112279535, 112504616, 112728637, 112951597, 113173494,
	113394326, 113614091, 113832786, 114050409, 114266960, 114482434, 114696832, 114910149,
	115122385, 115333538, 115543605, 115752584, 115960474, 116167272, 116372977, 116577587,
	116781099, 116983512, 117184824, 117385033, 117584138, 117782135, 117979024, 118174802,
	118369468, 118563020, 118755456, 118946774, 119136973, 119326050, 119514005, 119700834,
	119886536, 120071110, 120254554, 120436867, 120618045, 120798088, 120976994, 121154762,
	121331389, 121506874, 121681216, 121854412, 122026461, 122197362, 122367112, 122535711,
	122703156, 122869447, 123034581, 123198557, 123361373, 123523028, 123683521, 123842849,
	124001012, 124158007, 124313834, 124468491, 124621977, 124774289, 124925427, 125075389,
	125224174, 125371780, 125518206, 125663451, 125807513, 125950390, 126092083, 126232588,
	126371905, 126510033, 126646970, 126782715, 126917267, 127050624, 127182785, 127313749,
	127443515, 127572081, 127699447, 127825610, 127950571, 128074327, 128196878, 128318222,
	128438358, 128557285, 128675002, 128791508, 128906802, 129020883, 129133749, 129245400,
	129355834, 129465051, 129573049, 129679827, 129785385, 129889721, 129992835, 130094725,
	130195391, 130294831, 130393045, 130490032, 130585790, 130680319, 130773618, 130865687,
	130956523, 131046127, 131134497, 131221634, 131307535, 131392200, 131475628, 131557819,
	131638772, 131718486, 131796959, 131874193, 131950185, 132024935, 132098442, 132170707,
	132241727, 132311502, 132380032, 132447316, 132513353, 132578143, 132641685, 132703979,
	132765024, 132824819, 132883364, 132940658, 132996700, 133051491, 133105030, 133157316,
	133208348, 133258127, 133306651, 133353921, 133399935, 133444694, 133488197, 133530443,
	133571433, 133611165, 133649640, 133686857, 133722815, 133757515, 133790956, 133823137,
	133854059, 133883721, 133912123, 133939265, 133965145, 133989765, 134013124, 134035221,
	134056057, 134075631, 134093943, 134110993, 134126780, 134141305, 134154567, 134166567,
	134177304, 134186778, 134194989, 134201937, 134207622, 134212043, 134215201, 134217096,
	134217727,
};

#define ANGLE_SCALE	1304	/* the angle to 1024ths of a half turn, Q27 */

static int32_t sineAt(int32_t index)
{
	return index < 512 ? quarterSine[index] : quarterSine[1024 - index];
}

/* 0x8e2468 */
int32_t aacPsCos(int32_t angle)
{
	int32_t index = (int32_t)(((int64_t)aacAbs(angle) * ANGLE_SCALE) >> 27) & 0x3ff;

	return index < 512 ? quarterSine[512 - index] : -quarterSine[index - 512];
}

/* 0x8e2548: rounded to the nearest step */
int32_t aacPsSin(int32_t angle)
{
	int32_t step = (int32_t)(((int64_t)angle * ANGLE_SCALE + 0x04000000) >> 27);

	if (step < 0)
		return -sineAt(-step & 0x3ff);
	return sineAt(step & 0x3ff);
}

static void clearWords(int32_t *to, int32_t count)
{
	int32_t i;

	for (i = 0; i < count; i++)
		to[i] = 0;
}

int32_t *aacPsTake(int32_t **cursor, int32_t count, int32_t clear)
{
	int32_t *block = *cursor;

	*cursor += count;
	if (clear)
		clearWords(block, count);
	return block;
}

/*
 * 0x8e08f8: parametric stereo's state, cleared, its delay lines carved
 * from the second channel's history, then its state and buffer rows
 */
int32_t aacPsInit(AacPs *ps, int32_t *history, int32_t *state, int32_t *buffer)
{
	int32_t *cursor = history;
	int32_t i;

	clearWords((int32_t *)ps, sizeof(*ps) / 4);
	ps->iccMode = 0;
	ps->iidMode = 0;
	ps->extEnable = 0;
	ps->iccEnable = 0;
	ps->iidEnable = 0;
	ps->dataAvailable = 0;
	ps->headerValid = 0;

	for (i = 0; i < 3; i++)
		ps->power[i] = aacPsTake(&cursor, 20, 1);
	aacPsHybridInit(&ps->hybrid, &cursor);
	for (i = 0; i < 4; i++)
		ps->subband[i] = aacPsTake(&cursor, 10, 1);

	for (i = 0; i < 41; i++) {
		int32_t length = i < 12 ? 14 : 1;

		ps->qmfDelayReal[i] = aacPsTake(&cursor, length, 1);
		ps->qmfDelayImag[i] = aacPsTake(&cursor, length, 1);
	}

	ps->ringPosition = 0;
	ps->delayPosition = 0;
	for (i = 0; i < 20; i++)
		ps->subbandDelay[i] = i < 5 ? aacPsTake(&cursor, 32, 1) : aacPsTake(&state, 32, 1);
	for (i = 0; i < 10; i++)
		ps->subbandDelay2[i] = aacPsTake(&buffer, 32, 1);

	for (i = 0; i < 22; i++) {
		ps->h[0][i] = 0x04000000;
		ps->h[1][i] = 0x04000000;
		ps->h[2][i] = 0;
		ps->h[3][i] = 0;
	}
	return 0;
}
