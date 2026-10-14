/*
 * Host-sleep packet filter (a postfix expression machine) and the ARP and
 * ping offload helpers. See robin/mef.h. The filename is inferred.
 */
#include <stddef.h>
#include "robin/conn.h"
#include "robin/mef.h"
#include "robin/wlan.h"
#include "robin/rom.h"
#include "robin/runtime.h"

/* Bytes the stack may hold, the nesting depth allowed, and the depth reached so far. */
uint16_t wlanMefStackLimit __attribute__((section(".itcm.bss")));
uint16_t wlanMefDepthLimit __attribute__((section(".itcm.bss")));
uint8_t wlanMefDepth __attribute__((section(".itcm.bss")));
/* Space in front of a received frame that holds the offload headers. */
uint32_t wlanOffloadHeadroom __attribute__((section(".itcm.bss")));

int wlanMefStackPush(struct WlanMefStack *stack, unsigned int count, const void *source)
{
	unsigned int length = stack->length;

	if (length + count >= wlanMefStackLimit)
		return 0;
	__aeabi_memcpy(&stack->data[length], source, count);
	stack->length = length + count;
	return 1;
}

int wlanMefStackPop(struct WlanMefStack *stack, unsigned int count, void *destination)
{
	unsigned int length = stack->length;

	if (length < count)
		return 0;
	__aeabi_memcpy(destination, &stack->data[length - count], count);
	stack->length = length - count;
	return 1;
}

/* Pushes the payload, then the type, so that the type pops first. */
int wlanMefTokenPush(struct WlanMefStack *stack, const struct WlanMefToken *token)
{
	if (token->type == WLAN_MEF_NUMBER) {
		if (!wlanMefStackPush(stack, 4, &token->number))
			return 0;
		return wlanMefStackPush(stack, 1, token);
	}
	if (token->type != WLAN_MEF_SEQUENCE)
		return wlanMefStackPush(stack, 1, token);
	return wlanMefStackPush(stack, token->sequence.length, token->sequence.bytes) != 0 &&
	       wlanMefStackPush(stack, 1, &token->sequence.length) != 0 &&
	       wlanMefStackPush(stack, 1, token) != 0;
}

int wlanMefTokenPop(struct WlanMefStack *stack, struct WlanMefToken *token)
{
	uint8_t length;

	if (!wlanMefStackPop(stack, 1, token))
		return 0;
	if (token->type == WLAN_MEF_NUMBER)
		return wlanMefStackPop(stack, 4, &token->number);
	if (token->type == WLAN_MEF_SEQUENCE) {
		if (!wlanMefStackPop(stack, 1, &length))
			return 0;
		token->sequence.length = length;
		return wlanMefStackPop(stack, length, token->sequence.bytes);
	}
	return 1;
}

int wlanMefIsOperator(const struct WlanMefToken *token)
{
	return token->type > 0x40;
}

/* True when the sequence repeats count times at the offset. */
int wlanMefCompareSequence(const struct WlanMefPacket *packet, struct WlanMefToken *offset,
			   const struct WlanMefToken *sequence, const struct WlanMefToken *repeat)
{
	unsigned int count = repeat->number;
	unsigned int length = sequence->sequence.length;
	unsigned int start = offset->number;
	unsigned int i;
	int result;

	if (count * length + start <= packet->length) {
		for (i = 0; i < count; ++i) {
			if (memcmp(sequence->sequence.bytes, packet->data + i * length + start, length) != 0) {
				offset->number = 0;
				return 0;
			}
		}
		result = 1;
	} else {
		result = 0;
	}
	offset->number = result;
	return result;
}

/* Compares the value of the given size (1, 2 or 4) at the offset with the expected number. */
int wlanMefCompareNumber(const struct WlanMefPacket *packet, struct WlanMefToken *size,
			 const struct WlanMefToken *offset, const struct WlanMefToken *expected)
{
	uint32_t wanted = expected->number;
	uint32_t where = offset->number;
	uint32_t bytes = size->number;
	uint32_t value;
	uint32_t word;

	if (where + bytes <= packet->length) {
		if (bytes == 1) {
			value = packet->data[where];
		} else if (bytes == 2) {
			word = *(const uint16_t *)(packet->data + where);
			value = (word >> 8) | ((word << 24) >> 16);
		} else {
			word = *(const uint32_t *)(packet->data + where);
			value = ((word << 16) >> 24) | ((word << 24) >> 16);
		}
		if (wanted == value) {
			size->number = 1;
			return 1;
		}
	}
	size->number = 0;
	return 0;
}

