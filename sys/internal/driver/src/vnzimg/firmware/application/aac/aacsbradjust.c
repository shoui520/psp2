/*
 * The SBR envelope adjuster (ISO/IEC 14496-3 4.6.18.7, 0x8bb408 with its
 * kernels at 0x8b73e8-0x8ba168 and helpers at 0x8521b8-0x8522e4): the
 * HF-generated bands' energies estimated, the gains, noise floor and
 * sinusoids computed as floats (aacsbrfloat.c), limited and boosted per
 * limiter band, smoothed over the envelope's first slots and applied.
 * The real-valued (low-power) decoder also reduces the aliasing of the
 * gains and leaks the sinusoids into the neighbouring bands.
 */
#include "aac.h"

#define ONE		0x04000000	/* 1, with exponent 1 */
#define ONE_Q27		0x07ffffff	/* the smoothing's and alias degrees' 1 */
#define BOOST_MAX	0x050615f9	/* 1.584893^2 / 2, with exponent 2 */
#define SINE_LEAK	0x0010b0f2	/* 0.00815, Q27 */
#define MAX_EXPONENT	34

/* the adjuster's work area */
typedef struct AdjustWork {
	int32_t envelope[48];		/* E_orig */
	int32_t estimate[48];		/* E_curr */
	int32_t gain[48];
	int32_t noise[48];
	int32_t sine[48];
	int32_t noSine[64];		/* the band's SBR band has no sinusoid */
	int8_t envelopeExponent[48];
	int8_t estimateExponent[48];
	int8_t gainExponent[48];
	int8_t noiseExponent[48];
	int8_t sineExponent[48];
	int8_t sineMapped[48];		/* the first envelope with the band's sinusoid */
	int32_t groups[50];		/* the alias reduction's groups: first, end */
} AdjustWork;

/* the limiter gains' squares (0x9008a0, 0x9008b0): -3, 0, 3 and 200 dB */
static const int32_t limiterGain[4] = { 67269013, 67108864, 133900253, 90949470 };
static const int8_t limiterExponent[4] = { 0, 1, 1, 67 };

/* the filtered gains' share of the new ones over the smoothing's first slots, Q27 */
static const int32_t smoothing[4] = { 89478485, 49011460, 19729246, 4272218 };

/* 1 / n, Q27 */
static const int32_t reciprocal[49] = {
	134217727, 134217727, 67108864, 44739244, 33554432, 26843546, 22369622,
	19173962, 16777216, 14913081, 13421773, 12201612, 11184811, 10324441,
	9586981, 8947849, 8388608, 7895161, 7456541, 7064091, 6710887,
	6391321, 6100806, 5835554, 5592406, 5368709, 5162221, 4971027,
	4793491, 4628198, 4473925, 4329604, 4194304, 4067204, 3947580,
	3834792, 3728270, 3627506, 3532046, 3441480, 3355443, 3273603,
	3195660, 3121343, 3050403, 2982616, 2917777, 2855696, 2796203,
};

