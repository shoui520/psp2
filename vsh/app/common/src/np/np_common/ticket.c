/*
 * SceNpCommon: reading NP tickets. A ticket is a tree of big-endian fields, each a type and a
 * length followed by its data; the header's first byte holds the major version in its high
 * nibble, the second the minor version. Versions 2.1 and 3.0 have 4-byte field headers;
 * version 4.0 gives some fields a 32-bit length. The entitlements a ticket grants sit in one
 * field as packed records.
 */

#include "np_common_internal.h"

/* field types */
#define NP_TICKET_FIELD_U32		0x0001
#define NP_TICKET_FIELD_U64		0x0002
#define NP_TICKET_FIELD_STRING		0x0004
#define NP_TICKET_FIELD_TIME		0x0007
#define NP_TICKET_FIELD_BINARY		0x0008
#define NP_TICKET_FIELD_BODY		0x3000
#define NP_TICKET_FIELD_ENTITLEMENT	0x3001	/* 2.1: one per entitlement */
#define NP_TICKET_FIELD_ENTITLEMENTS	0x3010	/* 3.0 and 4.0: the packed records */
#define NP_TICKET_FIELD_DATE		0x3011
#define NP_TICKET_FIELD_EXTRA_DATA	0x3012	/* 4.0 */

/* entitlement record flags */
#define NP_TICKET_ENTITLEMENT_TYPE_MASK		0x3
#define NP_TICKET_ENTITLEMENT_HAS_EXPIRE_DATE	0x4

/* a ticket being read */
typedef struct NpTicketReader {
	const SceUInt8 *ticket;
	SceUInt8 version[4];	/* the major version, the minor version */
	SceSize size;
	SceSize offset;
} NpTicketReader;

typedef struct NpTicketFieldV3 {
	SceUInt16 type;
	SceUInt16 length;
} NpTicketFieldV3;

typedef struct NpTicketFieldV4 {
	SceUInt16 type;
	SceUInt32 length;
} NpTicketFieldV4;

/* a field's data being read */
typedef struct NpTicketBlockV3 {
	NpTicketFieldV3 field;
	const SceUInt8 *data;
	SceSize offset;
} NpTicketBlockV3;

typedef struct NpTicketBlockV4 {
	NpTicketFieldV4 field;
	const SceUInt8 *data;
	SceSize offset;
} NpTicketBlockV4;

/* an entitlement record's first byte */
typedef struct NpTicketEntitlementHeader {
	SceUInt8 flags;
	SceUInt8 idLength;
} NpTicketEntitlementHeader;

/* the body's fields in order, with the parameter each holds */
typedef struct NpTicketParamField {
	int paramId;
	SceUInt16 type;
	SceUInt16 size;
} NpTicketParamField;

typedef struct NpTicketParamFieldV2 {
	SceUInt16 type;
	SceUInt16 size;
} NpTicketParamFieldV2;

typedef struct NpTicketGetParamHandler {
	SceUInt16 major;
	SceUInt16 minor;
	int (*func)(NpTicketReader *reader, int paramId, SceNpTicketParam *param);
} NpTicketGetParamHandler;

typedef struct NpTicketGetEntitlementIdListHandler {
	SceUInt16 major;
	SceUInt16 minor;
	int (*func)(NpTicketReader *reader, SceNpEntitlementId *entIdList, SceSize entIdListNum);
} NpTicketGetEntitlementIdListHandler;

typedef struct NpTicketGetEntitlementHandler {
	SceUInt16 major;
	SceUInt16 minor;
	int (*func)(NpTicketReader *reader, const char *entId, SceNpEntitlement *ent, int prefix);
} NpTicketGetEntitlementHandler;

static int ticketGetParamV2(NpTicketReader *reader, int paramId, SceNpTicketParam *param);
static int ticketGetParamV3(NpTicketReader *reader, int paramId, SceNpTicketParam *param);
static int ticketGetParamV4(NpTicketReader *reader, int paramId, SceNpTicketParam *param);
static int ticketGetEntitlementIdListV2(NpTicketReader *reader, SceNpEntitlementId *entIdList,
					SceSize entIdListNum);
static int ticketGetEntitlementIdListV3(NpTicketReader *reader, SceNpEntitlementId *entIdList,
					SceSize entIdListNum);
static int ticketGetEntitlementIdListV4(NpTicketReader *reader, SceNpEntitlementId *entIdList,
					SceSize entIdListNum);
static int ticketGetEntitlementV2(NpTicketReader *reader, const char *entId,
				  SceNpEntitlement *ent, int prefix);
static int ticketGetEntitlementV3(NpTicketReader *reader, const char *entId,
				  SceNpEntitlement *ent, int prefix);
static int ticketGetEntitlementV4(NpTicketReader *reader, const char *entId,
				  SceNpEntitlement *ent, int prefix);

/* the 4.0 field types with a 32-bit length */
static const SceUInt16 s_longFieldTypes[3] = { 0x0000, NP_TICKET_FIELD_BODY, NP_TICKET_FIELD_ENTITLEMENTS };

static const NpTicketParamFieldV2 s_paramFieldsV2[11] = {
	{ NP_TICKET_FIELD_BINARY, SCE_NP_TICKET_SERIAL_ID_SIZE },
	{ NP_TICKET_FIELD_U32, 4 },
	{ NP_TICKET_FIELD_TIME, 8 },
	{ NP_TICKET_FIELD_TIME, 8 },
	{ NP_TICKET_FIELD_U64, 8 },
	{ NP_TICKET_FIELD_STRING, 32 },
	{ NP_TICKET_FIELD_BINARY, SCE_NP_SUBJECT_REGION_SIZE },
	{ NP_TICKET_FIELD_STRING, SCE_NP_SUBJECT_DOMAIN_SIZE },
	{ NP_TICKET_FIELD_BINARY, SCE_NP_SERVICE_ID_SIZE },
	{ NP_TICKET_FIELD_U32, 4 },
	{ NP_TICKET_FIELD_TIME, 8 },
};

