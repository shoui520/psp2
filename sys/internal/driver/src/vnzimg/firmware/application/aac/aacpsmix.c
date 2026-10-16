/*
 * Parametric stereo's slot: the decorrelated signal (ISO/IEC 14496-3
 * 8.6.4.5) with its transient attenuation, and the mixing into the two
 * channels (8.6.4.6) (0x8e0f90-0x8e2468, 0x8e26a8-0x8e2d40).
 */
#include "aac.h"

#define PEAK_DECAY	0x06209f08	/* 0.76592833836465, Q27 */
#define SMOOTH		0x06000000	/* 0.75 */
#define SMOOTH_NEW	0x02000000	/* 0.25 */
#define GAIN_MAX	0x07ffffff
#define RING		16		/* each decorrelator's ring of complex states */

/*
 * The mixing bands: hybrid subbands for the first 10, then QMF band
 * borders; the last 7 borders also group QMF bands 9-63 into gain bands
 */
static const int8_t mixingBorder[23] = {
	4, 5, 0, 1, 2, 3, 7, 6, 8, 9,
	3, 4, 5, 6, 7, 8, 9, 11, 14, 18, 23, 35, 64,
};

/* the extra right shift of each grouped gain band's power */
static const int8_t groupShift[6] = { 0, 1, 1, 2, 3, 4 };

/* the gain band of the 10 hybrid subbands, then of QMF bands 3-63 */
static const int8_t gainBand[71] = {
	0, 1, 2, 3, 1, 0, 5, 4, 6, 7,
	8, 9, 10, 11, 12, 13, 14, 14, 15, 15, 15, 16, 16, 16, 16, 17, 17, 17, 17, 17,
	18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
	19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
	19, 19, 19, 19, 19, 19, 19, 19, 19,
};

/*
 * The decorrelators: three allpass links (coefficients 0.65144, 0.56472,
 * 0.48954, delays 3, 4, 5 slots) and the band's fractional delay, as one
 * complex filter, Q27
 */
/* the hybrid subbands' decorrelators: numerator taps at delays 1, 4, 5, 6, 8, 9, 10, 13 (0x920798) */
static const int32_t hybridNumerator[10][8][2] = {
	{
		{ -23888653, 3687486 }, { 35197714, -11742846 }, { 38584906, -18528207 },
		{ 47322386, -14091679 }, { -53607886, 37991626 }, { -67974243, 33532250 },
		{ -72946309, 48204330 }, { 97948686, -91763024 },
	},
	{
		{ -21664820, 10719185 }, { 21096431, -30523994 }, { 9664953, -41697459 },
		{ 31904627, -37683932 }, { 18083237, 63167757 }, { -14757714, 74344603 },
		{ 15742353, 86005816 }, { -85187560, -103718262 },
	},
	{
		{ -17424172, 16753017 }, { -1456732, -37076293 }, { -26499023, -33613854 },
		{ 6092250, -48998657 }, { 65591209, 3868116 }, { 50012526, 56953113 },
		{ 85291347, 19240788 }, { -109047239, 78250225 },
	},
	{
		{ -11561479, 21227284 }, { -23426283, -28774693 }, { -42801596, -336172 },
		{ -21704998, -44349488 }, { 25382470, -60604455 }, { 75628330, -5026545 },
		{ 51142492, -70917335 }, { 70980483, 113913003 },
	},
	{
		{ -21664820, -10719185 }, { 21096431, 30523994 }, { 9664953, 41697459 },
		{ 31904627, 37683932 }, { 18083237, -63167757 }, { -14757714, -74344603 },
		{ 15742353, -86005816 }, { -85187560, 103718262 },
	},
	{
		{ -23888653, -3687486 }, { 35197714, 11742846 }, { 38584906, 18528207 },
		{ 47322386, 14091679 }, { -53607886, -37991626 }, { -67974243, -33532250 },
		{ -72946309, -48204330 }, { 97948686, 91763024 },
	},
	{
		{ 13111668, 20306391 }, { -7524404, 36333965 }, { 42797634, 672323 },
		{ -30293499, 38990868 }, { 46094239, 46824038 }, { -75128491, 10030953 },
		{ 27605905, 82962256 }, { -59142286, 120484808 },
	},
	{
		{ -948973, 24152945 }, { -36990517, 2911216 }, { -9992144, 41620263 },
		{ -47872560, -12091399 }, { -65249726, -7722817 }, { 9794883, -75159632 },
		{ -78966450, -37538265 }, { 42976473, -127151172 },
	},
	{
		{ 22403589, 9074390 }, { 32792035, 17362490 }, { -8679884, -41913590 },
		{ 23548801, 43398592 }, { -6696963, -65362986 }, { 29170475, 69957083 },
		{ 40731111, -77367941 }, { 74304219, -111773350 },
	},
	{
		{ 23547312, -5457971 }, { 25821775, -26646003 }, { -39010717, 17614008 },
		{ 49201694, -4144548 }, { -35641213, 55198485 }, { 59999238, -46314161 },
		{ -84020208, 24195586 }, { -88204930, 101164659 },
	},
};

