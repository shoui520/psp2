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
#include "robin/sys.h"
#include "robin/uapbss.h"

extern void abort(void);
extern void __aeabi_memclr4(void *dest, size_t n);

struct WlanInitStage p2pDirectInitStage __attribute__((section(".itcm.rw")));

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
				attribute = own ? (uint8_t *)&ps->groupInfoAttribute : attributes[P2P_SLOT_GROUP_INFO];
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

/*
 * The Probe Response or Association attributes of a peer, merged into the frame being built: the
 * elements in the received frame with the OUI are copied into elements of at most 255 bytes, a P2P
 * attribute that is the last of its element is joined with the next one, and the Group Info
 * attribute is dropped. The result is bounded by 900 bytes (limit is what the frame already holds).
 * The length of the elements built.
 */
unsigned int p2pDirectApplyContext(uint8_t *out, const void *oui, unsigned int size, const uint8_t *in,
				   uint16_t limit)
{
	uint16_t frameLength = 0;
	uint8_t *length = out + 1;
	uint8_t *dst = out + 2;
	uint16_t remaining;
	const uint8_t *element = in;
	uint16_t left;

	out[0] = 0xdd;
	__aeabi_memcpy(dst, oui, size);
	remaining = size;
	dst += size;
	while (memcmp(element + 2, oui, size) == 0) {
		const uint8_t *body = element + size;
		const uint8_t *attribute = body + 2;

		if (memcmp(element + 2, wlanP2pOui, size) == 0 &&
		    p2pDirectBuildContext(attribute, (uint8_t)(element[1] - size)) != 0) {
			unsigned int bytes = (uint8_t)(element[1] - size);

			element = body + 2;
			left = bytes;
			for (;;) {
				unsigned int attributeLength;
				unsigned int used;

				if (left == 0)
					break;
				attributeLength = (element[2] << 8) | element[1];
				used = (uint16_t)(remaining + 3);
				if (attributeLength + limit + used >= 900) {
					*length = remaining;
					return remaining;
				}
				if (*element == P2P_ATTR_GROUP_INFO) {
					element = p2pDirectParseContext(element, &left);
					continue;
				}
				if (attributeLength + 3 >= left) {
					if (attributeLength + used >= 255)
						p2pDirectQueryContext(&dst, &length, &remaining, size, &limit, &frameLength,
								      wlanP2pOui);
					if ((unsigned int)(((element[2] << 8) | element[1]) + 3) <= 251) {
						p2pDirectSyncContext(element, dst, -1, NULL);
						remaining = ((element[2] << 8) | element[1]) + remaining + 3;
						dst += ((element[2] << 8) | element[1]) + 3;
					}
					element = p2pDirectParseContext(element, &left);
					continue;
				}
				if (attributeLength + used >= 255)
					p2pDirectQueryContext(&dst, &length, &remaining, size, &limit, &frameLength, wlanP2pOui);
				__aeabi_memcpy(dst, element, ((element[2] << 8) | element[1]) + 3);
				dst += ((element[2] << 8) | element[1]) + 3;
				remaining = ((element[2] << 8) | element[1]) + remaining + 3;
				attributeLength = (element[2] << 8) | element[1];
				element += attributeLength + 3;
				left = (uint16_t)(left - attributeLength - 3);
			}
			continue;
		}
		if ((int)(limit + remaining + element[1] - size) >= 900)
			break;
		if ((int)(element[1] + remaining - size) >= 255)
			p2pDirectQueryContext(&dst, &length, &remaining, size, &limit, &frameLength, oui);
		__aeabi_memcpy(dst, body + 2, element[1] - size);
		remaining = element[1] - size + remaining;
		dst += element[1] - size;
		element += element[1] + 2;
	}
	*length = remaining;
	return (uint16_t)(frameLength + remaining + 2);
}

/*
 * Reads the attributes of the P2P elements that start at element into context (one pointer per slot)
 * and sets bit n of mask for the attribute n found; changed gets the bits that were new while an
 * attribute went on into the next element. length is what the frame holds from the element on.
 */
void p2pDirectResolveContext(const uint8_t *element, unsigned int *mask, unsigned int *changed, int length,
			     struct WlanDirectContext *context)
{
	static const uint8_t slots[19] = {
		P2P_SLOT_STATUS, P2P_SLOT_MINOR_REASON, P2P_SLOT_CAPABILITY, P2P_SLOT_DEVICE_ID,
		P2P_SLOT_GO_INTENT, P2P_SLOT_CONFIGURATION_TIMEOUT, P2P_SLOT_LISTEN_CHANNEL,
		P2P_SLOT_GROUP_BSSID, P2P_SLOT_EXTENDED_LISTEN_TIMING, P2P_SLOT_INTENDED_INTERFACE_ADDRESS,
		P2P_SLOT_MANAGEABILITY, P2P_SLOT_CHANNEL_LIST, P2P_SLOT_NOTICE_OF_ABSENCE,
		P2P_SLOT_DEVICE_INFO, P2P_SLOT_GROUP_INFO, P2P_SLOT_GROUP_ID, P2P_SLOT_INTERFACE,
		P2P_SLOT_OPERATING_CHANNEL, P2P_SLOT_INVITATION_FLAGS,
	};
	uint16_t left = 0;
	uint16_t consumed;
	const uint8_t *position;

