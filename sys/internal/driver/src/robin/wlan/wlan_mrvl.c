/* Marvell command family; filename attested by Sony's prototype debug records. */
#include "../../wlanbt/wlanbt_internal.h"
#include <net/ether.h>
#include <string.h>

typedef struct SceWlanBtRobinWlanMulticastRequest {
	uint8_t action[2];
	uint8_t count[2];
	SceNetEtherAddr addresses[32];
} SceWlanBtRobinWlanMulticastRequest;

typedef struct SceWlanBtRobinWlanAdhocResult {
	uint8_t padding[3];
	SceNetEtherAddr bssid;
} SceWlanBtRobinWlanAdhocResult;

typedef struct WlanIbssParameter {
	uint8_t id;
	uint8_t length;
	uint8_t atimWindow[2];
} WlanIbssParameter;

typedef struct WlanCfParameter {
	uint8_t id;
	uint8_t length;
	uint8_t count;
	uint8_t period;
	uint8_t maxDuration[2];
	uint8_t remainingDuration[2];
} WlanCfParameter;

typedef union WlanSsParameters {
	WlanIbssParameter ibss;
	WlanCfParameter contentionFree;
} WlanSsParameters;

typedef struct WlanFhParameter {
	uint8_t id;
	uint8_t length;
	uint8_t dwellTime[2];
	uint8_t hopSet;
	uint8_t hopPattern;
	uint8_t hopIndex;
} WlanFhParameter;

typedef struct WlanDsParameter {
	uint8_t id;
	uint8_t length;
	uint8_t channel;
} WlanDsParameter;

typedef union WlanPhyParameters {
	WlanFhParameter frequencyHopping;
	WlanDsParameter directSequence;
} WlanPhyParameters;

enum { ROBIN_ADHOC_RATE_CAPACITY = 14 };

typedef struct WlanAdhocStartRequest {
	uint8_t ssid[32];
	uint8_t bssType;
	uint8_t beaconInterval[2];
	uint8_t dtimPeriod;
	WlanSsParameters ssParameters;
	WlanPhyParameters phyParameters;
	uint8_t reserved[2];
	uint8_t capabilities[2];
	/* The original copies rates into this area before appending TLVs at slot 14. */
	uint8_t ratesAndParameters[ROBIN_ADHOC_RATE_CAPACITY + 512];
} WlanAdhocStartRequest;

typedef struct WlanAdhocJoinRequest {
	SceNetEtherAddr bssid;
	uint8_t ssid[32];
	uint8_t bssType;
	uint8_t beaconInterval[2];
	uint8_t dtimPeriod;
	uint8_t timestamp[8];
	uint8_t localTime[8];
	WlanPhyParameters phyParameters;
	WlanSsParameters ssParameters;
	uint8_t capabilities[2];
	/* Four reserved bytes separate the fourteen rate slots from the TLVs. */
	uint8_t ratesAndParameters[ROBIN_ADHOC_RATE_CAPACITY + 4 + 125];
} WlanAdhocJoinRequest;

_Static_assert(sizeof(SceWlanBtRobinWlanAdhocResult) == 9, "ad-hoc result size");
_Static_assert(offsetof(SceWlanBtRobinWlanAdhocResult, bssid) == 3, "ad-hoc BSSID");
_Static_assert(sizeof(WlanSsParameters) == 8, "SS parameter union");
_Static_assert(sizeof(WlanPhyParameters) == 7, "PHY parameter union");
_Static_assert(sizeof(WlanAdhocStartRequest) == 581, "ad-hoc start capacity");
_Static_assert(offsetof(WlanAdhocStartRequest, ssParameters) == 36, "ad-hoc SS parameters");
_Static_assert(offsetof(WlanAdhocStartRequest, phyParameters) == 44, "ad-hoc PHY parameters");
_Static_assert(offsetof(WlanAdhocStartRequest, ratesAndParameters) == 55, "ad-hoc rates");
_Static_assert(sizeof(WlanAdhocJoinRequest) == 218, "ad-hoc join capacity");
_Static_assert(offsetof(WlanAdhocJoinRequest, phyParameters) == 58, "join PHY parameters");
_Static_assert(offsetof(WlanAdhocJoinRequest, ssParameters) == 65, "join SS parameters");
_Static_assert(offsetof(WlanAdhocJoinRequest, ratesAndParameters) == 75, "join rates");

typedef struct SceWlanBtRobinWlanSettingRequest {
	uint8_t action[2];
	uint8_t settingId[2];
	uint8_t size[2];
	uint8_t data[128];
} SceWlanBtRobinWlanSettingRequest;

typedef struct SceWlanBtRobinWlanControlRequest {
	uint8_t control[2];
	uint8_t reserved[2];
} SceWlanBtRobinWlanControlRequest;

typedef struct SceWlanBtRobinWlanMacAddressMessage {
	uint8_t action[2];
	SceNetEtherAddr address;
} SceWlanBtRobinWlanMacAddressMessage;

typedef struct SceWlanBtRobinWlanEventRequest {
	uint8_t action[2];
	uint8_t events[2];
	uint8_t parameters[64];
} SceWlanBtRobinWlanEventRequest;

typedef struct SceWlanBtRobinWlanEventParameter {
	uint8_t type[2];
	uint8_t size[2];
	uint8_t value[2];
} SceWlanBtRobinWlanEventParameter;

typedef struct SceWlanBtRobinWlanIbssCoalescingMessage {
	uint8_t action[2];
	uint8_t enabled[2];
	SceNetEtherAddr bssid;
	uint8_t beaconInterval[2];
	uint8_t atimWindow[2];
	uint8_t useGRateProtection[2];
} SceWlanBtRobinWlanIbssCoalescingMessage;

typedef struct SceWlanBtRobinWlanMemoryMessage {
	uint8_t action[2];
	uint8_t reserved[2];
	uint8_t address[4];
	uint8_t value[4];
} SceWlanBtRobinWlanMemoryMessage;

typedef struct SceWlanBtRobinWlanInactivityTimeoutRequest {
	uint8_t action[2];
	uint8_t timeoutUnit[2];
	uint8_t unicastTimeout[2];
	uint8_t multicastTimeout[2];
	uint8_t powerSaveEntryTimeout[2];
	uint8_t reserved[2];
} SceWlanBtRobinWlanInactivityTimeoutRequest;

typedef struct SceWlanBtRobinWlanTlvHeader {
	uint8_t type[2];
	uint8_t size[2];
} SceWlanBtRobinWlanTlvHeader;

typedef struct SceWlanBtRobinWlanWepKeyParameter {
	SceWlanBtRobinWlanTlvHeader header;
	uint8_t keyType[2];
	uint8_t keyFlags[2];
	uint8_t keyLength[2];
	uint8_t keyIndex;
	uint8_t isDefault;
	uint8_t key[];
} SceWlanBtRobinWlanWepKeyParameter;

typedef struct SceWlanBtRobinWlanWepKeyRequest {
	uint8_t action[2];
	uint8_t parameters[128];
} SceWlanBtRobinWlanWepKeyRequest;

_Static_assert(sizeof(SceWlanBtRobinWlanWepKeyParameter) == 12, "WEP key parameter");
_Static_assert(sizeof(SceWlanBtRobinWlanWepKeyRequest) == 130, "WEP key request");

typedef struct SceWlanBtRobinWlanPasswordKeyRequest {
	uint8_t action[2];
	uint8_t cacheResult[2];
	uint8_t parameters[256];
} SceWlanBtRobinWlanPasswordKeyRequest;

typedef struct SceWlanBtRobinWlanPasswordKeyParameter {
	SceWlanBtRobinWlanTlvHeader header;
	uint8_t key[32];
} SceWlanBtRobinWlanPasswordKeyParameter;

_Static_assert(sizeof(SceWlanBtRobinWlanPasswordKeyRequest) == 260,
	       "password key request");
_Static_assert(sizeof(SceWlanBtRobinWlanPasswordKeyParameter) == 36,
	       "password key parameter");

typedef struct WlanAssociationRequest {
	SceNetEtherAddr peer;
	uint8_t capabilities[2];
	uint8_t listenInterval[2];
	uint8_t beaconInterval[2];
	uint8_t dtimPeriod;
	uint8_t parameters[499];
} WlanAssociationRequest;

typedef struct WlanAssociationResponse {
	uint8_t capabilities[2];
	uint8_t status[2];
	uint8_t associationId[2];
	uint8_t informationElements[];
} WlanAssociationResponse;

typedef struct WlanChannelParameter {
	SceWlanBtRobinWlanTlvHeader header;
	uint8_t channel;
} WlanChannelParameter;

typedef struct WlanContentionFreeParameter {
	SceWlanBtRobinWlanTlvHeader header;
	uint8_t count;
	uint8_t period;
	uint8_t maxDuration[2];
	uint8_t remainingDuration[2];
} WlanContentionFreeParameter;

typedef struct WlanAuthenticationParameter {
	SceWlanBtRobinWlanTlvHeader header;
	uint8_t authenticationMode[2];
} WlanAuthenticationParameter;

typedef struct WlanAssociationParameter {
	SceWlanBtRobinWlanTlvHeader header;
	uint8_t data[];
} WlanAssociationParameter;

typedef struct WlanWmmInformation {
	uint8_t oui[3];
	uint8_t ouiType;
	uint8_t ouiSubtype;
	uint8_t version;
	uint8_t qosInfo;
} WlanWmmInformation;

typedef struct WlanWmmParameter {
	SceWlanBtRobinWlanTlvHeader header;
	WlanWmmInformation information;
} WlanWmmParameter;

typedef struct WlanHtCapabilitiesParameter {
	SceWlanBtRobinWlanTlvHeader header;
	uint8_t capabilities[2];
	uint8_t ampduParameters;
	uint8_t supportedMcs[16];
	uint8_t extendedCapabilities[2];
	uint8_t transmitBeamforming[4];
	uint8_t antennaSelection;
} WlanHtCapabilitiesParameter;

typedef struct WlanHtOperationParameter {
	SceWlanBtRobinWlanTlvHeader header;
	uint8_t channel;
	uint8_t htParameters;
	uint8_t operationMode[2];
	uint8_t stbcParameters[2];
	uint8_t basicMcs[16];
} WlanHtOperationParameter;

_Static_assert(sizeof(WlanHtOperationParameter) == 26, "HT operation parameter");

_Static_assert(sizeof(WlanAssociationRequest) == 512, "association request capacity");
_Static_assert(offsetof(WlanAssociationRequest, parameters) == 13, "association TLVs");
_Static_assert(sizeof(WlanAssociationResponse) == 6, "association response header");
_Static_assert(sizeof(WlanChannelParameter) == 5, "DS channel parameter");
_Static_assert(sizeof(WlanContentionFreeParameter) == 10, "CF parameter");
_Static_assert(sizeof(WlanAuthenticationParameter) == 6, "authentication parameter");
_Static_assert(sizeof(WlanWmmInformation) == 7, "WMM information payload");
_Static_assert(sizeof(WlanWmmParameter) == 11, "WMM parameter");
_Static_assert(sizeof(WlanHtCapabilitiesParameter) == 30, "HT capabilities parameter");

typedef struct SceWlanBtRobinWlanScanSsidParameter {
	SceWlanBtRobinWlanTlvHeader header;
	uint8_t ssid[];
} SceWlanBtRobinWlanScanSsidParameter;

typedef struct SceWlanBtRobinWlanWildcardSsidParameter {
	SceWlanBtRobinWlanTlvHeader header;
	uint8_t maxLength;
	uint8_t ssid[];
} SceWlanBtRobinWlanWildcardSsidParameter;

typedef struct SceWlanBtRobinWlanScanChannel {
	uint8_t radio;
	uint8_t channel;
	uint8_t scanMode;
	uint8_t minScanTime[2];
	uint8_t maxScanTime[2];
} SceWlanBtRobinWlanScanChannel;

typedef struct SceWlanBtRobinWlanScanChannelParameter {
	SceWlanBtRobinWlanTlvHeader header;
	SceWlanBtRobinWlanScanChannel channels[];
} SceWlanBtRobinWlanScanChannelParameter;

typedef struct SceWlanBtRobinWlanScanStartParameter {
	SceWlanBtRobinWlanTlvHeader header;
	uint8_t startLater[2];
} SceWlanBtRobinWlanScanStartParameter;

typedef struct WlanScanRequest {
	uint8_t bssType;
	SceNetEtherAddr bssid;
	uint8_t parameters[];
} WlanScanRequest;

typedef struct WlanScanResponse {
	uint8_t bssDescriptorSize[2];
	uint8_t bssCount;
	uint8_t records[];
} WlanScanResponse;

typedef struct WlanScanRatesParameter {
	SceWlanBtRobinWlanTlvHeader header;
	uint8_t rates[12];
} WlanScanRatesParameter;

typedef struct WlanScanProbeCountParameter {
	SceWlanBtRobinWlanTlvHeader header;
	uint8_t count[2];
} WlanScanProbeCountParameter;

typedef struct WlanScanInformationElementsParameter {
	SceWlanBtRobinWlanTlvHeader header;
	uint8_t informationElements[];
} WlanScanInformationElementsParameter;

_Static_assert(sizeof(WlanScanRequest) == 7, "scan request header");
_Static_assert(sizeof(WlanScanResponse) == 3, "scan response header");
_Static_assert(sizeof(WlanScanRatesParameter) == 16, "scan rates parameter");
_Static_assert(sizeof(WlanScanProbeCountParameter) == 6, "scan probe count parameter");
_Static_assert(sizeof(WlanScanInformationElementsParameter) == 4, "scan IE parameter");

typedef struct SceWlanBtRobinWlanBackgroundScanRequest {
	uint8_t action[2];
	uint8_t enabled;
	uint8_t bssType;
	uint8_t channelsPerScan;
	uint8_t reservedControl[3];
	uint8_t scanInterval[4];
	uint8_t reservedInterval[4];
	uint8_t reportCondition[4];
	uint8_t reservedReport[2];
	uint8_t parameters[256];
} SceWlanBtRobinWlanBackgroundScanRequest;

typedef struct SceWlanBtRobinWlanSdioPullControlRequest {
	uint8_t action[2];
	uint8_t pullUpDelay[2];
	uint8_t pullDownDelay[2];
	/* SCE adds two control bytes to the six-byte protocol request. */
	uint8_t extendedControls[2];
} SceWlanBtRobinWlanSdioPullControlRequest;

enum {
	ROBIN_MEF_NUMBER = 1,
	ROBIN_MEF_BYTE_SEQUENCE = 2,
	ROBIN_MEF_BYTE_EQUAL = 65,
};

/* Reverse-Polish expression: compare sixteen MAC repetitions at packetOffset. */
typedef struct SceWlanBtRobinWlanMacFilterExpression {
	uint8_t repeat[4];
	uint8_t repeatType;
	SceNetEtherAddr address;
	uint8_t addressSize;
	uint8_t addressType;
	uint8_t packetOffset[4];
	uint8_t offsetType;
	uint8_t comparison;
} SceWlanBtRobinWlanMacFilterExpression;