/* and denominator taps at delays 2, 3, 4, 6, 7, 8, 11 (0x920a18) */
static const int32_t hybridDenominator[10][7][2] = {
	{
		{ -86191080, 14694208 }, { -72531470, 22002179 }, { -65096084, 8925736 },
		{ 44168988, -22069991 }, { 40825793, -12858616 }, { 33714839, -15494618 },
		{ -19954422, 13641348 },
	},
	{
		{ -76453582, 42422533 }, { -48083955, 58590468 }, { -60290971, 26118347 },
		{ 8870919, -48572528 }, { 26087867, -33933944 }, { 10197924, -35675985 },
		{ 5467220, 23545165 },
	},
	{
		{ -58078691, 65358147 }, { -7429227, 75430209 }, { -51035430, 41383016 },
		{ -33516429, -36257865 }, { 1932336, -42759273 }, { -20432291, -30972488 },
		{ 23923704, 3452784 },
	},
	{
		{ -33142322, 80909879 }, { 35729603, 66845387 }, { -38012674, 53592964 },
		{ -49118803, 5032618 }, { -22920757, -36148698 }, { -36810488, -4664932 },
		{ 11901737, -21038394 },
	},
	{
		{ -76453582, -42422533 }, { -48083955, -58590468 }, { -60290971, -26118347 },
		{ 8870919, 48572528 }, { 26087867, 33933944 }, { 10197924, 35675985 },
		{ 5467220, -23545165 },
	},
	{
		{ -86191080, -14694208 }, { -72531470, -22002179 }, { -65096084, -8925736 },
		{ 44168988, 22069991 }, { 40825793, 12858616 }, { 33714839, 15494618 },
		{ -19954422, -13641348 },
	},
	{
		{ 62309318, 61338169 }, { 42109549, -63021391 }, { 21721917, 62010703 },
		{ 48350051, -10012815 }, { -18255004, 38714910 }, { 35931921, 9255836 },
		{ 12451094, 20718002 },
	},
	{
		{ 10276846, 86828615 }, { 74338799, 14786907 }, { -13576732, 64287181 },
		{ -3873991, 49223733 }, { -42628444, -3860737 }, { -14602292, 34110799 },
		{ -23185156, -6834756 },
	},
	{
		{ 86979327, 8911727 }, { -42109549, -63021391 }, { 50725244, 41762645 },
		{ -23104480, -43636761 }, { 30099379, 30432167 }, { 3694913, -36920470 },
		{ 4845904, -23680848 },
	},
	{
		{ 73453299, -47428204 }, { -74338799, 14786907 }, { 65027678, 9411182 },
		{ -35458159, 34361354 }, { 38913286, -17828223 }, { -37053538, 1951622 },
		{ -19588639, 14161586 },
	},
};

