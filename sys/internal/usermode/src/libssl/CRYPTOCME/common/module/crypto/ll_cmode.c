/* Filename and descriptive names are ours: CRYPTOCME's cipher modes, a
 * stream cipher over the data and cipher block chaining with any 64-bit
 * block cipher.
 */
#include "ll_ciph.h"

#define SSL_CBC_BLOCK_WORDS 2
#define SSL_CBC_BLOCK_SIZE (SSL_CBC_BLOCK_WORDS * 4)

int sslProcessStreamCipher(SslCipherCore *core, unsigned char *output,
			   const unsigned char *input, SceUInt32 length)
{
	if (length != 0)
		((SslCipherStreamFunction)core->descriptor->engine)(
			core->schedule, length, input, output);
	return 0;
}

int sslDecryptCbc(const unsigned char *input, unsigned char *output,
		  SceUInt32 length, const void *schedule, unsigned char *iv,
		  SslCipherBlockFunction block)
{
	SceUInt32 data[SSL_CBC_BLOCK_WORDS];
	SceUInt32 chain[SSL_CBC_BLOCK_WORDS];
	SceUInt32 cipher0, cipher1;
	SceUInt32 blocks;
	int words = SSL_CBC_BLOCK_WORDS;
	SceUInt32 *ivec = chain;
	int status = 0;
	const unsigned char *source;
	unsigned char *destination;
	int i;

	destination = (unsigned char *)chain;
	source = iv;
	for (i = SSL_CBC_BLOCK_WORDS; i != 0; i--) {
		destination[0] = source[0];
		destination[1] = source[1];
		destination[2] = source[2];
		destination[3] = source[3];
		destination += 4;
		source += 4;
	}
	for (blocks = length / SSL_CBC_BLOCK_SIZE; blocks != 0; blocks--) {
		destination = (unsigned char *)data;
		source = input;
		for (i = words; i != 0; i--) {
			destination[0] = source[0];
			destination[1] = source[1];
			destination[2] = source[2];
			destination[3] = source[3];
			destination += 4;
			source += 4;
		}
		cipher0 = data[0];
		cipher1 = data[1];
		block(data, schedule);
		chain[0] ^= data[0];
		chain[1] ^= data[1];
		destination = output;
		source = (const unsigned char *)ivec;
		for (i = words; i != 0; i--) {
			destination[0] = source[0];
			destination[1] = source[1];
			destination[2] = source[2];
			destination[3] = source[3];
			destination += 4;
			source += 4;
		}
		chain[0] = cipher0;
		chain[1] = cipher1;
		input += SSL_CBC_BLOCK_SIZE;
		output += SSL_CBC_BLOCK_SIZE;
	}
	destination = iv;
	source = (const unsigned char *)ivec;
	for (i = words; i != 0; i--) {
		destination[0] = source[0];
		destination[1] = source[1];
		destination[2] = source[2];
		destination[3] = source[3];
		destination += 4;
		source += 4;
	}
	return status;
}

int sslEncryptCbc(const unsigned char *input, unsigned char *output,
		  SceUInt32 length, const void *schedule, unsigned char *iv,
		  SslCipherBlockFunction block)
{
	SceUInt32 chain[SSL_CBC_BLOCK_WORDS];
	SceUInt32 data[SSL_CBC_BLOCK_WORDS];
	SceUInt32 blocks;
	int words = SSL_CBC_BLOCK_WORDS;
	SceUInt32 *ivec = chain;
	int status = 0;
	const unsigned char *source;
	unsigned char *destination;
	int i;

	destination = (unsigned char *)chain;
	source = iv;
	for (i = SSL_CBC_BLOCK_WORDS; i != 0; i--) {
		destination[0] = source[0];
		destination[1] = source[1];
		destination[2] = source[2];
		destination[3] = source[3];
		destination += 4;
		source += 4;
	}
	for (blocks = length / SSL_CBC_BLOCK_SIZE; blocks != 0; blocks--) {
		destination = (unsigned char *)data;
		source = input;
		for (i = words; i != 0; i--) {
			destination[0] = source[0];
			destination[1] = source[1];
			destination[2] = source[2];
			destination[3] = source[3];
			destination += 4;
			source += 4;
		}
		data[0] ^= chain[0];
		data[1] ^= chain[1];
		block(data, schedule);
		destination = output;
		source = (const unsigned char *)data;
		for (i = words; i != 0; i--) {
			destination[0] = source[0];
			destination[1] = source[1];
			destination[2] = source[2];
			destination[3] = source[3];
			destination += 4;
			source += 4;
		}
		chain[0] = data[0];
		chain[1] = data[1];
		input += SSL_CBC_BLOCK_SIZE;
		output += SSL_CBC_BLOCK_SIZE;
	}
	destination = iv;
	source = (const unsigned char *)ivec;
	for (i = words; i != 0; i--) {
		destination[0] = source[0];
		destination[1] = source[1];
		destination[2] = source[2];
		destination[3] = source[3];
		destination += 4;
		source += 4;
	}
	return status;
}

/* The engine holds the block function of each direction */
int sslProcessCbcCipher(SslCipherCore *core, unsigned char *output,
			const unsigned char *input, SceUInt32 length)
{
	const SslCipherBlockFunction *engine = core->descriptor->engine;

	if (core->encrypt)
		sslEncryptCbc(input, output, length, core->schedule, core->iv,
			      engine[0]);
	else
		sslDecryptCbc(input, output, length, core->schedule, core->iv,
			      engine[1]);
	return 0;
}
