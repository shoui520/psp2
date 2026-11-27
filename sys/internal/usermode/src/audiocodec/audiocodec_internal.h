#ifndef SYS_USERMODE_AUDIOCODEC_INTERNAL_H
#define SYS_USERMODE_AUDIOCODEC_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>

extern void *sceClibMemcpy_safe(void *dst, const void *src, SceSize len);
extern void *sceClibMemset(void *dst, int c, SceSize len);

/* Codec type identifiers accepted by SceAudiocodec */
#define AUDIOCODEC_TYPE_AT3P		0x1000
#define AUDIOCODEC_TYPE_AT3		0x1001
#define AUDIOCODEC_TYPE_MP3		0x1002
#define AUDIOCODEC_TYPE_AAC		0x1003
#define AUDIOCODEC_TYPE_AT9		0x1004

/* Error codes returned by SceAudiocodec */
#define AUDIOCODEC_ERROR_DECODE		((int32_t)0x807F8001)
#define AUDIOCODEC_ERROR_INVALID_ARG	((int32_t)0x807F8002)
#define AUDIOCODEC_ERROR_UNSUPPORTED	((int32_t)0x807F8003)

/*
 * SceAudiocodec work buffer header passed to all SceAudiocodec exports.
 * Words [0..7] are common; words [8..16] carry codec-specific parameters.
 */
typedef struct AudiocodecWork {
	int32_t internalError;		/* 0x00 */
	void *workMem;			/* 0x04 */
	uint32_t workMemSize;		/* 0x08 */
	uint32_t reserved0C;		/* 0x0C */
	void *inputBuf;			/* 0x10 */
	int32_t inputConsumed;		/* 0x14 */
	void *outputBuf;		/* 0x18 */
	int32_t outputBytes;		/* 0x1C */
	uint32_t codecParam[16];	/* 0x20..0x5C */
} AudiocodecWork;

/* Shared x^(4/3) inverse quantization table (used by MP3 and AAC) */
extern const float kAudioPow43Table[8207];

/* Codec dispatch entrypoints */
int32_t atrac3CheckNeedMem(uint32_t *work);
int32_t atrac3Init(uint32_t *work);
int32_t atrac3Decode(uint32_t *work);

int32_t atrac3pCheckNeedMem(uint32_t *work);
int32_t atrac3pInit(uint32_t *work);
int32_t atrac3pDecode(uint32_t *work);

int32_t mp3CheckNeedMem(uint32_t *work);
int32_t mp3Init(uint32_t *work);
int32_t mp3Decode(uint32_t *work);

int32_t aacCheckNeedMem(uint32_t *work);
int32_t aacInit(uint32_t *work);
int32_t aacDecode(uint32_t *work);

int32_t atrac9CheckNeedMem(uint32_t *work);
int32_t atrac9Init(uint32_t *work);
int32_t atrac9Decode(uint32_t *work);

/* Statically linked math & sort helpers (fdlibm single-precision + BSD qsort) */
float audioFloorf(float x);
float audioLogf(float x);
float audioPowf(float x, float y);
float audioScalbnf(float x, int n);
float audioSinf(float x);
float audioCeilf(float x);
float audioSqrtf(float x);
float audioCopysignf(float x, float y);
void audioQsort(void *base, size_t nmemb, size_t size,
		int (*compar)(const void *, const void *));

#endif /* SYS_USERMODE_AUDIOCODEC_INTERNAL_H */
