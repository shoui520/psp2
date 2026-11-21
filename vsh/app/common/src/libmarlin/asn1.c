/*
 * DER (ASN.1): the nodes of a tree, their encoding and decoding. The file's name is ours.
 */
#include "mln_internal.h"

/* a node, and its data where the node owns it */
int MlnImpl_810353EC(int heap, MlnAsn1Node *node)
{
	if (node->flags & MLN_ASN1_STATIC)
		return 0;
	if (node->constructed != 0 || !(node->flags & MLN_ASN1_DATA_BORROWED))
		MlnFree(heap, node->data);
	node->data = NULL;
	MlnFree(heap, node);
	return 0;
}

/*
 * A UTCTime or GeneralizedTime, YYMMDDHHMMSS or YYYYMMDDHHMMSS with an optional fraction
 * of three digits, then Z or a zone of four. A two-digit year below the pivot is 20YY.
 */
int MlnImpl_81035440(const MlnAsn1Node *node, int pivot, MlnAsn1Time *time)
{
	const unsigned char *p = node->data;
	const unsigned char *end = p + node->length;
	const unsigned char *c;
	int utc = node->tag == MLN_ASN1_TAG_UTC_TIME;
	int position = utc ? 2 : 0;	/* in the GeneralizedTime form */
	int last = node->length + position;
	int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
	int millisecond = 0, offset;
	int digit;

	if (p >= end)
		return 1;
	for (;;) {
		c = p++;
		if (*c == 'Z' || *c == '+' || *c == '-')
			break;
		digit = *c - '0';
		if ((unsigned int)digit > 9 && *c != '.')
			return 1;
		if (position < 4) {
			year = year * 10 + digit;
		} else if (position < 6) {
			month = month * 10 + digit;
		} else if (position < 8) {
			day = day * 10 + digit;
		} else if (position < 10) {
			hour = hour * 10 + digit;
		} else if (position < 12) {
			minute = minute * 10 + digit;
		} else if (position < 14) {
			second = second * 10 + digit;
		} else if (position == 14) {
			if (*c != '.' && !utc)
				return 0x22000006;
		} else if (position == 15) {
			millisecond += digit * 100;
		} else if (position == 16) {
			millisecond += digit * 10;
		} else if (position == 17) {
			millisecond += digit;
		}
		if (++position == last)
			return 1;
	}
	if (*c == '+' || *c == '-') {
		if (end <= c + 4)
			return 0x22000006;
		offset = 60 * ((c[1] - '0') * 600 + (c[2] - '0') * 60 + (c[3] - '0') * 10 +
			       (c[4] - '0'));
		if (*c == '+')
			offset = -offset;
	} else {
		offset = 0;
	}
	if (utc) {
		if (pivot > year)
			year += 2000;
		else
			year += 1900;
	}
	MlnMemset(time, 0, sizeof(*time));
	time->year = year;
	time->month = month;
	time->day = day;
	time->hour = hour;
	time->minute = minute;
	time->millisecond = millisecond;
	time->offset = offset;
	time->second = second;
	return 0;
}

/*
 * The header of the encoding at der: its identifier and length, definite or indefinite
 * (whose end is found by skipping its elements). Gives where the encoding ends, and fills a
 * node that points into it. end, when given, bounds the encoding.
 */
