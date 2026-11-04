/* Private descriptive names and this filename are ours, not recovered Sony names.
 * The original keeps these interpreter methods separate from the low-level codec.
 */
#include "asn1.h"
#include "memory.h"

static SceUInt32 sslAsn1True = 0xff;
static SceUInt32 sslAsn1False;

static const SslAsn1Operation sslAsn1DecodeOperations[16] = {
	sslAsn1Configure, sslAsn1Navigate, sslAsn1Label, sslAsn1UnsupportedOperation,
	sslAsn1InvokeCallback, sslAsn1InspectElement, sslAsn1TestElement, sslAsn1Branch,
	sslAsn1Arithmetic, sslAsn1LoadConstant,
	sslAsn1UnsupportedOperation, sslAsn1UnsupportedOperation,
	sslAsn1UnsupportedOperation, sslAsn1UnsupportedOperation,
	sslAsn1UnsupportedOperation, sslAsn1UnsupportedOperation
};

/* The shared rejection entry ignores every argument. */
static const SslAsn1TreeMethods sslAsn1DecodeMethods = {
	sslAsn1CreateTree, sslAsn1InitTree, sslAsn1DestroyTree,
	(int (*)(SslAsn1Element *, SceUInt8 *, SceUInt32 *, SceUInt32))
		sslAsn1UnsupportedOperation,
	sslAsn1UnsupportedOperation, sslAsn1DecodeTree, sslAsn1UnsupportedOperation,
	sslAsn1MeasureTree, sslAsn1ElementCount, sslAsn1ElementAt
};

static const SslAsn1Operation sslAsn1EncodeOperations[16] = {
	sslAsn1Configure, sslAsn1Navigate, sslAsn1Label, sslAsn1BuildElement,
	sslAsn1InvokeCallback, sslAsn1InspectElement, sslAsn1TestElement, sslAsn1Branch,
	sslAsn1Arithmetic, sslAsn1LoadConstant, sslAsn1StackOperation,
	sslAsn1UnsupportedOperation, sslAsn1UnsupportedOperation,
	sslAsn1UnsupportedOperation, sslAsn1UnsupportedOperation,
	sslAsn1UnsupportedOperation
};

static const SslAsn1TreeMethods sslAsn1EncodeMethods = {
	sslAsn1CreateTree, sslAsn1InitTree, sslAsn1DestroyTree, sslAsn1EncodeTree,
	sslAsn1UnsupportedOperation, sslAsn1DecodeTree, sslAsn1UnsupportedOperation,
	sslAsn1MeasureTree, sslAsn1ElementCount, sslAsn1ElementAt
};

int sslAsn1ResetNavigator(SslAsn1Navigator *navigator)
{
	int index;

	if (navigator->tree != NULL)
		navigator->methods->Destroy(navigator->tree);
	navigator->tree = navigator->methods->Create();
	navigator->programCounter = 0;
	navigator->stackCount = 0;
	for (index = 0; index < 16; index++)
		navigator->registers[index] = 0;
	return 0;
}

int sslAsn1InitNavigator(SslAsn1Navigator *navigator,
			 const SslAsn1TreeMethods *methods,
			 const SslAsn1Operation *operations)
{
	sslFillMemory(navigator, 0, sizeof(*navigator));
	navigator->operations = operations;
	navigator->methods = methods;
	sslAsn1ResetNavigator(navigator);
	return navigator->tree == NULL ? 10005 : 0;
}

void sslAsn1DestroyNavigator(SslAsn1Navigator *navigator)
{
	if (navigator->tree != NULL)
		navigator->methods->Destroy(navigator->tree);
	if (navigator->savedIndices != NULL)
		sslFree(navigator->savedIndices);
	if (navigator->labels != NULL)
		sslFree(navigator->labels);
	if (navigator->stack != NULL)
		sslFree(navigator->stack);
	if (navigator->flags & 1)
		sslFree(navigator);
}

int sslAsn1EncodeNavigator(SslAsn1Navigator *navigator, SceUInt8 *data,
			   SceUInt32 *length, SceUInt32 capacity)
{
	SslAsn1Element *element;
	SceUInt32 measured;

	element = navigator->methods->At(navigator->tree, 0);
	measured = navigator->methods->Measure(element);
	if (length != NULL)
		*length = measured;
	if (data == NULL)
		return 0;
	element = navigator->methods->At(navigator->tree, 0);
	return navigator->methods->Encode(element, data, length, capacity);
}

