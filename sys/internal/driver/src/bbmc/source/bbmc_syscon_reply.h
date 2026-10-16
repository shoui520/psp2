#pragma once

#include <psp2kern/types.h>
#include <stddef.h>

typedef union BbmcSysconReplyWords8 {
	SceUInt32 words[2];
	SceUInt8 bytes[8];
} BbmcSysconReplyWords8;

typedef struct BbmcSysconDecodedReply8 {
	SceUInt8 replyBytes[2];
	SceUInt32 decodedValue;
} BbmcSysconDecodedReply8;

_Static_assert(sizeof(BbmcSysconReplyWords8) == 8, "BbmcSysconReplyWords8 size");
_Static_assert(sizeof(BbmcSysconDecodedReply8) == 8, "BbmcSysconDecodedReply8 size");
_Static_assert(offsetof(BbmcSysconDecodedReply8, replyBytes) == 0,
	       "BbmcSysconDecodedReply8 replyBytes");
_Static_assert(offsetof(BbmcSysconDecodedReply8, decodedValue) == 4,
	       "BbmcSysconDecodedReply8 decodedValue");