static const NpTicketParamField s_paramFields[12] = {
	{ SCE_NP_TICKET_PARAM_SERIAL_ID, NP_TICKET_FIELD_BINARY, SCE_NP_TICKET_SERIAL_ID_SIZE },
	{ SCE_NP_TICKET_PARAM_ISSUER_ID, NP_TICKET_FIELD_U32, 4 },
	{ SCE_NP_TICKET_PARAM_ISSUED_DATE, NP_TICKET_FIELD_TIME, 8 },
	{ SCE_NP_TICKET_PARAM_EXPIRE_DATE, NP_TICKET_FIELD_TIME, 8 },
	{ SCE_NP_TICKET_PARAM_SUBJECT_ACCOUNT_ID, NP_TICKET_FIELD_U64, 8 },
	{ SCE_NP_TICKET_PARAM_SUBJECT_ONLINE_ID, NP_TICKET_FIELD_STRING, 32 },
	{ SCE_NP_TICKET_PARAM_SUBJECT_REGION, NP_TICKET_FIELD_BINARY, SCE_NP_SUBJECT_REGION_SIZE },
	{ SCE_NP_TICKET_PARAM_SUBJECT_DOMAIN, NP_TICKET_FIELD_STRING, SCE_NP_SUBJECT_DOMAIN_SIZE },
	{ SCE_NP_TICKET_PARAM_SERVICE_ID, NP_TICKET_FIELD_BINARY, SCE_NP_SERVICE_ID_SIZE },
	{ SCE_NP_TICKET_PARAM_SUBJECT_DOB, NP_TICKET_FIELD_DATE, 4 },
	{ SCE_NP_TICKET_PARAM_SUBJECT_STATUS, NP_TICKET_FIELD_U32, 4 },
	{ SCE_NP_TICKET_PARAM_STATUS_DURATION, NP_TICKET_FIELD_TIME, 8 },
};

/* 4.0: the parameters after the entitlements */
static const NpTicketParamField s_paramFieldsExtra[2] = {
	{ 13, NP_TICKET_FIELD_BINARY, 8 },
	{ 14, NP_TICKET_FIELD_BINARY, 64 },
};

static const NpTicketGetParamHandler s_getParamHandlers[] = {
	{ 2, 1, ticketGetParamV2 },
	{ 3, 0, ticketGetParamV3 },
	{ 4, 0, ticketGetParamV4 },
	{ 0, 0, NULL },
};

static const NpTicketGetEntitlementIdListHandler s_getEntitlementIdListHandlers[] = {
	{ 2, 1, ticketGetEntitlementIdListV2 },
	{ 3, 0, ticketGetEntitlementIdListV3 },
	{ 4, 0, ticketGetEntitlementIdListV4 },
	{ 0, 0, NULL },
};

static const NpTicketGetEntitlementHandler s_getEntitlementHandlers[] = {
	{ 2, 1, ticketGetEntitlementV2 },
	{ 3, 0, ticketGetEntitlementV3 },
	{ 4, 0, ticketGetEntitlementV4 },
	{ 0, 0, NULL },
};

static const NpTicketGetEntitlementIdListHandler *s_getEntitlementIdListHandler;
static const NpTicketGetEntitlementHandler *s_getEntitlementHandler;
static const NpTicketGetParamHandler *s_getParamHandler;

static inline SceUInt16 ticketSwap16(SceUInt16 value)
{
	return (SceUInt16)((value >> 8) | (value << 8));
}

static inline SceUInt32 ticketSwap32(SceUInt32 value)
{
	return (value << 24) | ((value >> 24) & 0xff) | (((value >> 8) & 0xff) << 16) |
	       (((value >> 16) & 0xff) << 8);
}

static inline SceUInt64 ticketSwap64(SceUInt64 value)
{
	const SceUInt8 *bytes = (const SceUInt8 *)&value;
	SceUInt64 result = 0;
	int i;

	for (i = 0; i < 8; i++)
		result = (result << 8) | bytes[i];
	return result;
}

static inline int ticketReaderInit(NpTicketReader *reader, const unsigned char *ticket,
				   SceSize ticketSize)
{
	int ret = SCE_NP_AUTH_ERROR_EINVAL;

	if (ticketSize >= 4) {
		reader->ticket = ticket;
		reader->size = ticketSize;
		reader->offset = 4;
		sceClibMemcpy_safe(reader->version, ticket, 4);
		reader->version[0] >>= 4;
		ret = 0;
	}
	return ret;
}

static inline SceBool ticketIsLongField(SceUInt16 type)
{
	SceUInt32 i;

	for (i = 0; i < 3; i++) {
		if (type == s_longFieldTypes[i])
			return SCE_TRUE;
	}
	return SCE_FALSE;
}

/* a field whose data is skipped, not entered, when seeking */
static inline SceBool ticketIsLeafField(SceUInt16 type)
{
	return ((type & 0xf000) == 0 && type != 0) ||
	       ((type & 0xf000) == NP_TICKET_FIELD_BODY && (type & 0x10) != 0);
}

int sceNpAuthGetTicketParam(const unsigned char *ticket, SceSize ticketSize, int paramId,
			    SceNpTicketParam *param)
{
	NpTicketReader reader;
	int ret;

	if (ticket == NULL || param == NULL)
		return SCE_NP_AUTH_ERROR_EINVAL;
	sceClibMemset(param, 0, sizeof(*param));
	if (ticketSize < 4)
		return SCE_NP_AUTH_ERROR_EINVAL;
	ret = ticketReaderInit(&reader, ticket, ticketSize);
	if (ret != 0)
		return ret;
	for (s_getParamHandler = s_getParamHandlers; s_getParamHandler->major > 0;
	     s_getParamHandler++) {
		if (s_getParamHandler->major == reader.version[0] &&
		    s_getParamHandler->minor == reader.version[1])
			break;
	}
	if (s_getParamHandler->major == 0)
		return SCE_NP_AUTH_ERROR_UNSUPPORTED_TICKET_VERSION;
	if (s_getParamHandler->major < 4) {
		if (paramId < 0 || paramId >= SCE_NP_TICKET_PARAM_MAX)
			return SCE_NP_AUTH_ERROR_TICKET_PARAM_NOT_FOUND;
	} else {
		if (paramId < 0 || (paramId >= SCE_NP_TICKET_PARAM_MAX && paramId != 13 && paramId != 14))
			return SCE_NP_AUTH_ERROR_TICKET_PARAM_NOT_FOUND;
	}
	return s_getParamHandler->func(&reader, paramId, param);
}

