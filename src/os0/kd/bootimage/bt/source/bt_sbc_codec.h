#ifndef BT_SBC_CODEC_H
#define BT_SBC_CODEC_H

#include <stddef.h>
#include <stdint.h>

typedef struct SbcContext {
	union {
		uint8_t *stream;
		const uint8_t *input;
	};
	uint32_t bitBuffer;
	int bitCount;
	int frequency;
	int blocks;
	int mode;
	int allocation;
	int subbands;
	int bitpool;
	int minimumBitpool;
	int maximumBitpool;
	uint32_t crc;
	int channels;
	uint32_t jointSubbands;
	int stereoInput;
	int fadeState;
	int fadeProgress;
	int scaleFactors[2][8];
	int bitNeed[2][8];
	int bits[2][8];
	int jointScaleFactors[2][8];
	uint8_t reserved[4];
	float samples[2][16][8];
	float workingSamples[2][16][8];
	float history[2][400];
} SbcContext;

_Static_assert(sizeof(SbcContext) == 5576, "SBC context");
_Static_assert(_Alignof(SbcContext) == 4, "SBC context alignment");
_Static_assert(offsetof(SbcContext, frequency) == 12, "SBC frequency");
_Static_assert(offsetof(SbcContext, channels) == 48, "SBC channels");
_Static_assert(offsetof(SbcContext, scaleFactors) == 68, "SBC scale factors");
_Static_assert(offsetof(SbcContext, bitNeed) == 132, "SBC bit need");
_Static_assert(offsetof(SbcContext, bits) == 196, "SBC bit allocation");
_Static_assert(offsetof(SbcContext, jointScaleFactors) == 260, "SBC joint scale factors");
_Static_assert(offsetof(SbcContext, reserved) == 324, "SBC unused bytes");
_Static_assert(offsetof(SbcContext, samples) == 328, "SBC samples");
_Static_assert(offsetof(SbcContext, workingSamples) == 1352, "SBC working samples");
_Static_assert(offsetof(SbcContext, history) == 2376, "SBC history");

extern const int gSbcLoudnessOffsetsFour[4][4];
extern const int gSbcLoudnessOffsetsEight[4][8];
extern const float gSbcPrototypeWindow[80];
extern const float gSbcPrototypeWindowFour[40];
void SbcSynthesizeEight(float *historyPosition, const float *subbands,
			float *output);
void SbcAnalyzeEight(const float *historyPosition, float *subbands);
void __attribute__((pcs("aapcs-vfp")))
SbcConvertStereo(float *leftPosition, float *rightPosition,
		 const int16_t *pcm, float scale);

void SbcAllocateBits(SbcContext *context, int encoding);
void *SbcResetDecoder(SbcContext *context);
int SbcInitializeEncoder(SbcContext *context, int frequency, int blocks,
			 int mode, int allocation, int subbands,
			 int minimumBitpool, int maximumBitpool);
int SbcEncodeFrames(SbcContext *context, const uint8_t *pcm,
		    uint8_t *encoded, int frameCount);
int SbcDecodeFrame(SbcContext *context, const uint8_t *encoded,
		   int16_t *pcm, int interleaved);

#endif