typedef struct SceWlanBtRobinWlanMacFilterRequest {
	uint8_t criteria[4];
	uint8_t entryCount[2];
	uint8_t mode;
	uint8_t action;
	uint8_t expressionSize[2];
	SceWlanBtRobinWlanMacFilterExpression expression;
} SceWlanBtRobinWlanMacFilterRequest;

typedef struct SceWlanBtRobinWlanRssiRequest {
	uint8_t action[2];
	uint8_t dataAveraging[2];
	uint8_t beaconAveraging[2];
	uint8_t reserved[18];
	uint8_t reservedTimestamp[8];
} SceWlanBtRobinWlanRssiRequest;

/* RSSI_INFO response wire fields, all little endian. */
typedef struct SceWlanBtRobinWlanRssiResponse {
	uint8_t action[2];
	uint8_t dataAveraging[2];
	uint8_t beaconAveraging[2];
	uint8_t lastDataRssi[2];
	uint8_t lastDataNoise[2];
	uint8_t averageDataRssi[2];
	uint8_t averageDataNoise[2];
	uint8_t lastBeaconRssi[2];
	uint8_t lastBeaconNoise[2];
	uint8_t averageBeaconRssi[2];
	uint8_t averageBeaconNoise[2];
	uint8_t beaconTimestamp[8];
} SceWlanBtRobinWlanRssiResponse;

_Static_assert(sizeof(SceWlanBtRobinWlanRssiResponse) == 30, "RSSI response size");
_Static_assert(offsetof(SceWlanBtRobinWlanRssiResponse, averageBeaconRssi) == 18,
	       "average beacon signal strength");
_Static_assert(offsetof(SceWlanBtRobinWlanRssiResponse, beaconTimestamp) == 22,
	       "beacon TSF timestamp");

typedef struct SceWlanBtRobinWlanAddBlockAckMessage {
	uint8_t result;
	SceNetEtherAddr peer;
	uint8_t dialogToken;
	uint8_t parameters[2];
	uint8_t timeout[2];
	uint8_t startingSequence[2];
} SceWlanBtRobinWlanAddBlockAckMessage;

typedef struct SceWlanBtRobinWlanHtTransmitRequest {
	uint8_t action[2];
	uint8_t capabilities[2];
	uint8_t information[2];
} SceWlanBtRobinWlanHtTransmitRequest;

typedef struct SceWlanBtRobinWlanDeleteBlockAckMessage {
	uint8_t result;
	SceNetEtherAddr peer;
	uint8_t parameters[2];
	uint8_t reason[2];
	uint8_t reserved;
} SceWlanBtRobinWlanDeleteBlockAckMessage;

typedef struct SceWlanBtRobinWlanTransmitBufferRequest {
	uint8_t action[2];
	uint8_t size[2];
	uint8_t endPort[2];
	uint8_t reserved[2];
} SceWlanBtRobinWlanTransmitBufferRequest;

typedef struct SceWlanBtRobinWlanAmsduRequest {
	uint8_t action[2];
	uint8_t enabled[2];
	uint8_t currentBufferSize[2];
} SceWlanBtRobinWlanAmsduRequest;

typedef struct SceWlanBtRobinWlanCoexistenceParameter {
	SceWlanBtRobinWlanTlvHeader header;
	uint8_t mode[4];
} SceWlanBtRobinWlanCoexistenceParameter;

typedef struct SceWlanBtRobinWlanCoexistenceRequest {
	uint8_t action[2];
	uint8_t reserved[2];
	uint8_t parameters[32];
} SceWlanBtRobinWlanCoexistenceRequest;

typedef struct SceWlanBtRobinWlanHtRateBitmap {
	uint16_t rates[8];
} SceWlanBtRobinWlanHtRateBitmap;

typedef struct SceWlanBtRobinWlanRateScopeParameter {
	SceWlanBtRobinWlanTlvHeader header;
	uint8_t dsssRates[2];
	uint8_t ofdmRates[2];
	SceWlanBtRobinWlanHtRateBitmap htRates;
} SceWlanBtRobinWlanRateScopeParameter;

typedef struct SceWlanBtRobinWlanRateDropParameter {
	SceWlanBtRobinWlanTlvHeader header;
	uint8_t mode[4];
} SceWlanBtRobinWlanRateDropParameter;

typedef struct SceWlanBtRobinWlanRateOptionParameter {
	SceWlanBtRobinWlanTlvHeader header;
	uint8_t value[2];
} SceWlanBtRobinWlanRateOptionParameter;

typedef struct SceWlanBtRobinWlanTransmitRateRequest {
	uint8_t action[2];
	uint8_t configurationIndex[2];
	uint8_t parameters[2044];
} SceWlanBtRobinWlanTransmitRateRequest;

typedef struct SceWlanBtRobinWlanDeepSleepParameter {
	SceWlanBtRobinWlanTlvHeader header;
	uint8_t timeout[2];
} SceWlanBtRobinWlanDeepSleepParameter;

typedef struct SceWlanBtRobinWlanPowerSaveRequest {
	uint8_t action[2];
	uint8_t powerSaveBitmap[2];
} SceWlanBtRobinWlanPowerSaveRequest;

typedef struct SceWlanBtRobinWlanAutoDeepSleepRequest {
	SceWlanBtRobinWlanPowerSaveRequest powerSave;
	SceWlanBtRobinWlanDeepSleepParameter parameter;
} SceWlanBtRobinWlanAutoDeepSleepRequest;

typedef struct SceWlanBtRobinWlanStationPowerSaveParameter {
	SceWlanBtRobinWlanTlvHeader header;
	uint8_t nullPacketInterval[2];
	uint8_t multipleDtims[2];
	uint8_t beaconMissTimeout[2];
	uint8_t localListenInterval[2];
	uint8_t adhocWakePeriod[2];
	uint8_t mode[2];
	uint8_t delayToPowerSave[2];
} SceWlanBtRobinWlanStationPowerSaveParameter;

typedef struct SceWlanBtRobinWlanEnableStationPowerSaveRequest {
	SceWlanBtRobinWlanPowerSaveRequest powerSave;
	SceWlanBtRobinWlanStationPowerSaveParameter parameter;
} SceWlanBtRobinWlanEnableStationPowerSaveRequest;

typedef struct SceWlanBtRobinWlanSleepConfirmRequest {
	uint8_t action[2];
	uint8_t responseControl[2];
} SceWlanBtRobinWlanSleepConfirmRequest;

typedef struct SceWlanBtRobinWlanHostSleepRequest {
	uint8_t action[2];
	uint8_t conditions[4];
	uint8_t gpio;
	uint8_t gap;
} SceWlanBtRobinWlanHostSleepRequest;

typedef struct SceWlanBtRobinWlanCustomInformationElement {
	uint8_t index[2];
	uint8_t managementSubtypeMask[2];
	uint8_t size[2];
	uint8_t data[256];
} SceWlanBtRobinWlanCustomInformationElement;

typedef struct SceWlanBtRobinWlanManagementInformationElementRequest {
	uint8_t action[2];
	SceWlanBtRobinWlanTlvHeader header;
	SceWlanBtRobinWlanCustomInformationElement element;
} SceWlanBtRobinWlanManagementInformationElementRequest;

typedef struct SceWlanBtRobinWlanHostSleepDataRequest {
	uint8_t action[2];
	SceWlanBtRobinWlanTlvHeader header;
	uint8_t size[2];
	uint8_t data[10];
} SceWlanBtRobinWlanHostSleepDataRequest;

typedef struct SceWlanBtRobinWlanHostSleepAddressDataRequest {
	uint8_t action[2];
	SceWlanBtRobinWlanTlvHeader header;
	SceNetEtherAddr address;
	uint8_t data[257];
} SceWlanBtRobinWlanHostSleepAddressDataRequest;

typedef struct SceWlanBtRobinWlanHostSleepAddressDataControlRequest {
	uint8_t action[2];
	SceWlanBtRobinWlanTlvHeader header;
} SceWlanBtRobinWlanHostSleepAddressDataControlRequest;

typedef struct SceWlanBtRobinWlanHostSleepAddressQueryRequest {
	SceWlanBtRobinWlanHostSleepAddressDataControlRequest control;
	SceNetEtherAddr address;
} SceWlanBtRobinWlanHostSleepAddressQueryRequest;

/* Responses share the command's complete six-byte control header. */
typedef struct WlanHostSleepPeerResponse {
	SceWlanBtRobinWlanHostSleepAddressDataControlRequest control;
	uint8_t records[];
} WlanHostSleepPeerResponse;

typedef struct WlanHostSleepPeerDataResponse {
	SceWlanBtRobinWlanHostSleepAddressQueryRequest query;
	uint8_t data[];
} WlanHostSleepPeerDataResponse;

_Static_assert(sizeof(WlanHostSleepPeerResponse) == 6, "host-sleep response prefix");
_Static_assert(offsetof(WlanHostSleepPeerResponse, records) == 6,
	       "host-sleep response TLVs");
_Static_assert(sizeof(WlanHostSleepPeerDataResponse) == 12, "peer data response prefix");
_Static_assert(offsetof(WlanHostSleepPeerDataResponse, data) == 12,
	       "host-sleep peer response data");

typedef struct SceWlanBtRobinWlanEnergyDetectionMacRequest {
	uint8_t enabled[2];
	uint8_t offset[2];
} SceWlanBtRobinWlanEnergyDetectionMacRequest;

typedef struct SceWlanBtRobinWlanAddBlockAckResponse {
	uint8_t result;
	SceNetEtherAddr peer;
	uint8_t dialogToken;
	uint8_t status[2];
	uint8_t parameters[2];
	uint8_t timeout[2];
	uint8_t startingSequence[2];
} SceWlanBtRobinWlanAddBlockAckResponse;

_Static_assert(sizeof(SceWlanBtRobinWlanAddBlockAckResponse) == 16,
	"Robin add block acknowledgement response");

_Static_assert(sizeof(SceWlanBtRobinWlanEnergyDetectionMacRequest) == 4,
	"Robin energy detection MAC request");

_Static_assert(sizeof(SceWlanBtRobinWlanHostSleepAddressQueryRequest) == 12,
	"Robin host sleep address query request");

_Static_assert(sizeof(SceWlanBtRobinWlanHostSleepAddressDataControlRequest) == 6,
	"Robin clear host sleep address data request");

_Static_assert(sizeof(SceWlanBtRobinWlanHostSleepAddressDataRequest) == 269,
	"Robin host sleep address data request");

_Static_assert(sizeof(SceWlanBtRobinWlanHostSleepDataRequest) == 18,
	"Robin host sleep data request");

_Static_assert(sizeof(SceWlanBtRobinWlanCustomInformationElement) == 262,
	"Robin custom information element");
_Static_assert(sizeof(SceWlanBtRobinWlanManagementInformationElementRequest) == 268,
	"Robin management information element request");

_Static_assert(sizeof(SceWlanBtRobinWlanHostSleepRequest) == 8,
	       "Robin host sleep request");

_Static_assert(sizeof(SceWlanBtRobinWlanSleepConfirmRequest) == 4,
	       "Robin sleep confirmation request");

_Static_assert(sizeof(SceWlanBtRobinWlanStationPowerSaveParameter) == 18,
	       "Robin station power save parameter");
_Static_assert(sizeof(SceWlanBtRobinWlanEnableStationPowerSaveRequest) == 22,
	       "Robin enable station power save request");

_Static_assert(sizeof(SceWlanBtRobinWlanPowerSaveRequest) == 4,
	       "Robin power save request");
_Static_assert(sizeof(SceWlanBtRobinWlanDeepSleepParameter) == 6,
	       "Robin deep sleep parameter");
_Static_assert(sizeof(SceWlanBtRobinWlanAutoDeepSleepRequest) == 10,
	       "Robin automatic deep sleep request");

_Static_assert(sizeof(SceWlanBtRobinWlanHtRateBitmap) == 16, "Robin HT rate bitmap");
_Static_assert(sizeof(SceWlanBtRobinWlanRateScopeParameter) == 24, "Robin rate scope");
_Static_assert(sizeof(SceWlanBtRobinWlanRateDropParameter) == 8, "Robin rate drop");
_Static_assert(sizeof(SceWlanBtRobinWlanRateOptionParameter) == 6, "Robin rate option");
_Static_assert(sizeof(SceWlanBtRobinWlanTransmitRateRequest) == 2048,
	       "Robin transmit rate request");

_Static_assert(sizeof(SceWlanBtRobinWlanCoexistenceParameter) == 8,
	       "Robin coexistence parameter");
_Static_assert(sizeof(SceWlanBtRobinWlanCoexistenceRequest) == 36,
	       "Robin coexistence request");

_Static_assert(sizeof(SceWlanBtRobinWlanAmsduRequest) == 6, "Robin AMSDU request");

_Static_assert(sizeof(SceWlanBtRobinWlanTransmitBufferRequest) == 8,
	       "Robin transmit buffer request");

_Static_assert(sizeof(SceWlanBtRobinWlanDeleteBlockAckMessage) == 12,
	       "Robin delete block acknowledgement message");

_Static_assert(sizeof(SceWlanBtRobinWlanHtTransmitRequest) == 6,
	       "Robin HT transmit request");

_Static_assert(sizeof(SceWlanBtRobinWlanAddBlockAckMessage) == 14,
	       "Robin add block acknowledgement message");

uint8_t gBlockAckDialogToken;

_Static_assert(sizeof(SceWlanBtRobinWlanRssiRequest) == 32,
	       "Robin RSSI request");

const SceWlanBtRobinWlanMacFilterExpression gMacFilterExpression = {
	.repeat = { 16, 0, 0, 0 },
	.repeatType = ROBIN_MEF_NUMBER,
	.address = { { 0 } },
	.addressSize = sizeof(SceNetEtherAddr),
	.addressType = ROBIN_MEF_BYTE_SEQUENCE,
	.packetOffset = { 0, 0, 0, 0 },
	.offsetType = ROBIN_MEF_NUMBER,
	.comparison = ROBIN_MEF_BYTE_EQUAL,
};

const uint8_t gWlanSupportedMcs[16] = { 0xff };
/* Rates use units of 500 kbit/s; bit 7 identifies a basic rate. */
const uint8_t gWlanAdhocBasicRates[8] = {
	0x80 | 2, 0x80 | 4, 0x80 | 11, 0x80 | 22, 12, 18, 24, 36,
};
const WlanWmmInformation gWlanWmmInformation = {
	.oui = { 0x00, 0x50, 0xf2 },
	.ouiType = 2,
	.ouiSubtype = 0,
	.version = 1,
	.qosInfo = 0,
};
const uint8_t gWlanAdhocBasicMcs[16] = { 0xff };
const uint8_t gWlanAdhocHighRates[4] = { 48, 72, 96, 108 };
/* Scan rates in units of 500 kbit/s. */
const uint8_t gWlanScanRates[12] = { 2, 4, 11, 12, 18, 22, 24, 36, 48, 72, 96, 108 };

_Static_assert(sizeof(SceWlanBtRobinWlanMacFilterExpression) == 19,
	       "Robin MAC filter expression");
