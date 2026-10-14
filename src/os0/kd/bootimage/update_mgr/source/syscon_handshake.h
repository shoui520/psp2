#ifndef UPDATE_MGR_SYSCON_HANDSHAKE_H
#define UPDATE_MGR_SYSCON_HANDSHAKE_H

#include <stddef.h>
#include <stdint.h>

typedef struct SysconHandshakeData {
	uint32_t words[10];
} SysconHandshakeData;

typedef struct SysconHandshakeRequest {
	uint32_t mode;
	uint32_t protocolExtension;
	SysconHandshakeData input;
	SysconHandshakeData output;
} SysconHandshakeRequest;

_Static_assert(sizeof(SysconHandshakeData) == 40, "Syscon handshake data");
_Static_assert(sizeof(SysconHandshakeRequest) == 88, "Syscon handshake request");
_Static_assert(offsetof(SysconHandshakeRequest, input) == 8,
		"Syscon handshake input offset");
_Static_assert(offsetof(SysconHandshakeRequest, output) == 48,
		"Syscon handshake output offset");

int SysconUtilHandshakeStep(unsigned int phase, int smId,
		const SysconHandshakeData *input, SysconHandshakeData *output);

#endif
