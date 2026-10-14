/* SceKernelUtils Rijndael encryption data and entries. */
#include <psp2kern/kernel/utils.h>
#include <sce/aes.h>
#include <stdint.h>

/* Shift each byte row one column farther, wrapping at the block width. */
const uint8_t aesEncryptShiftRows256[8][4] = {
	{ 0, 5, 10, 15 }, { 4, 9, 14, 19 },
	{ 8, 13, 18, 23 }, { 12, 17, 22, 27 },
	{ 16, 21, 26, 31 }, { 20, 25, 30, 3 },
	{ 24, 29, 2, 7 }, { 28, 1, 6, 11 },
};

const uint8_t aesEncryptShiftRows192[6][4] = {
	{ 0, 5, 10, 15 }, { 4, 9, 14, 19 },
	{ 8, 13, 18, 23 }, { 12, 17, 22, 3 },
	{ 16, 21, 2, 7 }, { 20, 1, 6, 11 },
};

/* SubBytes followed by the forward MixColumns column (2s, s, s, 3s). */
#define AES_XTIME(byte) \
	(((uint32_t)(byte) << 1) ^ (((uint32_t)(byte) >> 7) * 0x11bU))
#define AES_FORWARD_COLUMN(byte) \
	(AES_XTIME(byte) | ((uint32_t)(byte) << 8) | \
	 ((uint32_t)(byte) << 16) | ((AES_XTIME(byte) ^ (uint32_t)(byte)) << 24))