int sslAsn1DecodeNavigator(SslAsn1Navigator *navigator, const SceUInt8 *data,
			   SceUInt32 size, SceUInt32 *consumed)
{
	SceUInt32 count = navigator->methods->Count(navigator->tree);
	int status;

	status = navigator->methods->Decode(navigator->tree, data, size, consumed);
	if (status == 0 && count != navigator->methods->Count(navigator->tree))
		navigator->currentIndex = count;
	return status;
}

int sslAsn1RunSchema(SslAsn1Navigator *navigator, void *arg)
{
	const SceUInt32 *schema = navigator->schema;
	int tracing = navigator->flags & 2;
	int position = 0, previous, status = 0;
	SceUInt32 instruction;

	navigator->programCounter = 0;
	if (tracing && navigator->callback == NULL)
		tracing = 0;
	navigator->callbackArg = arg;
	for (;;) {
		previous = position;
		if (position > navigator->instructionCount)
			return 10001;
		if (tracing) {
			navigator->callback(navigator, navigator->callbackArg,
					    navigator->registers, -1, 0, 0);
			position = navigator->programCounter;
		}
		navigator->programCounter = (int)((SceUInt32)position + 1u);
		instruction = schema[position];
		if (instruction == 0)
			return status;
		status = navigator->operations[(instruction >> 4) & 15](
			navigator, instruction & 15, (instruction >> 8) & 255,
			(instruction >> 16) & 255, instruction >> 24);
		position = navigator->programCounter;
		if (tracing) {
			navigator->programCounter = previous;
			navigator->callback(navigator, navigator->callbackArg,
					    navigator->registers, -1, 1, status);
			navigator->programCounter = position;
		}
		if ((status > 0 && status != 10016) || (instruction & 255) == 1)
			return status;
	}
}

int sslAsn1LoadSchema(SslAsn1Navigator *navigator, const SceUInt32 *schema)
{
	int *labels, *replacement;
	int capacity = navigator->labelCount;
	int labelCount = 0, savedCount = 0, position = 0, instructionCount = 1;
	int index, saved, label;
	SceUInt32 instruction;

	if (capacity != 0) {
		labels = navigator->labels;
		navigator->labels = NULL;
	} else {
		capacity = 4;
		labels = sslAllocate(4 * sizeof(*labels));
		if (labels == NULL)
			return 10005;
	}
	sslFillMemory(labels, 255, (SceUInt32)capacity * sizeof(*labels));
	navigator->instructionCount = 0;
	navigator->schema = schema;
	navigator->currentIndex = -1;
	for (;;) {
		instruction = schema[position];
		position = (int)((SceUInt32)position + 1u);
		navigator->instructionCount = instructionCount;
		if (instruction == 0)
			break;
		saved = -1;
		if ((instruction & 255) >= 0x10 && (instruction & 255) <= 0x13) {
			saved = instruction >> 16;
		} else if ((instruction & 255) == 0x20) {
			label = instruction >> 16;
			if (capacity < label + 1) {
				replacement = sslReallocate(labels, (label + 5u) * sizeof(*labels),
							(SceUInt32)capacity * sizeof(*labels));
				if (replacement == NULL) {
					sslFree(labels);
					return 10005;
				}
				labels = replacement;
				capacity = label + 4;
				navigator->labelCount = capacity;
				for (index = labelCount; index < capacity; index++)
					labels[index] = 0xffff;
			}
			labels[label] = position;
			if (label + 1 > labelCount)
				labelCount = label + 1;
		}
		if (saved > savedCount)
			savedCount = saved;
		instructionCount = (int)((SceUInt32)navigator->instructionCount + 1u);
	}
	navigator->labelCount = labelCount;
	if (navigator->labels != NULL)
		sslFree(navigator->labels);
	navigator->labels = labels;
	navigator->savedIndexCount = savedCount + 1;
	if (navigator->savedIndexCount <= 0) {
		navigator->savedIndices = NULL;
	} else {
		if (navigator->savedIndices != NULL)
			sslFree(navigator->savedIndices);
		navigator->savedIndices = sslAllocate((savedCount + 1u) * sizeof(*navigator->savedIndices));
		if (navigator->savedIndices == NULL)
			return 10005;
		sslFillMemory(navigator->savedIndices, 0,
			      (savedCount + 1u) * sizeof(*navigator->savedIndices));
	}
	return 0;
}

