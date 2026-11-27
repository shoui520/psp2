#ifndef SCE_DEFLATE_I_H
#define SCE_DEFLATE_I_H

#include <stddef.h>
#include <stdint.h>

#ifndef __SNC__
/* SNC's intrinsics, as Sony's GCC 4.8 build of the library computes them */
#define __clz(x) ((unsigned int)__builtin_clz(x))

static inline unsigned int __rbit(unsigned int x)
{
	unsigned int r;

	__asm__("rbit %0, %1" : "=r" (r) : "r" (x));
	return r;
}
#endif

typedef union {
	uint16_t *p;
	int i;
} count_t;

#ifdef SCE_DEFLT_2014
typedef struct {
	/* Histograms and sorted-symbol cursors become the fast literal table. */
	union {
		uint16_t ft[256];
		struct {
			uint32_t flags[16];
			count_t count[15];
			uint8_t len[286];
			uint8_t pad[6];
		} s;
	} u;
	union {
		int16_t distsym_fix[32];
		struct {
			int16_t distsym[30];
			uint32_t distmaxinfo[15];
		} d;
	} dist;
	uint32_t litmaxinfo[7];
	union {
		uint16_t lit_fix[288];
		struct {
			union {
				uint16_t lit[286];
				signed char len[128];
			} sym;
			uint16_t litmaxcode[8];
		} s;
	} tail;
} deflateInternalData_t;

typedef char DeflateInternalDataSizeCheck[
	sizeof(deflateInternalData_t) == 1248 ? 1 : -1];
typedef char DeflateSymbolsOffsetCheck[
	offsetof(deflateInternalData_t, tail.s.sym) == 660 ? 1 : -1];
typedef char DeflateLengthsOffsetCheck[
	offsetof(deflateInternalData_t, u.s.len) == 124 ? 1 : -1];

enum {
	DEFLATE_LITERAL_SYMBOL_BASE = offsetof(deflateInternalData_t, tail.s.sym) / 2
};

extern const uint8_t _sceDeflateLengthOrder[19];

int _sceDeflateMakeHuffmanTableLength(deflateInternalData_t *pW);
int _sceDeflateMakeHuffmanTable(deflateInternalData_t *pW);
int _sceDeflateMakeHuffmanFixcode(deflateInternalData_t *pW);
#else
typedef struct {
	/* Histograms and sorted-symbol cursors become the fast literal table. */
	union {
		uint16_t ft[256];
		struct {
			uint32_t flags[16];
			count_t count[15];
			uint8_t len[286];
			uint8_t pad[10];
		} s;
	} u;
	uint32_t distmaxinfo[10];
	uint8_t distmaxcode[5];
	char pad[3];
	int endflag;
	uint32_t litmaxinfo[7];
	uint8_t distsym[30];
	char padDist[2];
	union {
		uint16_t ftdist[32];
		/* These construction limits are consumed before ftdist overwrites them. */
		struct {
			uint8_t litmaxcode[8];
			int distminlen;
		} w;
	} v;
	union {
		uint16_t lit[286];
		signed char len[128];
	} sym;
} deflateInternalData_t;

typedef char DeflateInternalDataSizeCheck[
	sizeof(deflateInternalData_t) == 1260 ? 1 : -1];
typedef char DeflateSymbolsOffsetCheck[
	offsetof(deflateInternalData_t, sym) == 688 ? 1 : -1];
typedef char DeflateLengthsOffsetCheck[
	offsetof(deflateInternalData_t, u.s.len) == 124 ? 1 : -1];

enum {
	DEFLATE_LITERAL_SYMBOL_BASE = offsetof(deflateInternalData_t, sym) / 2,
	DEFLATE_DISTANCE_SYMBOL_BASE = offsetof(deflateInternalData_t, distsym)
};

extern const uint8_t _sceDeflateLengthOrder[19];

unsigned int _sceDeflateMakeHuffmanTableLength(deflateInternalData_t *pW);
int _sceDeflateMakeHuffmanTable(deflateInternalData_t *pW);
void _sceDeflateMakeHuffmanFixcode(deflateInternalData_t *pW, int prevflag);
#endif

#endif