/* the noise floor's table V (4.6.18.8.2), real and imaginary, Q27 */
static const int32_t noiseTable[512][2] = {
	{ -134148144, -79837288 }, { 130343472, -90635240 }, { 18965034, -127628960 },
	{ -63089708, -50117636 }, { 108320504, 39800480 }, { -52320056, 120222320 },
	{ -1413380, -89870928 }, { -122495648, -15465826 }, { 73605568, 100960408 },
	{ 53699512, -132780792 }, { -134040528, -118308992 }, { -128219640, 122015672 },
	{ -61372308, -76123360 }, { -97884552, -131544480 }, { 101499208, 28119056 },
	{ 9488445, -105022552 }, { 99987176, -122364968 }, { -129439824, -127157768 },
	{ 40835248, -66354920 }, { 89342072, 86775704 }, { 123073640, 23507024 },
	{ -94992488, 70529608 }, { -94021416, -60854356 }, { -133541960, -120892472 },
	{ 131754152, -103969288 }, { -73378952, -3450642 }, { -2267782, 385885 },
	{ -115575352, 57107744 }, { -132731912, -117952056 }, { 69466568, 89827608 },
	{ -133727872, -77990872 }, { -134176616, 132029968 }, { 74177120, 79791176 },
	{ 46414072, 127345000 }, { 84106480, -99861976 }, { -103548576, -45477876 },
	{ -122933032, 4949818 }, { -102388656, -122637248 }, { 107090096, -125065384 },
	{ 73112528, -15997688 }, { -114943096, 56948388 }, { -124664648, 37408908 },
	{ -15714710, -133950424 }, { 28664544, -121757352 }, { -102262760, 133906504 },
	{ 131682416, -128653680 }, { -115310840, 128535712 }, { -125234864, 66346184 },
	{ 40917288, -94677232 }, { 114473832, 62768440 }, { 122578480, -134002440 },
	{ -7905693, 94948072 }, { 38116072, 46484372 }, { 127853344, -73676696 },
	{ -105449936, -101426376 }, { -128566488, -27411548 }, { 110610384, 129727632 },
	{ -87490424, -119098088 }, { -125686320, 134044304 }, { 122711456, -131923280 },
	{ -94483488, 78915728 }, { 756682, 82903872 }, { 119541088, 70844616 },
	{ -92185712, 108457248 }, { 96858680, -92959008 }, { -84460864, 18289900 },
	{ 40182688, -61809048 }, { -123187656, -99338184 }, { 133276480, 54783128 },
	{ 110552856, -99369504 }, { -132221688, -134180592 }, { -128735432, -133194720 },
	{ -28737528, -125392672 }, { -92370624, -36094244 }, { 123281664, 12560402 },
	{ -128933264, 48451384 }, { 69318336, -95795664 }, { 82048264, 63015412 },
	{ 63533476, -36685972 }, { 122135864, 129809568 }, { 60189672, 133159520 },
	{ 89408992, 129641136 }, { 100558928, -120634704 }, { -133642720, 70847528 },
	{ 130729520, -22623566 }, { 97554472, -64506080 }, { 128086920, 92408376 },
	{ -97928216, -102822112 }, { -114567552, 119102296 }, { -109269912, -130836472 },
	{ -118018688, 100325480 }, { -96064096, -132299232 }, { 112104416, 112343640 },
	{ -64540024, -132672216 }, { 130377928, 107499840 }, { 69783592, 107706544 },
	{ -1138960, -102904904 }, { -94347512, 74302816 }, { -128707320, -58069976 },
	{ 130297568, 12516954 }, { -124022944, 114766496 }, { -93290008, 132383528 },
	{ 35647160, 98400800 }, { 37632560, 19512458 }, { -99506504, 133292080 },
	{ -2352563, -110886168 }, { -73989904, -132739376 }, { 131480896, -126193448 },
	{ -133139032, 89951400 }, { -90845176, 16953702 }, { 12267553, -27565276 },
	{ -96179032, -131249104 }, { 108736008, 72105320 }, { 54515204, -35526100 },
	{ -90838808, 126838504 }, { 116567792, -24606940 }, { -133547152, -3535459 },
	{ 113184720, 13967982 }, { -12369464, 93335024 }, { 134158904, -16587354 },
	{ -107015528, -122919984 }, { 129318744, 129708624 }, { -107297384, 86334080 },
	{ -15523676, 38369956 }, { -53583684, 126338616 }, { 132995272, -123564368 },
	{ 38428260, -122185168 }, { -111807024, -90369344 }, { 128049680, 65985148 },
	{ -8656860, 4362829 }, { -133647376, 56894604 }, { -87914144, 110791520 },
	{ -109057864, -69292904 }, { -133743096, 113401272 }, { 386333, 86930488 },
	{ 94189960, -27451590 }, { 129334728, 54635968 }, { -92454216, 122593072 },
	{ -46809220, 95928488 }, { 123453576, 89264792 }, { -132887696, 115250104 },
	{ 92430104, 74706016 }, { -133525704, -26914090 }, { 126452576, -133810280 },
	{ -90482384, 66502496 }, { -63537804, -115298840 }, { 19224880, -126360080 },
	{ -39283240, 7729901 }, { 58779124, -105904456 }, { -48781604, 87072992 },
	{ -11744863, 131113200 }, { -129513760, -72424296 }, { 74527000, 105886408 },
	{ 98701320, 129455784 }, { -41459552, -108265912 }, { 4798278, -130628232 },
	{ 132500664, 64973640 }, { -109641520, -121906880 }, { 91089360, 109098216 },
	{ -21217904, 114460280 }, { 108345104, -33175158 }, { 64140984, -62187300 },
	{ 129342344, 51656040 }, { -133068656, -33480984 }, { 111510608, -127212704 },
	{ -78857200, 1732445 }, { 128229080, -114832728 }, { -129507920, -85927496 },
	{ -130630224, 16613642 }, { 122675496, 77809336 }, { -134118192, 95408464 },
	{ -116602864, -27235058 }, { -35218780, -91623136 }, { -33104014, -117631464 },
	{ 3243071, 36497712 }, { 110150640, -114202896 }, { 118846272, -120308480 },
	{ -24391492, -35100816 }, { 12556708, 73611880 }, { -73374704, 128823216 },
	{ 49728996, -80410032 }, { -94453840, 122443704 }, { -46440388, -133468024 },
	{ -92307544, -40585880 }, { -36028456, 111555960 }, { 65863772, -60880772 },
	{ 52312692, 128198544 }, { -131207392, 7121451 }, { -23253964, -124514688 },
	{ 134147984, 78229536 }, { -87169376, 92134440 }, { -16128838, -76701840 },
	{ -79117936, -46771028 }, { -56123332, 21845844 }, { 134064248, 14946614 },
	{ -76033824, -121460152 }, { 126349912, 47354588 }, { -101636480, 72008552 },
	{ 27570970, -126748704 }, { 134191384, 107153952 }, { 39028204, 47504724 },
	{ -84367616, 52030432 }, { 58305396, -132266648 }, { -131934128, 28214612 },
	{ 26189944, -126486560 }, { -128146608, 132022672 }, { 125332024, -95136200 },
	{ -114401032, -11196910 }, { -115997800, -61465044 }, { 52183556, 130559528 },
	{ 123540872, -83797032 }, { 119671928, 73753928 }, { -49438212, 129464136 },
	{ 126019392, -120753480 }, { 133234792, -5042606 }, { -126249856, 55475324 },
	{ 133869064, -22589508 }, { -48183376, -62590056 }, { 7029301, -34413912 },
	{ 49262716, -51879536 }, { 123014816, -41054020 }, { 92611312, 122073936 },
	{ -51886896, 133548752 }, { -39259780, 50257820 }, { -80775184, 116473672 },
	{ -130753016, 129477864 }, { 118731112, 77186472 }, { 6977890, 28547656 },
	{ -71805976, 130515408 }, { -66343232, 131780152 }, { -132788504, -54021508 },
	{ -131642600, -97786872 }, { -36692644, 134151856 }, { 8470216, -73201792 },
	{ -27463198, -19072310 }, { 88884136, 97346216 }, { -113768776, 3184070 },
	{ -119507280, 119274704 }, { 128719240, 103005296 }, { 98655568, -5029411 },
	{ -42606660, -49437908 }, { -45782776, 53970588 }, { 64161288, -52912948 },
	{ 131934944, 2670653 }, { -41557932, -24262164 }, { 134207784, -35274932 },
	{ -125023456, -131953696 }, { 134115016, -107566104 }, { -34929048, -102005152 },
	{ -47932524, 25902630 }, { -134082280, 100187032 }, { 116175072, 74616824 },
	{ 44839516, 115676832 }, { 132889960, 6177234 }, { -89515536, -123001976 },
	{ 85921880, 21004444 }, { 133641312, 61531560 }, { -85136272, 28291912 },
	{ -10343955, -120234168 }, { 132325376, 118436032 }, { 107507504, -49461780 },
	{ 105183928, 61078460 }, { 11687420, 108634480 }, { -116516936, 52811064 },
	{ -52971080, -89670104 }, { 131366040, -97264688 }, { -127558600, 120209720 },
	{ 22824046, 73394352 }, { -103227920, -129153176 }, { 133873168, 57307156 },
	{ 128093888, 130194312 }, { 133652544, -72620952 }, { 37659160, -114570160 },
	{ 114429368, -86661176 }, { -67925632, -88377024 }, { -130474040, -30997870 },
	{ 128075992, -133197872 }, { -130092640, 99020008 }, { 41435916, 55720436 },
	{ -32915340, 84834504 }, { -45383396, -51890960 }, { -7820637, -9315749 },
	{ -30733794, 130264816 }, { -24843588, 63841688 }, { -14077075, -117802832 },
	{ -96484544, 104731408 }, { 133941072, 120851400 }, { 77260160, -122184216 },
	{ 38801872, 129262120 }, { 56625116, 64624024 }, { 125272184, -58434404 },
	{ -130308472, 116281472 }, { 49288604, 87632976 }, { -108841216, 11782129 },
	{ -35219540, -124519280 }, { 112738192, 74946976 }, { -134096416, -128882048 },
	{ 100192816, 16300600 }, { -100360760, -36101968 }, { 128555976, -106096328 },
	{ 128140760, -11527660 }, { 65375216, 134216440 }, { 62185808, 14715801 },
	{ -102672544, 119736880 }, { 77037472, 47365040 }, { 101165696, 129795544 },
	{ -79422536, -119997856 }, { 100781280, -39745456 }, { -132349224, 33601288 },
	{ -54708564, -120857120 }, { 89830944, 132378264 }, { -130813560, -255313 },
	{ 120991256, 133924312 }, { -117117432, 133189064 }, { -122848760, -21070448 },
	{ -4436888, -49936060 }, { 9694615, -119192056 }, { 133543968, 130317840 },
	{ -100535704, 134198248 }, { 6154190, 133965848 }, { -119527536, -42670384 },
	{ -112450488, 131047016 }, { 44902280, -115737984 }, { -133825248, 125141912 },
	{ -30638590, 25333274 }, { 90258800, -4893862 }, { -6907567, -124285216 },
	{ 134146992, 125661656 }, { 89860280, 132749152 }, { -133684824, -59934544 },
	{ 110199336, 133601320 }, { 133125880, 96667632 }, { -87623496, 70043832 },
	{ 126010912, -100522784 }, { 129835856, 121992928 }, { -29831190, 76670576 },
	{ -59234020, -124404856 }, { -115017848, 119245112 }, { 123189112, -62219168 },
	{ 97384320, -134082912 }, { -133830616, 78130232 }, { 104205272, 126596624 },
	{ 10358017, 78703128 }, { -75228808, 110759560 }, { 132068760, 52972300 },
	{ 63816432, 92090872 }, { 88147616, 24604306 }, { 4393450, -100573520 },
	{ -51920980, 68903824 }, { -130655952, -129586360 }, { -71514096, -122706232 },
	{ 133972528, 82052096 }, { -67450448, -119224720 }, { 2678816, 114385064 },
	{ 134124288, 126941648 }, { 111276920, -8487181 }, { -78733072, 129977488 },
	{ -23587070, -64648548 }, { 111983608, -17479780 }, { 7981246, 27529462 },
	{ 109394808, -127085328 }, { -60366276, 54887768 }, { -120455680, 134011808 },
	{ 53253912, -100468232 }, { -10185714, 99450256 }, { 102466104, 56031376 },
	{ -99978928, 127138968 }, { 87080624, 55481128 }, { 83643864, -124954440 },
	{ 56661112, -10351928 }, { 3629991, -7271270 }, { 107376560, 122865856 },
	{ -106504224, -48598760 }, { 85728032, 10909556 }, { 70988456, 80596232 },
	{ 99641296, 6028947 }, { 133004576, -26106936 }, { -107927600, -118801232 },
	{ -86721584, 96903424 }, { 15646795, -112290352 }, { -127578224, -130110536 },
	{ -83522176, 111088336 }, { 4032539, -133867280 }, { -131516216, 49024540 },
	{ -134200256, -48347388 }, { 119602288, -131391440 }, { 13969328, 103827872 },
	{ 128801688, -47561152 }, { 68240632, 128993560 }, { 22825516, -103151728 },
	{ 34725716, 134074520 }, { -1497868, 132199120 }, { -106835568, 130376968 },
	{ -133230840, -133604112 }, { -133989104, 2519453 }, { -95027512, 45205452 },
	{ -94579280, 125188600 }, { 134011064, -132507456 }, { -85047024, -22110484 },
	{ -21821410, -128767312 }, { -58580124, -127245160 }, { -134014352, 129178080 },
	{ -22543826, -132858792 }, { -118083720, -96268640 }, { 59301552, -125586152 },
	{ 125238800, -134101376 }, { -126086728, -75711384 }, { -118903488, 63920656 },
	{ 134179424, -112595192 }, { -101168472, 1093396 }, { 126013920, -15145837 },
	{ 114254768, 70261976 }, { 53286344, 109762768 }, { -49693396, -116865600 },
	{ -48351704, 46514140 }, { -125344344, -113382496 }, { -87642568, -24749180 },
	{ 16052868, 134082632 }, { 126557336, 111620704 }, { 100772208, -47691884 },
	{ 76130952, -32315382 }, { 62891428, -40453536 }, { 130610376, -133611320 },
	{ -51405360, 132227160 }, { 55063896, 2841036 }, { 12935988, 5921665 },
	{ -114465240, 122776424 }, { 119275008, -133862408 }, { -64696204, -129930288 },
	{ 37007292, 78698232 }, { -88434896, 78967848 }, { 132658232, 134210144 },
	{ -27717772, 73273544 }, { -83384664, -80387936 }, { 27273184, -116607264 },
	{ -131252256, 129239336 }, { 14914992, 28836362 }, { -55523644, 37872000 },
	{ 32390816, 68846128 }, { -89111728, -11072533 }, { -72072008, -131063480 },
	{ -130492832, 29637064 }, { 117296200, -17174734 }, { 25568962, 2150994 },
	{ -62214536, -127841096 }, { -9481270, -126808648 }, { -124076352, -14035940 },
	{ -112504784, -2275049 }, { 100951440, -134158248 }, { -56509688, 133843184 },
	{ -96763984, -46988232 }, { 105821704, 70935944 }, { 130720048, -35830688 },
	{ 133152664, -76517688 }, { 103065272, -102702544 }, { -110061784, -98690536 },
	{ 109957864, 133812960 }, { -35862776, 92480536 }, { -58131388, 114517000 },
	{ 133137248, 123314216 }, { -108302968, -43791944 }, { 57821004, -29419312 },
	{ 90878144, -128148504 }, { 75365632, -94883624 }, { 14538280, -11581429 },
	{ 122446048, -88566720 }, { -65730304, 75550144 }, { -119498952, -96175816 },
	{ 87603168, 88470968 }, { 90515736, -109634920 }, { -64116924, -22534560 },
	{ -133836520, -125581872 }, { -121989944, 83261160 }, { -8883365, -31960216 },
	{ 133453048, 25249784 }, { 131112472, -38472888 }, { 127256728, -130871200 },
	{ -128090016, -106848024 }, { -65907324, 44151208 }, { 134058240, 119445704 },
	{ 67711728, -115420632 }, { 63300960, -25072146 }, { -83324488, 100664200 },
	{ -58877312, 134215136 }, { 132379704, -71912384 }, { -82557808, -120145024 },
	{ -5155998, -93803184 }, { -40402432, -10291559 }, { 56212108, 2936817 },
	{ -115609048, 132805064 }, { 90230368, -18111862 }, { -94942128, -102740112 },
	{ 126224992, 12114762 }, { -110577088, 11978622 }, { -43044520, 67301360 },
	{ 77300232, -132830488 }, { -48755956, 9986125 }, { 134189600, -18965350 },
	{ -123971576, -131505584 }, { -59870740, -72790640 }, { 59360208, 95733152 },
	{ 4928351, 85371048 }, { 70028672, -114617680 }, { -127105720, -2451283 },
	{ -132552904, 110446040 }, { 117352992, 119989968 }, { -125375520, 55531312 },
	{ 128934840, 124979128 }, { 130908256, 115629824 }, { 133737856, 94207480 },
	{ -127111016, -39701660 }, { 122943176, -131731792 },
};

