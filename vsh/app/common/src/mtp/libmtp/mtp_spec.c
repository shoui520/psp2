/*
 * The device's description ("SPEC:"): the strings the shell's ini file sets (names,
 * versions, the storages), the DeviceInfo dataset, the properties and formats the
 * responder supports, and the PS Vita's own information and capability documents.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <modulemgr/module_info.h>
#include <regmgr/regmgr.h>
#include <vshbridge/vshbridge.h>
#include "mtp_internal.h"

#define MTP_TAG_SPEC	"SPEC:"

/* SceModulemgr's user syscall; no SDK header declares it */
int sceKernelGetSystemSwVersion(SceKernelSystemSwVersion *info);

/* the ID storage leaf of the model name */
#define MTP_SPEC_IDSTORAGE_MODEL	277

/* the versions the PS Vita information reports */
#define MTP_SPEC_PROTOCOL_VERSION	1800010
#define MTP_SPEC_COM_VERSION		190

/* a thumbnail size the PS Vita information offers, per kind of content */
typedef struct MtpSpecThumbnail {
	int kind;			/* MTP_SPEC_THUMB_* */
	int isVideo;			/* has a duration */
	SceUInt32 codecType;
	SceUInt32 width;
	SceUInt32 height;
	SceUInt32 duration;
} MtpSpecThumbnail;

#define MTP_SPEC_THUMB_PHOTO	1
#define MTP_SPEC_THUMB_MUSIC	2
#define MTP_SPEC_THUMB_VIDEO	3
#define MTP_SPEC_THUMB_GAME	4

static MtpSpecIniCallback s_iniCallback;

/* the serial number and the ini data, one block */
static struct {
	char serialNumber[256];
	MtpSpecIniData ini;
} s_spec;

/* the values of the enumeration forms */
static const SceUInt16 s_protectionStatusValues[] = { 0x0000, 0x0001, 0x8002, 0x8003 };
static const SceUInt8 s_nonConsumableValues[] = { 0, 1 };
static const SceUInt32 s_sampleRateValues[] = { 44100 };
static const SceUInt16 s_numberOfChannelsValues[] = { 1, 2 };
static const SceUInt32 s_audioWaveCodecValues[] = { 0x0001 };
static const SceUInt32 s_wmaAudioWaveCodecValues[] = { 0x0160, 0x0161, 0x0009 };
static const SceUInt32 s_mp3AudioWaveCodecValues[] = { 0x0055 };
static const SceUInt16 s_bitrateTypeValues[] = { 0 };
static const SceUInt16 s_wmaBitrateTypeValues[] = { 1, 2 };
static const SceUInt16 s_mp3BitrateTypeValues[] = { 1, 2 };
static const SceUInt16 s_drmStatusValues[] = { 0, 1 };
static const SceUInt32 s_audioBitDepthValues[] = { 16 };
static const SceUInt16 s_representativeSampleFormatValues[] = { 0x3801 };

static const SceUInt16 s_operationsSupported[] = {
	0x1001, 0x1002, 0x1003, 0x1004, 0x1005, 0x1006, 0x1007, 0x1008,
	0x1009, 0x100C, 0x100D, 0x9407, 0x9408, 0x9409, 0x940C, 0x940D,
	0x1014, 0x1015, 0x1016, 0x101B, 0x9801, 0x9802, 0x9803, 0x9804,
	0x9810, 0x9811, 0x9805, 0x9806, 0x9510, 0x9511, 0x9513, 0x9514,
	0x9515, 0x9516, 0x9518, 0x951C, 0x951F, 0x9520, 0x9523, 0x9524,
	0x9528, 0x9529, 0x952A, 0x952B, 0x952C, 0x952E, 0x952F, 0x9533,
	0x9534, 0x9535, 0x9536, 0x9537, 0x9538, 0x9600, 0x9601, 0x9602,
	0x9603, 0x9604, 0x953B, 0x953C, 0x953D, 0x953F, 0x9540
};

static const SceUInt16 s_eventsSupported[] = {
	0x4001, 0x4004, 0x4005, 0xC801, 0xC104, 0xC105, 0xC108, 0xC107,
	0xC10B, 0xC10D, 0xC10F, 0xC110, 0xC111, 0xC112, 0xC113, 0xC115,
	0xC117, 0xC118, 0xC119, 0xC120, 0xC122, 0xC123, 0xC124, 0xC125,
	0xC600, 0xC601, 0xC611, 0xC129, 0xC12B, 0xC12C, 0xC12D
};

static const SceUInt16 s_devicePropertiesSupported[] = {
	0xD401,		/* SynchronizationPartner */
	0xD402,		/* DeviceFriendlyName */
	0xD405		/* DeviceIcon */
};