/* True when the expected bytes equal the packet bytes masked with the mask. */
int wlanMefCompareMasked(const struct WlanMefPacket *packet, struct WlanMefToken *mask,
			 const struct WlanMefToken *offset, const struct WlanMefToken *expected)
{
	unsigned int where = offset->number;
	unsigned int length = expected->sequence.length;
	unsigned int i;
	int result;

	mask->type = WLAN_MEF_NUMBER;
	if (where + length <= packet->length) {
		for (i = 0; i < length; ++i) {
			if (expected->sequence.bytes[i] != (uint8_t)(packet->data[where + i] & mask->sequence.bytes[i])) {
				mask->number = 0;
				return 0;
			}
		}
		result = 1;
	} else {
		result = 0;
	}
	mask->number = result;
	return result;
}

void wlanMefAnd(struct WlanMefToken *first, const struct WlanMefToken *second)
{
	if (first->type != WLAN_MEF_NUMBER || second->type != WLAN_MEF_NUMBER ||
	    first->number != 1 || second->number != 1) {
		first->type = WLAN_MEF_NUMBER;
		first->number = 0;
	}
}

void wlanMefOr(struct WlanMefToken *first, const struct WlanMefToken *second)
{
	if (first->type == WLAN_MEF_NUMBER && second->type == WLAN_MEF_NUMBER &&
	    (first->number == 1 || second->number == 1)) {
		first->number = 1;
	} else {
		first->type = WLAN_MEF_NUMBER;
		first->number = 0;
	}
}

/*
 * Pops the top of the stack; operands that are operators themselves are
 * evaluated first (within the depth limit). Applies the operator and pushes
 * the result. An empty stack passes the packet.
 */
int wlanMefEvaluate(struct WlanMefStack *stack, const struct WlanMefPacket *packet)
{
	struct WlanMefToken first;
	struct WlanMefToken second;
	struct WlanMefToken third;
	unsigned int operator;

	if (!wlanMefTokenPop(stack, &first))
		return 1;
	if (!wlanMefIsOperator(&first))
		return wlanMefTokenPush(stack, &first) != 0;
	operator = first.type;
	if (!wlanMefTokenPop(stack, &first))
		return 0;
	if (wlanMefIsOperator(&first)) {
		wlanMefTokenPush(stack, &first);
		if (!wlanMefEvaluate(stack, packet))
			return 0;
		if (++wlanMefDepth > wlanMefDepthLimit)
			return 0;
		wlanMefTokenPop(stack, &first);
	}
	if (!wlanMefTokenPop(stack, &second))
		return 0;
	if (wlanMefIsOperator(&second)) {
		wlanMefTokenPush(stack, &second);
		if (!wlanMefEvaluate(stack, packet))
			return 0;
		wlanMefTokenPop(stack, &second);
	}
	switch (operator) {
	case WLAN_MEF_COMPARE_SEQUENCE:
	case WLAN_MEF_COMPARE_NUMBER:
	case WLAN_MEF_COMPARE_MASKED:
		if (!wlanMefTokenPop(stack, &third))
			return 0;
		if (wlanMefIsOperator(&third)) {
			wlanMefTokenPush(stack, &third);
			if (!wlanMefEvaluate(stack, packet))
				return 0;
			wlanMefTokenPop(stack, &third);
		}
		if (operator == WLAN_MEF_COMPARE_SEQUENCE)
			wlanMefCompareSequence(packet, &first, &second, &third);
		else if (operator == WLAN_MEF_COMPARE_NUMBER)
			wlanMefCompareNumber(packet, &first, &second, &third);
		else
			wlanMefCompareMasked(packet, &first, &second, &third);
		break;
	case WLAN_MEF_AND:
		wlanMefAnd(&first, &second);
		break;
	case WLAN_MEF_OR:
		wlanMefOr(&first, &second);
		break;
	default:
		return 1;
	}
	return wlanMefTokenPush(stack, &first) != 0;
}