_Static_assert(sizeof(SceWlanBtRobinWlanMacFilterRequest) == 29,
	       "Robin MAC filter request");

_Static_assert(sizeof(SceWlanBtRobinWlanMulticastRequest) == 196,
	       "Robin multicast request");
_Static_assert(sizeof(SceWlanBtRobinWlanSettingRequest) == 134,
	       "Robin setting request");
_Static_assert(sizeof(SceWlanBtRobinWlanControlRequest) == 4, "Robin control request");
_Static_assert(sizeof(SceWlanBtRobinWlanMacAddressMessage) == 8,
	       "Robin MAC address message");
_Static_assert(sizeof(SceWlanBtRobinWlanEventRequest) == 68, "Robin event request");
_Static_assert(sizeof(SceWlanBtRobinWlanEventParameter) == 6, "Robin event parameter");
_Static_assert(sizeof(SceWlanBtRobinWlanIbssCoalescingMessage) == 16,
	       "Robin IBSS coalescing message");
_Static_assert(sizeof(SceWlanBtRobinWlanMemoryMessage) == 12, "Robin memory message");
_Static_assert(sizeof(SceWlanBtRobinWlanInactivityTimeoutRequest) == 12,
	       "Robin inactivity timeout request");
_Static_assert(sizeof(SceWlanBtScanSsid) == 36, "Robin scan SSID");
_Static_assert(sizeof(SceWlanBtRobinWlanTlvHeader) == 4, "Robin TLV header");
_Static_assert(sizeof(SceWlanBtRobinWlanScanChannel) == 7, "Robin scan channel");
_Static_assert(sizeof(SceWlanBtRobinWlanScanStartParameter) == 6,
	       "Robin scan start parameter");
_Static_assert(sizeof(SceWlanBtRobinWlanBackgroundScanRequest) == 278,
	       "Robin background scan request");
_Static_assert(offsetof(SceWlanBtRobinWlanBackgroundScanRequest, parameters) == 22,
	       "Robin background scan parameters");
_Static_assert(sizeof(SceWlanBtRobinWlanSdioPullControlRequest) == 8,
	       "Robin SDIO pull control request");

SceWlanBtRobinWlanCommand *robinWlanAllocateCommand(SceWlanBtRobinWlanContext *context,
						 unsigned int size);
int robinWlanSubmitCommand(SceWlanBtRobinWlanContext *context,
			  SceWlanBtRobinWlanCommand *command, uint16_t commandId,
			  unsigned int size);
int robinWlanReleaseCommand(SceWlanBtRobinWlanContext *context,
			   SceWlanBtRobinWlanCommand *command);
int robinWlanSubmitAndWaitCommand(SceWlanBtRobinWlanContext *context,
				 SceWlanBtRobinWlanCommand *command, uint16_t commandId,
				 unsigned int size);
int robinWlanWaitCommand(SceWlanBtRobinWlanContext *context,
			SceWlanBtRobinWlanCommand *command);
int wlanIsActiveScanChannel(int channel);

int wlanDerivePasswordKey(const char *password, const void *salt, int saltSize,
			  uint8_t *output);
int wlanBssHasWpa(const WlanBssDesc *descriptor);
int wlanBssHasRsn(const WlanBssDesc *descriptor);
int wlanBssHasWmm(const WlanBssDesc *descriptor);
int wlanCopyInformationElement(const void *buffer, unsigned int length, int id,
			       void *output, unsigned int *outputSize,
			       unsigned int *offset);
int wlanCopyWpaInformationElement(const void *buffer, unsigned int length,
				  void *output, unsigned int *outputSize);

int robinWlanRequestSetup(SceWlanBtRobinWlanContext *context,
			 int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *))
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 47);

	if (!command)
		return 0x80418005;
	command->callback = callback;
	return robinWlanSubmitCommand(context, command, 3, 47);
}

int robinWlanValidateSetupResponse(const void *response, int responseSize)
{
	(void)response;
	return responseSize == 47 ? 0 : 0x80418001;
}

int robinWlanSetMulticastAddresses(SceWlanBtRobinWlanContext *context,
				 unsigned int count, const SceNetEtherAddr *addresses)
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 196);
	SceWlanBtRobinWlanMulticastRequest *request;
	int result;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	request->count[0] = count;
	request->action[0] = 1;
	request->action[1] = 0;
	request->count[1] = count >> 8;
	if (addresses)
		memcpy(request->addresses, addresses, count * sizeof(*addresses));
	result = robinWlanSubmitAndWaitCommand(context, command, 16, 196);
	robinWlanReleaseCommand(context, command);
	return result;
}

int robinWlanReadSetting(SceWlanBtRobinWlanContext *context, uint16_t settingId,
			void *data, unsigned int size)
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 134);
	SceWlanBtRobinWlanSettingRequest *request;
	const SceWlanBtRobinWlanSettingRequest *response;
	int result;

	if (!command)
		return 0x80418005;
	if (size > 128)
		result = 0x80418001;
	else {
		request = command->requestPayload;
		request->settingId[1] = settingId >> 8;
		request->settingId[0] = settingId;
		request->size[0] = size;
		request->action[0] = 0;
		request->action[1] = 0;
		request->size[1] = 0;
		result = robinWlanSubmitAndWaitCommand(context, command, 22, size + 6);
		if (result >= 0) {
			response = command->responsePayload;
			memcpy(data, response->data, size);
		}
	}
	robinWlanReleaseCommand(context, command);
	return result;
}

int robinWlanWriteSetting(SceWlanBtRobinWlanContext *context, uint16_t settingId,
			 const void *data, unsigned int size)
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 134);
	SceWlanBtRobinWlanSettingRequest *request;
	int result;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	command->callback = NULL;
	request->action[1] = 0;
	request->action[0] = 1;
	request->size[1] = size >> 8;
	request->settingId[0] = settingId;
	request->size[0] = size;
	request->settingId[1] = settingId >> 8;
	memcpy(request->data, data, size);
	result = robinWlanSubmitCommand(context, command, 22, size + 6);
	if (result >= 0) {
		result = robinWlanWaitCommand(context, command);
		if (result >= 0)
			result = 0;
		robinWlanReleaseCommand(context, command);
	}
	return result;
}

int robinWlanRequestDeauthentication(SceWlanBtRobinWlanContext *context,
		SceWlanBtRobinWlanCommand **output,
		int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *))
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 8);
	SceWlanBtRobinDeauthentication *request;
	int result;

	if (!command)
		return 0x80418005;
	command->callback = callback;
	request = command->requestPayload;
	__builtin_memcpy(request->bssid, &context->bss.bssid, sizeof(request->bssid));
	request->reason = 3;
	result = robinWlanSubmitCommand(context, command, 0x24, 8);
	if (result >= 0)
		*output = command;
	return result;
}

int robinWlanDeauthenticate(SceWlanBtRobinWlanContext *context)
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 8);
	SceWlanBtRobinDeauthentication *request;
	uint8_t *reasonBytes;
	int result;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	command->callback = NULL;
	__builtin_memcpy(request->bssid, &context->bss.bssid, sizeof(request->bssid));
	reasonBytes = (uint8_t *)&request->reason;
	reasonBytes[0] = 3;
	reasonBytes[1] = 0;
	result = robinWlanSubmitCommand(context, command, 0x24, 8);
	if (result >= 0) {
		result = robinWlanWaitCommand(context, command);
		robinWlanReleaseCommand(context, command);
	}
	return result;
}

int robinWlanSetControl(SceWlanBtRobinWlanContext *context, uint16_t control)
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 4);
	SceWlanBtRobinWlanControlRequest *request;
	int result;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	command->callback = NULL;
	request->control[0] = control;
	request->control[1] = control >> 8;
	result = robinWlanSubmitCommand(context, command, 40, 4);
	if (result >= 0) {
		result = robinWlanWaitCommand(context, command);
		robinWlanReleaseCommand(context, command);
	}
	return result;
}

int robinWlanRequestStartAdhoc(SceWlanBtRobinWlanContext *context,
	WlanBssConnectionConfig *configuration, unsigned int capacity,
	SceWlanBtRobinWlanCommand **output,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *))
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 581);
	WlanAdhocStartRequest *request;
	WlanIbssParameter ibss;
	WlanHtCapabilitiesParameter *ht;
	WlanHtOperationParameter *operation;
	SceWlanBtRobinWlanScanChannelParameter *channel;
	SceWlanBtRobinWlanTlvHeader *security;
	uint8_t *parameter;
	uintptr_t end;
	unsigned int index, rateCount = 0, ssidSize;
	int result;

	if (!command)
		return 0x80418005;
	ssidSize = configuration->bss.ssidSize;
	command->callback = callback;
	request = command->requestPayload;
	if (ssidSize > 32)
		return 0x80418001;
	if (configuration->security.securityMode)
		return 0x80418006;
	memcpy(request->ssid, configuration->bss.ssid, ssidSize);
	if (configuration->bss.bssMode != 2)
		return 0x80418001;
	request->bssType = 2;
	request->beaconInterval[0] = configuration->bss.beaconInterval;
	request->beaconInterval[1] = configuration->bss.beaconInterval >> 8;
	request->dtimPeriod = configuration->bss.dtimPeriod;
	request->capabilities[0] = configuration->bss.capabilities;
	request->capabilities[1] = configuration->bss.capabilities >> 8;
	ibss.id = 6;
	ibss.length = 2;
	ibss.atimWindow[0] = configuration->bss.atimWindow;
	ibss.atimWindow[1] = configuration->bss.atimWindow >> 8;
	memcpy(&request->ssParameters.ibss, &ibss, sizeof(ibss));
	request->phyParameters.directSequence.id = 3;
	request->phyParameters.directSequence.length = 1;
	request->phyParameters.directSequence.channel = configuration->bss.channel;
	for (index = 0; index < 16 && configuration->bss.supportedRates[index]; ++index)
		request->ratesAndParameters[rateCount++] = configuration->bss.supportedRates[index];
	for (index = 0; index < 16 && configuration->bss.extendedRates[index]; ++index)
		request->ratesAndParameters[rateCount++] = configuration->bss.extendedRates[index];
	parameter = request->ratesAndParameters + ROBIN_ADHOC_RATE_CAPACITY;
	end = (uintptr_t)request + sizeof(*request);
	if (!rateCount) {
		memcpy(request->ratesAndParameters, gWlanAdhocBasicRates, sizeof(gWlanAdhocBasicRates));
		memcpy(request->ratesAndParameters + sizeof(gWlanAdhocBasicRates),
		       gWlanAdhocHighRates, sizeof(gWlanAdhocHighRates));
		ht = (WlanHtCapabilitiesParameter *)parameter;
		if ((uintptr_t)(ht + 1) > end)
			return 0x80418005;
		ht->header.type[0] = 45;
		ht->header.type[1] = 0;
		ht->header.size[0] = 26;
		ht->header.size[1] = 0;
		ht->capabilities[0] = 0;
		ht->capabilities[1] = 32;
		ht->ampduParameters = 3;
		memcpy(ht->supportedMcs, gWlanSupportedMcs, sizeof(ht->supportedMcs));
		ht->extendedCapabilities[0] = 0;
		ht->extendedCapabilities[1] = 0;
		ht->transmitBeamforming[0] = 0;
		ht->transmitBeamforming[1] = 0;
		ht->transmitBeamforming[2] = 0;
		ht->transmitBeamforming[3] = 0;
		ht->antennaSelection = 0;
		operation = (WlanHtOperationParameter *)(ht + 1);
		if ((uintptr_t)(operation + 1) > end)
			return 0x80418005;
		operation->header.type[0] = 61;
		operation->header.type[1] = 0;
		operation->header.size[0] = 22;
		operation->header.size[1] = 0;
		operation->channel = configuration->bss.channel;
		operation->operationMode[0] = 4;
		operation->operationMode[1] = 0;
		memcpy(operation->basicMcs, gWlanAdhocBasicMcs, sizeof(operation->basicMcs));
		parameter = (uint8_t *)(operation + 1);
	}
	channel = (SceWlanBtRobinWlanScanChannelParameter *)parameter;
	if ((uintptr_t)&channel->channels[1] > end)
		return 0x80418005;
	channel->header.type[0] = 1;
	channel->header.type[1] = 1;
	channel->header.size[0] = 7;
	channel->header.size[1] = 0;
	channel->channels[0].radio = 0;
	channel->channels[0].channel = configuration->bss.channel;
	channel->channels[0].scanMode = wlanIsActiveScanChannel(configuration->bss.channel) == 0;
	channel->channels[0].minScanTime[0] = 0;
	channel->channels[0].minScanTime[1] = 0;
	channel->channels[0].maxScanTime[0] = 0;
	channel->channels[0].maxScanTime[1] = 0;
	parameter = (uint8_t *)&channel->channels[1];
	if (!configuration->security.securityMode) {
		security = (SceWlanBtRobinWlanTlvHeader *)parameter;
		if ((uintptr_t)(security + 1) + 1 > end)
			return 0x80418005;
		security->type[0] = 0xc0;
		security->type[1] = 1;
		security->size[0] = 1;
		security->size[1] = 0;
		*(uint8_t *)(security + 1) = 1;
		parameter = (uint8_t *)(security + 1) + 1;
	}
	result = robinWlanSubmitCommand(context, command, 43, parameter - (uint8_t *)request);
	if (result >= 0)
		*output = command;
	return result;
}

int robinWlanStartAdhoc(SceWlanBtRobinWlanContext *context,
		      WlanBssConnectionConfig *configuration, unsigned int capacity)
{
	SceWlanBtRobinWlanCommand *command = NULL;
	const SceWlanBtRobinWlanAdhocResult *response;
	int result;

	result = robinWlanRequestStartAdhoc(context, configuration, capacity, &command, NULL);
	if (result >= 0) {
		result = robinWlanWaitCommand(context, command);
		if (result >= 0) {
			if (command->responseSize > 8) {
				response = command->responsePayload;
				result = 0;
				configuration->bss.bssid = response->bssid;
			} else {
				result = 0x80418001;
			}
			goto release;
		}
	}
	if (!command)
		return result;
release:
	robinWlanReleaseCommand(context, command);
	return result;
}