/* the same for QMF bands 3-22 (0x920c48) */
static const int32_t qmfNumerator[20][8][2] = {
	{
		{ 9946956, -22030057 }, { -34053192, -14736135 }, { 42781792, 1344478 },
		{ -12204155, -47843938 }, { 1032055, -65697061 }, { -73140132, 19885440 },
		{ 70002545, -52387641 }, { -82096296, -106181894 },
	},
	{
		{ -14882525, -14422176 }, { 18822580, 27696591 }, { -35452523, 15341668 },
		{ -24289661, 37359945 }, { 61123000, -12657959 }, { 50675030, -51154891 },
		{ 47011506, 68478975 }, { -51946663, -123757598 },
	},
	{
		{ -15824218, 7752200 }, { -944046, -30040136 }, { 22927924, -26006613 },
		{ 39430934, -6690475 }, { 24334750, 53895512 }, { -17275719, 65991863 },
		{ -66639143, 41851279 }, { -18285051, -132966362 },
	},
	{
		{ 1628932, 14754725 }, { -13646532, 23075018 }, { -8627837, 29697177 },
		{ -28222388, -21820619 }, { -44661905, 33533096 }, { -17097848, -62115705 },
		{ -36923569, -64498319 }, { 16612765, -133185639 },
	},
	{
		{ 12033901, 2889088 }, { 21158852, -10780969 }, { -4285335, -27056598 },
		{ 2726693, 31482743 }, { -39970072, -34137677 }, { 43211646, 42538164 },
		{ 62072566, -32244096 }, { 50387435, -124400576 },
	},
	{
		{ 5598601, -8523048 }, { -20778877, -1964186 }, { 13533070, 19913325 },
		{ 18724480, -20513098 }, { 23059063, -43550970 }, { -55038178, -14223600 },
		{ 27827647, 59378736 }, { 80755546, -107205127 },
	},
	{
		{ -4977992, -6630069 }, { 14366122, 11143512 }, { -18052705, -10676351 },
		{ -24193944, 114004 }, { 44363480, 12136486 }, { 51109624, -14241214 },
		{ -56434379, 23688044 }, { 105663987, -82761825 },
	},
	{
		{ -6344669, 1951872 }, { -5310330, -14750018 }, { 18003970, 1701885 },
		{ 14208740, 15274390 }, { -2011849, 42660944 }, { -34451789, 35217869 },
		{ -19838101, -53257735 }, { 123428762, -52723212 },
	},
	{
		{ -245948, 5215265 }, { -2913909, 13036064 }, { -14498078, 5219621 },
		{ 1366807, -17722711 }, { -38836064, 6777968 }, { 11240499, -44066072 },
		{ 49867667, -16289611 }, { 132848863, -19120126 },
	},
	{
		{ 3715423, 1538981 }, { 7936732, -7936727 }, { 9155538, -9155528 },
		{ -11729671, 9246919 }, { -13829342, -33387013 }, { 11315625, 40122209 },
		{ 13053296, 46283578 }, { 133287386, 15775629 },
	},
	{
		{ 2033473, -2234755 }, { -9052823, 2023543 }, { -3624748, 10068106 },
		{ 12188961, 1950179 }, { 26878335, -18890398 }, { -27173789, -26416140 },
		{ -42525403, 10138801 }, { 124714707, 49604826 },
	},
	{
		{ -1030677, -1946614 }, { 7069533, 2545191 }, { -815686, -8629122 },
		{ -5528790, -8330967 }, { 21868904, 19899185 }, { 33075872, 8326506 },
		{ -7554655, -38613511 }, { 107710400, 80080375 },
	},
	{
		{ -1531548, 217970 }, { -3638695, -4690980 }, { 3486148, 5894764 },
		{ -1880437, 7673091 }, { -13021685, 22829425 }, { -29153791, 8321175 },
		{ 34568681, -5308258 }, { 83424084, 105141906 },
	},
	{
		{ -210161, 1014823 }, { 427753, 4525177 }, { -4336675, -2947206 }, { 5295804, -2922231 },
		{ -21980184, -6761997 }, { 18431448, -19079649 }, { 3405861, 30412012 },
		{ 53497681, 123095063 },
	},
	{
		{ 556460, 341000 }, { 1516075, -2975462 }, { 3804833, 602629 }, { -4215421, -1406374 },
		{ 1546558, -19650782 }, { -5481704, 22067910 }, { -26164896, 1852557 },
		{ 19954441, 132726103 },
	},
	{
		{ 294754, -236142 }, { -1996109, 1180495 }, { -2568960, 746350 }, { 1265510, 2814579 },
		{ 16262416, -2314478 }, { -5257951, -18204690 }, { -652264, -21848931 },
		{ -14937858, 133383876 },
	},
	{
		{ -62637, -182947 }, { 1483463, 46620 }, { 1284278, -1132241 }, { 775833, -1816275 },
		{ 4645018, 12292700 }, { 10935690, 10497952 }, { 17485873, 192287 },
		{ -48820252, 125023919 },
	},
	{
		{ -81569, -1281 }, { -690496, -469261 }, { -382480, 883857 }, { -1047041, 371393 },
		{ -8237530, 5411040 }, { -11042510, -2706174 }, { -679548, 13097580 },
		{ -79402044, 108211426 },
	},
	{
		{ -8544, 22611 }, { 147361, 340532 }, { -13444, -427818 }, { 436730, 230359 },
		{ -4718457, -4572507 }, { 7275231, -2126046 }, { -8705941, -809165 },
		{ -104615664, 84083048 },
	},
	{
		{ 2298, 1962 }, { 14511, -91620 }, { 48580, 95344 }, { -31636, -119317 },
		{ 1716543, -2801140 }, { -2615885, 2742152 }, { 581974, -4332820 },
		{ -122756510, 54270046 },
	},
};

