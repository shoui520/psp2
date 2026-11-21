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

/* a tree: its children depth first, then the node */
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
