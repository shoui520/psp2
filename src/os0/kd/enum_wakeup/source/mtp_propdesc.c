/* SceEnumWakeUp: DevicePropDesc datasets. */
#include "ew_internal.h"

/* a DevicePropDesc dataset with the current value; returns its size */
int mtpBuildPropDesc(SceUInt32 code, char *buf, SceSize size)
{
	MtpDevicePropDescHead *head = (MtpDevicePropDescHead *)buf;
	MtpWireValue *value = (MtpWireValue *)(buf + sizeof(*head));
	const MtpPropDesc *desc;
	MtpWireRange *range;
	MtpWireEnumeration *enumeration;
	const char *str;
	char *current, *form;
	MtpString s;
	int off, end, n, level;
	SceUInt16 count, i;

	desc = mtpFindPropDesc(code);
	if (desc == NULL)
		return 0;
	head->devicePropertyCode = desc->code;
	head->dataType = desc->dataType;
	head->getSet = desc->getSet;

	/* the factory default value */
	switch (desc->dataType) {
	case PTP_DTC_INT8:
	case PTP_DTC_UINT8:
		value->u8 = desc->defaultValue.u8;
		off = sizeof(*head) + 1;
		break;
	case PTP_DTC_INT16:
	case PTP_DTC_UINT16:
		value->u16 = desc->defaultValue.u16;
		off = sizeof(*head) + 2;
		break;
	case PTP_DTC_INT32:
	case PTP_DTC_UINT32:
		value->u32 = desc->defaultValue.u32;
		off = sizeof(*head) + 4;
		break;
	case PTP_DTC_INT64:
	case PTP_DTC_UINT64:
		value->u64[0] = desc->defaultValue.u64[0];
		value->u64[1] = desc->defaultValue.u64[1];
		off = sizeof(*head) + 8;
		break;
	case PTP_DTC_INT128:
	case PTP_DTC_UINT128:
		value->u128[0] = desc->defaultValue.u128[0];
		value->u128[1] = desc->defaultValue.u128[1];
		value->u128[2] = desc->defaultValue.u128[2];
		value->u128[3] = desc->defaultValue.u128[3];
		off = sizeof(*head) + 16;
		break;
	case PTP_DTC_AINT8:
	case PTP_DTC_AUINT8:
		n = mtpPackArray8(value, &desc->defaultValue.array);
		if (n < 0)
			return n;
		off = sizeof(*head) + n;
		break;
	case PTP_DTC_AINT16:
	case PTP_DTC_AUINT16:
		n = mtpPackArray16(value, &desc->defaultValue.array);
		if (n < 0)
			return n;
		off = sizeof(*head) + n;
		break;
	case PTP_DTC_AINT32:
	case PTP_DTC_AUINT32:
		n = mtpPackArray32(value, &desc->defaultValue.array);
		if (n < 0)
			return n;
		off = sizeof(*head) + n;
		break;
	case PTP_DTC_AINT64:
	case PTP_DTC_AUINT64:
		n = mtpPackArray64(value, &desc->defaultValue.array);
		if (n < 0)
			return n;
		off = sizeof(*head) + n;
		break;
	case PTP_DTC_STR:
		s.buf = buf + sizeof(*head) + 1;
		s.length = 0xff;
		n = mtpPackString(&s, desc->defaultValue.str);
		if (n < 0)
			return n;
		value->u8 = s.length;
		off = sizeof(*head) + 1 + n;
		break;
	default:
		return MTP_ERROR_FAILED;
	}

	/* the current value */
	current = buf + off;
	switch (code) {
	case MTP_DPC_SYNCHRONIZATION_PARTNER:
		str = mtpSyncPartner();
		goto string;
	case MTP_DPC_DEVICE_FRIENDLY_NAME:
		str = mtpFriendlyName();
string:
		s.buf = current + 1;
		s.length = 0xff;
		n = mtpPackString(&s, str);
		if (n < 0)
			return n;
		*current = s.length;
		n += 1;
		break;
	case MTP_DPC_DEVICE_ICON:
		n = mtpCacheRead(mtpIconPath(), current + 4, size - 4 - off);
		if (n < 0)
			return n;
		*(MtpUInt32 *)current = n;
		n += 4;
		if (n < 0)
			return n;
		break;
	case PTP_DPC_BATTERY_LEVEL:
		if (scePowerIsBatteryExist() && (level = scePowerGetBatteryLifePercent()) >= 0)
			*current = level;
		else
			*current = 0;
		n = 1;
		break;
	default:
		return MTP_ERROR_FAILED;
	}
	off += n;

	/* the form */
	buf[off] = desc->formFlag;
	form = buf + off + 1;
	end = off + 1;
	switch (desc->formFlag) {
	case PTP_DPFF_RANGE:
		range = (MtpWireRange *)form;
		switch (desc->dataType) {
		case PTP_DTC_UINT16:
			range->range16.min = desc->form.range16.min;
			range->range16.max = desc->form.range16.max;
			range->range16.step = desc->form.range16.step;
			end += sizeof(range->range16);
			break;
		case PTP_DTC_UINT32:
			range->range32.min = desc->form.range32.min;
			range->range32.max = desc->form.range32.max;
			range->range32.step = desc->form.range32.step;
			end += sizeof(range->range32);
			break;
		case PTP_DTC_UINT8:
			range->range8.min = desc->form.range8.min;
			range->range8.max = desc->form.range8.max;
			range->range8.step = desc->form.range8.step;
			end += sizeof(range->range8);
			break;
		}
		break;
	case PTP_DPFF_ENUMERATION:
		enumeration = (MtpWireEnumeration *)form;
		count = desc->form.enumeration.count;
		enumeration->count = count;
		end += sizeof(*enumeration);
		switch (desc->dataType) {
		case PTP_DTC_INT8:
		case PTP_DTC_UINT8: {
			const SceUInt8 *values = desc->form.enumeration.values;

			for (i = 0; i < count; i++)
				enumeration->values[i] = values[i];
			end += count;
			break;
		}
		case PTP_DTC_INT16:
		case PTP_DTC_UINT16: {
			const SceUInt16 *values = desc->form.enumeration.values;

			for (i = 0; i < count; i++)
				((MtpUInt16 *)enumeration->values)[i] = values[i];
			end += count * 2;
			break;
		}
		case PTP_DTC_INT32:
		case PTP_DTC_UINT32: {
			const SceUInt32 *values = desc->form.enumeration.values;

			for (i = 0; i < count; i++)
				((MtpUInt32 *)enumeration->values)[i] = values[i];
			end += count * 4;
			break;
		}
		case PTP_DTC_INT64:
		case PTP_DTC_UINT64: {
			const SceUInt32 *values = desc->form.enumeration.values;

			for (i = 0; i < count; i++) {
				((MtpUInt32 *)enumeration->values)[i * 2] = values[i * 2];
				((MtpUInt32 *)enumeration->values)[i * 2 + 1] = values[i * 2 + 1];
			}
			end += count * 8;
			break;
		}
		}
		break;
	case MTP_DPFF_FIXED_ARRAY:
	case MTP_DPFF_REGULAR_EXPRESSION:
		s.buf = form + 1;
		s.length = 0xff;
		n = mtpPackString(&s, desc->form.str);
		if (n < 0)
			return n;
		*form = s.length;
		end += 1 + n;
		break;
	}
	return end;
}