/* and their denominators (0x921148) */
static const int32_t qmfDenominator[20][7][2] = {
	{
		{ -1373371, -87423884 }, { 29005561, 70025617 }, { 51342776, -41001075 },
		{ 45314998, -19609555 }, { -27231767, -33023024 }, { 32487151, 17926472 },
		{ 11344142, -21344219 },
	},
	{
		{ -81337125, -16844100 }, { 27555282, -66524335 }, { -11985133, -61258483 },
		{ -25047436, 36856191 }, { -424741, 38627294 }, { -32823038, -6636171 },
		{ 19058222, 8140814 },
	},
	{
		{ -32382551, 71719422 }, { -63023054, 26105004 }, { -56708257, -16765455 },
		{ 1256254, -39974776 }, { 22640577, -26257151 }, { 29888673, -3157251 },
		{ -5524123, 16732798 },
	},
	{
		{ 59432146, 44622904 }, { 59521772, 24654726 }, { -38803255, 40167921 },
		{ 18159607, 30706209 }, { -30536726, 4885732 }, { -24586675, 10685490 },
		{ -14439646, -3442669 },
	},
	{
		{ 53188665, -45427427 }, { -23204447, -56020490 }, { 16635340, 49862344 },
		{ -28156342, -14346367 }, { 23468804, 14129362 }, { 17935776, -15563876 },
		{ 1839944, -12238308 },
	},
	{
		{ -30684979, -57953811 }, { -21754171, 52519213 }, { 48660003, 7785356 },
		{ 27650705, -2613767 }, { -7763059, -22790776 }, { -10933271, 17778733 },
		{ 10176245, 656282 },
	},
	{
		{ -59035028, 16150169 }, { 49017931, -20303892 }, { 27441864, -36910119 },
		{ -19117174, 14828807 }, { -7628847, 19536768 }, { 4438492, -17631310 },
		{ 169293, 8289123 },
	},
	{
		{ 2677192, 56769443 }, { -45516649, -18853614 }, { -18608140, -38441395 },
		{ 7066520, -19628030 }, { 15888207, -8637374 }, { 910617, 15650348 },
		{ -6601391, 697331 },
	},
	{
		{ 51679627, 9019525 }, { 17403335, 42015368 }, { -39405543, -1176409 },
		{ 3877574, 17347249 }, { -15093782, -3101048 }, { -4741256, -12488006 },
		{ -986386, -5127038 },
	},
	{
		{ 18402877, -44428508 }, { 15953057, -38514086 }, { -17657712, 31530123 },
		{ -10561505, -10561500 }, { 8015957, 10168186 }, { 6948854, 8814577 },
		{ 3870557, -1091612 },
	},
	{
		{ -35767330, -25137685 }, { -35012806, 14502779 }, { 17993666, 27486726 },
		{ 12046702, 2692753 }, { 352914, -10694906 }, { -7663985, -5226055 },
		{ 1063566, 2828068 },
	},
	{
		{ -29101220, 26480089 }, { 31511525, 13052501 }, { 29422662, -2921218 },
		{ -9407514, 3386917 }, { -5803118, 6438238 }, { 7191912, 2175475 }, { -1988561, 947218 },
	},
	{
		{ 17328116, 30379398 }, { -11602224, -28010245 }, { 9790117, -24390585 },
		{ 4842056, -6242345 }, { 6784615, -932999 }, { -5936424, 65278 }, { -781195, -1335247 },
	},
	{
		{ 29249301, -8998272 }, { -10151945, 24508963 }, { -14962684, -17463424 },
		{ -569217, 6021708 }, { -4431521, -2802571 }, { 4320672, -1411379 }, { 846957, -597242 },
	},
	{
		{ -2058024, -26149537 }, { 21007682, -8701667 }, { -19202464, 4450895 },
		{ -2017460, -3959484 }, { 1161605, 3672954 }, { -2716996, 1941595 }, { 419940, 499579 },
	},
	{
		{ -21640597, -3079904 }, { -17506402, -7251389 }, { -4110036, 15903793 },
		{ 2656247, 1570899 }, { 1027627, -2469935 }, { 1395319, -1852324 }, { -267480, 266641 },
	},
	{
		{ -6181183, 16358048 }, { 5801111, 14005120 }, { 9761086, 8798176 }, { -1974062, 62038 },
		{ -1521826, 784465 }, { -496167, 1398805 }, { -147632, -124891 },
	},
	{
		{ 10961782, 7200538 }, { 4350833, -10503840 }, { 9235837, -3440285 },
		{ 918852, -624452 }, { 938871, 214512 }, { 30155, -834315 }, { 47222, -66522 },
	},
	{
		{ 6278908, -6084692 }, { -7002559, 2900555 }, { 813262, -6519989 }, { -196095, 453150 },
		{ -257535, -341884 }, { 98472, 357744 }, { 20825, 12272 },
	},
	{
		{ -2284225, -3727512 }, { 3501278, 1450277 }, { -2702585, -1867871 },
		{ -19310, -121920 }, { -5880, 106845 }, { -50318, -77929 }, { -1308, 2724 },
	},
};

