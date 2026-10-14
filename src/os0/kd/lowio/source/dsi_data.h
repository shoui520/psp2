/* DSI timing tables. Word and table meanings remain partly unknown. */
#pragma once
#include <stdint.h>

static uint32_t s_dsiUnkb358[9];

static const uint32_t s_dsiUnk8a8c[9] = {
    0x00002bcf,
    0x00000006,
    0x00000f05,
    0x00000001,
    0x00000806,
    0x00004906,
    0x00000016,
    0x00000003,
    0x00000020,
};

static const uint32_t s_dsiUnk8ae8[9] = {
    0x00001a60,
    0x00000003,
    0x00000b03,
    0x00000003,
    0x00000003,
    0x000041ec,
    0x00000007,
    0x00000000,
    0x00000020,
};

static const uint32_t s_dsiUnk8b20[9] = {
    0x00003a03,
    0x00000007,
    0x00001507,
    0x00000005,
    0x00000507,
    0x00004884,
    0x0000000b,
    0x00000000,
    0x00000020,
};

static const uint32_t s_dsiUnk8bec[9] = {
    0x000039f5,
    0x00000007,
    0x00001507,
    0x00000005,
    0x00000507,
    0x00004871,
    0x0000000b,
    0x00000000,
    0x00000020,
};

static const uint32_t s_dsiUnk8c48[9] = {
    0x000033e0,
    0x00000007,
    0x0000150a,
    0x00000002,
    0x0000040b,
    0x000040ca,
    0x00000008,
    0x00000004,
    0x00000020,
};

static const uint32_t s_dsiUnk8cdc[9] = {
    0x00004876,
    0x00000009,
    0x00001f09,
    0x00000005,
    0x00000709,
    0x00004871,
    0x0000000b,
    0x00000001,
    0x00000020,
};

static const uint32_t s_dsiUnk8d38[9] = {
    0x0000488a,
    0x00000009,
    0x00001f09,
    0x00000005,
    0x00000709,
    0x00004890,
    0x0000000b,
    0x00000001,
    0x00000020,
};

static const uint32_t s_dsiUnk8da8[9] = {
    0x00002bcf,
    0x00000006,
    0x00001108,
    0x00000001,
    0x00000809,
    0x00004906,
    0x00000065,
    0x00000003,
    0x00000020,
};

static const uint32_t s_dsiUnk8e04[9] = {
    0x00001519,
    0x00000003,
    0x00000703,
    0x00000003,
    0x00000003,
    0x000034bd,
    0x00000007,
    0x00000000,
    0x00000020,
};

static const uint32_t s_dsiUnk8fa0[9] = {
    0x00002bcf,
    0x00000006,
    0x00001007,
    0x00000001,
    0x00000808,
    0x00004906,
    0x00000016,
    0x00000003,
    0x00000020,
};

static const uint32_t s_dsiMode8ab0[14] = {
    0x00000000,
    0x002d5190,
    (uint32_t)s_dsiUnk8b20,
    0x0038a5f4,
    (uint32_t)s_dsiUnk8d38,
    0x00000a50,
    0x00000465,
    0x00000000,
    0x00000210,
    0x0000002c,
    0x00000094,
    0x00000004,
    0x00000005,
    0x00000024,
};

static const uint32_t s_dsiMode8b44[14] = {
    0x00000000,
    0x002d5190,
    (uint32_t)s_dsiUnk8b20,
    0x0038a5f4,
    (uint32_t)s_dsiUnk8d38,
    0x00000a50,
    0x00000465,
    0x00000001,
    0x00000210,
    0x0000002c,
    0x00000094,
    0x00000002,
    0x00000005,
    0x00000010,
};

static const uint32_t s_dsiMode8b7c[14] = {
    0x00000001,
    0x00107ac0,
    (uint32_t)s_dsiUnk8e04,
    0x00149970,
    (uint32_t)s_dsiUnk8ae8,
    0x00000360,
    0x00000272,
    0x00000000,
    0x0000000c,
    0x00000040,
    0x00000044,
    0x00000006,
    0x00000005,
    0x00000028,
};

static const uint32_t s_dsiMode8bb4[14] = {
    0x00000005,
    0x00223a1c,
    (uint32_t)s_dsiUnkb358,
    0x00000000,
    0x00000000,
    0x000004e2,
    0x00000252,
    0x00000000,
    0x00000014,
    0x0000010a,
    0x00000004,
    0x00000004,
    0x00000004,
    0x0000002e,
};

static const uint32_t s_dsiMode8c10[14] = {
    0x00000004,
    0x0023f48b,
    (uint32_t)s_dsiUnk8da8,
    0x00000000,
    0x00000000,
    0x00000460,
    0x00000249,
    0x00000000,
    0x00000088,
    0x00000004,
    0x00000014,
    0x00000004,
    0x00000004,
    0x00000021,
};

static const uint32_t s_dsiMode8c6c[14] = {
    0x00000000,
    0x002d45f9,
    (uint32_t)s_dsiUnk8bec,
    0x00389777,
    (uint32_t)s_dsiUnk8cdc,
    0x00000898,
    0x00000465,
    0x00000000,
    0x00000058,
    0x0000002c,
    0x00000094,
    0x00000004,
    0x00000005,
    0x00000024,
};

