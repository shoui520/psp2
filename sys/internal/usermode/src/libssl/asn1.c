/* Private descriptive names in this file are ours, not recovered Sony names. */
#include "asn1.h"
#include "memory.h"
#include <string.h>

static const SceUInt8 sslAsn1Zero = 0;

int sslAsn1DecodeHeader(SslAsn1Element *element, const SceUInt8 *data, SceUInt32 size)
{
	SceUInt32 pos, tag, length, count, shift;
	SceUInt8 identifier, encoding;

	if (size < 2)
		return 2;
	identifier = data[0];
	encoding = identifier & 0xe0;
	element->encoding = encoding;
	tag = identifier & 0x1f;
	pos = 1;
	if (tag == 0x1f) {
		tag = 0;
		shift = 0;
		do {
			SceUInt8 byte = data[pos++];

			tag |= byte & 0x7f;
			if (byte & 0x80)
				break;
			shift += 7;
			if (shift > 32)
				return 1;
			tag <<= 7;
			if (pos >= size)
				return 2;
		} while (1);
	}
	element->tag = tag;
	if (pos >= size)
		return 2;
	length = data[pos++];
	if (length & 0x80) {
		count = length & 0x7f;
		length = 0;
		if (count == 0) {
			element->encoding = encoding | 2;
			if (!(identifier & 0x20))
				element->flags |= 8;
		} else {
			if (count > 4)
				return 3;
			if (pos + count >= size)
				return 2;
			do {
				length = (length << 8) | data[pos++];
			} while (--count);
		}
	}
	element->length = length;
	element->data = data + pos;
	element->headerLength = pos;
	return 0;
}

int sslAsn1Reserve(SslAsn1Tree *tree, SceUInt32 capacity)
{
	SslAsn1Element *elements, *oldElements;
	SceUInt32 first, last, address, count, i;
	int moved;

	if (capacity <= tree->count)
		return 0;
	if (!(tree->flags & 2))
		return 6;
	oldElements = tree->elements;
	if (oldElements) {
		moved = 1;
		elements = sslReallocate(oldElements, capacity * sizeof(*elements),
					 tree->capacity * sizeof(*elements));
		if (!elements)
			return 5;
		sslFillMemory(elements + tree->capacity, 0,
		       (capacity - tree->capacity) * sizeof(*elements));
	} else {
		moved = 0;
		elements = sslAllocate(capacity * sizeof(*elements));
		if (!elements)
			return 5;
		sslFillMemory(elements, 0, capacity * sizeof(*elements));
	}
	/* The allocator is a callback and may have changed the tree. */
	oldElements = tree->elements;
	if (elements != oldElements && moved) {
		count = tree->count;
		first = (SceUInt32)oldElements;
		last = first + count * sizeof(*elements);
		for (i = 0; i < count; ++i) {
			address = (SceUInt32)elements[i].parent;
			if (address >= first && address <= last)
				elements[i].parent = elements + (int)(address - first) / (int)sizeof(*elements);
			address = (SceUInt32)elements[i].next;
			if (address >= first && address <= last)
				elements[i].next = elements + (int)(address - first) / (int)sizeof(*elements);
			address = (SceUInt32)elements[i].child;
			if (address >= first && address <= last)
				elements[i].child = elements + (int)(address - first) / (int)sizeof(*elements);
		}
	}
	tree->capacity = capacity;
	tree->elements = elements;
	return 0;
}

int sslAsn1AppendElement(SslAsn1Tree *tree, SceUInt32 *index)
{
	SceUInt32 count = tree->count;
	SceUInt32 growth;
	int result;

	if (count >= tree->capacity) {
		growth = (count >> 1) + 20;
		if (growth >= 2000)
			growth = 2000;
		result = sslAsn1Reserve(tree, count + growth);
		if (result)
			return result;
		count = tree->count;
	}
	*index = count;
	tree->count++;
	memset(&tree->elements[count], 0, sizeof(*tree->elements));
	return 0;
}