const uint32_t aesEncryptionTable[256] = {
	AES_FORWARD_COLUMN(0x63), AES_FORWARD_COLUMN(0x7c), AES_FORWARD_COLUMN(0x77), AES_FORWARD_COLUMN(0x7b),
	AES_FORWARD_COLUMN(0xf2), AES_FORWARD_COLUMN(0x6b), AES_FORWARD_COLUMN(0x6f), AES_FORWARD_COLUMN(0xc5),
	AES_FORWARD_COLUMN(0x30), AES_FORWARD_COLUMN(0x01), AES_FORWARD_COLUMN(0x67), AES_FORWARD_COLUMN(0x2b),
	AES_FORWARD_COLUMN(0xfe), AES_FORWARD_COLUMN(0xd7), AES_FORWARD_COLUMN(0xab), AES_FORWARD_COLUMN(0x76),
	AES_FORWARD_COLUMN(0xca), AES_FORWARD_COLUMN(0x82), AES_FORWARD_COLUMN(0xc9), AES_FORWARD_COLUMN(0x7d),
	AES_FORWARD_COLUMN(0xfa), AES_FORWARD_COLUMN(0x59), AES_FORWARD_COLUMN(0x47), AES_FORWARD_COLUMN(0xf0),
	AES_FORWARD_COLUMN(0xad), AES_FORWARD_COLUMN(0xd4), AES_FORWARD_COLUMN(0xa2), AES_FORWARD_COLUMN(0xaf),
	AES_FORWARD_COLUMN(0x9c), AES_FORWARD_COLUMN(0xa4), AES_FORWARD_COLUMN(0x72), AES_FORWARD_COLUMN(0xc0),
	AES_FORWARD_COLUMN(0xb7), AES_FORWARD_COLUMN(0xfd), AES_FORWARD_COLUMN(0x93), AES_FORWARD_COLUMN(0x26),
	AES_FORWARD_COLUMN(0x36), AES_FORWARD_COLUMN(0x3f), AES_FORWARD_COLUMN(0xf7), AES_FORWARD_COLUMN(0xcc),
	AES_FORWARD_COLUMN(0x34), AES_FORWARD_COLUMN(0xa5), AES_FORWARD_COLUMN(0xe5), AES_FORWARD_COLUMN(0xf1),
	AES_FORWARD_COLUMN(0x71), AES_FORWARD_COLUMN(0xd8), AES_FORWARD_COLUMN(0x31), AES_FORWARD_COLUMN(0x15),
	AES_FORWARD_COLUMN(0x04), AES_FORWARD_COLUMN(0xc7), AES_FORWARD_COLUMN(0x23), AES_FORWARD_COLUMN(0xc3),
	AES_FORWARD_COLUMN(0x18), AES_FORWARD_COLUMN(0x96), AES_FORWARD_COLUMN(0x05), AES_FORWARD_COLUMN(0x9a),
	AES_FORWARD_COLUMN(0x07), AES_FORWARD_COLUMN(0x12), AES_FORWARD_COLUMN(0x80), AES_FORWARD_COLUMN(0xe2),
	AES_FORWARD_COLUMN(0xeb), AES_FORWARD_COLUMN(0x27), AES_FORWARD_COLUMN(0xb2), AES_FORWARD_COLUMN(0x75),
	AES_FORWARD_COLUMN(0x09), AES_FORWARD_COLUMN(0x83), AES_FORWARD_COLUMN(0x2c), AES_FORWARD_COLUMN(0x1a),
	AES_FORWARD_COLUMN(0x1b), AES_FORWARD_COLUMN(0x6e), AES_FORWARD_COLUMN(0x5a), AES_FORWARD_COLUMN(0xa0),
	AES_FORWARD_COLUMN(0x52), AES_FORWARD_COLUMN(0x3b), AES_FORWARD_COLUMN(0xd6), AES_FORWARD_COLUMN(0xb3),
	AES_FORWARD_COLUMN(0x29), AES_FORWARD_COLUMN(0xe3), AES_FORWARD_COLUMN(0x2f), AES_FORWARD_COLUMN(0x84),
	AES_FORWARD_COLUMN(0x53), AES_FORWARD_COLUMN(0xd1), AES_FORWARD_COLUMN(0x00), AES_FORWARD_COLUMN(0xed),
	AES_FORWARD_COLUMN(0x20), AES_FORWARD_COLUMN(0xfc), AES_FORWARD_COLUMN(0xb1), AES_FORWARD_COLUMN(0x5b),
	AES_FORWARD_COLUMN(0x6a), AES_FORWARD_COLUMN(0xcb), AES_FORWARD_COLUMN(0xbe), AES_FORWARD_COLUMN(0x39),
	AES_FORWARD_COLUMN(0x4a), AES_FORWARD_COLUMN(0x4c), AES_FORWARD_COLUMN(0x58), AES_FORWARD_COLUMN(0xcf),
	AES_FORWARD_COLUMN(0xd0), AES_FORWARD_COLUMN(0xef), AES_FORWARD_COLUMN(0xaa), AES_FORWARD_COLUMN(0xfb),
	AES_FORWARD_COLUMN(0x43), AES_FORWARD_COLUMN(0x4d), AES_FORWARD_COLUMN(0x33), AES_FORWARD_COLUMN(0x85),
	AES_FORWARD_COLUMN(0x45), AES_FORWARD_COLUMN(0xf9), AES_FORWARD_COLUMN(0x02), AES_FORWARD_COLUMN(0x7f),
	AES_FORWARD_COLUMN(0x50), AES_FORWARD_COLUMN(0x3c), AES_FORWARD_COLUMN(0x9f), AES_FORWARD_COLUMN(0xa8),
	AES_FORWARD_COLUMN(0x51), AES_FORWARD_COLUMN(0xa3), AES_FORWARD_COLUMN(0x40), AES_FORWARD_COLUMN(0x8f),
	AES_FORWARD_COLUMN(0x92), AES_FORWARD_COLUMN(0x9d), AES_FORWARD_COLUMN(0x38), AES_FORWARD_COLUMN(0xf5),
	AES_FORWARD_COLUMN(0xbc), AES_FORWARD_COLUMN(0xb6), AES_FORWARD_COLUMN(0xda), AES_FORWARD_COLUMN(0x21),
	AES_FORWARD_COLUMN(0x10), AES_FORWARD_COLUMN(0xff), AES_FORWARD_COLUMN(0xf3), AES_FORWARD_COLUMN(0xd2),
	AES_FORWARD_COLUMN(0xcd), AES_FORWARD_COLUMN(0x0c), AES_FORWARD_COLUMN(0x13), AES_FORWARD_COLUMN(0xec),
	AES_FORWARD_COLUMN(0x5f), AES_FORWARD_COLUMN(0x97), AES_FORWARD_COLUMN(0x44), AES_FORWARD_COLUMN(0x17),
	AES_FORWARD_COLUMN(0xc4), AES_FORWARD_COLUMN(0xa7), AES_FORWARD_COLUMN(0x7e), AES_FORWARD_COLUMN(0x3d),
	AES_FORWARD_COLUMN(0x64), AES_FORWARD_COLUMN(0x5d), AES_FORWARD_COLUMN(0x19), AES_FORWARD_COLUMN(0x73),
	AES_FORWARD_COLUMN(0x60), AES_FORWARD_COLUMN(0x81), AES_FORWARD_COLUMN(0x4f), AES_FORWARD_COLUMN(0xdc),
	AES_FORWARD_COLUMN(0x22), AES_FORWARD_COLUMN(0x2a), AES_FORWARD_COLUMN(0x90), AES_FORWARD_COLUMN(0x88),
	AES_FORWARD_COLUMN(0x46), AES_FORWARD_COLUMN(0xee), AES_FORWARD_COLUMN(0xb8), AES_FORWARD_COLUMN(0x14),
	AES_FORWARD_COLUMN(0xde), AES_FORWARD_COLUMN(0x5e), AES_FORWARD_COLUMN(0x0b), AES_FORWARD_COLUMN(0xdb),
	AES_FORWARD_COLUMN(0xe0), AES_FORWARD_COLUMN(0x32), AES_FORWARD_COLUMN(0x3a), AES_FORWARD_COLUMN(0x0a),
	AES_FORWARD_COLUMN(0x49), AES_FORWARD_COLUMN(0x06), AES_FORWARD_COLUMN(0x24), AES_FORWARD_COLUMN(0x5c),
	AES_FORWARD_COLUMN(0xc2), AES_FORWARD_COLUMN(0xd3), AES_FORWARD_COLUMN(0xac), AES_FORWARD_COLUMN(0x62),
	AES_FORWARD_COLUMN(0x91), AES_FORWARD_COLUMN(0x95), AES_FORWARD_COLUMN(0xe4), AES_FORWARD_COLUMN(0x79),
	AES_FORWARD_COLUMN(0xe7), AES_FORWARD_COLUMN(0xc8), AES_FORWARD_COLUMN(0x37), AES_FORWARD_COLUMN(0x6d),
	AES_FORWARD_COLUMN(0x8d), AES_FORWARD_COLUMN(0xd5), AES_FORWARD_COLUMN(0x4e), AES_FORWARD_COLUMN(0xa9),
	AES_FORWARD_COLUMN(0x6c), AES_FORWARD_COLUMN(0x56), AES_FORWARD_COLUMN(0xf4), AES_FORWARD_COLUMN(0xea),
	AES_FORWARD_COLUMN(0x65), AES_FORWARD_COLUMN(0x7a), AES_FORWARD_COLUMN(0xae), AES_FORWARD_COLUMN(0x08),
	AES_FORWARD_COLUMN(0xba), AES_FORWARD_COLUMN(0x78), AES_FORWARD_COLUMN(0x25), AES_FORWARD_COLUMN(0x2e),
	AES_FORWARD_COLUMN(0x1c), AES_FORWARD_COLUMN(0xa6), AES_FORWARD_COLUMN(0xb4), AES_FORWARD_COLUMN(0xc6),
	AES_FORWARD_COLUMN(0xe8), AES_FORWARD_COLUMN(0xdd), AES_FORWARD_COLUMN(0x74), AES_FORWARD_COLUMN(0x1f),
	AES_FORWARD_COLUMN(0x4b), AES_FORWARD_COLUMN(0xbd), AES_FORWARD_COLUMN(0x8b), AES_FORWARD_COLUMN(0x8a),
	AES_FORWARD_COLUMN(0x70), AES_FORWARD_COLUMN(0x3e), AES_FORWARD_COLUMN(0xb5), AES_FORWARD_COLUMN(0x66),
	AES_FORWARD_COLUMN(0x48), AES_FORWARD_COLUMN(0x03), AES_FORWARD_COLUMN(0xf6), AES_FORWARD_COLUMN(0x0e),
	AES_FORWARD_COLUMN(0x61), AES_FORWARD_COLUMN(0x35), AES_FORWARD_COLUMN(0x57), AES_FORWARD_COLUMN(0xb9),
	AES_FORWARD_COLUMN(0x86), AES_FORWARD_COLUMN(0xc1), AES_FORWARD_COLUMN(0x1d), AES_FORWARD_COLUMN(0x9e),
	AES_FORWARD_COLUMN(0xe1), AES_FORWARD_COLUMN(0xf8), AES_FORWARD_COLUMN(0x98), AES_FORWARD_COLUMN(0x11),
	AES_FORWARD_COLUMN(0x69), AES_FORWARD_COLUMN(0xd9), AES_FORWARD_COLUMN(0x8e), AES_FORWARD_COLUMN(0x94),
	AES_FORWARD_COLUMN(0x9b), AES_FORWARD_COLUMN(0x1e), AES_FORWARD_COLUMN(0x87), AES_FORWARD_COLUMN(0xe9),
	AES_FORWARD_COLUMN(0xce), AES_FORWARD_COLUMN(0x55), AES_FORWARD_COLUMN(0x28), AES_FORWARD_COLUMN(0xdf),
	AES_FORWARD_COLUMN(0x8c), AES_FORWARD_COLUMN(0xa1), AES_FORWARD_COLUMN(0x89), AES_FORWARD_COLUMN(0x0d),
	AES_FORWARD_COLUMN(0xbf), AES_FORWARD_COLUMN(0xe6), AES_FORWARD_COLUMN(0x42), AES_FORWARD_COLUMN(0x68),
	AES_FORWARD_COLUMN(0x41), AES_FORWARD_COLUMN(0x99), AES_FORWARD_COLUMN(0x2d), AES_FORWARD_COLUMN(0x0f),
	AES_FORWARD_COLUMN(0xb0), AES_FORWARD_COLUMN(0x54), AES_FORWARD_COLUMN(0xbb), AES_FORWARD_COLUMN(0x16),
};

