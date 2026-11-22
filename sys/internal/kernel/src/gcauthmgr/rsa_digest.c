/* Descriptive source name: the original library has no debug records. */
#include <gcauthmgr/rsa_signature.h>

int GcAuthRsaEncodeDigest(unsigned char *output, int size,
			const MarlinRsaData *digest, int algorithm)
{
	/* DER DigestInfo: SEQUENCE, AlgorithmIdentifier/OID, OCTET STRING. */
	unsigned char digestInfo[19];
	unsigned char *destination;
	const unsigned char *source;
	int digestSize;
	int prefixSize;
	int paddingSize;
	int remaining;
	int index;
	int result = 0;

	switch (algorithm) {
	case 2: /* MD2, 1.2.840.113549.2.2 */
		digestInfo[0] = 0x30;
		digestInfo[2] = 0x30;
		digestInfo[4] = 0x06;
		digestInfo[1] = 0x20;
		digestInfo[3] = 0x0c;
		digestInfo[5] = 0x08;
		digestInfo[6] = 0x2a;
		digestInfo[9] = 0x86;
		digestInfo[7] = 0x86;
		digestInfo[8] = 0x48;
		digestSize = 16;
		digestInfo[10] = 0xf7;
		prefixSize = 18;
		digestInfo[11] = 0x0d;
		digestInfo[12] = 0x02;
		digestInfo[13] = 0x02;
		digestInfo[14] = 0x05;
		digestInfo[15] = 0x00;
		digestInfo[16] = 0x04;
		digestInfo[17] = 16;
		break;
	case 4: /* MD5, 1.2.840.113549.2.5 */
		digestInfo[0] = 0x30;
		digestInfo[2] = 0x30;
		digestInfo[4] = 0x06;
		digestInfo[1] = 0x20;
		digestInfo[3] = 0x0c;
		digestInfo[5] = 0x08;
		digestInfo[6] = 0x2a;
		digestInfo[9] = 0x86;
		digestInfo[7] = 0x86;
		digestInfo[8] = 0x48;
		digestSize = 16;
		digestInfo[10] = 0xf7;
		prefixSize = 18;
		digestInfo[11] = 0x0d;
		digestInfo[12] = 0x02;
		digestInfo[13] = 0x05;
		digestInfo[14] = 0x05;
		digestInfo[15] = 0x00;
		digestInfo[16] = 0x04;
		digestInfo[17] = 16;
		break;
	case 5: /* SHA-1, 1.3.14.3.2.26 */
		digestInfo[0] = 0x30;
		digestInfo[2] = 0x30;
		digestInfo[4] = 0x06;
		digestInfo[1] = 0x21;
		digestInfo[3] = 0x09;
		digestInfo[5] = 0x05;
		digestInfo[6] = 0x2b;
		digestInfo[7] = 0x0e;
		digestSize = 20;
		digestInfo[8] = 0x03;
		prefixSize = 15;
		digestInfo[9] = 0x02;
		digestInfo[10] = 0x1a;
		digestInfo[11] = 0x05;
		digestInfo[12] = 0x00;
		digestInfo[13] = 0x04;
		digestInfo[14] = 20;
		break;
	case 11: /* SHA-256, 2.16.840.1.101.3.4.2.1 */
		digestInfo[0] = 0x30;
		digestInfo[2] = 0x30;
		digestInfo[4] = 0x06;
		digestInfo[1] = 0x31;
		digestInfo[3] = 0x0d;
		digestInfo[5] = 0x09;
		digestInfo[6] = 0x60;
		digestInfo[7] = 0x86;
		digestInfo[8] = 0x48;
		digestInfo[14] = 0x01;
		digestInfo[9] = 0x01;
		digestInfo[10] = 0x65;
		digestSize = 32;
		digestInfo[11] = 0x03;
		prefixSize = 19;
		digestInfo[12] = 0x04;
		digestInfo[13] = 0x02;
		digestInfo[15] = 0x05;
		digestInfo[16] = 0x00;
		digestInfo[17] = 0x04;
		digestInfo[18] = 32;
		break;
	case 12: /* SHA-384, 2.16.840.1.101.3.4.2.2 */
		digestInfo[4] = 0x06;
		digestInfo[0] = 0x30;
		digestInfo[1] = 0x41;
		digestInfo[2] = 0x30;
		digestInfo[3] = 0x0d;
		digestInfo[5] = 0x09;
		digestInfo[6] = 0x60;
		digestInfo[7] = 0x86;
		digestInfo[8] = 0x48;
		digestInfo[9] = 0x01;
		digestInfo[10] = 0x65;
		digestInfo[11] = 0x03;
		prefixSize = 19;
		digestInfo[12] = 0x04;
		digestSize = 48;
		digestInfo[13] = 0x02;
		digestInfo[14] = 0x02;
		digestInfo[15] = 0x05;
		digestInfo[16] = 0x00;
		digestInfo[17] = 0x04;
		digestInfo[18] = 48;
		break;
	case 13: /* SHA-512, 2.16.840.1.101.3.4.2.3 */
		digestInfo[0] = 0x30;
		digestInfo[2] = 0x30;
		digestInfo[4] = 0x06;
		digestInfo[1] = 0x51;
		digestInfo[3] = 0x0d;
		digestInfo[5] = 0x09;
		digestInfo[6] = 0x60;
		digestInfo[7] = 0x86;
		digestInfo[8] = 0x48;
		digestInfo[9] = 0x01;
		digestInfo[10] = 0x65;
		digestSize = 64;
		digestInfo[14] = 0x03;
		digestInfo[11] = 0x03;
		prefixSize = 19;
		digestInfo[12] = 0x04;
		digestInfo[13] = 0x02;
		digestInfo[15] = 0x05;
		digestInfo[16] = 0x00;
		digestInfo[17] = 0x04;
		digestInfo[18] = 64;
		break;
	case 14: /* SHA-224, 2.16.840.1.101.3.4.2.4 */
		digestInfo[0] = 0x30;
		digestInfo[2] = 0x30;
		digestInfo[4] = 0x06;
		digestInfo[1] = 0x2d;
		digestInfo[3] = 0x0d;
		digestInfo[5] = 0x09;
		digestInfo[6] = 0x60;
		digestInfo[7] = 0x86;
		digestInfo[8] = 0x48;
		digestInfo[9] = 0x01;
		digestInfo[10] = 0x65;
		digestSize = 28;
		digestInfo[11] = 0x03;
		prefixSize = 19;
		digestInfo[12] = 0x04;
		digestInfo[13] = 0x02;
		digestInfo[14] = 0x04;
		digestInfo[15] = 0x05;
		digestInfo[16] = 0x00;
		digestInfo[17] = 0x04;
		digestInfo[18] = 28;
		break;
	default:
		return -1;
	}
	if (digest->size != digestSize)
		return -1;
	paddingSize = size - (prefixSize + digestSize) - 3;
	if (size < prefixSize + digestSize + 11)
		return -1;
	output[0] = 0;
	output[1] = 1;
	destination = output + 2;
	if (paddingSize > 0) {
		unsigned char *padding = destination;

		remaining = paddingSize;
		do {
			--remaining;
			*padding++ = 0xff;
		} while (remaining != 0);
	}
	destination += paddingSize;
	output[paddingSize + 2] = 0;
	++destination;
	if (prefixSize > 0) {
		unsigned char *prefix = destination;

		source = digestInfo;
		remaining = prefixSize;
		do {
			unsigned char byte = *source++;

			--remaining;
			*prefix++ = byte;
		} while (remaining != 0);
	}
	if (digestSize > 0) {
		destination += prefixSize;
		index = 0;
		do {
			*destination++ = digest->data[index];
			++index;
		} while (index < digestSize);
	}
	return result;
}