int sslAsn1DecodeTree(SslAsn1Tree *tree, const SceUInt8 *data, SceUInt32 size,
		      SceUInt32 *consumed)
{
	const SceUInt8 *cursor = data;
	SceUInt32 end = (SceUInt32)data + size;
	SceUInt32 limit = end;
	SceUInt32 index;
	int previousIndex = -1;
	int descend = 1;
	int indefinite = 0;
	SslAsn1Element *element = 0;

	for (;;) {
		SslAsn1Element *previous;
		int result;
		int elementIndefinite;
		int constructed;

		result = sslAsn1AppendElement(tree, &index);
		if (result)
			return result;
		element = &tree->elements[index];
		result = sslAsn1DecodeHeader(element, cursor, limit - (SceUInt32)cursor);
		if (result)
			return result;
		if (element->length > limit - (SceUInt32)cursor)
			return 2;
		elementIndefinite = element->encoding & 2;
		constructed = element->encoding & 0x20;
		cursor += element->headerLength;
		previous = previousIndex == -1 ? 0 : &tree->elements[previousIndex];
		if (descend) {
			element->parent = previous;
			if (previous) {
				previous->child = element;
				if (!indefinite)
					limit = (SceUInt32)previous->data + previous->length;
				indefinite = previous->encoding & 2;
			} else {
				indefinite = 0;
			}
		} else if (previous) {
			element->parent = previous->parent;
			previous->next = element;
		}
		element->next = 0;
		descend = 0;
		previousIndex = index;
		if (!element->encoding && !element->tag && !element->length) {
			if (!indefinite)
				return 11;
			element->parent->length = cursor - element->parent->data;
			goto ascend;
		}
		if (constructed && (elementIndefinite || element->length)) {
			indefinite = elementIndefinite;
			descend = 1;
		} else {
			cursor += element->length;
		}
		if ((SceUInt32)cursor > limit)
			return 2;
		if (elementIndefinite && !constructed)
			return 12;
		while ((SceUInt32)cursor == limit) {
			SslAsn1Element *parent;

ascend:
			parent = element->parent;
			descend = 0;
			element = parent;
			if (!element) {
				if (consumed)
					*consumed = cursor - data;
				return 0;
			}
			previousIndex = element - tree->elements;
			parent = element->parent;
			indefinite = parent ? parent->encoding & 2 : 0;
			limit = parent && !indefinite ? (SceUInt32)parent->data + parent->length : end;
			if (limit < (SceUInt32)cursor)
				return 2;
		}
	}
}

SslAsn1Element *sslAsn1SetData(SslAsn1Element *element, const SceUInt8 *data, SceUInt32 length)
{
	element->length = length;
	element->flags |= 8;
	element->data = data;
	return element;
}

SslAsn1Element *sslAsn1SetTag(SslAsn1Element *element, int tagClass, SceUInt32 tag, int encoding)
{
	element->encoding = (tagClass & 0xc0) | (encoding & 0x2f);
	element->flags |= 8;
	if (!(tagClass & 0xc0) && (tag == 16 || tag == 17))
		element->encoding |= 0x20;
	element->tag = tag;
	return element;
}

SslAsn1Element *sslAsn1SetValue(SslAsn1Element *element, int tagClass, SceUInt32 tag,
			       const SceUInt8 *data, SceUInt32 length, int encoding, int flags)
{
	SceUInt8 tagEncoding = (tagClass & 0xc0) | (encoding & 0x2f);

	element->encoding = tagEncoding;
	if (!(tagClass & 0xc0) && (tag == 16 || tag == 17))
		element->encoding = tagEncoding | 0x20;
	element->data = data;
	element->tag = tag;
	element->length = length;
	element->flags |= 8 | flags;
	return element;
}