int sceNpAuthGetEntitlementIdList(const unsigned char *ticket, SceSize ticketSize,
				  SceNpEntitlementId *entIdList, SceSize entIdListNum)
{
	NpTicketReader reader;
	int ret;

	if (ticket == NULL)
		return SCE_NP_AUTH_ERROR_EINVAL;
	ret = ticketReaderInit(&reader, ticket, ticketSize);
	if (ret != 0)
		return ret;
	for (s_getEntitlementIdListHandler = s_getEntitlementIdListHandlers;
	     s_getEntitlementIdListHandler->major > 0; s_getEntitlementIdListHandler++) {
		if (s_getEntitlementIdListHandler->major == reader.version[0] &&
		    s_getEntitlementIdListHandler->minor == reader.version[1])
			break;
	}
	if (s_getEntitlementIdListHandler->major == 0)
		return SCE_NP_AUTH_ERROR_UNSUPPORTED_TICKET_VERSION;
	return s_getEntitlementIdListHandler->func(&reader, entIdList, entIdListNum);
}

static int getEntitlement(const unsigned char *ticket, SceSize ticketSize, const char *entId,
			  SceNpEntitlement *ent, int prefix)
{
	NpTicketReader reader;
	int ret;

	if (ticket == NULL || entId == NULL || ent == NULL)
		return SCE_NP_AUTH_ERROR_EINVAL;
	ret = ticketReaderInit(&reader, ticket, ticketSize);
	if (ret != 0)
		return ret;
	for (s_getEntitlementHandler = s_getEntitlementHandlers; s_getEntitlementHandler->major > 0;
	     s_getEntitlementHandler++) {
		if (s_getEntitlementHandler->major == reader.version[0] &&
		    s_getEntitlementHandler->minor == reader.version[1])
			break;
	}
	if (s_getEntitlementHandler->major == 0)
		return SCE_NP_AUTH_ERROR_UNSUPPORTED_TICKET_VERSION;
	return s_getEntitlementHandler->func(&reader, entId, ent, prefix);
}

int sceNpAuthGetEntitlementById(const unsigned char *ticket, SceSize ticketSize,
				const char *entId, SceNpEntitlement *ent)
{
	SceNpEntitlement entitlement;
	int ret;

	if (ticket == NULL)
		return SCE_NP_AUTH_ERROR_EINVAL;
	ret = getEntitlement(ticket, ticketSize, entId, &entitlement, 0);
	if (ent != NULL)
		sceClibMemcpy_safe(ent, &entitlement, sizeof(*ent));
	return ret;
}

int sceNpAuthGetEntitlementByIdPrefix(const unsigned char *ticket, SceSize ticketSize,
				      const char *entId, SceNpEntitlement *ent)
{
	SceNpEntitlement entitlement;
	int ret;

	if (ticket == NULL)
		return SCE_NP_AUTH_ERROR_EINVAL;
	ret = getEntitlement(ticket, ticketSize, entId, &entitlement, 1);
	if (ent != NULL)
		sceClibMemcpy_safe(ent, &entitlement, sizeof(*ent));
	return ret;
}

/* move to the next field of the type: to its header, or with enter set past its header */
static int ticketSeekV4(NpTicketReader *reader, SceUInt16 type, SceBool enter)
{
	SceSize offset = reader->offset;
	SceUInt16 fieldType, rawType;
	SceUInt32 length;
	NpTicketFieldV3 header;
	SceUInt32 rawLength;

	enter &= 1;
	for (;;) {
		if (offset + 2 >= reader->size)
			return SCE_NP_UTIL_ERROR_PARSER_FAILED;
		sceClibMemcpy_safe(&fieldType, reader->ticket + offset, 2);
		fieldType = ticketSwap16(fieldType);
		if (ticketIsLongField(fieldType)) {
			if (offset + 6 >= reader->size)
				return SCE_NP_UTIL_ERROR_PARSER_FAILED;
			sceClibMemcpy_safe(&rawType, reader->ticket + offset, 2);
			sceClibMemcpy_safe(&rawLength, reader->ticket + offset + 2, 4);
			length = ticketSwap32(rawLength);
		} else {
			if (offset + 4 >= reader->size)
				return SCE_NP_UTIL_ERROR_PARSER_FAILED;
			sceClibMemcpy_safe(&header, reader->ticket + offset, 4);
			rawType = header.type;
			length = ticketSwap16(header.length);
		}
		rawType = ticketSwap16(rawType);
		if (rawType == type && !enter)
			break;
		offset += ticketIsLongField(fieldType) ? 6 : 4;
		if (ticketIsLeafField(rawType)) {
			offset += length;
			if (offset > reader->size)
				return SCE_NP_UTIL_ERROR_PARSER_FAILED;
		}
		if (rawType == type)
			break;
	}
	reader->offset = offset;
	return 0;
}

