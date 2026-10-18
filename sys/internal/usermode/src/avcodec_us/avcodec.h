/*
 * SceAvcodec JPEG syscalls whose arguments past the third (or second) arrive as a
 * block on the caller's stack (os0:us/avcodec_us.suprx packs them).
 */

#pragma once

#include <scetypes.h>

int _sceJpegDecodeMJpeg(int a0, int a1, int a2, const void *args);
int _sceJpegDecodeMJpegYCbCr(int a0, int a1, int a2, const void *args);
int _sceJpegMJpegCsc(int a0, int a1, int a2, const void *args);
int _sceJpegGetOutputInfo(int a0, int a1, int a2, const void *args);
int _sceJpegInitMJpegWithParam(const void *param, const void *args);
int _sceJpegCsc(int a0, int a1, int a2, const void *args);
int _sceJpegEncoderInit(int context, int a1, int a2, const void *args);
int _sceJpegEncoderEncode(int context, int a1, const void *args);
int _sceJpegEncoderEnd(int context, const void *args);
int _sceJpegEncoderSetValidRegion(int context, int a1, int a2, const void *args);
int _sceJpegEncoderSetCompressionRatio(int context, int a1, const void *args);
int _sceJpegEncoderSetHeaderMode(int context, int a1, const void *args);
int _sceJpegEncoderSetOutputAddr(int context, int a1, int a2, const void *args);
int _sceJpegEncoderCsc(int context, int a1, int a2, const void *args);
int _sceJpegEncoderInitWithParam(int context, const void *param, const void *args);