static const uint32_t s_dsiMode8ca4[14] = {
    0x00000000,
    0x002d45f9,
    (uint32_t)s_dsiUnk8bec,
    0x00389777,
    (uint32_t)s_dsiUnk8cdc,
    0x00000898,
    0x00000465,
    0x00000001,
    0x00000058,
    0x0000002c,
    0x00000094,
    0x00000002,
    0x00000005,
    0x00000010,
};

static const uint32_t s_dsiMode8d00[14] = {
    0x00000005,
    0x00223a1c,
    (uint32_t)s_dsiUnkb358,
    0x00000000,
    0x00000000,
    0x0000041a,
    0x00000252,
    0x00000000,
    0x00000014,
    0x00000042,
    0x00000004,
    0x00000004,
    0x00000004,
    0x0000002a,
};

static const uint32_t s_dsiMode8d5c[14] = {
    0x00000000,
    0x002d5190,
    (uint32_t)s_dsiUnk8b20,
    0x0038a5f4,
    (uint32_t)s_dsiUnk8d38,
    0x000007bc,
    0x000002ee,
    0x00000000,
    0x000001b8,
    0x00000028,
    0x000000dc,
    0x00000005,
    0x00000005,
    0x00000014,
};

static const uint32_t s_dsiMode8dcc[14] = {
    0x00000001,
    0x00107ac0,
    (uint32_t)s_dsiUnk8e04,
    0x00149970,
    (uint32_t)s_dsiUnk8ae8,
    0x00000360,
    0x00000271,
    0x00000000,
    0x0000000c,
    0x00000040,
    0x00000044,
    0x00000005,
    0x00000005,
    0x00000027,
};

static const uint32_t s_dsiMode8e28[14] = {
    0x00000001,
    0x00107ac0,
    (uint32_t)s_dsiUnk8e04,
    0x00149970,
    (uint32_t)s_dsiUnk8ae8,
    0x0000035a,
    0x0000020e,
    0x00000000,
    0x00000010,
    0x0000003e,
    0x0000003c,
    0x0000000a,
    0x00000006,
    0x0000001e,
};

static const uint32_t s_dsiMode8e60[14] = {
    0x0000000f,
    0x00287cf3,
    (uint32_t)s_dsiUnk8c48,
    0x00000000,
    0x00000000,
    0x00000594,
    0x00000307,
    0x00000000,
    0x00000072,
    0x00000008,
    0x00000018,
    0x00000004,
    0x00000004,
    0x0000002f,
};

static const uint32_t s_dsiMode8e98[14] = {
    0x00000001,
    0x00107ac0,
    (uint32_t)s_dsiUnk8e04,
    0x00149970,
    (uint32_t)s_dsiUnk8ae8,
    0x0000035a,
    0x0000020d,
    0x00000000,
    0x00000010,
    0x0000003e,
    0x0000003c,
    0x00000009,
    0x00000006,
    0x0000001e,
};

static const uint32_t s_dsiMode8ed0[14] = {
    0x00000005,
    0x00223a1c,
    (uint32_t)s_dsiUnkb358,
    0x00000000,
    0x00000000,
    0x0000055f,
    0x00000237,
    0x00000000,
    0x00000014,
    0x00000187,
    0x00000004,
    0x00000004,
    0x00000004,
    0x0000000f,
};

static const uint32_t s_dsiMode8f08[14] = {
    0x00000000,
    0x002d45f9,
    (uint32_t)s_dsiUnk8bec,
    0x00389777,
    (uint32_t)s_dsiUnk8cdc,
    0x00000abe,
    0x00000465,
    0x00000000,
    0x0000027e,
    0x0000002c,
    0x00000094,
    0x00000004,
    0x00000005,
    0x00000024,
};

static const uint32_t s_dsiMode8f68[14] = {
    0x00000000,
    0x002d45f9,
    (uint32_t)s_dsiUnk8bec,
    0x00389777,
    (uint32_t)s_dsiUnk8cdc,
    0x00000672,
    0x000002ee,
    0x00000000,
    0x0000006e,
    0x00000028,
    0x000000dc,
    0x00000005,
    0x00000005,
    0x00000014,
};

static struct {
	uint32_t vic;
	const uint32_t *mode;
} s_dsiVicMap[15] = {
    {0x0, s_dsiMode8d00},
    {0x80, s_dsiMode8bb4},
    {0x20, s_dsiMode8ed0},
    {0x8900, s_dsiMode8e60},
    {0x8300, s_dsiMode8e98},
    {0x8370, s_dsiMode8e28},
    {0x8480, s_dsiMode8dcc},
    {0x84f0, s_dsiMode8b7c},
    {0x8500, s_dsiMode8ca4},
    {0x8580, s_dsiMode8b44},
    {0x8600, s_dsiMode8f68},
    {0x8680, s_dsiMode8d5c},
    {0x8710, s_dsiMode8c6c},
    {0x8790, s_dsiMode8ab0},
    {0x8730, s_dsiMode8f08},
};