/* the field at the reader's position and where its data starts */
static int ticketGetFieldV4(NpTicketReader *reader, NpTicketFieldV4 *field, const SceUInt8 **data)
{
	SceSize offset = reader->offset;
	SceUInt16 fieldType, rawType;
	NpTicketFieldV3 header;
	SceUInt32 rawLength;

	if (offset + 2 >= reader->size)
		return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	sceClibMemcpy_safe(&fieldType, reader->ticket + offset, 2);
	fieldType = ticketSwap16(fieldType);
	if (ticketIsLongField(fieldType)) {
		if (offset + 8 >= reader->size)
			return SCE_NP_UTIL_ERROR_PARSER_FAILED;
		sceClibMemcpy_safe(&rawType, reader->ticket + offset, 2);
		sceClibMemcpy_safe(&rawLength, reader->ticket + offset + 2, 4);
		field->type = ticketSwap16(rawType);
		field->length = ticketSwap32(rawLength);
	} else {
		if (offset + 4 >= reader->size)
			return SCE_NP_UTIL_ERROR_PARSER_FAILED;
		sceClibMemcpy_safe(&header, reader->ticket + offset, 4);
		field->type = ticketSwap16(header.type);
		field->length = ticketSwap16(header.length);
	}
	if (offset + field->length + 4 > reader->size)
		return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	if (data != NULL)
		*data = reader->ticket + offset + (ticketIsLongField(fieldType) ? 6 : 4);
	return 0;
}

/* read the field at the reader's position into param (or data) and move past it */
static int ticketReadFieldV4(NpTicketReader *reader, SceNpTicketParam *param, void *data,
			     SceSize *dataSize)
{
	SceSize offset = reader->offset;
	NpTicketFieldV4 field;
	SceUInt32 u32;
	SceUInt64 u64;
	int ret;

	ret = ticketGetFieldV4(reader, &field, NULL);
	if (ret < 0)
		return ret;
	if (field.type == 0 ||
	    ((field.type & 0xf000) == NP_TICKET_FIELD_BODY && (field.type & 0x10) == 0))
		return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	offset += ticketIsLongField(field.type) ? 6 : 4;
	if (param != NULL)
		sceClibMemset(param, 0, sizeof(*param));
	switch (field.type) {
	case NP_TICKET_FIELD_U32:
		sceClibMemcpy_safe(&u32, reader->ticket + offset, 4);
		param->u32 = ticketSwap32(u32);
		break;
	case NP_TICKET_FIELD_U64:
	case NP_TICKET_FIELD_TIME:
		sceClibMemcpy_safe(&u64, reader->ticket + offset, 8);
		param->u64 = ticketSwap64(u64);
		break;
	case NP_TICKET_FIELD_STRING:
	case NP_TICKET_FIELD_BINARY:
		if (field.length >= SCE_NP_TICKET_PARAM_DATA_LEN) {
			sceClibMemcpy_safe(param, reader->ticket + offset, SCE_NP_TICKET_PARAM_DATA_LEN - 1);
			param->data[SCE_NP_TICKET_PARAM_DATA_LEN - 1] = '\0';
		} else {
			sceClibMemcpy_safe(param, reader->ticket + offset, field.length);
			param->data[field.length] = '\0';
		}
		break;
	case NP_TICKET_FIELD_DATE:
		sceClibMemcpy_safe(&u32, reader->ticket + offset, 4);
		u32 = ticketSwap32(u32);
		param->date.year = (SceUShort16)(u32 >> 16);
		param->date.month = (SceUChar8)(u32 >> 8);
		param->date.day = (SceUChar8)u32;
		break;
	case NP_TICKET_FIELD_EXTRA_DATA:
		if (data != NULL)
			sceClibMemcpy_safe(data, reader->ticket + offset, field.length);
		if (dataSize != NULL)
			*dataSize = field.length;
		break;
	default:
		break;
	}
	reader->offset = offset + field.length;
	return field.type;
}

/* the 4.0 field of type 0x3012, copied out with its size */
int SceNpCommon_85503FA6(const unsigned char *ticket, SceSize ticketSize, void *data,
			 SceSize *dataSize)
{
	NpTicketReader reader;
	int ret;

	if (ticket == NULL)
		return SCE_NP_AUTH_ERROR_EINVAL;
	ret = ticketReaderInit(&reader, ticket, ticketSize);
	if (ret < 0)
		return ret;
	if (reader.version[0] < 4)
		return SCE_NP_AUTH_ERROR_UNSUPPORTED_TICKET_VERSION;
	reader.offset = 4;
	if (ticketSeekV4(&reader, NP_TICKET_FIELD_EXTRA_DATA, SCE_FALSE) != 0)
		return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	ret = ticketReadFieldV4(&reader, NULL, data, dataSize);
	if (ret < 0)
		return ret;
	return 0;
}

/* 2.1 and 3.0: move to the next field of the type */
static int ticketSeekV3(NpTicketReader *reader, SceUInt16 type, SceBool enter)
{
	SceSize offset = reader->offset;
	SceSize next = offset + 4;
	NpTicketFieldV3 header;

	if (next >= reader->size)
		return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	enter &= 1;
	for (;;) {
		sceClibMemcpy_safe(&header, reader->ticket + offset, 4);
		header.type = ticketSwap16(header.type);
		header.length = ticketSwap16(header.length);
		if (header.type == type && !enter)
			break;
		offset = next;
		if (ticketIsLeafField(header.type)) {
			offset = next + header.length;
			if (offset > reader->size)
				return SCE_NP_UTIL_ERROR_PARSER_FAILED;
		}
		if (header.type == type)
			break;
		next = offset + 4;
		if (next >= reader->size)
			return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	}
	reader->offset = offset;
	return 0;
}

static int ticketGetFieldV3(NpTicketReader *reader, NpTicketFieldV3 *field, const SceUInt8 **data)
{
	SceSize offset = reader->offset;

	if (offset + 4 >= reader->size)
		return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	sceClibMemcpy_safe(field, reader->ticket + offset, 4);
	field->length = ticketSwap16(field->length);
	field->type = ticketSwap16(field->type);
	if (offset + field->length + 4 > reader->size)
		return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	if (data != NULL)
		*data = reader->ticket + offset + 4;
	return 0;
}

