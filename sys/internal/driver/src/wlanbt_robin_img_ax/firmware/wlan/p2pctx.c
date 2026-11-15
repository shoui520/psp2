/*
 * The P2P attributes of a group owner's frames (0x400e0 - 0x407a4): the attributes the host gave
 * and the ones the group answers with are put into vendor elements (OUI 50:6f:9a, type 9), an
 * attribute that does not fit goes on into a following element, and a received attribute is
 * read back out of its elements. The names of the firmware's placeholders are kept; see
 * robin/p2pgo.h.
 */
#include <stddef.h>
#include "robin/apsta.h"
#include "robin/conn.h"
#include "robin/mactx.h"
#include "robin/p2pgo.h"
#include "robin/rom.h"
#include "robin/runtime.h"

extern void abort(void);

/* The Channel List attribute: its header and the first channel run of the list. */
unsigned int p2pDirectHandleContext(const uint8_t *list, uint8_t *attribute)
{
	attribute[0] = P2P_ATTR_CHANNEL_LIST;
	__aeabi_memcpy(attribute + 3, list + 3, 3);
	attribute[6] = list[6];
	attribute[1] = 5;
	attribute[2] = 0;
	return 0;
}

unsigned int p2pDirectProcessContext(unsigned int id, const uint8_t *list, uint8_t *attribute)
{
	attribute[0] = id;
	__aeabi_memcpy(attribute + 3, list + 3, 3);
	attribute[6] = list[6];
	attribute[1] = 5;
	attribute[2] = 0;
	return 0;
}

/* The channels of the first list that the second has too become the group's common channels. */
void p2pDirectUpdateContext(struct WlanConnection *connection, const uint8_t *first, unsigned int firstCount,
			    const uint8_t *second, unsigned int secondCount)
{
	struct WlanPsPeerState *ps = wlanConnectionPsState(connection);
	unsigned int count = 0;
	unsigned int i;
	unsigned int j;

	p2pDirectHandleContext(second, (uint8_t *)&ps->commonChannels);
	for (i = 0; i < firstCount; i = (uint16_t)(i + 1)) {
		for (j = 0; j < secondCount; j = (uint16_t)(j + 1)) {
			if (first[8 + i] == second[8 + j]) {
				ps->commonChannels.channels[count] = first[8 + i];
				count = (uint16_t)(count + 1);
				break;
			}
		}
	}
	ps->commonChannels.count = count;
	{
		unsigned int length = ps->commonChannels.length[0] | (ps->commonChannels.length[1] << 8);

		length += count;
		ps->commonChannels.length[0] = length;
		ps->commonChannels.length[1] = length >> 8;
	}
}

/*
 * Appends the attribute to the P2P elements at element (a run of vendor elements ended by a zero
 * word), starting a new element when it does not fit (an element holds 255 bytes). The bytes added.
 */
unsigned int p2pDirectConfigureContext(uint8_t *element, const uint8_t *attribute)
{
	unsigned int added = 0;
	unsigned int room;
	unsigned int length = (attribute[2] << 8) | attribute[1];
	unsigned int total;
	uint8_t *tail;
	uint8_t *first;

	if (length == 0 || element == NULL)
		return 0;
	for (; *element == 0xdd; element += element[1] + 2) {
		if (element[1] < 0xf8) {
			room = (uint8_t)(-4 - element[1]);
			goto found;
		}
	}
	p2pDirectRecordState(element);
	added = 6;
	room = 248;
found:
	tail = &element[element[1] + 2];
	tail[0] = attribute[0];
	tail[1] = attribute[1];
	tail[2] = attribute[2];
	total = (uint16_t)(added + 3);
	if (room < length) {
		const uint8_t *source = attribute + 3;

		first = tail + 3;
		element[1] += 3;
		while (length > room) {
			__aeabi_memcpy(first, source, room);
			length = (uint16_t)(length - room);
			first += room;
			element[1] += room;
			if (length != 0) {
				p2pDirectRecordState(first);
				element = first;
				first += 6;
				total = (uint16_t)(total + 6);
			}
			total = (uint16_t)(total + room);
			source += room;
			room = 251;
		}
		if (length != 0) {
			__aeabi_memcpy(first, source, length);
			total = (uint16_t)(total + length);
			element[1] += length;
		}
		__aeabi_memclr(first + length, 6);
		return total;
	}
	__aeabi_memcpy(tail + 3, attribute + 3, length);
	total = (uint16_t)(length + total);
	element[1] += attribute[1] + 3;
	__aeabi_memclr(&tail[length + 3], 6);
	return total;
}