int wlanMefResetDepth(void)
{
	wlanMefDepth = 1;
	return 1;
}

int wlanMefInit(void)
{
	wlanMefStackLimit = 100;
	wlanMefDepthLimit = 7;
	return 7;
}

/* Internet checksum of length bytes. */
unsigned int wlanInternetChecksum(const uint16_t *data, unsigned int length)
{
	const uint16_t *word = data;
	uint32_t sum = 0;

	while (length > 1) {
		sum += *word++;
		if (sum & 0x80000000)
			sum = (uint16_t)sum + (sum >> 16);
		length = (uint16_t)(length - 2);
	}
	if (length != 0)
		sum += *(const uint8_t *)data;
	while (sum >> 16)
		sum = (uint16_t)sum + (sum >> 16);
	return (uint16_t)~sum;
}

/* Destination and source address of an 802.11 data frame, by the ToDS and FromDS bits. */
void wlanFrameGetEthernetAddresses(void *destination, void *source, const uint8_t *frame)
{
	const uint8_t *address;

	if (frame == NULL || destination == NULL || source == NULL)
		return;
	address = (frame[3] & 1) ? frame + 18 : frame + 6;
	__aeabi_memcpy(destination, address, 6);
	if (frame[3] & 2)
		address = (frame[3] & 1) ? frame + 26 : frame + 18;
	else
		address = frame + 12;
	__aeabi_memcpy(source, address, 6);
}

/* 1 for the broadcast address, 8 for a group address, 2 for a unicast address. */
unsigned int wlanAddressClass(const uint16_t *address)
{
	if (address[0] == 0xffff && address[1] == 0xffff && address[2] == 0xffff)
		return 1;
	if (*(const uint8_t *)address & 1)
		return 8;
	return 2;
}

/*
 * Builds the Ethernet header (addresses and a big-endian length) in front of
 * the payload of a received frame, when the destination is of an accepted
 * class, and describes the result in packet. Returns 1 if it did.
 */
int wlanBuildEthernetHeader(OsaBuffer *buffer, unsigned int classMask, struct WlanMefPacket *packet)
{
	const uint8_t *frame = (const uint8_t *)buffer + buffer->dataOffset;
	uint8_t *header = (uint8_t *)frame + wlanOffloadHeadroom + 32 - 14;
	uint16_t destination[6];
	uint16_t source[6];
	unsigned int length;

	wlanFrameGetEthernetAddresses(destination, source, frame);
	if ((wlanAddressClass(destination) & classMask) == 0)
		return 0;
	__aeabi_memcpy(header, destination, 6);
	__aeabi_memcpy(header + 6, source, 6);
	length = (uint16_t)(buffer->length - 0x60);
	header[12] = length >> 8;
	header[13] = length;
	packet->length = length + 14;
	packet->data = header;
	return 1;
}

/* Copies a length-prefixed filter stack. */
void wlanMefLoad(void *destination, const uint8_t *source)
{
	__aeabi_memcpy(destination, source, ((source[1] << 8) | source[0]) + 2);
}

/* Address classes (see wlanAddressClass) the filter expression applies to. */
uint32_t wlanMefClassMask __attribute__((section(".itcm.bss")));

int wlanMefClassEnabled(uint32_t mask)
{
	return (__aeabi_uread4(&wlanMefClassMask) & mask) != 0;
}

/* Turns the ARP request in the buffer into the reply for the local address; NULL if it is not ARP. */
struct WlanSnapArp *wlanBuildArpReply(OsaBuffer *buffer, struct WlanSnapArp *reply,
				      const uint8_t *localAddress)
{
	const struct WlanSnapArp *request;

	if (reply == NULL || buffer == NULL)
		return NULL;
	request = (const struct WlanSnapArp *)((const uint8_t *)buffer + buffer->dataOffset +
					       wlanOffloadHeadroom + 32);
	if (request->snap[6] != 0x08 || request->snap[7] != 0x06)
		return NULL;
	__aeabi_memcpy(reply, request, 16);
	reply->operation[0] = 0;
	reply->operation[1] = 2;
	__aeabi_memcpy(reply->senderHardware, localAddress, 6);
	__aeabi_memcpy(reply->senderProtocol, request->targetProtocol, 4);
	__aeabi_memcpy(reply->targetHardware, request->senderHardware, 6);
	__aeabi_memcpy(reply->targetProtocol, request->senderProtocol, 4);
	return (struct WlanSnapArp *)((uint8_t *)reply + 8);
}

