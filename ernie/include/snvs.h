/*
 * The secure NVS (app/snvs.c): host commands 0x00d0-0x00d3 (sceSysconSnvs*) read and write
 * the NVS in packets AES-CBC encrypted under the session key handshake mode 0x0f leaves in
 * handshakeKey, each with a sequence number one above the last and a checksum.
 */
#ifndef ERNIE_SNVS_H
#define ERNIE_SNVS_H

/* the status of a packet out of sequence, and of one whose checksum is wrong */
#define SNVS_BAD_SEQUENCE	0x14
#define SNVS_BAD_CHECKSUM	0x15

/* a packet: an NVS offset and size, the sequence number, the data, then 0xffff less the sum
   of the bytes before */
struct SnvsPacket {
	unsigned int	offset;
	unsigned int	size;
	unsigned long	sequence;
};

/* the last packet's sequence number */
struct Snvs {
	unsigned long	sequence;
	unsigned char	started;		/* a packet was taken since snvsReset */
};

extern void snvsReset(void);
extern int snvsUnlocked(void);
extern int snvsHandshake(unsigned char *data);
extern int snvsLock(unsigned char *data);
extern int snvsRead(unsigned char *data, unsigned char *answer);
extern int snvsWrite(unsigned char *data, unsigned char *answer);
extern int snvsReserved0(unsigned char *data);
extern int snvsReserved1(unsigned char *data);

#endif