#undef AES_FORWARD_COLUMN
#undef AES_XTIME

/* Inverse row permutations, in the original block-width order. */
const uint8_t aesDecryptShiftRows256[8][4] = {
	{ 0, 29, 26, 23 },
	{ 4, 1, 30, 27 },
	{ 8, 5, 2, 31 },
	{ 12, 9, 6, 3 },
	{ 16, 13, 10, 7 },
	{ 20, 17, 14, 11 },
	{ 24, 21, 18, 15 },
	{ 28, 25, 22, 19 },
};

const uint8_t aesDecryptShiftRows192[6][4] = {
	{ 0, 21, 18, 15 },
	{ 4, 1, 22, 19 },
	{ 8, 5, 2, 23 },
	{ 12, 9, 6, 3 },
	{ 16, 13, 10, 7 },
	{ 20, 17, 14, 11 },
};

/* Inverse SubBytes followed by the column (14s, 9s, 13s, 11s). */
#define AES_X2(byte) \
	(((uint32_t)(byte) << 1) ^ (((uint32_t)(byte) >> 7) * 0x11bU))
#define AES_X4(byte) AES_X2(AES_X2(byte))
#define AES_X8(byte) AES_X2(AES_X4(byte))
#define AES_INVERSE_COLUMN(byte) \
	((AES_X8(byte) ^ AES_X4(byte) ^ AES_X2(byte)) | \
	 ((AES_X8(byte) ^ (uint32_t)(byte)) << 8) | \
	 ((AES_X8(byte) ^ AES_X4(byte) ^ (uint32_t)(byte)) << 16) | \
	 ((AES_X8(byte) ^ AES_X2(byte) ^ (uint32_t)(byte)) << 24))