static int32_t shift27(int64_t value)
{
	return (int32_t)(value >> 27);
}

/*
 * 0x8e1408: one decorrelator step: the state is the input less the
 * denominator's taps, the output the numerator's taps over the ring
 */
static void ringFilter(int32_t real, int32_t imag, int32_t *outReal, int32_t *outImag,
	const int32_t (*numerator)[2], const int32_t (*denominator)[2], int32_t *ring,
	int32_t position)
{
	static const int8_t numeratorDelay[8] = { 1, 4, 5, 6, 8, 9, 10, 13 };
	static const int8_t denominatorDelay[7] = { 2, 3, 4, 6, 7, 8, 11 };
	int64_t reRe = 0, imIm = 0, reIm = 0, imRe = 0;
	int32_t *state;
	int32_t k;

	for (k = 0; k < 8; k++) {
		state = &ring[2 * ((position + numeratorDelay[k]) & (RING - 1))];
		reRe += (int64_t)numerator[k][0] * state[0];
		imIm += (int64_t)numerator[k][1] * state[1];
		reIm += (int64_t)numerator[k][0] * state[1];
		imRe += (int64_t)numerator[k][1] * state[0];
	}
	*outReal = shift27(reRe - imIm);
	*outImag = shift27(reIm + imRe);

	reRe = imIm = reIm = imRe = 0;
	for (k = 0; k < 7; k++) {
		state = &ring[2 * ((position + denominatorDelay[k]) & (RING - 1))];
		reRe += (int64_t)denominator[k][0] * state[0];
		imIm += (int64_t)denominator[k][1] * state[1];
		reIm += (int64_t)denominator[k][0] * state[1];
		imRe += (int64_t)denominator[k][1] * state[0];
	}
	state = &ring[2 * ((position - 1) & (RING - 1))];
	state[0] = shift27((int64_t)real * 0x08000000 + (imIm - reRe));
	state[1] = shift27((int64_t)imag * 0x08000000 - (reIm + imRe));
}

static int64_t energy(int32_t real, int32_t imag)
{
	return (int64_t)real * real + (int64_t)imag * imag;
}

