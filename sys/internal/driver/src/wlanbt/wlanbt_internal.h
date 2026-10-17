#ifndef WLANBT_INTERNAL_H
#define WLANBT_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <psp2common/net.h>
#include <psp2kern/kernel/threadmgr/fast_mutex.h>
#include <psp2kern/kernel/proc_event.h>
#include <net_ps/netdrv.h>

#include <wlanbt/wlanbt.h>
struct SdifSdioContext;

typedef struct WlanInformationElement {
	uint8_t id;
	uint8_t length;
	uint8_t data[];
} WlanInformationElement;

_Static_assert(sizeof(WlanInformationElement) == 2, "information element header");
_Static_assert(offsetof(WlanInformationElement, data) == 2, "information element payload");

typedef struct WlanSecurityInfo {
	uint32_t enabled;
	uint8_t groupCipher;
	uint8_t pairwiseCipher;
	uint8_t keyManagement;
	uint8_t reserved[5];
} WlanSecurityInfo;

_Static_assert(sizeof(WlanSecurityInfo) == 12, "WLAN security information size");
_Static_assert(offsetof(WlanSecurityInfo, groupCipher) == 4, "group cipher mask");
_Static_assert(offsetof(WlanSecurityInfo, pairwiseCipher) == 5, "pairwise cipher mask");
_Static_assert(offsetof(WlanSecurityInfo, keyManagement) == 6, "key management mask");

typedef struct WlanHtInfo {
	uint32_t enabled;
	uint8_t reserved[12];
} WlanHtInfo;

typedef struct WlanBssDesc {
	uint16_t size;
	uint16_t informationElementsSize;
	uint32_t ssidSize;
	uint8_t ssid[32];
	SceNetEtherAddr bssid;
	uint8_t channel;
	uint8_t bssMode;
	uint16_t capabilities;
	uint16_t rssi;
	uint16_t signalQuality;
	uint16_t beaconInterval;
	uint16_t dtimPeriod;
	uint16_t atimWindow;
	uint8_t supportedRates[16];
	uint8_t extendedRates[16];
	WlanSecurityInfo wpa;
	WlanSecurityInfo rsn;
	WlanHtInfo ht;
	uint8_t informationElements[];
} WlanBssDesc;

_Static_assert(sizeof(WlanHtInfo) == 16, "WLAN HT information size");
_Static_assert(sizeof(WlanBssDesc) == 132, "WLAN BSS descriptor size");
_Static_assert(offsetof(WlanBssDesc, ssid) == 8, "BSS SSID");
_Static_assert(offsetof(WlanBssDesc, bssid) == 40, "BSS address");
_Static_assert(offsetof(WlanBssDesc, channel) == 46, "BSS channel");
_Static_assert(offsetof(WlanBssDesc, capabilities) == 48, "BSS capabilities");
_Static_assert(offsetof(WlanBssDesc, dtimPeriod) == 56, "BSS DTIM period");
_Static_assert(offsetof(WlanBssDesc, atimWindow) == 58, "BSS ATIM window");
_Static_assert(offsetof(WlanBssDesc, supportedRates) == 60, "BSS supported rates");
_Static_assert(offsetof(WlanBssDesc, extendedRates) == 76, "BSS extended rates");
_Static_assert(offsetof(WlanBssDesc, wpa) == 92, "BSS WPA information");
_Static_assert(offsetof(WlanBssDesc, rsn) == 104, "BSS RSN information");
_Static_assert(offsetof(WlanBssDesc, ht) == 116, "BSS HT information");
_Static_assert(offsetof(WlanBssDesc, informationElements) == 132, "BSS IE data");

/* Marvell scan-response record; the multibyte wire fields are little endian. */
typedef struct WlanScanRecord {
	uint8_t ieLength[2];
	SceNetEtherAddr bssid;
	uint8_t rssi;
	uint8_t timestamp[8];
	uint8_t beaconInterval[2];
	uint8_t capabilities[2];
	uint8_t informationElements[];
} WlanScanRecord;

_Static_assert(sizeof(WlanScanRecord) == 21, "scan record fixed header");
_Static_assert(offsetof(WlanScanRecord, bssid) == 2, "scan record BSSID");
_Static_assert(offsetof(WlanScanRecord, rssi) == 8, "scan record RSSI");
_Static_assert(offsetof(WlanScanRecord, timestamp) == 9, "scan record timestamp");
_Static_assert(offsetof(WlanScanRecord, beaconInterval) == 17, "scan beacon interval");
_Static_assert(offsetof(WlanScanRecord, capabilities) == 19, "scan capabilities");

typedef struct SceWlanBtScanSsid {
	uint32_t size;
	uint8_t ssid[32];
} SceWlanBtScanSsid;

typedef struct SceWlanBtRobinWlanBackgroundScanQuery {
	uint8_t flush;
} SceWlanBtRobinWlanBackgroundScanQuery;