const uint32_t aesDecryptionTable[256] = {
	AES_INVERSE_COLUMN(0x52), AES_INVERSE_COLUMN(0x09), AES_INVERSE_COLUMN(0x6a), AES_INVERSE_COLUMN(0xd5),
	AES_INVERSE_COLUMN(0x30), AES_INVERSE_COLUMN(0x36), AES_INVERSE_COLUMN(0xa5), AES_INVERSE_COLUMN(0x38),
	AES_INVERSE_COLUMN(0xbf), AES_INVERSE_COLUMN(0x40), AES_INVERSE_COLUMN(0xa3), AES_INVERSE_COLUMN(0x9e),
	AES_INVERSE_COLUMN(0x81), AES_INVERSE_COLUMN(0xf3), AES_INVERSE_COLUMN(0xd7), AES_INVERSE_COLUMN(0xfb),
	AES_INVERSE_COLUMN(0x7c), AES_INVERSE_COLUMN(0xe3), AES_INVERSE_COLUMN(0x39), AES_INVERSE_COLUMN(0x82),
	AES_INVERSE_COLUMN(0x9b), AES_INVERSE_COLUMN(0x2f), AES_INVERSE_COLUMN(0xff), AES_INVERSE_COLUMN(0x87),
	AES_INVERSE_COLUMN(0x34), AES_INVERSE_COLUMN(0x8e), AES_INVERSE_COLUMN(0x43), AES_INVERSE_COLUMN(0x44),
	AES_INVERSE_COLUMN(0xc4), AES_INVERSE_COLUMN(0xde), AES_INVERSE_COLUMN(0xe9), AES_INVERSE_COLUMN(0xcb),
	AES_INVERSE_COLUMN(0x54), AES_INVERSE_COLUMN(0x7b), AES_INVERSE_COLUMN(0x94), AES_INVERSE_COLUMN(0x32),
	AES_INVERSE_COLUMN(0xa6), AES_INVERSE_COLUMN(0xc2), AES_INVERSE_COLUMN(0x23), AES_INVERSE_COLUMN(0x3d),
	AES_INVERSE_COLUMN(0xee), AES_INVERSE_COLUMN(0x4c), AES_INVERSE_COLUMN(0x95), AES_INVERSE_COLUMN(0x0b),
	AES_INVERSE_COLUMN(0x42), AES_INVERSE_COLUMN(0xfa), AES_INVERSE_COLUMN(0xc3), AES_INVERSE_COLUMN(0x4e),
	AES_INVERSE_COLUMN(0x08), AES_INVERSE_COLUMN(0x2e), AES_INVERSE_COLUMN(0xa1), AES_INVERSE_COLUMN(0x66),
	AES_INVERSE_COLUMN(0x28), AES_INVERSE_COLUMN(0xd9), AES_INVERSE_COLUMN(0x24), AES_INVERSE_COLUMN(0xb2),
	AES_INVERSE_COLUMN(0x76), AES_INVERSE_COLUMN(0x5b), AES_INVERSE_COLUMN(0xa2), AES_INVERSE_COLUMN(0x49),
	AES_INVERSE_COLUMN(0x6d), AES_INVERSE_COLUMN(0x8b), AES_INVERSE_COLUMN(0xd1), AES_INVERSE_COLUMN(0x25),
	AES_INVERSE_COLUMN(0x72), AES_INVERSE_COLUMN(0xf8), AES_INVERSE_COLUMN(0xf6), AES_INVERSE_COLUMN(0x64),
	AES_INVERSE_COLUMN(0x86), AES_INVERSE_COLUMN(0x68), AES_INVERSE_COLUMN(0x98), AES_INVERSE_COLUMN(0x16),
	AES_INVERSE_COLUMN(0xd4), AES_INVERSE_COLUMN(0xa4), AES_INVERSE_COLUMN(0x5c), AES_INVERSE_COLUMN(0xcc),
	AES_INVERSE_COLUMN(0x5d), AES_INVERSE_COLUMN(0x65), AES_INVERSE_COLUMN(0xb6), AES_INVERSE_COLUMN(0x92),
	AES_INVERSE_COLUMN(0x6c), AES_INVERSE_COLUMN(0x70), AES_INVERSE_COLUMN(0x48), AES_INVERSE_COLUMN(0x50),
	AES_INVERSE_COLUMN(0xfd), AES_INVERSE_COLUMN(0xed), AES_INVERSE_COLUMN(0xb9), AES_INVERSE_COLUMN(0xda),
	AES_INVERSE_COLUMN(0x5e), AES_INVERSE_COLUMN(0x15), AES_INVERSE_COLUMN(0x46), AES_INVERSE_COLUMN(0x57),
	AES_INVERSE_COLUMN(0xa7), AES_INVERSE_COLUMN(0x8d), AES_INVERSE_COLUMN(0x9d), AES_INVERSE_COLUMN(0x84),
	AES_INVERSE_COLUMN(0x90), AES_INVERSE_COLUMN(0xd8), AES_INVERSE_COLUMN(0xab), AES_INVERSE_COLUMN(0x00),
	AES_INVERSE_COLUMN(0x8c), AES_INVERSE_COLUMN(0xbc), AES_INVERSE_COLUMN(0xd3), AES_INVERSE_COLUMN(0x0a),
	AES_INVERSE_COLUMN(0xf7), AES_INVERSE_COLUMN(0xe4), AES_INVERSE_COLUMN(0x58), AES_INVERSE_COLUMN(0x05),
	AES_INVERSE_COLUMN(0xb8), AES_INVERSE_COLUMN(0xb3), AES_INVERSE_COLUMN(0x45), AES_INVERSE_COLUMN(0x06),
	AES_INVERSE_COLUMN(0xd0), AES_INVERSE_COLUMN(0x2c), AES_INVERSE_COLUMN(0x1e), AES_INVERSE_COLUMN(0x8f),
	AES_INVERSE_COLUMN(0xca), AES_INVERSE_COLUMN(0x3f), AES_INVERSE_COLUMN(0x0f), AES_INVERSE_COLUMN(0x02),
	AES_INVERSE_COLUMN(0xc1), AES_INVERSE_COLUMN(0xaf), AES_INVERSE_COLUMN(0xbd), AES_INVERSE_COLUMN(0x03),
	AES_INVERSE_COLUMN(0x01), AES_INVERSE_COLUMN(0x13), AES_INVERSE_COLUMN(0x8a), AES_INVERSE_COLUMN(0x6b),
	AES_INVERSE_COLUMN(0x3a), AES_INVERSE_COLUMN(0x91), AES_INVERSE_COLUMN(0x11), AES_INVERSE_COLUMN(0x41),
	AES_INVERSE_COLUMN(0x4f), AES_INVERSE_COLUMN(0x67), AES_INVERSE_COLUMN(0xdc), AES_INVERSE_COLUMN(0xea),
	AES_INVERSE_COLUMN(0x97), AES_INVERSE_COLUMN(0xf2), AES_INVERSE_COLUMN(0xcf), AES_INVERSE_COLUMN(0xce),
	AES_INVERSE_COLUMN(0xf0), AES_INVERSE_COLUMN(0xb4), AES_INVERSE_COLUMN(0xe6), AES_INVERSE_COLUMN(0x73),
	AES_INVERSE_COLUMN(0x96), AES_INVERSE_COLUMN(0xac), AES_INVERSE_COLUMN(0x74), AES_INVERSE_COLUMN(0x22),
	AES_INVERSE_COLUMN(0xe7), AES_INVERSE_COLUMN(0xad), AES_INVERSE_COLUMN(0x35), AES_INVERSE_COLUMN(0x85),
	AES_INVERSE_COLUMN(0xe2), AES_INVERSE_COLUMN(0xf9), AES_INVERSE_COLUMN(0x37), AES_INVERSE_COLUMN(0xe8),
	AES_INVERSE_COLUMN(0x1c), AES_INVERSE_COLUMN(0x75), AES_INVERSE_COLUMN(0xdf), AES_INVERSE_COLUMN(0x6e),
	AES_INVERSE_COLUMN(0x47), AES_INVERSE_COLUMN(0xf1), AES_INVERSE_COLUMN(0x1a), AES_INVERSE_COLUMN(0x71),
	AES_INVERSE_COLUMN(0x1d), AES_INVERSE_COLUMN(0x29), AES_INVERSE_COLUMN(0xc5), AES_INVERSE_COLUMN(0x89),
	AES_INVERSE_COLUMN(0x6f), AES_INVERSE_COLUMN(0xb7), AES_INVERSE_COLUMN(0x62), AES_INVERSE_COLUMN(0x0e),
	AES_INVERSE_COLUMN(0xaa), AES_INVERSE_COLUMN(0x18), AES_INVERSE_COLUMN(0xbe), AES_INVERSE_COLUMN(0x1b),
	AES_INVERSE_COLUMN(0xfc), AES_INVERSE_COLUMN(0x56), AES_INVERSE_COLUMN(0x3e), AES_INVERSE_COLUMN(0x4b),
	AES_INVERSE_COLUMN(0xc6), AES_INVERSE_COLUMN(0xd2), AES_INVERSE_COLUMN(0x79), AES_INVERSE_COLUMN(0x20),
	AES_INVERSE_COLUMN(0x9a), AES_INVERSE_COLUMN(0xdb), AES_INVERSE_COLUMN(0xc0), AES_INVERSE_COLUMN(0xfe),
	AES_INVERSE_COLUMN(0x78), AES_INVERSE_COLUMN(0xcd), AES_INVERSE_COLUMN(0x5a), AES_INVERSE_COLUMN(0xf4),
	AES_INVERSE_COLUMN(0x1f), AES_INVERSE_COLUMN(0xdd), AES_INVERSE_COLUMN(0xa8), AES_INVERSE_COLUMN(0x33),
	AES_INVERSE_COLUMN(0x88), AES_INVERSE_COLUMN(0x07), AES_INVERSE_COLUMN(0xc7), AES_INVERSE_COLUMN(0x31),
	AES_INVERSE_COLUMN(0xb1), AES_INVERSE_COLUMN(0x12), AES_INVERSE_COLUMN(0x10), AES_INVERSE_COLUMN(0x59),
	AES_INVERSE_COLUMN(0x27), AES_INVERSE_COLUMN(0x80), AES_INVERSE_COLUMN(0xec), AES_INVERSE_COLUMN(0x5f),
	AES_INVERSE_COLUMN(0x60), AES_INVERSE_COLUMN(0x51), AES_INVERSE_COLUMN(0x7f), AES_INVERSE_COLUMN(0xa9),
	AES_INVERSE_COLUMN(0x19), AES_INVERSE_COLUMN(0xb5), AES_INVERSE_COLUMN(0x4a), AES_INVERSE_COLUMN(0x0d),
	AES_INVERSE_COLUMN(0x2d), AES_INVERSE_COLUMN(0xe5), AES_INVERSE_COLUMN(0x7a), AES_INVERSE_COLUMN(0x9f),
	AES_INVERSE_COLUMN(0x93), AES_INVERSE_COLUMN(0xc9), AES_INVERSE_COLUMN(0x9c), AES_INVERSE_COLUMN(0xef),
	AES_INVERSE_COLUMN(0xa0), AES_INVERSE_COLUMN(0xe0), AES_INVERSE_COLUMN(0x3b), AES_INVERSE_COLUMN(0x4d),
	AES_INVERSE_COLUMN(0xae), AES_INVERSE_COLUMN(0x2a), AES_INVERSE_COLUMN(0xf5), AES_INVERSE_COLUMN(0xb0),
	AES_INVERSE_COLUMN(0xc8), AES_INVERSE_COLUMN(0xeb), AES_INVERSE_COLUMN(0xbb), AES_INVERSE_COLUMN(0x3c),
	AES_INVERSE_COLUMN(0x83), AES_INVERSE_COLUMN(0x53), AES_INVERSE_COLUMN(0x99), AES_INVERSE_COLUMN(0x61),
	AES_INVERSE_COLUMN(0x17), AES_INVERSE_COLUMN(0x2b), AES_INVERSE_COLUMN(0x04), AES_INVERSE_COLUMN(0x7e),
	AES_INVERSE_COLUMN(0xba), AES_INVERSE_COLUMN(0x77), AES_INVERSE_COLUMN(0xd6), AES_INVERSE_COLUMN(0x26),
	AES_INVERSE_COLUMN(0xe1), AES_INVERSE_COLUMN(0x69), AES_INVERSE_COLUMN(0x14), AES_INVERSE_COLUMN(0x63),
	AES_INVERSE_COLUMN(0x55), AES_INVERSE_COLUMN(0x21), AES_INVERSE_COLUMN(0x0c), AES_INVERSE_COLUMN(0x7d),
};

