/*
 * A NEMO message's SOAP fault (0x81079594-): the Fault read with the
 * exceptions of its detail and of a FaultDetails header, and the header
 * left of a message read. Sony's object starts here: its strings are a pool
 * of their own, with a second copy of the SOAP namespace. The file's name is
 * ours.
 */
#include "mln_internal.h"

#define MLN_NEMO_NS_SOAP	"http://schemas.xmlsoap.org/soap/envelope/"
#define MLN_NEMO_NS_CORE	"http://nemo.intertrust.com/2005/10/core"

/*
 * A SOAP Fault read: its faultcode and faultstring, its faultactor if any,
 * and the exceptions of its detail and of a FaultDetails header.
 */
__attribute__((noinline))
unsigned int MlnImpl_81079594(MlnXmlNode *fault, MlnXmlNode *header, MlnNemoFaults **out)
{
	MlnXmlNodeList *list = NULL;
	MlnXmlNodeList *details = NULL;
	MlnXmlNode *detail;
	MlnXmlNode *faultDetails;
	MlnNemoFaults *faults;
	unsigned int ret;

	faults = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*faults));
	if (faults == NULL) {
		ret = 2;
		goto out;
	}
	MlnMemset(faults, 0, sizeof(*faults));
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, fault, "faultcode", NULL, &list);
	if (ret != 0)
		goto out;
	if (list->count != 1)
		goto format;
	ret = MlnXmlGetText(MLN_HEAP_GENERAL, list->nodes[0], &faults->code);
	if (ret != 0)
		goto out;
	MlnXmlListFree(MLN_HEAP_GENERAL, list);
	list = NULL;
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, fault, "faultstring", NULL, &list);
	if (ret != 0)
		goto out;
	if (list->count != 1)
		goto format;
	ret = MlnXmlGetText(MLN_HEAP_GENERAL, list->nodes[0], &faults->string);
	if (ret != 0)
		goto out;
	MlnXmlListFree(MLN_HEAP_GENERAL, list);
	list = NULL;
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, fault, "faultactor", NULL, &list);
	if (ret == 0) {
		if (list->count != 1)
			goto format;
		ret = MlnXmlGetText(MLN_HEAP_GENERAL, list->nodes[0], &faults->actor);
	}
	if (ret != 0 && ret != MLN_XML_ERROR_NOT_FOUND)
		goto out;
	MlnXmlListFree(MLN_HEAP_GENERAL, list);
	list = NULL;
	detail = NULL;
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, fault, "detail", MLN_NEMO_NS_SOAP, &list);
	if (ret == 0) {
		if (list->count != 1)
			goto format;
		detail = list->nodes[0];
	} else if (ret != MLN_XML_ERROR_NOT_FOUND) {
		goto out;
	}
	faultDetails = NULL;
	if (header != NULL) {
		ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, header, "FaultDetails", MLN_NEMO_NS_CORE,
				      &details);
		if (ret == 0)
			faultDetails = details->nodes[0];
		else if (ret != MLN_XML_ERROR_NOT_FOUND)
			goto out;
	}
	ret = MlnImpl_810792B4(detail, faultDetails, &faults->items, &faults->count);
	if (ret == 0) {
		MlnXmlListFree(MLN_HEAP_GENERAL, list);
		list = NULL;
		MlnXmlListFree(MLN_HEAP_GENERAL, details);
		*out = faults;
		faults = NULL;
		details = NULL;
	}
	goto out;
format:
	ret = MLN_NEMO_ERROR_FORMAT;
out:
	MlnImpl_810794DC(faults);
	MlnXmlListFree(MLN_HEAP_GENERAL, list);
	MlnXmlListFree(MLN_HEAP_GENERAL, details);
	return ret;
}

/* the fault a message read gave */
__attribute__((noinline))
unsigned int MlnImpl_81079814(const MlnNemoMessage *message, MlnNemoFaults **faults)
{
	if (message == NULL || faults == NULL)
		return 3;
	if (message->faults == NULL)
		return MLN_NEMO_ERROR_NO_FAULT;
	*faults = message->faults;
	return 0;
}

