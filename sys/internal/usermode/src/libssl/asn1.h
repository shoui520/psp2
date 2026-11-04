/* Names in this private interface are ours; Sony's names are not yet known. */
#ifndef LIBSSL_ASN1_H
#define LIBSSL_ASN1_H

#include <scetypes.h>
#include <stddef.h>
#include "CRYPTOCME/common/property.h"

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

/* Interpreter interface names are ours. */
typedef struct SslAsn1Navigator SslAsn1Navigator;
typedef int (*SslAsn1Operation)(SslAsn1Navigator *navigator, int command,
				int first, int second, int third);
typedef int (*SslAsn1Callback)(SslAsn1Navigator *navigator, void *arg, SceInt32 *registers,
			       int first, int second, int third);

typedef struct SslAsn1TreeMethods {
	SslAsn1Tree *(*Create)(void);
	SslAsn1Tree *(*Init)(SslAsn1Tree *, SslAsn1Element *, SceUInt32, SceUInt32);
	void (*Destroy)(SslAsn1Tree *);
	int (*Encode)(SslAsn1Element *, SceUInt8 *, SceUInt32 *, SceUInt32);
	int (*opaqueOperation)();
	int (*Decode)(SslAsn1Tree *, const SceUInt8 *, SceUInt32, SceUInt32 *);
	int (*opaqueTransform)();
	int (*Measure)(SslAsn1Element *);
	SceUInt32 (*Count)(const SslAsn1Tree *);
	SslAsn1Element *(*At)(const SslAsn1Tree *, SceUInt32);
} SslAsn1TreeMethods;

struct SslAsn1Navigator {
	const SslAsn1TreeMethods *methods;
	const SslAsn1Operation *operations;
	SslAsn1Tree *tree;
	int instructionCount;
	const SceUInt32 *schema;
	int programCounter;
	int savedIndexCount;
	int labelCount;
	SceUInt32 opaqueState;
	SceInt32 stackCount;
	SceInt32 stackCapacity;
	SceInt32 *stack;
	SceInt32 *savedIndices;
	SceInt32 *labels;
	SceInt32 registers[16];
	SslAsn1Callback callback;
	void *callbackArg;
	SceInt32 currentIndex;
	SceUInt32 opaqueTraversal;
	SceInt32 mode;
	SslAsn1Element *currentElement;
	SceUInt32 flags;
};

_Static_assert(sizeof(SslAsn1Element) == 28, "SslAsn1Element");
_Static_assert(sizeof(SslAsn1Tree) == 16, "SslAsn1Tree");
_Static_assert(sizeof(SslAsn1TreeMethods) == 40, "SslAsn1TreeMethods");
_Static_assert(sizeof(SslAsn1Navigator) == 148, "SslAsn1Navigator");
_Static_assert(offsetof(SslAsn1Navigator, savedIndices) == 48, "savedIndices");
_Static_assert(offsetof(SslAsn1Navigator, stackCount) == 36, "stackCount");
_Static_assert(offsetof(SslAsn1Navigator, stackCapacity) == 40, "stackCapacity");
_Static_assert(offsetof(SslAsn1Navigator, stack) == 44, "stack");
_Static_assert(offsetof(SslAsn1Navigator, registers) == 56, "registers");
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
int sslAsn1ResetNavigator(SslAsn1Navigator *navigator);
int sslAsn1InitNavigator(SslAsn1Navigator *navigator,
			 const SslAsn1TreeMethods *methods,
			 const SslAsn1Operation *operations);
void sslAsn1DestroyNavigator(SslAsn1Navigator *navigator);
int sslAsn1DecodeNavigator(SslAsn1Navigator *navigator, const SceUInt8 *data,
			   SceUInt32 size, SceUInt32 *consumed);
int sslAsn1EncodeNavigator(SslAsn1Navigator *navigator, SceUInt8 *data,
			   SceUInt32 *length, SceUInt32 capacity);
int sslAsn1RunSchema(SslAsn1Navigator *navigator, void *arg);
int sslAsn1LoadSchema(SslAsn1Navigator *navigator, const SceUInt32 *schema);
int sslAsn1DecodeSchema(SslAsn1Navigator *navigator, const SceUInt32 *schema,
			void *arg, const SceUInt8 *data, SceUInt32 size,
			SceUInt32 *consumed);
int sslAsn1Configure(SslAsn1Navigator *navigator, int command, int first,
		     int second, int third);
int sslAsn1Label(SslAsn1Navigator *navigator, int command, int first,
		 int second, int third);
int sslAsn1Branch(SslAsn1Navigator *navigator, int command, int first,
		  int second, int third);
int sslAsn1LoadConstant(SslAsn1Navigator *navigator, int command, int first,
			int second, int third);
int sslAsn1UnsupportedOperation(SslAsn1Navigator *navigator, int command,
			       int first, int second, int third);
int sslAsn1TestElement(SslAsn1Navigator *navigator, int command, int first,
		      int second, int third);
int sslAsn1Arithmetic(SslAsn1Navigator *navigator, int command, int first,
		     int second, int third);
int sslAsn1PropertyCallback(SslAsn1Navigator *navigator, void *arg,
			  SceInt32 *value, int command, int type, int id);
int sslAsn1InitDecoder(SslAsn1Navigator *navigator);
int sslAsn1InitEncoder(SslAsn1Navigator *navigator);
int sslAsn1StackOperation(SslAsn1Navigator *navigator, int command, int first,
			  int second, int third);
int sslAsn1EncodeSchema(SslAsn1Navigator *navigator, const SceUInt32 *schema,
			void *arg, SceUInt8 *data, SceUInt32 *length,
			SceUInt32 capacity);
int sslAsn1EncodeProperties(const SceUInt32 *schema, SslPropertyList *list,
			    SceUInt8 *data, SceUInt32 *length, SceUInt32 capacity);
int sslAsn1DecodeProperties(const SceUInt32 *schema, SslPropertyList *list,
			    const SceUInt8 *data, SceUInt32 size,
			    SceUInt32 *consumed);

#endif