	*changed = 0;
	*mask = 0;
	if (element == NULL)
		return;
	position = element;
	while (memcmp(position + 2, wlanP2pOui, 4) == 0) {
		unsigned int bytes = position[1];

		position += 6;
		consumed = bytes + 2;
		left = bytes - 4;
		for (;;) {
			unsigned int before;
			unsigned int next;

			if (left == 0)
				break;
			before = *mask;
			if (*position > 18)
				return;
			context->attributes[slots[*position]] = (uint8_t *)position;
			*mask |= 1u << *position;
			next = (uint16_t)(((position[2] << 8) | position[1]) + 3);
			if (next <= left) {
				position += next;
				left = left - next;
				continue;
			}
			*changed |= *mask ^ before;
			{
				const uint8_t *start = position;

				position = p2pDirectParseContext(position, &left);
				if (position == NULL)
					return;
				length = (int16_t)(length - (position - start));
			}
			if (left == 0 || length == 0) {
				consumed = 0;
				goto element_done;
			}
			return;
		}
		element_done:
		length = (int16_t)(length - consumed);
		if ((int16_t)length <= 0 || position == NULL)
			return;
	}
}

/*
 * Finds the P2P elements of a frame (body: they start at its fixed offset 40; else by their OUI)
 * and reads their attributes into context. Returns the context.
 */
struct WlanDirectContext *p2pDirectScheduleContext(const uint8_t *frame, int body, unsigned int *mask,
						   unsigned int *changed, struct WlanDirectContext *context)
{
	unsigned int found = 0;
	unsigned int fresh = 0;
	const uint8_t *element;

	if (body != 0)
		element = frame + 40;
	else
		element = p2pDirectDispatchContext((WlanWirelessFrame *)(uintptr_t)frame, 1, wlanP2pOui, 4);
	if (element != NULL) {
		int length = (int16_t)((((frame[1] << 8) | frame[0]) - (uint16_t)(element - frame)) + 32);

		__aeabi_memclr4(context, sizeof(*context));
		p2pDirectResolveContext(element, &found, &fresh, length, context);
		if (mask != NULL)
			*mask = found;
		if (changed != NULL)
			*changed = fresh;
	}
	return context;
}

/*
 * An event for the host about a peer's frame: the peer's address, the event type and value, the
 * group's setting for types 3 and 4, and the P2P and WPS elements of the frame (types above 8),
 * with a Group ID attribute added to a probe response of a group owner.
 */