/* cpmulslla1.w: the product doubled, saturating */
static int64_t doubled(int32_t a, int32_t b)
{
	int64_t product = (int64_t)a * b;

	if (product >= (int64_t)1 << 62)
		return 0x7fffffffffffffffLL;
	return product * 2;
}

/* two floats' mantissas' product */
static int32_t floatProduct(int32_t a, int32_t b)
{
	return (int32_t)(doubled(a, b) >> 28);
}

static int64_t clip(int64_t value, int32_t bits)
{
	int64_t high = ((int64_t)1 << (bits - 1)) - 1;

	if (value > high)
		return high;
	if (value < -high - 1)
		return -high - 1;
	return value;
}

/* a wide value scaled by 2^exponent: left clipped to 56 bits, or right */
static int64_t scaleWide(int64_t value, int8_t exponent)
{
	if (exponent >= 0)
		return clip((int64_t)((uint64_t)value << (exponent & 63)), 56);
	return value >> (-exponent & 63);
}

static int32_t shiftRight(int32_t value, int32_t count)
{
	return (int32_t)((int64_t)value >> (count & 63));
}

static int32_t shiftLeft(int32_t value, int32_t count)
{
	return (int32_t)((uint64_t)(int64_t)value << (count & 63));
}

/* an energy sum as a float, divided by the slots it was summed over */
static void energyFloat(int64_t energy, int32_t slots, int8_t exponent, int32_t *mantissa,
	int8_t *outExponent)
{
	int32_t norm = aacSbrNorm64(energy);
	int32_t value;

	if (norm >= 1)
		value = (int32_t)((int64_t)((uint64_t)energy << norm) >> 27);
	else
		value = (int32_t)((energy >> (-norm & 63)) >> 27);
	if (value == 0) {
		*mantissa = 0;
		*outExponent = 0;
		return;
	}
	*mantissa = (int32_t)(((int64_t)value * slots) >> 27);
	*outExponent = (int8_t)(exponent - norm);
}