/* Builds the ICMP echo reply to an unfragmented echo request that fits maxLength. */
void wlanBuildEchoReply(OsaBuffer *buffer, uint8_t *reply, uint16_t *replyLength, unsigned int maxLength)
{
	const uint8_t *request;
	uint8_t *ip;
	unsigned int ipLength;
	unsigned int total;

	if (reply == NULL || buffer == NULL)
		return;
	request = (const uint8_t *)buffer + buffer->dataOffset + wlanOffloadHeadroom + 32;
	ipLength = (uint16_t)((request[10] << 8) | request[11]);
	total = (uint16_t)(ipLength + 8);
	if ((request[14] & 0x20) != 0 || total > maxLength)
		return;
	__aeabi_memcpy(reply, request, total);
	ip = reply + 8;
	__aeabi_uwrite4(__aeabi_uread4(request + 8 + 16), ip + 12);
	__aeabi_uwrite4(__aeabi_uread4(request + 8 + 12), ip + 16);
	ip[10] = 0;
	ip[11] = 0;
	*(uint16_t *)(ip + 10) = wlanInternetChecksum((const uint16_t *)ip, 20);
	ip += 20;
	ip[0] = 0;
	ip[2] = 0;
	ip[3] = 0;
	*(uint16_t *)(ip + 2) = wlanInternetChecksum((const uint16_t *)ip, (uint16_t)(ipLength - 20));
	*replyLength = total;
}

/* Parameter slots: a value (or values) followed by a mask of the valid indexes. */
int wlanParam8Set(uint8_t *slot, unsigned int index, const uint8_t *value)
{
	if (index != 0)
		return 0;
	slot[1] |= 1;
	slot[0] = *value;
	return 1;
}

int wlanParam8Get(const uint8_t *slot, unsigned int index, uint8_t *value)
{
	if (index != 0 || !(slot[1] & 1))
		return 0;
	*value = slot[0];
	return 1;
}

int wlanParam16Set(uint8_t *slot, unsigned int index, const uint16_t *value)
{
	if (index >= 2)
		return 0;
	slot[4] |= 1 << index;
	*(uint16_t *)(slot + 2 * index) = *value;
	return 1;
}

int wlanParam16Get(const uint8_t *slot, unsigned int index, uint16_t *value)
{
	if (index >= 2 || !(slot[4] & (1 << index)))
		return 0;
	*value = (slot[2 * index + 1] << 8) | slot[2 * index];
	return 1;
}

int wlanParam32Set(uint8_t *slot, unsigned int index, const uint32_t *value)
{
	if (index >= 2)
		return 0;
	slot[8] |= 1 << index;
	__aeabi_uwrite4(*value, slot + 4 * index);
	return 1;
}

int wlanParam32Get(const uint8_t *slot, unsigned int index, uint32_t *value)
{
	if (index >= 2 || !(slot[8] & (1 << index)))
		return 0;
	*value = __aeabi_uread4(slot + 4 * index);
	return 1;
}

int wlanParam64Set(uint8_t *slot, unsigned int index, const void *value)
{
	if (index >= 2)
		return 0;
	slot[16] |= 1 << index;
	__aeabi_memcpy(slot + 8 * index, value, 8);
	return 1;
}

int wlanParam64Get(const uint8_t *slot, unsigned int index, void *value)
{
	if (index >= 2 || !(slot[16] & (1 << index)))
		return 0;
	__aeabi_memcpy(value, slot + 8 * index, 8);
	return 1;
}

/* The configured filters; the last one is the copy that gets evaluated. */
struct WlanMefEntry wlanMefEntries[WLAN_MEF_ENTRIES] __attribute__((section(".itcm.bss")));
struct WlanMefConfig wlanMefConfig __attribute__((section(".itcm.bss")));
/* Address classes the wake filter lets through. */
uint32_t wlanWakeClassMask __attribute__((section(".itcm.bss")));

/*
 * Evaluates the working filter on a received frame. The 14 bytes in front of
 * the payload are borrowed for an Ethernet header and put back. Returns the
 * filter's verdict.
 */