int sslAsn1DecodeSchema(SslAsn1Navigator *navigator, const SceUInt32 *schema,
			void *arg, const SceUInt8 *data, SceUInt32 size,
			SceUInt32 *consumed)
{
	SceUInt32 count, decoded;
	int status;

	sslAsn1ResetNavigator(navigator);
	if (schema != NULL) {
		status = sslAsn1LoadSchema(navigator, schema);
		if (status != 0)
			return status == 5 ? 10005 : status;
	}
	if (consumed != NULL)
		decoded = *consumed;
	count = navigator->methods->Count(navigator->tree);
	status = navigator->methods->Decode(navigator->tree, data, size, &decoded);
	if (status == 0 && count != navigator->methods->Count(navigator->tree))
		navigator->currentIndex = count;
	if (consumed != NULL)
		*consumed = decoded;
	if (status != 0)
		return status == 5 ? 10005 : 10001;
	status = sslAsn1RunSchema(navigator, arg);
	return status == 5 ? 10005 : status;
}

int sslAsn1InitDecoder(SslAsn1Navigator *navigator)
{
	return sslAsn1InitNavigator(navigator, &sslAsn1DecodeMethods, sslAsn1DecodeOperations);
}

int sslAsn1DecodeProperties(const SceUInt32 *schema, SslPropertyList *list,
			    const SceUInt8 *data, SceUInt32 size,
			    SceUInt32 *consumed)
{
	SslAsn1Navigator navigator;
	int status;

	status = sslAsn1InitDecoder(&navigator);
	if (status != 0)
		return status;
	navigator.callback = sslAsn1PropertyCallback;
	if (list != NULL && list->count == 0)
		navigator.flags |= 16;
	status = sslAsn1DecodeSchema(&navigator, schema, list, data, size, consumed);
	sslAsn1DestroyNavigator(&navigator);
	return status;
}

int sslAsn1InitEncoder(SslAsn1Navigator *navigator)
{
	return sslAsn1InitNavigator(navigator, &sslAsn1EncodeMethods, sslAsn1EncodeOperations);
}

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

int sslAsn1TestElement(SslAsn1Navigator *navigator, int command, int first,
		      int second, int third)
{
	SslAsn1Element *element;
	SceUInt32 tagBit, mismatch = 0;
	int encoding = command & 3;
	unsigned int shift;

	if (navigator->currentIndex < 0)
		return 10008;
	element = navigator->tree->elements + navigator->currentIndex;
	if (first & 0x80)
		first = navigator->registers[first & 15];
	if (second & 0x80)
		second = navigator->registers[second & 15];
	if (third & 0x80)
		third = navigator->registers[third & 15];
	if (!(command & 4) || encoding == 1) {
		if (command & 4)
			encoding = 2;
		if ((element->encoding & 0xc0) != (encoding << 6))
			mismatch |= 1;
		if (element->tag != (SceUInt32)first)
			mismatch |= 2;
		if ((element->encoding & 0x3f & (unsigned char)third) != second)
			mismatch |= 4;
	} else if (encoding == 0) {
		if ((element->encoding & 0xc0) != first)
			mismatch |= 1;
		shift = element->tag & 255;
		tagBit = shift < 32 ? 1u << shift : 0;
		if (((SceUInt32)second & tagBit) != tagBit)
			mismatch |= 2;
	} else if (encoding == 2) {
		if (element->tag != (SceUInt32)first || !(element->encoding & 2))
			mismatch = 1;
	}
	if (command & 8) {
		navigator->registers[0] = (int)mismatch;
		return 0;
	}
	return mismatch != 0 ? 10001 : 0;
}

int sslAsn1StackOperation(SslAsn1Navigator *navigator, int command, int first,
			  int second, int third)
{
	int operands[3] = { first, second, third };
	SceInt32 *replacement, value;
	SceUInt32 oldSize;
	int index, count;

	if (command == 0) {
		if ((SceInt32)((SceUInt32)navigator->stackCount + 3u) >=
		    navigator->stackCapacity) {
			oldSize = (SceUInt32)navigator->stackCapacity * sizeof(*navigator->stack);
			/* Capacity grows by six words; the allocator gets six extra bytes. */
			replacement = sslReallocate(navigator->stack, oldSize + 6u, oldSize);
			if (replacement == NULL)
				return 10005;
			navigator->stack = replacement;
			navigator->stackCapacity = (SceInt32)((SceUInt32)navigator->stackCapacity + 6u);
		}
		for (index = 2; index >= 0; index--) {
			if (!(operands[index] & 0x80))
				continue;
			count = navigator->stackCount;
			value = navigator->registers[operands[index] & 15];
			navigator->stackCount = (SceInt32)((SceUInt32)count + 1u);
			navigator->stack[count] = value;
		}
		return 0;
	}
	if (command != 1)
		return 10011;
	if (navigator->stackCount == 0)
		return 10012;
	for (index = 0; index < 3; index++) {
		if (!(operands[index] & 0x80))
			continue;
		count = (SceInt32)((SceUInt32)navigator->stackCount - 1u);
		value = navigator->stack[count];
		navigator->stackCount = count;
		navigator->registers[operands[index] & 15] = value;
	}
	return 0;
}