/*
 * Puts the attributes the masks name into the frame's P2P elements: bit n of include takes the
 * attribute n of the group (the host's: context, or this connection's own), bit n of exclude
 * clears it. -1 when a bit is in both. The bytes added.
 */
unsigned int p2pDirectCheckContext(struct WlanConnection *connection, WlanWirelessFrame *frame,
				   struct WlanDirectContext *context, unsigned int include,
				   unsigned int exclude)
{
	struct WlanPsPeerState *ps = wlanConnectionPsState(connection);
	uint8_t *elements = p2pDirectFilterState((uint8_t *)frame);
	uint8_t **attributes = context != NULL ? context->attributes : NULL;
	uint8_t *attribute = NULL;
	unsigned int added = 0;
	unsigned int i = 0;

	do {
		unsigned int bit = 1u << i;

		if (bit > include && bit > exclude)
			break;
		if (((include | exclude) & bit) != 0) {
			unsigned int own = include & bit;

			if (own != 0 && (exclude & bit) != 0)
				return -1;
			switch (bit) {
			case 1u << P2P_ATTR_STATUS:
				attribute = own ? (uint8_t *)&ps->p2pStatus : attributes[P2P_SLOT_STATUS];
				break;
			case 1u << P2P_ATTR_MINOR_REASON:
				break;
			case 1u << P2P_ATTR_CAPABILITY:
				attribute = own ? ps->capabilityAttribute : attributes[P2P_SLOT_CAPABILITY];
				break;
			case 1u << P2P_ATTR_DEVICE_ID:
				attribute = own ? ps->deviceIdAttribute : attributes[P2P_SLOT_DEVICE_ID];
				break;
			case 1u << P2P_ATTR_GO_INTENT:
				attribute = own ? ps->groupOwnerIntentAttribute : attributes[P2P_SLOT_GO_INTENT];
				break;
			case 1u << P2P_ATTR_CONFIGURATION_TIMEOUT:
				attribute = own ? ps->configTimeoutAttribute : attributes[P2P_SLOT_CONFIGURATION_TIMEOUT];
				break;
			case 1u << P2P_ATTR_LISTEN_CHANNEL:
				attribute = own ? ps->listenChannelAttribute : attributes[P2P_SLOT_LISTEN_CHANNEL];
				break;
			case 1u << P2P_ATTR_GROUP_BSSID:
				attribute = own ? ps->groupBssidAttribute : attributes[P2P_SLOT_GROUP_BSSID];
				break;
			case 1u << P2P_ATTR_EXTENDED_LISTEN_TIMING:
				attribute = own ? ps->extendedListenAttribute : attributes[P2P_SLOT_EXTENDED_LISTEN_TIMING];
				break;
			case 1u << P2P_ATTR_INTENDED_INTERFACE_ADDRESS:
				attribute = own ? ps->intendedAddressAttribute : attributes[P2P_SLOT_INTENDED_INTERFACE_ADDRESS];
				break;
			case 1u << P2P_ATTR_MANAGEABILITY:
				break;
			case 1u << P2P_ATTR_CHANNEL_LIST:
				attribute = own ? ps->channelListAttribute : attributes[P2P_SLOT_CHANNEL_LIST];
				break;
			case 1u << P2P_ATTR_NOTICE_OF_ABSENCE:
				break;
			case 1u << P2P_ATTR_DEVICE_INFO:
				attribute = own ? ps->deviceInfoAttribute : attributes[P2P_SLOT_DEVICE_INFO];
				break;
			case 1u << P2P_ATTR_GROUP_INFO:
				attribute = own ? ps->groupInfoAttribute : attributes[P2P_SLOT_GROUP_INFO];
				break;
			case 1u << P2P_ATTR_GROUP_ID:
				attribute = own ? &ps->groupIdType : attributes[P2P_SLOT_GROUP_ID];
				break;
			case 1u << P2P_ATTR_INTERFACE:
				attribute = own ? ps->interfaceAttribute : attributes[P2P_SLOT_INTERFACE];
				break;
			case 1u << P2P_ATTR_OPERATING_CHANNEL:
				attribute = own ? ps->operatingChannelAttribute : attributes[P2P_SLOT_OPERATING_CHANNEL];
				break;
			case 1u << P2P_ATTR_INVITATION_FLAGS:
				attribute = own ? ps->invitationFlagsAttribute : attributes[P2P_SLOT_INVITATION_FLAGS];
				break;
			}
			if (bit == 1u << P2P_ATTR_MINOR_REASON || bit == 1u << P2P_ATTR_MANAGEABILITY) {
				/* the attribute last looked at is emptied (the original never loads one for these) */
				attribute[1] = 0;
				attribute[2] = 0;
			} else if (((attribute[2] << 8) | attribute[1]) != 0) {
				added += p2pDirectConfigureContext(elements, attribute);
			}
		}
		i = (uint8_t)(i + 1);
	} while (i < P2P_ATTR_COUNT);
	return added;
}