/* a header's nodes freed (a node that cannot be freed stops it) */
__attribute__((noinline))
unsigned int MlnImpl_81079854(MlnNemoHeaderNodes *nodes)
{
	unsigned int ret;
	int i;

	if (nodes == NULL)
		return 0;
	if (nodes->items != NULL)
		for (i = 0; i < nodes->count; i++) {
			ret = MlnXmlNodeFree(MLN_HEAP_GENERAL, nodes->items[i].node);
			if (ret != 0)
				return ret;
		}
	MlnFree(MLN_HEAP_GENERAL, nodes->items);
	MlnFree(MLN_HEAP_GENERAL, nodes);
	return 0;
}

/* a header emptied but for a request's MessageID or originatesFrom kept */
__attribute__((noinline))
unsigned int MlnImpl_810798CC(MlnNemoHeader *header)
{
	int i;

	for (i = 0; i < 8; i++) {
		MlnBufferDestroy(MLN_HEAP_GENERAL, header->parts[i]);
		header->parts[i] = NULL;
	}
	if (header->present & MLN_NEMO_PART_REQUEST_ID) {
		header->count = 1;
		header->present = MLN_NEMO_PART_REQUEST_ID;
	} else if (header->present & MLN_NEMO_PART_EXPECTED_ORIGIN) {
		header->count = 1;
		header->present = MLN_NEMO_PART_EXPECTED_ORIGIN;
	} else {
		header->count = 0;
		header->present = 0;
	}
	return 0;
}

/* a header emptied, a request's MessageID and originatesFrom too */
static inline __attribute__((always_inline)) void MlnNemoHeaderClear(MlnNemoHeader *header)
{
	MlnImpl_810798CC(header);
	header->present = 0;
	header->count = 0;
	MlnBufferDestroy(MLN_HEAP_GENERAL, MLN_NEMO_HEADER_PART(header, MLN_NEMO_PART_REQUEST_ID));
	MLN_NEMO_HEADER_PART(header, MLN_NEMO_PART_REQUEST_ID) = NULL;
	MlnBufferDestroy(MLN_HEAP_GENERAL,
			 MLN_NEMO_HEADER_PART(header, MLN_NEMO_PART_EXPECTED_ORIGIN));
	MLN_NEMO_HEADER_PART(header, MLN_NEMO_PART_EXPECTED_ORIGIN) = NULL;
}

__attribute__((noinline))
unsigned int MlnImpl_810799B0(MlnNemoHeader *header)
{
	MlnNemoHeaderClear(header);
	return 0;
}

/* a header with no parts */
unsigned int MlnImpl_810799F0(MlnNemoHeader *header)
{
	header->count = 0;
	header->present = 0;
	header->parts[0] = NULL;
	header->parts[1] = NULL;
	header->parts[2] = NULL;
	header->parts[3] = NULL;
	header->parts[4] = NULL;
	header->parts[5] = NULL;
	header->parts[6] = NULL;
	header->parts[7] = NULL;
	header->parts[8] = NULL;
	header->parts[9] = NULL;
	return 0;
}

/* a service's certificate freed with what was read of it */
__attribute__((noinline))
unsigned int MlnImpl_81079A2C(MlnNemoPeerKey *key)
{
	if (key != NULL) {
		MlnBufferDestroy(MLN_HEAP_GENERAL, key->certificate);
		MlnBufferDestroy(MLN_HEAP_GENERAL, key->keyId);
		MlnBufferDestroy(MLN_HEAP_GENERAL, key->modulus);
		MlnBufferDestroy(MLN_HEAP_GENERAL, key->exponent);
		MlnFree(MLN_HEAP_GENERAL, key);
	}
	return 0;
}

/*
 * A certificate copied into a peer's key, with its subject key identifier in
 * base64 and its RSA public key. An X.509 error is the message's
 * MLN_NEMO_ERROR_CRYPTO; a base64 or key error 5.
 */
