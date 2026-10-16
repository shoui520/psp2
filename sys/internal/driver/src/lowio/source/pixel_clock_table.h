/* DSI pixel clocks in rodata order: PLL coefficient sets (named after the
 * first clock that uses each), each clock's PLL, source and divider words
 * with its frequency and pixel frequency in MHz, and the clock index
 * (clocks in units of 100 Hz). */

static const uint32_t s_pixelPll1006301[3];
static const uint32_t s_pixelPll1080000[3];
static const uint32_t s_pixelPll1948052[3];
static const uint32_t s_pixelPll1558442[3];
static const uint32_t s_pixelPll2000000[3];
static const uint32_t s_pixelPll3409091[3];
static const uint32_t s_pixelPll3708791[3];
static const uint32_t s_pixelPll1600000[3];
static const uint32_t s_pixelPll3252273[3];
static const uint32_t s_pixelPll1258741[3];
static const uint32_t s_pixelPll2653427[3];
static const uint32_t s_pixelPll2243100[3];
static const uint32_t s_pixelPll2356363[3];
static const uint32_t s_pixelPll2298462[3];

static const uint32_t s_pixelPll1006301[3] = {
	0x00000017, 0x00000b01, 0x04180301,
};

static const uint32_t s_pixelPll1080000[3] = {
	0x004c074c, 0x00000904, 0x10180301,
};

static const uint32_t s_pixelPll1948052[3] = {
	0x00060063, 0x00000a01, 0x10000301,
};

static const PixelClockSetting s_pixelClock3712500 = {
	0x00000000, 0x00000001, s_pixelPll1080000,
	{ 0x00000036, 0x00000000, 0x08100301 },
	371.25, 74.25,
};

static const PixelClockSetting s_pixelClock1006301 = {
	0x00000010, 0x00000000, s_pixelPll1006301,
	{ 0x000c0060, 0x00000001, 0x10000301 },
	100.73077392578125, 25.182693481445312,
};

static const PixelClockSetting s_pixelClock3409091 = {
	0x00000010, 0x00000001, s_pixelPll3409091,
	{ 0x00080063, 0x00000000, 0x10000301 },
	340.9090881347656, 68.18181610107422,
};

static const PixelClockSetting s_pixelClock2356363 = {
	0x00000010, 0x00000002, s_pixelPll2356363,
	{ 0x0009004f, 0x00000000, 0x08100301 },
	216.0, 36.0,
};

static const uint32_t s_pixelPll1558442[3] = {
	0x0002014f, 0x00000a00, 0x10000301,
};

static const PixelClockSetting s_pixelClock1258741 = {
	0x00000010, 0x00000001, s_pixelPll1258741,
	{ 0x000a014f, 0x00000200, 0x10000301 },
	125.8741226196289, 25.174823760986328,
};

static const PixelClockSetting s_pixelClock1350000 = {
	0x00000000, 0x00000001, s_pixelPll1080000,
	{ 0x00000027, 0x00000100, 0x04180301 },
	270.0, 54.0,
};

static const PixelClockSetting s_pixelClock3252273 = {
	0x00000010, 0x00000001, s_pixelPll3252273,
	{ 0x00030027, 0x00000000, 0x04180301 },
	325.227294921875, 65.04545593261719,
};

static const PixelClockSetting s_pixelClock1080000 = {
	0x00000000, 0x00000000, s_pixelPll1080000,
	{ 0x0000000f, 0x00000002, 0x04180301 },
	108.0, 27.0,
};

static const PixelClockSetting s_pixelClock2967033 = {
	0x00000010, 0x00000000, s_pixelPll1258741,
	{ 0x0006004f, 0x00000000, 0x08100301 },
	296.70330810546875, 74.17582702636719,
};

static const PixelClockSetting s_pixelClock2653427 = {
	0x00000010, 0x00000000, s_pixelPll2653427,
	{ 0x000c007b, 0x00000000, 0x10000301 },
	265.3426513671875, 66.33566284179688,
};

static const PixelClockSetting s_pixelClock2727273 = {
	0x00000010, 0x00000000, s_pixelPll1600000,
	{ 0x000a0063, 0x00000000, 0x10000301 },
	272.7272644042969, 68.18181610107422,
};

static const PixelClockSetting s_pixelClock2000000 = {
	0x00000010, 0x00000001, s_pixelPll2000000,
	{ 0x0005001f, 0x00000000, 0x04180301 },
	200.0, 40.0,
};

