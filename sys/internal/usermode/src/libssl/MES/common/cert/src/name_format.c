/* Private function names and filename are ours; MES component path is recovered. */
#include "ri_cert.h"
#include "../../../../asn1.h"
#include "../../../../memory.h"
#include "../../../../objects/objects.h"

static const char *const sslCertificateNameLabels[][2] = {
	{ "\x55\x04\x03", "CN" },
	{ "\x55\x04\x04", "SN" },
	{ "\x55\x04\x06", "C" },
	{ "\x55\x04\x07", "L" },
	{ "\x55\x04\x08", "ST" },
	{ "\x55\x04\x09", "STREET" },
	{ "\x55\x04\x0a", "O" },
	{ "\x55\x04\x0b", "OU" },
	{ "\x2a\x86\x48\x86\xf7\x0d\x01\x09\x01", "EmailAddress" }
};

int sslFormatCertificateHex(const SslCertificatePropertyValue *value,
			    SceUInt32 capacity, int mode, char *data)
{
	SslAsn1Element element = { 0 };
	SceUInt8 header[6];
	SceUInt32 position = 0, i, headerLength;
	unsigned int byte, high, low;
	char alphabet = 'A';

	if (mode == 1) {
		alphabet = 'a';
		element.tag = value->value;
		element.length = value->length;
		element.data = value->data;
		if (element.tag == 16 || element.tag == 17)
			element.encoding = 32;
		element.flags = 8;
		if ((SceUInt32)sslAsn1HeaderSize(&element) > sizeof(header))
			return 10023;
		headerLength = sslAsn1EncodeHeader(&element, header);
		/* The original capacity test counts header bytes before hex expansion. */
		if (headerLength + 2 * value->length + 1 >= capacity)
			return 10016;
		data[position++] = '#';
		for (i = 0; i < headerLength; i++) {
			byte = header[i];
			high = byte >> 4;
			low = byte & 15;
			data[position++] = high + (high < 10 ? '0' : 'a' - 10);
			data[position++] = low + (low < 10 ? '0' : 'a' - 10);
		}
	}
	for (i = 0; i < value->length; i++) {
		byte = ((const SceUInt8 *)value->data)[i];
		high = byte >> 4;
		low = byte & 15;
		data[position++] = high + (high < 10 ? '0' : alphabet - 10);
		data[position++] = low + (low < 10 ? '0' : alphabet - 10);
		if (mode == 2)
			data[position++] = ':';
	}
	if (mode == 2 && (SceInt32)position > 0)
		position--;
	data[position] = '\0';
	return 0;
}

int sslFormatCertificateString(const SslCertificatePropertyValue *value,
			       SceUInt32 capacity, int mode, char *data)
{
	SceUInt32 index = 0, position = 0, length = value->length;
	SceUInt32 code, tag;
	const SceUInt8 *input;
	char *output = data;

	while (index < length) {
		input = (const SceUInt8 *)value->data + index;
		code = input[0];
		tag = value->value;
		index++;
		if (tag == 30) {
			code = (code << 8) | input[1];
			index++;
			if (code >= 0xd800 && code <= 0xdbff) {
				code = (code << 10) + (input[2] << 8) + input[3] - 56613888;
				index += 2;
			}
		} else if (tag == 28) {
			code = (code << 24) | (input[1] << 16) |
			       (input[2] << 8) | input[3];
			index += 3;
		}
		if (code < 128) {
			if (mode == 1 &&
			    ((code == ' ' && (index == 1 || index == length)) ||
			     (code == '#' && index == 1) || code == ',' || code == '+' ||
			     code == '=' || code == '"' || code == '>' || code == '<' ||
			     code == ';' || code == '\\')) {
				if (position >= capacity)
					return 10016;
				*output++ = '\\';
				position++;
			}
			if (position >= capacity)
				return 10016;
			*output++ = code;
			position++;
		} else if (tag == 12) {
			if (position >= capacity)
				return 10016;
			*output++ = code;
			position++;
		} else if (code < 0x800) {
			if (position + 1 >= capacity)
				return 10016;
			output[0] = (code >> 6) | 0xc0;
			output[1] = (code & 63) | 0x80;
			output += 2;
			position += 2;
		} else if (code < 0x10000) {
			if (position + 2 >= capacity)
				return 10016;
			output[1] = ((code >> 6) & 63) | 0x80;
			output[0] = (code >> 12) | 0xe0;
			output[2] = (code & 63) | 0x80;
			output += 3;
			position += 3;
		} else if (code < 0x200000) {
			if (position + 3 >= capacity)
				return 10016;
			output[1] = ((code >> 12) & 63) | 0x80;
			output[2] = ((code >> 6) & 63) | 0x80;
			output[0] = (code >> 18) | 0xf0;
			output[3] = (code & 63) | 0x80;
			output += 4;
			position += 4;
		} else if (code < 0x4000000) {
			if (position + 4 >= capacity)
				return 10016;
			output[1] = ((code >> 18) & 63) | 0x80;
			output[0] = (code >> 24) | 0xf8;
			output[2] = ((code >> 12) & 63) | 0x80;
			output[3] = ((code >> 6) & 63) | 0x80;
			output[4] = (code & 63) | 0x80;
			output += 5;
			position += 5;
		} else {
			if (position + 5 >= capacity)
				return 10016;
			output[2] = ((code >> 18) & 63) | 0x80;
			output[0] = ((code >> 30) & 1) | 0xfc;
			output[1] = ((code >> 24) & 63) | 0x80;
			output[3] = ((code >> 12) & 63) | 0x80;
			output[4] = ((code >> 6) & 63) | 0x80;
			output[5] = (code & 63) | 0x80;
			output += 6;
			position += 6;
		}
		length = value->length;
	}
	if (position >= capacity)
		return 10016;
	data[position] = '\0';
	return 0;
}