int robinWlanJoinAdhoc(SceWlanBtRobinWlanContext *context,
		     WlanBssConnectionConfig *configuration)
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 218);
	WlanAdhocJoinRequest *request;
	WlanIbssParameter ibss;
	WlanHtCapabilitiesParameter *ht;
	WlanHtOperationParameter *operation;
	SceWlanBtRobinWlanScanChannelParameter *channel;
	SceWlanBtRobinWlanTlvHeader *security;
	uint8_t *parameter;
	uintptr_t end;
	unsigned int index, rateCount = 0;
	int result;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	if (configuration->bss.ssidSize > 32) {
		result = 0x80418001;
		goto out;
	}
	if (configuration->security.securityMode) {
		result = 0x80418006;
		goto out;
	}
	request->bssid = configuration->bss.bssid;
	memcpy(request->ssid, configuration->bss.ssid, configuration->bss.ssidSize);
	if (configuration->bss.bssMode != 2) {
		result = 0x80418001;
		goto out;
	}
	request->bssType = 2;
	request->beaconInterval[0] = configuration->bss.beaconInterval;
	request->beaconInterval[1] = configuration->bss.beaconInterval >> 8;
	request->dtimPeriod = configuration->bss.dtimPeriod;
	request->phyParameters.directSequence.id = 3;
	request->phyParameters.directSequence.length = 1;
	request->phyParameters.directSequence.channel = configuration->bss.channel;
	ibss.id = 6;
	ibss.length = 2;
	ibss.atimWindow[0] = configuration->bss.atimWindow;
	ibss.atimWindow[1] = configuration->bss.atimWindow >> 8;
	memcpy(&request->ssParameters.ibss, &ibss, sizeof(ibss));
	request->capabilities[0] = configuration->bss.capabilities;
	request->capabilities[1] = configuration->bss.capabilities >> 8;
	for (index = 0; index < 16 && configuration->bss.supportedRates[index]; ++index)
		request->ratesAndParameters[rateCount++] = configuration->bss.supportedRates[index];
	for (index = 0; index < 16 && configuration->bss.extendedRates[index]; ++index)
		request->ratesAndParameters[rateCount++] = configuration->bss.extendedRates[index];
	parameter = request->ratesAndParameters + ROBIN_ADHOC_RATE_CAPACITY + 4;
	end = (uintptr_t)request + sizeof(*request);
	if (configuration->bss.ht.enabled) {
		ht = (WlanHtCapabilitiesParameter *)parameter;
		if ((uintptr_t)(ht + 1) > end)
			goto noSpace;
		ht->header.type[0] = 45;
		ht->header.type[1] = 0;
		ht->header.size[0] = 26;
		ht->header.size[1] = 0;
		ht->capabilities[0] = 32;
		ht->capabilities[1] = 0;
		ht->ampduParameters = 3;
		memcpy(ht->supportedMcs, gWlanSupportedMcs, sizeof(ht->supportedMcs));
		ht->extendedCapabilities[0] = 0;
		ht->extendedCapabilities[1] = 0;
		ht->transmitBeamforming[0] = 0;
		ht->transmitBeamforming[1] = 0;
		ht->transmitBeamforming[2] = 0;
		ht->transmitBeamforming[3] = 0;
		ht->antennaSelection = 0;
		operation = (WlanHtOperationParameter *)(ht + 1);
		if ((uintptr_t)(operation + 1) > end)
			goto noSpace;
		operation->header.type[0] = 61;
		operation->header.type[1] = 0;
		operation->header.size[0] = 22;
		operation->header.size[1] = 0;
		operation->channel = configuration->bss.channel;
		operation->operationMode[0] = 4;
		operation->operationMode[1] = 0;
		memcpy(operation->basicMcs, gWlanAdhocBasicMcs, sizeof(operation->basicMcs));
		parameter = (uint8_t *)(operation + 1);
	}
	channel = (SceWlanBtRobinWlanScanChannelParameter *)parameter;
	if ((uintptr_t)&channel->channels[1] > end)
		goto noSpace;
	channel->header.type[0] = 1;
	channel->header.type[1] = 1;
	channel->header.size[0] = 7;
	channel->header.size[1] = 0;
	channel->channels[0].radio = 0;
	channel->channels[0].channel = configuration->bss.channel;
	channel->channels[0].scanMode = wlanIsActiveScanChannel(configuration->bss.channel) == 0;
	channel->channels[0].minScanTime[0] = 0;
	channel->channels[0].minScanTime[1] = 0;
	channel->channels[0].maxScanTime[0] = 0;
	channel->channels[0].maxScanTime[1] = 0;
	parameter = (uint8_t *)&channel->channels[1];
	if (!configuration->security.securityMode) {
		security = (SceWlanBtRobinWlanTlvHeader *)parameter;
		if ((uintptr_t)(security + 1) + 1 > end)
			goto noSpace;
		security->type[0] = 0xc0;
		security->type[1] = 1;
		security->size[0] = 1;
		security->size[1] = 0;
		*(uint8_t *)(security + 1) = 1;
		parameter = (uint8_t *)(security + 1) + 1;
	}
	result = robinWlanSubmitAndWaitCommand(context, command, 44,
					     parameter - (uint8_t *)request);
	/* The original releases the command after every allocated error path. */
out:
	robinWlanReleaseCommand(context, command);
	return result;
noSpace:
	result = 0x80418005;
	goto out;
}

int robinWlanRequestStopAdhoc(SceWlanBtRobinWlanContext *context,
			     SceWlanBtRobinWlanCommand **output,
			     int (*callback)(SceWlanBtRobinWlanContext *,
					     SceWlanBtRobinWlanCommand *))
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 0);
	int result;

	if (!command)
		return 0x80418005;
	command->callback = callback;
	result = robinWlanSubmitCommand(context, command, 64, 0);
	if (result >= 0)
		*output = command;
	return result;
}

int robinWlanStopAdhoc(SceWlanBtRobinWlanContext *context)
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 0);
	int result;

	if (!command)
		return 0x80418005;
	command->callback = NULL;
	result = robinWlanSubmitCommand(context, command, 64, 0);
	if (result >= 0) {
		result = robinWlanWaitCommand(context, command);
		robinWlanReleaseCommand(context, command);
	}
	return result;
}

int robinWlanRequestMacAddress(SceWlanBtRobinWlanContext *context,
			       SceWlanBtRobinWlanCommand **output,
			       int (*callback)(SceWlanBtRobinWlanContext *,
					       SceWlanBtRobinWlanCommand *))
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 8);
	SceWlanBtRobinWlanMacAddressMessage *request;
	int result;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	command->callback = callback;
	request->action[0] = 0;
	request->action[1] = 0;
	result = robinWlanSubmitCommand(context, command, 77, 8);
	if (result >= 0)
		*output = command;
	return result;
}

int robinWlanReadMacAddressResponse(SceWlanBtRobinWlanCommand *command,
				   SceNetEtherAddr *address)
{
	const SceWlanBtRobinWlanMacAddressMessage *response;

	if (command->responseSize != 8)
		return 0x80418001;
	response = command->responsePayload;
	*address = response->address;
	return 0;
}

int robinWlanGetMacAddress(SceWlanBtRobinWlanContext *context,
			 SceNetEtherAddr *address)
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 8);
	SceWlanBtRobinWlanMacAddressMessage *request;
	const SceWlanBtRobinWlanMacAddressMessage *response;
	int result;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	command->callback = NULL;
	request->action[0] = 0;
	request->action[1] = 0;
	result = robinWlanSubmitCommand(context, command, 77, 8);
	if (result >= 0) {
		result = robinWlanWaitCommand(context, command);
		if (result >= 0) {
			if (command->responseSize != 8) {
				result = 0x80418001;
			} else {
				response = command->responsePayload;
				result = 0;
				*address = response->address;
			}
		}
		robinWlanReleaseCommand(context, command);
	}
	return result;
}

int robinWlanSetWepKeys(SceWlanBtRobinWlanContext *context,
		       const WlanConnectionSecurity *security)
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 130);
	SceWlanBtRobinWlanWepKeyRequest *request;
	SceWlanBtRobinWlanWepKeyParameter *parameter;
	unsigned int keySize, parameterSize, index, defaultKey;
	uint8_t *current;
	uintptr_t end;
	int result;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	request->action[0] = 1;
	request->action[1] = 0;
	defaultKey = security->defaultKey;
	if (defaultKey > 3) {
		result = 0x80418001;
		goto out;
	}
	if (security->wepCipher == 1)
		keySize = 5;
	else if (security->wepCipher == 2)
		keySize = 13;
	else {
		result = 0x80418001;
		goto out;
	}
	parameterSize = sizeof(*parameter) + keySize;
	current = request->parameters;
	end = (uintptr_t)(request + 1);
	for (index = 0; index < 4; index++) {
		parameter = (SceWlanBtRobinWlanWepKeyParameter *)current;
		if ((uintptr_t)(current + parameterSize) > end) {
			result = 0x80418005;
			goto out;
		}
		parameter->header.type[0] = 0;
		parameter->header.type[1] = 1;
		parameter->header.size[0] = keySize + 8;
		parameter->header.size[1] = 0;
		parameter->keyType[0] = 0;
		parameter->keyType[1] = 0;
		parameter->keyFlags[0] = 7;
		parameter->keyFlags[1] = 0;
		parameter->keyLength[0] = keySize;
		parameter->keyLength[1] = 0;
		parameter->keyIndex = index;
		parameter->isDefault = defaultKey == index;
		current += parameterSize;
		memcpy(parameter->key, security->wepKeys[index], keySize);
	}
	result = robinWlanSubmitAndWaitCommand(context, command, 0x5e,
					     current - (uint8_t *)request);
out:
	robinWlanReleaseCommand(context, command);
	return result;
}

int robinWlanRequestBackgroundScan(
	SceWlanBtRobinWlanContext *context, uint8_t enabled,
	const SceWlanBtScanSsid *ssids, unsigned int ssidCount,
	const SceWlanBtScanSsid *wildcardSsids, unsigned int wildcardCount,
	uint8_t bssType, uint8_t channelsPerScan, uint32_t scanInterval,
	const uint8_t *channels, unsigned int channelCount, uint8_t scanMode,
	uint16_t minScanTime, uint16_t maxScanTime,
	SceWlanBtRobinWlanCommand **output,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *))
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 278);
	SceWlanBtRobinWlanBackgroundScanRequest *request;
	SceWlanBtRobinWlanTlvHeader *current;
	SceWlanBtRobinWlanScanChannelParameter *channelParameter;
	SceWlanBtRobinWlanScanStartParameter *startParameter;
	uintptr_t end;
	unsigned int ssidTotal = 0;
	unsigned int count;
	unsigned int size;
	int result;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	command->callback = callback;
	request->enabled = enabled;
	request->action[0] = 1;
	end = (uintptr_t)(request + 1);
	request->bssType = bssType;
	current = (SceWlanBtRobinWlanTlvHeader *)request->parameters;
	request->channelsPerScan = channelsPerScan;
	request->scanInterval[0] = scanInterval;
	request->action[1] = 0;
	request->scanInterval[1] = scanInterval >> 8;
	request->scanInterval[2] = scanInterval >> 16;
	request->scanInterval[3] = scanInterval >> 24;
	if (ssidCount) {
		size = ssids->size;
		for (;;) {
			SceWlanBtRobinWlanScanSsidParameter *parameter =
				(SceWlanBtRobinWlanScanSsidParameter *)current;

			if (size > 32)
				return 0x80418001;
			if (end < (uintptr_t)(parameter + 1) + size)
				return 0x80418005;
			parameter->header.type[0] = 0;
			++ssidTotal;
			parameter->header.type[1] = 0;
			parameter->header.size[0] = size;
			parameter->header.size[1] = 0;
			current = (SceWlanBtRobinWlanTlvHeader *)
				((uint8_t *)memcpy(parameter->ssid, ssids->ssid, size) + size);
			if (ssidTotal >= ssidCount || ssidTotal > 9)
				break;
			++ssids;
			size = ssids->size;
			__builtin_prefetch((const void *)((uintptr_t)ssids + 6 * sizeof(*ssids)));
		}
	}
	if (wildcardCount) {
		count = 0;
		size = wildcardSsids->size;
		for (;;) {
			SceWlanBtRobinWlanWildcardSsidParameter *parameter =
				(SceWlanBtRobinWlanWildcardSsidParameter *)current;
			unsigned int payloadSize = size + 1;

			if (size > 32)
				return 0x80418001;
			if (payloadSize >= 0x10000 ||
			    end < (uintptr_t)(parameter + 1) + size)
				return 0x80418005;
			parameter->header.type[0] = 18;
			parameter->header.type[1] = 1;
			++count;
			parameter->header.size[0] = payloadSize;
			parameter->maxLength = 255;
			parameter->header.size[1] = payloadSize >> 8;
			++ssidTotal;
			memcpy(parameter->ssid, wildcardSsids->ssid, size);
			current = (SceWlanBtRobinWlanTlvHeader *)(parameter->ssid + size);
			if (count >= wildcardCount || count > 9)
				break;
			++wildcardSsids;
			size = wildcardSsids->size;
			__builtin_prefetch((const void *)((uintptr_t)wildcardSsids +
							5 * sizeof(*wildcardSsids)));
		}
	}
	if (ssidTotal) {
		request->reportCondition[0] = 1;
		request->reportCondition[3] = 0x80;
		request->reportCondition[1] = 0;
		request->reportCondition[2] = 0;
	} else {
		request->reportCondition[0] = 0;
		request->reportCondition[1] = 0;
		request->reportCondition[2] = 0;
		request->reportCondition[3] = 0;
	}
	channelParameter = (SceWlanBtRobinWlanScanChannelParameter *)current;
	size = sizeof(SceWlanBtRobinWlanScanChannel) * channelCount;
	if (size >= 0x10000 || end < (uintptr_t)(channelParameter + 1) + size)
		return 0x80418005;
	channelParameter->header.size[0] = size;
	channelParameter->header.type[0] = 1;
	channelParameter->header.type[1] = 1;
	channelParameter->header.size[1] = size >> 8;
	if (channelCount) {
		SceWlanBtRobinWlanScanChannel *channel = channelParameter->channels;
		const uint8_t *channelEnd = channels + channelCount;

		do {
			int active;

			channel->radio = 0;
			channel->channel = *channels;
			active = wlanIsActiveScanChannel(*channels++);
			channel->minScanTime[1] = minScanTime >> 8;
			channel->maxScanTime[0] = maxScanTime;
			channel->minScanTime[0] = minScanTime;
			channel->maxScanTime[1] = maxScanTime >> 8;
			channel->scanMode = active ? scanMode : scanMode | 1;
			++channel;
		} while (channels != channelEnd);
	}
	startParameter = (SceWlanBtRobinWlanScanStartParameter *)
		(channelParameter->channels + channelCount);
	if (end < (uintptr_t)(startParameter + 1))
		return 0x80418005;
	startParameter->header.type[0] = 30;
	startParameter->header.type[1] = 1;
	startParameter->header.size[0] = 2;
	startParameter->header.size[1] = 0;
	startParameter->startLater[0] = 0;
	startParameter->startLater[1] = 0;
	result = robinWlanSubmitCommand(context, command, 107,
		(uintptr_t)(startParameter + 1) - (uintptr_t)request);
	*output = command;
	return result;
}

int robinWlanSetBackgroundScan(
	SceWlanBtRobinWlanContext *context, uint8_t enabled,
	const SceWlanBtScanSsid *ssids, unsigned int ssidCount,
	const SceWlanBtScanSsid *wildcardSsids, unsigned int wildcardCount,
	uint8_t bssType, uint8_t channelsPerScan, uint32_t scanInterval,
	const uint8_t *channels, unsigned int channelCount, uint8_t scanMode,
	uint16_t minScanTime, uint16_t maxScanTime)
{
	SceWlanBtRobinWlanCommand *command = NULL;
	int result;

	result = robinWlanRequestBackgroundScan(context, enabled, ssids, ssidCount,
		wildcardSsids, wildcardCount, bssType, channelsPerScan, scanInterval,
		channels, channelCount, scanMode, minScanTime, maxScanTime, &command, NULL);
	if (result >= 0)
		result = robinWlanWaitCommand(context, command);
	if (command)
		robinWlanReleaseCommand(context, command);
	return result;
}

