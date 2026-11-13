/* Filename and descriptive names are ours: CRYPTOCME's low-level symmetric
 * ciphers: an algorithm descriptor, the cipher object built from it, and the
 * buffering object over it that the cipher backend drives.
 */
#ifndef LIBSSL_CRYPTOCME_LL_CIPH_H
#define LIBSSL_CRYPTOCME_LL_CIPH_H

#include <scetypes.h>
#include "ll_obj.h"
#include "../dmem/dmem.h"

typedef struct SslCipherCore SslCipherCore;
typedef struct SslCipherDescriptor SslCipherDescriptor;

/* A block cipher on one block in place, with its key schedule */
typedef int (*SslCipherBlockFunction)(SceUInt32 *block, const void *schedule);
/* A stream cipher over a run of bytes */
typedef void (*SslCipherStreamFunction)(void *key, SceUInt32 length,
					const unsigned char *input,
					unsigned char *output);

/* SslCipherDescriptor.flags */
#define SSL_CIPHER_SAVABLE 0x1
/* A block cipher in a chaining mode */
#define SSL_CIPHER_CHAINED 0x20000
/* The state and the extra area are 16-byte aligned */
#define SSL_CIPHER_ALIGNED 0x80000

struct SslCipherDescriptor {
	const char *name;
	SslDerBuffer oid;
	unsigned short maxKeyLength;
	unsigned char keyLength;
	unsigned char minKeyLength;
	unsigned char blockSize;
	unsigned char ivLength;
	unsigned short stateSize;
	unsigned short extraSize;
	unsigned short version;
	int (*SetKey)(SslCipherCore *core, const void *key,
		      const unsigned char *iv, int flags);
	int (*Process)(SslCipherCore *core, unsigned char *output,
		       const unsigned char *input, SceUInt32 length);
	void (*Cleanup)(SslCipherCore *core);
	int (*GetInfo)(const SslCipherDescriptor *descriptor,
		       SslCipherCore *core, int selector, SceUInt32 *value,
		       void **pointer);
	SceUInt32 flags;
	const void *engine;
	const SceUInt32 *stateSchema;
	SslDirectMemoryContext *dmem;
};

_Static_assert(sizeof(SslCipherDescriptor) == 56, "SslCipherDescriptor");

/* SslCipherCore.flags */
#define SSL_CIPHER_CORE_ALLOCATED 0x1
#define SSL_CIPHER_CORE_KEY_SAVED 0x100
#define SSL_CIPHER_CORE_STATE_ALLOCATED 0x10000

/* sslSetCipherCoreKey flags: the direction, keep the key schedule, keep the
 * chaining value, save a copy of the key
 */
#define SSL_CIPHER_ENCRYPT 0x1
#define SSL_CIPHER_KEEP_SCHEDULE 0x2
#define SSL_CIPHER_KEEP_IV 0x4
#define SSL_CIPHER_SAVE_KEY 0x100

/* The cipher object: this header, the algorithm's state, then its extra
 * area
 */
struct SslCipherCore {
	SslDirectMemoryContext *dmem;
	const SslCipherDescriptor *descriptor;
	unsigned char *initialIv;
	unsigned char *iv;
	void *state;
	void *schedule;
	unsigned short encrypt;
	unsigned short ivUsed;
	SceUInt32 size;
	SceUInt32 keyLength;
	SceUInt32 savedKeyLength;
	void *savedKey;
	void *extra;
	SceUInt32 parameter;
	SceUInt32 flags;
};

_Static_assert(sizeof(SslCipherCore) == 56, "SslCipherCore");

/* Info selectors of the descriptor and the object */
#define SSL_CIPHER_INFO_BLOCK_SIZE 1
#define SSL_CIPHER_INFO_MIN_KEY_LENGTH 2
#define SSL_CIPHER_INFO_MAX_KEY_LENGTH 3
#define SSL_CIPHER_INFO_KEY_LENGTH 4
#define SSL_CIPHER_INFO_IV_LENGTH 6
#define SSL_CIPHER_INFO_STATE_SIZE 7
#define SSL_CIPHER_INFO_PARAMETER 8
#define SSL_CIPHER_INFO_SET_PARAMETER 9
#define SSL_CIPHER_INFO_IV 11
#define SSL_CIPHER_INFO_INITIAL_IV 12
#define SSL_CIPHER_INFO_OID 13
#define SSL_CIPHER_INFO_VERSION 14
#define SSL_CIPHER_INFO_NAME 15
#define SSL_CIPHER_INFO_SAVED_KEY 16
#define SSL_CIPHER_INFO_DIRECTION 21
#define SSL_CIPHER_INFO_DESCRIPTOR 22
#define SSL_CIPHER_INFO_SAVABLE 23

int sslProcessStreamCipher(SslCipherCore *core, unsigned char *output,
			   const unsigned char *input, SceUInt32 length);
int sslDecryptCbc(const unsigned char *input, unsigned char *output,
		  SceUInt32 length, const void *schedule, unsigned char *iv,
		  SslCipherBlockFunction block);
int sslEncryptCbc(const unsigned char *input, unsigned char *output,
		  SceUInt32 length, const void *schedule, unsigned char *iv,
		  SslCipherBlockFunction block);
int sslProcessCbcCipher(SslCipherCore *core, unsigned char *output,
			const unsigned char *input, SceUInt32 length);