/* Fixed background-scan operation schema, independent of caller allocation. */
typedef struct WlanBackgroundScanConfig {
	uint8_t callerMetadata[8];
	uint32_t flags;
	uint16_t scanMode;
	uint8_t channels[14];
	uint8_t bssType;
	uint8_t ssidCount;
	uint8_t channelsPerScan;
	uint8_t reserved;
	uint16_t minScanTime;
	uint16_t maxScanTime;
	uint32_t scanInterval;
	SceWlanBtScanSsid ssids[10];
	uint32_t informationElementMask[8];
} WlanBackgroundScanConfig;

_Static_assert(sizeof(WlanBackgroundScanConfig) == 432, "background scan schema");
_Static_assert(offsetof(WlanBackgroundScanConfig, flags) == 8, "background flags");
_Static_assert(offsetof(WlanBackgroundScanConfig, scanMode) == 12, "background mode");
_Static_assert(offsetof(WlanBackgroundScanConfig, channels) == 14, "background channels");
_Static_assert(offsetof(WlanBackgroundScanConfig, bssType) == 28, "background BSS type");
_Static_assert(offsetof(WlanBackgroundScanConfig, ssidCount) == 29, "background SSID count");
_Static_assert(offsetof(WlanBackgroundScanConfig, channelsPerScan) == 30,
	       "background channels per scan");
_Static_assert(offsetof(WlanBackgroundScanConfig, minScanTime) == 32, "background minimum time");
_Static_assert(offsetof(WlanBackgroundScanConfig, maxScanTime) == 34, "background maximum time");
_Static_assert(offsetof(WlanBackgroundScanConfig, scanInterval) == 36, "background interval");
_Static_assert(offsetof(WlanBackgroundScanConfig, ssids) == 40, "background SSIDs");
_Static_assert(offsetof(WlanBackgroundScanConfig, informationElementMask) == 400,
	       "background IE mask");

/* Complete signal result; the caller's first eight bytes are preserved. */
typedef struct WlanSignalInfo {
	uint8_t callerMetadata[8];
	int32_t rssi;
	uint32_t signalQuality;
} WlanSignalInfo;

_Static_assert(sizeof(WlanSignalInfo) == 16, "WLAN signal result size");
_Static_assert(offsetof(WlanSignalInfo, rssi) == 8, "signal strength in dBm");
_Static_assert(offsetof(WlanSignalInfo, signalQuality) == 12, "signal quality percentage");

/* Input configuration and output result are successive allocation phases. */
typedef struct WlanScanConfig {
	uint32_t size;
	uint32_t bufferSize;
	uint32_t flags;
	uint16_t scanMode;
	uint8_t channels[14];
	uint8_t bssType;
	uint8_t ssidCount;
	SceNetEtherAddr bssid;
	SceWlanBtScanSsid ssids[10];
	uint8_t reserved[4];
	uint32_t minScanTime;
	uint32_t maxScanTime;
	uint32_t informationElementMask[8];
	uint32_t vendorInformationElementsSize;
	uint8_t vendorInformationElements[];
} WlanScanConfig;

typedef struct WlanScanResult {
	uint32_t size;
	uint32_t bufferSize;
	uint32_t bssNum;
	uint8_t bssDesc[];
} WlanScanResult;

_Static_assert(sizeof(SceWlanBtScanSsid) == 36, "scan SSID size");
_Static_assert(sizeof(WlanScanConfig) == 444, "scan configuration size");
_Static_assert(offsetof(WlanScanConfig, channels) == 14, "scan channels");
_Static_assert(offsetof(WlanScanConfig, bssid) == 30, "scan BSSID");
_Static_assert(offsetof(WlanScanConfig, ssids) == 36, "scan SSID list");
_Static_assert(offsetof(WlanScanConfig, minScanTime) == 400, "minimum scan time");
_Static_assert(offsetof(WlanScanConfig, informationElementMask) == 408, "scan IE mask");
_Static_assert(offsetof(WlanScanConfig, vendorInformationElementsSize) == 440,
	       "scan vendor IE size");
_Static_assert(sizeof(WlanScanResult) == 12, "scan result header");
_Static_assert(offsetof(WlanScanResult, bssDesc) == 12, "variable BSS records");

/* The configuration allocation is cleared as one complete 92-byte object. */
typedef struct SceWlanBtRobinConfig {
	int available;
	unsigned int configuration;
	SceKernelFastMutex mutex;
	SceWlanBtMonitorCallback *monitor;
	uint8_t interruptEnable;
	uint8_t forceOffFlags;
	uint8_t reserved[2];
	unsigned int wlanState;
	unsigned int bluetoothState;
	unsigned int wakeSources;
} SceWlanBtRobinConfig;