int sslAsn1Configure(SslAsn1Navigator *navigator, int command, int first,
		     int second, int third)
{
	SceUInt32 value;

	if (second & 0x80)
		second = navigator->registers[second & 15];
	if (third & 0x80)
		third = navigator->registers[third & 15];
	value = (SceUInt32)second + ((SceUInt32)third << 8);
	if (command != 2)
		return (int)value;
	navigator->flags |= value;
	return 0;
}

int sslAsn1Label(SslAsn1Navigator *navigator, int command, int first,
		 int second, int third)
{
	return 0;
}

int sslAsn1Branch(SslAsn1Navigator *navigator, int command, int first,
		  int second, int third)
{
	int value = first;
	int label = (int)((SceUInt32)second | ((SceUInt32)third << 8));
	int take;

	if (first & 0x80)
		value = navigator->registers[first & 15];
	switch (command) {
	case 0:
		take = value == 0;
		break;
	case 1:
		take = value != 0;
		break;
	case 2:
		take = value <= 0;
		break;
	case 3:
		take = value >= 0;
		break;
	case 4:
		take = value < 0;
		break;
	case 5:
		take = value > 0;
		break;
	case 6:
		if (!(first & 0x80))
			return 10018;
		navigator->registers[first & 15] = navigator->programCounter;
		take = 1;
		break;
	case 7:
		navigator->programCounter = value;
		return 0;
	default:
		return 10018;
	}
	if (!take)
		return 0;
	if (label >= navigator->labelCount)
		return 10008;
	navigator->programCounter = navigator->labels[label];
	return 0;
}

int sslAsn1Arithmetic(SslAsn1Navigator *navigator, int command, int first,
		     int second, int third)
{
	SceUInt32 result;

	if (!(first & 0x80))
		return 10018;
	if (second & 0x80)
		second = navigator->registers[second & 15];
	if (third & 0x80)
		third = navigator->registers[third & 15];
	switch (command) {
	case 0:
		result = (SceUInt32)second + (SceUInt32)third;
		break;
	case 1:
		result = (SceUInt32)second - (SceUInt32)third;
		break;
	case 2:
		result = (SceUInt32)second * (SceUInt32)third;
		break;
	case 3:
	case 4:
		if (third == 0)
			return 10018;
		if ((SceUInt32)second == 0x80000000u && third == -1)
			result = command == 3 ? 0x80000000u : 0;
		else if (command == 3)
			result = second / third;
		else
			result = second % third;
		break;
	case 5:
		result = (SceUInt32)second | (SceUInt32)third;
		break;
	case 6:
		result = (SceUInt32)second & (SceUInt32)third;
		break;
	case 7:
		result = (SceUInt32)second ^ (SceUInt32)third;
		break;
	default:
		return 10011;
	}
	navigator->registers[first & 15] = (int)result;
	return 0;
}

int sslAsn1LoadConstant(SslAsn1Navigator *navigator, int command, int first,
			int second, int third)
{
	SceUInt32 value;
	int position;

	if (!(first & 0x80))
		return 10018;
	switch (command) {
	case 0:
		navigator->registers[first & 15] =
			(int)((SceUInt32)second | ((SceUInt32)third << 8));
		return 0;
	case 1:
		value = (SceUInt32)navigator->registers[first & 15] & 0xffff;
		value |= ((SceUInt32)second | ((SceUInt32)third << 8)) << 16;
		navigator->registers[first & 15] = (int)value;
		return 0;
	case 2:
		position = navigator->programCounter;
		value = navigator->schema[position];
		navigator->programCounter = (int)((SceUInt32)position + 1u);
		navigator->registers[first & 15] = (int)value;
		return 0;
	default:
		return 10011;
	}
}

int sslAsn1UnsupportedOperation(SslAsn1Navigator *navigator, int command,
			       int first, int second, int third)
{
	return 10011;
}

int sslAsn1EncodeSchema(SslAsn1Navigator *navigator, const SceUInt32 *schema,
			void *arg, SceUInt8 *data, SceUInt32 *length,
			SceUInt32 capacity)
{
	SslAsn1Element *element;
	SceUInt32 encoded, measured;
	int status;

	sslAsn1ResetNavigator(navigator);
	if (schema != NULL) {
		status = sslAsn1LoadSchema(navigator, schema);
		if (status != 0)
			return status == 5 ? 10005 : status;
	}
	status = sslAsn1RunSchema(navigator, arg);
	if (status != 0)
		return status == 5 ? 10005 : status;
	if (length != NULL)
		encoded = *length;
	element = navigator->methods->At(navigator->tree, 0);
	measured = navigator->methods->Measure(element);
	if (data != NULL) {
		encoded = measured;
		element = navigator->methods->At(navigator->tree, 0);
		status = navigator->methods->Encode(element, data, &encoded, capacity);
		measured = encoded;
	}
	if (length != NULL)
		*length = measured;
	return status == 5 ? 10005 : status != 0 ? 10001 : 0;
}