/* 0x8b8820: E_curr per QMF band (bs_interpol_freq) */
static void estimateInterpolated(int32_t **real, int32_t **imag, int32_t low, int32_t high,
	int32_t start, int32_t end, int8_t shift, int32_t *mantissa, int8_t *exponent)
{
	int32_t band, slot;

	for (band = low; band < high; band++) {
		int64_t energy = 0;

		for (slot = start; slot < end; slot++)
			energy += (int64_t)real[slot][band] * real[slot][band] +
				(int64_t)imag[slot][band] * imag[slot][band];
		energyFloat(energy, reciprocal[end - start], (int8_t)(shift << 1), mantissa++,
			exponent++);
	}
}

/* 0x8b8b40 */
static void estimateInterpolatedReal(int32_t **real, int32_t low, int32_t high, int32_t start,
	int32_t end, int8_t shift, int32_t *mantissa, int8_t *exponent)
{
	int32_t band, slot;

	for (band = low; band < high; band++) {
		int64_t energy = 0;

		for (slot = start; slot < end; slot++)
			energy += (int64_t)real[slot][band] * real[slot][band];
		energyFloat(energy, reciprocal[end - start], (int8_t)((shift << 1) + 1),
			mantissa++, exponent++);
	}
}

/* the mean over an SBR band's QMF bands, repeated for each */
static int32_t bandFloat(int64_t total, int32_t slots, int32_t bands, int8_t exponent,
	int32_t *mantissa, int8_t *outExponent, int32_t index)
{
	int32_t value, k;

	energyFloat(total, slots, exponent, &value, &exponent);
	if (value != 0)
		value = (int32_t)(((int64_t)value * reciprocal[bands]) >> 27);
	for (k = 0; k < bands; k++, index++) {
		mantissa[index] = value;
		outExponent[index] = exponent;
	}
	return index;
}

/* 0x8b8e48: E_curr per SBR band */
static void estimateBands(int32_t **real, int32_t **imag, int32_t count, const uint8_t *table,
	int32_t start, int32_t end, int8_t shift, int32_t *mantissa, int8_t *exponent)
{
	int32_t band, k, slot, index = 0;

	for (band = 0; band < count; band++) {
		int64_t total = 0;

		for (k = table[band]; k < table[band + 1]; k++) {
			int64_t energy = 0;

			for (slot = start; slot < end; slot++)
				energy += (int64_t)real[slot][k] * real[slot][k] +
					(int64_t)imag[slot][k] * imag[slot][k];
			total += energy >> 4;
		}
		index = bandFloat(total, reciprocal[end - start], table[band + 1] - table[band],
			(int8_t)((shift << 1) + 4), mantissa, exponent, index);
	}
}

/* 0x8b9288 */
static void estimateBandsReal(int32_t **real, int32_t count, const uint8_t *table,
	int32_t start, int32_t end, int8_t shift, int32_t *mantissa, int8_t *exponent)
{
	int32_t band, k, slot, index = 0;

	for (band = 0; band < count; band++) {
		int64_t total = 0;

		for (k = table[band]; k < table[band + 1]; k++) {
			int64_t energy = 0;

			for (slot = start; slot < end; slot++)
				energy += (int64_t)real[slot][k] * real[slot][k];
			total += energy >> 4;
		}
		index = bandFloat(total, reciprocal[end - start], table[band + 1] - table[band],
			(int8_t)((shift << 1) + 5), mantissa, exponent, index);
	}
}

/*
 * 0x8b9670: a band's gain, noise floor and sinusoid (4.6.18.7.4); the
 * noise is left out where a sinusoid is, and at a transient
 */
static void computeGains(int32_t envelope, int8_t envelopeExponent, int32_t estimate,
	int8_t estimateExponent, int32_t noise, int8_t noiseExponent, int32_t *gain,
	int8_t *gainExponent, int32_t *noiseLevel, int8_t *noiseLevelExponent, int32_t *sine,
	int8_t *sineExponent, int32_t sineInBand, int32_t sineHere, int32_t transient)
{
	int32_t estimatePlus, noisePlus, product;
	int8_t estimatePlusExponent, noisePlusExponent, productExponent, denominatorExponent;

	aacSbrFloatAdd(estimate, estimateExponent, ONE, 1, &estimatePlus, &estimatePlusExponent);
	product = floatProduct(envelope, noise);
	productExponent = (int8_t)(envelopeExponent + noiseExponent);
	aacSbrFloatAdd(noise, noiseExponent, ONE, 1, &noisePlus, &noisePlusExponent);
	aacSbrFloatDivide(product, productExponent, noisePlus, noisePlusExponent, noiseLevel,
		noiseLevelExponent);
	denominatorExponent = (int8_t)(estimatePlusExponent + noisePlusExponent);

	if (sineInBand) {
		aacSbrFloatDivide(product, productExponent, floatProduct(estimatePlus, noisePlus),
			denominatorExponent, gain, gainExponent);
		if (sineHere)
			aacSbrFloatDivide(envelope, envelopeExponent, noisePlus, noisePlusExponent,
				sine, sineExponent);
	} else if (transient) {
		aacSbrFloatDivide(envelope, envelopeExponent, estimatePlus, estimatePlusExponent,
			gain, gainExponent);
	} else {
		aacSbrFloatDivide(envelope, envelopeExponent, floatProduct(estimatePlus, noisePlus),
			denominatorExponent, gain, gainExponent);
	}
}