int sslAsn1SetInteger(SslAsn1Element *element, SceUInt32 value, SceUInt8 *buffer)
{
	int signByte, signBit, byte, index, length, allocated;

	element->flags &= ~4;
	if (!value) {
		element->encoding = 0;
		element->data = &sslAsn1Zero;
		element->tag = 2;
		element->length = 1;
		element->flags |= 8;
		return 0;
	}
	signByte = value & 0x80000000u ? 0xff : 0;
	signBit = value & 0x80000000u ? 0x80 : 0;
	allocated = 0;
	if (!buffer) {
		buffer = sslAllocate(4);
		if (!buffer)
			return 2;
		allocated = 1;
	}
	for (index = 3; index >= 0; --index) {
		byte = (value >> (index * 8)) & 0xff;
		if (byte != signByte)
			break;
	}
	length = 0;
	if (index < 0) {
		if (signBit)
			buffer[length++] = 0xff;
	} else {
		if ((byte & 0x80) != signBit)
			buffer[length++] = signByte;
		buffer[length++] = byte;
		while (--index >= 0)
			buffer[length++] = value >> (index * 8);
	}
	element->encoding = 0;
	element->data = buffer;
	element->tag = 2;
	element->length = length;
	element->flags |= 8 | allocated;
	return 0;
}

int sslAsn1CheckTag(const SslAsn1Element *element, SceUInt32 tag)
{
	int constructed;

	if (element->tag != tag || (element->encoding & 0xc0))
		return 7;
	constructed = tag == 16 || tag == 17;
	return constructed == !!(element->encoding & 0x20) ? 0 : 7;
}

int sslAsn1GetInteger(const SslAsn1Element *element, int *value)
{
	const SceUInt8 *data;
	SceUInt32 n, result, i;
	int error = sslAsn1CheckTag(element, 2);

	if (error)
		return error;
	n = element->length;
	if (!n) {
		*value = 0;
		return 0;
	}
	if (n > 4)
		return 9;
	data = element->data;
	if (element->flags & 4)
		result = element->prefix;
	else
		result = *data++;
	if (result & 0x80)
		result |= 0xffffff00u;
	for (i = 1; i < n; ++i)
		result = (result << 8) | *data++;
	*value = result;
	return 0;
}

int sslAsn1HeaderSize(const SslAsn1Element *element)
{
	SceUInt32 tag = element->tag;
	SceUInt32 length = element->length;
	int size = 1;

	if (tag >= 0x1f) {
		size = 2;
		if (tag >= 0x80)
			size = tag < 0x4000 ? 3 : 4;
	}
	if ((element->encoding & 2) || length < 0x80)
		return size + 1;
	if (length <= 0xff)
		return size + 2;
	if (length <= 0xffff)
		return size + 3;
	return size + (length <= 0xffffff ? 4 : 5);
}

int sslAsn1MeasureTree(SslAsn1Element *element)
{
	int total = 0;
	int ascending = 0;

	do {
		SslAsn1Element *parent, *next;
		SceUInt32 size;

		if (!ascending) {
			while (!(element->encoding & 8) &&
			       (((element->encoding & 0x20) && element->child) ||
				(element->encoding & 4))) {
				element->length = !element->data && !!(element->flags & 4);
				element->headerLength = 0;
				if (!element->child)
					break;
				element = element->child;
			}
		}
		if (element->encoding & 8)
			element->headerLength = 0;
		else
			element->headerLength = sslAsn1HeaderSize(element);
		parent = element->parent;
		element->flags &= ~8;
		size = element->length + element->headerLength;
		next = element->next;
		if (parent)
			parent->length += size;
		else
			total += size;
		if (next) {
			element = next;
			ascending = 0;
		} else {
			element = parent;
			ascending = 1;
		}
	} while (element);
	return total;
}

SslAsn1Tree *sslAsn1InitTree(SslAsn1Tree *tree, SslAsn1Element *elements,
			     SceUInt32 count, SceUInt32 capacity)
{
	tree->count = count;
	tree->capacity = capacity;
	tree->elements = elements;
	tree->flags = elements ? 0 : 2;
	return tree;
}

SslAsn1Element *sslAsn1InsertNext(SslAsn1Element *element, SslAsn1Element *next)
{
	SslAsn1Element *parent = element->parent;

	next->parent = parent;
	if (parent)
		parent->flags |= 8;
	next->next = element->next;
	element->next = next;
	return element;
}

int sslAsn1PrependChild(SslAsn1Element *parent, SslAsn1Element *child)
{
	if (!(parent->encoding & 0x20) && !(parent->encoding & 4))
		return 4;
	child->parent = parent;
	child->next = parent->child;
	parent->child = child;
	parent->flags |= 8;
	return 0;
}

