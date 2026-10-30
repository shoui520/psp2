#ifndef BBMC_QMI_INFORMATION_H
#define BBMC_QMI_INFORMATION_H

#include "qmi_backend.h"

typedef struct BbmcHomeNetworkInfo {
	SceUInt16 mcc;
	SceUInt16 mnc;
	char description[68];
} BbmcHomeNetworkInfo;

/* name unknown: the IMEI string, copied whole into IdStorage leaf 0x113 */
typedef struct BbmcImeiString {
	char digits[32];
} BbmcImeiString;

typedef struct BbmcDeviceInformation {
	char manufacturer[33];
	SceUInt8 reservedManufacturer[3];
	char model[33];
	SceUInt8 reservedModel[3];
	char revision[101];
	SceUInt8 reservedRevision[3];
	char msisdn[33];
	SceUInt8 reservedMsisdn[3];
	char esn[33];
	SceUInt8 reservedEsn[3];
	BbmcImeiString imei;
	char meid[33];
	SceUInt8 reservedMeid[3];
	char iccid[21];
	SceUInt8 reservedIccid[3];
	SceUInt8 messageProtocol;
	SceUInt8 reservedProtocol;
	SceUInt16 prlVersion;
	char imsi[16];
	SceUInt32 imeiSoftwareVersion;
	BbmcHomeNetworkInfo homeNetwork;
} BbmcDeviceInformation;

typedef struct BbmcSmscAddress {
	char type[4];
	SceUInt16 digitsLength;
	SceUInt16 typeLength;
	char digits[20];
} BbmcSmscAddress;

typedef struct QmiSmscAddressValue {
	char type[3];
	SceUInt8 digitsLength;
	char digits[];
} QmiSmscAddressValue;

typedef struct QmiHomeNetworkValue {
	SceUInt8 mccLow;
	SceUInt8 mccHigh;
	SceUInt8 mncLow;
	SceUInt8 mncHigh;
	SceUInt8 descriptionLength;
	char description[];
} QmiHomeNetworkValue;

typedef struct QmiGprsQos {
	SceUInt32 precedenceClass;
	SceUInt32 delayClass;
	SceUInt32 reliabilityClass;
	SceUInt32 peakThroughputClass;
	SceUInt32 meanThroughputClass;
} QmiGprsQos;

typedef struct QmiGprsQosTlv {
	QmiTlvHeader header;
	SceUInt8 precedenceClass[4];
	SceUInt8 delayClass[4];
	SceUInt8 reliabilityClass[4];
	SceUInt8 peakThroughputClass[4];
	SceUInt8 meanThroughputClass[4];
} QmiGprsQosTlv;

typedef struct BbmcProfileSettings {
	SceUInt32 validFields;
	char profileName[15];
	SceUInt8 profileNameLength;
	SceUInt32 pdpType;
	char apn[100];
	SceUInt32 apnLength;
	SceUInt32 primaryDnsAddress;
	SceUInt32 secondaryDnsAddress;
	SceUInt32 papAuthentication;
	SceUInt32 chapAuthentication;
	SceUInt32 pcscfAddressUsingPco;
	SceUInt32 ipv4Address;
	QmiUmtsQos requestedUmtsQos;
	QmiUmtsQos minimumUmtsQos;
	QmiGprsQos requestedGprsQos;
	QmiGprsQos minimumGprsQos;
	char username[128];
	SceUInt32 usernameLength;
	char password[128];
	SceUInt32 passwordLength;
} BbmcProfileSettings;

_Static_assert(sizeof(BbmcHomeNetworkInfo) == 72, "home network information size");
_Static_assert(sizeof(BbmcDeviceInformation) == 436, "device information size");
_Static_assert(offsetof(BbmcDeviceInformation, model) == 36, "device model");
_Static_assert(offsetof(BbmcDeviceInformation, revision) == 72, "device revision");
_Static_assert(offsetof(BbmcDeviceInformation, msisdn) == 176, "device MSISDN");
_Static_assert(offsetof(BbmcDeviceInformation, esn) == 212, "device ESN");
_Static_assert(offsetof(BbmcDeviceInformation, imei) == 248, "device IMEI");
_Static_assert(offsetof(BbmcDeviceInformation, meid) == 280, "device MEID");
_Static_assert(offsetof(BbmcDeviceInformation, iccid) == 316, "device ICCID");
_Static_assert(offsetof(BbmcDeviceInformation, messageProtocol) == 340, "message protocol");
_Static_assert(offsetof(BbmcDeviceInformation, prlVersion) == 342, "PRL version");
_Static_assert(offsetof(BbmcDeviceInformation, imsi) == 344, "device IMSI");
_Static_assert(offsetof(BbmcDeviceInformation, imeiSoftwareVersion) == 360, "IMEI software version");
_Static_assert(offsetof(BbmcDeviceInformation, homeNetwork) == 364, "home network");
_Static_assert(sizeof(BbmcSmscAddress) == 28, "SMSC address size");
_Static_assert(offsetof(BbmcSmscAddress, digits) == 8, "SMSC digits");
_Static_assert(sizeof(QmiSmscAddressValue) == 4, "SMSC wire prefix");
_Static_assert(sizeof(QmiHomeNetworkValue) == 5, "home network wire prefix");
_Static_assert(sizeof(QmiGprsQos) == 20, "GPRS QoS size");
_Static_assert(sizeof(QmiGprsQosTlv) == 23, "GPRS QoS parameter size");
_Static_assert(sizeof(BbmcProfileSettings) == 552, "profile settings size");
_Static_assert(offsetof(BbmcProfileSettings, apn) == 24, "profile APN");
_Static_assert(offsetof(BbmcProfileSettings, requestedUmtsQos) == 152, "requested UMTS QoS");
_Static_assert(offsetof(BbmcProfileSettings, minimumUmtsQos) == 200, "minimum UMTS QoS");
_Static_assert(offsetof(BbmcProfileSettings, requestedGprsQos) == 248, "requested GPRS QoS");
_Static_assert(offsetof(BbmcProfileSettings, minimumGprsQos) == 268, "minimum GPRS QoS");
_Static_assert(offsetof(BbmcProfileSettings, username) == 288, "profile username");
_Static_assert(offsetof(BbmcProfileSettings, password) == 420, "profile password");

int qmiReadDefaultProfileSettings(BbcmdArgument *output,
	const QmiTlvHeader *parameters, unsigned int size);
int bbcmdReadInformationResponse(BbcmdRequest *request,
	BbcmdProtocolClient *client, int finalResponse);
int bbcmdGetInformation(BbcmdRequest *request);

#endif
