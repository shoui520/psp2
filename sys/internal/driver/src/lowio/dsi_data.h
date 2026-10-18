/* DSI video modes, the PHY timing of each link rate, and the VIC table the
 * SceDsiForDriver exports look modes up in. Modes are named after their
 * resolution and field rate (rates rounded: 60 is 59.94); the PHY timings
 * after the pixel clock (ScePervasiveForDriver_91C80C41 units) they serve. */
#pragma once
#include <stddef.h>
#include <stdint.h>

typedef struct DsiPhyTiming {
	uint32_t timing[9];		/* phyTiming[0-8]; timing[1] also sets the clock lane */
} DsiPhyTiming;

#define DSI_MODE_HFP_BLANKING	0x1	/* send the front porch as a blanking packet */
#define DSI_MODE_HSA_BLANKING	0x2	/* the sync width */
#define DSI_MODE_HBP_BLANKING	0x4	/* the back porch */
#define DSI_MODE_NARROW		0x8	/* two pixels fewer than the timing gives */

typedef struct DsiVicMode {
	uint32_t flags;			/* DSI_MODE_* */
	uint32_t pixelClock24;		/* 24 bits per pixel */
	const DsiPhyTiming *phy24;
	uint32_t pixelClock30;		/* 30 bits per pixel; 0: not supported */
	const DsiPhyTiming *phy30;
	uint32_t hTotal;
	uint32_t vTotal;
	uint32_t interlaced;
	uint32_t hFrontPorch;
	uint32_t hSyncWidth;
	uint32_t hBackPorch;
	uint32_t vFrontPorch;
	uint32_t vSyncWidth;
	uint32_t vBackPorch;
} DsiVicMode;

typedef struct DsiVicEntry {
	uint32_t vic;
	const DsiVicMode *mode;		/* NULL: not available on this panel */
} DsiVicEntry;

/* The OLED timing for this SoC revision, chosen by DsiInit. */
static DsiPhyTiming s_dsiPhyOledCurrent;

static const DsiPhyTiming s_dsiPhyOledEs = {{
	0x00002bcf, 0x00000006, 0x00000f05, 0x00000001, 0x00000806,
	0x00004906, 0x00000016, 0x00000003, 0x00000020,
}};

static const DsiPhyTiming s_dsiPhy1350000 = {{
	0x00001a60, 0x00000003, 0x00000b03, 0x00000003, 0x00000003,
	0x000041ec, 0x00000007, 0x00000000, 0x00000020,
}};

static const DsiPhyTiming s_dsiPhy2970000 = {{
	0x00003a03, 0x00000007, 0x00001507, 0x00000005, 0x00000507,
	0x00004884, 0x0000000b, 0x00000000, 0x00000020,
}};

static const DsiPhyTiming s_dsiPhy2967033 = {{
	0x000039f5, 0x00000007, 0x00001507, 0x00000005, 0x00000507,
	0x00004871, 0x0000000b, 0x00000000, 0x00000020,
}};

static const DsiPhyTiming s_dsiPhy2653427 = {{
	0x000033e0, 0x00000007, 0x0000150a, 0x00000002, 0x0000040b,
	0x000040ca, 0x00000008, 0x00000004, 0x00000020,
}};

static const DsiPhyTiming s_dsiPhy3708791 = {{
	0x00004876, 0x00000009, 0x00001f09, 0x00000005, 0x00000709,
	0x00004871, 0x0000000b, 0x00000001, 0x00000020,
}};

static const DsiPhyTiming s_dsiPhy3712500 = {{
	0x0000488a, 0x00000009, 0x00001f09, 0x00000005, 0x00000709,
	0x00004890, 0x0000000b, 0x00000001, 0x00000020,
}};

static const DsiPhyTiming s_dsiPhy2356363 = {{
	0x00002bcf, 0x00000006, 0x00001108, 0x00000001, 0x00000809,
	0x00004906, 0x00000065, 0x00000003, 0x00000020,
}};

static const DsiPhyTiming s_dsiPhy1080000 = {{
	0x00001519, 0x00000003, 0x00000703, 0x00000003, 0x00000003,
	0x000034bd, 0x00000007, 0x00000000, 0x00000020,
}};

static const DsiPhyTiming s_dsiPhyOled = {{
	0x00002bcf, 0x00000006, 0x00001007, 0x00000001, 0x00000808,
	0x00004906, 0x00000016, 0x00000003, 0x00000020,
}};

static const DsiVicMode s_dsiMode1080p50 = {
	0x00000000, 0x002d5190, &s_dsiPhy2970000, 0x0038a5f4, &s_dsiPhy3712500,
	2640, 1125, 0,
	528, 44, 148,
	4, 5, 36,
};

static const DsiVicMode s_dsiMode1080i50 = {
	0x00000000, 0x002d5190, &s_dsiPhy2970000, 0x0038a5f4, &s_dsiPhy3712500,
	2640, 1125, 1,
	528, 44, 148,
	2, 5, 16,
};