/* the object formats the responder plays back */
static const SceUInt16 s_playbackFormats[] = {
	0x3000, 0x3001, 0x3004, 0x3009, 0x3800, 0xB900, 0xB903, 0xB980,
	0xB982, 0xB411, 0xB402, 0x3801, 0x3804, 0x3807, 0x380B, 0x380D,
	0xB014, 0xB005, 0xB006, 0xB007, 0xB00A, 0xB00B, 0xB00F, 0xB010
};

/* the object formats that take references */
static const SceUInt16 s_referenceFormats[] = {
	0x3008, 0x3009, 0x3000, 0xB903, 0xB900, 0x3800, 0x3801, 0x3804,
	0x3807, 0x380B, 0x380D, 0xB982, 0xB411, 0xB980, 0xB014, 0xB005
};

static const MtpDeviceInfo s_deviceInfo = {
	100,
	6,
	100,
	"microsoft.com: 1.0; scei.co.jp/psvita:1.2.0.1.0; sony.net/MRLN: 1.0",
	0,
	{ sizeof(s_operationsSupported) / sizeof(SceUInt16), s_operationsSupported },
	{ sizeof(s_eventsSupported) / sizeof(SceUInt16), s_eventsSupported },
	{ sizeof(s_devicePropertiesSupported) / sizeof(SceUInt16), s_devicePropertiesSupported },
	{ 0, NULL },
	{ sizeof(s_playbackFormats) / sizeof(SceUInt16), s_playbackFormats },
	s_spec.ini.manufacturer,
	s_spec.ini.model,
	s_spec.ini.deviceVersion,
	s_spec.serialNumber
};

/* the object properties each kind of object has */
static const SceUInt16 s_genericObjectProps[] = {
	0xDC01, 0xDC02, 0xDC03, 0xDC04, 0xDC07, 0xDC08, 0xDC09, 0xDC0B,
	0xDC41, 0xDC44, 0xDC4F
};

static const SceUInt16 s_audioObjectProps[] = {
	0xDC01, 0xDC02, 0xDC03, 0xDC04, 0xDC07, 0xDC08, 0xDC09, 0xDC0B,
	0xDC41, 0xDC44, 0xDC4F, 0xDC46, 0xDC8B, 0xDC8C, 0xDC91, 0xDC9A,
	0xDC9B, 0xDE93, 0xDE94, 0xDE99, 0xDE9A, 0xDC89, 0xDC8A, 0xDC99,
	0xDE92, 0xDC9D, 0xDE95, 0xD901
};

static const SceUInt16 s_videoObjectProps[] = {
	0xDC01, 0xDC02, 0xDC03, 0xDC04, 0xDC07, 0xDC08, 0xDC09, 0xDC0B,
	0xDC41, 0xDC44, 0xDC4F, 0xDC46, 0xDC8C, 0xDC9A, 0xDC9B, 0xDC8A,
	0xD901, 0xDC99, 0xDC81, 0xDC84, 0xDC83, 0xDC86
};

static const SceUInt16 s_imageObjectProps[] = {
	0xDC01, 0xDC02, 0xDC03, 0xDC04, 0xDC07, 0xDC08, 0xDC09, 0xDC0B,
	0xDC41, 0xDC44, 0xDC4F, 0xDC87, 0xDC88
};

#define MTP_SPEC_PROPS(list)	{ sizeof(list) / sizeof(SceUInt16), list }

/* an object format and the properties its objects have */
typedef struct MtpSpecFormatProps {
	SceUInt16 format;
	MtpCodeList props;
} MtpSpecFormatProps;