/* (1 - alpha) times a gain, the product's magnitude truncated */
static int32_t keptGain(int32_t factor, int32_t gain)
{
	uint64_t a = factor < 0 ? -(int64_t)factor : factor;
	uint64_t b = gain < 0 ? -(int64_t)gain : gain;
	int64_t magnitude = (int64_t)((a * b) >> 27);

	return (int32_t)((factor < 0) != (gain < 0) ? -magnitude : magnitude);
}

/*
 * 0x8b73e8: the low-power decoder's alias reduction (4.6.18.7.5): the bands
 * of a group share the group's mean gain as far as their alias degree says,
 * keeping the group's energy
 */
static void reduceAliasing(const int32_t *alias, int32_t *gain, int8_t *gainExponent,
	const int32_t *estimate, const int8_t *estimateExponent, const int32_t *noSine,
	int32_t count, int32_t *borders)
{
	int32_t grouping = 0, n = 0, group, k;

	for (k = 0; k < count - 1; k++) {
		if (alias[k + 1] && noSine[k]) {
			if (!grouping) {
				borders[n++] = k;
				grouping = 1;
			} else if (borders[n - 1] + 3 == k) {
				borders[n++] = k + 1;
				grouping = 0;
			}
		} else if (grouping) {
			borders[n++] = noSine[k] ? k + 1 : k;
			grouping = 0;
		}
	}
	if (grouping)
		borders[n++] = count;

	for (group = 0; group < n >> 1; group++) {
		int32_t low = borders[2 * group], high = borders[2 * group + 1];
		int32_t energy = 0, sum = 0, newSum = 0, average, ratio;
		int8_t energyExponent = 0, sumExponent = 0, newSumExponent = 0;
		int8_t averageExponent, ratioExponent;

		for (k = low; k < high; k++) {
			aacSbrFloatAdd(estimate[k], estimateExponent[k], energy, energyExponent, &energy,
				&energyExponent);
			aacSbrFloatAdd(floatProduct(estimate[k], gain[k]),
				(int8_t)(estimateExponent[k] + gainExponent[k]), sum, sumExponent, &sum,
				&sumExponent);
		}
		aacSbrFloatDivide(sum, sumExponent, energy, energyExponent, &average,
			&averageExponent);

		for (k = low; k < high; k++) {
			int32_t alpha = alias[k];

			if (k < count - 1 && alpha < alias[k + 1])
				alpha = alias[k + 1];
			aacSbrFloatAdd(floatProduct(alpha, average), averageExponent,
				keptGain(ONE_Q27 - alpha, gain[k]), gainExponent[k], &gain[k],
				&gainExponent[k]);
			aacSbrFloatAdd(floatProduct(gain[k], estimate[k]),
				(int8_t)(estimateExponent[k] + gainExponent[k]), newSum, newSumExponent,
				&newSum, &newSumExponent);
		}
		aacSbrFloatDivide(sum, sumExponent, newSum, newSumExponent, &ratio, &ratioExponent);

		for (k = low; k < high; k++) {
			gain[k] = floatProduct(gain[k], ratio);
			gainExponent[k] = (int8_t)(gainExponent[k] + ratioExponent);
		}
	}
}

/*
 * 0x8b7d78: the filtered gains brought to the new gains' exponents, as far
 * as their headroom allows; the new gains give way for the rest
 */
static void alignGains(int32_t *history, int8_t *historyExponent, int32_t *gain,
	int8_t *gainExponent, int32_t count)
{
	int32_t j;

	for (j = 0; j < count; j++) {
		int8_t difference = (int8_t)(gainExponent[j] - historyExponent[j]);
		int8_t rest;
		int32_t room;

		if (difference >= 1) {
			historyExponent[j] = (int8_t)(historyExponent[j] + difference);
			history[j] = shiftRight(history[j], difference);
			continue;
		}
		if (difference == 0)
			continue;

		room = history[j] == 0 ? 28 : aacSignBits32(history[j]) - 4;
		room = aacMin(28, room);
		if (room >= -difference) {
			history[j] = shiftLeft(history[j], -difference);
			historyExponent[j] = (int8_t)(historyExponent[j] + difference);
			continue;
		}
		history[j] = shiftLeft(history[j], room);
		historyExponent[j] = (int8_t)(historyExponent[j] - room);
		rest = (int8_t)-(difference + room);
		if (rest >= 1)
			gain[j] = shiftRight(gain[j], rest);
		else
			gain[j] = shiftLeft(gain[j], -rest);
		gainExponent[j] = (int8_t)(gainExponent[j] + rest);
	}
}

/* 0x8b8078: values sharing one exponent, rescaled to another */
static void setExponent(int32_t *values, int8_t *exponent, int8_t target, int32_t count)
{
	int8_t difference = (int8_t)(target - *exponent);
	int32_t j;

	if (difference != 0) {
		for (j = 0; j < count; j++) {
			if (difference >= 1)
				values[j] = shiftRight(values[j], difference);
			else
				values[j] = shiftLeft(values[j], -difference);
		}
	}
	*exponent = target;
}

/* a sinusoid's level at the noise floor's exponent */
static int64_t sineLevel(int32_t sine, int8_t exponent)
{
	return scaleWide((int64_t)sine << 28, exponent);
}

/*
 * 0x8b9958: one slot's bands with their (smoothed) gains, plus the noise
 * floor or the sinusoid at its phase (4.6.18.7.6)
 */