__attribute__((noinline))
int MlnImpl_810356A8(int heap, const unsigned char *der, const unsigned char *end,
		     const unsigned char **next, MlnAsn1Node *node)
{
	unsigned char identifier = der[0];
	unsigned int first = der[1];
	const unsigned char *data;
	const unsigned char *after;
	unsigned int length;
	int count, i;
	unsigned char flags;
	int ret;

	length = first & 0x7f;
	if (first == 0x80) {
		data = der + 2;
		after = data;
		while (after[0] != 0 || after[1] != 0) {
			if (end != NULL && end < after + 2)
				return 0x22000004;
			ret = MlnImpl_810356A8(heap, after, end, &after, NULL);
			if (ret != 0)
				return ret;
		}
		length = after - data;
		after += 2;
		flags = MLN_ASN1_DATA_BORROWED | MLN_ASN1_CONTENTS | MLN_ASN1_INDEFINITE;
	} else if (first & 0x80) {
		count = length;
		if (end != NULL && end < der + count + 2)
			return 0x22000004;
		if (count < 1 || count > 4)
			return 0x22000006;
		length = der[2];
		data = der + 3;
		for (i = 1; i < count; i++)
			length = (length << 8) + *data++;
		after = data + length;
		flags = MLN_ASN1_DATA_BORROWED | MLN_ASN1_CONTENTS;
	} else {
		data = der + 2;
		after = data + length;
		flags = MLN_ASN1_DATA_BORROWED | MLN_ASN1_CONTENTS;
	}
	if (end != NULL && end < after)
		return 0x22000004;
	if (next != NULL)
		*next = after;
	if (node == NULL)
		return 0;
	MlnMemset(node, 0, sizeof(*node));
	node->tagClass = identifier & 0xc0;
	node->constructed = identifier & 0x20;
	node->tag = identifier & 0x1f;
	node->flags = flags;
	node->length = length;
	node->data = (unsigned char *)data;
	node->encoding = der;
	node->encodingLength = after - der;
	return 0;
}

/*
 * Walks a tree depth first, checking each node's class and tag; what the release build
 * did with each node (depth is the indent) is compiled out.
 */
int MlnImpl_81035884(const MlnAsn1Node *node, int depth)
{
	int i;

	if (node == NULL || node->tagClass > 3 || node->tag > 30)
		return 0x22000001;
	if (!node->constructed)
		return 0;
	for (i = 0; i < (int)node->length; i++)
		if (MlnImpl_81035884(node->children[i], depth + 1) != 0)
			break;
	return 0;
}

/* an empty node */
static inline int MlnAsn1NewNode(int heap, int tagClass, unsigned char constructed, int tag,
				 MlnAsn1Node **out)
{
	MlnAsn1Node *node;

	if (tagClass > 3 || tag > 31)
		return 0x22000001;
	node = MlnAllocate(heap, sizeof(*node));
	if (node == NULL)
		return 2;
	MlnMemset(node, 0, sizeof(*node));
	node->tagClass = tagClass;
	node->constructed = constructed;
	node->tag = tag;
	*out = node;
	return 0;
}

/* a tree: its children depth first, then the node */
__attribute__((noinline))
int MlnImpl_81035ABC(int heap, MlnAsn1Node *node)
{
	int i, ret;

	if (node == NULL)
		return 0;
	if (node->constructed) {
		for (i = 0; i < (int)node->length; i++) {
			ret = MlnImpl_81035ABC(heap, node->children[i]);
			if (ret != 0)
				return ret;
		}
	}
	if (!(node->flags & MLN_ASN1_STATIC)) {
		if (node->constructed != 0)
			MlnFree(heap, node->data);
		else if (!(node->flags & MLN_ASN1_DATA_BORROWED))
			MlnFree(heap, node->data);
		node->data = NULL;
		MlnFree(heap, node);
	}
	return 0;
}

/*
 * A node: its data copied, or borrowed from the caller. A constructed node gets its children
 * later.
 */