int sslAsn1EncodeProperties(const SceUInt32 *schema, SslPropertyList *list,
			    SceUInt8 *data, SceUInt32 *length, SceUInt32 capacity)
{
	SslAsn1Navigator navigator;
	int status;

	status = sslAsn1InitEncoder(&navigator);
	if (status != 0)
		return status;
	navigator.callback = sslAsn1PropertyCallback;
	status = sslAsn1EncodeSchema(&navigator, schema, list, data, length, capacity);
	sslAsn1DestroyNavigator(&navigator);
	return status;
}

int sslAsn1PropertyCallback(SslAsn1Navigator *navigator, void *arg,
			  SceInt32 *value, int command, int type, int id)
{
	SslPropertyList *list = arg;
	SslAsn1Element *element = navigator->currentElement;
	SslProperty *property, *append;
	const unsigned char *data = NULL;
	SceUInt32 length = 0;
	void *old, *storage;

	if (element != NULL) {
		length = element->length;
		data = element->data;
	}
	if (command == -1)
		return 0;
	if (command < 32) {
		if (element == NULL)
			return 10001;
		if (sslFindProperty(list, type, id, NULL, &property, 0) != 0)
			return 10008;
		sslAsn1SetValue(element, 0, (SceUInt32)command, property->data,
				property->length, 0, 0);
		return 0;
	}
	switch (command) {
	case 64:
	case 72:
		sslFindProperty(list, type, id, NULL, &property, 0);
		*value = property != NULL;
		return 0;
	case 65:
		if (element == NULL)
			return 10001;
		if (sslFindProperty(list, type, id, NULL, &property, 0) != 0)
			return 10008;
		sslAsn1SetValue(element, 0, property->value, property->data,
				property->length, 0, 0);
		return 0;
	case 66:
	case 70:
		if (element == NULL)
			return 10001;
		if (command == 70) {
			data = (const unsigned char *)((SceUInt32)data - element->headerLength);
			length += element->headerLength;
		}
		return sslAddPropertyValue(list, type, id, element->tag, data, length,
					   SSL_PROPERTY_REPLACE) != 0 ? 10005 : 0;
	case 67:
		return sslAddPropertyValue(list, type, id, 2, NULL, (SceUInt32)*value,
					   SSL_PROPERTY_REPLACE) != 0 ? 10005 : 0;
	case 68:
		if (sslFindProperty(list, type, id, NULL, &property, 0) != 0)
			return 10008;
		*value = (SceInt32)property->length;
		return 0;
	case 69:
		if (element == NULL)
			return 10001;
		if (sslFindProperty(list, type, id, NULL, &property, 0) != 0)
			return 10008;
		sslAsn1SetData(element, property->data, property->length);
		element->encoding = 8;
		return 0;
	case 71:
		if (element == NULL)
			return 10001;
		length = element->length;
		if (sslFindProperty(list, type, id, NULL, &append, 0) != 0) {
			if (element->encoding & 2)
				length -= 2;
			return sslAddPropertyValue(list, type, id, element->tag,
						   element->data, length,
						   SSL_PROPERTY_REPLACE) != 0 ? 10005 : 0;
		}
		if (append->flags & SSL_PROPERTY_OWNS_DATA) {
			old = append->data;
			storage = sslReallocate(old, append->length + length, append->length);
			append->data = storage;
			if (append->data == NULL) {
				append->data = old;
				return 10005;
			}
		} else {
			storage = sslAllocate(append->length + length);
			if (storage == NULL)
				return 10005;
			sslCopyMemory(storage, append->data, append->length);
			append->data = storage;
			append->flags |= SSL_PROPERTY_OWNS_DATA;
		}
		sslCopyMemory((unsigned char *)append->data + append->length, element->data, length);
		append->length += length;
		return 0;
	case 73:
	case 74:
		if (element == NULL)
			return 10001;
		if (command == 73)
			element->encoding = 8;
		if (sslFindProperty(list, type, id, NULL, &property, 0) != 0)
			return 10008;
		sslAsn1SetData(element, property->data, property->length);
		return 0;
	default:
		return 10011;
	}
}
