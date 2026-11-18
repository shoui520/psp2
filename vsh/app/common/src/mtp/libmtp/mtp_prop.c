/*
 * The properties ("PROP:"): a device or object property's description and value, packed
 * as the host reads them, and an object property's value the host sends, unpacked.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include "mtp_internal.h"

#define MTP_TAG_PROP			"PROP:"

/* ScePower's user exports; no SDK header declares them */
int scePowerIsBatteryExist(void);
int scePowerGetBatteryLifePercent(void);

/* a value of the data type: as many bytes as it is wide, an array or a string */
static int _packFactoryDefaultValue(void *buf, SceUInt16 dataType, const MtpPropValue *value)
{
	MtpString string;
	SceUInt8 *dst = (SceUInt8 *)buf;
	int ret;

	switch (dataType) {
	case MTP_TYPE_INT8:
		mtpUtilMemcpy(dst, value, 1);
		return 1;
	case MTP_TYPE_UINT8:
		mtpUtilMemcpy(dst, value, 1);
		return 1;
	case MTP_TYPE_INT16:
		mtpUtilMemcpy(dst, value, 2);
		return 2;
	case MTP_TYPE_UINT16:
		mtpUtilMemcpy(dst, value, 2);
		return 2;
	case MTP_TYPE_INT32:
		mtpUtilMemcpy(dst, value, 4);
		return 4;
	case MTP_TYPE_UINT32:
		mtpUtilMemcpy(dst, value, 4);
		return 4;
	case MTP_TYPE_INT64:
		mtpUtilMemcpy(dst, value, 8);
		return 8;
	case MTP_TYPE_UINT64:
		mtpUtilMemcpy(dst, value, 8);
		return 8;
	case MTP_TYPE_INT128:
		mtpUtilMemcpy(dst, value, 16);
		return 16;
	case MTP_TYPE_UINT128:
		mtpUtilMemcpy(dst, value, 16);
		return 16;
	case MTP_TYPE_AINT8:
	case MTP_TYPE_AUINT8:
		return mtpUtilPackUInt8Array(dst, (const MtpArray *)value);
	case MTP_TYPE_AINT16:
	case MTP_TYPE_AUINT16:
		return mtpUtilPackCodeList(dst, (const MtpCodeList *)value);
	case MTP_TYPE_AINT32:
	case MTP_TYPE_AUINT32:
		return mtpUtilPackUInt32Array(dst, (const MtpArray *)value);
	case MTP_TYPE_AINT64:
	case MTP_TYPE_AUINT64:
		return mtpUtilPackUInt64Array(dst, (const MtpArray *)value);
	case MTP_TYPE_STR:
		string.numChars = 0xFF;
		string.chars = dst + 1;
		ret = mtpUtilPackMtpString(&string, value->str);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_PROP, 115, "unable to pack string=0x%x\n", ret);
			return ret;
		}
		dst[0] = string.numChars;
		return ret + 1;
	default:
		MTP_ERR(MTP_TAG_PROP, 124, "unknown data type=%d\n", dataType);
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
}

