/* WLAN utility methods; the original does not record a source filename. */
#include "wlanbt_internal.h"
#include "sce/atomic.h"
#include <string.h>

/* WPA vendor-element OUI and type. */
static const uint8_t gWpaElementSelector[4] = { 0x00, 0x50, 0xf2, 0x01 };

static const uint8_t gRsnOui[3] = { 0x00, 0x0f, 0xac };
static const uint8_t gWpaOui[3] = { 0x00, 0x50, 0xf2 };

typedef struct WlanSecuritySuite {
	uint8_t oui[3];
	uint8_t type;
} WlanSecuritySuite;

typedef struct WlanSecurityParameter {
	uint16_t version;
	WlanSecuritySuite groupCipher;
	uint16_t pairwiseCount;
	WlanSecuritySuite pairwise[];
} WlanSecurityParameter;

typedef struct WlanKeyManagementList {
	uint16_t count;
	WlanSecuritySuite suites[];
} WlanKeyManagementList;

_Static_assert(sizeof(WlanSecuritySuite) == 4, "WLAN security suite selector");
_Static_assert(sizeof(WlanSecurityParameter) == 8, "WLAN security parameter prefix");
_Static_assert(sizeof(WlanKeyManagementList) == 2, "WLAN key management prefix");

int wlanParseSecurityInformationElement(WlanSecurityInfo *security,
	const WlanInformationElement *element)
{
	const WlanSecurityParameter *parameter = (const void *)element->data;
	uintptr_t end = (uintptr_t)element->data + element->length;
	const uint8_t *oui = gRsnOui;
	const WlanSecuritySuite *suite;
	const WlanKeyManagementList *keyManagement;
	unsigned int count;
	unsigned int i;
	uint8_t mask;

	if (element->id == 221) {
		parameter = (const void *)(element->data + sizeof(gWpaElementSelector));
		if (end < (uintptr_t)parameter ||
		    memcmp(element->data, gWpaElementSelector, sizeof(gWpaElementSelector)))
			return 0x80418007;
		oui = gWpaOui;
	}
	if (end < (uintptr_t)&parameter->groupCipher ||
	    end < (uintptr_t)&parameter->pairwiseCount)
		return 0x80418007;
	if (memcmp(parameter->groupCipher.oui, oui, sizeof(gRsnOui))) {
		security->groupCipher |= 0x80;
	} else {
		switch (parameter->groupCipher.type) {
		case 1:
			security->groupCipher |= 1;
			break;
		case 2:
			security->groupCipher |= 4;
			break;
		case 4:
			security->groupCipher |= 8;
			break;
		case 5:
			security->groupCipher |= 2;
			break;
		default:
			security->groupCipher |= 0x80;
			break;
		}
	}
	if (end < (uintptr_t)parameter->pairwise)
		return 0x80418007;
	count = parameter->pairwiseCount;
	suite = parameter->pairwise;
	if (count) {
		mask = security->pairwiseCipher;
		for (i = 0; i < count; i++, suite++) {
			if (end < (uintptr_t)(suite + 1))
				return 0x80418007;
			if (memcmp(suite->oui, oui, sizeof(gRsnOui)))
				mask |= 0x80;
			else if (suite->type == 2)
				mask |= 4;
			else if (suite->type == 4)
				mask |= 8;
			else
				mask |= 0x80;
			security->pairwiseCipher = mask;
		}
	}
	keyManagement = (const void *)suite;
	if (end < (uintptr_t)keyManagement->suites)
		return 0x80418007;
	count = keyManagement->count;
	suite = keyManagement->suites;
	if (count) {
		mask = security->keyManagement;
		for (i = 0; i < count; i++, suite++) {
			if (end < (uintptr_t)(suite + 1))
				return 0x80418007;
			if (memcmp(suite->oui, oui, sizeof(gRsnOui)))
				mask |= 0x80;
			else if (suite->type == 1)
				mask |= 1;
			else if (suite->type == 2)
				mask |= 2;
			else
				mask |= 0x80;
			security->keyManagement = mask;
		}
	}
	security->enabled = 1;
	return 0;
}

SceWlanVisitor *WlanVisitorInitialize(SceWlanVisitor *visitor)
{
	visitor->count = 0;
	visitor->result = 0;
	visitor->accepting = 1;
	return visitor;
}

int WlanVisitorEnter(SceWlanVisitor *visitor)
{
	uint32_t count;

	if (!visitor->accepting)
		return visitor->result;
	AtomicFullBarrier();
	do {
		count = AtomicLoadReserved32(&visitor->count);
	} while (AtomicStoreReserved32(&visitor->count, count + 1));
	return 0;
}

SceWlanVisitor *WlanVisitorLeave(SceWlanVisitor *visitor)
{
	uint32_t count;

	AtomicFullBarrier();
	do {
		count = AtomicLoadReserved32(&visitor->count);
	} while (AtomicStoreReserved32(&visitor->count, count - 1));
	return visitor;
}

int WlanVisitorIsIdle(const SceWlanVisitor *visitor)
{
	return visitor->count == 0;
}

SceWlanVisitor *WlanVisitorClose(SceWlanVisitor *visitor, int result)
{
	visitor->result = result;
	visitor->accepting = 0;
	return visitor;
}

int wlanCopyInformationElement(const void *buffer, unsigned int length, int id,
			       void *output, unsigned int *outputSize,
			       unsigned int *offset)
{
	const uint8_t *start = (const uint8_t *)buffer + *offset;
	unsigned int remaining = length - *offset;
	const WlanInformationElement *element;
	unsigned int consumed = 0;
	unsigned int payloadSize;
	int result;

	do {
		element = (const void *)(start + consumed);
		consumed += sizeof(*element);
		if (remaining < consumed)
			goto notFound;
		payloadSize = element->length;
		consumed += payloadSize;
		if (remaining < consumed)
			goto notFound;
	} while (element->id != id);
	if (*outputSize < payloadSize) {
		result = 0x80418005;
		goto failed;
	}
	*outputSize = payloadSize;
	if (payloadSize)
		memcpy(output, element->data, payloadSize);
	*offset += consumed;
	return 0;
notFound:
	result = 0x80418007;
failed:
	*outputSize = 0;
	return result;
}

int wlanCopyWpaInformationElement(const void *buffer, unsigned int length,
				  void *output, unsigned int *outputSize)
{
	unsigned int capacity = *outputSize;
	unsigned int offset = 0;
	unsigned int remaining;
	unsigned int consumed;
	unsigned int payloadSize;
	const uint8_t *start;
	const WlanInformationElement *element;

	for (;;) {
		start = (const uint8_t *)buffer + offset;
		remaining = length - offset;
		consumed = 0;
		do {
			element = (const void *)(start + consumed);
			consumed += sizeof(*element);
			if (remaining < consumed)
				return 0x80418007;
			payloadSize = element->length;
			consumed += payloadSize;
			if (remaining < consumed)
				return 0x80418007;
		} while (element->id != 221);
		if (payloadSize > capacity)
			return 0x80418005;
		offset += consumed;
		if (!payloadSize)
			continue;
		memcpy(output, element->data, payloadSize);
		if (payloadSize > 3 &&
		    memcmp(output, gWpaElementSelector, sizeof(gWpaElementSelector)) == 0) {
			*outputSize = payloadSize;
			return 0;
		}
		capacity = *outputSize;
	}
}

uint32_t *wlanEnableInformationElement(uint32_t *mask, unsigned int id)
{
	mask[id >> 5] |= 1u << (id & 31);
	return mask;
}

