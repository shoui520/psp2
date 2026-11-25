/* Copyright (C) 2011, 2012 The uOFW team
   See the file COPYING for copying permission.
*/

#include "common_header.h"

typedef struct
{
    s32 version; // 0
    s32 reserved0; // 4
    s32 err; // 8
    void *edramAddr; // 12
    s32 neededMem; // 16
    s32 reserved1; // 20
    void *inBuf; // 24
    s32 readSample; // 28
    void *outBuf; // 32
    s32 decodedSample; // 36

    /* Codec-specific parameter block (offsets 40..103). */
    union {
        struct {
            s8 b0;
            s8 b1;
            s8 b2;
            s8 b3;
        } v8;
        s32 v32;
    } param0; // 40 (e.g. input buffer size / sample rate)
    union {
        struct {
            u8 b0;
            s8 b1;
            s8 b2;
            s8 b3;
        } v8;
        struct {
            s16 h0;
            s16 h1;
        } v16;
        s32 v32;
    } param1; // 44
    s32 param[14]; // 48..103
    void *allocMem; // 104
} SceAudiocodecCodec;

s32 sceAudiocodecCheckNeedMem(SceAudiocodecCodec *info, s32 codec);
s32 sceAudiocodecInit(SceAudiocodecCodec *info, s32 codec);
s32 sceAudiocodec_3DD7EE1A(SceAudiocodecCodec *info, s32 codec);
s32 sceAudiocodecDecode(SceAudiocodecCodec *info, s32 codec);
s32 sceAudiocodecGetInfo(SceAudiocodecCodec *info, s32 codec);
s32 sceAudiocodecAlcExtendParameter(SceAudiocodecCodec *info, s32 codec, s32 *sizeOut);
s32 sceAudiocodecGetEDRAM(SceAudiocodecCodec *info, s32 codec);
s32 sceAudiocodecReleaseEDRAM(SceAudiocodecCodec *info);