/* the device property's value as the console has it now */
static int _packCurrentValue(SceUInt16 code, void *buf, SceSize size)
{
	MtpString string;
	SceUInt8 *dst = (SceUInt8 *)buf;
	SceUInt32 length = 0;
	SceUInt8 level;
	int percent;
	int ret;

	switch (code) {
	case MTP_DEVICE_PROP_BATTERY_LEVEL:
		level = 0;
		if (scePowerIsBatteryExist() != 0) {
			percent = scePowerGetBatteryLifePercent();
			if (percent >= 0) {
				level = (SceUInt8)percent;
			} else {
				MTP_ERR(MTP_TAG_PROP, 160,
					"unable to get battery status or detecting\n");
				level = 0;
			}
		}
		mtpUtilMemcpy(dst, &level, 1);
		return 1;
	case MTP_DEVICE_PROP_SYNCHRONIZATION_PARTNER:
		string.numChars = 0xFF;
		string.chars = dst + 1;
		ret = mtpUtilPackMtpString(&string, mtpSpecGetSyncPartner());
		if (ret < 0) {
			MTP_ERR(MTP_TAG_PROP, 176,
				"unable to pack synchronization partner=0x%x\n", ret);
			return ret;
		}
		dst[0] = string.numChars;
		return ret + 1;
	case MTP_DEVICE_PROP_DEVICE_FRIENDLY_NAME:
		string.numChars = 0xFF;
		string.chars = dst + 1;
		ret = mtpUtilPackMtpString(&string, mtpSpecGetFriendlyName());
		if (ret < 0) {
			MTP_ERR(MTP_TAG_PROP, 190, "unable to pack friendly name=0x%x\n", ret);
			return ret;
		}
		dst[0] = string.numChars;
		return ret + 1;
	case MTP_DEVICE_PROP_DEVICE_ICON:
		/* the icon file's bytes, after their count */
		ret = mtpFileGetData(mtpSpecGetDeviceIconPath(), dst + 4, size - 4);
		if (ret < 0)
			return ret;
		length = ret;
		mtpUtilMemcpy(dst, &length, 4);
		return ret + 4;
	default:
		MTP_ERR(MTP_TAG_PROP, 230, "prop code=%x is not supported\n", code);
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
}

/* minimum, maximum and step */
static int _packRangeForm(void *buf, SceUInt16 dataType, const MtpPropForm *form)
{
	SceUInt8 *dst = (SceUInt8 *)buf;
	int ret = 0;

	switch (dataType) {
	case MTP_TYPE_UINT32:
		mtpUtilMemcpy(dst, &form->range32.min, 4);
		mtpUtilMemcpy(dst + 4, &form->range32.max, 4);
		mtpUtilMemcpy(dst + 8, &form->range32.step, 4);
		return 12;
	case MTP_TYPE_UINT16:
		mtpUtilMemcpy(dst, &form->range16.min, 2);
		mtpUtilMemcpy(dst + 2, &form->range16.max, 2);
		mtpUtilMemcpy(dst + 4, &form->range16.step, 2);
		return 6;
	case MTP_TYPE_UINT8:
		mtpUtilMemcpy(dst, &form->range8.min, 1);
		mtpUtilMemcpy(dst + 1, &form->range8.max, 1);
		mtpUtilMemcpy(dst + 2, &form->range8.step, 1);
		return 3;
	default:
		MTP_ERR(MTP_TAG_PROP, 280, "not implemented\n");
		break;
	}
	return ret;
}

/* the count, then each value */
static int _packEnumForm(void *buf, SceUInt16 dataType, const MtpPropForm *form)
{
	SceUInt8 *dst = (SceUInt8 *)buf;
	SceUInt16 count = form->enumeration.count;
	const SceUInt8 *values;
	SceUInt16 i;
	int n;

	mtpUtilMemcpy(dst, &form->enumeration.count, 2);
	n = 2;
	switch (dataType) {
	case MTP_TYPE_INT8:
	case MTP_TYPE_UINT8:
		for (i = 0; i < count; i++) {
			values = (const SceUInt8 *)form->enumeration.values;
			mtpUtilMemcpy(dst + n, values + i, 1);
			n += 1;
		}
		break;
	case MTP_TYPE_INT16:
	case MTP_TYPE_UINT16:
		for (i = 0; i < count; i++) {
			values = (const SceUInt8 *)form->enumeration.values;
			mtpUtilMemcpy(dst + n, values + i * 2, 2);
			n += 2;
		}
		break;
	case MTP_TYPE_INT32:
	case MTP_TYPE_UINT32:
		for (i = 0; i < count; i++) {
			values = (const SceUInt8 *)form->enumeration.values;
			mtpUtilMemcpy(dst + n, values + i * 4, 4);
			n += 4;
		}
		break;
	case MTP_TYPE_INT64:
	case MTP_TYPE_UINT64:
		for (i = 0; i < count; i++) {
			values = (const SceUInt8 *)form->enumeration.values;
			mtpUtilMemcpy(dst + n, values + i * 8, 8);
			n += 8;
		}
		break;
	default:
		MTP_ERR(MTP_TAG_PROP, 340,
			"not supported data type(need to implement)=%x", dataType);
		break;
	}
	return n;
}

/* DevicePropDesc: code, data type, get/set, default and current values, then the form */
int mtpPropPackDevicePropDesc(SceUInt16 code, void *buf, SceSize size)
{
	const MtpPropDesc *desc;
	MtpString string;
	SceUInt8 *dst = (SceUInt8 *)buf;
	int ret = 0;
	int n;
	int off;
	int flag;

	desc = mtpSpecGetDevicePropDesc(code);
	if (desc == NULL) {
		MTP_ERR(MTP_TAG_PROP, 373,
			"unable to get device property code(not supported?)\n");
		return ret;
	}
	mtpUtilMemcpy(dst, &desc->code, 2);
	mtpUtilMemcpy(dst + 2, &desc->dataType, 2);
	dst[4] = desc->getSet;
	ret = _packFactoryDefaultValue(dst + 5, desc->dataType, &desc->defaultValue);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_PROP, 389, "unable to pack factory default value=0x%x\n", ret);
		return ret;
	}
	off = ret + 5;
	n = _packCurrentValue(code, dst + off, size - off);
	if (n < 0) {
		MTP_ERR(MTP_TAG_PROP, 397, "unable to pack current value=0x%x\n", n);
		return n;
	}
	flag = off + n;
	ret = flag + 1;
	dst[flag] = desc->formFlag;
	switch (desc->formFlag) {
	case MTP_FORM_RANGE:
		ret += _packRangeForm(dst + ret, desc->dataType, &desc->form);
		break;
	case MTP_FORM_ENUM:
		ret += _packEnumForm(dst + ret, desc->dataType, &desc->form);
		break;
	case MTP_FORM_REGEX:
		string.numChars = 0xFF;
		string.chars = dst + ret + 1;
		ret = mtpUtilPackMtpString(&string, desc->form.str);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_PROP, 422, "unable to pack string=0x%x\n", ret);
			break;
		}
		dst[flag + 1] = string.numChars;
		ret += flag + 2;
		break;
	case MTP_FORM_FIXED_ARRAY:
		string.numChars = 0xFF;
		string.chars = dst + ret + 1;
		ret = mtpUtilPackMtpString(&string, desc->form.str);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_PROP, 435, "unable to pack string=0x%x\n", ret);
			break;
		}
		dst[flag + 1] = string.numChars;
		ret += flag + 2;
		break;
	case MTP_FORM_NONE:
	case MTP_FORM_DATETIME:
		break;
	}
	return ret;
}