__attribute__((noinline))
int MlnImpl_810360E8(int heap, int tagClass, unsigned char constructed, int tag,
		     unsigned int length, const void *data, int borrowed, MlnAsn1Node **out)
{
	MlnAsn1Node *node = NULL;
	int copy = length != 0;
	int ret;

	if (data == NULL && copy) {
		ret = 0x22000001;
		goto fail;
	}
	if (tagClass > 3 || tag > 31) {
		ret = 0x22000001;
		goto fail;
	}
	node = MlnAllocate(heap, sizeof(*node));
	if (node == NULL) {
		ret = 2;
		goto fail;
	}
	MlnMemset(node, 0, sizeof(*node));
	node->tagClass = tagClass;
	node->constructed = constructed;
	node->tag = tag;
	if (borrowed) {
		node->length = length;
		node->data = (unsigned char *)data;
		node->flags |= MLN_ASN1_DATA_BORROWED;
	} else if (!copy) {
		node->length = 0;
		node->data = NULL;
	} else {
		node->data = MlnAllocate(heap, length);
		if (node->data == NULL) {
			ret = 2;
			goto fail;
		}
		node->length = length;
		MlnMemcpy(node->data, data, length);
	}
	*out = node;
	return 0;
fail:
	MlnImpl_81035ABC(heap, node);
	return ret;
}

/* the octets that encode a definite length */
static inline unsigned int MlnAsn1LengthSize(unsigned int length)
{
	if ((length >> 7) == 0)
		return 1;
	if ((length >> 8) == 0)
		return 2;
	if ((length >> 16) == 0)
		return 3;
	if ((length >> 24) == 0)
		return 4;
	return 5;
}

/* the size of a node's encoding: identifier, length and contents */
__attribute__((noinline))
int MlnImpl_81036204(const MlnAsn1Node *node, unsigned int *size)
{
	unsigned int childSize = 0;
	unsigned int contents, i;
	int ret;

	if (size == NULL || node == NULL)
		return 0x22000001;
	if (node->flags & MLN_ASN1_ENCODED) {
		*size = node->encodingLength;
		return 0;
	}
	if (!node->constructed) {
		if (node->flags & MLN_ASN1_INDEFINITE)
			*size = node->length + 4;
		else
			*size = node->length + 1 + MlnAsn1LengthSize(node->length);
		return 0;
	}
	contents = 0;
	for (i = 0; i < node->length; i++) {
		ret = MlnImpl_81036204(node->children[i], &childSize);
		if (ret != 0)
			return ret;
		contents += childSize;
	}
	if (node->flags & MLN_ASN1_INDEFINITE)
		*size = contents + 4;
	else
		*size = contents + 1 + MlnAsn1LengthSize(contents);
	return 0;
}

/* a definite length after the identifier at p, short or long form */
static inline void MlnAsn1PutLength(unsigned char *p, unsigned int length)
{
	unsigned int count = MlnAsn1LengthSize(length) - 1;
	unsigned char *q;

	for (q = p + 1 + count; q != p; q--) {
		*q = length;
		length >>= 8;
	}
	if (count != 0)
		p[1] = 0x80 | count;
}

/* a definite length and the node's data after an identifier at p */
static inline void MlnAsn1PutData(const MlnAsn1Node *node, unsigned char *p, unsigned char **pos)
{
	unsigned int size;

	MlnAsn1PutLength(p, node->length);
	size = MlnAsn1LengthSize(node->length);
	MlnMemcpy(p + 1 + size, node->data, node->length);
	*pos = p + node->length + 1 + size;
}

/*
 * Encodes a tree at *pos and moves *pos past it. A constructed node's children go after a
 * two-octet header, moved up once their length is known.
 */
