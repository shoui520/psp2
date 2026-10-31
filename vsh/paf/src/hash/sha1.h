#ifndef PAF_HASH_SHA1_H
#define PAF_HASH_SHA1_H

#include <stddef.h>
#include <stdint.h>

typedef struct SHA1Context {
	uint32_t Intermediate_Hash[5];
	uint32_t Length_Low;
	uint32_t Length_High;
	int16_t Message_Block_Index;
	uint8_t Message_Block[64];
	int32_t Computed;
	int32_t Corrupted;
} SHA1Context;

enum {
	shaSuccess = 0,
	shaNull,
	shaInputTooLong,
	shaStateError
};

#define SHA1HashSize 20
#define SHA1Reset scePafSha1Init
#define SHA1Input scePafSha1Update
#define SHA1Result scePafSha1Result

typedef char SHA1ContextSizeCheck[sizeof(SHA1Context) == 104 ? 1 : -1];

int32_t SHA1Reset(SHA1Context *context);
int32_t SHA1Input(SHA1Context *context, const uint8_t *message_array, size_t length);
int32_t SHA1Result(SHA1Context *context, uint8_t message_digest[SHA1HashSize]);
void SHA1PadMessage(SHA1Context *context);
void SHA1ProcessMessageBlock(SHA1Context *context);

#endif