int mtpPropPackDevicePropValue(SceUInt16 code, void *buf, SceSize size)
{
	return _packCurrentValue(code, buf, size);
}

int mtpPropPackObjectPropsSupported(SceUInt16 format, MtpCodeArray *array)
{
	const MtpCodeList *list;
	SceUInt32 count;

	list = mtpSpecGetObjectPropsSupported(format);
	if (list == NULL) {
		MTP_ERR(MTP_TAG_PROP, 486, "unable to get object property supported list\n");
		return SCE_MTP_ERROR_UNSUPPORTED_PROPERTY;
	}
	if (array->count < list->count) {
		MTP_ERR(MTP_TAG_PROP, 493, "buffer too small\n");
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	count = list->count;
	array->count = count;
	mtpUtilMemcpy(array->codes, list->codes, count * 2);
	return list->count * 2;
}

/*
 * ObjectPropDesc: code, data type, get/set, default value, group, then the form; a format
 * may have a form of its own
 */
int mtpPropPackObjectPropDesc(SceUInt16 code, SceUInt16 format, void *buf)
{
	const MtpPropDesc *desc;
	const MtpPropForm *form;
	SceUInt8 *dst = (SceUInt8 *)buf;
	int ret;
	int n;

	if (mtpSpecIsSupportedFormat(format) == 0) {
		MTP_ERR(MTP_TAG_PROP, 527, "%04x is not supported format\n", format);
		return SCE_MTP_ERROR_UNSUPPORTED_FORMAT;
	}
	desc = mtpSpecGetObjectPropDesc(code);
	if (desc == NULL) {
		MTP_ERR(MTP_TAG_PROP, 535, "invalid object property=%04x\n", code);
		return SCE_MTP_ERROR_UNSUPPORTED_PROPERTY;
	}
	mtpUtilMemcpy(dst, &desc->code, 2);
	mtpUtilMemcpy(dst + 2, &desc->dataType, 2);
	mtpUtilMemcpy(dst + 4, &desc->getSet, 1);
	ret = _packFactoryDefaultValue(dst + 5, desc->dataType, &desc->defaultValue);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_PROP, 552, "unable to pack factory default value=0x%x\n", ret);
		return ret;
	}
	mtpUtilMemcpy(dst + ret + 5, &desc->groupCode, 4);
	mtpUtilMemcpy(dst + ret + 9, &desc->formFlag, 1);
	n = ret + 10;
	form = (const MtpPropForm *)mtpSpecGetObjectPropFormatForm(code, format);
	if (form == NULL)
		form = &desc->form;
	switch (desc->formFlag) {
	case MTP_FORM_RANGE:
		n += _packRangeForm(dst + n, desc->dataType, form);
		break;
	case MTP_FORM_ENUM:
		n += _packEnumForm(dst + n, desc->dataType, form);
		break;
	case MTP_FORM_BYTE_ARRAY:
		mtpUtilMemcpy(dst + n, form, 4);
		n = ret + 14;
		break;
	case MTP_FORM_NONE:
	case MTP_FORM_DATETIME:
	case MTP_FORM_FIXED_ARRAY:
	case MTP_FORM_REGEX:
		break;
	}
	return n;
}