int sslFormatCertificateAttribute(SslCertificateNameComponent *component, int mode,
				  SslCertificateNameLabelLookup lookup,
				  SceUInt32 capacity, char *data)
{
	SslCertificatePropertyValue oid, value;
	const char *label;
	SceUInt32 usable, length, tag;
	int status;

	sslFillMemory(data, 0, capacity);
	usable = capacity - 1;
	status = component->methods->GetComponentInfo(component, 1, &oid);
	if (status != 0)
		return status;
	status = component->methods->GetComponentInfo(component, 2, &value);
	if (status != 0)
		return status;
	status = lookup(oid.data, oid.length, &label);
	if (status == 10008) {
		if (mode == 1)
			status = sslFormatObjectIdentifier(oid.data, oid.length, usable, data);
		else
			status = sslFormatCertificateHex(&oid, usable, mode, data);
		if (status != 0)
			return status;
		length = sslStringLength(data);
	} else {
		if (status != 0)
			return status;
		length = sslStringLength(label);
		if (length > usable)
			return 10016;
		sslCopyMemory(data, label, length);
	}
	if (usable - length < 1)
		return 10016;
	tag = value.value;
	data[length++] = '=';
	switch (tag) {
	case 12:
	case 18:
	case 19:
	case 20:
	case 22:
	case 23:
	case 24:
	case 26:
	case 28:
	case 30:
		status = sslFormatCertificateString(&value, usable - length, mode,
					     data + length);
		break;
	default:
		status = sslFormatCertificateHex(&value, usable - length, mode,
					  data + length);
		break;
	}
	if (status == 0)
		sslStringLength(data);
	return status;
}

int sslFormatCertificateNameForward(SslCertificateName *name, int mode,
				    SceUInt32 capacity, char *data)
{
	SslCertificateNameComponentRequest request;
	SslCertificateNameComponent *component;
	SceUInt32 used = 0;
	int count, index, status;

	status = name->methods->GetInfo(name, 10, &count);
	if (status != 0)
		return status;
	for (index = 0; index < count; index++) {
		request.index = index;
		status = name->methods->GetInfo(name, 11, &request);
		component = NULL;
		if (status != 0)
			goto fail;
		component = request.component;
		if (component == NULL) {
			status = 10001;
			goto fail;
		}
		status = sslFormatCertificateAttribute(component, 2,
						sslGetCertificateNameLabel,
						capacity - used, data + used);
		if (status != 0)
			goto fail;
		used = sslStringLength(data);
		data[used++] = ';';
		sslFreeCertificateNameComponent(component);
	}
	if (capacity <= used)
		return 10016;
	if (used != 0)
		used--;
	data[used] = '\0';
	return status;
fail:
	if (component != NULL)
		sslFreeCertificateNameComponent(component);
	return status;
}

int sslFormatCertificateName(SslCertificateName *name, int mode,
			    SceUInt32 capacity, char *data)
{
	SslCertificateNameComponentRequest request;
	SslCertificateNameComponent *component;
	SceUInt32 used = 0;
	SceInt32 count, index, previous = -1, savedPrevious;
	int status;
	char separator;

	if (mode == 2)
		return sslFormatCertificateNameForward(name, mode, capacity, data);
	status = name->methods->GetInfo(name, 10, &count);
	if (status != 0)
		return status;
	index = (SceInt32)((SceUInt32)count - 1);
	while (index >= 0) {
		request.index = index;
		status = name->methods->GetInfo(name, 11, &request);
		component = NULL;
		if (status != 0)
			goto fail;
		component = request.component;
		if (component == NULL) {
			status = 10001;
			goto fail;
		}
		savedPrevious = previous;
		previous = index;
		if (index < (SceInt32)((SceUInt32)count - 1)) {
			separator = index == savedPrevious ? '+' : ',';
			if (capacity <= used) {
				status = 10016;
				goto fail;
			}
			data[used++] = separator;
		}
		status = sslFormatCertificateAttribute(component, 1, sslGetObjectShortName,
						capacity - used, data + used);
		if (status != 0)
			goto fail;
		used = sslStringLength(data);
		sslFreeCertificateNameComponent(component);
		index = (SceInt32)((SceUInt32)index - 1);
	}
	if (capacity <= used)
		return 10016;
	data[used] = '\0';
	return status;
fail:
	if (component != NULL)
		sslFreeCertificateNameComponent(component);
	return status;
}

int sslGetCertificateNameLabel(const void *oid, SceUInt32 length,
			       const char **result)
{
	int i;

	for (i = 0; i < 9; i++) {
		if (length == sslStringLength(sslCertificateNameLabels[i][0]) &&
		    sslCompareMemory(oid, sslCertificateNameLabels[i][0], length) == 0) {
			*result = sslCertificateNameLabels[i][1];
			return 0;
		}
	}
	return 10008;
}
