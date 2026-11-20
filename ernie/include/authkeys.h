#ifndef ERNIE_AUTHKEYS_H
#define ERNIE_AUTHKEYS_H

#include "ernie.h"

/* the factory handshake key, expanded: AES-256's 15 round keys */
#define FACTORY_ROUND_KEYS \
	0xe61677f4UL, 0xd69f64c5UL, 0xd98f5348UL, 0xd1123d77UL, \
	0x87119e22UL, 0x82d7b137UL, 0xdb0ca8d6UL, 0xc3b9e472UL, \
	0xa638219cUL, 0x70a74559UL, 0xa9281611UL, 0x783a2b66UL, \
	0x3b916f11UL, 0xb946de26UL, 0x624a76f0UL, 0xa1f39282UL, \
	0xb50a2cd1UL, 0xc5ad6988UL, 0x6c857f99UL, 0x14bf54ffUL, \
	0xc1994f07UL, 0x78df9121UL, 0x1a95e7d1UL, 0xbb667553UL, \
	0x58e01f48UL, 0x9d4d76c0UL, 0xf1c80959UL, 0xe5775da6UL, \
	0x186c0323UL, 0x60b39202UL, 0x7a2675d3UL, 0xc1400080UL, \
	0x95981623UL, 0x08d560e3UL, 0xf91d69baUL, 0x1c6a341cUL, \
	0x846e1bbfUL, 0xe4dd89bdUL, 0x9efbfc6eUL, 0x5fbbfceeUL, \
	0xbd57fc83UL, 0xb5829c60UL, 0x4c9ff5daUL, 0x50f5c1c6UL, \
	0xd788630bUL, 0x3355eab6UL, 0xadae16d8UL, 0xf215ea36UL, \
	0xb8dea524UL, 0x0d5c3944UL, 0x41c3cc9eUL, 0x11360d58UL, \
	0x558db461UL, 0x66d85ed7UL, 0xcb76480fUL, 0x3963a239UL, \
	0xaacc5e5eUL, 0xa790671aUL, 0xe653ab84UL, 0xf765a6dcUL,

extern const unsigned long factoryRoundKeys[60];

/* Shared key for the application nonce generator. */
extern const unsigned long randomKey[4];

/* IV shared by the host handshake and secure NVS. */
extern const unsigned char handshakeIv[16];

/* Serial authentication keys and packet constants. */
extern const unsigned char serialHeader[4];
extern const unsigned char serialIv[16];
#if !FW_SINCE(0x00090009)
/* 0x00080504: the keys expanded, AES-128's 11 round keys each */
extern const unsigned long serialKey0First[44];
extern const unsigned long serialKey0Second[44];
extern const unsigned char serialKey0Check[16];
extern const unsigned long serialKey1First[44];
extern const unsigned long serialKey1Second[44];
extern const unsigned char serialKey1Check[16];
#else
extern const unsigned char serialKey0First[16];
extern const unsigned char serialKey0Second[16];
extern const unsigned char serialKey0Check[16];
extern const unsigned char serialKey1First[16];
extern const unsigned char serialKey1Second[16];
extern const unsigned char serialKey1Check[16];
#endif
#if FW_SINCE(0x00090804)
extern const unsigned char serialKey2First[16];
extern const unsigned char serialKey2Second[16];
extern const unsigned char serialKey2Check[16];
#endif

#endif