/* Puts the attributes of a probe or association frame (Status and Capability... 0xc) into it. */
unsigned int p2pDirectValidateContext(struct WlanConnection *connection, WlanWirelessFrame *frame)
{
	uint8_t *buffer = (uint8_t *)frame;
	unsigned int added;

	wlanConnectionPsState(connection);
	added = (uint16_t)p2pDirectCheckContext(connection, frame, NULL, 0xc, 0);
	*(uint16_t *)buffer += added;
	return added;
}

/* The same with Capability and Device Info (0x2004); a group owner leaves its group capability out. */
unsigned int p2pDirectPrepareContext(struct WlanConnection *connection, WlanWirelessFrame *frame)
{
	struct WlanPsPeerState *ps = wlanConnectionPsState(connection);
	uint8_t *buffer = (uint8_t *)frame;
	unsigned int added;

	if (connection->mode == 1) {
		uint8_t options = ps->p2pOptions;

		ps->p2pOptions = 0;
		added = (uint16_t)p2pDirectCheckContext(connection, frame, NULL, 0x2004, 0);
		ps->p2pOptions = options;
	} else {
		added = (uint16_t)p2pDirectCheckContext(connection, frame, NULL, 0x2004, 0);
	}
	*(uint16_t *)buffer += added;
	return added;
}

/*
 * The vendor element of a management frame whose OUI is the pattern: NULL when the frame has none.
 * The elements start after the fixed fields of the frame's subtype; reassociation says the frame
 * counts them (the length it is checked against).
 */
uint8_t *p2pDirectDispatchContext(WlanWirelessFrame *frame, int reassociation, const void *oui, unsigned int size)
{
	uint8_t *buffer = (uint8_t *)frame;
	uint8_t *element = NULL;
	unsigned int consumed = 0;
	unsigned int length = (buffer[1] << 8) | buffer[0];

	switch (buffer[2] >> 4) {
	case 0:
		if (reassociation == 1)
			consumed = 4;
		element = buffer + 36;
		break;
	case 1:
		break;
	case 2:
		if (reassociation == 1)
			consumed = 10;
		element = buffer + 42;
		break;
	case 4:
		element = buffer + 32;
		break;
	case 5:
	case 8:
		if (reassociation == 1)
			consumed = 12;
		element = buffer + 44;
		break;
	default:
		return NULL;
	}
	do {
		unsigned int step;

		if (*element == 0xdd && memcmp(element + 2, oui, size) == 0)
			return element;
		step = element[1] + 2;
		element += step;
		consumed = (uint16_t)(consumed + step);
	} while (consumed < length);
	return NULL;
}