#undef AES_INVERSE_COLUMN
#undef AES_X8
#undef AES_X4
#undef AES_X2

const uint8_t aesInverseSubstitution[256] = {
	0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
	0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
	0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
	0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
	0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
	0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
	0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
	0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
	0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
	0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
	0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
	0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
	0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
	0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
	0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
	0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d,
};


int ksceAesEncrypt2(SceAesContext *ctx, const void *src, void *dst)
{
	return ksceAesEncrypt1(ctx, src, dst);
}

int ksceAesEncrypt1(SceAesContext *context, const void *src, void *dst)
{
	const SceAesCompactContext *ctx = (const SceAesCompactContext *)context;
	const uint32_t *input = src;
	uint32_t *output = dst;
	const uint32_t *key = ctx->roundKeys;
	const uint8_t *substitution = (const uint8_t *)aesEncryptionTable + 2;
	const uint8_t (*shiftRows)[4];
	uint32_t state[8], transformed[8];
	uint32_t words = ctx->blockWords;
	uint32_t rounds = ctx->rounds - 1;
	unsigned int column;

	if (words != 4 && words != 6 && words != 8)
		return -1;
	shiftRows = words == 6 ? aesEncryptShiftRows192 : aesEncryptShiftRows256;
	for (column = words; column != 0; column--)
		state[column - 1] = input[column - 1];
	do {
		const uint8_t *bytes = (const uint8_t *)state;

		for (column = 0; column != words; column++)
			state[column] ^= *key++;
		for (column = 0; column != words; column++) {
			unsigned int first = words == 4 ? column * 4 : shiftRows[column][0];
			unsigned int second = words == 4 ? ((column + 1) & 3) * 4 + 1 : shiftRows[column][1];
			unsigned int third = words == 4 ? ((column + 2) & 3) * 4 + 2 : shiftRows[column][2];
			unsigned int fourth = words == 4 ? ((column + 3) & 3) * 4 + 3 : shiftRows[column][3];
			uint32_t a = aesEncryptionTable[bytes[first]];
			uint32_t b = aesEncryptionTable[bytes[second]];
			uint32_t c = aesEncryptionTable[bytes[third]];
			uint32_t d = aesEncryptionTable[bytes[fourth]];

			transformed[column] = a ^ ((b << 8) | (b >> 24)) ^
				((c << 16) | (c >> 16)) ^ ((d << 24) | (d >> 8));
		}
		for (column = 0; column != words; column++)
			state[column] = transformed[column];
	} while (--rounds != 0);
	for (column = 0; column != words; column++)
		state[column] ^= *key++;
	for (column = 0; column != words; column++) {
		const uint8_t *bytes = (const uint8_t *)state;
		unsigned int first = words == 4 ? column * 4 : shiftRows[column][0];
		unsigned int second = words == 4 ? ((column + 1) & 3) * 4 + 1 : shiftRows[column][1];
		unsigned int third = words == 4 ? ((column + 2) & 3) * 4 + 2 : shiftRows[column][2];
		unsigned int fourth = words == 4 ? ((column + 3) & 3) * 4 + 3 : shiftRows[column][3];

		transformed[column] = substitution[4 * bytes[first]] |
			((uint32_t)substitution[4 * bytes[second]] << 8) |
			((uint32_t)substitution[4 * bytes[third]] << 16) |
			((uint32_t)substitution[4 * bytes[fourth]] << 24);
	}
	if (words == 4) {
		uint32_t first = transformed[0] ^ key[0];
		uint32_t second = transformed[1] ^ key[1];
		uint32_t third = transformed[2] ^ key[2];
		uint32_t fourth = transformed[3] ^ key[3];

		output[0] = first;
		output[1] = second;
		output[2] = third;
		output[3] = fourth;
	} else {
		for (column = 0; column != words; column += 2) {
			uint32_t first = transformed[column] ^ key[column];
			uint32_t second = transformed[column + 1] ^ key[column + 1];

			output[column] = first;
			output[column + 1] = second;
		}
	}
	return 0;
}

