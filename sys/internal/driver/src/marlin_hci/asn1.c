/* Descriptive source name: the original library has no debug records. */
#include "asn1.h"

int MarlinAsn1ReadLength(const unsigned char *data, const unsigned char *end,
			const unsigned char **next, int *size)
{
	unsigned int index;
	int length = 0;
	unsigned int lengthOctets = *data & 0x7f;
	unsigned int remaining = end - data;

	if ((signed char)*data < 0) {
		if (lengthOctets > 4 || remaining - 1 < lengthOctets)
			return 0x22000003;
		for (index = 0; index < lengthOctets; index++) {
			length <<= 8;
			length += data[index + 1];
		}
		if (length < 0)
			return 0x22000003;
		*size = length;
		*next = data + index + 1;
	} else {
		*size = *data;
		*next = data + 1;
	}
	if (end != 0 && *next + *size > end)
		return 0x22000004;
	return 0;
}

int MarlinAsn1CreateNode(void *allocator, int tagClass, int constructed,
			 int tag, MarlinAsn1Node **output);
int MarlinAsn1CreatePrimitive(void *allocator, int tagClass, int constructed,
			      int tag, unsigned int size, const void *data,
			      int reference, MarlinAsn1Node **output);
int MarlinAsn1AppendChild(void *allocator, MarlinAsn1Node *parent,
			 MarlinAsn1Node *child);

int MarlinAsn1ParseNode(void *allocator, int mode, const unsigned char *input,
			const unsigned char *end, unsigned int *consumed,
			const unsigned char **next, MarlinAsn1Node **output)
{
	int result = 0;
	int tag;
	int constructed;
	int tagClass;
	unsigned int remaining;
	const unsigned char *cursor = input;
	int size;
	MarlinAsn1Node *node;
	unsigned int childSize;
	MarlinAsn1Node *child;

	tag = *cursor & 0x1f;
	constructed = *cursor & 0x20;
	tagClass = (unsigned char)(*cursor >> 6) & 7;
	node = 0;
	remaining = end - input;
	child = 0;
	if (remaining <= 1) {
		result = 0x22000003;
		goto out;
	}
	if (constructed != 0) {
		if (mode != 2 && mode != 3) {
			result = MarlinAsn1CreateNode(allocator, tagClass,
						    constructed, tag, &node);
			if (result != 0)
				goto out;
		}
		if (cursor[1] == 0x80) {
			if (mode != 2 && mode != 3)
				node->flags |= 1;
			size = -1;
			cursor += 2;
		} else {
			result = MarlinAsn1ReadLength(cursor + 1, end, &cursor, &size);
			if (result != 0)
				goto out;
			end = cursor + size;
		}
		while (end == 0 || cursor < end) {
			if (size < 0 && cursor[0] == 0 && cursor[1] == 0) {
				cursor += 2;
				break;
			}
			result = MarlinAsn1ParseNode(allocator, mode, cursor, end,
						   &childSize, &cursor, &child);
			if (result != 0)
				goto out;
			if (mode != 2 && mode != 3) {
				result = MarlinAsn1AppendChild(allocator, node, child);
				if (result != 0)
					goto out;
				child = 0;
			}
		}
	} else {
		if (cursor[1] == 0x80) {
			result = 0x22000002;
			goto out;
		}
		result = MarlinAsn1ReadLength(cursor + 1, end, &cursor, &size);
		if (result != 0)
			goto out;
		if (mode != 2 && mode != 3) {
			result = MarlinAsn1CreatePrimitive(allocator, tagClass,
				constructed, tag, size, cursor, mode == 1, &node);
			if (result != 0)
				goto out;
		}
		cursor += size;
	}
	if (mode != 2 && mode != 3) {
		node->encodedData = input;
		node->encodedSize = cursor - input;
	}
	if (consumed != 0)
		*consumed = cursor - input;
	if (next != 0)
		*next = cursor;
	if (output != 0) {
		*output = node;
		node = 0;
	}
out:
	if (node != 0)
		MarlinAsn1Destroy(allocator, node);
	if (child != 0)
		MarlinAsn1Destroy(allocator, child);
	return result;
}

int MarlinAsn1GetContentSpan(void *allocator, const unsigned char *input,
			    unsigned int inputSize, const unsigned char **content,
			    unsigned int *contentSize, unsigned int *encodedSize)
{
	int result;
	const unsigned char *end = input + inputSize;
	unsigned int constructed;

	if (inputSize <= 1) {
		result = 0x22000003;
		goto out;
	}
	constructed = *input & 0x20;
	if (constructed == 0x20) {
		if (input[1] != 0x80) {
			result = MarlinAsn1ReadLength(input + 1, end, content,
						    (int *)contentSize);
			if (result != 0)
				goto out;
			*encodedSize = (*content - input) + *contentSize;
		} else {
			result = MarlinAsn1ParseNode(allocator, 3, input, end,
						   contentSize, content, 0);
			if (result != 0)
				goto out;
			*content = input + 2;
			*encodedSize = *contentSize;
			*contentSize -= 4;
		}
	} else {
		if (input[1] == 0x80) {
			result = 0x22000002;
			goto out;
		}
		result = MarlinAsn1ReadLength(input + 1, end, content,
					    (int *)contentSize);
		if (result != 0)
			goto out;
		*encodedSize = (*content - input) + *contentSize;
	}
	result = 0;
out:
	return result;
}