__attribute__((noinline))
int MlnImpl_81036364(const MlnAsn1Node *node, unsigned char **pos)
{
	unsigned char *p = *pos;
	unsigned char *cursor;
	unsigned int contents, size, i;
	int ret;

	if (node->flags & MLN_ASN1_ENCODED) {
		MlnMemcpy(p, node->encoding, node->encodingLength);
		*pos = p + node->encodingLength;
		return 0;
	}
	if (node->flags & MLN_ASN1_CONTENTS) {
		p[0] = node->tagClass << 6 | (node->constructed ? 0x20 : 0) | (node->tag & 0x1f);
		if (node->flags & MLN_ASN1_INDEFINITE) {
			if (!node->constructed)
				return 0x22000002;
			p[1] = 0x80;
			MlnMemcpy(p + 2, node->data, node->length);
			p[node->length + 2] = 0;
			p[node->length + 3] = 0;
			*pos = p + node->length + 4;
			return 0;
		}
		MlnAsn1PutData(node, p, pos);
		return 0;
	}
	if (!node->constructed) {
		p[0] = node->tagClass << 6 | (node->tag & 0x1f);
		if (node->flags & MLN_ASN1_INDEFINITE)
			return 0x22000002;
		MlnAsn1PutData(node, p, pos);
		return 0;
	}
	{
		cursor = p + 2;
		for (i = 0; i < node->length; i++) {
			if (node->children[i] == NULL)
				return 0x22000001;
			ret = MlnImpl_81036364(node->children[i], &cursor);
			if (ret != 0)
				return ret;
		}
		p = *pos;
		contents = cursor - p - 2;
		p[0] = node->tagClass << 6 | 0x20 | (node->tag & 0x1f);
		if (node->flags & MLN_ASN1_INDEFINITE) {
			p[1] = 0x80;
			p[contents + 2] = 0;
			p[contents + 3] = 0;
			*pos = p + contents + 4;
			return 0;
		}
		size = MlnAsn1LengthSize(contents);
		MlnMemmove(p + 1 + size, p + 2, contents);
		MlnAsn1PutLength(p, contents);
		*pos = p + contents + 1 + size;
		return 0;
	}
}

/* the size a tree encodes to */
int MlnImpl_810368BC(int heap, const MlnAsn1Node *node, unsigned int *size)
{
	return MlnImpl_81036204(node, size);
}

static inline int MlnAsn1Encode(const MlnAsn1Node *node, unsigned char **pos)
{
	if (node == NULL)
		return 0x22000001;
	return MlnImpl_81036364(node, pos);
}

/* a tree encoded into a new buffer */
int MlnImpl_810368D0(int heap, MlnAsn1Node *node, unsigned char **data, unsigned int *size)
{
	unsigned char *cursor = NULL;
	int ret;

	ret = MlnImpl_81036204(node, size);
	if (ret != 0)
		goto fail;
	cursor = MlnAllocate(heap, *size);
	if (cursor == NULL) {
		ret = 2;
		goto fail;
	}
	*data = cursor;
	ret = MlnAsn1Encode(node, &cursor);
	if (ret == 0)
		return 0;
fail:
	MlnFree(heap, cursor);
	return ret;
}

/* a tree encoded into the caller's buffer, which must hold *size */
int MlnImpl_8103696C(int heap, const MlnAsn1Node *node, unsigned char *buffer, unsigned int *size)
{
	int ret;

	ret = MlnImpl_81036204(node, size);
	if (ret != 0)
		return ret;
	return MlnAsn1Encode(node, &buffer);
}

/* a constructed node's length form: definite, or indefinite */
int MlnImpl_810369B8(int heap, MlnAsn1Node *node, int definite)
{
	if (node == NULL)
		return 0x22000001;
	if (!node->constructed)
		return 0x22000005;
	if (definite)
		node->flags &= ~MLN_ASN1_INDEFINITE;
	else
		node->flags |= MLN_ASN1_INDEFINITE;
	return 0;
}

/* whether a node's length is definite */
int MlnImpl_81036A04(int heap, const MlnAsn1Node *node)
{
	return !(node->flags & MLN_ASN1_INDEFINITE);
}

/* a child appended to a constructed node */
static inline int MlnAsn1Append(int heap, MlnAsn1Node *parent, MlnAsn1Node *child)
{
	if (parent == NULL)
		return 0x22000001;
	if (!parent->constructed)
		return 0x22000005;
	return MlnImpl_81036FE4(heap, parent, parent->length, child);
}