int ksceAesDecrypt2(SceAesContext *context, const void *src, void *dst)
{
	const SceAesCompactContext *ctx = (const SceAesCompactContext *)context;
	const uint32_t *input = src;
	uint32_t *output = dst;
	const uint32_t *key = ctx->roundKeys;
	const uint8_t *substitution = aesInverseSubstitution;
	const uint8_t (*shiftRows)[4];
	uint32_t state[8], transformed[8];
	uint32_t words = ctx->blockWords;
	uint32_t rounds = ctx->rounds - 1;
	unsigned int column;

	if (words != 4 && words != 6 && words != 8)
		return -1;
	shiftRows = words == 6 ? aesDecryptShiftRows192 : aesDecryptShiftRows256;
	for (column = words; column != 0; column--)
		state[column - 1] = input[column - 1];
	do {
		const uint8_t *bytes = (const uint8_t *)state;

		for (column = 0; column != words; column++)
			state[column] ^= *key++;
		for (column = 0; column != words; column++) {
			unsigned int first = words == 4 ? column * 4 : shiftRows[column][0];
			unsigned int second = words == 4 ? ((column + 3) & 3) * 4 + 1 : shiftRows[column][1];
			unsigned int third = words == 4 ? ((column + 2) & 3) * 4 + 2 : shiftRows[column][2];
			unsigned int fourth = words == 4 ? ((column + 1) & 3) * 4 + 3 : shiftRows[column][3];
			uint32_t a = aesDecryptionTable[bytes[first]];
			uint32_t b = aesDecryptionTable[bytes[second]];
			uint32_t c = aesDecryptionTable[bytes[third]];
			uint32_t d = aesDecryptionTable[bytes[fourth]];

			transformed[column] = a ^ ((b << 8) | (b >> 24)) ^
				((c << 16) | (c >> 16)) ^ ((d << 24) | (d >> 8));
		}
		for (column = 0; column != words; column++)
			state[column] = transformed[column];
	} while (--rounds != 0);
	for (column = 0; column != words; column++)
		state[column] ^= *key++;
	for (column = 0; column != words; column++) {
		const uint8_t *bytes = (const uint8_t *)state;
		unsigned int first = words == 4 ? column * 4 : shiftRows[column][0];
		unsigned int second = words == 4 ? ((column + 3) & 3) * 4 + 1 : shiftRows[column][1];
		unsigned int third = words == 4 ? ((column + 2) & 3) * 4 + 2 : shiftRows[column][2];
		unsigned int fourth = words == 4 ? ((column + 1) & 3) * 4 + 3 : shiftRows[column][3];

		transformed[column] = substitution[bytes[first]] |
			((uint32_t)substitution[bytes[second]] << 8) |
			((uint32_t)substitution[bytes[third]] << 16) |
			((uint32_t)substitution[bytes[fourth]] << 24);
	}
	if (words == 4) {
		uint32_t first = transformed[0] ^ key[0];
		uint32_t second = transformed[1] ^ key[1];
		uint32_t third = transformed[2] ^ key[2];
		uint32_t fourth = transformed[3] ^ key[3];

		output[0] = first;
		output[1] = second;
		output[2] = third;
		output[3] = fourth;
	} else {
		for (column = 0; column != words; column += 2) {
			uint32_t first = transformed[column] ^ key[column];
			uint32_t second = transformed[column + 3] ^ key[column + 3];

			output[column] = first;
			output[column + 3] = second;
		}
	}
	return 0;
}

