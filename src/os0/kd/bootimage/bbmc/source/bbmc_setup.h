#ifndef BBMC_SETUP_H
#define BBMC_SETUP_H

#include "qmi_information.h"

typedef struct BbmcPinProtection {
	SceUInt8 pinId;
	SceUInt8 protectionEnabled;
	SceUInt8 pinLength;
	SceUInt8 reserved;
	char pin[12];
} BbmcPinProtection;

typedef struct BbmcPinChange {
	SceUInt8 pinId;
	SceUInt8 oldPinLength;
	SceUInt8 newPinLength;
	SceUInt8 reserved;
	char oldPin[12];
	char newPin[12];
} BbmcPinChange;

typedef struct BbmcSystemSelectionPreference {
	SceUInt8 validFields;
	SceUInt8 reserved;
	SceUInt16 modePreference;
	SceUInt32 bandPreferenceLow;
	SceUInt32 bandPreferenceHigh;
	SceUInt32 prlPreference;
	SceUInt32 roamingPreference;
	SceUInt32 emergencyMode;
} BbmcSystemSelectionPreference;

typedef struct BbmcNetworkRegistration {
	SceUInt32 action;
	SceUInt32 radioInterface;
	SceUInt16 mcc;
	SceUInt16 mnc;
} BbmcNetworkRegistration;

typedef union BbmcSetupValue {
	BbmcDeviceInformation deviceInformation;
	BbmcSmscAddress smscAddress;
	BbmcPinProtection pinProtection;
	BbmcPinChange pinChange;
	BbmcSystemSelectionPreference systemSelection;
	BbmcProfileSettings profile;
	BbmcNetworkRegistration networkRegistration;
	QmiCountedBytes countedBytes;
} BbmcSetupValue;

typedef struct BbmcSetupDescriptor {
	SceUInt32 type;
	SceSize size;
	SceUInt32 access;
} BbmcSetupDescriptor;

_Static_assert(sizeof(BbmcPinProtection) == 16, "PIN protection size");
_Static_assert(offsetof(BbmcPinProtection, pin) == 4, "PIN protection bytes");
_Static_assert(sizeof(BbmcPinChange) == 28, "PIN change size");
_Static_assert(offsetof(BbmcPinChange, oldPin) == 4, "old PIN bytes");
_Static_assert(offsetof(BbmcPinChange, newPin) == 16, "new PIN bytes");
_Static_assert(sizeof(BbmcSystemSelectionPreference) == 24,
	"system selection preference size");
_Static_assert(offsetof(BbmcSystemSelectionPreference, modePreference) == 2,
	"system selection mode");
_Static_assert(offsetof(BbmcSystemSelectionPreference, bandPreferenceLow) == 4,
	"system selection band low");
_Static_assert(offsetof(BbmcSystemSelectionPreference, bandPreferenceHigh) == 8,
	"system selection band high");
_Static_assert(offsetof(BbmcSystemSelectionPreference, prlPreference) == 12,
	"system selection PRL");
_Static_assert(offsetof(BbmcSystemSelectionPreference, roamingPreference) == 16,
	"system selection roaming");
_Static_assert(offsetof(BbmcSystemSelectionPreference, emergencyMode) == 20,
	"system selection emergency mode");
_Static_assert(sizeof(BbmcNetworkRegistration) == 12,
	"network registration size");
_Static_assert(offsetof(BbmcNetworkRegistration, radioInterface) == 4,
	"registration radio interface");
_Static_assert(offsetof(BbmcNetworkRegistration, mcc) == 8,
	"registration country code");
_Static_assert(offsetof(BbmcNetworkRegistration, mnc) == 10,
	"registration network code");
_Static_assert(sizeof(BbmcSetupValue) == 552, "setup value size");
_Static_assert(sizeof(BbmcSetupDescriptor) == 12, "setup descriptor size");

#endif