/* a copy of a tree; a node that holds its encoding or borrows its data shares it */
__attribute__((noinline))
int MlnImpl_81036A14(int heap, const MlnAsn1Node *node, MlnAsn1Node **out)
{
	MlnAsn1Node *copy = NULL;
	MlnAsn1Node *child = NULL;
	int i, ret;

	if (node == NULL)
		return 0x22000001;
	if (node->flags & MLN_ASN1_ENCODED) {
		ret = MlnAsn1NewNode(heap, node->tagClass, node->constructed, node->tag, &copy);
		if (ret != 0)
			goto fail;
	} else if (node->flags & MLN_ASN1_CONTENTS) {
		ret = MlnImpl_810360E8(heap, node->tagClass, node->constructed, node->tag,
				       node->length, node->data,
				       node->flags & MLN_ASN1_ENCODED, &copy);
		if (ret != 0)
			goto fail;
	} else if (!node->constructed) {
		ret = MlnImpl_810360E8(heap, node->tagClass, node->constructed, node->tag,
				       node->length, node->data,
				       node->flags & MLN_ASN1_DATA_BORROWED, &copy);
		if (ret != 0)
			goto fail;
	} else {
		ret = MlnAsn1NewNode(heap, node->tagClass, node->constructed, node->tag, &copy);
		if (ret != 0)
			goto fail;
		for (i = 0; i < (int)node->length; i++) {
			ret = MlnImpl_81036A14(heap, node->children[i], &child);
			if (ret != 0)
				goto fail;
			ret = MlnAsn1Append(heap, copy, child);
			if (ret != 0)
				goto fail;
			child = NULL;
		}
	}
	copy->flags = node->flags;
	copy->encoding = node->encoding;
	copy->encodingLength = node->encodingLength;
	*out = copy;
	return 0;
fail:
	MlnImpl_81035ABC(heap, copy);
	copy = NULL;
	MlnImpl_81035ABC(heap, child);
	return ret;
}

/* a node standing for an encoding of the caller's, written as it is */
int MlnImpl_81036CA8(int heap, unsigned int size, const unsigned char *encoding,
		     MlnAsn1Node **out)
{
	MlnAsn1Node *node;

	if (out == NULL)
		return 0x22000001;
	node = MlnAllocate(heap, sizeof(*node));
	if (node == NULL)
		return 2;
	MlnMemset(node, 0, sizeof(*node));
	node->tagClass = 3;
	node->constructed = 0;
	node->tag = 5;
	*out = node;
	(*out)->encodingLength = size;
	(*out)->encoding = encoding;
	(*out)->flags |= MLN_ASN1_ENCODED;
	return 0;
}

/* a node whose contents are given encoded, a constructed node's too */
int MlnImpl_81036D28(int heap, int tagClass, unsigned char constructed, int tag,
		     unsigned int length, const void *contents, MlnAsn1Node **out)
{
	MlnAsn1Node *node = NULL;
	int copy = length != 0;
	int ret;

	if (contents == NULL && copy) {
		ret = 0x22000001;
		goto fail;
	}
	if (tagClass > 3 || tag > 31) {
		ret = 0x22000001;
		goto fail;
	}
	node = MlnAllocate(heap, sizeof(*node));
	if (node == NULL) {
		ret = 2;
		goto fail;
	}
	MlnMemset(node, 0, sizeof(*node));
	node->tagClass = tagClass;
	node->constructed = constructed;
	node->tag = tag;
	if (!copy) {
		node->length = 0;
		node->data = NULL;
	} else {
		node->data = MlnAllocate(heap, length);
		if (node->data == NULL) {
			ret = 2;
			goto fail;
		}
		node->length = length;
		MlnMemcpy(node->data, contents, length);
	}
	*out = node;
	node->flags |= MLN_ASN1_CONTENTS;
	return 0;
fail:
	MlnImpl_81035ABC(heap, node);
	return ret;
}