unsigned int p2pDirectNotifyContext(struct WlanConnection *connection, const uint8_t *frame, uint8_t *event,
				    unsigned int type, uint16_t value)
{
	struct WlanPsPeerState *ps = wlanConnectionPsState(connection);
	struct WlanDirectContext context;
	uint8_t groupId[40];
	uint8_t deviceInfoCopy[12];
	uint8_t capabilityCopy[8];
	unsigned int mask;
	unsigned int length = 0;
	unsigned int used;
	unsigned int count;
	uint8_t *out;

	event[0] = 2;
	event[1] = 0;
	__aeabi_memcpy(event + 6, frame + 12, 6);
	*(uint16_t *)event += 6;
	*(uint16_t *)(event + 2) = type;
	*(uint16_t *)(event + 4) = value;
	*(uint16_t *)event += 4;
	out = event + 12;
	if (type == 3 || type == 4) {
		*(uint32_t *)out = ps->persistentGroupIndex;
		out = event + 16;
		*(uint16_t *)event += 4;
	}
	count = (frame[1] << 8) | frame[0];
	if (count > 32) {
		unsigned int added;

		if (type > 8) {
			const uint8_t *p2p = p2pDirectDispatchContext((WlanWirelessFrame *)(uintptr_t)frame, 1, wlanP2pOui, 4);
			const uint8_t *wps = p2pDirectDispatchContext((WlanWirelessFrame *)(uintptr_t)frame, 1, wlanWpsOui, 4);
			unsigned int before = (event[1] << 8) | event[0];

			*(uint16_t *)event = p2pDirectApplyContext(out, wlanP2pOui, 4, p2p, before) + before;
			used = (uint16_t)(((event[1] << 8) | event[0]) - before);
			if (p2p != NULL && (frame[2] >> 4) == 5) {
				mask = 0;
				p2pDirectScheduleContext(frame, 0, NULL, &mask, &context);
				if (mask & 0x2000)
					p2pDirectSyncContext(context.attributes[P2P_SLOT_DEVICE_INFO], deviceInfoCopy, 9,
							     &context.attributes[P2P_SLOT_DEVICE_INFO]);
				if (mask & 4)
					p2pDirectSyncContext(context.attributes[P2P_SLOT_CAPABILITY], capabilityCopy, 5,
							     &context.attributes[P2P_SLOT_CAPABILITY]);
				if (context.attributes[P2P_SLOT_CAPABILITY] != NULL &&
				    (context.attributes[P2P_SLOT_CAPABILITY][4] & 1) != 0) {
					unsigned int size;

					groupId[0] = P2P_ATTR_GROUP_ID;
					if (context.attributes[P2P_SLOT_DEVICE_INFO] != NULL)
						__aeabi_memcpy(groupId + 3, context.attributes[P2P_SLOT_DEVICE_INFO] + 3, 6);
					__aeabi_memcpy(groupId + 9, frame + 46, frame[45]);
					size = frame[45] + 6;
					groupId[1] = size;
					groupId[2] = size >> 8;
					if (((event[1] << 8) | event[0]) <= 900) {
						uint8_t *header;

						length = (uint16_t)(size + 3);
						p2pDirectRecordState(&out[used]);
						used = (uint16_t)(used + 6);
						*(uint16_t *)event += 6;
						header = &out[used];
						__aeabi_memcpy(header, groupId, length);
						*(uint16_t *)event += length;
						header[-5] += length;
					}
				}
			}
			added = p2pDirectApplyContext(&out[used + length], wlanWpsOui, 4, wps, (event[1] << 8) | event[0]);
		} else {
			added = p2pDirectApplyContext(out, wlanP2pOui, 4, frame + 40, (event[1] << 8) | event[0]);
		}
		*(uint16_t *)event += added;
		count = event[1];
	}
	return count;
}

/* A random listen period: 100 ms times a number between the shortest and the longest discovery period. */
int p2pDirectFormatContext(struct WlanConnection *connection)
{
	struct WlanPsPeerState *ps = wlanConnectionPsState(connection);

	return 100 * (rand() % (ps->maxDiscoveryPeriod - ps->minDiscoveryPeriod + 1) + ps->minDiscoveryPeriod);
}

int p2pDirectInspectContext(WlanWirelessFrame *frame)
{
	return p2pDirectDispatchContext(frame, 1, wlanP2pOui, 4) != NULL;
}

int p2pDirectTrackContext(const WlanWirelessFrame *frame)
{
	return frame->body[1] == 0;
}

/* The tie-breaker bit counts up: 1 or 2. */
void p2pDirectManageContext(struct WlanConnection *connection)
{
	struct WlanPsPeerState *ps = wlanConnectionPsState(connection);

	ps->tieBreaker = (ps->tieBreaker & 1) + 1;
}

/* The group's state is cleared when the firmware starts, and again when the init stage runs. */
int p2pDirectAcquireContext(void)
{
	__aeabi_memclr4(p2pPeerStateStorage, sizeof(p2pPeerStateStorage));
	__aeabi_memclr(p2pPersistentGroupStorage, sizeof(p2pPersistentGroupStorage));
	p2pPersistentGroupsInit((struct WlanPsPeerState *)p2pPeerStateStorage);
	p2pDirectInitStage.run = p2pDirectAcquireContext;
	wlanInitStageRegister(&p2pDirectInitStage);
	return 0;
}

/*
 * Adds the sender of an association request to the Group Info attribute's clients (or finds it
 * there): its device address and capability from the request's P2P attributes, its interface
 * address, and its Device Info. NULL when the request has no Device Info or Capability.
 */
struct WlanP2pClient *p2pDirectReleaseContext(struct WlanConnection *connection, const WlanWirelessFrame *request)
{
	struct WlanPsPeerState *ps = wlanConnectionPsState(connection);
	struct WlanDirectContext context;
	struct WlanP2pClient *client;
	unsigned int mask = 0;
	unsigned int changed = 0;
	unsigned int count = 0;
	uint8_t deviceInfoCopy[84];
	uint8_t capabilityCopy[8];
	unsigned int length;
	unsigned int irq;

