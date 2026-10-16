/* Internet checksum helpers; original source filename unavailable. */
#include <stdint.h>

unsigned int BtInternetChecksum(unsigned int initialSum,
				const uint16_t *words, int length)
{
	while (length > 1) {
		initialSum += *words++;
		length -= 2;
	}
	if (length == 1)
		initialSum += *(const uint8_t *)words;
	while (initialSum >> 16)
		initialSum = (uint16_t)initialSum + (initialSum >> 16);
	return (uint16_t)initialSum;
}

unsigned int BtTransportChecksum(unsigned int protocol,
				 unsigned int sourceAddress,
				 unsigned int destinationAddress,
				 const void *packet, unsigned int length)
{
	uint16_t protocolBe = __builtin_bswap16((uint16_t)protocol);
	uint16_t lengthBe = __builtin_bswap16((uint16_t)length);
	unsigned int sourceBe = __builtin_bswap32(sourceAddress);
	unsigned int destinationBe = __builtin_bswap32(destinationAddress);
	unsigned int sourceSum, destinationSum;

	if ((uintptr_t)packet & 1)
		return 0;
	sourceSum = (uint16_t)sourceBe + (sourceBe >> 16);
	destinationSum = (uint16_t)destinationBe + (destinationBe >> 16);
	return BtInternetChecksum(protocolBe + lengthBe + sourceSum +
				  destinationSum, packet, length);
}