void sslAsn1DestroyTree(SslAsn1Tree *tree)
{
	SceUInt32 i = 0;
	SceUInt32 capacity = tree->capacity;

	while (i < capacity) {
		SslAsn1Element *element = &tree->elements[i];

		if (element->data && (element->flags & 1)) {
			sslFree((void *)element->data);
			element = &tree->elements[i];
			capacity = tree->capacity;
			element->data = 0;
		}
		++i;
	}
	if ((tree->flags & 2) && tree->elements) {
		sslFree(tree->elements);
		tree->elements = 0;
	}
	tree->count = 0;
	if (tree->flags & 1)
		sslFree(tree);
}

SslAsn1Tree *sslAsn1ClearTree(SslAsn1Tree *tree)
{
	tree->count = 0;
	tree->capacity = 0;
	tree->elements = 0;
	tree->flags = 0;
	return tree;
}

SslAsn1Element *sslAsn1SetPrefix(SslAsn1Element *element, int prefix)
{
	if (!(element->flags & 4)) {
		element->flags |= 4;
		element->length++;
	}
	element->prefix = prefix;
	return element;
}

SslAsn1Element *sslAsn1GetData(SslAsn1Element *element, const SceUInt8 **data,
			       SceUInt32 *length)
{
	*data = element->data;
	*length = element->length;
	return element;
}

int sslAsn1EncodeHeader(const SslAsn1Element *element, SceUInt8 *data)
{
	SceUInt8 *cursor = data;
	SceUInt8 *lengthByte;
	SceUInt32 tag = element->tag;
	SceUInt32 length;
	SceUInt8 count;

	if (tag <= 0x1e) {
		*cursor++ = (element->encoding & 0xe0) | tag;
	} else {
		*cursor++ = (element->encoding & 0xe0) | 0x1f;
		if (tag >= 0x4000)
			*cursor++ = (tag >> 14) & 0x7f;
		if (tag >= 0x80)
			*cursor++ = (tag >> 7) & 0x7f;
		*cursor++ = tag & 0x7f;
	}
	lengthByte = cursor++;
	length = element->length;
	if (element->encoding & 2) {
		*lengthByte = 0x80;
	} else if (length <= 0x7f) {
		*lengthByte = length;
	} else {
		count = 1;
		if (length > 0xffffff) {
			*cursor++ = length >> 24;
			count++;
		}
		if (length > 0xffff) {
			*cursor++ = length >> 16;
			count++;
		}
		if (length > 0xff) {
			*cursor++ = length >> 8;
			count++;
		}
		*cursor++ = length;
		*lengthByte = 0x80 | count;
	}
	return cursor - data;
}

int sslAsn1EncodeTree(SslAsn1Element *element, SceUInt8 *data, SceUInt32 *written,
		      SceUInt32 size)
{
	SceUInt8 *cursor = data;
	SceUInt32 end = (SceUInt32)data + size;
	int ascending = 0;

	do {
		if (!ascending) {
			for (;;) {
				SceUInt32 length = element->length;

				if (element->flags & 8)
					return 12;
				if ((SceUInt32)cursor + length + element->headerLength > end)
					return 2;
				if (!(element->encoding & 8)) {
					cursor += sslAsn1EncodeHeader(element, cursor);
					length = element->length;
				}
				if ((element->flags & 4) && length) {
					--length;
					*cursor++ = element->prefix;
				}
				if (!(element->encoding & 8) &&
				    ((element->encoding & 0x20) || (element->encoding & 4))) {
					if (element->child) {
						element = element->child;
						continue;
					}
					if (element->data && length) {
						sslCopyMemory(cursor, element->data, length);
						cursor += length;
					}
				} else if (length) {
					sslCopyMemory(cursor, element->data, length);
					cursor += length;
				}
				break;
			}
		}
		if (element->next) {
			element = element->next;
			ascending = 0;
		} else {
			element = element->parent;
			ascending = 1;
		}
	} while (element);
	*written = cursor - data;
	return 0;
}