static inline __attribute__((always_inline)) unsigned int MlnNemoPeerKeyRead(
	const MlnBuffer *certificate, MlnNemoPeerKey *key, MlnBuffer **keyId, MlnX509 **x509)
{
	unsigned int ret;

	key->certificate = MlnBufferCreate(MLN_HEAP_GENERAL, certificate->size, certificate->data);
	if (key->certificate == NULL)
		return 2;
	ret = MlnImpl_81038EF4(certificate, x509);
	if (ret == 0)
		ret = MlnImpl_810390C0(*x509, keyId);
	if (ret != 0) {
		if (ret >> 16 != 0)
			ret = MLN_NEMO_ERROR_CRYPTO;
		return ret;
	}
	ret = MlnImpl_81026814(MLN_HEAP_GENERAL, *keyId, 0, 0, &key->keyId);
	if (ret == 0) {
		MlnBufferDestroy(MLN_HEAP_GENERAL, *keyId);
		*keyId = NULL;
		ret = MlnImpl_81039228(*x509, &key->modulus, &key->exponent);
	}
	if (ret != 0) {
		if (ret >> 16 != 0)
			ret = 5;
		return ret;
	}
	MlnImpl_81038E90(*x509);
	*x509 = NULL;
	return 0;
}

/* a peer's key freed, inline where Sony does not call MlnImpl_81079A2C */
static inline __attribute__((always_inline)) void MlnNemoPeerKeyFree(MlnNemoPeerKey *key)
{
	if (key == NULL)
		return;
	MlnBufferDestroy(MLN_HEAP_GENERAL, key->certificate);
	MlnBufferDestroy(MLN_HEAP_GENERAL, key->keyId);
	MlnBufferDestroy(MLN_HEAP_GENERAL, key->modulus);
	MlnBufferDestroy(MLN_HEAP_GENERAL, key->exponent);
	MlnFree(MLN_HEAP_GENERAL, key);
}

/* the message's peer keys, read from the service's two certificates */
unsigned int MlnImpl_81079A7C(MlnNemoMessage *message, MlnBuffer *first, MlnBuffer *second)
{
	MlnNemoPeerKey *keys[2];
	MlnBuffer *keyId = NULL;
	MlnX509 *x509 = NULL;
	unsigned int ret;

	keys[0] = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*keys[0]));
	if (keys[0] == NULL) {
		ret = 2;
		goto out;
	}
	MlnMemset(keys[0], 0, sizeof(*keys[0]));
	ret = MlnNemoPeerKeyRead(first, keys[0], &keyId, &x509);
	if (ret != 0)
		goto fail;
	keys[1] = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*keys[1]));
	if (keys[1] == NULL) {
		ret = 2;
		goto fail;
	}
	MlnMemset(keys[1], 0, sizeof(*keys[1]));
	ret = MlnNemoPeerKeyRead(second, keys[1], &keyId, &x509);
	if (ret != 0) {
		MlnBufferDestroy(MLN_HEAP_GENERAL, keyId);
		MlnNemoPeerKeyFree(keys[0]);
		MlnNemoPeerKeyFree(keys[1]);
		goto close;
	}
	if (message->keys[0] != NULL)
		MlnImpl_81079A2C(message->keys[0]);
	message->keys[0] = keys[0];
	if (message->keys[1] != NULL)
		MlnImpl_81079A2C(message->keys[1]);
	message->keys[1] = keys[1];
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, keyId);
	goto close;
fail:
	MlnBufferDestroy(MLN_HEAP_GENERAL, keyId);
	MlnNemoPeerKeyFree(keys[0]);
close:
	MlnImpl_81038E90(x509);
	return ret;
}