_Static_assert(sizeof(SceWlanBtRobinConfig) == 92, "WLAN/BT configuration size");
_Static_assert(offsetof(SceWlanBtRobinConfig, mutex) == 8, "configuration mutex");
_Static_assert(offsetof(SceWlanBtRobinConfig, monitor) == 72, "configuration monitor");
_Static_assert(offsetof(SceWlanBtRobinConfig, interruptEnable) == 76,
	       "saved SDIO interrupt enable");
_Static_assert(offsetof(SceWlanBtRobinConfig, forceOffFlags) == 77,
	       "forced power off sources");
_Static_assert(offsetof(SceWlanBtRobinConfig, wlanState) == 80, "WLAN power state");
_Static_assert(offsetof(SceWlanBtRobinConfig, bluetoothState) == 84,
	       "Bluetooth power state");
_Static_assert(offsetof(SceWlanBtRobinConfig, wakeSources) == 88, "device wake sources");

/* IdStorage WlanRegion leaf: thirteen trailing bytes are unused. */
typedef struct SceWlanRegion {
	uint16_t channelMask;
	uint8_t flags;
	uint8_t reserved[13];
} SceWlanRegion;

typedef struct SceWlanCallback {
	SceUID threadId;
	SceUID callbackId;
} SceWlanCallback;

/* Complete object cleared by wlanBtInitializeRobin. */
typedef struct SceWlan {
	SceProcEventHandler procEventHandler;
	SceUID procEventHandlerId;
	int powerSuspended;
	int powerRestricted;
	SceWlanRegion region;
	SceKernelFastMutex mutex;
	SceWlanCallback callbacks[2];
} SceWlan;

_Static_assert(sizeof(SceWlanRegion) == 16, "WLAN IdStorage region size");
_Static_assert(sizeof(SceWlanCallback) == 8, "WLAN callback registration size");
_Static_assert(sizeof(SceWlan) == 136, "WLAN object size");
_Static_assert(offsetof(SceWlan, procEventHandlerId) == 28, "process handler UID");
_Static_assert(offsetof(SceWlan, powerSuspended) == 32, "WLAN suspend state");
_Static_assert(offsetof(SceWlan, powerRestricted) == 36, "WLAN power restriction");
_Static_assert(offsetof(SceWlan, region) == 40, "IdStorage WLAN region");
_Static_assert(offsetof(SceWlan, mutex) == 56, "WLAN callback mutex");
_Static_assert(offsetof(SceWlan, callbacks) == 120, "WLAN callback registrations");

typedef struct SceWlanVisitor {
	uint32_t count;
	int result;
	int accepting;
} SceWlanVisitor;

_Static_assert(sizeof(SceWlanVisitor) == 12, "WLAN visitor counter size");
_Static_assert(offsetof(SceWlanVisitor, result) == 4, "visitor close result");
_Static_assert(offsetof(SceWlanVisitor, accepting) == 8, "visitor admission state");

struct SdifSdioFunction;
struct NetDriverPacket;
typedef struct SceWlanBtRobinWlanContext SceWlanBtRobinWlanContext;

/* Complete TCM subobject, followed by the context mutex at runtime byte 1520. */
typedef struct SceWlanBtRobinWlanTcm {
	SceWlanBtRobinWlanContext *context;
	SceWlanBtRobinWlanContext *contextAlias;
	SceKernelFastMutex mutex;
	SceUID eventFlag;
	uint16_t control;
	uint16_t informationElementMaskEnabled;
	uint32_t informationElementMask[8];
} SceWlanBtRobinWlanTcm;

_Static_assert(sizeof(SceWlanBtRobinWlanTcm) == 112, "complete TCM subobject");
_Static_assert(offsetof(SceWlanBtRobinWlanTcm, contextAlias) == 4, "TCM context alias");
_Static_assert(offsetof(SceWlanBtRobinWlanTcm, mutex) == 8, "TCM mutex");
_Static_assert(offsetof(SceWlanBtRobinWlanTcm, eventFlag) == 72, "TCM event flag");
_Static_assert(offsetof(SceWlanBtRobinWlanTcm, control) == 76, "TCM MAC control");
_Static_assert(offsetof(SceWlanBtRobinWlanTcm, informationElementMaskEnabled) == 78,
	       "TCM information-element mask state");
_Static_assert(offsetof(SceWlanBtRobinWlanTcm, informationElementMask) == 80,
	       "TCM information-element mask");

/* Full security object embedded at byte 48 in the 192-byte connect request. */
typedef struct WlanConnectionSecurity {
	uint32_t authenticationMode;
	uint32_t securityMode;
	uint32_t wepCipher;
	uint8_t defaultKey;
	uint8_t reserved[3];
	uint8_t wepKeys[4][13];
	char secret[63];
	uint8_t secretType;
} WlanConnectionSecurity;