static const MtpSpecFormatProps s_objectPropsSupported[] = {
	{ 0xB402, MTP_SPEC_PROPS(s_genericObjectProps) },
	{ 0x3000, MTP_SPEC_PROPS(s_genericObjectProps) },
	{ 0x3001, MTP_SPEC_PROPS(s_genericObjectProps) },
	{ 0x3004, MTP_SPEC_PROPS(s_genericObjectProps) },
	{ 0x3008, MTP_SPEC_PROPS(s_audioObjectProps) },
	{ 0x3009, MTP_SPEC_PROPS(s_audioObjectProps) },
	{ 0x300C, MTP_SPEC_PROPS(s_audioObjectProps) },
	{ 0x3000, MTP_SPEC_PROPS(s_genericObjectProps) },
	{ 0xB901, MTP_SPEC_PROPS(s_audioObjectProps) },
	{ 0xB900, MTP_SPEC_PROPS(s_audioObjectProps) },
	{ 0xB903, MTP_SPEC_PROPS(s_audioObjectProps) },
	{ 0xB980, MTP_SPEC_PROPS(s_audioObjectProps) },
	{ 0xB981, MTP_SPEC_PROPS(s_genericObjectProps) },
	{ 0xB982, MTP_SPEC_PROPS(s_genericObjectProps) },
	{ 0xB411, MTP_SPEC_PROPS(s_genericObjectProps) },
	{ 0xBA03, MTP_SPEC_PROPS(s_videoObjectProps) },
	{ 0xBA05, MTP_SPEC_PROPS(s_videoObjectProps) },
	{ 0x3800, MTP_SPEC_PROPS(s_imageObjectProps) },
	{ 0x3801, MTP_SPEC_PROPS(s_imageObjectProps) },
	{ 0x3804, MTP_SPEC_PROPS(s_imageObjectProps) },
	{ 0x3807, MTP_SPEC_PROPS(s_imageObjectProps) },
	{ 0x3808, MTP_SPEC_PROPS(s_imageObjectProps) },
	{ 0x380B, MTP_SPEC_PROPS(s_imageObjectProps) },
	{ 0x380D, MTP_SPEC_PROPS(s_imageObjectProps) },
	{ 0xB014, MTP_SPEC_PROPS(s_genericObjectProps) },
	{ 0xB005, MTP_SPEC_PROPS(s_genericObjectProps) },
	{ 0xB006, MTP_SPEC_PROPS(s_genericObjectProps) },
	{ 0xB007, MTP_SPEC_PROPS(s_genericObjectProps) },
	{ 0xB00A, MTP_SPEC_PROPS(s_genericObjectProps) },
	{ 0xB00B, MTP_SPEC_PROPS(s_genericObjectProps) },
	{ 0xB00F, MTP_SPEC_PROPS(s_genericObjectProps) },
	{ 0xB010, MTP_SPEC_PROPS(s_genericObjectProps) }
};

#define MTP_SPEC_ENUM(array)	\
	{ .enumeration = { .count = sizeof(array) / sizeof(array[0]), .values = array } }

static const MtpPropDesc s_devicePropDescs[] = {
	{ 0x5001, MTP_TYPE_UINT8, MTP_GET, { 0 }, 0, MTP_FORM_RANGE,	/* BatteryLevel */
	  { .range8 = { 0, 100, 1 } } },
	{ 0xD401, MTP_TYPE_STR, MTP_GET, { .str = "Content Manager Assistant" }, 0,
	  MTP_FORM_NONE, { 0 } },					/* SynchronizationPartner */
	{ 0xD402, MTP_TYPE_STR, MTP_GET, { .str = "PlayStation(R)Vita" }, 0,
	  MTP_FORM_NONE, { 0 } },					/* DeviceFriendlyName */
	{ 0xD405, MTP_TYPE_AUINT8, MTP_GET, { 0 }, 0, MTP_FORM_NONE, { 0 } }	/* DeviceIcon */
};