int mtpPropPackObjectPropValue(const MtpPropData *value, void *buf)
{
	MtpPropValue data;
	SceUInt16 dataType;

	if (value->dataType == MTP_TYPE_STR) {
		data.str = (const char *)value->data;
		dataType = MTP_TYPE_STR;
	} else {
		mtpUtilMemcpy(&data, value->data, 16);
		dataType = value->dataType;
	}
	return _packFactoryDefaultValue(buf, dataType, &data);
}

/* the value into the caller's room; a name too long for it is trimmed */
int mtpPropUnpackObjectPropValue(MtpPropData *value, const void *buf)
{
	MtpString string;
	SceUInt32 dataType = value->dataType;
	char *name;
	SceUInt32 length;
	int ret;
	int n;

	switch (dataType) {
	case MTP_TYPE_INT8:
	case MTP_TYPE_UINT8:
		mtpUtilMemcpy(value->data, buf, 1);
		ret = 1;
		break;
	case MTP_TYPE_INT16:
	case MTP_TYPE_UINT16:
		mtpUtilMemcpy(value->data, buf, 2);
		ret = 2;
		break;
	case MTP_TYPE_INT32:
	case MTP_TYPE_UINT32:
		mtpUtilMemcpy(value->data, buf, 4);
		ret = 4;
		break;
	case MTP_TYPE_INT64:
	case MTP_TYPE_UINT64:
		mtpUtilMemcpy(value->data, buf, 8);
		ret = 8;
		break;
	case MTP_TYPE_INT128:
	case MTP_TYPE_UINT128:
		mtpUtilMemcpy(value->data, buf, 16);
		ret = 16;
		break;
	case MTP_TYPE_STR:
		string.numChars = 0xFF;
		string.chars = (SceUInt8 *)buf + 1;
		length = mtpUtilGetUnpackMtpStringSize(&string);
		if (length > value->size) {
			name = (char *)mtpUtilMalloc(length + 1);
			ret = mtpUtilUnpackMtpString(name, length + 1, &string);
			if (ret < 0) {
				MTP_ERR(MTP_TAG_PROP, 675, "unable to pack string=0x%x\n", ret);
				mtpUtilFree(name);
				break;
			}
			n = mtpUtilTrimFileName(name, (char *)value->data, value->size,
				value->size);
			if (n < 0) {
				mtpUtilFree(name);
				MTP_ERR(MTP_TAG_PROP, 683, "unable to trim file name=0x%x\n", n);
				ret = -2;
				break;
			}
			mtpUtilFree(name);
			break;
		}
		ret = mtpUtilUnpackMtpString((char *)value->data, value->size, &string);
		if (ret < 0)
			MTP_ERR(MTP_TAG_PROP, 692, "unable to pack string=0x%x\n", ret);
		break;
	default:
		MTP_ERR(MTP_TAG_PROP, 700, "not supported data type=%x\n", dataType);
		ret = SCE_MTP_ERROR_NOT_INITIALIZED;
		break;
	}
	return ret;
}