int ksceAesDecrypt1(SceAesContext *context, const void *src, void *dst)
{
	const SceAesFullContext *full = (const SceAesFullContext *)context;
	const SceAesCompactContext *ctx = &full->encryption;
	const uint32_t *input = src;
	uint32_t *output = dst;
	const uint32_t *key = full->decryptionRoundKeys;
	const uint8_t *substitution = aesInverseSubstitution;
	const uint8_t (*shiftRows)[4];
	uint32_t state[8], transformed[8];
	uint32_t words = ctx->blockWords;
	uint32_t rounds = ctx->rounds - 1;
	unsigned int column;

	if (words != 4 && words != 6 && words != 8)
		return -1;
	shiftRows = words == 6 ? aesDecryptShiftRows192 : aesDecryptShiftRows256;
	for (column = words; column != 0; column--)
		state[column - 1] = input[column - 1];
	do {
		const uint8_t *bytes = (const uint8_t *)state;

		for (column = 0; column != words; column++)
			state[column] ^= *key++;
		for (column = 0; column != words; column++) {
			unsigned int first = words == 4 ? column * 4 : shiftRows[column][0];
			unsigned int second = words == 4 ? ((column + 3) & 3) * 4 + 1 : shiftRows[column][1];
			unsigned int third = words == 4 ? ((column + 2) & 3) * 4 + 2 : shiftRows[column][2];
			unsigned int fourth = words == 4 ? ((column + 1) & 3) * 4 + 3 : shiftRows[column][3];
			uint32_t a = aesDecryptionTable[bytes[first]];
			uint32_t b = aesDecryptionTable[bytes[second]];
			uint32_t c = aesDecryptionTable[bytes[third]];
			uint32_t d = aesDecryptionTable[bytes[fourth]];

			transformed[column] = a ^ ((b << 8) | (b >> 24)) ^
				((c << 16) | (c >> 16)) ^ ((d << 24) | (d >> 8));
		}
		for (column = 0; column != words; column++)
			state[column] = transformed[column];
	} while (--rounds != 0);
	for (column = 0; column != words; column++)
		state[column] ^= *key++;
	for (column = 0; column != words; column++) {
		const uint8_t *bytes = (const uint8_t *)state;
		unsigned int first = words == 4 ? column * 4 : shiftRows[column][0];
		unsigned int second = words == 4 ? ((column + 3) & 3) * 4 + 1 : shiftRows[column][1];
		unsigned int third = words == 4 ? ((column + 2) & 3) * 4 + 2 : shiftRows[column][2];
		unsigned int fourth = words == 4 ? ((column + 1) & 3) * 4 + 3 : shiftRows[column][3];

		transformed[column] = substitution[bytes[first]] |
			((uint32_t)substitution[bytes[second]] << 8) |
			((uint32_t)substitution[bytes[third]] << 16) |
			((uint32_t)substitution[bytes[fourth]] << 24);
	}
	if (words == 4) {
		uint32_t first = transformed[0] ^ key[0];
		uint32_t second = transformed[1] ^ key[1];
		uint32_t third = transformed[2] ^ key[2];
		uint32_t fourth = transformed[3] ^ key[3];

		output[0] = first;
		output[1] = second;
		output[2] = third;
		output[3] = fourth;
	} else {
		for (column = 0; column != words; column += 2) {
			uint32_t first = transformed[column] ^ key[column];
			uint32_t second = transformed[column + 3] ^ key[column + 3];

			output[column] = first;
			output[column + 3] = second;
		}
	}
	return 0;
}