static void assemble(int32_t *real, int32_t *imag, const int32_t *historyGain,
	const int32_t *historyNoise, int32_t h, const int32_t *gain, const int8_t *gainExponent,
	const int32_t *noise, const int32_t *sine, const int8_t *sineExponent,
	int8_t noiseExponent, uint8_t *phaseIndex, int32_t kx, int32_t count, int32_t shift,
	int32_t transient, int32_t *noiseIndex)
{
	int32_t phase = (int8_t)*phaseIndex, index = *noiseIndex, odd = kx & 1, j;

	for (j = 0; j < count; j++) {
		int32_t g = gain[j], q = noise[j];
		int64_t re, im, s;

		if (h >= 1) {
			g = (int32_t)((doubled(h, historyGain[j]) + doubled(gain[j], ONE_Q27 - h)) >> 28);
			q = (int32_t)((doubled(h, historyNoise[j]) + doubled(noise[j], ONE_Q27 - h)) >>
				28);
		}
		re = scaleWide(doubled(real[j], g), (int8_t)(gainExponent[j] - shift));
		im = scaleWide(doubled(imag[j], g), (int8_t)(gainExponent[j] - shift));
		index = (index + 1) & 0x1ff;

		if (sine[j]) {
			s = sineLevel(sine[j], (int8_t)(sineExponent[j] - noiseExponent));
			if (phase == 1 || phase == 3) {
				if ((phase == 1) == !odd)
					im += s;
				else
					im -= s;
			} else if (phase == 0) {
				re += s;
			} else {
				re -= s;
			}
		} else if (!transient) {
			re += doubled(noiseTable[index][0], q);
			im += doubled(noiseTable[index][1], q);
		}
		real[j] = (int32_t)(re >> 28);
		imag[j] = (int32_t)(im >> 28);
		odd = !odd;
	}
	*phaseIndex = (phase + 1) & 3;
	*noiseIndex = index;
}

/* a band's sample with its gain, clipped to 28 bits before the exponent */
static int32_t gainedEdge(int32_t sample, int32_t gain, int8_t exponent)
{
	int64_t value = clip(doubled(gain, sample) >> 28, 28);

	return (int32_t)(scaleWide(value << 28, exponent) >> 28);
}

static int32_t gained(int32_t sample, int32_t gain, int8_t exponent)
{
	return (int32_t)(scaleWide(doubled(gain, sample), exponent) >> 28);
}

static int32_t noiseSample(int32_t index, int32_t noise)
{
	return (int32_t)(doubled(noiseTable[index][0], noise) >> 28);
}

static int32_t leak(int32_t sine)
{
	return (int32_t)(doubled(SINE_LEAK, sine) >> 28);
}

/*
 * 0x8ba168: the real-valued slot; a sinusoid at an imaginary phase leaks
 * into the bands either side, the one below kx at the low band's exponent
 * (4.6.18.8.4)
 */
static void assembleReal(int32_t *real, const int32_t *gain, const int8_t *gainExponent,
	const int32_t *noise, const int32_t *sine, const int8_t *sineExponent,
	int8_t noiseExponent, uint8_t *phaseIndex, int32_t kx, int32_t count, int32_t shift,
	int8_t lowExponent, int32_t transient, int32_t *noiseIndex)
{
	int32_t phase = (int8_t)*phaseIndex, index = *noiseIndex, odd = kx & 1;
	int32_t sines = 0, previous, current, next, y, j;
	int8_t lowShift = (int8_t)(lowExponent - noiseExponent);

	/* the first band */
	y = gainedEdge(real[0], gain[0], (int8_t)(gainExponent[0] - shift));
	index = (index + 1) & 0x1ff;
	current = (int32_t)(sineLevel(sine[0], (int8_t)(sineExponent[0] - noiseExponent)) >> 28);
	next = 0;
	if (count >= 2)
		next = (int32_t)(sineLevel(sine[1], (int8_t)(sineExponent[1] - noiseExponent)) >> 28);
	if (sine[0])
		sines = 1;
	else if (!transient)
		y += noiseSample(index, noise[0]);

	if (phase == 0) {
		y += current;
	} else if (phase == 2) {
		y -= current;
	} else {
		int64_t below = doubled(SINE_LEAK, current);
		int32_t level;

		below = lowShift >= 0 ? below >> (lowShift & 63) :
			(int64_t)((uint64_t)below << (-lowShift & 63));
		level = (int32_t)clip(below >> 28, 28);
		if ((phase == 1) == !!odd) {
			real[-1] += level;
			y -= leak(next);
		} else {
			real[-1] -= level;
			y += leak(next);
		}
	}
	real[0] = y;
	odd = !odd;

	/* the middle bands */
	for (j = 1; j < count - 1; j++) {
		y = gained(real[j], gain[j], (int8_t)(gainExponent[j] - shift));
		index = (index + 1) & 0x1ff;
		previous = current;
		current = next;
		next = (int32_t)(sineLevel(sine[j + 1], (int8_t)(sineExponent[j + 1] - noiseExponent)) >>
			28);
		if (sine[j])
			sines++;
		else if (!transient)
			y += noiseSample(index, noise[j]);

		if (phase == 0) {
			y += current;
		} else if (phase == 2) {
			y -= current;
		} else if (sines < 17) {
			if ((phase == 1) == !!odd)
				y += leak(previous - next);
			else
				y -= leak(previous - next);
		}
		real[j] = y;
		odd = !odd;
	}

	/* the last band, leaking into the one above it */
	if (count >= 2) {
		int32_t level;

		y = gainedEdge(real[j], gain[j], (int8_t)(gainExponent[j] - shift));
		index = (index + 1) & 0x1ff;
		level = (int32_t)clip(doubled(current, SINE_LEAK) >> 28, 28);
		current = next;
		if (sine[j])
			sines++;
		else if (!transient)
			y += noiseSample(index, noise[j]);

		if (phase == 0) {
			real[j] = y + current;
		} else if (phase == 2) {
			real[j] = y - current;
		} else if (sines >= 17) {
			real[j] = y;
		} else if ((phase == 1) == !!odd) {
			real[j] = y + level;
			if (kx + j < 62)
				real[j + 1] -= leak(current);
		} else {
			real[j] = y - level;
			if (kx + j < 62)
				real[j + 1] += leak(current);
		}
	}
	*phaseIndex = (phase + 1) & 3;
	*noiseIndex = index;
}

/*
 * 0x8521b8: the first envelope each high band's sinusoid is in: 0 for one
 * the last frame already had, the transient envelope (l_A) for a new one,
 * 5 (none) elsewhere; the bands' flags are kept 16 per word, from the top
 */
static void mapSines(const uint8_t *table, int32_t count, const uint8_t *harmonics,
	uint32_t *mask, int8_t pointer, int8_t *sineMapped)
{
	uint32_t previous = 0, flags = 0;
	int32_t bit = 0, i;

	for (i = 0; i < 48; i++)
		sineMapped[i] = 5;
	for (i = count - 1; i >= 0; i--) {
		if (bit == 0) {
			flags = 0;
			previous = *mask;
		}
		if ((int8_t)harmonics[i]) {
			flags |= 1u << bit;
			sineMapped[(table[i] + table[i + 1] - 2 * table[0]) >> 1] =
				previous & (1u << bit) ? 0 : pointer;
		}
		if (++bit == 16 || i == 0) {
			*mask++ = flags;
			bit = 0;
		}
	}
}

/* 0x8522c6 */
static void saveHistory(int32_t *gain, int32_t *noise, const int32_t *newGain,
	const int32_t *newNoise, int32_t count)
{
	int32_t j;

	for (j = 0; j < count; j++) {
		gain[j] = newGain[j];
		noise[j] = newNoise[j];
	}
}

