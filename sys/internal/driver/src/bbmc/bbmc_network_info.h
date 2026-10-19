#ifndef BBMC_NETWORK_INFO_H
#define BBMC_NETWORK_INFO_H

#include <stddef.h>
#include <scetypes.h>

typedef struct BbmcRoamingIndicator {
	SceUInt32 radioInterface;
	SceUInt32 roamingIndicator;
} BbmcRoamingIndicator;

typedef struct BbmcServingSystemInfo {
	SceUInt8 validFlags;
	SceUInt32 registrationState;
	SceUInt32 csAttachState;
	SceUInt32 psAttachState;
	SceUInt32 selectedNetwork;
	SceUInt32 radioInterfaces[8];
	SceUInt32 roamingIndicator;
	SceUInt32 dataCapabilities[8];
	SceUInt16 mcc;
	SceUInt16 mnc;
	SceUInt8 description[68];
	SceUInt8 descriptionLength;
	SceUInt8 radioInterfaceCount;
	SceUInt8 dataCapabilityCount;
	SceUInt16 cdmaSystemId;
	SceUInt16 cdmaNetworkId;
	SceUInt16 baseStationId;
	SceInt32 baseStationLatitude;
	SceInt32 baseStationLongitude;
	SceUInt8 roamingIndicatorCount;
	SceUInt8 leapSeconds;
	SceInt8 localTimeOffset;
	SceUInt8 cdmaProtocolRevision;
	BbmcRoamingIndicator roamingIndicators[8];
	SceUInt32 defaultRoamingIndicator;
	SceUInt32 daylightSavingTime;
} BbmcServingSystemInfo;

typedef struct BbmcSignalStrength {
	SceInt32 level;
	SceUInt32 radioInterface;
	SceInt8 strength;
} BbmcSignalStrength;

typedef struct BbmcRadioSignalValue {
	SceUInt32 radioInterface;
	SceUInt8 value;
} BbmcRadioSignalValue;

typedef struct BbmcSignalStrengthInfo {
	SceUInt32 validMask;
	SceUInt32 radioInterface;
	SceInt8 strength;
	/* Only whole-record zero/copy operations touch these three bytes. */
	SceUInt8 reserved[3];
	SceUInt16 strengthCount;
	BbmcSignalStrength strengths[4];
	SceUInt16 rssiCount;
	BbmcSignalStrength rssi[4];
	SceUInt16 ecioCount;
	BbmcRadioSignalValue ecio[4];
	SceUInt16 errorRateCount;
	BbmcRadioSignalValue errorRates[4];
	SceInt32 io;
	SceUInt8 sinr;
	SceInt8 rsrq;
} BbmcSignalStrengthInfo;

typedef struct BbmcNetworkInfo {
	SceInt32 signalLevel;
	BbmcServingSystemInfo serving;
	BbmcSignalStrengthInfo signal;
} BbmcNetworkInfo;

_Static_assert(sizeof(BbmcRoamingIndicator) == 8, "roaming indicator size");
_Static_assert(sizeof(BbmcServingSystemInfo) == 256, "serving system size");
_Static_assert(offsetof(BbmcServingSystemInfo, descriptionLength) == 160,
	"serving system description length");
_Static_assert(offsetof(BbmcServingSystemInfo, baseStationLatitude) == 172,
	"serving system base station latitude");
_Static_assert(offsetof(BbmcServingSystemInfo, roamingIndicators) == 184,
	"serving system roaming indicators");
_Static_assert(sizeof(BbmcSignalStrength) == 12, "signal strength size");
_Static_assert(sizeof(BbmcRadioSignalValue) == 8, "radio signal value size");
_Static_assert(sizeof(BbmcSignalStrengthInfo) == 196, "signal strength info size");
_Static_assert(offsetof(BbmcSignalStrengthInfo, rssi) == 68,
	"signal strength RSSI list");
_Static_assert(offsetof(BbmcSignalStrengthInfo, ecio) == 120,
	"signal strength ECIO list");
_Static_assert(offsetof(BbmcSignalStrengthInfo, errorRates) == 156,
	"signal strength error rate list");

_Static_assert(sizeof(BbmcNetworkInfo) == 456, "network info size");
_Static_assert(offsetof(BbmcNetworkInfo, serving) == 4, "network serving system");
_Static_assert(offsetof(BbmcNetworkInfo, signal) == 260, "network signal strength");

#endif
