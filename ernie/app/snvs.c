/*
 * The secure NVS: the host first runs handshake mode 0x0f through command 0x00d0, then reads
 * (0x00d2) and writes (0x00d3) the NVS with packets encrypted under its session key; 0x00d1
 * locks it again. snvsCheck keeps the packets in sequence and checks their sums.
 */
#include "handshake.h"
#include "kernel.h"
#include "snvs.h"

static struct Snvs snvs;

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

/*
 * PLACEHOLDER: cryptographic routine, deferred (0x00d2, sceSysconSnvsReadData: takes a block
 * of memory pool 2 (status 0x13 without one), AES-CBC decrypts the host's 0x10-byte packet
 * from data into the buffer at 0xffcaa under handshakeKey (0xff6f6) with the IV at 0x10588,
 * checks it with snvsCheck, reads the packet's NVS offset and size into it at 0x0e, puts
 * snvsChecksum of its first 0x2e bytes at 0x2e, and AES-CBC encrypts the 0x30 bytes into
 * answer; then frees the block and returns the status). Returns 0.
 */
int snvsRead(unsigned char *data, unsigned char *answer)
{
	return 0;
}

/*
 * PLACEHOLDER: cryptographic routine, deferred (0x00d3, sceSysconSnvsWriteData: as snvsRead,
 * the host's 0x30-byte packet decrypted and checked, its 0x20 bytes at 0x0e written to its NVS
 * offset and size, snvsChecksum of the first 0x0e bytes put at 0x0e, and the first 0x10 bytes
 * encrypted into answer). Returns 0.
 */
int snvsWrite(unsigned char *data, unsigned char *answer)
{
	return 0;
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