int robinWlanRequestBackgroundScanQuery(SceWlanBtRobinWlanContext *context,
			    SceWlanBtRobinWlanCommand **output,
			    int (*callback)(SceWlanBtRobinWlanContext *,
					    SceWlanBtRobinWlanCommand *))
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 1);
	SceWlanBtRobinWlanBackgroundScanQuery *request;
	int result;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	command->callback = callback;
	request->flush = 1;
	result = robinWlanSubmitCommand(context, command, 108, 1);
	if (result >= 0)
		*output = command;
	return result;
}

int robinWlanSubscribeEvents(SceWlanBtRobinWlanContext *context,
			     int (*callback)(SceWlanBtRobinWlanContext *,
					     SceWlanBtRobinWlanCommand *))
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 68);
	SceWlanBtRobinWlanEventRequest *request;
	SceWlanBtRobinWlanEventParameter *parameter;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	command->callback = callback;
	request->events[0] = 9;
	request->action[0] = 1;
	request->action[1] = 0;
	request->events[1] = 0;
	parameter = (SceWlanBtRobinWlanEventParameter *)request->parameters;
	if ((uintptr_t)(request + 1) < (uintptr_t)(parameter + 1))
		return 0x80418005;
	parameter->type[1] = 1;
	parameter->type[0] = 4;
	parameter->size[0] = 2;
	parameter->size[1] = 0;
	parameter->value[0] = 100;
	parameter->value[1] = 100;
	return robinWlanSubmitCommand(context, command, 117, 10);
}

int robinWlanRequestIbssCoalescing(SceWlanBtRobinWlanContext *context, int enabled,
				 SceWlanBtRobinWlanCommand **output,
				 int (*callback)(SceWlanBtRobinWlanContext *,
						 SceWlanBtRobinWlanCommand *))
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 16);
	SceWlanBtRobinWlanIbssCoalescingMessage *request;
	int result;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	command->callback = callback;
	request->action[0] = 1;
	request->action[1] = 0;
	if (enabled) {
		request->enabled[0] = 1;
		request->enabled[1] = 0;
	} else {
		request->enabled[0] = 0;
		request->enabled[1] = 0;
	}
	result = robinWlanSubmitCommand(context, command, 131, 16);
	if (result >= 0)
		*output = command;
	return result;
}

int robinWlanSetIbssCoalescing(SceWlanBtRobinWlanContext *context, int enabled)
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 16);
	SceWlanBtRobinWlanIbssCoalescingMessage *request;
	int result;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	command->callback = NULL;
	request->action[1] = 0;
	request->action[0] = 1;
	if (enabled) {
		request->enabled[0] = 1;
		request->enabled[1] = 0;
	} else {
		request->enabled[0] = 0;
		request->enabled[1] = 0;
	}
	result = robinWlanSubmitCommand(context, command, 131, 16);
	if (result >= 0) {
		result = robinWlanWaitCommand(context, command);
		robinWlanReleaseCommand(context, command);
	}
	return result;
}

int robinWlanReadMemory(SceWlanBtRobinWlanContext *context, uint32_t address,
			uint32_t *output)
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 12);
	SceWlanBtRobinWlanMemoryMessage *request;
	const SceWlanBtRobinWlanMemoryMessage *response;
	int result;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	request->address[0] = address;
	request->action[0] = 0;
	request->action[1] = 0;
	request->address[1] = address >> 8;
	request->address[2] = address >> 16;
	request->address[3] = address >> 24;
	result = robinWlanSubmitAndWaitCommand(context, command, 134, 12);
	if (result >= 0) {
		response = command->responsePayload;
		*output = (uint32_t)response->value[2] << 16 |
			  (uint32_t)response->value[1] << 8 |
			  response->value[0] |
			  (uint32_t)response->value[3] << 24;
	}
	robinWlanReleaseCommand(context, command);
	return result;
}

int robinWlanWriteMemory(SceWlanBtRobinWlanContext *context, uint32_t address,
			 uint32_t value)
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 12);
	SceWlanBtRobinWlanMemoryMessage *request;
	int result;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	request->address[1] = address >> 8;
	request->address[2] = address >> 16;
	request->action[1] = 0;
	request->address[0] = address;
	request->action[0] = 1;
	request->address[3] = address >> 24;
	request->value[0] = value;
	request->value[1] = value >> 8;
	request->value[2] = value >> 16;
	request->value[3] = value >> 24;
	result = robinWlanSubmitAndWaitCommand(context, command, 134, 12);
	robinWlanReleaseCommand(context, command);
	return result;
}

int robinWlanSetInactivityTimeout(SceWlanBtRobinWlanContext *context,
				int (*callback)(SceWlanBtRobinWlanContext *,
						SceWlanBtRobinWlanCommand *))
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 12);
	SceWlanBtRobinWlanInactivityTimeoutRequest *request;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	command->callback = callback;
	request->timeoutUnit[0] = 1000;
	request->timeoutUnit[1] = 1000 >> 8;
	request->action[0] = 1;
	request->unicastTimeout[0] = 200;
	request->action[1] = 0;
	request->unicastTimeout[1] = 0;
	request->multicastTimeout[0] = 50;
	request->multicastTimeout[1] = 0;
	request->powerSaveEntryTimeout[0] = 0;
	request->powerSaveEntryTimeout[1] = 0;
	return robinWlanSubmitCommand(context, command, 138, 12);
}

int robinWlanSetSdioPullControl(
	SceWlanBtRobinWlanContext *context, uint16_t pullUpDelay, uint16_t pullDownDelay,
	uint8_t pullControl, uint8_t pullFlags,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *))
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 8);
	SceWlanBtRobinWlanSdioPullControlRequest *request;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	command->callback = callback;
	request->pullUpDelay[0] = pullUpDelay;
	request->pullDownDelay[0] = pullDownDelay;
	request->extendedControls[0] = pullControl;
	request->extendedControls[1] = pullFlags;
	request->pullUpDelay[1] = pullUpDelay >> 8;
	request->pullDownDelay[1] = pullDownDelay >> 8;
	request->action[0] = 1;
	request->action[1] = 0;
	return robinWlanSubmitCommand(context, command, 147, 8);
}

int robinWlanSetMacFilter(
	SceWlanBtRobinWlanContext *context, const SceNetEtherAddr *address,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *))
{
	SceWlanBtRobinWlanMacFilterExpression expression = gMacFilterExpression;
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 42);
	SceWlanBtRobinWlanMacFilterRequest *request;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	command->callback = callback;
	request->criteria[0] = 11;
	request->mode = 3;
	request->expressionSize[0] = sizeof(expression);
	request->criteria[1] = 0;
	request->criteria[2] = 0;
	request->criteria[3] = 0;
	request->entryCount[0] = 1;
	request->entryCount[1] = 0;
	request->action = 1;
	request->expressionSize[1] = 0;
	expression.packetOffset[0] = 56;
	expression.address = *address;
	request->expression = expression;
	return robinWlanSubmitCommand(context, command, 154, sizeof(*request));
}

int robinWlanRequestRssi(
	SceWlanBtRobinWlanContext *context, SceWlanBtRobinWlanCommand **output,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *))
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 32);
	SceWlanBtRobinWlanRssiRequest *request;
	int result;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	command->callback = callback;
	request->action[0] = 0;
	request->action[1] = 0;
	result = robinWlanSubmitCommand(context, command, 164, sizeof(*request));
	if (result >= 0)
		*output = command;
	return result;
}

int robinWlanReadRssiResponse(SceWlanBtRobinWlanCommand *command, WlanSignalInfo *output)
{
	const SceWlanBtRobinWlanRssiResponse *response;
	int16_t rssi;
	int magnitude;

	if (command->responseSize != sizeof(*response))
		return 0x80418001;
	response = command->responsePayload;
	rssi = (uint16_t)response->averageBeaconRssi[0] |
		(uint16_t)response->averageBeaconRssi[1] << 8;
	magnitude = rssi < 0 ? -rssi : rssi;
	output->rssi = rssi;
	if ((uint16_t)magnitude > 99) {
		output->signalQuality = 1;
		return 0;
	}
	if ((uint16_t)magnitude <= 50)
		output->signalQuality = 100;
	else
		output->signalQuality = (uint16_t)(2 * (100 - magnitude));
	return 0;
}

int robinWlanGetRssi(SceWlanBtRobinWlanContext *context, WlanSignalInfo *output)
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 32);
	SceWlanBtRobinWlanRssiRequest *request;
	const SceWlanBtRobinWlanRssiResponse *response;
	int16_t rssi;
	int magnitude;
	int result;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	command->callback = NULL;
	request->action[0] = 0;
	request->action[1] = 0;
	result = robinWlanSubmitCommand(context, command, 164, sizeof(*request));
	if (result < 0)
		return result;
	result = robinWlanWaitCommand(context, command);
	if (result < 0)
		goto out;
	if (command->responseSize != sizeof(*response)) {
		result = 0x80418001;
		goto out;
	}
	response = command->responsePayload;
	rssi = (uint16_t)response->averageBeaconRssi[0] |
		(uint16_t)response->averageBeaconRssi[1] << 8;
	magnitude = rssi < 0 ? -rssi : rssi;
	output->rssi = rssi;
	if ((uint16_t)magnitude > 99) {
		output->signalQuality = 1;
		result = 0;
		goto out;
	}
	if ((uint16_t)magnitude <= 50)
		output->signalQuality = 100;
	else
		output->signalQuality = (uint16_t)(2 * (100 - magnitude));
	result = 0;
out:
	robinWlanReleaseCommand(context, command);
	return result;
}

int robinWlanRequestFunctionInitialize(
	SceWlanBtRobinWlanContext *context,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *))
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 0);

	if (!command)
		return 0x80418005;
	command->callback = callback;
	return robinWlanSubmitCommand(context, command, 169, 0);
}

int robinWlanRequestFunctionShutdown(
	SceWlanBtRobinWlanContext *context,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *))
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 0);

	if (!command)
		return 0x80418005;
	command->callback = callback;
	return robinWlanSubmitCommand(context, command, 170, 0);
}

int robinWlanSetPasswordKey(SceWlanBtRobinWlanContext *context,
			   const WlanConnectionSecurity *security,
			   const WlanBssDesc *descriptor)
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 260);
	SceWlanBtRobinWlanPasswordKeyRequest *request;
	SceWlanBtRobinWlanScanSsidParameter *ssid;
	SceWlanBtRobinWlanPasswordKeyParameter *parameter;
	uint8_t output[64];
	unsigned int ssidSize;
	uintptr_t end;
	int result;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	request->action[0] = 1;
	request->action[1] = 0;
	request->cacheResult[0] = 0;
	request->cacheResult[1] = 0;
	ssidSize = descriptor->ssidSize;
	if (ssidSize > 32) {
		result = 0x80418001;
		goto out;
	}
	ssid = (SceWlanBtRobinWlanScanSsidParameter *)request->parameters;
	end = (uintptr_t)(request + 1);
	if ((uintptr_t)(ssid->ssid + ssidSize) > end) {
		result = 0x80418005;
		goto out;
	}
	ssid->header.type[0] = 0;
	ssid->header.type[1] = 0;
	ssid->header.size[0] = ssidSize;
	ssid->header.size[1] = 0;
	memcpy(ssid->ssid, descriptor->ssid, ssidSize);
	parameter = (SceWlanBtRobinWlanPasswordKeyParameter *)(ssid->ssid + ssidSize);
	if (security->secretType == 1) {
		wlanDerivePasswordKey(security->secret, descriptor->ssid,
				     descriptor->ssidSize, output);
		if ((uintptr_t)(parameter + 1) > end) {
			result = 0x80418005;
			goto out;
		}
		parameter->header.type[0] = 0x44;
		parameter->header.type[1] = 1;
		parameter->header.size[0] = 32;
		parameter->header.size[1] = 0;
		memcpy(parameter->key, output, sizeof(parameter->key));
	} else if (security->secretType == 2) {
		if ((uintptr_t)(parameter + 1) > end) {
			result = 0x80418005;
			goto out;
		}
		parameter->header.type[0] = 0x44;
		parameter->header.type[1] = 1;
		parameter->header.size[0] = 32;
		parameter->header.size[1] = 0;
		memcpy(parameter->key, security->secret, sizeof(parameter->key));
	} else {
		result = 0x80418001;
		goto out;
	}
	result = robinWlanSubmitAndWaitCommand(context, command, 0xc4,
		(uint8_t *)(parameter + 1) - (uint8_t *)request);
out:
	robinWlanReleaseCommand(context, command);
	return result;
}

