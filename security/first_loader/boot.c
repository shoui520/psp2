/*
 * first_loader's program, run at 0x5c000 after reset copies it there: it brings up the
 * Cmep's keyrings, reads second_loader from the eMMC (Sdif) or the SD card, decrypts and
 * checks it, and starts it. It uses the Cmep control block (0xe0000000), the keyring
 * controller (0xe0030000), the bignum unit (0xe0040000), Bigmac (0xe0050000), Sdif
 * (0xe0b00000), GPIO 0 (0xe20a0000) and Pervasive (0xe3100000).
 *
 * The program is incomplete. These helpers are compiled but are not linked into
 * the ROM until the program's startup and its callers are implemented.
 */

#ifndef FL_BUILD_DATE
#error "FL_BUILD_DATE (build, YYYYMMDD in hex) not set"
#endif

/* code that differs between the builds: FL_SINCE(0x20110616) */
#define FL_SINCE(date)	(FL_BUILD_DATE >= (date))

/* Copy whole words and return their sum. When size / 4 is zero the original
 * hardware loop counter wraps, so that case requires a full counter cycle.
 */
unsigned int copyWordsAndSum(unsigned int *dst, const unsigned int *src,
			     unsigned int size)
{
	unsigned int words = size >> 2;
	unsigned int sum = 0;

	do {
		unsigned int word = *src++;

		*dst++ = word;
		sum += word;
	} while (--words != 0);

	return sum;
}

void *fillBytes(void *dst, int value, unsigned int size)
{
	unsigned char *p = dst;
	unsigned int i;

	for (i = 0; i < size; i++)
		p[i] = value;
	return dst;
}

/* Returns the OR of all byte differences, not a lexicographic comparison. */
unsigned int compareBytes(const void *left, const void *right, unsigned int size)
{
	const unsigned char *a = left;
	const unsigned char *b = right;
	unsigned int difference = 0;
	unsigned int i;

	for (i = 0; i < size; i++)
		difference |= a[i] ^ b[i];
	return difference;
}

/* Read bits in little-endian bit order, as used by the card registers. */
unsigned int readBits(const unsigned char *data, unsigned int first,
		      unsigned int count)
{
	unsigned int result = 0;
	unsigned int i;

	data += first >> 3;
	first &= 7;
	for (i = 0; i < count; i++) {
		if (*data & (1U << first))
			result |= 1U << (i & 31);
		if (++first == 8) {
			first = 0;
			data++;
		}
	}
	return result;
}