	p2pDirectScheduleContext((const uint8_t *)request, 0, &changed, &mask, &context);
	if (context.attributes[P2P_SLOT_CAPABILITY] == NULL || context.attributes[P2P_SLOT_DEVICE_INFO] == NULL)
		return NULL;
	(void)changed;
	if (mask & 0x2000)
		p2pDirectSyncContext(context.attributes[P2P_SLOT_DEVICE_INFO], deviceInfoCopy, -1,
				     &context.attributes[P2P_SLOT_DEVICE_INFO]);
	if (mask & 4)
		p2pDirectSyncContext(context.attributes[P2P_SLOT_CAPABILITY], capabilityCopy, -1,
				     &context.attributes[P2P_SLOT_CAPABILITY]);
	client = (struct WlanP2pClient *)ps->groupInfoAttribute.clients;
	while (ps->groupClientCount > count) {
		if (memcmp(client->deviceAddress, context.attributes[P2P_SLOT_DEVICE_INFO] + 3, 6) == 0)
			return client;
		client = (struct WlanP2pClient *)((uint8_t *)client + client->length + 1);
		count = (uint8_t)(count + 1);
	}
	irq = sysIrqDisable();
	__aeabi_memclr(client, 0x59);
	client->capability = context.attributes[P2P_SLOT_CAPABILITY][3];
	__aeabi_memcpy(client->deviceAddress, context.attributes[P2P_SLOT_DEVICE_INFO] + 3, 6);
	__aeabi_memcpy(client->interfaceAddress, (const uint8_t *)request + 12, 6);
	length = (context.attributes[P2P_SLOT_DEVICE_INFO][2] << 8) | context.attributes[P2P_SLOT_DEVICE_INFO][1];
	__aeabi_memcpy(client->information, context.attributes[P2P_SLOT_DEVICE_INFO] + 9, length - 6);
	client->length = context.attributes[P2P_SLOT_DEVICE_INFO][1] + 7;
	ps->groupInfoAttribute.id = P2P_ATTR_GROUP_INFO;
	length = ((ps->groupInfoAttribute.length[1] << 8) | ps->groupInfoAttribute.length[0]) + length + 8;
	ps->groupInfoAttribute.length[0] = length;
	ps->groupInfoAttribute.length[1] = length >> 8;
	ps->groupClientCount++;
	ps->p2pAuthCount++;
	sysIrqRestore(irq);
	macMgmtScheduleFrame(connection, 5);
	return client;
}

/* Removes the client from the Group Info attribute, closing the gap and pointing its peers' roles at the moved entries. */
int p2pDirectSetupContext(struct WlanConnection *connection, struct WlanP2pClient *client)
{
	struct WlanPsPeerState *ps;
	struct WlanP2pClient *entry;
	unsigned int size;
	unsigned int index = 0;
	unsigned int irq;

	if (client == NULL)
		return 0;
	ps = wlanConnectionPsState(connection);
	size = (uint8_t)(client->length + 1);
	if (((ps->groupInfoAttribute.length[1] << 8) | ps->groupInfoAttribute.length[0]) < (int)size)
		return 0;
	entry = (struct WlanP2pClient *)ps->groupInfoAttribute.clients;
	while (ps->groupClientCount > index && memcmp(entry->deviceAddress, client->deviceAddress, 6) != 0) {
		entry = (struct WlanP2pClient *)((uint8_t *)entry + entry->length + 1);
		index = (uint8_t)(index + 1);
	}
	irq = sysIrqDisable();
	if ((int)ps->groupClientCount - 1 > (int)index) {
		struct WlanP2pClient *next = (struct WlanP2pClient *)((uint8_t *)entry + entry->length + 1);

		while ((int)ps->groupClientCount - 1 > (int)index) {
			unsigned int bytes = (uint8_t)(next->length + 1);
			struct WlanConnection *peer;

			if (entry != next)
				__aeabi_memcpy(entry, next, bytes);
			peer = wlanConnectionFindByPeer(entry->deviceAddress);
			if (peer != NULL) {
				struct WlanConnectionRole *role = wlanConnectionInfoSlot(peer);

				if (role->p2pContext == NULL)
					return 0;
				role->p2pContext = entry;
			}
			entry = (struct WlanP2pClient *)((uint8_t *)entry + bytes);
			next = (struct WlanP2pClient *)((uint8_t *)next + bytes);
			index = (uint8_t)(index + 1);
		}
	}
	{
		unsigned int length = ((ps->groupInfoAttribute.length[1] << 8) | ps->groupInfoAttribute.length[0]) - size;

		ps->groupInfoAttribute.length[0] = length;
		ps->groupInfoAttribute.length[1] = length >> 8;
	}
	ps->groupClientCount--;
	ps->p2pAuthCount--;
	sysIrqRestore(irq);
	macMgmtScheduleFrame(connection, 5);
	return 1;
}