int mrvlExecAssociate(SceWlanBtRobinWlanContext *context,
		       const WlanBssDesc *descriptor,
		       const WlanConnectionSecurity *security,
		       const void *informationElements, unsigned int informationElementsSize,
		       unsigned int *htEnabled)
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 512);
	WlanAssociationRequest *request;
	WlanAssociationParameter *parameter;
	WlanChannelParameter *channel;
	WlanContentionFreeParameter *contentionFree;
	WlanAuthenticationParameter *authentication;
	SceWlanBtRobinWlanScanChannelParameter *scanChannel;
	WlanWmmParameter *wmm;
	WlanHtCapabilitiesParameter *ht;
	const WlanAssociationResponse *response;
	uint8_t rates[260];
	uint8_t output[256];
	unsigned int rateCount = 0, outputSize, offset;
	unsigned int ssidSize, highThroughput, status, capabilities;
	unsigned int authenticationMode;
	uint16_t beaconInterval;
	uint8_t selectedChannel;
	uintptr_t end;
	int result;
	unsigned int i;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	end = (uintptr_t)(request + 1);
	if (descriptor->ssidSize > 32)
		goto invalid;
	request->peer = descriptor->bssid;
	capabilities = descriptor->capabilities;
	request->capabilities[0] = capabilities;
	request->capabilities[1] = capabilities >> 8;
	request->listenInterval[0] = 10;
	request->listenInterval[1] = 0;
	beaconInterval = descriptor->beaconInterval;
	request->beaconInterval[0] = beaconInterval;
	request->beaconInterval[1] = beaconInterval >> 8;
	request->dtimPeriod = descriptor->dtimPeriod;
	ssidSize = descriptor->ssidSize;
	if (ssidSize > 32)
		goto invalid;
	parameter = (WlanAssociationParameter *)request->parameters;
	if ((uintptr_t)(parameter->data + ssidSize) > end)
		goto noSpace;
	parameter->header.type[0] = 0;
	parameter->header.type[1] = 0;
	parameter->header.size[0] = ssidSize;
	parameter->header.size[1] = 0;
	channel = (WlanChannelParameter *)((uint8_t *)memcpy(parameter->data,
					 descriptor->ssid, ssidSize) + ssidSize);
	selectedChannel = descriptor->channel;
	if ((uintptr_t)(channel + 1) > end)
		goto noSpace;
	channel->header.type[0] = 3;
	channel->header.type[1] = 0;
	channel->header.size[0] = 1;
	channel->header.size[1] = 0;
	channel->channel = selectedChannel;
	contentionFree = (WlanContentionFreeParameter *)(channel + 1);
	if ((uintptr_t)(contentionFree + 1) > end)
		goto noSpace;
	contentionFree->header.type[0] = 4;
	contentionFree->header.type[1] = 0;
	contentionFree->header.size[0] = 6;
	contentionFree->header.size[1] = 0;
	contentionFree->count = 0;
	contentionFree->period = 0;
	contentionFree->maxDuration[0] = 0;
	contentionFree->maxDuration[1] = 0;
	contentionFree->remainingDuration[0] = 0;
	contentionFree->remainingDuration[1] = 0;
	for (i = 0; i < 16 && descriptor->supportedRates[i]; i++)
		rates[rateCount++] = descriptor->supportedRates[i];
	for (i = 0; i < 16 && descriptor->extendedRates[i]; i++)
		rates[rateCount++] = descriptor->extendedRates[i];
	parameter = (WlanAssociationParameter *)(contentionFree + 1);
	if ((uintptr_t)(parameter->data + rateCount) > end)
		goto noSpace;
	parameter->header.type[0] = 1;
	parameter->header.type[1] = 0;
	parameter->header.size[0] = rateCount;
	parameter->header.size[1] = 0;
	memcpy(parameter->data, rates, rateCount);
	authentication = (WlanAuthenticationParameter *)(parameter->data + rateCount);
	authenticationMode = security->authenticationMode;
	if (authenticationMode > 1)
		goto invalid;
	if ((uintptr_t)(authentication + 1) > end)
		goto noSpace;
	authentication->header.type[0] = 0x1f;
	authentication->header.type[1] = 1;
	authentication->header.size[0] = 2;
	authentication->header.size[1] = 0;
	authentication->authenticationMode[0] = authenticationMode;
	authentication->authenticationMode[1] = 0;
	parameter = (WlanAssociationParameter *)(authentication + 1);
	highThroughput = descriptor->ht.enabled;
	switch (security->securityMode) {
	case 0:
		break;
	case 1:
		result = robinWlanSetWepKeys(context, security);
		if (result < 0)
			goto out;
		highThroughput = 0;
		break;
	case 2:
		outputSize = 255;
		offset = 0;
		if (wlanBssHasRsn(descriptor)) {
			result = wlanCopyInformationElement(descriptor->informationElements,
				descriptor->informationElementsSize, 48, output, &outputSize, &offset);
			if (result < 0)
				goto out;
			if (outputSize >= 0x10000 ||
			    (uintptr_t)(parameter->data + outputSize) > end)
				goto noSpace;
			parameter->header.type[0] = 48;
			parameter->header.type[1] = 0;
			parameter->header.size[0] = outputSize;
			parameter->header.size[1] = outputSize >> 8;
			memcpy(parameter->data, output, outputSize);
			parameter = (WlanAssociationParameter *)(parameter->data + outputSize);
		} else {
			if (!wlanBssHasWpa(descriptor))
				goto invalid;
			result = wlanCopyWpaInformationElement(descriptor->informationElements,
				descriptor->informationElementsSize, output, &outputSize);
			if (result < 0)
				goto out;
			if (outputSize >= 0x10000 ||
			    (uintptr_t)(parameter->data + outputSize) > end)
				goto noSpace;
			parameter->header.type[0] = 221;
			parameter->header.type[1] = 0;
			parameter->header.size[0] = outputSize;
			parameter->header.size[1] = outputSize >> 8;
			parameter = (WlanAssociationParameter *)((uint8_t *)
				memcpy(parameter->data, output, outputSize) + outputSize);
			if (!(descriptor->wpa.pairwiseCipher & 8))
				highThroughput = 0;
		}
		result = robinWlanSetPasswordKey(context, security, descriptor);
		if (result < 0)
			goto out;
		break;
	case 3:
		outputSize = 255;
		if (!wlanBssHasWpa(descriptor))
			goto invalid;
		result = wlanCopyWpaInformationElement(descriptor->informationElements,
			descriptor->informationElementsSize, output, &outputSize);
		if (result < 0)
			goto out;
		if (outputSize >= 0x10000 ||
		    (uintptr_t)(parameter->data + outputSize) > end)
			goto noSpace;
		parameter->header.type[0] = 221;
		parameter->header.type[1] = 0;
		parameter->header.size[0] = outputSize;
		parameter->header.size[1] = outputSize >> 8;
		memcpy(parameter->data, output, outputSize);
		if (!(descriptor->wpa.pairwiseCipher & 8))
			highThroughput = 0;
		result = robinWlanSetPasswordKey(context, security, descriptor);
		if (result < 0)
			goto out;
		parameter = (WlanAssociationParameter *)(parameter->data + outputSize);
		break;
	case 4:
		outputSize = 255;
		offset = 0;
		if (!wlanBssHasRsn(descriptor))
			goto invalid;
		result = wlanCopyInformationElement(descriptor->informationElements,
			descriptor->informationElementsSize, 48, output, &outputSize, &offset);
		if (result < 0)
			goto out;
		if (outputSize >= 0x10000 ||
		    (uintptr_t)(parameter->data + outputSize) > end)
			goto noSpace;
		parameter->header.type[0] = 48;
		parameter->header.type[1] = 0;
		parameter->header.size[0] = outputSize;
		parameter->header.size[1] = outputSize >> 8;
		parameter = (WlanAssociationParameter *)((uint8_t *)
			memcpy(parameter->data, output, outputSize) + outputSize);
		result = robinWlanSetPasswordKey(context, security, descriptor);
		if (result < 0)
			goto out;
		break;
	default:
		goto invalid;
	}
	if (informationElements) {
		if (informationElementsSize >= 0x10000 ||
		    (uintptr_t)(parameter->data + informationElementsSize) > end)
			goto noSpace;
		parameter->header.type[0] = 10;
		parameter->header.type[1] = 1;
		parameter->header.size[0] = informationElementsSize;
		parameter->header.size[1] = informationElementsSize >> 8;
		memcpy(parameter->data, informationElements, informationElementsSize);
		parameter = (WlanAssociationParameter *)
			(parameter->data + informationElementsSize);
	}
	scanChannel = (SceWlanBtRobinWlanScanChannelParameter *)parameter;
	if ((uintptr_t)(scanChannel->channels + 1) > end)
		goto noSpace;
	scanChannel->header.size[0] = 7;
	scanChannel->header.type[0] = 1;
	scanChannel->header.type[1] = 1;
	scanChannel->header.size[1] = 0;
	scanChannel->channels[0].radio = 0;
	scanChannel->channels[0].channel = descriptor->channel;
	scanChannel->channels[0].scanMode = !wlanIsActiveScanChannel(descriptor->channel);
	scanChannel->channels[0].minScanTime[0] = 0;
	scanChannel->channels[0].minScanTime[1] = 0;
	scanChannel->channels[0].maxScanTime[0] = 0;
	scanChannel->channels[0].maxScanTime[1] = 0;
	parameter = (WlanAssociationParameter *)(scanChannel->channels + 1);
	if (wlanBssHasWmm(descriptor)) {
		wmm = (WlanWmmParameter *)parameter;
		if ((uintptr_t)(wmm + 1) > end)
			goto noSpace;
		wmm->header.type[0] = 221;
		wmm->header.type[1] = 0;
		wmm->header.size[0] = 7;
		wmm->header.size[1] = 0;
		wmm->information = gWlanWmmInformation;
		parameter = (WlanAssociationParameter *)(wmm + 1);
	}
	if (highThroughput) {
		ht = (WlanHtCapabilitiesParameter *)parameter;
		if ((uintptr_t)(ht + 1) > end)
			goto noSpace;
		ht->header.type[0] = 45;
		ht->header.size[0] = 26;
		ht->header.type[1] = 0;
		ht->header.size[1] = 0;
		ht->capabilities[0] = 0x2c;
		ht->capabilities[1] = 0x11;
		ht->ampduParameters = 3;
		memcpy(ht->supportedMcs, gWlanSupportedMcs, sizeof(ht->supportedMcs));
		ht->extendedCapabilities[0] = 0;
		ht->extendedCapabilities[1] = 0;
		ht->transmitBeamforming[0] = 0;
		ht->transmitBeamforming[1] = 0;
		ht->transmitBeamforming[2] = 0;
		ht->transmitBeamforming[3] = 0;
		ht->antennaSelection = 0;
		parameter = (WlanAssociationParameter *)(ht + 1);
	}
	result = robinWlanSubmitAndWaitCommand(context, command, 0x12,
		(uint8_t *)parameter - (uint8_t *)request);
	if (result < 0)
		goto out;
	response = command->responsePayload;
	capabilities = response->capabilities[0] | response->capabilities[1] << 8;
	switch (capabilities) {
	case 0xfffc:
		status = response->status[0] | response->status[1] << 8;
		if (status == 1)
			result = 0x80411604;
		else if (status == 2)
			result = 0x80411605;
		else
			result = 0x8041160b;
		break;
	case 0xfffd:
		result = 0x80411603;
		break;
	case 0xfffe:
		result = 0x8041160a;
		break;
	case 0xffff:
		result = 0x80411609;
		break;
	default:
		status = response->status[0] | response->status[1] << 8;
		if (status) {
			if (status - 12 <= 1)
				result = 0x80411602;
			else
				result = 0x8041160c;
		} else {
			result = 0;
			*htEnabled = highThroughput;
		}
		break;
	}
	goto out;
invalid:
	result = 0x80418001;
	goto out;
noSpace:
	result = 0x80418005;
out:
	robinWlanReleaseCommand(context, command);
	return result;
}

int robinWlanRequestAddBlockAck(
	SceWlanBtRobinWlanContext *context, const SceNetEtherAddr *peer,
	uint16_t bufferSize, uint16_t tid, uint8_t policy, uint8_t amsdu,
	uint16_t timeout, uint16_t startingSequence,
	SceWlanBtRobinWlanCommand **output,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *))
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 14);
	SceWlanBtRobinWlanAddBlockAckMessage *request;
	uint8_t token, nextToken;
	uint16_t parameters;
	int result;

	if (!command)
		return 0x80418005;
	token = gBlockAckDialogToken;
	command->callback = callback;
	nextToken = token + 1;
	if (!token) {
		nextToken = 2;
		token = 1;
	}
	request = command->requestPayload;
	gBlockAckDialogToken = nextToken;
	parameters = (policy & 1) << 1 | tid << 2 | (amsdu & 1) | bufferSize << 6;
	request->peer = *peer;
	request->dialogToken = token;
	request->timeout[0] = timeout;
	request->parameters[0] = parameters;
	request->startingSequence[0] = startingSequence;
	request->parameters[1] = parameters >> 8;
	request->timeout[1] = timeout >> 8;
	request->startingSequence[1] = startingSequence >> 8;
	result = robinWlanSubmitCommand(context, command, 206, sizeof(*request));
	if (result >= 0)
		*output = command;
	return result;
}

int robinWlanReadAddBlockAckResult(const SceWlanBtRobinWlanCommand *command,
				 uint8_t *result)
{
	const SceWlanBtRobinWlanAddBlockAckMessage *response = command->responsePayload;

	*result = response->result;
	return 0;
}

int robinWlanSetHtTransmit(SceWlanBtRobinWlanContext *context,
			   uint16_t capabilities, uint16_t information)
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 6);
	SceWlanBtRobinWlanHtTransmitRequest *request;
	int result;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	request->capabilities[1] = capabilities >> 8;
	request->information[1] = information >> 8;
	request->capabilities[0] = capabilities;
	request->information[0] = information;
	request->action[0] = 1;
	request->action[1] = 0;
	result = robinWlanSubmitAndWaitCommand(context, command, 205, sizeof(*request));
	robinWlanReleaseCommand(context, command);
	return result;
}

int robinWlanRequestDeleteBlockAck(
	SceWlanBtRobinWlanContext *context, uint8_t result, const SceNetEtherAddr *peer,
	uint16_t parameters, uint16_t reason,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *))
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 12);
	SceWlanBtRobinWlanDeleteBlockAckMessage *request;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	command->callback = callback;
	request->result = result;
	request->peer = *peer;
	request->parameters[0] = parameters;
	request->reason[0] = reason;
	request->parameters[1] = parameters >> 8;
	request->reason[1] = reason >> 8;
	return robinWlanSubmitCommand(context, command, 208, sizeof(*request));
}

int robinWlanSetTransmitBuffer(
	SceWlanBtRobinWlanContext *context, uint16_t size,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *))
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 8);
	SceWlanBtRobinWlanTransmitBufferRequest *request;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	command->callback = callback;
	request->size[1] = size >> 8;
	request->action[0] = 1;
	request->size[0] = size;
	request->action[1] = 0;
	request->endPort[0] = 0;
	request->endPort[1] = 0;
	return robinWlanSubmitCommand(context, command, 217, sizeof(*request));
}

int robinWlanSetAmsduAggregation(
	SceWlanBtRobinWlanContext *context, int enabled, uint16_t currentBufferSize,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *))
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 6);
	SceWlanBtRobinWlanAmsduRequest *request;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	command->callback = callback;
	request->action[0] = 1;
	request->action[1] = 0;
	if (enabled) {
		request->enabled[0] = 1;
		request->enabled[1] = 0;
	} else {
		request->enabled[0] = 0;
		request->enabled[1] = 0;
	}
	request->currentBufferSize[0] = currentBufferSize;
	request->currentBufferSize[1] = currentBufferSize >> 8;
	return robinWlanSubmitCommand(context, command, 223, sizeof(*request));
}

int robinWlanRequestCoexistence(
	SceWlanBtRobinWlanContext *context, uint8_t mode,
	SceWlanBtRobinWlanCommand **output,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *))
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 36);
	SceWlanBtRobinWlanCoexistenceRequest *request;
	SceWlanBtRobinWlanCoexistenceParameter *parameter;
	int result;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	command->callback = callback;
	request->action[0] = 1;
	request->action[1] = 0;
	parameter = (SceWlanBtRobinWlanCoexistenceParameter *)request->parameters;
	if ((uintptr_t)request + sizeof(*request) <
	    (uintptr_t)parameter + sizeof(*parameter))
		return 0x80418005;
	parameter->header.type[0] = 96;
	parameter->header.size[0] = sizeof(parameter->mode);
	parameter->header.type[1] = 1;
	parameter->header.size[1] = 0;
	parameter->mode[0] = mode;
	result = robinWlanSubmitCommand(context, command, 224, 12);
	if (result >= 0)
		*output = command;
	return result;
}