static const MtpPropDesc s_objectPropDescs[] = {
	{ 0xDC01, MTP_TYPE_UINT32, MTP_GET, { 0 }, 1, MTP_FORM_NONE, { 0 } },	/* StorageID */
	{ 0xDC02, MTP_TYPE_UINT16, MTP_GET, { 0 }, 1, MTP_FORM_NONE, { 0 } },	/* ObjectFormat */
	{ 0xDC03, MTP_TYPE_UINT16, MTP_GET, { 0 }, 1, MTP_FORM_ENUM,	/* ProtectionStatus */
	  MTP_SPEC_ENUM(s_protectionStatusValues) },
	{ 0xDC04, MTP_TYPE_UINT64, MTP_GET, { 0 }, 1, MTP_FORM_NONE, { 0 } },	/* ObjectSize */
	{ 0xDC07, MTP_TYPE_STR, MTP_GET, { 0 }, 1, MTP_FORM_NONE, { 0 } },	/* ObjectFileName */
	{ 0xDC08, MTP_TYPE_STR, MTP_GET, { 0 }, 1, MTP_FORM_DATETIME, { 0 } },	/* DateCreated */
	{ 0xDC09, MTP_TYPE_STR, MTP_GET, { 0 }, 1, MTP_FORM_DATETIME, { 0 } },	/* DateModified */
	{ 0xDC0B, MTP_TYPE_UINT32, MTP_GET, { 0 }, 1, MTP_FORM_NONE, { 0 } },	/* ParentObject */
	{ 0xDC41, MTP_TYPE_UINT128, MTP_GET, { 0 }, 1, MTP_FORM_NONE, { 0 } },
	{ 0xDC44, MTP_TYPE_STR, MTP_GET_SET, { 0 }, 1, MTP_FORM_NONE, { 0 } },	/* Name */
	{ 0xDC4F, MTP_TYPE_UINT8, MTP_GET, { 0 }, 1, MTP_FORM_ENUM,	/* NonConsumable */
	  MTP_SPEC_ENUM(s_nonConsumableValues) },
	{ 0xDC46, MTP_TYPE_STR, MTP_GET_SET, { 0 }, 2, MTP_FORM_NONE, { 0 } },	/* Artist */
	{ 0xDC8B, MTP_TYPE_UINT16, MTP_GET_SET, { 0 }, 2, MTP_FORM_NONE, { 0 } },	/* Track */
	{ 0xDC8C, MTP_TYPE_STR, MTP_GET_SET, { 0 }, 2, MTP_FORM_NONE, { 0 } },	/* Genre */
	{ 0xDC91, MTP_TYPE_UINT32, MTP_GET_SET, { 0 }, 2, MTP_FORM_NONE, { 0 } },	/* UseCount */
	{ 0xDC9A, MTP_TYPE_STR, MTP_GET_SET, { 0 }, 2, MTP_FORM_NONE, { 0 } },	/* AlbumName */
	{ 0xDC9B, MTP_TYPE_STR, MTP_GET_SET, { 0 }, 2, MTP_FORM_NONE, { 0 } },	/* AlbumArtist */
	{ 0xDE93, MTP_TYPE_UINT32, MTP_GET_SET, { 0 }, 2, MTP_FORM_ENUM,	/* SampleRate */
	  MTP_SPEC_ENUM(s_sampleRateValues) },
	{ 0xDE94, MTP_TYPE_UINT16, MTP_GET_SET, { 0 }, 2, MTP_FORM_ENUM,	/* NumberOfChannels */
	  MTP_SPEC_ENUM(s_numberOfChannelsValues) },
	{ 0xDE99, MTP_TYPE_UINT32, MTP_GET_SET, { 0 }, 2, MTP_FORM_ENUM,	/* AudioWAVECodec */
	  MTP_SPEC_ENUM(s_audioWaveCodecValues) },
	{ 0xDE9A, MTP_TYPE_UINT32, MTP_GET_SET, { 0 }, 2, MTP_FORM_RANGE,	/* AudioBitRate */
	  { .range32 = { 1, 1536000, 1 } } },
	{ 0xDC89, MTP_TYPE_UINT32, MTP_GET_SET, { 0 }, 2, MTP_FORM_NONE, { 0 } },	/* Duration */
	{ 0xDC8A, MTP_TYPE_UINT16, MTP_GET_SET, { 0 }, 2, MTP_FORM_RANGE,	/* Rating */
	  { .range16 = { 0, 100, 1 } } },
	{ 0xDC99, MTP_TYPE_STR, MTP_GET_SET, { 0 }, 2, MTP_FORM_DATETIME, { 0 } },
	{ 0xDE92, MTP_TYPE_UINT16, MTP_GET_SET, { 0 }, 2, MTP_FORM_ENUM,	/* BitrateType */
	  MTP_SPEC_ENUM(s_bitrateTypeValues) },
	{ 0xDC9D, MTP_TYPE_UINT16, MTP_GET_SET, { 0 }, 2, MTP_FORM_ENUM,	/* DRMStatus */
	  MTP_SPEC_ENUM(s_drmStatusValues) },
	{ 0xDE95, MTP_TYPE_UINT32, MTP_GET_SET, { 0 }, 2, MTP_FORM_ENUM,	/* AudioBitDepth */
	  MTP_SPEC_ENUM(s_audioBitDepthValues) },
	{ 0xDC87, MTP_TYPE_UINT32, MTP_GET_SET, { 0 }, 2, MTP_FORM_RANGE,	/* Width */
	  { .range32 = { 0, 1600, 1 } } },
	{ 0xDC88, MTP_TYPE_UINT32, MTP_GET_SET, { 0 }, 2, MTP_FORM_RANGE,	/* Height */
	  { .range32 = { 0, 1200, 1 } } },
	{ 0xDC81, MTP_TYPE_UINT16, MTP_GET_SET, { 0 }, 2, MTP_FORM_ENUM,
	  MTP_SPEC_ENUM(s_representativeSampleFormatValues) },
	{ 0xDC84, MTP_TYPE_UINT32, MTP_GET_SET, { 0 }, 2, MTP_FORM_RANGE,
	  { .range32 = { 0, 320, 1 } } },			/* RepresentativeSampleWidth */
	{ 0xDC83, MTP_TYPE_UINT32, MTP_GET_SET, { 0 }, 2, MTP_FORM_RANGE,
	  { .range32 = { 0, 240, 1 } } },			/* RepresentativeSampleHeight */
	{ 0xD901, MTP_TYPE_UINT8, MTP_GET_SET, { 0 }, 2, MTP_FORM_NONE, { 0 } },
	{ 0xDC86, MTP_TYPE_AUINT8, MTP_GET_SET, { 0 }, 4, MTP_FORM_BYTE_ARRAY,
	  { .maxLength = 0xFFF4 } }				/* RepresentativeSampleData */
};