static const DsiVicMode s_dsiMode576p50Alt = {
	0x00000001, 0x00107ac0, &s_dsiPhy1080000, 0x00149970, &s_dsiPhy1350000,
	864, 626, 0,
	12, 64, 68,
	6, 5, 40,
};

static const DsiVicMode s_dsiModeOled50 = {
	0x00000005, 0x00223a1c, &s_dsiPhyOledCurrent, 0x00000000, NULL,
	1250, 594, 0,
	20, 266, 4,
	4, 4, 46,
};

static const DsiVicMode s_dsiModeLcd60 = {
	0x00000004, 0x0023f48b, &s_dsiPhy2356363, 0x00000000, NULL,
	1120, 585, 0,
	136, 4, 20,
	4, 4, 33,
};

static const DsiVicMode s_dsiMode1080p60 = {
	0x00000000, 0x002d45f9, &s_dsiPhy2967033, 0x00389777, &s_dsiPhy3708791,
	2200, 1125, 0,
	88, 44, 148,
	4, 5, 36,
};

static const DsiVicMode s_dsiMode1080i60 = {
	0x00000000, 0x002d45f9, &s_dsiPhy2967033, 0x00389777, &s_dsiPhy3708791,
	2200, 1125, 1,
	88, 44, 148,
	2, 5, 16,
};

static const DsiVicMode s_dsiModeOled60 = {
	0x00000005, 0x00223a1c, &s_dsiPhyOledCurrent, 0x00000000, NULL,
	1050, 594, 0,
	20, 66, 4,
	4, 4, 42,
};

static const DsiVicMode s_dsiMode720p50 = {
	0x00000000, 0x002d5190, &s_dsiPhy2970000, 0x0038a5f4, &s_dsiPhy3712500,
	1980, 750, 0,
	440, 40, 220,
	5, 5, 20,
};

static const DsiVicMode s_dsiMode576p50 = {
	0x00000001, 0x00107ac0, &s_dsiPhy1080000, 0x00149970, &s_dsiPhy1350000,
	864, 625, 0,
	12, 64, 68,
	5, 5, 39,
};

static const DsiVicMode s_dsiMode480p60Alt = {
	0x00000001, 0x00107ac0, &s_dsiPhy1080000, 0x00149970, &s_dsiPhy1350000,
	858, 526, 0,
	16, 62, 60,
	10, 6, 30,
};

static const DsiVicMode s_dsiMode720p60Alt = {
	0x0000000f, 0x00287cf3, &s_dsiPhy2653427, 0x00000000, NULL,
	1428, 775, 0,
	114, 8, 24,
	4, 4, 47,
};

static const DsiVicMode s_dsiMode480p60 = {
	0x00000001, 0x00107ac0, &s_dsiPhy1080000, 0x00149970, &s_dsiPhy1350000,
	858, 525, 0,
	16, 62, 60,
	9, 6, 30,
};

static const DsiVicMode s_dsiModeOled48 = {
	0x00000005, 0x00223a1c, &s_dsiPhyOledCurrent, 0x00000000, NULL,
	1375, 567, 0,
	20, 391, 4,
	4, 4, 15,
};

static const DsiVicMode s_dsiMode1080p24 = {
	0x00000000, 0x002d45f9, &s_dsiPhy2967033, 0x00389777, &s_dsiPhy3708791,
	2750, 1125, 0,
	638, 44, 148,
	4, 5, 36,
};

static const DsiVicMode s_dsiMode720p60 = {
	0x00000000, 0x002d45f9, &s_dsiPhy2967033, 0x00389777, &s_dsiPhy3708791,
	1650, 750, 0,
	110, 40, 220,
	5, 5, 20,
};

/* Head 0 entries 0-2 are rewritten for the panel by SceDsiForDriver_F2921E29. */
static DsiVicEntry s_dsiVicTable[15] = {
	{ 0x0, &s_dsiModeOled60 },
	{ 0x80, &s_dsiModeOled50 },
	{ 0x20, &s_dsiModeOled48 },
	{ 0x8900, &s_dsiMode720p60Alt },
	{ 0x8300, &s_dsiMode480p60 },
	{ 0x8370, &s_dsiMode480p60Alt },
	{ 0x8480, &s_dsiMode576p50 },
	{ 0x84f0, &s_dsiMode576p50Alt },
	{ 0x8500, &s_dsiMode1080i60 },
	{ 0x8580, &s_dsiMode1080i50 },
	{ 0x8600, &s_dsiMode720p60 },
	{ 0x8680, &s_dsiMode720p50 },
	{ 0x8710, &s_dsiMode1080p60 },
	{ 0x8790, &s_dsiMode1080p50 },
	{ 0x8730, &s_dsiMode1080p24 },
};
