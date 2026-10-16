#ifndef UPDATE_MGR_SPKG_HARDWARE_H
#define UPDATE_MGR_SPKG_HARDWARE_H

#include <stdint.h>

typedef struct SpkgHardwarePolicy {
	uint32_t minimumVersion;
	uint32_t reservedVersion;
	uint32_t supportMask;
} SpkgHardwarePolicy;

typedef struct SpkgHardwareIndexEntry {
	uint16_t minimumProductCode;
	uint16_t maximumProductCode;
	uint16_t productSubCode;
	uint16_t reserved;
	uint32_t hardwareIndex;
} SpkgHardwareIndexEntry;

_Static_assert(sizeof(SpkgHardwarePolicy) == 12, "Hardware policy size");
_Static_assert(sizeof(SpkgHardwareIndexEntry) == 12, "Hardware index entry size");

extern const SpkgHardwarePolicy spkgHardwarePolicies[90];
extern const SpkgHardwareIndexEntry spkgHardwareIndices[89];

#endif