/* the forms some object formats have instead of their property's own */
static const MtpPropFormatForm s_audioBitRateForms[] = {
	{ 0xB901, { .range32 = { 1, 385000, 1 } } },
	{ 0x300C, { .range32 = { 1, 385000, 1 } } }
};

static const MtpPropFormatForm s_audioWaveCodecForms[] = {
	{ 0xB901, MTP_SPEC_ENUM(s_wmaAudioWaveCodecValues) },
	{ 0x300C, MTP_SPEC_ENUM(s_wmaAudioWaveCodecValues) },
	{ 0x3009, MTP_SPEC_ENUM(s_mp3AudioWaveCodecValues) }
};

static const MtpPropFormatForm s_bitrateTypeForms[] = {
	{ 0xB901, MTP_SPEC_ENUM(s_wmaBitrateTypeValues) },
	{ 0x300C, MTP_SPEC_ENUM(s_wmaBitrateTypeValues) },
	{ 0x3009, MTP_SPEC_ENUM(s_mp3BitrateTypeValues) }
};

/* a property whose form depends on the object format */
typedef struct MtpSpecPropForms {
	SceUInt16 code;
	SceUInt32 count;
	const MtpPropFormatForm *forms;
} MtpSpecPropForms;

static const MtpSpecPropForms s_objectPropForms[] = {
	{ 0xDE9A, 2, s_audioBitRateForms },
	{ 0xDE99, 3, s_audioWaveCodecForms },
	{ 0xDE92, 3, s_bitrateTypeForms }
};

static const MtpSpecThumbnail s_thumbnails[] = {
	{ MTP_SPEC_THUMB_PHOTO, 0, 17, 213, 120, 0 },
	{ MTP_SPEC_THUMB_VIDEO, 1, 5, 213, 120, 15 },
	{ MTP_SPEC_THUMB_MUSIC, 0, 17, 192, 192, 0 },
	{ MTP_SPEC_THUMB_GAME, 0, 17, 192, 192, 0 }
};

static inline void _saveSpecIniData(void)
{
	int ret;

	if (s_iniCallback == NULL)
		return;
	ret = s_iniCallback(&s_spec.ini, 0);
	if (ret < 0)
		sceClibPrintf("[MTP %s%s(%d)err] unable to save ini value: 0x%x\n", MTP_TAG_SPEC,
			      "_saveSpecIniData", 1269, ret);
}

void mtpSpecLoadIniData(MtpSpecIniCallback callback)
{
	SceKernelSystemSwVersion version;
	int ret;

	mtpUtilMemset(&s_spec, 0, sizeof(s_spec));
	if (callback != NULL)
		s_iniCallback = callback;
	if (s_iniCallback != NULL) {
		ret = s_iniCallback(&s_spec.ini, 1);
		if (ret >= 0)
			goto loaded;
		MTP_ERR(MTP_TAG_SPEC, 1327, "unable to get ini value from app: 0x%x\n", ret);
	}
	mtpUtilStrcpy(s_spec.ini.manufacturer, "Sony Interactive Entertainment Inc.");
	mtpUtilStrcpy(s_spec.ini.model, "PlayStation(R)Vita");
	mtpUtilMemset(&version, 0, sizeof(version));
	version.size = sizeof(version);
	ret = sceKernelGetSystemSwVersion(&version);
	if (ret < 0)
		MTP_ERR(MTP_TAG_SPEC, 1338, "sceKernelGetSystemSwVersion failed: 0x%x\n", ret);
	mtpUtilStrcpy(s_spec.ini.deviceVersion, version.versionString);
	mtpUtilStrcpy(s_spec.ini.friendlyName, "PlayStation(R)Vita");
	mtpUtilStrcpy(s_spec.ini.syncPartner, "Content Manager Assistant");
	mtpUtilStrcpy(s_spec.ini.deviceIconPath, "vs0:vsh/mtpresponder/DevIcon.fil");
	sceClibMemset(s_spec.ini.storages, 0, sizeof(s_spec.ini.storages));
	s_spec.ini.storages[0].attribute = 1;
	sceClibSnprintf(s_spec.ini.storages[0].path, sizeof(s_spec.ini.storages[0].path), "ux0:");
	mtpUtilStrcpy(s_spec.ini.storageFile, "storage.hds");
loaded:
	mtpUtilGetSerialNumberFromIDStorage(s_spec.serialNumber);
	_saveSpecIniData();
}

