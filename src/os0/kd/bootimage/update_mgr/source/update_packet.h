#ifndef UPDATE_MGR_PACKET_H
#define UPDATE_MGR_PACKET_H

#include <psp2kern/types.h>
#include <stddef.h>
#include "pup.h"
#include "spkg.h"

typedef struct UpdatePacketPayload {
	uint64_t segment_id;
	uint64_t descriptorFormat;
	SceKernelPAVector vectorVector;
	SceKernelPAVector dataVector;
	union {
		SceKernelPARange descriptors[497];
		struct {
			SceKernelPARange descriptors[481];
			SpkgPackageInfo packageInfo;
			SpkgChunkInfo chunkInfo;
		} returned;
	} records;
} UpdatePacketPayload;

typedef struct UpdateDigestPacketPayload {
	uint64_t descriptorFormat;
	PupHeaderDigest headerDigest;
	SceKernelPAVector vectorVector;
	SceKernelPAVector dataVector;
	SceKernelPARange descriptors[494];
} UpdateDigestPacketPayload;

_Static_assert(sizeof(UpdatePacketPayload) == 4032, "update packet size");
_Static_assert(offsetof(UpdatePacketPayload, records) == 56 &&
	       offsetof(UpdatePacketPayload, records.returned.packageInfo) == 3904 &&
	       offsetof(UpdatePacketPayload, records.returned.chunkInfo) == 3968,
	       "update packet input and output layout");
_Static_assert(sizeof(UpdateDigestPacketPayload) == 4032, "digest packet size");
_Static_assert(offsetof(UpdateDigestPacketPayload, vectorVector) == 40 &&
	       offsetof(UpdateDigestPacketPayload, descriptors) == 80,
	       "digest packet layout");

#endif
