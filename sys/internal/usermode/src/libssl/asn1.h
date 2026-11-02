/* Names in this private interface are ours; Sony's names are not yet known. */
#ifndef LIBSSL_ASN1_H
#define LIBSSL_ASN1_H

#include <scetypes.h>
#include <stddef.h>

typedef struct SslAsn1Element SslAsn1Element;

struct SslAsn1Element {
	SceUInt32 length;
	const SceUInt8 *data;
	SceUInt32 tag;
	SceUInt8 encoding;
	SceUInt8 headerLength;
	SceUInt8 flags;
	SceUInt8 prefix;
	SslAsn1Element *parent;
	SslAsn1Element *next;
	SslAsn1Element *child;
};

typedef struct SslAsn1Tree {
	SceUInt32 count;
	SceUInt32 capacity;
	SslAsn1Element *elements;
	SceUInt32 flags;
} SslAsn1Tree;

/* This view contains the fields used by the tree navigation method. */
typedef struct SslAsn1Navigator SslAsn1Navigator;
typedef int (*SslAsn1Callback)(SslAsn1Navigator *navigator, void *arg, SceInt32 *registers,
			       int first, int second, int third);

struct SslAsn1Navigator {
	void *methods;
	void *schema;
	SslAsn1Tree *tree;
	SceUInt8 reserved[36];
	SceInt32 *savedIndices;
	SceUInt8 reservedBeforeRegisters[4];
	SceInt32 registers[16];
	SslAsn1Callback callback;
	void *callbackArg;
	SceInt32 currentIndex;
	SceUInt8 reservedBeforeMode[4];
	SceInt32 mode;
	SslAsn1Element *currentElement;
};

_Static_assert(sizeof(SslAsn1Element) == 28, "SslAsn1Element");
_Static_assert(sizeof(SslAsn1Tree) == 16, "SslAsn1Tree");
_Static_assert(offsetof(SslAsn1Navigator, currentIndex) == 128, "currentIndex");
_Static_assert(offsetof(SslAsn1Navigator, mode) == 136, "mode");

int sslAsn1DecodeHeader(SslAsn1Element *element, const SceUInt8 *data, SceUInt32 size);
int sslAsn1Reserve(SslAsn1Tree *tree, SceUInt32 capacity);
int sslAsn1AppendElement(SslAsn1Tree *tree, SceUInt32 *index);
int sslAsn1DecodeTree(SslAsn1Tree *tree, const SceUInt8 *data, SceUInt32 size,
		      SceUInt32 *consumed);
SslAsn1Element *sslAsn1SetData(SslAsn1Element *element, const SceUInt8 *data, SceUInt32 length);
SslAsn1Element *sslAsn1SetTag(SslAsn1Element *element, int tagClass, SceUInt32 tag, int encoding);
SslAsn1Element *sslAsn1SetValue(SslAsn1Element *element, int tagClass, SceUInt32 tag,
			       const SceUInt8 *data, SceUInt32 length, int encoding, int flags);
int sslAsn1SetInteger(SslAsn1Element *element, SceUInt32 value, SceUInt8 *buffer);
int sslAsn1CheckTag(const SslAsn1Element *element, SceUInt32 tag);
int sslAsn1GetInteger(const SslAsn1Element *element, int *value);
int sslAsn1HeaderSize(const SslAsn1Element *element);
int sslAsn1MeasureTree(SslAsn1Element *element);
SslAsn1Tree *sslAsn1InitTree(SslAsn1Tree *tree, SslAsn1Element *elements,
			     SceUInt32 count, SceUInt32 capacity);
SslAsn1Element *sslAsn1InsertNext(SslAsn1Element *element, SslAsn1Element *next);
int sslAsn1PrependChild(SslAsn1Element *parent, SslAsn1Element *child);
void sslAsn1DestroyTree(SslAsn1Tree *tree);
SslAsn1Tree *sslAsn1ClearTree(SslAsn1Tree *tree);
SslAsn1Tree *sslAsn1CreateTree(void);
SceUInt32 sslAsn1ElementCount(const SslAsn1Tree *tree);
SslAsn1Element *sslAsn1ElementAt(const SslAsn1Tree *tree, SceUInt32 index);
SslAsn1Element *sslAsn1SetPrefix(SslAsn1Element *element, int prefix);
SslAsn1Element *sslAsn1GetData(SslAsn1Element *element, const SceUInt8 **data,
			       SceUInt32 *length);
int sslAsn1EncodeHeader(const SslAsn1Element *element, SceUInt8 *data);
int sslAsn1EncodeTree(SslAsn1Element *element, SceUInt8 *data, SceUInt32 *written,
		      SceUInt32 size);
int sslAsn1Navigate(SslAsn1Navigator *navigator, int command, int destination,
		    int count, int group);
int sslAsn1BuildElement(SslAsn1Navigator *navigator, int command, int first,
		      int second, int third);
int sslAsn1InvokeCallback(SslAsn1Navigator *navigator, int skipFirst, int first,
			 int second, int third);
int sslAsn1InspectElement(SslAsn1Navigator *navigator, int command, int first,
			  int second, int third);

#endif