static int ticketReadFieldV3(NpTicketReader *reader, SceNpTicketParam *param)
{
	SceSize offset = reader->offset;
	NpTicketFieldV3 field;
	SceUInt32 u32;
	SceUInt64 u64;
	int ret;

	ret = ticketGetFieldV3(reader, &field, NULL);
	if (ret < 0)
		return ret;
	if (field.type == 0 ||
	    ((field.type & 0xf000) == NP_TICKET_FIELD_BODY && (field.type & 0x10) == 0))
		return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	offset += 4;
	sceClibMemset(param, 0, sizeof(*param));
	switch (field.type) {
	case NP_TICKET_FIELD_U32:
		sceClibMemcpy_safe(&u32, reader->ticket + offset, 4);
		param->u32 = ticketSwap32(u32);
		break;
	case NP_TICKET_FIELD_U64:
	case NP_TICKET_FIELD_TIME:
		sceClibMemcpy_safe(&u64, reader->ticket + offset, 8);
		param->u64 = ticketSwap64(u64);
		break;
	case NP_TICKET_FIELD_STRING:
	case NP_TICKET_FIELD_BINARY:
		if (field.length >= SCE_NP_TICKET_PARAM_DATA_LEN) {
			sceClibMemcpy_safe(param, reader->ticket + offset, SCE_NP_TICKET_PARAM_DATA_LEN - 1);
			param->data[SCE_NP_TICKET_PARAM_DATA_LEN - 1] = '\0';
		} else {
			sceClibMemcpy_safe(param, reader->ticket + offset, field.length);
			param->data[field.length] = '\0';
		}
		break;
	case NP_TICKET_FIELD_DATE:
		sceClibMemcpy_safe(&u32, reader->ticket + offset, 4);
		u32 = ticketSwap32(u32);
		param->date.year = (SceUShort16)(u32 >> 16);
		param->date.month = (SceUChar8)(u32 >> 8);
		param->date.day = (SceUChar8)u32;
		break;
	default:
		break;
	}
	reader->offset = offset + field.length;
	return field.type;
}

static int ticketGetParamV2(NpTicketReader *reader, int paramId, SceNpTicketParam *param)
{
	int i, ret;

	if (paramId > SCE_NP_TICKET_PARAM_STATUS_DURATION)
		return SCE_NP_AUTH_ERROR_TICKET_PARAM_NOT_FOUND;
	ret = ticketSeekV3(reader, NP_TICKET_FIELD_BODY, SCE_TRUE);
	if (ret != 0)
		return ret;
	for (i = 0; i <= paramId; i++) {
		if (ticketReadFieldV3(reader, param) != s_paramFieldsV2[i].type)
			return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	}
	return 0;
}

static int ticketGetParamV3(NpTicketReader *reader, int paramId, SceNpTicketParam *param)
{
	int i, ret;

	if (paramId >= SCE_NP_TICKET_PARAM_MAX)
		return SCE_NP_AUTH_ERROR_TICKET_PARAM_NOT_FOUND;
	ret = ticketSeekV3(reader, NP_TICKET_FIELD_BODY, SCE_TRUE);
	if (ret != 0)
		return ret;
	for (i = 0; i < SCE_NP_TICKET_PARAM_MAX; i++) {
		if (ticketReadFieldV3(reader, param) != s_paramFields[i].type)
			return SCE_NP_UTIL_ERROR_PARSER_FAILED;
		if (s_paramFields[i].paramId == paramId)
			break;
	}
	return 0;
}

static int ticketGetParamV4(NpTicketReader *reader, int paramId, SceNpTicketParam *param)
{
	int i, type, ret;

	if (paramId >= SCE_NP_TICKET_PARAM_MAX && paramId != 13 && paramId != 14)
		return SCE_NP_AUTH_ERROR_TICKET_PARAM_NOT_FOUND;
	ret = ticketSeekV4(reader, NP_TICKET_FIELD_BODY, SCE_TRUE);
	if (ret != 0)
		return ret;
	for (i = 0; i < SCE_NP_TICKET_PARAM_MAX; i++) {
		type = ticketReadFieldV4(reader, param, NULL, NULL);
		if (type != s_paramFields[i].type) {
			if (type != NP_TICKET_FIELD_ENTITLEMENTS)
				return SCE_NP_UTIL_ERROR_PARSER_FAILED;
			break;
		}
		if (s_paramFields[i].paramId == paramId)
			return 0;
	}
	ret = ticketSeekV4(reader, NP_TICKET_FIELD_BINARY, SCE_FALSE);
	if (ret < 0)
		return ret;
	for (i = 0; i < 2; i++) {
		if (ticketReadFieldV4(reader, param, NULL, NULL) != s_paramFieldsExtra[i].type)
			return SCE_NP_UTIL_ERROR_PARSER_FAILED;
		if (s_paramFieldsExtra[i].paramId == paramId)
			return 0;
	}
	return SCE_NP_AUTH_ERROR_TICKET_PARAM_NOT_FOUND;
}

static int ticketGetEntitlementIdListV2(NpTicketReader *reader, SceNpEntitlementId *entIdList,
					SceSize entIdListNum)
{
	SceNpTicketParam param;
	SceSize i;

	for (i = 0;; i++) {
		if (ticketSeekV3(reader, NP_TICKET_FIELD_ENTITLEMENT, SCE_TRUE) != 0)
			return i;
		if (ticketReadFieldV3(reader, &param) != NP_TICKET_FIELD_BINARY)
			return SCE_NP_UTIL_ERROR_PARSER_FAILED;
		if (entIdList != NULL && entIdListNum > i)
			sceClibMemcpy_safe(&entIdList[i], &param, sizeof(SceNpEntitlementId));
	}
}

static int ticketReadEntitlementHeaderV3(NpTicketBlockV3 *block, NpTicketEntitlementHeader *header)
{
	SceUInt8 byte;

	if (block->offset + 1 > block->field.length)
		return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	sceClibMemcpy_safe(&byte, block->data + block->offset, 1);
	block->offset++;
	header->flags = ((byte >> 6) & 3) | (((byte >> 5) & 1) << 2);
	header->idLength = byte & 0x1f;
	if ((byte & 0x1f) == 0)
		header->idLength = 32;
	return 0;
}