static int32_t subtractSaturate(int32_t a, int32_t b)
{
	int64_t difference = (int64_t)a - b;

	if (difference > 0x7fffffff)
		return 0x7fffffff;
	if (difference < -(int64_t)0x80000000)
		return (int32_t)0x80000000;
	return (int32_t)difference;
}

/* each gain band's power in this slot */
static void bandPowers(AacPs *ps, const int32_t *real, const int32_t *imag, int32_t *power)
{
	const int32_t *re = ps->subband[0], *im = ps->subband[1];
	const int8_t *border = &mixingBorder[16];
	int32_t group, k;

	power[0] = shift27(energy(re[0], im[0]) + energy(re[5], im[5]));
	power[1] = shift27(energy(re[1], im[1]) + energy(re[4], im[4]));
	power[2] = shift27(energy(re[2], im[2]));
	power[3] = shift27(energy(re[3], im[3]));
	power[4] = shift27(energy(re[7], im[7]));
	power[5] = shift27(energy(re[6], im[6]));
	power[6] = shift27(energy(re[8], im[8]));
	power[7] = shift27(energy(re[9], im[9]));
	for (k = 3; k < 9; k++)
		power[k + 5] = shift27(energy(real[k], imag[k]));

	for (group = 0; group < 6; group++) {
		int32_t end = aacMin(ps->stop, border[group + 1]);
		int64_t sum = 0;

		for (k = border[group]; k < end; k++)
			sum += energy(real[k], imag[k]);
		power[14 + group] = (int32_t)(sum >> (27 + groupShift[group]));
	}
}

/*
 * The transient attenuation: the gain that keeps each band's power
 * within 1.5 times its smoothed excess over the decaying peak
 */
static void bandGains(AacPs *ps, const int32_t *power, int32_t *gain)
{
	int32_t *peak = ps->power[0], *smooth = ps->power[1], *excess = ps->power[2];
	int32_t i, limit;

	for (i = 0; i < 20; i++) {
		peak[i] = aacMax(power[i], shift27((int64_t)peak[i] * PEAK_DECAY));
		excess[i] = shift27((int64_t)subtractSaturate(peak[i], power[i]) * SMOOTH_NEW +
			(int64_t)excess[i] * SMOOTH);
		smooth[i] = shift27((int64_t)smooth[i] * SMOOTH + (int64_t)power[i] * SMOOTH_NEW);
		limit = shift27((int64_t)excess[i] * SMOOTH) << 1;
		gain[i] = smooth[i] < limit ? aacSbrQuotient(smooth[i], limit) : GAIN_MAX;
	}
}

static int32_t attenuate(int32_t value, int32_t gain)
{
	return shift27((int64_t)gain * value);
}

/*
 * 0x8e17c0: the decorrelated signal: the hybrid subbands and QMF bands
 * 3-22 through their decorrelators, the bands above through plain delays
 * of 14 slots (to band 34) or 1, each attenuated by its band's gain
 */
static void decorrelate(AacPs *ps, const int32_t *real, const int32_t *imag,
	int32_t *rightReal, int32_t *rightImag)
{
	int32_t power[20], gain[20];
	int32_t outReal, outImag, oldReal, oldImag;
	int32_t k, position, g;

	bandPowers(ps, real, imag, power);
	bandGains(ps, power, gain);

	for (k = 0; k < 10; k++) {
		ringFilter(ps->subband[0][k], ps->subband[1][k], &outReal, &outImag,
			hybridNumerator[k], hybridDenominator[k], ps->subbandDelay2[k],
			ps->ringPosition);
		g = gain[gainBand[k]];
		ps->subband[2][k] = attenuate(outReal, g);
		ps->subband[3][k] = attenuate(outImag, g);
	}
	for (k = 3; k < 23; k++) {
		ringFilter(real[k], imag[k], &outReal, &outImag, qmfNumerator[k - 3],
			qmfDenominator[k - 3], ps->subbandDelay[k - 3], ps->ringPosition);
		g = gain[gainBand[k + 7]];
		rightReal[k] = attenuate(outReal, g);
		rightImag[k] = attenuate(outImag, g);
	}
	for (k = 23; k < ps->stop; k++) {
		position = k - 23 < 12 ? ps->delayPosition : 0;
		oldReal = ps->qmfDelayReal[k - 23][position];
		oldImag = ps->qmfDelayImag[k - 23][position];
		ps->qmfDelayReal[k - 23][position] = real[k];
		ps->qmfDelayImag[k - 23][position] = imag[k];
		g = gain[gainBand[k + 7]];
		rightReal[k] = attenuate(oldReal, g);
		rightImag[k] = attenuate(oldImag, g);
	}

	if (++ps->delayPosition >= 14)
		ps->delayPosition = 0;
	if (--ps->ringPosition < 0)
		ps->ringPosition = RING - 1;
}

