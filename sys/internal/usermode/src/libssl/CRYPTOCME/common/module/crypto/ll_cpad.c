/* Filename and descriptive names are ours: CRYPTOCME's padding of the last
 * blocks of a block cipher: PKCS #5, and CBC with ciphertext stealing, which
 * needs no padding at all.
 */
#include "ll_ciph.h"
#include "../../object.h"
#include "../../../../memory.h"

#define SSL_CIPHER_MAX_BLOCK_SIZE 16

int sslCbcCtsPad(SslCipherCore *core, unsigned char *output,
		 SceUInt32 *outputLength, SceUInt32 maxOutputLength,
		 const unsigned char *input, SceUInt32 inputLength)
{
	SceUInt32 value;
	unsigned char *iv;
	unsigned char block[SSL_CIPHER_MAX_BLOCK_SIZE];
	unsigned char saved[SSL_CIPHER_MAX_BLOCK_SIZE];
	int ret = 0;
	SceUInt32 blockSize;
	SceUInt32 prefix;
	SceUInt32 i;
	int status;

	status = sslGetCipherDescriptorInfo(NULL, core,
					    SSL_CIPHER_INFO_BLOCK_SIZE, &value,
					    NULL);
	if (status != 0)
		return status;
	blockSize = value;
	if (blockSize >= inputLength)
		return SSL_CRYPTO_ERROR_NOT_INITIALIZED;
	if (inputLength > maxOutputLength)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	prefix = 0;
	if (inputLength > blockSize * 2) {
		/* Everything but the last two blocks, the second one partial
		 * or whole, is plain CBC
		 */
		prefix = (inputLength - blockSize - 1) & ~(blockSize - 1);
		if (core->keyLength != 0)
			core->descriptor->Process(core, output, input, prefix);
		output += prefix;
		input += prefix;
		inputLength -= prefix;
	}
	status = sslGetCipherCoreInfo(core, SSL_CIPHER_INFO_DIRECTION, &value,
				      NULL);
	if (status != 0)
		return status;
	status = sslGetCipherCoreInfo(core, SSL_CIPHER_INFO_IV, NULL,
				      (void **)&iv);
	if (status != 0)
		return status;
	if (value != 0) {
		for (i = 0; i < blockSize; i++)
			block[i] = iv[i] ^ input[i];
		sslFillMemory(iv, 0, blockSize);
		if (core->keyLength != 0)
			core->descriptor->Process(core, block, block,
						  blockSize);
		for (i = blockSize; i < inputLength; i++) {
			output[i] = block[i - blockSize];
			block[i - blockSize] ^= input[i];
		}
		sslFillMemory(iv, 0, blockSize);
		if (core->keyLength != 0)
			core->descriptor->Process(core, output, block,
						  blockSize);
	} else {
		sslCopyMemory(saved, iv, blockSize);
		sslFillMemory(iv, 0, blockSize);
		if (core->keyLength != 0)
			core->descriptor->Process(core, block, input,
						  blockSize);
		for (i = blockSize; i < inputLength; i++) {
			output[i] = block[i - blockSize] ^ input[i];
			block[i - blockSize] = input[i];
		}
		sslCopyMemory(iv, saved, blockSize);
		if (core->keyLength != 0)
			core->descriptor->Process(core, output, block,
						  blockSize);
	}
	sslCopyMemory(iv, output, blockSize);
	*outputLength = inputLength + prefix;
	return ret;
}

int sslPkcs5Pad(SslCipherCore *core, unsigned char *output,
		SceUInt32 *outputLength, SceUInt32 maxOutputLength,
		const unsigned char *input, SceUInt32 inputLength)
{
	SceUInt32 value;
	unsigned char block[SSL_CIPHER_MAX_BLOCK_SIZE];
	const SslCipherDescriptor *descriptor;
	SceUInt32 blockSize;
	SceUInt32 encrypt;
	SceUInt32 whole;
	SceUInt32 pad;
	SceUInt32 length;
	SceUInt32 i;
	int status;

	status = sslGetCipherDescriptorInfo(NULL, core,
					    SSL_CIPHER_INFO_BLOCK_SIZE, &value,
					    NULL);
	if (status != 0)
		return status;
	blockSize = value;
	status = sslGetCipherCoreInfo(core, SSL_CIPHER_INFO_DIRECTION, &value,
				      NULL);
	descriptor = core->descriptor;
	if (status != 0)
		return status;
	encrypt = value;
	if (encrypt != 0) {
		if (maxOutputLength < ((blockSize + inputLength) &
				       (blockSize - 1)))
			return SSL_CRYPTO_ERROR_NO_OPERATION;
		whole = inputLength & ~(blockSize - 1);
	} else {
		if (inputLength & (blockSize - 1))
			return SSL_CRYPTO_ERROR_NOT_INITIALIZED;
		whole = inputLength - blockSize;
		if (maxOutputLength < whole)
			return SSL_CRYPTO_ERROR_NO_OPERATION;
	}
	if (whole != 0) {
		if (core->keyLength != 0) {
			descriptor->Process(core, output, input, whole);
			descriptor = core->descriptor;
		}
		output += whole;
		input += whole;
		maxOutputLength -= whole;
		inputLength -= whole;
	}
	if (encrypt != 0) {
		if (inputLength != 0) {
			sslCopyMemory(block, input, inputLength);
			descriptor = core->descriptor;
		}
		pad = blockSize - inputLength;
		for (i = inputLength; i < blockSize; i++)
			block[i] = pad;
		if (core->keyLength != 0)
			descriptor->Process(core, output, block, blockSize);
		whole += blockSize;
	} else {
		if (core->keyLength != 0)
			descriptor->Process(core, block, input, blockSize);
		pad = block[blockSize - 1];
		if (blockSize < pad)
			return SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;
		length = blockSize - pad;
		if (length > maxOutputLength)
			return SSL_CRYPTO_ERROR_NO_OPERATION;
		for (i = blockSize - 2; i > length; i--) {
			if (block[i] != pad)
				return SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;
		}
		whole += length;
		if (length != 0)
			sslCopyMemory(output, block, length);
	}
	*outputLength = whole;
	return 0;
}