void mtpSpecTerminate(void)
{
	s_iniCallback = NULL;
	mtpUtilMemset(&s_spec, 0, sizeof(s_spec));
}

char *mtpSpecGetSerialNumber(void)
{
	return s_spec.serialNumber;
}

char *mtpSpecGetSyncPartner(void)
{
	return s_spec.ini.syncPartner;
}

char *mtpSpecGetDeviceIconPath(void)
{
	return s_spec.ini.deviceIconPath;
}

char *mtpSpecGetFriendlyName(void)
{
	return s_spec.ini.friendlyName;
}

void mtpSpecSetFriendlyName(const char *name)
{
	SceSize length = mtpUtilStrlen(name);

	if (length >= sizeof(s_spec.ini.friendlyName))
		length = sizeof(s_spec.ini.friendlyName) - 1;
	mtpUtilMemcpy(s_spec.ini.friendlyName, name, length);
	s_spec.ini.friendlyName[length] = '\0';
	_saveSpecIniData();
}

char *mtpSpecFindStorage(const char *name)
{
	MtpSpecStorage *storage = s_spec.ini.storages;
	int i;

	for (i = 0; i < MTP_SPEC_NUM_STORAGES; i++, storage++) {
		if (mtpUtilMemcmp(storage->path, name, mtpUtilStrlen(name)) == 0)
			return storage->path;
	}
	return NULL;
}

int mtpSpecGetStorageAttribute(const char *name)
{
	int i;

	for (i = 0; i < MTP_SPEC_NUM_STORAGES; i++) {
		if (mtpUtilMemcmp(s_spec.ini.storages[i].path, name, mtpUtilStrlen(name)) == 0)
			return s_spec.ini.storages[i].attribute;
	}
	return 0;
}

const MtpDeviceInfo *mtpSpecGetDeviceInfo(void)
{
	return &s_deviceInfo;
}

const MtpPropDesc *mtpSpecGetDevicePropDesc(SceUInt16 code)
{
	SceUInt32 i;

	for (i = 0; i < sizeof(s_devicePropDescs) / sizeof(s_devicePropDescs[0]); i++) {
		if (s_devicePropDescs[i].code == code)
			return &s_devicePropDescs[i];
	}
	return NULL;
}

const MtpCodeList *mtpSpecGetObjectPropsSupported(SceUInt16 format)
{
	SceUInt32 i;

	for (i = 0; i < sizeof(s_objectPropsSupported) / sizeof(s_objectPropsSupported[0]); i++) {
		if (s_objectPropsSupported[i].format == format)
			return &s_objectPropsSupported[i].props;
	}
	return NULL;
}

int mtpSpecIsSupportedFormat(SceUInt16 format)
{
	SceUInt32 i;

	for (i = 0; i < sizeof(s_playbackFormats) / sizeof(s_playbackFormats[0]); i++) {
		if (s_playbackFormats[i] == format)
			return 1;
	}
	return 0;
}

int mtpSpecIsReferenceFormat(SceUInt16 format)
{
	SceUInt32 i;

	for (i = 0; i < sizeof(s_referenceFormats) / sizeof(s_referenceFormats[0]); i++) {
		if (s_referenceFormats[i] == format)
			return 1;
	}
	return 0;
}

const MtpPropDesc *mtpSpecGetObjectPropDesc(SceUInt16 code)
{
	SceUInt32 i;

	for (i = 0; i < sizeof(s_objectPropDescs) / sizeof(s_objectPropDescs[0]); i++) {
		if (s_objectPropDescs[i].code == code)
			return &s_objectPropDescs[i];
	}
	return NULL;
}

const void *mtpSpecGetObjectPropFormatForm(SceUInt16 code, SceUInt16 format)
{
	const MtpPropFormatForm *forms;
	const void *form = NULL;
	SceUInt32 i, j;

	for (i = 0; i < sizeof(s_objectPropForms) / sizeof(s_objectPropForms[0]); i++) {
		if (s_objectPropForms[i].code == code) {
			forms = s_objectPropForms[i].forms;
			for (j = 0; j < s_objectPropForms[i].count; j++) {
				if (forms[j].format == format)
					form = &forms[j].form;
			}
		}
	}
	return form;
}