/*
 * 0x8e26a8: each mixing band's matrix one slot further, applied to the
 * left signal and the decorrelated one: hybrid subbands, then QMF bands
 */
static void mixBands(AacPs *ps, int32_t *real, int32_t *imag, int32_t *rightReal,
	int32_t *rightImag)
{
	int32_t *leftRe = ps->subband[0], *leftIm = ps->subband[1];
	int32_t *rightRe = ps->subband[2], *rightIm = ps->subband[3];
	int32_t band, k, m, start, end;
	int32_t lRe, lIm, dRe, dIm;

	for (band = 0; band < 22; band++) {
		if (band == 10) {
			leftRe = real;
			leftIm = imag;
			rightRe = rightReal;
			rightIm = rightImag;
		}
		for (m = 0; m < 4; m++)
			ps->hStart[m][band] += ps->hDelta[m][band];

		start = mixingBorder[band];
		end = band < 10 ? start + 1 : aacMin(ps->stop, mixingBorder[band + 1]);
		for (k = start; k < end; k++) {
			lRe = leftRe[k];
			lIm = leftIm[k];
			dRe = rightRe[k];
			dIm = rightIm[k];
			leftRe[k] = (int32_t)(((int64_t)ps->hStart[0][band] * lRe +
				(int64_t)ps->hStart[2][band] * dRe) >> 26);
			leftIm[k] = (int32_t)(((int64_t)ps->hStart[0][band] * lIm +
				(int64_t)ps->hStart[2][band] * dIm) >> 26);
			rightRe[k] = (int32_t)(((int64_t)ps->hStart[1][band] * lRe +
				(int64_t)ps->hStart[3][band] * dRe) >> 26);
			rightIm[k] = (int32_t)(((int64_t)ps->hStart[1][band] * lIm +
				(int64_t)ps->hStart[3][band] * dIm) >> 26);
		}
	}
}

/*
 * 0x8e0f90: one slot of parametric stereo: the slot brought to the
 * delay lines' scale, its low bands split into hybrid subbands (the
 * filters' delay is 6 slots, so from the slot 6 ahead), the decorrelated
 * signal made and both channels mixed, the subbands summed back
 */
void aacPsMix(AacPs *ps, int32_t **real, int32_t **imag, int32_t *rightReal,
	int32_t *rightImag, int32_t *timing, int32_t slot, int32_t kx, int32_t *work)
{
	int32_t *re = real[0], *im = imag[0];
	int32_t lowShift = (slot < 6 ? timing[2] : timing[0]) - timing[6];
	int32_t highShift = timing[3] - timing[6];
	int32_t k;

	for (k = 0; k < kx; k++) {
		re[k] = aacShiftDown(re[k], lowShift);
		im[k] = aacShiftDown(im[k], lowShift);
	}
	for (; k < ps->stop; k++) {
		re[k] = aacShiftDown(re[k], highShift);
		im[k] = aacShiftDown(im[k], highShift);
	}

	aacPsHybridAnalysis(real[6], imag[6], ps->subband[0], ps->subband[1], &ps->hybrid,
		timing[0] - timing[6], work);
	decorrelate(ps, re, im, rightReal, rightImag);
	mixBands(ps, re, im, rightReal, rightImag);
	aacPsHybridSynthesis(ps->subband[0], ps->subband[1], re, im, &ps->hybrid);
	aacPsHybridSynthesis(ps->subband[2], ps->subband[3], rightReal, rightImag,
		&ps->hybrid);
}