/* 0x8522e4: a limiter band's E_orig and E_curr sums (from 1), and their ratio */
static void limiterEnergies(const int32_t *envelope, const int8_t *envelopeExponent,
	const int32_t *estimate, const int8_t *estimateExponent, int32_t low, int32_t high,
	int32_t *sum, int8_t *sumExponent, int32_t *ratio, int8_t *ratioExponent)
{
	int32_t envelopeSum = 1, estimateSum = 1, k;
	int8_t envelopeSumExponent = 0, estimateSumExponent = 0;

	for (k = low; k < high; k++) {
		aacSbrFloatAdd(envelopeSum, envelopeSumExponent, envelope[k], envelopeExponent[k],
			&envelopeSum, &envelopeSumExponent);
		aacSbrFloatAdd(estimateSum, estimateSumExponent, estimate[k], estimateExponent[k],
			&estimateSum, &estimateSumExponent);
	}
	aacSbrFloatDivide(envelopeSum, envelopeSumExponent, estimateSum, estimateSumExponent,
		ratio, ratioExponent);
	*sum = envelopeSum;
	*sumExponent = envelopeSumExponent;
}

/* the gains of one limiter band limited, then boosted back to its energy (4.6.18.7.5) */
static void limit(AdjustWork *w, int32_t low, int32_t high, int32_t gainLimit,
	int8_t gainLimitExponent, int32_t transient)
{
	int32_t sum, maximum, denominator = 0, boost, ratio, k;
	int8_t sumExponent, maximumExponent, denominatorExponent = 0, boostExponent;
	int8_t ratioExponent;

	limiterEnergies(w->envelope, w->envelopeExponent, w->estimate, w->estimateExponent, low,
		high, &sum, &sumExponent, &maximum, &maximumExponent);
	maximum = floatProduct(maximum, gainLimit);
	maximumExponent = (int8_t)(maximumExponent + gainLimitExponent);
	if (maximum == 0) {
		maximumExponent = -28;
	} else {
		int64_t wide = (int64_t)maximum << 28;
		int32_t norm = (int8_t)aacFixedNorm64(&wide);

		maximumExponent = (int8_t)(maximumExponent - norm);
		maximum = shiftLeft(maximum, norm);
		if (maximumExponent >= MAX_EXPONENT) {
			maximumExponent = MAX_EXPONENT;
			maximum = 0x04000000;
		}
	}

	for (k = low; k < high; k++) {
		if (maximumExponent < w->gainExponent[k] ||
		    (w->gainExponent[k] == maximumExponent && maximum < w->gain[k])) {
			aacSbrFloatDivide(maximum, maximumExponent, w->gain[k], w->gainExponent[k],
				&ratio, &ratioExponent);
			w->noise[k] = floatProduct(w->noise[k], ratio);
			w->gain[k] = maximum;
			w->gainExponent[k] = maximumExponent;
			w->noiseExponent[k] = (int8_t)(w->noiseExponent[k] + ratioExponent);
		}
	}

	for (k = low; k < high; k++) {
		aacSbrFloatAdd(floatProduct(w->gain[k], w->estimate[k]),
			(int8_t)(w->estimateExponent[k] + w->gainExponent[k]), denominator,
			denominatorExponent, &denominator, &denominatorExponent);
		if (w->sine[k])
			aacSbrFloatAdd(w->sine[k], w->sineExponent[k], denominator,
				denominatorExponent, &denominator, &denominatorExponent);
		else if (!transient)
			aacSbrFloatAdd(w->noise[k], w->noiseExponent[k], denominator,
				denominatorExponent, &denominator, &denominatorExponent);
	}
	aacSbrFloatDivide(sum, sumExponent, denominator, denominatorExponent, &boost,
		&boostExponent);
	if (boostExponent >= 3 || (boostExponent == 2 && boost > BOOST_MAX)) {
		boostExponent = 2;
		boost = BOOST_MAX;
	}

	for (k = low; k < high; k++) {
		w->gain[k] = floatProduct(w->gain[k], boost);
		w->gainExponent[k] = (int8_t)(w->gainExponent[k] + boostExponent);
		w->sine[k] = floatProduct(w->sine[k], boost);
		w->sineExponent[k] = (int8_t)(w->sineExponent[k] + boostExponent);
		w->noise[k] = floatProduct(w->noise[k], boost);
		w->noiseExponent[k] = (int8_t)(w->noiseExponent[k] + boostExponent);
	}
}

/* the envelopes' highest exponent: the scale the adjusted bands are written at */
static int8_t envelopeExponent(const AacSbrFrame *frame, int32_t *index, int32_t count)
{
	int32_t peak = -28, i;

	for (i = 0; i < count; i++, (*index)++)
		peak = aacMax(peak, (int8_t)((frame->envelope[*index] & 0xff) - 16));
	return (int8_t)(((peak + 1) >> 1) + 6);
}

/*
 * 0x8bb408: the frame's envelopes adjusted. QMF rows below numSlots are
 * this frame's, those from numSlots on overlap into the next; each part
 * keeps its own exponent (timing[3] and timing[4]).
 */