int robinWlanSetTransmitRate(SceWlanBtRobinWlanContext *context, unsigned int rate)
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 2048);
	SceWlanBtRobinWlanTransmitRateRequest *request;
	SceWlanBtRobinWlanHtRateBitmap htRates = { { 0 } };
	SceWlanBtRobinWlanRateScopeParameter *scope;
	SceWlanBtRobinWlanRateDropParameter *drop;
	SceWlanBtRobinWlanRateOptionParameter *option;
	uint8_t *parameters;
	uintptr_t end;
	uint8_t dsssRates = 1, ofdmRates = 0;
	int result;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	request->action[0] = 1;
	parameters = request->parameters;
	request->action[1] = 0;
	end = (uintptr_t)request + sizeof(*request);
	request->configurationIndex[0] = 0;
	request->configurationIndex[1] = 0;
	switch (rate) {
	case 0: dsssRates = 1; break;
	case 1: dsssRates = 2; break;
	case 2: dsssRates = 4; break;
	case 3: dsssRates = 8; break;
	case 4: dsssRates = 0; ofdmRates = 1; break;
	case 5: dsssRates = 0; ofdmRates = 2; break;
	case 6: dsssRates = 0; ofdmRates = 4; break;
	case 7: dsssRates = 0; ofdmRates = 8; break;
	case 8: dsssRates = 0; ofdmRates = 16; break;
	case 9: dsssRates = 0; ofdmRates = 32; break;
	case 10: dsssRates = 0; ofdmRates = 64; break;
	case 11: dsssRates = 0; ofdmRates = 128; break;
	case 12: dsssRates = 0; htRates.rates[0] = 1; break;
	case 13: dsssRates = 0; htRates.rates[0] = 2; break;
	case 14: dsssRates = 0; htRates.rates[0] = 4; break;
	case 15: dsssRates = 0; htRates.rates[0] = 8; break;
	case 16: dsssRates = 0; htRates.rates[0] = 16; break;
	case 17: dsssRates = 0; htRates.rates[0] = 32; break;
	case 18: dsssRates = 0; htRates.rates[0] = 64; break;
	case 19: dsssRates = 0; htRates.rates[0] = 128; break;
	case 44:
		result = 0x80418006;
		goto release;
	case 256:
		htRates = (SceWlanBtRobinWlanHtRateBitmap) {
			{ 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0x3fff }
		};
		dsssRates = 15;
		ofdmRates = 255;
		goto addOption;
	case 257:
		htRates.rates[0] = 254;
		dsssRates = 8;
		ofdmRates = 252;
addOption:
		option = (SceWlanBtRobinWlanRateOptionParameter *)parameters;
		if (end < (uintptr_t)option + sizeof(*option)) {
			result = 0x80418005;
			goto release;
		}
		option->header.type[0] = 98;
		option->header.type[1] = 1;
		option->header.size[0] = sizeof(option->value);
		option->header.size[1] = 0;
		option->value[0] = rate == 256 ? 0 : 22;
		option->value[1] = 0;
		parameters += sizeof(*option);
		break;
	default:
		result = 0x80418001;
		goto release;
	}
	scope = (SceWlanBtRobinWlanRateScopeParameter *)parameters;
	if (end < (uintptr_t)scope + sizeof(*scope)) {
		result = 0x80418005;
		goto release;
	}
	scope->dsssRates[0] = dsssRates;
	scope->header.size[0] = 20;
	scope->header.type[0] = 83;
	scope->header.type[1] = 1;
	scope->header.size[1] = 0;
	scope->dsssRates[1] = 0;
	scope->ofdmRates[0] = ofdmRates;
	scope->ofdmRates[1] = 0;
	scope->htRates = htRates;
	parameters += sizeof(*scope);
	drop = (SceWlanBtRobinWlanRateDropParameter *)parameters;
	if (end < (uintptr_t)drop + sizeof(*drop)) {
		result = 0x80418005;
		goto release;
	}
	drop->header.type[0] = 81;
	drop->header.type[1] = 1;
	drop->header.size[0] = sizeof(drop->mode);
	drop->header.size[1] = 0;
	drop->mode[0] = 0;
	drop->mode[1] = 0;
	drop->mode[2] = 0;
	drop->mode[3] = 0;
	parameters += sizeof(*drop);
	result = robinWlanSubmitAndWaitCommand(context, command, 214,
					     parameters - (uint8_t *)request);
release:
	robinWlanReleaseCommand(context, command);
	return result;
}

int robinWlanEnableAutoDeepSleep(
	SceWlanBtRobinWlanContext *context, uint16_t timeout,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *))
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 10);
	SceWlanBtRobinWlanAutoDeepSleepRequest *request;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	command->callback = callback;
	request->powerSave.action[0] = 255;
	request->powerSave.action[1] = 0;
	request->powerSave.powerSaveBitmap[0] = 1;
	request->powerSave.powerSaveBitmap[1] = 0;
	request->parameter.header.type[1] = 1;
	request->parameter.header.type[0] = 113;
	request->parameter.header.size[0] = sizeof(request->parameter.timeout);
	request->parameter.header.size[1] = 0;
	request->parameter.timeout[0] = timeout;
	request->parameter.timeout[1] = timeout >> 8;
	return robinWlanSubmitCommand(context, command, 228, sizeof(*request));
}

int robinWlanRequestDisableStationPowerSave(
	SceWlanBtRobinWlanContext *context, SceWlanBtRobinWlanCommand **output,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *))
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 4);
	SceWlanBtRobinWlanPowerSaveRequest *request;
	int result;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	command->callback = callback;
	request->action[0] = 254;
	request->powerSaveBitmap[0] = 16;
	request->action[1] = 0;
	request->powerSaveBitmap[1] = 0;
	result = robinWlanSubmitCommand(context, command, 228, sizeof(*request));
	if (result >= 0)
		*output = command;
	return result;
}

int robinWlanDisableStationPowerSave(SceWlanBtRobinWlanContext *context)
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 4);
	SceWlanBtRobinWlanPowerSaveRequest *request;
	int result;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	command->callback = NULL;
	request->action[1] = 0;
	request->action[0] = 254;
	request->powerSaveBitmap[0] = 16;
	request->powerSaveBitmap[1] = 0;
	result = robinWlanSubmitCommand(context, command, 228, sizeof(*request));
	if (result >= 0) {
		result = robinWlanWaitCommand(context, command);
		robinWlanReleaseCommand(context, command);
	}
	return result;
}

int robinWlanRequestEnableStationPowerSave(
	SceWlanBtRobinWlanContext *context, uint16_t nullPacketInterval,
	uint16_t beaconMissTimeout, uint16_t adhocWakePeriod, uint16_t mode,
	SceWlanBtRobinWlanCommand **output,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *))
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 22);
	SceWlanBtRobinWlanEnableStationPowerSaveRequest *request;
	int result;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	command->callback = callback;
	request->powerSave.powerSaveBitmap[0] = 16;
	request->powerSave.action[1] = 0;
	request->powerSave.powerSaveBitmap[1] = 0;
	request->parameter.header.size[1] = 0;
	request->parameter.multipleDtims[1] = 0;
	request->parameter.localListenInterval[0] = 0;
	request->parameter.localListenInterval[1] = 0;
	request->parameter.header.type[0] = 114;
	request->parameter.header.type[1] = 1;
	request->parameter.nullPacketInterval[0] = nullPacketInterval;
	request->parameter.multipleDtims[0] = 1;
	request->parameter.header.size[0] = 14;
	request->parameter.nullPacketInterval[1] = nullPacketInterval >> 8;
	request->parameter.beaconMissTimeout[1] = beaconMissTimeout >> 8;
	request->powerSave.action[0] = 255;
	request->parameter.beaconMissTimeout[0] = beaconMissTimeout;
	request->parameter.adhocWakePeriod[0] = adhocWakePeriod;
	request->parameter.mode[0] = mode;
	request->parameter.adhocWakePeriod[1] = adhocWakePeriod >> 8;
	request->parameter.mode[1] = mode >> 8;
	request->parameter.delayToPowerSave[0] = 1000;
	request->parameter.delayToPowerSave[1] = 1000 >> 8;
	result = robinWlanSubmitCommand(context, command, 228, sizeof(*request));
	if (result >= 0)
		*output = command;
	return result;
}

int robinWlanEnableStationPowerSave(
	SceWlanBtRobinWlanContext *context, uint16_t nullPacketInterval,
	uint16_t beaconMissTimeout, uint16_t adhocWakePeriod, uint16_t mode)
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 22);
	SceWlanBtRobinWlanEnableStationPowerSaveRequest *request;
	int result;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	command->callback = NULL;
	request->powerSave.action[1] = 0;
	request->powerSave.powerSaveBitmap[1] = 0;
	request->parameter.header.size[1] = 0;
	request->parameter.multipleDtims[1] = 0;
	request->parameter.localListenInterval[0] = 0;
	request->parameter.localListenInterval[1] = 0;
	request->powerSave.action[0] = 255;
	request->parameter.header.type[0] = 114;
	request->parameter.adhocWakePeriod[1] = adhocWakePeriod >> 8;
	request->powerSave.powerSaveBitmap[0] = 16;
	request->parameter.header.type[1] = 1;
	request->parameter.multipleDtims[0] = 1;
	request->parameter.header.size[0] = 14;
	request->parameter.nullPacketInterval[1] = nullPacketInterval >> 8;
	request->parameter.beaconMissTimeout[1] = beaconMissTimeout >> 8;
	request->parameter.nullPacketInterval[0] = nullPacketInterval;
	request->parameter.beaconMissTimeout[0] = beaconMissTimeout;
	request->parameter.adhocWakePeriod[0] = adhocWakePeriod;
	request->parameter.mode[0] = mode;
	request->parameter.mode[1] = mode >> 8;
	request->parameter.delayToPowerSave[0] = 1000;
	request->parameter.delayToPowerSave[1] = 1000 >> 8;
	result = robinWlanSubmitCommand(context, command, 228, sizeof(*request));
	if (result >= 0) {
		result = robinWlanWaitCommand(context, command);
		robinWlanReleaseCommand(context, command);
	}
	return result;
}

int robinWlanConfirmSleep(
	SceWlanBtRobinWlanContext *context,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *))
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 4);
	SceWlanBtRobinWlanSleepConfirmRequest *request;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	command->callback = callback;
	command->flags = 1;
	request->responseControl[0] = 1;
	request->action[0] = 5;
	request->action[1] = 0;
	request->responseControl[1] = 0;
	return robinWlanSubmitCommand(context, command, 228, sizeof(*request));
}

int robinWlanConfigureHostSleep(
	SceWlanBtRobinWlanContext *context, uint32_t conditions, uint8_t gpio, uint8_t gap,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *))
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 8);
	SceWlanBtRobinWlanHostSleepRequest *request;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	command->callback = callback;
	request->action[0] = 1;
	request->action[1] = 0;
	request->conditions[0] = conditions;
	request->conditions[1] = conditions >> 8;
	request->conditions[2] = conditions >> 16;
	request->conditions[3] = conditions >> 24;
	request->gpio = gpio;
	request->gap = gap;
	return robinWlanSubmitCommand(context, command, 229, sizeof(*request));
}

int robinWlanSetManagementInformationElements(
	SceWlanBtRobinWlanContext *context, const void *informationElements, size_t size)
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 268);
	SceWlanBtRobinWlanManagementInformationElementRequest *request;
	unsigned int parameterSize = size + 6;
	int result;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	request->action[0] = 1;
	request->action[1] = 0;
	if (parameterSize >= 65536 ||
	    (uintptr_t)request + sizeof(*request) <
	    (uintptr_t)&request->element + parameterSize) {
		result = 0x80418005;
	} else {
		request->header.type[0] = 105;
		request->header.type[1] = 1;
		request->header.size[0] = parameterSize;
		request->element.managementSubtypeMask[1] = 1;
		request->header.size[1] = parameterSize >> 8;
		request->element.index[0] = 0;
		request->element.index[1] = 0;
		request->element.managementSubtypeMask[0] = 0;
		request->element.size[0] = size;
		request->element.size[1] = size >> 8;
		memcpy(request->element.data, informationElements, size);
		result = robinWlanSubmitAndWaitCommand(context, command, 242, size + 12);
	}
	robinWlanReleaseCommand(context, command);
	return result;
}

int robinWlanSetHostSleepData(
	SceWlanBtRobinWlanContext *context, const void *data, size_t size,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *))
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 18);
	SceWlanBtRobinWlanHostSleepDataRequest *request;
	unsigned int parameterSize = size + 2;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	command->callback = callback;
	request->action[1] = 0;
	request->action[0] = 1;
	if (parameterSize >= 65536 ||
	    (uintptr_t)request + sizeof(*request) <
	    (uintptr_t)&request->size + parameterSize)
		return 0x80418005;
	request->header.type[0] = 116;
	request->header.type[1] = 1;
	request->header.size[0] = parameterSize;
	request->header.size[1] = parameterSize >> 8;
	request->size[0] = size;
	request->size[1] = size >> 8;
	if (data && size)
		memcpy(request->data, data, size);
	return robinWlanSubmitCommand(context, command, 245, size + 8);
}

int robinWlanSetHostSleepAddressData(
	SceWlanBtRobinWlanContext *context, const SceNetEtherAddr *address,
	const void *data, size_t size,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *))
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 269);
	SceWlanBtRobinWlanHostSleepAddressDataRequest *request;
	unsigned int parameterSize;

	if (!command)
		return 0x80418005;
	parameterSize = address ? sizeof(*address) : 0;
	request = command->requestPayload;
	parameterSize += size;
	command->callback = callback;
	request->action[1] = 0;
	request->action[0] = 1;
	if (parameterSize >= 65536 ||
	    (uintptr_t)request + sizeof(*request) <
	    (uintptr_t)&request->address + parameterSize)
		return 0x80418005;
	request->header.type[0] = 115;
	request->header.type[1] = 1;
	request->header.size[0] = parameterSize;
	request->header.size[1] = parameterSize >> 8;
	if (address)
		request->address = *address;
	if (data && size)
		memcpy(request->data, data, size);
	return robinWlanSubmitCommand(context, command, 245, parameterSize + 6);
}

int robinWlanClearHostSleepAddressData(
	SceWlanBtRobinWlanContext *context,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *))
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 6);
	SceWlanBtRobinWlanHostSleepAddressDataControlRequest *request;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	command->callback = callback;
	request->header.type[0] = 115;
	request->action[0] = 4;
	request->header.type[1] = 1;
	request->action[1] = 0;
	request->header.size[0] = 0;
	request->header.size[1] = 0;
	return robinWlanSubmitCommand(context, command, 245, sizeof(*request));
}

int robinWlanRequestHostSleepAddressData(
	SceWlanBtRobinWlanContext *context,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *))
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 6);
	SceWlanBtRobinWlanHostSleepAddressDataControlRequest *request;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	command->callback = callback;
	request->header.type[0] = 115;
	request->header.type[1] = 1;
	request->action[0] = 0;
	request->action[1] = 0;
	request->header.size[0] = 0;
	request->header.size[1] = 0;
	return robinWlanSubmitCommand(context, command, 245, sizeof(*request));
}

