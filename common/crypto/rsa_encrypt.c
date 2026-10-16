/* Descriptive source name: the original library has no debug records. */
#include <sce/rsa.h>

int MarlinRsaHasRandomGenerator(void);
void MarlinRsaFillNonzeroRandom(unsigned char *data, int size);
int MarlinRsaExponentOperation(unsigned int *output, const unsigned int *input,
			       const MarlinRsaExponentKey *key, int count);

int MarlinRsaEncryptBlock(MarlinRsaData *output, const MarlinRsaData *input,
			 const MarlinRsaExponentKey *key, int count, int size)
{
	unsigned int block[64];
	unsigned char *encoded = (unsigned char *)block;
	unsigned char *front;
	unsigned char *back;
	const unsigned char *source;
	unsigned int *first;
	unsigned int *last;
	int inputSize;
	int paddingSize;
	int half;
	int lastByteOffset;
	int remaining;

	if (!MarlinRsaHasRandomGenerator())
		return -1;
	inputSize = (int)input->size;
	if (inputSize < 0)
		return -1;
	if (inputSize > size - 11)
		return -1;
	paddingSize = size - inputSize - 3;
	encoded[0] = 0;
	encoded[1] = 2;
	MarlinRsaFillNonzeroRandom(encoded + 2, paddingSize);
	encoded[paddingSize + 2] = 0;
	remaining = (int)input->size;
	front = encoded + size - inputSize;
	lastByteOffset = 4 * (count - 1);
	half = count / 2;
	if (remaining > 0) {
		source = input->data;
		do {
			*front++ = *source++;
		} while (--remaining != 0);
	}
	front = encoded;
	back = encoded + lastByteOffset;
	remaining = half;
	if (remaining > 0) {
		first = block;
		last = block + count;
		do {
			unsigned int high = back[3] | (back[2] << 8) |
				(back[1] << 16) | ((unsigned int)back[0] << 24);
			unsigned int low = front[3] | (front[2] << 8) |
				(front[1] << 16) | ((unsigned int)front[0] << 24);

			back -= 4;
			front += 4;
			*first++ = high;
			*--last = low;
		} while (--remaining != 0);
	}
	if (MarlinRsaExponentOperation(block, block, key, count) == -1)
		return -1;
	front = output->data;
	back = front + lastByteOffset;
	remaining = half;
	if (remaining > 0) {
		first = block;
		last = block + count;
		do {
			unsigned int high = *--last;
			unsigned int low = *first++;

			front[3] = high;
			front[0] = high >> 24;
			front[1] = high >> 16;
			front[2] = high >> 8;
			back[0] = low >> 24;
			back[1] = low >> 16;
			front += 4;
			back[2] = low >> 8;
			back[3] = low;
			back -= 4;
		} while (--remaining != 0);
	}
	output->size = size;
	return 0;
}

int MarlinRsaEncrypt512(MarlinRsaData *output, const MarlinRsaData *input,
			const MarlinRsaExponentKey *key)
{
	return MarlinRsaEncryptBlock(output, input, key, 16, 64);
}

int MarlinRsaEncrypt1024(MarlinRsaData *output, const MarlinRsaData *input,
			 const MarlinRsaExponentKey *key)
{
	return MarlinRsaEncryptBlock(output, input, key, 32, 128);
}

int MarlinRsaEncrypt2048(MarlinRsaData *output, const MarlinRsaData *input,
			 const MarlinRsaExponentKey *key)
{
	return MarlinRsaEncryptBlock(output, input, key, 64, 256);
}