int MarlinAsn1CreateNode(void *allocator, int tagClass, int constructed,
			 int tag, MarlinAsn1Node **output)
{
	MarlinAsn1Node *node;

	if (output == 0 || tagClass > 3 || tag > 31)
		return 0x22000001;
	node = MarlinAllocateMemory(allocator, sizeof(*node));
	if (node == 0)
		return 2;
	MarlinFillMemory(node, 0, sizeof(*node));
	node->tagClass = tagClass;
	node->constructed = constructed;
	node->tag = tag;
	*output = node;
	return 0;
}

int MarlinAsn1CreatePrimitive(void *allocator, int tagClass, int constructed,
			      int tag, unsigned int size, const void *data,
			      int reference, MarlinAsn1Node **output)
{
	int result = 0;
	MarlinAsn1Node *node = 0;

	if (size != 0 && data == 0) {
		result = 0x22000001;
		goto out;
	}
	result = MarlinAsn1CreateNode(allocator, tagClass, constructed, tag, &node);
	if (result == 0) {
		if (reference != 0) {
			node->value.primitive.size = size;
			node->value.primitive.data = (unsigned char *)data;
			node->flags |= 4;
		} else if (size != 0) {
			node->value.primitive.data = MarlinAllocateMemory(allocator, size);
			if (node->value.primitive.data == 0) {
				result = 2;
				goto out;
			}
			node->value.primitive.size = size;
			MarlinCopyMemory(node->value.primitive.data, data, size);
		} else {
			node->value.primitive.size = 0;
			node->value.primitive.data = 0;
		}
		*output = node;
	}
out:
	if (result != 0) {
		MarlinAsn1Destroy(allocator, node);
		node = 0;
	}
	return result;
}

int MarlinAsn1Walk(void *allocator, MarlinAsn1Node *node,
		   int (*callback)(void *, MarlinAsn1Node *, void *), void *context)
{
	int result = 0;
	int index;

	if (node == 0)
		return 0;
	if (node->constructed != 0) {
		for (index = 0; (int)node->value.sequence.count > index; index++) {
			result = MarlinAsn1Walk(allocator,
				node->value.sequence.children[index], callback, context);
			if (result != 0)
				break;
		}
	}
	if (result == 0)
		return callback(allocator, node, context);
	return result;
}

int MarlinAsn1FreeNode(void *allocator, MarlinAsn1Node *node, void *context)
{
	if ((signed char)node->flags >= 0) {
		if (node->constructed != 0) {
			MarlinFreeMemory(allocator, node->value.sequence.children);
			node->value.sequence.children = 0;
		} else {
			if ((node->flags & 4) == 0)
				MarlinFreeMemory(allocator, node->value.primitive.data);
			node->value.primitive.data = 0;
		}
		MarlinFreeMemory(allocator, node);
	}
	return 0;
}

int MarlinAsn1Parse(void *allocator, const unsigned char *data,
		    unsigned int size, const unsigned char **end,
		    MarlinAsn1Node **output)
{
	return MarlinAsn1ParseNode(allocator, 1, data, data + size, 0, end, output);
}

int MarlinAsn1Destroy(void *allocator, MarlinAsn1Node *node)
{
	return MarlinAsn1Walk(allocator, node, MarlinAsn1FreeNode, 0);
}

int MarlinAsn1InsertChild(void *allocator, MarlinAsn1Node *parent,
			 int index, MarlinAsn1Node *child)
{
	MarlinAsn1Node **children;

	if (parent == 0)
		return 0x22000001;
	if (parent->constructed == 0)
		return 0x22000005;
	if (index < 0)
		return 0x22000001;
	if ((int)parent->value.sequence.count < index)
		index = parent->value.sequence.count;
	children = MarlinReallocateMemory(allocator,
		parent->value.sequence.children,
		(parent->value.sequence.count + 1) * sizeof(*children));
	if (children == 0)
		return 2;
	parent->value.sequence.children = children;
	parent->value.sequence.count++;
	MarlinMoveMemory(children + index + 1, children + index,
		(parent->value.sequence.count - index - 1) * sizeof(*children));
	children[index] = child;
	return 0;
}

int MarlinAsn1AppendChild(void *allocator, MarlinAsn1Node *parent,
			 MarlinAsn1Node *child)
{
	if (parent == 0)
		return 0x22000001;
	if (parent->constructed == 0)
		return 0x22000005;
	return MarlinAsn1InsertChild(allocator, parent,
				   parent->value.sequence.count, child);
}

int MarlinAsn1ReplaceChild(void *allocator, MarlinAsn1Node *parent,
			  int index, MarlinAsn1Node *replacement,
			  MarlinAsn1Node **previous)
{
	int result = 0;
	MarlinAsn1Node *node = 0;

	if (parent == 0 || index < 0 ||
	    (int)parent->value.sequence.count <= index || replacement == 0)
		return 0x22000001;
	if (parent->constructed == 0)
		return 0x22000005;
	node = parent->value.sequence.children[index];
	parent->value.sequence.children[index] = replacement;
	if (previous == 0) {
		MarlinAsn1Destroy(allocator, node);
		node = 0;
	} else {
		*previous = node;
	}
	return result;
}