int sslFreeCipherCore(SslCipherCore *core);
int sslCreateCipherCore(SslCipherCore **result,
			const SslCipherDescriptor *descriptor,
			SslDirectMemoryContext *dmem);
int sslSetCipherCoreKey(SslCipherCore *core, const void *key,
			SceUInt32 keyLength, const unsigned char *iv, int flags);
int sslProcessCipherCore(SslCipherCore *core, unsigned char *output,
			 const unsigned char *input, SceUInt32 length);
int sslGetCipherDescriptorInfo(const SslCipherDescriptor *descriptor,
			       SslCipherCore *core, int selector,
			       SceUInt32 *value, void **pointer);
int sslGetCipherCoreInfo(SslCipherCore *core, int selector, SceUInt32 *value,
			 void **pointer);
SceUInt32 sslGetCipherCoreFlags(SslCipherCore *core);
int sslSetCipherCoreInfo(SslCipherCore *core, int selector, SceUInt32 value,
			 void **pointer);
int sslQueryCipherCoreInfo(SslCipherCore *core, int selector,
			   SceUInt32 *value, void **pointer);
int sslQueryCipherDescriptorInfo(const SslCipherDescriptor *descriptor,
				 SslCipherCore *core, int selector,
				 SceUInt32 *value, void **pointer);
int sslSaveCipherCoreState(SslCipherCore *core, void *buffer,
			   SceUInt32 *length, SceUInt32 max);
int sslRestoreCipherCoreState(SslCipherCore *core, const void *buffer,
			      SceUInt32 length);

/* The buffering of a cipher: whole blocks to the cipher, the rest kept for
 * the next call; at the end, the last blocks to a padding function
 */
typedef int (*SslCipherPadFunction)(SslCipherCore *core,
				    unsigned char *output,
				    SceUInt32 *outputLength, SceUInt32 max,
				    const unsigned char *input,
				    SceUInt32 inputLength, void *parameter,
				    SceUInt32 flags);

/* SslCipherBuffer.flags */
#define SSL_CIPHER_BUFFER_ALLOCATED 0x1
#define SSL_CIPHER_BUFFER_PADDING 0x4

typedef struct SslCipherBuffer {
	SslCipherCore *core;
	int encrypt;
	SceUInt32 flags;
	SslDirectMemoryContext *dmem;
	unsigned char *buffer;
	SceUInt32 blockSize;
	SceUInt32 count;
	SceUInt32 limit;
	SceUInt32 reserve;
	SslCipherPadFunction Pad;
	void *padParameter;
} SslCipherBuffer;

_Static_assert(sizeof(SslCipherBuffer) == 44, "SslCipherBuffer");

/* SslCipherBuffer info selectors */
#define SSL_CIPHER_BUFFER_INFO_BUFFERING 32
#define SSL_CIPHER_BUFFER_INFO_PAD_FUNCTION 33
#define SSL_CIPHER_BUFFER_INFO_RESERVE 34
#define SSL_CIPHER_BUFFER_INFO_PADDING 35
#define SSL_CIPHER_BUFFER_INFO_PAD_PARAMETER 36

/* sslProcessLowLevelCipher options: the last call */
#define SSL_CIPHER_BUFFER_FINAL 0x2

int sslFreeLowLevelCipher(SslCipherBuffer *buffer);
int sslCreateLowLevelCipher(SslCipherBuffer **result,
			    const SslCipherDescriptor *descriptor,
			    SslDirectMemoryContext *dmem);
int sslControlLowLevelCipher(SslCipherBuffer *buffer, const void *key,
			     SceUInt32 keyLength, const unsigned char *iv,
			     int encrypt, int flags);
int sslInitLowLevelCipher(SslCipherBuffer *buffer, const void *key,
			  SceUInt32 keyLength, const unsigned char *iv,
			  int encrypt);
int sslProcessLowLevelCipher(SslCipherBuffer *buffer, unsigned char *output,
			     SceUInt32 *outputLength, SceUInt32 max,
			     const unsigned char *input, SceUInt32 length,
			     int options);
int sslSetLowLevelCipherInfo(SslCipherBuffer *buffer, int selector,
			     SceUInt32 value, void *pointer);
int sslGetLowLevelCipherInfo(SslCipherBuffer *buffer, int selector,
			     SceUInt32 *value, void **pointer);
int sslSaveLowLevelCipherState(SslCipherBuffer *buffer, unsigned char *data,
			       SceUInt32 *length, SceUInt32 max);
int sslRestoreLowLevelCipherState(SslCipherBuffer *buffer,
				  const unsigned char *data,
				  SceUInt32 length);

extern SslCipherDescriptor sslAes128CbcCipherDescriptor;
extern SslCipherDescriptor sslAes256CbcCipherDescriptor;

const SslCipherDescriptor *sslGetDes3CipherDescriptor(void);
const SslCipherDescriptor *sslGetDesEde3CbcCipherDescriptor(void);
const SslCipherDescriptor *sslGetAes256CipherDescriptor(void);
const SslCipherDescriptor *sslGetAesCbcCipherDescriptor(void);
const SslCipherDescriptor *sslGetCamelliaCbcCipherDescriptor(void);
const SslCipherDescriptor *sslGetAes128CbcCipherDescriptor(void);

#endif
