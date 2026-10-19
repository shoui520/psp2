#ifndef SCE_NETPS_DUMP_H
#define SCE_NETPS_DUMP_H

#include <scetypes.h>
#include <stddef.h>

/* Sony's private capture ring: the complete allocation precedes its buffer. */
typedef struct SceNetPsDump {
	char name[32];
	SceUID processId;
	char *buffer;
	int length;
	int bufferedLength;
	int writePosition;
	int readPosition;
	unsigned int flags;
	int waiters;
} SceNetPsDump;

/* Standard PCAP file header emitted when a capture ring is created. */
typedef struct SceNetPsPcapHeader {
	unsigned int magic;
	unsigned short versionMajor;
	unsigned short versionMinor;
	int timeZone;
	unsigned int significantFigures;
	unsigned int snapshotLength;
	unsigned int linkType;
} SceNetPsPcapHeader;

_Static_assert(sizeof(SceNetPsDump) == 64, "capture ring size");
_Static_assert(offsetof(SceNetPsDump, name) == 0, "capture name");
_Static_assert(offsetof(SceNetPsDump, processId) == 32, "capture process");
_Static_assert(offsetof(SceNetPsDump, buffer) == 36, "capture buffer");
_Static_assert(offsetof(SceNetPsDump, length) == 40, "capture capacity");
_Static_assert(offsetof(SceNetPsDump, bufferedLength) == 44, "captured length");
_Static_assert(offsetof(SceNetPsDump, writePosition) == 48, "capture writer");
_Static_assert(offsetof(SceNetPsDump, readPosition) == 52, "capture reader");
_Static_assert(offsetof(SceNetPsDump, flags) == 56, "capture flags");
_Static_assert(offsetof(SceNetPsDump, waiters) == 60, "capture waiters");
_Static_assert(sizeof(SceNetPsPcapHeader) == 24, "PCAP file header size");
_Static_assert(offsetof(SceNetPsPcapHeader, magic) == 0, "PCAP magic");
_Static_assert(offsetof(SceNetPsPcapHeader, versionMajor) == 4, "PCAP major");
_Static_assert(offsetof(SceNetPsPcapHeader, versionMinor) == 6, "PCAP minor");
_Static_assert(offsetof(SceNetPsPcapHeader, timeZone) == 8, "PCAP time zone");
_Static_assert(offsetof(SceNetPsPcapHeader, significantFigures) == 12,
	       "PCAP precision");
_Static_assert(offsetof(SceNetPsPcapHeader, snapshotLength) == 16,
	       "PCAP capture limit");
_Static_assert(offsetof(SceNetPsPcapHeader, linkType) == 20, "PCAP link type");

/* dev/net_ps_dump.c */
int sceNetPsDumpCreate(const char *name, int length, int flags);

/* netps_dump_reader.c */
int sceNetPsDumpRead(int dumpId, void *buffer, int length, int *flags);

#endif
