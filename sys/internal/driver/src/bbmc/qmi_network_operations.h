#pragma once

#include "qmi_backend.h"

/* Complete option format consumed by the optional Start Network request. */
typedef struct BbmcNetworkStartOptions {
	SceUInt8 technologyPreference;
	SceUInt8 profileIndex;
	SceUInt8 authenticationPreference;
	SceUInt8 reserved;
	SceUInt32 primaryDnsAddress;
	SceUInt32 secondaryDnsAddress;
	SceUInt32 primaryNbnsAddress;
	SceUInt32 secondaryNbnsAddress;
	char apn[100];
	SceUInt32 apnLength;
	SceUInt32 ipv4Address;
	SceUInt8 reservedUsername[32];
	SceUInt32 usernameLength;
	SceUInt8 reservedPassword[16];
	SceUInt32 passwordLength;
} BbmcNetworkStartOptions;

typedef struct QmiWdsStatisticsTlv {
	QmiTlvHeader header;
	SceUInt8 period;
	SceUInt8 mask[4];
} QmiWdsStatisticsTlv;

_Static_assert(sizeof(QmiWdsStatisticsTlv) == 8, "WDS statistics report option");

_Static_assert(sizeof(BbmcNetworkStartOptions) == 184, "network start option format");
_Static_assert(offsetof(BbmcNetworkStartOptions, apn) == 20, "network start APN");
_Static_assert(offsetof(BbmcNetworkStartOptions, apnLength) == 120,
	"network start APN length");
_Static_assert(offsetof(BbmcNetworkStartOptions, ipv4Address) == 124,
	"network start IPv4 address preference");
_Static_assert(offsetof(BbmcNetworkStartOptions, reservedUsername) == 128,
	"network start ignored username span");
_Static_assert(offsetof(BbmcNetworkStartOptions, usernameLength) == 160,
	"network start username length");
_Static_assert(offsetof(BbmcNetworkStartOptions, reservedPassword) == 164,
	"network start ignored password span");
_Static_assert(offsetof(BbmcNetworkStartOptions, passwordLength) == 180,
	"network start password length");

/* qmi_network_operations.c */
int bbcmdStartNetwork(BbcmdRequest *request);