static int ticketReadEntitlementHeaderV4(NpTicketBlockV4 *block, NpTicketEntitlementHeader *header)
{
	SceUInt8 byte;

	if (block->offset + 1 > block->field.length)
		return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	sceClibMemcpy_safe(&byte, block->data + block->offset, 1);
	block->offset++;
	header->flags = ((byte >> 6) & 3) | (((byte >> 5) & 1) << 2);
	header->idLength = byte & 0x1f;
	if ((byte & 0x1f) == 0)
		header->idLength = 32;
	return 0;
}

/*
 * The record's ID: a short one (up to 8 characters) follows the service ID and a hyphen,
 * a longer one stands alone.
 */
static int ticketReadEntitlementIdV3(NpTicketBlockV3 *block, const char *serviceId,
				     SceSize serviceIdLength, const NpTicketEntitlementHeader *header,
				     char *entId)
{
	SceUInt8 id[SCE_NP_ENTITLEMENT_ID_SIZE];
	SceSize length = header->idLength;

	if (block->offset + length > block->field.length)
		return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	sceClibMemcpy_safe(id, block->data + block->offset, header->idLength);
	block->offset += length;
	if (entId != NULL) {
		if (header->idLength <= 8) {
			if (serviceIdLength + header->idLength + 2 > SCE_NP_ENTITLEMENT_ID_SIZE)
				return SCE_NP_UTIL_ERROR_PARSER_FAILED;
			sceClibMemcpy_safe(entId, serviceId, serviceIdLength);
			entId[serviceIdLength] = '-';
			sceClibMemcpy_safe(&entId[serviceIdLength + 1], id, header->idLength);
			entId[header->idLength + 1 + serviceIdLength] = '\0';
		} else {
			sceClibMemset(entId, 0, SCE_NP_ENTITLEMENT_ID_SIZE);
			sceClibMemcpy_safe(entId, id, header->idLength);
		}
	}
	return 0;
}

static int ticketReadEntitlementIdV4(NpTicketBlockV4 *block, const char *serviceId,
				     SceSize serviceIdLength, const NpTicketEntitlementHeader *header,
				     char *entId)
{
	SceUInt8 id[SCE_NP_ENTITLEMENT_ID_SIZE];
	SceSize length = header->idLength;

	if (block->offset + length > block->field.length)
		return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	sceClibMemcpy_safe(id, block->data + block->offset, header->idLength);
	block->offset += length;
	if (entId != NULL) {
		if (header->idLength <= 8) {
			if (serviceIdLength + header->idLength + 2 > SCE_NP_ENTITLEMENT_ID_SIZE)
				return SCE_NP_UTIL_ERROR_PARSER_FAILED;
			sceClibMemcpy_safe(entId, serviceId, serviceIdLength);
			entId[serviceIdLength] = '-';
			sceClibMemcpy_safe(&entId[serviceIdLength + 1], id, header->idLength);
			entId[header->idLength + 1 + serviceIdLength] = '\0';
		} else {
			sceClibMemset(entId, 0, SCE_NP_ENTITLEMENT_ID_SIZE);
			sceClibMemcpy_safe(entId, id, header->idLength);
		}
	}
	return 0;
}

static inline int ticketBlockReadV3(NpTicketBlockV3 *block, void *buffer, SceSize size)
{
	if (block->offset + size > block->field.length)
		return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	sceClibMemcpy_safe(buffer, block->data + block->offset, size);
	block->offset += size;
	return 0;
}

static inline int ticketBlockReadV4(NpTicketBlockV4 *block, void *buffer, SceSize size)
{
	if (block->offset + size > block->field.length)
		return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	sceClibMemcpy_safe(buffer, block->data + block->offset, size);
	block->offset += size;
	return 0;
}

/* the record's dates and counts */
static int ticketReadEntitlementV3(NpTicketBlockV3 *block, const NpTicketEntitlementHeader *header,
				   SceNpEntitlement *ent)
{
	SceUInt64 u64;
	SceUInt32 u32;
	int ret;

	ent->type = header->flags & NP_TICKET_ENTITLEMENT_TYPE_MASK;
	ret = ticketBlockReadV3(block, &u64, 8);
	if (ret < 0)
		return ret;
	ent->createdDate = ticketSwap64(u64);
	if (header->flags & NP_TICKET_ENTITLEMENT_HAS_EXPIRE_DATE) {
		ret = ticketBlockReadV3(block, &u64, 8);
		if (ret < 0)
			return ret;
		ent->expireDate = ticketSwap64(u64);
	} else {
		ent->expireDate = 0;
	}
	if ((header->flags & NP_TICKET_ENTITLEMENT_TYPE_MASK) == SCE_NP_ENTITLEMENT_TYPE_CONSUMABLE) {
		ret = ticketBlockReadV3(block, &u32, 4);
		if (ret < 0)
			return ret;
		ent->remainingCount = ticketSwap32(u32);
		ret = ticketBlockReadV3(block, &u32, 4);
		if (ret < 0)
			return ret;
		ent->consumedCount = ticketSwap32(u32);
	} else {
		ent->remainingCount = 0;
		ent->consumedCount = 0;
	}
	return 0;
}

