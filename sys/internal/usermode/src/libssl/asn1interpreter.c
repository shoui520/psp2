/* Private descriptive names and this filename are ours, not recovered Sony names.
 * The original keeps these interpreter methods separate from the low-level codec.
 */
#include "asn1.h"
#include "memory.h"

static SceUInt32 sslAsn1True = 0xff;
static SceUInt32 sslAsn1False;

SslAsn1Tree *sslAsn1CreateTree(void)
{
	SslAsn1Tree *tree = sslAllocate(sizeof(*tree));

	if (!tree)
		return 0;
	sslAsn1InitTree(tree, 0, 0, 0);
	tree->flags |= 1;
	return tree;
}

SceUInt32 sslAsn1ElementCount(const SslAsn1Tree *tree)
{
	return tree->count;
}

SslAsn1Element *sslAsn1ElementAt(const SslAsn1Tree *tree, SceUInt32 index)
{
	return tree->elements + index;
}

int sslAsn1Navigate(SslAsn1Navigator *navigator, int command, int destination,
		    int count, int group)
{
	SslAsn1Element *element = 0;
	int remaining = 0;
	int requested;

	if (command & 8) {
		remaining = count;
		if (count & 0x80)
			remaining = navigator->registers[count & 15];
		if (navigator->currentIndex < 0)
			return 10008;
		element = navigator->tree->elements + navigator->currentIndex;
	}
	requested = remaining;
	switch (command) {
	case 0:
		navigator->savedIndices[(SceUInt32)count | ((SceUInt32)group << 8)] =
			navigator->currentIndex;
		return 0;
	case 1:
		navigator->mode = 1;
		break;
	case 2:
		navigator->mode = 0;
		break;
	case 3:
		break;
	case 8:
		while (remaining > 0 && element->child) {
			--remaining;
			element = element->child;
		}
		goto moved;
	case 9:
		while (remaining > 0 && element->parent) {
			--remaining;
			element = element->parent;
		}
		goto moved;
	case 10:
		while (remaining > 0 && element->next) {
			--remaining;
			element = element->next;
		}
moved:
		if (destination & 0x80)
			navigator->registers[destination & 15] = requested - remaining;
		else if (remaining)
			return 10008;
		navigator->currentIndex = (SceInt32)((SceUInt32)element -
						      (SceUInt32)navigator->tree->elements) /
					  (int)sizeof(*element);
		return 0;
	default:
		return 10018;
	}
	if (destination)
		navigator->currentIndex = navigator->savedIndices[destination];
	return 0;
}