static const uint32_t s_pixelPll2000000[3] = {
	0x00020031, 0x00000202, 0x10000301,
};

static const PixelClockSetting s_pixelClock2298462 = {
	0x00000010, 0x00000002, s_pixelPll2298462,
	{ 0x000c005f, 0x00000000, 0x04180301 },
	229.84616088867188, 38.30769348144531,
};

static const PixelClockSetting s_pixelClock2600000 = {
	0x00000010, 0x00000000, s_pixelPll1600000,
	{ 0x00020019, 0x00000000, 0x04180301 },
	260.0, 65.0,
};

static const uint32_t s_pixelPll3409091[3] = {
	0x00000031, 0x00000a00, 0x04180301,
};

static const PixelClockSetting s_pixelClock1558442 = {
	0x00000010, 0x00000000, s_pixelPll1558442,
	{ 0x00140713, 0x00000100, 0x10000301 },
	155.84414672851562, 38.961036682128906,
};

static const PixelClockSetting s_pixelClock3708791 = {
	0x00000010, 0x00000001, s_pixelPll3708791,
	{ 0x00000031, 0x00000000, 0x04180301 },
	370.8791198730469, 74.17582702636719,
};

static const uint32_t s_pixelPll3708791[3] = {
	0x00060063, 0x00000c01, 0x10000301,
};

static const uint32_t s_pixelPll1600000[3] = {
	0x00020027, 0x00000203, 0x10000301,
};

static const PixelClockSetting s_pixelClock1948052 = {
	0x00000010, 0x00000001, s_pixelPll1948052,
	{ 0x00080163, 0x00000000, 0x10000301 },
	194.80520629882812, 38.96104049682617,
};

static const uint32_t s_pixelPll3252273[3] = {
	0x00000034, 0x00000a00, 0x08100301,
};

static const uint32_t s_pixelPll1258741[3] = {
	0x00000031, 0x00000c00, 0x04180301,
};

static const uint32_t s_pixelPll2653427[3] = {
	0x00020143, 0x00000a00, 0x10000301,
};

static const uint32_t s_pixelPll2243100[3] = {
	0x0000001a, 0x00000c01, 0x08100301,
};

static const PixelClockSetting s_pixelClock2970000 = {
	0x00000000, 0x00000000, s_pixelPll1080000,
	{ 0x0000002b, 0x00000000, 0x04180301 },
	297.0, 74.25,
};

static const uint32_t s_pixelPll2356363[3] = {
	0x00020147, 0x00000a00, 0x10000301,
};

static const PixelClockSetting s_pixelClock2243100 = {
	0x00000010, 0x00000002, s_pixelPll2243100,
	{ 0x0001001f, 0x00000000, 0x04180301 },
	224.3076934814453, 37.38461685180664,
};

static const uint32_t s_pixelPll2298462[3] = {
	0x00010052, 0x00000800, 0x04180301,
};

static const PixelClockSetting s_pixelClock1600000 = {
	0x00000010, 0x00000000, s_pixelPll1600000,
	{ 0x0002001f, 0x00000100, 0x04180301 },
	160.0, 40.0,
};

static const PixelClockEntry s_pixelClocks[20] = {
	{ 1006301, &s_pixelClock1006301 },
	{ 1080000, &s_pixelClock1080000 },
	{ 1258741, &s_pixelClock1258741 },
	{ 1350000, &s_pixelClock1350000 },
	{ 1558442, &s_pixelClock1558442 },
	{ 1600000, &s_pixelClock1600000 },
	{ 1948052, &s_pixelClock1948052 },
	{ 2000000, &s_pixelClock2000000 },
	{ 2243100, &s_pixelClock2243100 },
	{ 2298462, &s_pixelClock2298462 },
	{ 2356363, &s_pixelClock2356363 },
	{ 2600000, &s_pixelClock2600000 },
	{ 2653427, &s_pixelClock2653427 },
	{ 2727273, &s_pixelClock2727273 },
	{ 2967033, &s_pixelClock2967033 },
	{ 2970000, &s_pixelClock2970000 },
	{ 3252273, &s_pixelClock3252273 },
	{ 3409091, &s_pixelClock3409091 },
	{ 3708791, &s_pixelClock3708791 },
	{ 3712500, &s_pixelClock3712500 },
};