_Static_assert(sizeof(WlanConnectionSecurity) == 132, "connection security");
_Static_assert(offsetof(WlanConnectionSecurity, securityMode) == 4, "security mode");
_Static_assert(offsetof(WlanConnectionSecurity, wepCipher) == 8, "WEP cipher");
_Static_assert(offsetof(WlanConnectionSecurity, defaultKey) == 12, "default WEP key");
_Static_assert(offsetof(WlanConnectionSecurity, wepKeys) == 16, "WEP key storage");
_Static_assert(offsetof(WlanConnectionSecurity, secret) == 68, "passphrase or key");
_Static_assert(offsetof(WlanConnectionSecurity, secretType) == 131, "secret type");

/* Fixed request portions copied intact by the driver's control entry point. */
typedef struct WlanWakeScanConfig {
	uint8_t callerMetadata[8];
	uint16_t scanMode;
	uint8_t channels[14];
	uint8_t bssType;
	uint8_t ssidCount;
	uint16_t scanInterval;
	SceWlanBtScanSsid ssids[10];
	uint32_t minScanTime;
	uint32_t maxScanTime;
} WlanWakeScanConfig;

typedef struct WlanWakeNetworkConfig {
	uint8_t callerMetadata[8];
	uint32_t flags;
	uint32_t networkId;
	SceWlanBtScanSsid ssid;
	SceWlanBtScanSsid secondarySsid;
	uint8_t channel;
	uint8_t hostSleepDataSize;
	uint16_t payloadSize;
} WlanWakeNetworkConfig;

typedef struct WlanWakeAdhocConfig {
	uint8_t callerMetadata[8];
	uint32_t flags;
	uint32_t networkId;
	uint8_t channel;
	uint8_t reserved[3];
	SceWlanBtScanSsid ssid;
	SceWlanBtScanSsid secondarySsid;
} WlanWakeAdhocConfig;

typedef struct WlanConnectionConfig {
	uint8_t callerMetadata[8];
	uint32_t flags;
	SceWlanBtScanSsid ssid;
	WlanConnectionSecurity security;
	uint8_t channels[4];
	uint32_t scanInformationElementsSize;
	uint32_t associationInformationElementsSize;
} WlanConnectionConfig;

/* Successful association replaces the input with this complete result phase. */
typedef struct WlanConnectionResult {
	uint8_t callerMetadata[8];
	WlanBssDesc bss;
} WlanConnectionResult;

_Static_assert(sizeof(WlanConnectionResult) == 140, "connection result size");
_Static_assert(offsetof(WlanConnectionResult, bss) == 8, "connection result BSS");

typedef struct WlanHostSleepPeer {
	SceNetEtherAddr address;
	uint8_t reserved[2];
	uint8_t data[256];
} WlanHostSleepPeer;

typedef struct WlanScalarControl {
	uint8_t callerMetadata[8];
	uint32_t value;
} WlanScalarControl;

typedef struct WlanHostSleepPeerList {
	uint8_t callerMetadata[8];
	uint16_t enabled;
	uint16_t peerCount;
	WlanHostSleepPeer peers[16];
} WlanHostSleepPeerList;

typedef struct WlanDiscoveredSsidList {
	uint8_t callerMetadata[8];
	uint16_t available;
	uint16_t ssidCount;
	SceWlanBtScanSsid ssids[];
} WlanDiscoveredSsidList;

typedef struct WlanGpioControl {
	uint8_t callerMetadata[8];
	uint16_t operation;
	uint16_t pin;
	uint32_t value;
} WlanGpioControl;

typedef struct WlanEnergyDetectionControl {
	uint8_t callerMetadata[8];
	uint16_t operation;
	uint16_t offset;
} WlanEnergyDetectionControl;

typedef struct WlanManagementInformationElements {
	uint8_t callerMetadata[8];
	uint32_t informationElementsSize;
	uint8_t informationElements[];
} WlanManagementInformationElements;

typedef struct WlanWakeNetworkRequest {
	WlanWakeNetworkConfig configuration;
	uint8_t payload[];
} WlanWakeNetworkRequest;

_Static_assert(sizeof(WlanScalarControl) == 12, "scalar control request");
_Static_assert(offsetof(WlanScalarControl, value) == 8, "scalar control value");
_Static_assert(sizeof(WlanHostSleepPeerList) == 4236, "host-sleep peer list");
_Static_assert(offsetof(WlanHostSleepPeerList, peers) == 12, "host-sleep peers");
_Static_assert(sizeof(WlanDiscoveredSsidList) == 12, "discovered SSID list header");
_Static_assert(offsetof(WlanDiscoveredSsidList, ssids) == 12, "discovered SSIDs");
_Static_assert(sizeof(WlanGpioControl) == 16, "GPIO control request");
_Static_assert(offsetof(WlanGpioControl, pin) == 10, "GPIO pin");
_Static_assert(offsetof(WlanGpioControl, value) == 12, "GPIO value");
_Static_assert(sizeof(WlanEnergyDetectionControl) == 12, "energy detection request");
_Static_assert(offsetof(WlanEnergyDetectionControl, offset) == 10,
	       "energy detection offset");
