/* SceEnumWakeUp: The DeviceInfo dataset, the device strings and properties. */
#include "ew_internal.h"

static const SceUInt16 mtpOperations[] = {
	PTP_OC_GET_DEVICE_INFO, PTP_OC_OPEN_SESSION, PTP_OC_CLOSE_SESSION,
	PTP_OC_GET_DEVICE_PROP_DESC,
};
static const SceUInt16 mtpEvents[] = { 0x4001 };	/* CancelTransaction */
static const SceUInt16 mtpDeviceProperties[] = { MTP_DPC_DEVICE_ICON };

const MtpDeviceInfo ewDeviceInfo = {
	.standardVersion = 100,
	.vendorExtensionId = 6,			/* Microsoft */
	.vendorExtensionVersion = 100,
	.vendorExtensionDesc = NULL,
	.functionalMode = 1,
	.operations = { 4, mtpOperations },
	.events = { 1, mtpEvents },
	.deviceProperties = { 1, mtpDeviceProperties },
	.captureFormats = { 0, NULL },
	.playbackFormats = { 0, ewFont8x16 },	/* (empty; points at the font) */
	.manufacturer = ewg.strings.provided.manufacturer,
	.model = ewg.strings.provided.model,
	.deviceVersion = ewg.strings.provided.deviceVersion,
	.serialNumber = ewg.strings.serialNumber,
};

const MtpPropDesc ewPropDescs[2] = {
	{
		.code = PTP_DPC_BATTERY_LEVEL,
		.dataType = PTP_DTC_UINT8,
		.getSet = 0,
		.defaultValue.u8 = 100,
		.formFlag = PTP_DPFF_RANGE,
		.form.range8 = { 0, 100, 10 },
	}, {
		.code = MTP_DPC_DEVICE_ICON,
		.dataType = PTP_DTC_AUINT8,
		.getSet = 0,
		.formFlag = PTP_DPFF_NONE,
	},
};

/* provider of the device strings */

int mtpSetupDeviceStrings(MtpDeviceStringsProvider provider)
{
	memset(&ewg.strings, 0, sizeof(ewg.strings));
	if (provider || (provider = ewg.provider)) {
		ewg.provider = provider;
		if (provider(&ewg.strings.provided, 1) >= 0)
			return mtpReadSerial(&ewg.strings);
	}
	mtpStrcpy(ewg.strings.provided.manufacturer, "Sony Interactive Entertainment Inc.");
	mtpStrcpy(ewg.strings.provided.model, "PlayStation(R)Vita");
	mtpStrcpy(ewg.strings.provided.deviceVersion, "");
	mtpStrcpy(ewg.strings.provided.friendlyName, "PlayStation(R)Vita");
	mtpStrcpy(ewg.strings.provided.iconPath, "vs0:vsh/mtpresponder/DevIcon.fil");
	return mtpReadSerial(&ewg.strings);
}

void mtpClearDeviceStrings(void)
{
	ewg.provider = NULL;
	memset(&ewg.strings, 0, sizeof(ewg.strings));
}

const char *mtpSyncPartner(void)
{
	return ewg.strings.provided.syncPartner;
}

const char *mtpIconPath(void)
{
	return ewg.strings.provided.iconPath;
}

const char *mtpFriendlyName(void)
{
	return ewg.strings.provided.friendlyName;
}

const MtpDeviceInfo *mtpDeviceInfo(void)
{
	return &ewDeviceInfo;
}

const MtpPropDesc *mtpFindPropDesc(SceUInt32 code)
{
	int i;

	if (code == PTP_DPC_BATTERY_LEVEL)
		i = 0;
	else if (code == MTP_DPC_DEVICE_ICON)
		i = 1;
	else
		return NULL;
	return &ewPropDescs[i];
}

/* the serial number: every second byte of ID storage leaf 0x112 */
int mtpReadSerial(MtpDeviceStrings *out)
{
	int i, ret;

	if (out == NULL)
		return MTP_ERROR_INVALID_ARGUMENT;
	ret = sceIdStorageLookup(0x112, 0, ewg.serial, sizeof(ewg.serial));
	if (ret < 0)
		return ret;
	for (i = 0; i < 0x20; i++)
		ewg.serial[i] = ewg.serial[i * 2 + 1];
	ewg.serial[0x20] = '\0';
	__builtin_memcpy(out->serialNumber, ewg.serial, 0x21);	/* Sony's build inlines it */
	return ret;
}
