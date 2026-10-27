/*
 * The secure NVS: the host first runs handshake mode 0x0f through command 0x00d0, then reads
 * (0x00d2) and writes (0x00d3) the NVS with packets encrypted under its session key; 0x00d1
 * locks it again. snvsCheck keeps the packets in sequence and checks their sums.
 */
#include "aes.h"
#include "authkeys.h"
#include "handshake.h"
#include "kernel.h"
#include "nvs.h"
#include "snvs.h"

static struct Snvs snvs;
static unsigned char snvsBuffer[0x30];

static unsigned int snvsChecksum(unsigned char *data, unsigned int length);
static int snvsCheck(struct SnvsPacket *packet, unsigned int length);

void snvsReset(void)
{
	snvs.sequence = 0;
	snvs.started = 0;
}

/* command 0x00d2 and 0x00d3's condition: the session is open */
int snvsUnlocked(void)
{
	return (unsigned char)handshakeUnlocked15();
}

/* 0x00d0: a step of the handshake */
int snvsHandshake(unsigned char *data)
{
	unsigned char result;

	result = 0;
	result = handshakeCommand(data);
	return result;
}

/* 0x00d1 */
int snvsLock(unsigned char *data)
{
	unsigned char result;

	result = 0;
	handshakeLock15();
	snvsReset();
	return result;
}

/* Decrypt the request, read the selected NVS bytes and encrypt the response. */
int snvsRead(unsigned char *data, unsigned char *answer)
{
	int result = 0;
	struct AesKey key;
	VP block;
	int poolResult;
	struct SnvsPacket *packet = (struct SnvsPacket *)snvsBuffer;
	unsigned int checksum;

	poolResult = pget_mpf(2, &block);
	if (poolResult != E_OK) {
		result = 0x13;
	} else {
		key.w = (unsigned long *)(unsigned long __near *)block;
		key.nk = 4;
		key.nb = 4;
		aesKeyExpansion((const unsigned long *)handshakeKey, (struct AesKey __near *)&key);
		aesCbcDecrypt((const unsigned char __near *)data, (unsigned char __near *)snvsBuffer,
			      (struct AesKey __near *)&key, handshakeIv, 0x10);
		result = snvsCheck(packet, 0x10);
		if (result == 0) {
			result = nvsRead(packet->offset, packet->size, snvsBuffer + 0x0e, 0);
			checksum = snvsChecksum(snvsBuffer, 0x2e);
			snvsBuffer[0x2e] = checksum;
			snvsBuffer[0x2f] = checksum >> 8;
			aesCbcEncrypt((const unsigned char __near *)snvsBuffer, (unsigned char __near *)answer,
				      (struct AesKey __near *)&key, handshakeIv, 0x30);
		}
	}
	rel_mpf(2, block);
	return result;
}

/* Decrypt the write packet and return its encrypted acknowledgement. */
int snvsWrite(unsigned char *data, unsigned char *answer)
{
	int result = 0;
	struct AesKey key;
	VP block;
	int poolResult;
	struct SnvsPacket *packet = (struct SnvsPacket *)snvsBuffer;
	unsigned int checksum;

	poolResult = pget_mpf(2, &block);
	if (poolResult != E_OK) {
		result = 0x13;
	} else {
		key.w = (unsigned long *)(unsigned long __near *)block;
		key.nk = 4;
		key.nb = 4;
		aesKeyExpansion((const unsigned long *)handshakeKey, (struct AesKey __near *)&key);
		aesCbcDecrypt((const unsigned char __near *)data, (unsigned char __near *)snvsBuffer,
			      (struct AesKey __near *)&key, handshakeIv, 0x30);
		result = snvsCheck(packet, 0x30);
		if (result == 0) {
			result = nvsWrite(packet->offset, packet->size, snvsBuffer + 0x0e, 0);
			checksum = snvsChecksum(snvsBuffer, 0x0e);
			snvsBuffer[0x0e] = checksum;
			snvsBuffer[0x0f] = checksum >> 8;
			aesCbcEncrypt((const unsigned char __near *)snvsBuffer, (unsigned char __near *)answer,
				      (struct AesKey __near *)&key, handshakeIv, 0x10);
		}
	}
	rel_mpf(2, block);
	return result;
}

/* 0xffff less the sum of length bytes (at most 0x40) */
static unsigned int snvsChecksum(unsigned char *data, unsigned int length)
{
	unsigned int sum;

	if (length > 0x40)
		return 0xffff;
	sum = 0;
	while (length != 0) {
		length--;
		sum += data[length];
	}
	return 0xffff - sum;
}

/* a decrypted packet of length bytes: its sequence number one above the last's (the first
   is taken), and its sum */
static int snvsCheck(struct SnvsPacket *packet, unsigned int length)
{
	unsigned char result;
	unsigned int sum;
	union {
		unsigned int	value;
		unsigned char	byte[2];
	} check;

	result = 0;
	if (snvs.started == 0)
		snvs.started = 1;
	else if (packet->sequence != snvs.sequence + 1)
		return SNVS_BAD_SEQUENCE;
	snvs.sequence = packet->sequence;
	sum = snvsChecksum((unsigned char *)packet, length - 2);
	check.byte[0] = *((unsigned char *)packet + length - 2);
	check.byte[1] = *((unsigned char *)packet + length - 1);
	if (sum != check.value)
		return SNVS_BAD_CHECKSUM;
	return result;
}

/* two handlers nothing calls (no command table entry): each takes a packet and returns 0 */
int snvsReserved0(unsigned char *data)
{
	return 0;
}

int snvsReserved1(unsigned char *data)
{
	return 0;
}