static int ticketReadEntitlementV4(NpTicketBlockV4 *block, const NpTicketEntitlementHeader *header,
				   SceNpEntitlement *ent)
{
	SceUInt64 u64;
	SceUInt32 u32;
	int ret;

	ent->type = header->flags & NP_TICKET_ENTITLEMENT_TYPE_MASK;
	ret = ticketBlockReadV4(block, &u64, 8);
	if (ret < 0)
		return ret;
	ent->createdDate = ticketSwap64(u64);
	if (header->flags & NP_TICKET_ENTITLEMENT_HAS_EXPIRE_DATE) {
		ret = ticketBlockReadV4(block, &u64, 8);
		if (ret < 0)
			return ret;
		ent->expireDate = ticketSwap64(u64);
	} else {
		ent->expireDate = 0;
	}
	if ((header->flags & NP_TICKET_ENTITLEMENT_TYPE_MASK) == SCE_NP_ENTITLEMENT_TYPE_CONSUMABLE) {
		ret = ticketBlockReadV4(block, &u32, 4);
		if (ret < 0)
			return ret;
		ent->remainingCount = ticketSwap32(u32);
		ret = ticketBlockReadV4(block, &u32, 4);
		if (ret < 0)
			return ret;
		ent->consumedCount = ticketSwap32(u32);
	} else {
		ent->remainingCount = 0;
		ent->consumedCount = 0;
	}
	return 0;
}

/* the bytes after a record's ID */
static inline SceSize ticketEntitlementSize(const NpTicketEntitlementHeader *header)
{
	SceSize size = 8;

	if (header->flags & NP_TICKET_ENTITLEMENT_HAS_EXPIRE_DATE)
		size = 16;
	if ((header->flags & NP_TICKET_ENTITLEMENT_TYPE_MASK) == SCE_NP_ENTITLEMENT_TYPE_CONSUMABLE)
		size += 8;
	return size;
}

static int ticketGetEntitlementIdListV3(NpTicketReader *reader, SceNpEntitlementId *entIdList,
					SceSize entIdListNum)
{
	NpTicketEntitlementHeader header;
	NpTicketBlockV3 block;
	SceNpTicketParam serviceId;
	SceSize serviceIdLength, offset, count;
	int ret;

	reader->offset = 4;
	ret = ticketGetParamV3(reader, SCE_NP_TICKET_PARAM_SERVICE_ID, &serviceId);
	if (ret < 0)
		return ret;
	serviceIdLength = sceClibStrnlen((const char *)serviceId.data, 0x7fffffff);
	reader->offset = 4;
	if (ticketSeekV3(reader, NP_TICKET_FIELD_ENTITLEMENTS, SCE_FALSE) != 0)
		return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	ret = ticketGetFieldV3(reader, &block.field, &block.data);
	if (ret < 0)
		return ret;
	block.offset = 0;
	count = 0;
	if (block.field.length == 0)
		return count;
	for (;;) {
		ret = ticketReadEntitlementHeaderV3(&block, &header);
		if (ret < 0)
			return ret;
		ret = ticketReadEntitlementIdV3(&block, (const char *)serviceId.data, serviceIdLength,
						&header,
						entIdList != NULL && entIdListNum > count ?
							(char *)&entIdList[count] : NULL);
		if (ret < 0)
			return ret;
		offset = ticketEntitlementSize(&header) + block.offset;
		if (offset > block.field.length)
			return SCE_NP_UTIL_ERROR_PARSER_FAILED;
		block.offset = offset;
		count++;
		if (offset >= block.field.length)
			return count;
	}
}

static int ticketGetEntitlementIdListV4(NpTicketReader *reader, SceNpEntitlementId *entIdList,
					SceSize entIdListNum)
{
	NpTicketEntitlementHeader header;
	NpTicketBlockV4 block;
	SceNpTicketParam serviceId;
	SceSize serviceIdLength, offset, count;
	int ret;

	reader->offset = 4;
	ret = ticketGetParamV4(reader, SCE_NP_TICKET_PARAM_SERVICE_ID, &serviceId);
	if (ret < 0)
		return ret;
	serviceIdLength = sceClibStrnlen((const char *)serviceId.data, 0x7fffffff);
	reader->offset = 4;
	if (ticketSeekV4(reader, NP_TICKET_FIELD_ENTITLEMENTS, SCE_FALSE) != 0)
		return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	ret = ticketGetFieldV4(reader, &block.field, &block.data);
	if (ret < 0)
		return ret;
	block.offset = 0;
	count = 0;
	if (block.field.length == 0)
		return count;
	for (;;) {
		ret = ticketReadEntitlementHeaderV4(&block, &header);
		if (ret < 0)
			return ret;
		ret = ticketReadEntitlementIdV4(&block, (const char *)serviceId.data, serviceIdLength,
						&header,
						entIdList != NULL && entIdListNum > count ?
							(char *)&entIdList[count] : NULL);
		if (ret < 0)
			return ret;
		offset = ticketEntitlementSize(&header) + block.offset;
		if (offset > block.field.length)
			return SCE_NP_UTIL_ERROR_PARSER_FAILED;
		block.offset = offset;
		count++;
		if (offset >= block.field.length)
			return count;
	}
}

static int ticketGetEntitlementV2(NpTicketReader *reader, const char *entId,
				  SceNpEntitlement *ent, int prefix)
{
	SceNpTicketParam param;
	SceSize length;

	if (prefix == 1)
		length = sceClibStrnlen(entId, 0x7fffffff);
	else
		length = sceClibStrnlen(entId, 0x7fffffff) + 1;
	do {
		if (ticketSeekV3(reader, NP_TICKET_FIELD_ENTITLEMENT, SCE_TRUE) != 0)
			return SCE_NP_AUTH_ERROR_ESRCH;
		if (ticketReadFieldV3(reader, &param) != NP_TICKET_FIELD_BINARY)
			return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	} while (sceClibMemcmp(entId, &param, length) != 0);
	sceClibMemset(ent, 0, sizeof(*ent));
	sceClibMemcpy_safe(&ent->id, &param, sizeof(ent->id));
	if (ticketReadFieldV3(reader, &param) != NP_TICKET_FIELD_TIME)
		return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	sceClibMemcpy_safe(&ent->createdDate, &param, sizeof(ent->createdDate));
	if (ticketReadFieldV3(reader, &param) != NP_TICKET_FIELD_TIME)
		return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	sceClibMemcpy_safe(&ent->expireDate, &param, sizeof(ent->expireDate));
	if (ticketReadFieldV3(reader, &param) != NP_TICKET_FIELD_U32)
		return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	sceClibMemcpy_safe(&ent->type, &param, sizeof(ent->type));
	if (ent->type != SCE_NP_ENTITLEMENT_TYPE_CONSUMABLE)
		return 0;
	if (ticketReadFieldV3(reader, &param) != NP_TICKET_FIELD_U32)
		return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	sceClibMemcpy_safe(&ent->remainingCount, &param, sizeof(ent->remainingCount));
	if (ticketReadFieldV3(reader, &param) != NP_TICKET_FIELD_U32)
		return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	sceClibMemcpy_safe(&ent->consumedCount, &param, sizeof(ent->consumedCount));
	return 0;
}