_Static_assert(sizeof(WlanManagementInformationElements) == 12, "management IE header");
_Static_assert(offsetof(WlanManagementInformationElements, informationElements) == 12,
	       "management IE data");
_Static_assert(sizeof(WlanWakeNetworkRequest) == 92, "wake network request header");
_Static_assert(offsetof(WlanWakeNetworkRequest, payload) == 92, "wake network payload");

_Static_assert(sizeof(WlanHostSleepPeer) == 264, "host-sleep peer size");
_Static_assert(offsetof(WlanHostSleepPeer, data) == 8, "host-sleep peer data");

_Static_assert(sizeof(WlanWakeScanConfig) == 396, "wake scan configuration");
_Static_assert(offsetof(WlanWakeScanConfig, scanMode) == 8, "wake scan mode");
_Static_assert(offsetof(WlanWakeScanConfig, ssids) == 28, "wake scan SSIDs");
_Static_assert(offsetof(WlanWakeScanConfig, minScanTime) == 388, "wake scan minimum time");
_Static_assert(offsetof(WlanWakeScanConfig, maxScanTime) == 392, "wake scan maximum time");
_Static_assert(sizeof(WlanWakeNetworkConfig) == 92, "wake network configuration");
_Static_assert(offsetof(WlanWakeNetworkConfig, ssid) == 16, "wake network SSID");
_Static_assert(offsetof(WlanWakeNetworkConfig, secondarySsid) == 52, "wake secondary SSID");
_Static_assert(offsetof(WlanWakeNetworkConfig, channel) == 88, "wake network channel");
_Static_assert(offsetof(WlanWakeNetworkConfig, payloadSize) == 90, "wake payload size");
_Static_assert(sizeof(WlanWakeAdhocConfig) == 92, "wake ad-hoc configuration");
_Static_assert(offsetof(WlanWakeAdhocConfig, ssid) == 20, "wake ad-hoc SSID");
_Static_assert(offsetof(WlanWakeAdhocConfig, secondarySsid) == 56, "wake ad-hoc secondary SSID");
_Static_assert(sizeof(WlanConnectionConfig) == 192, "connection fixed configuration");
_Static_assert(offsetof(WlanConnectionConfig, ssid) == 12, "connection SSID");
_Static_assert(offsetof(WlanConnectionConfig, security) == 48, "connection security");
_Static_assert(offsetof(WlanConnectionConfig, channels) == 180, "connection channels");
_Static_assert(offsetof(WlanConnectionConfig, scanInformationElementsSize) == 184,
	       "scan information-element size");
_Static_assert(offsetof(WlanConnectionConfig, associationInformationElementsSize) == 188,
	       "association information-element size");

/* A complete connection request; the BSS's optional IEs follow its fixed part. */
typedef struct WlanBssConnectionConfig {
	uint8_t callerMetadata[8];
	uint32_t flags;
	WlanConnectionSecurity security;
	WlanBssDesc bss;
} WlanBssConnectionConfig;

_Static_assert(sizeof(WlanBssConnectionConfig) == 276, "BSS connection request size");
_Static_assert(offsetof(WlanBssConnectionConfig, security) == 12, "connection security");
_Static_assert(offsetof(WlanBssConnectionConfig, bss) == 144, "connection BSS");
_Static_assert(offsetof(WlanBssConnectionConfig, bss.bssid) == 184, "connection BSSID");

typedef struct SceWlanBtRobinWlanCommand {
	uint16_t commandId;
	uint16_t responseId;
	void *requestBuffer;
	void *requestPayload;
	void *responsePayload;
	unsigned int responseSize;
	struct NetDriverPacket *responsePacket;
	int (*callback)(SceWlanBtRobinWlanContext *, struct SceWlanBtRobinWlanCommand *);
	unsigned int eventMask;
	int result;
	unsigned int flags;
	struct SceWlanBtRobinWlanCommand *next;
	uint64_t timestamp;
} SceWlanBtRobinWlanCommand;

_Static_assert(sizeof(SceWlanBtRobinWlanCommand) == 56, "Robin WLAN command");
_Static_assert(offsetof(SceWlanBtRobinWlanCommand, callback) == 24,
	       "Robin command completion callback");
_Static_assert(offsetof(SceWlanBtRobinWlanCommand, timestamp) == 48,
	       "Robin command submission time");

typedef struct SceWlanBtRobinSdioStatus {
	uint8_t reserved[3];
	uint8_t interruptStatus;
	uint8_t readBitmap[2];
	uint8_t writeBitmap[2];
	uint8_t readLengths[16][2];
} SceWlanBtRobinSdioStatus;