/* the time now, as xsd:dateTime in UTC ("YYYY-MM-DDTHH:MM:SSZ") */
__attribute__((noinline))
unsigned int MlnImpl_81079D78(char **out)
{
	MlnDateTime date;
	unsigned int now;
	int mode;
	char *time;
	unsigned int ret;
	unsigned int r;

	time = MlnAllocate(MLN_HEAP_GENERAL, 21);
	if (time == NULL) {
		ret = 2;
		goto out;
	}
	r = MlnImpl_81052D84((int *)&now, &mode);
	if (r == 0x220B0001) {
		ret = MlnImpl_81052E04(&now);
		if (ret != 0)
			goto out;
	} else if (r != 0) {
		ret = 5;
		goto out;
	}
	if (MlnTimeFromSeconds(&now, 0, 0, &date) != 0) {
		ret = 5;
		goto out;
	}
	ret = MlnImpl_810529E8(date.year, 1, 4, &time[0]);
	if (ret != 0)
		goto out;
	time[4] = '-';
	ret = MlnImpl_810529E8(date.month, 1, 2, &time[5]);
	if (ret != 0)
		goto out;
	time[7] = '-';
	ret = MlnImpl_810529E8(date.day, 1, 2, &time[8]);
	if (ret != 0)
		goto out;
	time[10] = 'T';
	ret = MlnImpl_810529E8(date.hour, 1, 2, &time[11]);
	if (ret != 0)
		goto out;
	time[13] = ':';
	ret = MlnImpl_810529E8(date.minute, 1, 2, &time[14]);
	if (ret != 0)
		goto out;
	time[16] = ':';
	ret = MlnImpl_810529E8(date.second, 1, 2, &time[17]);
	if (ret != 0)
		goto out;
	time[20] = '\0';
	time[19] = 'Z';
	*out = time;
	time = NULL;
out:
	MlnFree(MLN_HEAP_GENERAL, time);
	return ret;
}

/* a request's MessageID: the sender's node id, ':' and 16 random hex digits */
unsigned int MlnImpl_81079EE8(const MlnBuffer *credential, MlnBuffer **node)
{
	unsigned char random[8];
	char text[18];
	MlnBuffer suffix;
	MlnBuffer *id;
	unsigned int ret;
	int i;

	MlnMemset(random, 0, sizeof(random));
	MlnMemset(text, 0, sizeof(text));
	ret = MlnImpl_810790EC(sizeof(random), random);
	if (ret != 0)
		return ret;
	text[0] = ':';
	for (i = 0; i < 8; i++) {
		ret = MlnImpl_810529E8(random[i], 2, 2, &text[1 + 2 * i]);
		if (ret != 0)
			return ret;
	}
	text[17] = '\0';
	suffix.size = 17;
	suffix.data = (unsigned char *)text;
	id = MlnBufferConcat(MLN_HEAP_GENERAL, credential, &suffix);
	if (id == NULL)
		return 2;
	*node = id;
	return 0;
}