static inline int ticketBlockSkipV3(NpTicketBlockV3 *block, SceSize size)
{
	if (block->offset + size > block->field.length)
		return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	block->offset += size;
	return 0;
}

static inline int ticketBlockSkipV4(NpTicketBlockV4 *block, SceSize size)
{
	if (block->offset + size > block->field.length)
		return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	block->offset += size;
	return 0;
}

static int ticketGetEntitlementV3(NpTicketReader *reader, const char *entId,
				  SceNpEntitlement *ent, int prefix)
{
	NpTicketEntitlementHeader header;
	NpTicketBlockV3 block;
	char id[SCE_NP_ENTITLEMENT_ID_SIZE];
	SceNpTicketParam serviceId;
	SceSize length, serviceIdLength;
	int ret;

	if (prefix == 1)
		length = sceClibStrnlen(entId, 0x7fffffff);
	else
		length = sceClibStrnlen(entId, 0x7fffffff) + 1;
	if (length > SCE_NP_ENTITLEMENT_ID_SIZE)
		return SCE_NP_AUTH_ERROR_EINVAL;
	reader->offset = 4;
	ret = ticketGetParamV3(reader, SCE_NP_TICKET_PARAM_SERVICE_ID, &serviceId);
	if (ret < 0)
		return ret;
	serviceIdLength = sceClibStrnlen((const char *)serviceId.data, 0x7fffffff);
	reader->offset = 4;
	if (ticketSeekV3(reader, NP_TICKET_FIELD_ENTITLEMENTS, SCE_FALSE) != 0)
		return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	if (ticketSeekV3(reader, NP_TICKET_FIELD_ENTITLEMENTS, SCE_FALSE) != 0)
		return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	ret = ticketGetFieldV3(reader, &block.field, &block.data);
	if (ret < 0)
		return ret;
	block.offset = 0;
	if (block.field.length == 0)
		return SCE_NP_AUTH_ERROR_ESRCH;
	for (;;) {
		ret = ticketReadEntitlementHeaderV3(&block, &header);
		if (ret < 0)
			return ret;
		ret = ticketReadEntitlementIdV3(&block, (const char *)serviceId.data, serviceIdLength,
						&header, id);
		if (ret < 0)
			return ret;
		if (sceClibMemcmp(entId, id, length) == 0) {
			sceClibMemset(ent, 0, sizeof(*ent));
			ret = ticketReadEntitlementV3(&block, &header, ent);
			if (ret < 0)
				return ret;
			sceClibMemcpy_safe(ent, entId, length);
			return 0;
		}
		ret = ticketBlockSkipV3(&block, ticketEntitlementSize(&header));
		if (ret < 0)
			return ret;
		if (block.offset >= block.field.length)
			return SCE_NP_AUTH_ERROR_ESRCH;
	}
}

static int ticketGetEntitlementV4(NpTicketReader *reader, const char *entId,
				  SceNpEntitlement *ent, int prefix)
{
	NpTicketEntitlementHeader header;
	NpTicketBlockV4 block;
	char id[SCE_NP_ENTITLEMENT_ID_SIZE];
	SceNpTicketParam serviceId;
	SceSize length, serviceIdLength;
	int ret;

	if (prefix == 1)
		length = sceClibStrnlen(entId, 0x7fffffff);
	else
		length = sceClibStrnlen(entId, 0x7fffffff) + 1;
	if (length > SCE_NP_ENTITLEMENT_ID_SIZE)
		return SCE_NP_AUTH_ERROR_EINVAL;
	reader->offset = 4;
	ret = ticketGetParamV4(reader, SCE_NP_TICKET_PARAM_SERVICE_ID, &serviceId);
	if (ret < 0)
		return ret;
	serviceIdLength = sceClibStrnlen((const char *)serviceId.data, 0x7fffffff);
	reader->offset = 4;
	if (ticketSeekV4(reader, NP_TICKET_FIELD_ENTITLEMENTS, SCE_FALSE) != 0)
		return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	if (ticketSeekV4(reader, NP_TICKET_FIELD_ENTITLEMENTS, SCE_FALSE) != 0)
		return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	ret = ticketGetFieldV4(reader, &block.field, &block.data);
	if (ret < 0)
		return ret;
	block.offset = 0;
	if (block.field.length == 0)
		return SCE_NP_AUTH_ERROR_ESRCH;
	for (;;) {
		ret = ticketReadEntitlementHeaderV4(&block, &header);
		if (ret < 0)
			return ret;
		ret = ticketReadEntitlementIdV4(&block, (const char *)serviceId.data, serviceIdLength,
						&header, id);
		if (ret < 0)
			return ret;
		if (sceClibMemcmp(entId, id, length) == 0) {
			sceClibMemset(ent, 0, sizeof(*ent));
			ret = ticketReadEntitlementV4(&block, &header, ent);
			if (ret < 0)
				return ret;
			sceClibMemcpy_safe(ent, entId, length);
			return 0;
		}
		ret = ticketBlockSkipV4(&block, ticketEntitlementSize(&header));
		if (ret < 0)
			return ret;
		if (block.offset >= block.field.length)
			return SCE_NP_AUTH_ERROR_ESRCH;
	}
}