typedef struct SceWlanBtRobinWlanTxTransfer {
	uint32_t address;
	uint32_t size;
	void *buffer;
	struct NetDriverPacket *packet;
	uint32_t index;
	int errorCode;
} SceWlanBtRobinWlanTxTransfer;

/* Complete DCM subobject; the following TCM subobject begins at byte 1408. */
typedef struct SceWlanBtRobinWlanDcm {
	SceWlanBtRobinSdioStatus status;
	SceWlanBtRobinWlanContext *context;
	SceKernelFastMutex commandMutex;
	SceUID commandEventFlag;
	struct SdifSdioContext *sdioContext;
	struct SdifSdioFunction *sdioFunction;
	unsigned int subInterrupt;
	unsigned int ioPort;
	uint8_t reserved[8];
	unsigned int packetHeadroom;
	SceUID commandMemory;
	uint8_t *requestBuffer;
	uint8_t *responseBuffer;
	int waitingForWritePort;
	uint16_t nextReadPort;
	uint16_t nextWritePort;
	uint16_t readBitmap;
	uint16_t writeBitmap;
	uint16_t sequence;
	SceWlanBtRobinWlanCommand *currentCommand;
	SceWlanBtRobinWlanCommand *pendingCommands;
	SceWlanBtRobinWlanCommand commands[8];
	SceWlanBtRobinWlanTxTransfer txTransfers[15];
	uint8_t unusedStorage[364];
	uint8_t *aggregateBuffer;
	unsigned int aggregateSize;
	unsigned int aggregateCount;
	uint8_t reservedAggregate[32];
	unsigned int aggregateCapacity;
	uint16_t aggregatePortBitmap;
	uint16_t aggregateStartPort;
	SceUID aggregateMemory;
} SceWlanBtRobinWlanDcm;

_Static_assert(sizeof(SceWlanBtRobinWlanTxTransfer) == 24, "DCM TX transfer");
_Static_assert(offsetof(SceWlanBtRobinWlanTxTransfer, index) == 16, "SDIO async index");
_Static_assert(offsetof(SceWlanBtRobinWlanTxTransfer, errorCode) == 20, "SDIO async error");
_Static_assert(sizeof(SceWlanBtRobinWlanDcm) == 1408, "complete DCM subobject");
_Static_assert(_Alignof(SceWlanBtRobinWlanDcm) == 8, "DCM command timestamp alignment");
_Static_assert(offsetof(SceWlanBtRobinWlanDcm, context) == 40, "DCM parent");
_Static_assert(offsetof(SceWlanBtRobinWlanDcm, commandMutex) == 44, "DCM mutex");
_Static_assert(offsetof(SceWlanBtRobinWlanDcm, commandEventFlag) == 108, "DCM event flag");
_Static_assert(offsetof(SceWlanBtRobinWlanDcm, sdioFunction) == 116, "DCM SDIO function");
_Static_assert(offsetof(SceWlanBtRobinWlanDcm, commandMemory) == 140, "DCM command memory");
_Static_assert(offsetof(SceWlanBtRobinWlanDcm, currentCommand) == 168, "DCM current command");
_Static_assert(offsetof(SceWlanBtRobinWlanDcm, commands) == 176, "DCM command slots");
_Static_assert(offsetof(SceWlanBtRobinWlanDcm, txTransfers) == 624, "DCM TX slots");
_Static_assert(offsetof(SceWlanBtRobinWlanDcm, aggregateBuffer) == 1348, "DCM aggregate buffer");
_Static_assert(offsetof(SceWlanBtRobinWlanDcm, aggregateCapacity) == 1392,
	       "DCM aggregate capacity");
_Static_assert(offsetof(SceWlanBtRobinWlanDcm, aggregateMemory) == 1400,
	       "DCM aggregate memory");