int robinWlanReadHostSleepAddresses(SceWlanBtRobinWlanContext *context,
				  const void *buffer, unsigned int size)
{
	const WlanHostSleepPeerResponse *response = buffer;
	const SceWlanBtRobinWlanTlvHeader *header = (const void *)response->records;
	const uint8_t *data = (const void *)(header + 1);
	uintptr_t end = (uintptr_t)buffer + size;
	unsigned int length = header->size[0] | header->size[1] << 8;
	unsigned int count = 0;
	WlanHostSleepPeer *peer, *nextPeer;

	if (end < (uintptr_t)data + length)
		goto tooSmall;
	if ((int16_t)(header->type[0] | header->type[1] << 8) != 371)
		goto invalid;
	peer = &context->peers[0];
	nextPeer = &context->peers[1];
	for (;;) {
		if (length) {
			if (end < (uintptr_t)data + sizeof(peer->address))
				goto invalid;
			if (peer)
				__builtin_memcpy(&peer->address, data, sizeof(peer->address));
			if (length - sizeof(peer->address) > sizeof(peer->data))
				goto invalid;
			data += length;
		}
		++count;
		if (end <= (uintptr_t)data || count == 16)
			break;
		peer = nextPeer;
		header = (const void *)data;
		length = header->size[0] | header->size[1] << 8;
		data = (const void *)(header + 1);
		if (end < (uintptr_t)data + length)
			goto tooSmall;
		++nextPeer;
		if ((int16_t)(header->type[0] | header->type[1] << 8) != 371)
			goto invalid;
	}
	context->peerCount = count;
	return 0;
tooSmall:
	context->peerCount = count;
	return 0x80418005;
invalid:
	context->peerCount = count;
	return 0x80418001;
}

int robinWlanRequestHostSleepDataForAddress(
	SceWlanBtRobinWlanContext *context, const SceNetEtherAddr *address,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *))
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 12);
	SceWlanBtRobinWlanHostSleepAddressQueryRequest *request;
	unsigned int parameterSize = 0;
	unsigned int requestSize = 6;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	command->callback = callback;
	request->control.action[0] = 0;
	request->control.action[1] = 0;
	if (address) {
		parameterSize = sizeof(*address);
		requestSize = sizeof(*request);
	}
	if ((uintptr_t)request + sizeof(*request) <
	    (uintptr_t)&request->address + parameterSize)
		return 0x80418005;
	request->control.header.type[0] = 115;
	request->control.header.type[1] = 1;
	request->control.header.size[0] = parameterSize;
	request->control.header.size[1] = 0;
	if (address)
		request->address = *address;
	return robinWlanSubmitCommand(context, command, 245, requestSize);
}

int robinWlanReadHostSleepPeerData(SceWlanBtRobinWlanContext *context,
				 const void *buffer, unsigned int size, int index)
{
	const WlanHostSleepPeerDataResponse *response = buffer;
	const SceWlanBtRobinWlanTlvHeader *header = &response->query.control.header;
	uintptr_t end = (uintptr_t)buffer + size;
	unsigned int length = header->size[0] | header->size[1] << 8;
	uint8_t *destination;

	if (end < (uintptr_t)&response->query.address + length)
		return 0x80418005;
	if ((header->type[0] | header->type[1] << 8) == 371) {
		if (!length)
			return 0;
		if (end >= (uintptr_t)response->data &&
		    length - sizeof(response->query.address) <= sizeof(context->peers[0].data)) {
			destination = context->peers[index].data;
			if (!destination)
				return 0;
			memcpy(destination, response->data, length - sizeof(response->query.address));
			return 0;
		}
	}
	return 0x80418001;
}

int robinWlanRequestBssMode(
	SceWlanBtRobinWlanContext *context, uint8_t connectionType,
	SceWlanBtRobinWlanCommand **output,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *))
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 1);
	uint8_t *request;
	int result;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	command->callback = callback;
	*request = connectionType;
	result = robinWlanSubmitCommand(context, command, 247, sizeof(*request));
	if (result >= 0)
		*output = command;
	return result;
}

int robinWlanSetBssMode(SceWlanBtRobinWlanContext *context, uint8_t connectionType)
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 1);
	uint8_t *request;
	int result;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	command->callback = NULL;
	*request = connectionType;
	result = robinWlanSubmitCommand(context, command, 247, sizeof(*request));
	if (result >= 0) {
		result = robinWlanWaitCommand(context, command);
		robinWlanReleaseCommand(context, command);
	}
	return result;
}

int robinWlanSetEnergyDetectionMacOffset(
	SceWlanBtRobinWlanContext *context, int16_t offset)
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 4);
	SceWlanBtRobinWlanEnergyDetectionMacRequest *request;
	int result;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	command->callback = NULL;
	request->enabled[1] = 0;
	request->offset[0] = offset;
	request->offset[1] = (uint16_t)offset >> 8;
	request->enabled[0] = 1;
	result = robinWlanSubmitCommand(context, command, 292, sizeof(*request));
	if (result >= 0) {
		result = robinWlanWaitCommand(context, command);
		robinWlanReleaseCommand(context, command);
	}
	return result;
}

int robinWlanRequestDisableEnergyDetectionMac(
	SceWlanBtRobinWlanContext *context, SceWlanBtRobinWlanCommand **output,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *))
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 4);
	SceWlanBtRobinWlanEnergyDetectionMacRequest *request;
	int result;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	command->callback = callback;
	request->enabled[0] = 0;
	request->enabled[1] = 0;
	request->offset[0] = 0;
	request->offset[1] = 0;
	result = robinWlanSubmitCommand(context, command, 292, sizeof(*request));
	if (result >= 0)
		*output = command;
	return result;
}

int robinWlanDisableEnergyDetectionMac(SceWlanBtRobinWlanContext *context)
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 4);
	SceWlanBtRobinWlanEnergyDetectionMacRequest *request;
	int result;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	command->callback = NULL;
	request->enabled[0] = 0;
	request->enabled[1] = 0;
	request->offset[0] = 0;
	request->offset[1] = 0;
	result = robinWlanSubmitCommand(context, command, 292, sizeof(*request));
	if (result >= 0) {
		result = robinWlanWaitCommand(context, command);
		robinWlanReleaseCommand(context, command);
	}
	return result;
}

int robinWlanRespondAddBlockAck(
	SceWlanBtRobinWlanContext *context,
	const SceWlanBtRobinWlanAddBlockAckMessage *input,
	int (*callback)(SceWlanBtRobinWlanContext *, SceWlanBtRobinWlanCommand *))
{
	unsigned int parameters = input->parameters[0] | input->parameters[1] << 8;
	SceNetEtherAddr peer = input->peer;
	unsigned int bufferSize = parameters >> 6;
	unsigned int tid = parameters >> 2 & 15;
	uint16_t startingSequence = input->startingSequence[0] |
		input->startingSequence[1] << 8;
	uint8_t dialogToken = input->dialogToken;
	SceWlanBtRobinWlanCommand *command;
	SceWlanBtRobinWlanAddBlockAckResponse *response;

	if (bufferSize >= 16)
		bufferSize = 16;
	command = robinWlanAllocateCommand(context, 16);
	if (!command)
		return 0x80418005;
	parameters = tid << 2 | bufferSize << 6 | 2;
	response = command->requestPayload;
	command->callback = callback;
	response->result = 0;
	response->peer = peer;
	response->status[0] = 0;
	response->status[1] = 0;
	response->timeout[0] = 0;
	response->timeout[1] = 0;
	response->dialogToken = dialogToken;
	response->parameters[0] = parameters;
	response->parameters[1] = parameters >> 8;
	response->startingSequence[1] = startingSequence >> 8;
	response->startingSequence[0] = startingSequence;
	return robinWlanSubmitCommand(context, command, 207, sizeof(*response));
}

void robinWlanReadDeleteBlockAckReason(
	const SceWlanBtRobinWlanDeleteBlockAckMessage *message,
	uint8_t *initiator, uint16_t *reason)
{
	*initiator = (message->parameters[1] >> 3) & 1;
	*reason = message->reason[0] | message->reason[1] << 8;
}

void robinWlanReadDeleteBlockAckResult(
	const SceWlanBtRobinWlanDeleteBlockAckMessage *message,
	SceNetEtherAddr *peer, uint8_t *result, uint8_t *parameterFlags)
{
	*peer = message->peer;
	*result = message->result;
	*parameterFlags = message->parameters[0];
}

extern const uint8_t gBssTypeMapping[3];
int wlanIsSupportedChannel(int channel);
int wlanBuildScanResult(const WlanScanRecord *record, unsigned int recordCount,
	const uint8_t *recordEnd, WlanScanResult *result, unsigned int capacity,
	const unsigned int *mask);

int robinWlanScan(SceWlanBtRobinWlanContext *context, WlanScanConfig *configuration,
		 unsigned int capacity)
{
	uint8_t rates[12];
	uint32_t maskCopy[8];
	const unsigned int *mask = NULL;
	SceWlanBtRobinWlanCommand *command;
	WlanScanRequest *request;
	WlanScanResponse *response;
	SceWlanBtRobinWlanTlvHeader *current;
	WlanScanRatesParameter *rateParameter;
	SceWlanBtRobinWlanScanChannelParameter *channelParameter;
	WlanScanProbeCountParameter *probeParameter;
	uintptr_t end;
	unsigned int type, count, size, channelCount;
	uint16_t scanMode, minScanTime, maxScanTime;
	const uint8_t *recordEnd;
	int result;

	memcpy(rates, gWlanScanRates, sizeof(rates));
	command = robinWlanAllocateCommand(context, 2048);
	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	end = (uintptr_t)request + 2048;
	current = (SceWlanBtRobinWlanTlvHeader *)request->parameters;
	type = (uint8_t)(configuration->bssType - 1);
	if (configuration->ssidCount > 10 || type > 2 || !gBssTypeMapping[type])
		goto invalid;
	request->bssType = gBssTypeMapping[type];
	request->bssid = configuration->bssid;
	for (count = 0; count < configuration->ssidCount && count != 10; ++count) {
		size = configuration->ssids[count].size;
		if (size > 32)
			goto invalid;
		if (configuration->flags & 4) {
			SceWlanBtRobinWlanWildcardSsidParameter *parameter =
				(SceWlanBtRobinWlanWildcardSsidParameter *)current;
			unsigned int payloadSize = size + 1;

			if (payloadSize >= 0x10000 || end < (uintptr_t)(parameter + 1) + size)
				goto noSpace;
			parameter->header.type[0] = 18;
			parameter->header.type[1] = 1;
			parameter->header.size[0] = payloadSize;
			parameter->header.size[1] = payloadSize >> 8;
			parameter->maxLength = 255;
			current = (SceWlanBtRobinWlanTlvHeader *)(parameter->ssid + size);
			memcpy(parameter->ssid, configuration->ssids[count].ssid, size);
		} else {
			SceWlanBtRobinWlanScanSsidParameter *parameter =
				(SceWlanBtRobinWlanScanSsidParameter *)current;

			if (end < (uintptr_t)(parameter + 1) + size)
				goto noSpace;
			parameter->header.type[0] = 0;
			parameter->header.type[1] = 0;
			parameter->header.size[0] = size;
			parameter->header.size[1] = 0;
			current = (SceWlanBtRobinWlanTlvHeader *)(parameter->ssid + size);
			memcpy(parameter->ssid, configuration->ssids[count].ssid, size);
		}
	}
	rateParameter = (WlanScanRatesParameter *)current;
	if (end < (uintptr_t)(rateParameter + 1))
		goto noSpace;
	rateParameter->header.type[0] = 1;
	rateParameter->header.type[1] = 0;
	rateParameter->header.size[0] = sizeof(rates);
	rateParameter->header.size[1] = 0;
	memcpy(rateParameter->rates, rates, sizeof(rates));
	scanMode = configuration->scanMode;
	channelCount = 0;
	while (channelCount != 14 && configuration->channels[channelCount] &&
	       wlanIsSupportedChannel(configuration->channels[channelCount]))
		++channelCount;
	if (!channelCount)
		goto invalid;
	minScanTime = configuration->minScanTime;
	maxScanTime = configuration->maxScanTime;
	channelParameter = (SceWlanBtRobinWlanScanChannelParameter *)(rateParameter + 1);
	size = sizeof(SceWlanBtRobinWlanScanChannel) * channelCount;
	if (end < (uintptr_t)(channelParameter + 1) + size)
		goto noSpace;
	channelParameter->header.type[0] = 1;
	channelParameter->header.type[1] = 1;
	channelParameter->header.size[0] = size;
	channelParameter->header.size[1] = 0;
	for (count = 0; count < channelCount; ++count) {
		SceWlanBtRobinWlanScanChannel *channel = &channelParameter->channels[count];

		channel->radio = 0;
		channel->channel = configuration->channels[count];
		channel->scanMode = !wlanIsActiveScanChannel(configuration->channels[count]) ||
			scanMode == 2;
		channel->minScanTime[0] = minScanTime;
		channel->minScanTime[1] = minScanTime >> 8;
		channel->maxScanTime[0] = maxScanTime;
		channel->maxScanTime[1] = maxScanTime >> 8;
	}
	probeParameter = (WlanScanProbeCountParameter *)(channelParameter->channels + channelCount);
	if (end < (uintptr_t)(probeParameter + 1))
		goto noSpace;
	probeParameter->header.type[0] = 2;
	probeParameter->header.type[1] = 1;
	probeParameter->header.size[0] = 2;
	probeParameter->header.size[1] = 0;
	probeParameter->count[0] = 1;
	probeParameter->count[1] = 0;
	current = (SceWlanBtRobinWlanTlvHeader *)(probeParameter + 1);
	if ((configuration->flags & 1) && configuration->vendorInformationElementsSize) {
		WlanScanInformationElementsParameter *parameter =
			(WlanScanInformationElementsParameter *)current;

		size = configuration->vendorInformationElementsSize;
		if (size > 1024)
			goto invalid;
		if (end < (uintptr_t)(parameter + 1) + size)
			goto noSpace;
		parameter->header.type[0] = 10;
		parameter->header.type[1] = 1;
		parameter->header.size[0] = size;
		parameter->header.size[1] = size >> 8;
		memcpy(parameter->informationElements, configuration->vendorInformationElements, size);
		current = (SceWlanBtRobinWlanTlvHeader *)(parameter->informationElements + size);
	}
	result = robinWlanSubmitAndWaitCommand(context, command, 6,
		(uint8_t *)current - (uint8_t *)request);
	if (result < 0)
		goto out;
	response = command->responsePayload;
	if (configuration->flags & 2) {
		memcpy(maskCopy, configuration->informationElementMask, sizeof(maskCopy));
		mask = maskCopy;
	}
	recordEnd = response->records +
		(response->bssDescriptorSize[0] | response->bssDescriptorSize[1] << 8);
	if ((uintptr_t)recordEnd > (uintptr_t)response + command->responseSize)
		goto invalid;
	result = wlanBuildScanResult((const WlanScanRecord *)response->records,
		response->bssCount, recordEnd, (WlanScanResult *)configuration, capacity, mask);
	goto out;
invalid:
	result = 0x80418001;
	goto out;
noSpace:
	result = 0x80418005;
out:
	robinWlanReleaseCommand(context, command);
	return result;
}