/* a part of the header set from a copy of a value */
unsigned int MlnImpl_8107A04C(MlnNemoHeader *header, unsigned int part, const MlnBuffer *value)
{
	MlnBuffer *copy;
	unsigned int present;

	if (header == NULL || value == NULL)
		return 3;
	copy = MlnBufferCreate(MLN_HEAP_GENERAL, value->size, value->data);
	if (copy == NULL)
		return 2;
	switch (part) {
	case MLN_NEMO_PART_ACTION:
		MlnBufferDestroy(MLN_HEAP_GENERAL, MLN_NEMO_HEADER_PART(header, MLN_NEMO_PART_ACTION));
		MLN_NEMO_HEADER_PART(header, MLN_NEMO_PART_ACTION) = copy;
		break;
	case MLN_NEMO_PART_ORIGINATES_FROM:
		MlnBufferDestroy(MLN_HEAP_GENERAL,
				 MLN_NEMO_HEADER_PART(header, MLN_NEMO_PART_ORIGINATES_FROM));
		MLN_NEMO_HEADER_PART(header, MLN_NEMO_PART_ORIGINATES_FROM) = copy;
		break;
	case MLN_NEMO_PART_RELATES_TO:
		MlnBufferDestroy(MLN_HEAP_GENERAL,
				 MLN_NEMO_HEADER_PART(header, MLN_NEMO_PART_RELATES_TO));
		MLN_NEMO_HEADER_PART(header, MLN_NEMO_PART_RELATES_TO) = copy;
		break;
	case MLN_NEMO_PART_MESSAGE_ID:
		MlnBufferDestroy(MLN_HEAP_GENERAL,
				 MLN_NEMO_HEADER_PART(header, MLN_NEMO_PART_MESSAGE_ID));
		MLN_NEMO_HEADER_PART(header, MLN_NEMO_PART_MESSAGE_ID) = copy;
		break;
	case MLN_NEMO_PART_REQUEST_ID:
		MlnBufferDestroy(MLN_HEAP_GENERAL,
				 MLN_NEMO_HEADER_PART(header, MLN_NEMO_PART_REQUEST_ID));
		MLN_NEMO_HEADER_PART(header, MLN_NEMO_PART_REQUEST_ID) = copy;
		break;
	case MLN_NEMO_PART_EXPECTED_ORIGIN:
		MlnBufferDestroy(MLN_HEAP_GENERAL,
				 MLN_NEMO_HEADER_PART(header, MLN_NEMO_PART_EXPECTED_ORIGIN));
		MLN_NEMO_HEADER_PART(header, MLN_NEMO_PART_EXPECTED_ORIGIN) = copy;
		break;
	default:
		/* Sony keeps the copy */
		return 3;
	}
	present = header->present;
	header->present = present | part;
	if ((present & part) == 0)
		header->count++;
	return 0;
}

/* the Created time a message read carried, handed over */
unsigned int MlnImpl_8107A15C(MlnNemoMessage *message, int *timestamp)
{
	int created;

	if (message == NULL) {
		*timestamp = 0;
		return 3;
	}
	created = message->timestamp;
	message->timestamp = 0;
	*timestamp = created;
	if (created == 0)
		return MLN_NEMO_ERROR_NO_TIMESTAMP;
	return 0;
}

/* a message emptied of everything it holds */
unsigned int MlnImpl_8107A19C(MlnNemoMessage *message)
{
	int i;

	if (message == NULL)
		return 0;
	MlnNemoHeaderClear(&message->header);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, message->body);
	message->body = NULL;
	MlnImpl_81042A88(message->credentials);
	message->credentials = NULL;
	if (message->nodes != NULL)
		for (i = 0; i < message->nodeCount; i++) {
			MlnXmlNodeFree(MLN_HEAP_GENERAL, message->nodes[i]);
			message->nodes[i] = NULL;
		}
	MlnFree(MLN_HEAP_GENERAL, message->nodes);
	message->nodes = NULL;
	MlnXmlListFree(MLN_HEAP_GENERAL, message->bundles);
	message->bundles = NULL;
	MlnBufferDestroy(MLN_HEAP_GENERAL, message->peer);
	message->peer = NULL;
	MlnBufferDestroy(MLN_HEAP_GENERAL, message->nonce);
	message->nonce = NULL;
	MlnBufferDestroy(MLN_HEAP_GENERAL, message->peerNonce);
	message->peerNonce = NULL;
	MlnNemoPeerKeyFree(message->keys[0]);
	message->keys[0] = NULL;
	MlnNemoPeerKeyFree(message->keys[1]);
	message->keys[1] = NULL;
	MlnImpl_810794DC(message->faults);
	message->faults = NULL;
	return 0;
}

/*
 * An error of the XML, cipher or key code as a NEMO message's: malformed
 * input is the message's format, a failed operation its failure.
 */
__attribute__((noinline))
unsigned int MlnImpl_8107A340(unsigned int ret)
{
	switch (ret & 0xFFFF0000) {
	case 0x10010000:
	case 0x22010000:
	case 0x22090000:
	case 0x220A0000:
		return MLN_NEMO_ERROR_FORMAT;
	case 0x22020000:
	case 0x22030000:
	case 0x22060000:
		return MLN_NEMO_ERROR_FAILED;
	}
	return ret;
}