/* an empty constructed node */
int MlnImpl_81036E34(int heap, int tagClass, int tag, MlnAsn1Node **out)
{
	MlnAsn1Node *node;

	if (out == NULL || tagClass > 3 || tag > 31)
		return 0x22000001;
	node = MlnAllocate(heap, sizeof(*node));
	if (node == NULL)
		return 2;
	MlnMemset(node, 0, sizeof(*node));
	node->tagClass = tagClass;
	node->constructed = 1;
	node->tag = tag;
	*out = node;
	return 0;
}

/* a primitive node whose contents are given encoded */
int MlnImpl_81036EBC(int heap, int tagClass, int tag, unsigned int length,
		     const void *contents, MlnAsn1Node **out)
{
	return MlnImpl_81036D28(heap, tagClass, 0, tag, length, contents, out);
}

/* a tree, as the library's callers free it */
int MlnImpl_81036FC8(int heap, MlnAsn1Node *node)
{
	return MlnImpl_81035ABC(heap, node);
}

/* an encoding from 0x810368D0 */
int MlnImpl_81036FD4(int heap, unsigned char *data)
{
	MlnFree(heap, data);
	return 0;
}

/* a child inserted at index, or appended past the end */
int MlnImpl_81036FE4(int heap, MlnAsn1Node *parent, int index, MlnAsn1Node *child)
{
	MlnAsn1Node **children;
	int count;

	if (parent == NULL)
		return 0x22000001;
	if (!parent->constructed)
		return 0x22000005;
	if (index < 0)
		return 0x22000001;
	count = parent->length;
	children = MlnReallocate(heap, parent->children, (count + 1) * sizeof(*children));
	if (children == NULL)
		return 2;
	parent->children = children;
	if (index >= count)
		index = count;
	parent->length++;
	MlnMemmove(&children[index + 1], &children[index],
		   (parent->length - index - 1) * sizeof(*children));
	children[index] = child;
	return 0;
}

/* a definite length after the identifier at der: the length and where the contents start */
static inline int MlnAsn1GetLength(const unsigned char *der, const unsigned char *end,
				   int *length, const unsigned char **contents)
{
	unsigned int first = der[1];
	unsigned int count, i;
	int value;

	if (!(first & 0x80)) {
		*length = first;
		*contents = der + 2;
		return 0;
	}
	count = first & 0x7f;
	if (count > 4 || count > (unsigned int)(end - (der + 1)) - 1)
		return 0x22000003;
	value = 0;
	for (i = 0; i < count; i++)
		value = (value << 8) + der[2 + i];
	if (value < 0)
		return 0x22000003;
	*length = value;
	*contents = der + 2 + count;
	return 0;
}

/*
 * Decodes the encoding at der into a tree. mode 0 copies the data, 1 borrows it from der,
 * 2 and 3 only check the encoding and build nothing. end, when given, bounds it.
 */
