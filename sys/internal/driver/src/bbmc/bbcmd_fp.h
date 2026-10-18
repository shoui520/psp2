#pragma once

#include <scetypes.h>

/* Little-endian IEEE representations used by the byte-oriented QMI encoder. */
typedef struct BbcmdDoubleEncoding {
	SceUInt8 discardedSignificand[3];
	SceUInt8 significandLow;
	SceUInt8 significandMiddle;
	SceUInt8 significandHigh;
	SceUInt8 exponentSignificand;
	SceUInt8 signExponent;
} BbcmdDoubleEncoding;

typedef struct BbcmdFloatEncoding {
	SceUInt8 significandLow;
	SceUInt8 significandMiddle;
	SceUInt8 significandExponent;
	SceUInt8 signExponent;
} BbcmdFloatEncoding;

volatile BbcmdFloatEncoding *bbcmdEncodeClampedFloat(volatile BbcmdFloatEncoding *output,
					  const BbcmdDoubleEncoding *input);