/* Complete runtime object, including the fixed copies of caller requests. */
struct SceWlanBtRobinWlanContext {
	SceWlanBtRobinWlanDcm dcm;
	SceWlanBtRobinWlanTcm tcm;
	SceKernelFastMutex mutex;
	struct NetDriver driver;
	SceUID worker;
	SceUID workerEventFlag;
	uint32_t currentConfiguration;
	uint32_t nextConfiguration;
	uint32_t desiredConfiguration;
	uint32_t savedConfiguration;
	uint32_t workPending;
	uint32_t driverNotifyPending;
	int intError;
	uint32_t status;
	SceUID sysEventHandler;
	int transmitRequests;
	uint32_t transmitDeferred;
	uint32_t commandDeferred;
	SceWlanVisitor visitor;
	uint8_t reservedVisitor[4];
	SceNetEtherAddr address;
	uint8_t reservedAddress[2];
	int ioctlError;
	uint32_t shutdownState;
	WlanBssDesc bss;
	uint32_t htEnabled;
	uint32_t blockAckReceivePending;
	uint32_t deleteBlockAckPending;
	uint32_t blockAckTransmitState;
	uint8_t reservedBlockAck[4];
	uint32_t transmittedPackets;
	uint32_t blockAckPacketThreshold;
	WlanSignalInfo signal;
	uint64_t signalTimestamp;
	WlanWakeScanConfig wakeScan;
	uint8_t reservedWakeScan[8];
	uint32_t securityPreference;
	uint32_t wakeFlags;
	WlanWakeNetworkConfig wakeNetwork;
	WlanWakeAdhocConfig wakeAdhoc;
	uint32_t wakePayloadSize;
	uint8_t wakePayload[256];
	uint32_t wakeRestoreMode;
	uint32_t wakeReasonFlags;
	uint32_t firstNetworkId;
	uint32_t secondNetworkId;
	uint32_t peerCount;
	WlanHostSleepPeer peers[16];
	uint32_t discoveredSsidCount;
	SceWlanBtScanSsid discoveredSsids[10];
	SceUID wakeOwnerProcess;
	uint32_t deferredConfigurationRequest;
	uint32_t wakeEnabled;
	uint32_t interfaceStopRequested;
	uint32_t currentStateMarker;
	uint32_t pendingStateMarker;
	WlanConnectionConfig connection;
	uint32_t connectionSize;
	uint32_t suspendTimeoutReported;
	uint64_t suspendTimestamp;
};

_Static_assert(sizeof(SceWlanBtRobinWlanContext) == 7728, "complete WLAN runtime");
_Static_assert(offsetof(SceWlanBtRobinWlanContext, tcm) == 1408, "runtime TCM");
_Static_assert(offsetof(SceWlanBtRobinWlanContext, mutex) == 1520, "runtime mutex");
_Static_assert(offsetof(SceWlanBtRobinWlanContext, driver) == 1584, "runtime driver");
_Static_assert(offsetof(SceWlanBtRobinWlanContext, worker) == 1760, "runtime worker");
_Static_assert(offsetof(SceWlanBtRobinWlanContext, status) == 1796, "runtime status");
_Static_assert(offsetof(SceWlanBtRobinWlanContext, transmitRequests) == 1804,
	       "runtime transmit requests");
_Static_assert(offsetof(SceWlanBtRobinWlanContext, visitor) == 1816, "runtime visitor");
_Static_assert(offsetof(SceWlanBtRobinWlanContext, address) == 1832, "runtime MAC");
_Static_assert(offsetof(SceWlanBtRobinWlanContext, bss) == 1848, "runtime BSS");
_Static_assert(offsetof(SceWlanBtRobinWlanContext, htEnabled) == 1980, "runtime HT");
_Static_assert(offsetof(SceWlanBtRobinWlanContext, signal) == 2008, "runtime RSSI");
_Static_assert(offsetof(SceWlanBtRobinWlanContext, signalTimestamp) == 2024,
	       "runtime RSSI time");
_Static_assert(offsetof(SceWlanBtRobinWlanContext, wakeScan) == 2032, "runtime wake scan");
_Static_assert(offsetof(SceWlanBtRobinWlanContext, wakeNetwork) == 2444,
	       "runtime wake network");
_Static_assert(offsetof(SceWlanBtRobinWlanContext, wakeAdhoc) == 2536,
	       "runtime wake ad-hoc");
_Static_assert(offsetof(SceWlanBtRobinWlanContext, wakePayload) == 2632,
	       "runtime wake payload");
_Static_assert(offsetof(SceWlanBtRobinWlanContext, peers) == 2908, "runtime peers");
_Static_assert(offsetof(SceWlanBtRobinWlanContext, discoveredSsids) == 7136,
	       "runtime discovered SSIDs");
_Static_assert(offsetof(SceWlanBtRobinWlanContext, connection) == 7520,
	       "runtime connection request");
_Static_assert(offsetof(SceWlanBtRobinWlanContext, connectionSize) == 7712,
	       "runtime connection size");
_Static_assert(offsetof(SceWlanBtRobinWlanContext, suspendTimestamp) == 7720,
	       "runtime suspend time");

extern SceWlanBtRobinWlanContext gWlanContext;

typedef struct SceWlanBtRobinDeauthentication {
	uint8_t bssid[6];
	uint16_t reason;
} SceWlanBtRobinDeauthentication;

typedef struct WlanSdioMessageHeader {
	uint16_t messageSize;
	uint16_t messageType;
} WlanSdioMessageHeader;

/* Firmware exposes both the encoded halfword and the sequence/BSS fields. */
typedef union WlanCommandSequence {
	uint16_t encoded;
	struct {
		uint8_t number;
		uint8_t bssNumber : 4;
		uint8_t bssType : 4;
	};
} WlanCommandSequence;