int sslAsn1BuildElement(SslAsn1Navigator *navigator, int command, int first,
		      int second, int third)
{
	SslAsn1Element *element;
	SslAsn1Callback callback;
	SceUInt32 index;
	int editing = command & 8;
	int callbackCommand;
	int result = 0;

	if (first & 0x80)
		first = navigator->registers[first & 15];
	if (second & 0x80)
		second = navigator->registers[second & 15];
	if (third & 0x80)
		third = navigator->registers[third & 15];
	if (editing) {
		if (navigator->currentIndex < 0)
			return 10008;
		element = navigator->tree->elements + navigator->currentIndex;
	} else {
		if (sslAsn1AppendElement(navigator->tree, &index))
			return 10005;
		element = navigator->tree->elements + index;
	}

	switch (command) {
	case 0:
	case 1:
	case 5:
	case 7:
	case 10:
	case 11:
		callback = navigator->callback;
		navigator->currentElement = element;
		if (!callback)
			return 10010;
		if (command == 0) {
			result = callback(navigator, navigator->callbackArg, navigator->registers,
					  first, second, third);
		} else if (command == 1) {
			result = callback(navigator, navigator->callbackArg, navigator->registers,
					  2, first, second);
			if (element->length && (*element->data & 0x80))
				sslAsn1SetPrefix(element, 0);
		} else {
			switch (command) {
			case 5:
				callbackCommand = 69;
				break;
			case 7:
				callbackCommand = 65;
				break;
			case 10:
				callbackCommand = 73;
				break;
			default:
				callbackCommand = 74;
				break;
			}
			result = callback(navigator, navigator->callbackArg, 0,
					  callbackCommand, first, second);
		}
		break;
	case 2:
		sslAsn1SetValue(element, first, second, 0, 0, third, 0);
		break;
	case 3:
		/* The original ignores the integer setter's allocation failure. */
		sslAsn1SetInteger(element, first, 0);
		break;
	case 4:
		sslAsn1SetValue(element, 0, 1,
			       (const SceUInt8 *)(first ? &sslAsn1True : &sslAsn1False),
			       1, 0, 0);
		break;
	case 6:
		sslAsn1SetValue(element, 0x80, first, 0, 0, second, 0);
		break;
	case 8:
		element->tag = first;
		element->encoding = (second & 0xc0) | (third & 0x2f);
		return 0;
	case 9:
		sslAsn1SetPrefix(element, first);
		return 0;
	default:
		return 10011;
	}

	/* Callback errors still leave the new element linked, as in the original. */
	if (!editing) {
		if (navigator->currentIndex >= 0) {
			SslAsn1Element *current =
				navigator->tree->elements + navigator->currentIndex;

			if (navigator->mode)
				sslAsn1PrependChild(current, element);
			else
				sslAsn1InsertNext(current, element);
		}
		navigator->currentIndex = index;
		navigator->mode = 0;
	}
	return result;
}

int sslAsn1InvokeCallback(SslAsn1Navigator *navigator, int skipFirst, int first,
			 int second, int third)
{
	SslAsn1Callback callback = navigator->callback;
	SslAsn1Element *element = 0;
	int result;

	if (!callback)
		return 2;
	if (first & 0x80)
		first = navigator->registers[first & 15];
	if (second & 0x80)
		second = navigator->registers[second & 15];
	if (third & 0x80)
		third = navigator->registers[third & 15];
	if (navigator->currentIndex >= 0)
		element = navigator->tree->elements + navigator->currentIndex;
	navigator->currentElement = element;
	if (skipFirst == 1) {
		if (!element || !element->length)
			return 10022;
		--element->length;
		++element->data;
	}
	result = callback(navigator, navigator->callbackArg, navigator->registers,
			  first, second, third);
	if (skipFirst == 1) {
		++element->length;
		--element->data;
	}
	return result;
}

int sslAsn1InspectElement(SslAsn1Navigator *navigator, int command, int first,
			  int second, int third)
{
	SslAsn1Element *element;
	SslAsn1Element integer;
	int forceInteger;
	int result;

	if (navigator->currentIndex < 0)
		return 10008;
	element = navigator->tree->elements + navigator->currentIndex;
	if (command == 0) {
		if (first & 0x80)
			navigator->registers[first & 15] = element->encoding & 0xc0;
		if (second & 0x80)
			navigator->registers[second & 15] = element->tag;
		if (third & 0x80)
			navigator->registers[third & 15] = element->encoding & 0x2f;
		return 0;
	}
	if (command == 1) {
		if (!(first & 0x80))
			return 10018;
		forceInteger = second;
		if (second & 0x80)
			forceInteger = navigator->registers[second & 15];
		if (forceInteger) {
			integer.length = element->length;
			integer.data = element->data;
			integer.tag = 2;
			integer.encoding = 0;
			integer.headerLength = element->headerLength;
			integer.flags = element->flags;
			integer.prefix = element->prefix;
			element = &integer;
		}
		result = sslAsn1GetInteger(element, &navigator->registers[first & 15]);
		return result ? 10001 : 0;
	}
	if (command == 2) {
		if (first & 0x80)
			navigator->registers[first & 15] = element->length;
		return 0;
	}
	return 10011;
}