int mtpSpecGetVitaInformation(char *buf, SceSize size, int withOnlineId)
{
	SceKernelSystemSwVersion version;
	char onlineId[256];
	char userName[256];
	const MtpSpecThumbnail *thumbnail;
	const char *name;
	char *p;
	SceSize rest;
	int total, ret;
	SceUInt32 i;
	int timeZone;

	mtpUtilMemset(&version, 0, sizeof(version));
	version.size = sizeof(version);
	ret = sceKernelGetSystemSwVersion(&version);
	if (ret < 0)
		MTP_ERR(MTP_TAG_SPEC, 1692, "sceKernelGetSystemSwVersion failed: 0x%x\n", ret);

	char modelInfo[25] = { 0 };
	char model[13] = { 0 };

	if (vshIdStorageLookup(MTP_SPEC_IDSTORAGE_MODEL, 0, model, sizeof(model) - 1) == 0)
		sceClibSnprintf(modelInfo, sizeof(modelInfo) - 1, "modelInfo=\"%s\"", model);
	timeZone = 0;

	char timeZoneInfo[15] = { 0 };

	if (sceRegMgrGetKeyInt("/CONFIG/DATE", "time_zone", &timeZone) == 0)
		sceClibSnprintf(timeZoneInfo, sizeof(timeZoneInfo) - 1, "timezone=\"%u\"", timeZone);

	if (withOnlineId) {
		getSystemParamUserName(userName, 0);
		mtpUserNameCopy(onlineId, sizeof(onlineId), userName);
		ret = sceClibSnprintf(buf, size, "<%s responderVersion=\"%s\" protocolVersion=\"%08u\""
				      " comVersion=\"%u\" onlineId=\"%s\" %s %s>", "VITAInformation",
				      version.versionString, MTP_SPEC_PROTOCOL_VERSION,
				      MTP_SPEC_COM_VERSION, onlineId, modelInfo, timeZoneInfo);
	} else {
		ret = sceClibSnprintf(buf, size, "<%s responderVersion=\"%s\" protocolVersion=\"%08u\""
				      " comVersion=\"%u\" %s %s>", "VITAInformation",
				      version.versionString, MTP_SPEC_PROTOCOL_VERSION,
				      MTP_SPEC_COM_VERSION, modelInfo, timeZoneInfo);
	}
	if (ret < 0) {
		MTP_ERR(MTP_TAG_SPEC, 1758, "Snprintf failed: 0x%x\n", ret);
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}

	total = ret;
	p = buf + ret;
	rest = size - ret;
	thumbnail = s_thumbnails;
	for (i = 0; i < sizeof(s_thumbnails) / sizeof(s_thumbnails[0]); i++, thumbnail++) {
		switch (thumbnail->kind) {
		case MTP_SPEC_THUMB_PHOTO:
			name = "photoThumb";
			break;
		case MTP_SPEC_THUMB_MUSIC:
			name = "musicThumb";
			break;
		case MTP_SPEC_THUMB_VIDEO:
			name = "videoThumb";
			break;
		case MTP_SPEC_THUMB_GAME:
			name = "gameThumb";
			break;
		default:
			continue;
		}
		if (name == NULL)
			continue;
		if (thumbnail->isVideo)
			ret = sceClibSnprintf(p, rest, "<%s type=\"1\" codecType=\"%u\" width=\"%u\""
					      " height=\"%u\" duration=\"%u\"/>", name,
					      thumbnail->codecType, thumbnail->width, thumbnail->height,
					      thumbnail->duration);
		else
			ret = sceClibSnprintf(p, rest, "<%s type=\"0\" codecType=\"%u\" width=\"%u\""
					      " height=\"%u\"/>", name, thumbnail->codecType,
					      thumbnail->width, thumbnail->height);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_SPEC, 1788, "Snprintf failed: 0x%x\n", ret);
			return SCE_MTP_ERROR_NOT_INITIALIZED;
		}
		total += ret;
		p += ret;
		rest = size - total;
	}

	ret = sceClibSnprintf(p, rest, "</%s>", "VITAInformation");
	if (ret < 0) {
		MTP_ERR(MTP_TAG_SPEC, 1797, "Snprintf failed: 0x%x\n", ret);
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	return ret + total;
}