void aacSbrAdjustEnvelopes(int32_t *timing, AacSbrAdjust *adjust, AacSbrHeader *header,
	AacSbrFrame *frame, int32_t **real, int32_t **imag, int32_t *aliasDegree, int32_t *work,
	uint8_t created)
{
	AacSbrBands *bands = header->bands;
	const AacSbrGrid *grid = &frame->grid;
	AdjustWork *w = (AdjustWork *)work;
	int32_t numSlots = header->timeStep * header->timeSlots;
	int32_t kx = bands->kx, stop = bands->stop, count = stop - kx;
	uint8_t first = header->timeStep * grid->borders[0];
	int8_t hfExponent = (int8_t)(15 - timing[3]), leadExponent = (int8_t)(15 - timing[4]);
	int32_t gainLimit = limiterGain[header->limiterGains];
	int8_t gainLimitExponent = limiterExponent[header->limiterGains];
	const int32_t *noise = frame->noise;
	int32_t peak = 1, envelopeIndex, noiseEnvelope = 0, l, i, j;
	int8_t exponent, overlapExponent = 0, slotExponent;
	int32_t lowHeadroom, highHeadroom, headroom;

	mapSines(bands->table[1], bands->numHigh, frame->harmonics, adjust->sineMask,
		grid->pointer, w->sineMapped);

	/* the scales: the noise history's, and the envelopes' either side of numSlots */
	for (j = 0; j < count; j++)
		if (peak < adjust->history[1][j])
			peak = adjust->history[1][j];
	exponent = (int8_t)(adjust->noiseExponent - aacFixedNorm(peak));
	envelopeIndex = 0;
	for (l = 0; l < grid->numEnvelopes; l++) {
		int8_t e = envelopeExponent(frame, &envelopeIndex,
			grid->freqRes[l] ? bands->numHigh : bands->numLow);

		if (grid->borders[l] < header->timeSlots)
			exponent = aacMax(exponent, e);
		if (header->timeSlots < grid->borders[l + 1])
			overlapExponent = aacMax(overlapExponent, e);
	}

	envelopeIndex = 0;
	for (l = 0; l < grid->numEnvelopes; l++) {
		uint8_t start = header->timeStep * grid->borders[l];
		uint8_t end = header->timeStep * grid->borders[l + 1];
		int32_t res = grid->freqRes[l];
		int32_t numBands = res ? bands->numHigh : bands->numLow;
		const uint8_t *table = bands->table[res];
		int32_t transient, smoothingLength, noiseBand, noiseBorder, mapped, band, slot;

		if (grid->borders[l] == grid->noiseBorders[noiseEnvelope + 1]) {
			noiseEnvelope++;
			noise += bands->numNoise;
		}
		if (l == grid->pointer || l == adjust->previousTransient) {
			transient = 1;
			smoothingLength = 0;
		} else {
			transient = 0;
			smoothingLength = (int8_t)((1 - header->smoothingMode) << 2);
		}

		if (header->interpolFreq) {
			if (created)
				estimateInterpolated(real, imag, kx, stop, start, end, hfExponent,
					w->estimate, w->estimateExponent);
			else
				estimateInterpolatedReal(real, kx, stop, start, end, hfExponent,
					w->estimate, w->estimateExponent);
		} else {
			if (created)
				estimateBands(real, imag, numBands, table, start, end, hfExponent,
					w->estimate, w->estimateExponent);
			else
				estimateBandsReal(real, numBands, table, start, end, hfExponent,
					w->estimate, w->estimateExponent);
		}

		/* the gains, noise floor and sinusoids per QMF band */
		noiseBand = 0;
		noiseBorder = bands->noise[1];
		mapped = 0;
		j = 0;
		for (band = 0; band < numBands; band++, envelopeIndex++) {
			int32_t low = table[band], high = table[band + 1], sine = 0, k;

			for (k = low; k < high; k++, mapped++)
				if (w->sineMapped[mapped] <= l)
					sine = 1;
			for (k = low; k < high; k++, j++) {
				int32_t value = frame->envelope[envelopeIndex], q;

				if (k >= noiseBorder) {
					noiseBand++;
					noiseBorder = bands->noise[noiseBand + 1];
				}
				w->noSine[k - kx] = !sine;
				w->envelopeExponent[j] = (int8_t)((value & 0xff) - 16);
				w->envelope[j] = value & 0x0fffff00;
				w->sineExponent[j] = 0;
				w->sine[j] = 0;
				q = noise[noiseBand];
				computeGains(w->envelope[j], w->envelopeExponent[j], w->estimate[j],
					w->estimateExponent[j], q & 0x0fffff00, (int8_t)((int8_t)q - 38),
					&w->gain[j], &w->gainExponent[j], &w->noise[j], &w->noiseExponent[j],
					&w->sine[j], &w->sineExponent[j], sine, w->sineMapped[j] <= l,
					transient);
			}
		}

		for (i = 0; i < bands->numLimiter; i++)
			limit(w, bands->limiter[i], bands->limiter[i + 1], gainLimit,
				gainLimitExponent, transient);

		reduceAliasing(aliasDegree + kx, w->gain, w->gainExponent, w->estimate,
			w->estimateExponent, w->noSine, count, w->groups);

		/* amplitudes from here on; the noise at the slots' exponent */
		slotExponent = start < numSlots ? exponent : overlapExponent;
		for (j = 0; j < count; j++) {
			aacSbrFloatSqrt(&w->sine[j], &w->sineExponent[j]);
			aacSbrFloatSqrt(&w->gain[j], &w->gainExponent[j]);
			aacSbrFloatSqrt(&w->noise[j], &w->noiseExponent[j]);
			w->noise[j] = shiftRight(w->noise[j],
				(int8_t)(slotExponent - w->noiseExponent[j]));
		}
		if (adjust->first) {
			int8_t *historyExponent = (int8_t *)adjust->history[2];

			adjust->noiseExponent = slotExponent;
			for (j = 0; j < count; j++) {
				historyExponent[j] = w->gainExponent[j];
				adjust->history[1][j] = w->noise[j];
				adjust->history[0][j] = w->gain[j];
			}
			adjust->first = 0;
		}
		alignGains(adjust->history[0], (int8_t *)adjust->history[2], w->gain,
			w->gainExponent, count);

		for (slot = start; slot < end; slot++) {
			int32_t shift = (slot < numSlots ? exponent : overlapExponent) - hfExponent;
			int32_t h = 0;

			if (slot == numSlots && start < numSlots)
				setExponent(w->noise, &slotExponent, overlapExponent, count);
			setExponent(adjust->history[1], &adjust->noiseExponent, slotExponent, count);
			if (slot - start < smoothingLength)
				h = smoothing[slot - start];
			if (created)
				assemble(real[slot] + kx, imag[slot] + kx, adjust->history[0],
					adjust->history[1], h, w->gain, w->gainExponent, w->noise, w->sine,
					w->sineExponent, slotExponent, &adjust->phaseIndex, kx, count, shift,
					transient, &adjust->noiseIndex);
			else
				assembleReal(real[slot] + kx, w->gain, w->gainExponent, w->noise,
					w->sine, w->sineExponent, slotExponent, &adjust->phaseIndex, kx,
					count, shift, (int8_t)(15 - timing[0]), transient,
					&adjust->noiseIndex);
		}
		saveHistory(adjust->history[0], adjust->history[1], w->gain, w->noise, count);
	}

	/* the high band brought to one exponent per part, as high as it fits */
	if (created) {
		lowHeadroom = aacSbrHeadroom(real, imag, kx, stop, 0, first);
		highHeadroom = aacSbrHeadroom(real, imag, kx, stop, first, numSlots);
	} else {
		lowHeadroom = aacSbrHeadroomReal(real, kx, stop, 0, first);
		highHeadroom = aacSbrHeadroomReal(real, kx, stop, first, numSlots);
	}
	headroom = (int8_t)aacMin(exponent - highHeadroom, leadExponent - lowHeadroom);
	if (created) {
		aacSbrRescale(real, imag, kx, stop, 0, first, leadExponent - headroom);
		aacSbrRescale(real, imag, kx, stop, first, numSlots, exponent - headroom);
	} else {
		aacSbrRescaleReal(real, kx, stop, 0, first, leadExponent - headroom);
		aacSbrRescaleReal(real, kx, stop, first, numSlots, exponent - headroom);
	}
	timing[3] = 15 - headroom;
	timing[4] = 15 - overlapExponent;
	adjust->previousTransient = grid->pointer == grid->numEnvelopes ? 0 : -1;
}
