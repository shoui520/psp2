/*
 * libSceCes's callback state (Sony's internal/usermode/src/ces/libces/src/ces_callback.h): what
 * a conversion hands the error callback it calls, and what the callback hands back. Laid out
 * and named from the library's debug information (SDK 3.570.011 libSceCes.a).
 */
#ifndef _CES_CALLBACK_H
#define _CES_CALLBACK_H

#include <stdint.h>

typedef struct SceCesState {
	int32_t returnValue;
	union {
		uint32_t bitsValue;
		struct {
			uint32_t endian:1;
			uint32_t noUse:31;
		} src;
	} ctx;
	const void *codeBuffer;
	uint32_t codeLen;
	uint32_t maxCharCount;
	uint32_t setCharCount;
	uint32_t replaceCode;
	uint32_t replaceCode2;
	uint8_t replaceMbc[4];
	const uint8_t *replaceCodes;
	uint32_t replaceCodeN;
	void *callbackArg;
	void *callback;
} SceCesState;

#endif /* _CES_CALLBACK_H */