#define MTP_SPEC_CAPABILITY_INFO \
	"<?xml version=\"1.0\" encoding=\"UTF-8\"?>" \
	"<capabilityInfo version=\"1.0\">" \
	"<function type=\"photo\">" \
	"<format contentType=\"image/jpeg\" />" \
	"<format contentType=\"image/png\" />" \
	"<format contentType=\"image/tiff\" />" \
	"<format contentType=\"image/bmp\" />" \
	"<format contentType=\"image/gif\" />" \
	"<format contentType=\"image/x-mpo\" />" \
	"<format contentType=\"video/x-photocam-movie\" />" \
	"<option name=\"physicalView\" />" \
	"<option name=\"playlist\" />" \
	"<option name=\"viewRefresh\" />" \
	"</function>" \
	"<function type=\"music\">" \
	"<format contentType=\"audio/mp3\" />" \
	"<format contentType=\"audio/mp4\" codec=\"AAC\" />" \
	"<format contentType=\"audio/wav\" codec=\"LPCM\" />" \
	"<option name=\"playlist\" />" \
	"<option name=\"viewRefresh\" />" \
	"<option name=\"contentsSearch\" />" \
	"</function>" \
	"<function type=\"video\">" \
	"<format contentType=\"video/mp4\" videoCodec=\"MPEG4\" audioCodec=\"AAC\" />" \
	"<format contentType=\"video/mp4\" videoCodec=\"AVC\" audioCodec=\"AAC\" />" \
	"<format contentType=\"video/x-marlin-est\" />" \
	"<format contentType=\"video/x-marlin-vod\" />" \
	"<option name=\"physicalView\" />" \
	"<option name=\"viewRefresh\" />" \
	"<option name=\"contentsSearch\" />" \
	"</function>" \
	"<function type=\"game\">" \
	"<format contentType=\"vitaApp\" />" \
	"<format contentType=\"PSPGame\" />" \
	"<format contentType=\"PSPSaveData\" />" \
	"<format contentType=\"PSGame\" />" \
	"<format contentType=\"PSMApp\" />" \
	"</function>" \
	"<function type=\"backup\">" \
	"<format type=\"systemBackup\" />" \
	"</function>" \
	"<function type=\"accountLink\" interface=\"1\" />" \
	"<function type=\"systemUpdate\" interface=\"1\" />" \
	"<function type=\"networkRpc\" />" \
	"<function type=\"connectionHandover\" interface=\"2\" />" \
	"</capabilityInfo>" \

static const char s_capabilityInfo[] = MTP_SPEC_CAPABILITY_INFO;
/* its length with the terminating NUL, read from memory at each use as in Sony's build */
static const volatile int s_capabilityInfoLength = sizeof(MTP_SPEC_CAPABILITY_INFO);

#define MTP_SPEC_CAPABILITY_INFO_DOLCE \
	"<?xml version=\"1.0\" encoding=\"UTF-8\"?>" \
	"<capabilityInfo version=\"1.0\">" \
	"<interface value=\"2\" />" \
	"<function type=\"photo\">" \
	"<format contentType=\"image/jpeg\" />" \
	"<format contentType=\"image/png\" />" \
	"<format contentType=\"image/tiff\" />" \
	"<format contentType=\"image/bmp\" />" \
	"<format contentType=\"image/gif\" />" \
	"<format contentType=\"image/x-mpo\" />" \
	"<format contentType=\"video/x-photocam-movie\" />" \
	"<option name=\"physicalView\" />" \
	"<option name=\"viewRefresh\" />" \
	"</function>" \
	"<function type=\"music\">" \
	"<format contentType=\"audio/mp3\" />" \
	"<format contentType=\"audio/mp4\" codec=\"AAC\" />" \
	"<format contentType=\"audio/wav\" codec=\"LPCM\" />" \
	"<option name=\"playlist\" />" \
	"<option name=\"viewRefresh\" />" \
	"<option name=\"contentsSearch\" />" \
	"</function>" \
	"<function type=\"video\">" \
	"<format contentType=\"video/mp4\" videoCodec=\"MPEG4\" audioCodec=\"AAC\" />" \
	"<format contentType=\"video/mp4\" videoCodec=\"AVC\" audioCodec=\"AAC\" />" \
	"<format contentType=\"video/x-marlin-est\" />" \
	"<format contentType=\"video/x-marlin-vod\" />" \
	"<option name=\"physicalView\" />" \
	"<option name=\"viewRefresh\" />" \
	"<option name=\"contentsSearch\" />" \
	"</function>" \
	"<function type=\"game\">" \
	"<format contentType=\"vitaApp\" />" \
	"<format contentType=\"dolceApp\" />" \
	"<format contentType=\"PSPGame\" />" \
	"<format contentType=\"PSPSaveData\" />" \
	"<format contentType=\"PSGame\" />" \
	"<format contentType=\"PSMApp\" />" \
	"</function>" \
	"<function type=\"backup\">" \
	"<format type=\"systemBackup\" />" \
	"</function>" \
	"<function type=\"systemUpdate\" />" \
	"<function type=\"networkRpc\" />" \
	"<function type=\"connectionHandover\" />" \
	"</capabilityInfo>" \

static const char s_capabilityInfoDolce[] = MTP_SPEC_CAPABILITY_INFO_DOLCE;
static const volatile int s_capabilityInfoDolceLength =
	sizeof(MTP_SPEC_CAPABILITY_INFO_DOLCE);

void mtpSpecGetCapabilityInfo(const char **xml, int *length)
{
	if (vshSblAimgrIsDolce()) {
		*xml = s_capabilityInfoDolce;
		*length = s_capabilityInfoDolceLength;
	} else {
		*xml = s_capabilityInfo;
		*length = s_capabilityInfoLength;
	}
}