/*
 * The body of an attribute, which may go on in the elements that follow; remaining is what is left
 * of the current element. NULL when the next element is not a P2P one.
 */
const uint8_t *p2pDirectParseContext(const uint8_t *attribute, uint16_t *remaining)
{
	unsigned int length = (attribute[2] << 8) | attribute[1];
	const uint8_t *body = attribute + 3;

	*remaining -= 3;
	while (length != 0) {
		unsigned int left = *remaining;
		const uint8_t *next;

		if (left >= length) {
			*remaining -= length;
			body += length;
			return body;
		}
		next = left + body;
		length = (uint16_t)(length - left);
		if (memcmp(next + 2, wlanP2pOui, 4) != 0)
			return NULL;
		*remaining = next[1] - 4;
		body = next + 6;
	}
	return body;
}

/* 1 when the attributes do not fit the length or hold a Group Info attribute (14). */
unsigned int p2pDirectBuildContext(const uint8_t *attributes, unsigned int length)
{
	while (length != 0) {
		int size = (attributes[2] << 8) | attributes[1];

		if ((unsigned int)(size + 3) > length || attributes[0] == P2P_ATTR_GROUP_INFO)
			return 1;
		attributes += size + 3;
		length = (uint8_t)(length - size - 3);
	}
	return 0;
}

/* The header of a WPS element. */
__attribute__((noinline)) void p2pDirectComputeContext(uint8_t *element)
{
	element[0] = 0xdd;
	element[1] = 4;
	__aeabi_memcpy(element + 2, wlanWpsOui, 4);
}

/* Closes the element being filled (its length byte takes the attribute's) and starts the next. */
uint8_t *p2pDirectQueryContext(uint8_t **next, uint8_t **length, uint16_t *remaining, unsigned int size,
			       uint16_t *total, uint16_t *frameLength, const void *oui)
{
	**length = *remaining;
	*frameLength += *remaining + 2;
	if (memcmp(oui, wlanP2pOui, size) == 0)
		p2pDirectRecordState(*next);
	else
		p2pDirectComputeContext(*next);
	*length = *next + 1;
	*total += *remaining;
	*remaining = size;
	*next = *next + size + 2;
	return *next;
}

/* The next P2P element at or after the position. */
uint8_t *p2pDirectResetContext(uint8_t *position)
{
	do {
		if (*position == 0xdd && memcmp(position + 2, wlanP2pOui, 4) == 0)
			break;
		++position;
	} while (*position != 0xdd);
	return position;
}

/*
 * Copies an attribute that may be spread over several P2P elements into one piece at destination;
 * limit is what the destination holds (-1: unlimited).
 */
void p2pDirectSyncContext(const uint8_t *source, uint8_t *destination, int limit, uint8_t **mark)
{
	unsigned int done = 0;
	unsigned int length;
	uint8_t *out;
	const uint8_t *body;
	int distance;

	if (source == NULL)
		return;
	distance = (uint8_t)(p2pDirectResetContext((uint8_t *)source) - source);
	if (*mark != NULL)
		*mark = destination;
	if (limit != -1 && distance >= limit) {
		__aeabi_memcpy(destination, source, limit);
		return;
	}
	if (distance == 1) {
		destination[0] = source[0];
		source += 6;
		destination[1] = source[1];
		length = (source[2] << 8) | source[1];
		destination[2] = source[2];
		out = destination + 3;
	} else {
		if (distance == 3) {
			__aeabi_memcpy(destination, source, 3);
			length = (source[2] << 8) | source[1];
			source += 6;
		} else {
			__aeabi_memcpy(destination, source, 3);
			length = (source[2] << 8) | source[1];
		}
		out = destination + 3;
	}
	if (limit != -1 && (unsigned int)(limit - 3) < length)
		length = (uint16_t)(limit - 3);
	body = source + 3;
	while (done < length) {
		out[done] = body[done];
		if (body[done + 1] == 0xdd && memcmp(&body[done] + 3, wlanP2pOui, 4) == 0)
			body += 6;
		done = (uint16_t)(done + 1);
	}
}