int wlanMefMatchFrame(OsaBuffer *buffer)
{
	uint8_t saved[14];
	struct WlanMefPacket packet;
	struct WlanMefToken token;
	struct WlanMefStack *stack = &wlanMefEntries[WLAN_MEF_WORKING].stack;
	int verdict;

	__aeabi_memcpy(saved, (uint8_t *)buffer + buffer->dataOffset + 36 - 14, 14);
	if (!wlanBuildEthernetHeader(buffer, __aeabi_uread4(&wlanMefClassMask), &packet))
		return 0;
	wlanMefResetDepth();
	if (wlanMefEvaluate(stack, &packet)) {
		wlanMefTokenPop(stack, &token);
		verdict = token.number;
	} else {
		verdict = 0;
	}
	__aeabi_memcpy((void *)packet.data, saved, 14);
	return verdict;
}

void wlanMefConfigClear(void)
{
	__aeabi_memclr4(&wlanMefConfig, sizeof(wlanMefConfig));
}

/* Takes the address rules out of a list of TLVs (type, length, value; little endian). */
void wlanMefConfigParse(const uint8_t *tlvs, int length)
{
	unsigned int rules;
	unsigned int used;
	unsigned int size;

	*(uint32_t *)&wlanMefConfig.rules[0] = 0;
	while (length > 4) {
		size = (tlvs[3] << 8) | tlvs[2];
		if (((tlvs[1] << 8) | tlvs[0]) == WLAN_TLV_MEF_ADDRESS_RULES) {
			rules = size >> 3;
			if (size == 8 * rules && (int16_t)(8 * rules + 4) <= length) {
				used = rules < 8 ? rules : 8;
				__aeabi_memcpy(&wlanMefConfig.rules[0], tlvs + 4, 8 * used);
				while (rules < 8) {
					wlanMefConfig.rules[rules].addressType = 0;
					wlanMefConfig.rules[rules].protocol = 0;
					wlanMefConfig.rules[rules].target = 0;
					++rules;
				}
			}
		}
		length = (int16_t)(length - size - 4);
		tlvs += size + 4;
	}
}

/*
 * The wake filter: whether a received frame is let through by the address
 * class, the security override and the address rules. *securityDrop is set
 * when a security frame is held back.
 */
int wlanMefAcceptFrame(OsaBuffer *buffer, uint8_t *securityDrop)
{
	const uint8_t *frame = (const uint8_t *)buffer + buffer->dataOffset;
	const uint8_t *payload = frame + 36;
	const uint8_t *destination = (frame[3] & 1) ? frame + 18 : frame + 6;
	const uint16_t *protocol = NULL;
	struct WlanConnection *connection;
	unsigned int class;
	unsigned int type;
	unsigned int i;
	int isData = ((frame[2] >> 2) & 3) == 2;

	if (*(const uint16_t *)destination == 0xffff && *(const uint16_t *)(destination + 2) == 0xffff &&
	    *(const uint16_t *)(destination + 4) == 0xffff) {
		class = 1;
		type = 1;
	} else if (destination[0] & 1) {
		class = 8;
		type = 3;
	} else {
		class = 2;
		type = 2;
	}
	if (isData) {
		*securityDrop = 0;
		protocol = (const uint16_t *)(payload + 6);
		connection = buffer->context;
		if (wlanSecurityOverrideEnabled() && connection != NULL && (connection->flags & 4) &&
		    wlanIsSecurityProtocol(*protocol)) {
			*securityDrop = 1;
			return 0;
		}
	}
	if ((__aeabi_uread4(&wlanWakeClassMask) & class) == 0)
		return 0;
	if (!isData || *(uint32_t *)&wlanMefConfig.rules[0] == 0)
		return 1;
	for (i = 0; i < 8; ++i) {
		const struct WlanMefAddressRule *rule = &wlanMefConfig.rules[i];

		if (rule->addressType == 0)
			return 0;
		if ((rule->addressType == type || rule->addressType == 0xffff) &&
		    (rule->protocol == 0xffff || *protocol == rule->protocol) &&
		    (rule->target == 0xffffffff ||
		     (*protocol == 0x0608 && *(const uint32_t *)(protocol + 13) == rule->target)))
			return 1;
	}
	return 0;
}