_Static_assert(sizeof(WlanCommandSequence) == 2, "firmware sequence encoding");

typedef struct WlanFirmwareCommandHeader {
	uint16_t commandId;
	uint16_t commandSize;
	WlanCommandSequence sequence;
	uint16_t result;
} WlanFirmwareCommandHeader;

_Static_assert(offsetof(WlanFirmwareCommandHeader, sequence) == 4,
	       "firmware command sequence byte");
_Static_assert(offsetof(WlanFirmwareCommandHeader, result) == 6,
	       "firmware command result");

/* Commands have a variable payload after the same two wire headers. */
typedef struct WlanCommandMessage {
	WlanSdioMessageHeader header;
	WlanFirmwareCommandHeader command;
	uint8_t payload[];
} WlanCommandMessage;

_Static_assert(sizeof(WlanCommandMessage) == 12, "command wire headers");
_Static_assert(offsetof(WlanCommandMessage, command) == 4, "command wire header");
_Static_assert(offsetof(WlanCommandMessage, payload) == 12, "command variable payload");

typedef struct SceWlanBtRobinBootstrapMessage {
	WlanSdioMessageHeader header;
	WlanFirmwareCommandHeader command;
	union {
		SceWlanBtRobinDeauthentication deauthentication;
		uint8_t data[244];
	} payload;
} SceWlanBtRobinBootstrapMessage;

_Static_assert(sizeof(WlanSdioMessageHeader) == 4, "SDIO message header");
_Static_assert(sizeof(WlanFirmwareCommandHeader) == 8, "firmware command header");
_Static_assert(offsetof(SceWlanBtRobinBootstrapMessage, command) == 4,
	       "bootstrap command header");

_Static_assert(sizeof(SceWlanBtRobinSdioStatus) == 40, "Robin SDIO register status");
_Static_assert(offsetof(SceWlanBtRobinSdioStatus, interruptStatus) == 3,
	       "Robin interrupt status");
_Static_assert(offsetof(SceWlanBtRobinSdioStatus, readLengths) == 8,
	       "Robin port lengths");
_Static_assert(sizeof(SceWlanBtRobinDeauthentication) == 8,
	       "Robin deauthentication payload");
_Static_assert(sizeof(SceWlanBtRobinBootstrapMessage) == 256,
	       "Robin bootstrap message");
_Static_assert(offsetof(SceWlanBtRobinBootstrapMessage, payload) == 12,
	       "Robin command payload");

typedef struct SceWlanBtRobin {
	struct SdifSdioContext *sdioContext;
	uint8_t revision;
	uint8_t reserved[3];
	struct SdifSdioFunction *bluetoothFunction;
	struct SdifSdioFunction *wlanFunction;
	uint32_t fifoAddress;
	SceUID firmwareMemBlock;
	void *firmwareBuffer;
	unsigned int firmwareSize;
	int firmwareLoaded;
} SceWlanBtRobin;

_Static_assert(sizeof(SceWlanBtRobin) == 36, "Robin transport state size");
_Static_assert(offsetof(SceWlanBtRobin, revision) == 4, "Robin card revision");
_Static_assert(offsetof(SceWlanBtRobin, bluetoothFunction) == 8,
	       "Robin Bluetooth function");
_Static_assert(offsetof(SceWlanBtRobin, wlanFunction) == 12, "Robin WLAN function");
_Static_assert(offsetof(SceWlanBtRobin, fifoAddress) == 16, "Robin FIFO address");
_Static_assert(offsetof(SceWlanBtRobin, firmwareMemBlock) == 20,
	       "Robin firmware allocation");
_Static_assert(offsetof(SceWlanBtRobin, firmwareBuffer) == 24,
	       "Robin firmware buffer");
_Static_assert(offsetof(SceWlanBtRobin, firmwareSize) == 28,
	       "Robin firmware size");
_Static_assert(offsetof(SceWlanBtRobin, firmwareLoaded) == 32,
	       "Robin firmware load state");

int ScePowerForDriver_F535D928(unsigned int mask, int enabled);
struct SdifSdioContext *ksceSdifGetSdContextPartValidateSdio(unsigned int index);
int SceSdifForDriver_DECE963B(struct SdifSdioContext *sdio, uint32_t offset,
			    uint32_t size, uint8_t *buffer);
int SceSdifForDriver_5D65E66B(struct SdifSdioContext *sdio, uint32_t offset,
			    uint32_t size, uint8_t *buffer);
int SceSdifForDriver_AB0222F2(struct SdifSdioContext *sdio);
unsigned int wlanGetConfiguration(void);
int wlanSetConfiguration(int request);
int wlanRegisterCallback(SceUID callbackId);
int wlanUnregisterCallback(void);
int wlanGetWakeOnTargetProcess(void);

#endif