__attribute__((noinline))
int MlnImpl_81037084(int heap, int mode, const unsigned char *der, const unsigned char *end,
		     unsigned int *size, const unsigned char **next, MlnAsn1Node **out)
{
	MlnAsn1Node *node = NULL;
	MlnAsn1Node *child = NULL;
	const unsigned char *cursor;
	unsigned int childSize;
	unsigned char identifier;
	int build = (unsigned int)(mode - 2) > 1;
	int length, ret;

	if ((unsigned int)(end - der) <= 1) {
		ret = 0x22000003;
		goto done;
	}
	identifier = der[0];
	if (identifier & 0x20) {
		if (build) {
			ret = MlnAsn1NewNode(heap, identifier >> 6, identifier & 0x20,
					     identifier & 0x1f, &node);
			if (ret != 0)
				goto done;
		}
		if (der[1] == 0x80) {
			if (build)
				node->flags |= MLN_ASN1_INDEFINITE;
			length = -1;
			cursor = der + 2;
		} else {
			ret = MlnAsn1GetLength(der, end, &length, &cursor);
			if (ret != 0)
				goto done;
			if (end != NULL && end < cursor + length) {
				ret = 0x22000004;
				goto done;
			}
			end = cursor + length;
		}
		for (;;) {
			if (end != NULL && cursor >= end)
				break;
			if (length < 0 && cursor[0] == 0 && cursor[1] == 0) {
				cursor += 2;
				break;
			}
			ret = MlnImpl_81037084(heap, mode, cursor, end, &childSize, &cursor, &child);
			if (ret != 0)
				goto done;
			if (build) {
				ret = MlnAsn1Append(heap, node, child);
				if (ret != 0)
					goto done;
				child = NULL;
			}
		}
	} else {
		if (der[1] == 0x80) {
			ret = 0x22000002;
			goto done;
		}
		ret = MlnAsn1GetLength(der, end, &length, &cursor);
		if (ret != 0)
			goto done;
		if (end != NULL && end < cursor + length) {
			ret = 0x22000004;
			goto done;
		}
		if (build) {
			ret = MlnImpl_810360E8(heap, identifier >> 6, 0, identifier & 0x1f, length,
					       cursor, mode == 1, &node);
			if (ret != 0)
				goto done;
		}
		cursor += length;
	}
	if (build) {
		node->encoding = der;
		node->encodingLength = cursor - der;
	}
	if (size != NULL)
		*size = cursor - der;
	if (next != NULL)
		*next = cursor;
	if (out != NULL) {
		*out = node;
		node = NULL;
	}
	ret = 0;
done:
	if (node != NULL)
		MlnImpl_81035ABC(heap, node);
	if (child != NULL)
		MlnImpl_81035ABC(heap, child);
	return ret;
}

/* a definite length's header: where the contents start, their length and the whole size */
static inline int MlnAsn1GetHeader(const unsigned char *der, const unsigned char *end,
				   const unsigned char **contents, int *length,
				   unsigned int *encodingSize)
{
	int ret;

	ret = MlnAsn1GetLength(der, end, length, contents);
	if (ret != 0)
		return ret;
	if (end != NULL && end < *contents + *length)
		return 0x22000004;
	*encodingSize = *contents + *length - der;
	return 0;
}

/*
 * The header of the encoding at der: where its contents start, their length, and the size
 * of the whole encoding (an indefinite one is checked through to its end).
 */
int MlnImpl_81037660(int heap, const unsigned char *der, unsigned int size,
		     const unsigned char **contents, int *length, unsigned int *encodingSize)
{
	const unsigned char *end;
	int ret;

	if (contents == NULL || der == NULL)
		return 0x22000001;
	if (encodingSize == NULL || length == NULL)
		return 0x22000001;
	if (size <= 1)
		return 0x22000003;
	end = der + size;
	if (!(der[0] & 0x20)) {
		if (der[1] == 0x80)
			return 0x22000002;
		return MlnAsn1GetHeader(der, end, contents, length, encodingSize);
	}
	if (der[1] != 0x80)
		return MlnAsn1GetHeader(der, end, contents, length, encodingSize);
	ret = MlnImpl_81037084(heap, 3, der, end, (unsigned int *)length, contents, NULL);
	if (ret != 0)
		return ret;
	*contents = der + 2;
	*encodingSize = *length;
	*length -= 4;
	return 0;
}

/* checks the encoding of size bytes at der */
int MlnImpl_810378B0(int heap, const unsigned char *der, unsigned int size,
		     const unsigned char **next)
{
	return MlnImpl_81037084(heap, 2, der, der + size, NULL, next, NULL);
}

/* decodes the encoding of size bytes at der into a tree that borrows its data */
int MlnImpl_810378E4(int heap, const unsigned char *der, unsigned int size,
		     const unsigned char **next, MlnAsn1Node **out)
{
	return MlnImpl_81037084(heap, 1, der, der + size, NULL, next, out);
}
