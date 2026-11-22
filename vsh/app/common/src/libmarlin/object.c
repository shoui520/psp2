/*
 * Octopus objects: the stored record format and the frees of the objects the
 * XML parsers build (0x8103b4a0-0x8103caa4)
 */

#include "mln_internal.h"

static unsigned int MlnReadBe32(const unsigned char *p)
{
	return p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
}

static void MlnWriteBe32(unsigned char *p, unsigned int value)
{
	p[0] = value >> 24;
	p[1] = value >> 16;
	p[2] = value >> 8;
	p[3] = value;
}

/* a record, its header and its items */
static inline __attribute__((always_inline)) void MlnRecordFree(MlnRecord *record)
{
	MlnRecordHeader *header;
	MlnRecordItem *item;
	MlnRecordItem *next;

	if (record == NULL)
		return;
	header = record->header;
	if (header != NULL) {
		MlnFree(MLN_HEAP_GENERAL, header->id);
		MlnFree(MLN_HEAP_GENERAL, header);
	}
	for (item = record->first; item != NULL; item = next) {
		next = item->next;
		MlnBufferDestroy(MLN_HEAP_GENERAL, item->data);
		MlnFree(MLN_HEAP_GENERAL, item);
	}
	MlnFree(MLN_HEAP_GENERAL, record);
}

/*
 * parses a record from data: a big-endian tag, item count and id length, the
 * id, then each item as a big-endian length and its bytes; *size is what it
 * took of data
 */
__attribute__((noinline)) int MlnImpl_8103B4A0(const MlnBuffer *data, MlnRecord **out, unsigned int *size)
{
	MlnRecord *record;
	MlnRecordHeader *header;
	MlnRecordItem *item;
	MlnBuffer *id = NULL;
	MlnBuffer *buffer = NULL;
	const unsigned char *bytes;
	const unsigned char *p;
	unsigned int length;
	unsigned int used;
	unsigned int tag;
	int count;
	int i;
	int ret;

	if (out == NULL || data == NULL) {
		ret = 3;
		goto out;
	}
	bytes = data->data;
	record = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*record));
	if (record == NULL) {
		ret = 2;
		goto out;
	}
	MlnMemset(record, 0, sizeof(*record));
	tag = MlnReadBe32(bytes);
	count = MlnReadBe32(bytes + 4);
	length = MlnReadBe32(bytes + 8);
	id = MlnBufferCreate(MLN_HEAP_GENERAL, length, bytes + 12);
	if (id == NULL)
		goto fail;
	used = length + 12;
	p = bytes + 12 + length;
	for (i = 0; i < count; i++) {
		length = MlnReadBe32(p);
		p += 4;
		buffer = MlnBufferCreate(MLN_HEAP_GENERAL, length, p);
		p += length;
		used += length + 4;
		if (buffer == NULL)
			goto fail;
		item = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*item));
		if (item == NULL)
			goto fail;
		item->next = NULL;
		item->data = buffer;
		item->previous = record->last;
		if (record->last != NULL) {
			record->last->next = item;
			record->last = item;
		} else {
			record->last = item;
			record->first = item;
		}
		record->current = item;
		record->count++;
	}
	if (size != NULL)
		*size = used;
	header = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*header));
	record->header = header;
	if (header == NULL) {
		buffer = NULL;
		goto fail;
	}
	header->tag = tag;
	record->header->count = count;
	record->header->id = id;
	*out = record;
	ret = 0;
	buffer = NULL;
	id = NULL;
	goto out;
fail:
	MlnRecordFree(record);
	ret = 2;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, buffer);
	MlnBufferDestroy(MLN_HEAP_GENERAL, id);
	return ret;
}

/* an empty array of count objects */
__attribute__((noinline)) MlnOctopusArray *MlnImpl_8103B758(int count)
{
	MlnOctopusArray *array;

	if (count < 0)
		return NULL;
	array = MlnAllocate(MLN_HEAP_GENERAL, (count + 1) * sizeof(void *));
	if (array == NULL)
		return NULL;
	MlnMemset(array, 0, (count + 1) * sizeof(void *));
	array->count = count;
	return array;
}

/*
 * The frees, each one an object's release and the free of the object. Sony's
 * compiler inlined some of them into some callers and not into others; the
 * releases are inlined where Sony's are, and the frees are called elsewhere.
 */
#define MLN_ALWAYS_INLINE	static inline __attribute__((always_inline))

#define MLN_OCTOPUS_ARRAY_FREE(array, free) do {			\
	MlnOctopusArray *items_ = (array);				\
	int i_;								\
									\
	if (items_ != NULL) {						\
		for (i_ = 0; i_ < items_->count; i_++)			\
			free(items_->items[i_]);			\
		MlnFree(MLN_HEAP_GENERAL, items_);			\
	}								\
} while (0)

#define MlnOctopusBufferFree(buffer)	MlnBufferDestroy(MLN_HEAP_GENERAL, buffer)

MLN_ALWAYS_INLINE void MlnOctopusKeyRelease(MlnOctopusKey *key)
{
	MlnBufferDestroy(MLN_HEAP_GENERAL, key->uid);
	MlnBufferDestroy(MLN_HEAP_GENERAL, key->keyName);
	MlnBufferDestroy(MLN_HEAP_GENERAL, key->oaepParams);
	MlnBufferDestroy(MLN_HEAP_GENERAL, key->encryptedKey);
	MlnBufferDestroy(MLN_HEAP_GENERAL, key->data);
	MlnImpl_81080C80(key->key);
}

MLN_ALWAYS_INLINE void MlnOctopusKeyFree(MlnOctopusKey *key)
{
	if (key != NULL) {
		MlnOctopusKeyRelease(key);
		MlnFree(MLN_HEAP_GENERAL, key);
	}
}

MLN_ALWAYS_INLINE void MlnOctopusKeyPairFree(MlnOctopusKeyPair *pair)
{
	if (pair != NULL) {
		MlnOctopusKeyRelease(&pair->key);
		MlnBufferDestroy(MLN_HEAP_GENERAL, pair->pair);
		MlnFree(MLN_HEAP_GENERAL, pair);
	}
}

MLN_ALWAYS_INLINE void MlnOctopusSamlAttributeFree(MlnOctopusSamlAttribute *attribute)
{
	if (attribute != NULL) {
		MlnBufferDestroy(MLN_HEAP_GENERAL, attribute->name);
		if (attribute->type == MLN_OCTOPUS_SAML_STRING)
			MlnBufferDestroy(MLN_HEAP_GENERAL, attribute->value.string);
		MlnFree(MLN_HEAP_GENERAL, attribute);
	}
}

MLN_ALWAYS_INLINE void MlnOctopusReferenceFree(MlnOctopusReference *reference)
{
	if (reference != NULL) {
		MlnBufferDestroy(MLN_HEAP_GENERAL, reference->uid);
		MLN_OCTOPUS_ARRAY_FREE(reference->transforms, MlnOctopusBufferFree);
		MlnBufferDestroy(MLN_HEAP_GENERAL, reference->digestMethod);
		MlnBufferDestroy(MLN_HEAP_GENERAL, reference->digestValue);
		MlnFree(MLN_HEAP_GENERAL, reference);
	}
}

MLN_ALWAYS_INLINE void MlnOctopusExtensionFree(MlnOctopusExtension *extension)
{
	if (extension != NULL) {
		MlnBufferDestroy(MLN_HEAP_GENERAL, extension->uid);
		MLN_OCTOPUS_ARRAY_FREE(extension->transforms, MlnOctopusBufferFree);
		MlnBufferDestroy(MLN_HEAP_GENERAL, extension->digestMethod);
		MlnBufferDestroy(MLN_HEAP_GENERAL, extension->digestValue);
		MlnBufferDestroy(MLN_HEAP_GENERAL, extension->subject);
		if (extension->type == MLN_OCTOPUS_EXTENSION_SCUBA)
			MlnImpl_8103C370(extension->scubaKeys);
		MlnFree(MLN_HEAP_GENERAL, extension);
	}
}

MLN_ALWAYS_INLINE void MlnOctopusAttributeFree(MlnOctopusAttribute *attribute)
{
	if (attribute != NULL) {
		MlnBufferDestroy(MLN_HEAP_GENERAL, attribute->name);
		switch (attribute->type) {
		case MLN_OCTOPUS_ATTRIBUTE_STRING:
		case MLN_OCTOPUS_ATTRIBUTE_BYTES:
			MlnBufferDestroy(MLN_HEAP_GENERAL, attribute->value.string);
			break;
		case MLN_OCTOPUS_ATTRIBUTE_LIST:
		case MLN_OCTOPUS_ATTRIBUTE_ARRAY:
			MLN_OCTOPUS_ARRAY_FREE(attribute->value.attributes, MlnImpl_8103BA60);
			break;
		}
		MlnFree(MLN_HEAP_GENERAL, attribute);
	}
}

MLN_ALWAYS_INLINE void MlnOctopusProtectorFree(MlnOctopusProtector *protector)
{
	if (protector != NULL) {
		MlnBufferDestroy(MLN_HEAP_GENERAL, protector->uid);
		MlnImpl_8103C028(protector->contentKey);
		MLN_OCTOPUS_ARRAY_FREE(protector->protectedTargets, MlnImpl_8103C028);
		MlnFree(MLN_HEAP_GENERAL, protector);
	}
}

MLN_ALWAYS_INLINE void MlnOctopusLinkFree(MlnOctopusLink *link)
{
	if (link != NULL) {
		MlnBufferDestroy(MLN_HEAP_GENERAL, link->uid);
		MlnBufferDestroy(MLN_HEAP_GENERAL, link->from);
		MlnBufferDestroy(MLN_HEAP_GENERAL, link->to);
		MLN_OCTOPUS_ARRAY_FREE(link->attributes, MlnImpl_8103BA60);
		MLN_OCTOPUS_ARRAY_FREE(link->extensions, MlnImpl_8103C0B8);
		MlnImpl_8103C284(link->control);
		MlnFree(MLN_HEAP_GENERAL, link);
	}
}

int MlnImpl_8103B7A4(MlnOctopusKey *key)
{
	MlnOctopusKeyFree(key);
	return 0;
}

int MlnImpl_8103B808(MlnOctopusKeyPair *pair)
{
	MlnOctopusKeyPairFree(pair);
	return 0;
}

__attribute__((noinline)) int MlnImpl_8103B878(MlnOctopusSamlAttribute *attribute)
{
	MlnOctopusSamlAttributeFree(attribute);
	return 0;
}

MLN_ALWAYS_INLINE void MlnOctopusBroadcastKeyBlockFree(MlnOctopusBroadcastKeyBlock *block)
{
	if (block != NULL) {
		MlnBufferDestroy(MLN_HEAP_GENERAL, block->revocationInformation);
		MlnBufferDestroy(MLN_HEAP_GENERAL, block->encryptedBroadcastKeys);
		MlnFree(MLN_HEAP_GENERAL, block);
	}
}

int MlnImpl_8103B8C0(MlnOctopusBroadcastKeyBlock *block)
{
	MlnOctopusBroadcastKeyBlockFree(block);
	return 0;
}

__attribute__((noinline)) int MlnImpl_8103B8F8(MlnOctopusNode *node)
{
	if (node != NULL) {
		MlnBufferDestroy(MLN_HEAP_GENERAL, node->uid);
		MLN_OCTOPUS_ARRAY_FREE(node->attributes, MlnImpl_8103BA60);
		MLN_OCTOPUS_ARRAY_FREE(node->extensions, MlnImpl_8103C0B8);
		MlnOctopusExtensionFree(node->scubaKeys);
		MlnFree(MLN_HEAP_GENERAL, node);
	}
	return 0;
}

__attribute__((noinline)) int MlnImpl_8103BA60(MlnOctopusAttribute *attribute)
{
	if (attribute != NULL) {
		MlnBufferDestroy(MLN_HEAP_GENERAL, attribute->name);
		switch (attribute->type) {
		case MLN_OCTOPUS_ATTRIBUTE_STRING:
		case MLN_OCTOPUS_ATTRIBUTE_BYTES:
			MlnBufferDestroy(MLN_HEAP_GENERAL, attribute->value.string);
			break;
		case MLN_OCTOPUS_ATTRIBUTE_LIST:
		case MLN_OCTOPUS_ATTRIBUTE_ARRAY:
			MLN_OCTOPUS_ARRAY_FREE(attribute->value.attributes, MlnOctopusAttributeFree);
			break;
		}
		MlnFree(MLN_HEAP_GENERAL, attribute);
	}
	return 0;
}

__attribute__((noinline)) int MlnImpl_8103BBA0(MlnOctopusLink *link)
{
	if (link != NULL) {
		MlnBufferDestroy(MLN_HEAP_GENERAL, link->uid);
		MlnBufferDestroy(MLN_HEAP_GENERAL, link->from);
		MlnBufferDestroy(MLN_HEAP_GENERAL, link->to);
		MLN_OCTOPUS_ARRAY_FREE(link->attributes, MlnImpl_8103BA60);
		MLN_OCTOPUS_ARRAY_FREE(link->extensions, MlnOctopusExtensionFree);
		MlnImpl_8103C284(link->control);
		MlnFree(MLN_HEAP_GENERAL, link);
	}
	return 0;
}

__attribute__((noinline)) int MlnImpl_8103BD20(MlnOctopusProtector *protector)
{
	if (protector != NULL) {
		MlnBufferDestroy(MLN_HEAP_GENERAL, protector->uid);
		MlnOctopusReferenceFree(protector->contentKey);
		MLN_OCTOPUS_ARRAY_FREE(protector->protectedTargets, MlnOctopusReferenceFree);
		MlnFree(MLN_HEAP_GENERAL, protector);
	}
	return 0;
}

__attribute__((noinline)) int MlnImpl_8103BE9C(MlnOctopusController *controller)
{
	if (controller != NULL) {
		MlnBufferDestroy(MLN_HEAP_GENERAL, controller->uid);
		MlnOctopusReferenceFree(controller->control);
		MLN_OCTOPUS_ARRAY_FREE(controller->controlledTargets, MlnImpl_8103C028);
		MLN_OCTOPUS_ARRAY_FREE(controller->attributes, MlnImpl_8103BA60);
		MLN_OCTOPUS_ARRAY_FREE(controller->extensions, MlnImpl_8103C0B8);
		MlnFree(MLN_HEAP_GENERAL, controller);
	}
	return 0;
}

__attribute__((noinline)) int MlnImpl_8103C028(MlnOctopusReference *reference)
{
	MlnOctopusReferenceFree(reference);
	return 0;
}

__attribute__((noinline)) int MlnImpl_8103C0B8(MlnOctopusExtension *extension)
{
	MlnOctopusExtensionFree(extension);
	return 0;
}

__attribute__((noinline)) int MlnImpl_8103C16C(MlnOctopusContentKey *contentKey)
{
	if (contentKey != NULL) {
		MlnBufferDestroy(MLN_HEAP_GENERAL, contentKey->uid);
		MlnOctopusKeyFree(contentKey->secretKey);
		MLN_OCTOPUS_ARRAY_FREE(contentKey->attributes, MlnImpl_8103BA60);
		MLN_OCTOPUS_ARRAY_FREE(contentKey->extensions, MlnImpl_8103C0B8);
		MlnFree(MLN_HEAP_GENERAL, contentKey);
	}
	return 0;
}

__attribute__((noinline)) int MlnImpl_8103C284(MlnOctopusControl *control)
{
	if (control != NULL) {
		MlnBufferDestroy(MLN_HEAP_GENERAL, control->uid);
		MlnBufferDestroy(MLN_HEAP_GENERAL, control->protocol);
		MlnBufferDestroy(MLN_HEAP_GENERAL, control->type);
		MlnBufferDestroy(MLN_HEAP_GENERAL, control->codeModule);
		MlnBufferDestroy(MLN_HEAP_GENERAL, control->signer);
		MLN_OCTOPUS_ARRAY_FREE(control->attributes, MlnImpl_8103BA60);
		MLN_OCTOPUS_ARRAY_FREE(control->extensions, MlnImpl_8103C0B8);
		MlnFree(MLN_HEAP_GENERAL, control);
	}
	return 0;
}

__attribute__((noinline)) int MlnImpl_8103C370(MlnOctopusScubaKeys *keys)
{
	if (keys != NULL) {
		MLN_OCTOPUS_ARRAY_FREE(keys->secretKeys, MlnImpl_8103B7A4);
		MLN_OCTOPUS_ARRAY_FREE(keys->privateKeys, MlnImpl_8103B808);
		MLN_OCTOPUS_ARRAY_FREE(keys->publicKeys, MlnImpl_8103B808);
		MlnFree(MLN_HEAP_GENERAL, keys);
	}
	return 0;
}

/* frees an array and the objects of the kind (MLN_OCTOPUS_*) it holds */
__attribute__((noinline)) int MlnImpl_8103C57C(MlnOctopusArray *array, int kind)
{
	void *item;
	int i;

	if (array != NULL) {
		for (i = 0; i < array->count; i++) {
			item = array->items[i];
			switch (kind) {
			case MLN_OCTOPUS_PROTECTOR:
				MlnOctopusProtectorFree(item);
				break;
			case MLN_OCTOPUS_CONTENT_KEY:
				MlnImpl_8103C16C(item);
				break;
			case MLN_OCTOPUS_CONTROLLER:
				MlnImpl_8103BE9C(item);
				break;
			case MLN_OCTOPUS_CONTROL:
				MlnImpl_8103C284(item);
				break;
			case MLN_OCTOPUS_LINK:
				MlnOctopusLinkFree(item);
				break;
			case MLN_OCTOPUS_NODE:
				MlnImpl_8103B8F8(item);
				break;
			case MLN_OCTOPUS_REFERENCE:
				MlnOctopusReferenceFree(item);
				break;
			case MLN_OCTOPUS_ATTRIBUTE:
				MlnOctopusAttributeFree(item);
				break;
			case MLN_OCTOPUS_EXTENSION:
				MlnOctopusExtensionFree(item);
				break;
			case MLN_OCTOPUS_KEY:
				MlnOctopusKeyFree(item);
				break;
			case MLN_OCTOPUS_KEY_PAIR:
				MlnOctopusKeyPairFree(item);
				break;
			case MLN_OCTOPUS_BUFFER:
				MlnBufferDestroy(MLN_HEAP_GENERAL, item);
				break;
			case MLN_OCTOPUS_SAML_ATTRIBUTE:
				MlnOctopusSamlAttributeFree(item);
				break;
			case MLN_OCTOPUS_PLAIN:
				MlnFree(MLN_HEAP_GENERAL, item);
				break;
			}
		}
		MlnFree(MLN_HEAP_GENERAL, array);
	}
	return 0;
}

MLN_ALWAYS_INLINE void MlnOctopusBundleExtensionFree(MlnOctopusBundleExtension *extension)
{
	if (extension != NULL) {
		MlnImpl_8103C0B8(extension->extension);
		MlnImpl_8103C284(extension->control);
		MlnFree(MLN_HEAP_GENERAL, extension);
	}
}

/* a bundle with no content key count yet */
MlnOctopusBundle *MlnImpl_8103CAA4(void)
{
	MlnOctopusBundle *bundle;

	bundle = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*bundle));
	if (bundle != NULL) {
		MlnMemset(bundle, 0, sizeof(*bundle));
		bundle->contentKeyCount = -1;
	}
	return bundle;
}

int MlnImpl_8103CAD8(MlnOctopusBundle *bundle)
{
	if (bundle != NULL) {
		MlnImpl_8103C284(bundle->control);
		MlnImpl_8103BE9C(bundle->controller);
		MLN_OCTOPUS_ARRAY_FREE(bundle->contentKeys, MlnImpl_8103C16C);
		MlnOctopusBroadcastKeyBlockFree(bundle->broadcastKeyBlock);
		MLN_OCTOPUS_ARRAY_FREE(bundle->protectors, MlnImpl_8103BD20);
		MlnOctopusBundleExtensionFree(bundle->extension);
		MLN_OCTOPUS_ARRAY_FREE(bundle->attributes, MlnImpl_8103BA60);
		MlnFree(MLN_HEAP_GENERAL, bundle);
	}
	return 0;
}

int MlnImpl_8103CC38(MlnOctopusBundleExtension *extension)
{
	if (extension != NULL) {
		MlnOctopusExtensionFree(extension->extension);
		MlnImpl_8103C284(extension->control);
		MlnFree(MLN_HEAP_GENERAL, extension);
	}
	return 0;
}

int MlnImpl_8103CD0C(MlnOctopusSamlAssertion *assertion)
{
	if (assertion != NULL) {
		MlnBufferDestroy(MLN_HEAP_GENERAL, assertion->assertionId);
		MlnFree(MLN_HEAP_GENERAL, assertion->notBefore);
		MlnFree(MLN_HEAP_GENERAL, assertion->notOnOrAfter);
		MLN_OCTOPUS_ARRAY_FREE(assertion->attributes, MlnOctopusSamlAttributeFree);
		MlnBufferDestroy(MLN_HEAP_GENERAL, assertion->nameIdentifier);
		MlnFree(MLN_HEAP_GENERAL, assertion);
	}
	return 0;
}

__attribute__((noinline)) int MlnImpl_8103CDE0(MlnOctopusKey *key, MlnBuffer *uid, int usage, int format,
		     int encryptionMethod, MlnBuffer *oaepParams, MlnBuffer *keyName,
		     int keyEncryptionMethod, MlnBuffer *encryptedKey, MlnBuffer *data)
{
	if (key == NULL)
		return 3;
	key->uid = uid;
	key->usage = usage;
	key->format = format;
	key->encryptionMethod = encryptionMethod;
	key->oaepParams = oaepParams;
	key->keyEncryptionMethod = keyEncryptionMethod;
	key->keyName = keyName;
	key->encryptedKey = encryptedKey;
	key->data = data;
	key->key = NULL;
	key->state = 9;
	return 0;
}

__attribute__((noinline)) int MlnImpl_8103CE44(MlnOctopusKeyPair *pair, MlnBuffer *keyPair, MlnBuffer *uid, int usage,
		     int format, int encryptionMethod, MlnBuffer *oaepParams,
		     MlnBuffer *keyName, int keyEncryptionMethod, MlnBuffer *encryptedKey,
		     MlnBuffer *data)
{
	if (pair == NULL)
		return 3;
	pair->key.uid = uid;
	pair->key.usage = usage;
	pair->key.format = format;
	pair->key.encryptionMethod = encryptionMethod;
	pair->key.oaepParams = oaepParams;
	pair->key.keyEncryptionMethod = keyEncryptionMethod;
	pair->key.keyName = keyName;
	pair->key.encryptedKey = encryptedKey;
	pair->key.data = data;
	pair->key.key = NULL;
	pair->key.state = 9;
	pair->pair = keyPair;
	return 0;
}

/* Plankton values: a value's release, a value list's and a parameter block's */
MLN_ALWAYS_INLINE void MlnPlanktonParameterBlockFree(MlnPlanktonParameterBlock *block)
{
	unsigned int i;

	if (block != NULL) {
		for (i = 0; i < block->count; i++)
			MlnImpl_8103CEB0(&block->parameters[i]);
		MlnFree(MLN_HEAP_GENERAL, block->parameters);
		MlnFree(MLN_HEAP_GENERAL, block);
	}
}

MLN_ALWAYS_INLINE void MlnPlanktonValueRelease(MlnPlanktonValue *value)
{
	if (value == NULL)
		return;
	switch (value->type) {
	case MLN_PLANKTON_STRING:
		MlnBufferDestroy(MLN_HEAP_GENERAL, value->value.buffer);
		break;
	case MLN_PLANKTON_PARAMETER:
		MlnPlanktonParameterBlockFree(value->value.parameters);
		break;
	case MLN_PLANKTON_RESOURCE:
		MlnBufferDestroy(MLN_HEAP_GENERAL, value->value.buffer);
		break;
	case MLN_PLANKTON_VALUE_LIST:
		MlnImpl_8103CEB0(value->value.list);
		MlnFree(MLN_HEAP_GENERAL, value->value.list);
		break;
	case MLN_PLANKTON_BYTE_ARRAY:
		MlnBufferDestroy(MLN_HEAP_GENERAL, value->value.buffer);
		break;
	}
}

void MlnImpl_8103CEB0(MlnPlanktonValueList *list)
{
	unsigned int i;

	if (list == NULL)
		return;
	if (list->name != NULL)
		MlnBufferDestroy(MLN_HEAP_GENERAL, list->name);
	for (i = 0; i < list->count; i++)
		MlnPlanktonValueRelease(&list->values[i]);
	MlnFree(MLN_HEAP_GENERAL, list->values);
}

void MlnImpl_8103CFC0(MlnPlanktonParameterBlock *block)
{
	MlnPlanktonParameterBlockFree(block);
}

void MlnImpl_8103D01C(MlnPlanktonValue *value)
{
	MlnPlanktonValueRelease(value);
}

/* writes records as 8103B4A0 reads them, after a big-endian record count */
int MlnImpl_8103D0DC(const MlnRecordArray *array, MlnBuffer **out)
{
	MlnBuffer *buffer = NULL;
	MlnRecord *record;
	MlnRecordItem *item;
	MlnBuffer *id;
	unsigned char *p;
	unsigned int size;
	int ret;
	int i;

	if (array == NULL || array->records == NULL || out == NULL) {
		ret = 3;
		goto out;
	}
	size = array->count * 8 + 4;
	for (i = 0; i < array->count; i++) {
		record = array->records[i];
		if (record == NULL || record->header == NULL) {
			ret = 3;
			goto out;
		}
		size += record->header->id->size + 4;
		for (item = record->first; item != NULL; item = item->next)
			size += item->data->size + 4;
	}
	buffer = MlnBufferCreate(MLN_HEAP_GENERAL, size, NULL);
	if (buffer == NULL) {
		ret = 2;
		goto out;
	}
	p = buffer->data;
	MlnWriteBe32(p, array->count);
	p += 4;
	for (i = 0; i < array->count; i++) {
		id = array->records[i]->header->id;
		MlnWriteBe32(p, array->records[i]->header->tag);
		MlnWriteBe32(p + 4, array->records[i]->count);
		MlnWriteBe32(p + 8, id->size);
		p += 12;
		MlnMemcpy(p, id->data, id->size);
		p += id->size;
		for (item = array->records[i]->first; item != NULL; item = item->next) {
			MlnWriteBe32(p, item->data->size);
			p += 4;
			MlnMemcpy(p, item->data->data, item->data->size);
			p += item->data->size;
		}
	}
	*out = buffer;
	buffer = NULL;
	ret = 0;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, buffer);
	return ret;
}

/*
 * appends the records data holds (a big-endian record count, then the records
 * as 8103B4A0 reads them) to array->count, and loads them into a new
 * array->records
 */
int MlnImpl_8103D30C(const MlnBuffer *data, MlnRecordArray *array)
{
	MlnRecord *record = NULL;
	MlnBuffer *copy;
	unsigned char *start = NULL;
	unsigned int size = 0;
	int ret;
	int i;

	if (array == NULL) {
		copy = NULL;
		ret = 3;
		goto out;
	}
	if (data != NULL) {
		copy = MlnBufferCreate(MLN_HEAP_GENERAL, data->size, data->data);
		if (copy == NULL) {
			ret = 2;
			goto out;
		}
		start = copy->data;
		array->count += MlnReadBe32(start);
		copy->size -= 4;
		copy->data += 4;
	} else {
		copy = NULL;
	}
	/* Sony's: a record's size for each pointer */
	array->records = MlnAllocate(MLN_HEAP_GENERAL, array->count * sizeof(MlnRecord));
	if (array->records == NULL) {
		ret = 2;
		goto out;
	}
	MlnMemset(array->records, 0, array->count * sizeof(MlnRecord));
	if (copy == NULL) {
		ret = 0;
		goto out;
	}
	for (i = 0; copy->size != 0; i++) {
		size = 0;
		ret = MlnImpl_8103B4A0(copy, &record, &size);
		if (ret != 0)
			goto out;
		if (copy->size < size) {
			ret = 5;
			goto out;
		}
		copy->size -= size;
		copy->data += size;
		array->records[i] = record;
		record = NULL;
	}
	copy->data = start;
	ret = 0;
out:
	MlnFree(MLN_HEAP_GENERAL, record);
	MlnBufferDestroy(MLN_HEAP_GENERAL, copy);
	return ret;
}

/*
 * finds the record with id in data (records as 8103B4A0 reads them) and
 * parses it; data is left at the record
 */
int MlnImpl_8103D4AC(MlnBuffer *data, const MlnBuffer *id, MlnRecord **out)
{
	MlnBuffer window;
	unsigned int count;
	unsigned int i;
	unsigned char *match;
	unsigned char *start;
	int ret;

	if (id == NULL || data == NULL || out == NULL)
		return 3;
	*out = NULL;
	if (id->size > data->size)
		return 1;
	count = data->size + 1 - id->size;
	if (count == 0)
		return 1;
	for (i = 0;; ) {
		match = data->data + i;
		window.size = id->size;
		window.data = match;
		i++;
		if (MlnBufferCompare(MLN_HEAP_GENERAL, &window, id) == 0)
			break;
		if (i == count)
			return 1;
	}
	if (match == NULL)
		return 1;
	start = data->data;
	data->data = match - 12;
	data->size = data->size - 12 + (start - match);
	ret = MlnImpl_8103B4A0(data, out, NULL);
	if (ret != 0)
		return ret;
	if ((*out)->header == NULL)
		return 5;
	if (MlnBufferCompareKey(MLN_HEAP_GENERAL, id, (*out)->header->id) != 0)
		return 5;
	return 0;
}

void MlnImpl_8103D5B8(MlnRecord *record)
{
	MlnRecordFree(record);
}

/*
 * reads an XML signature Reference's DigestMethod Algorithm, its DigestValue
 * (base64-decoded) and its Transforms' Algorithms, if it has any
 */
int MlnImpl_8103D628(MlnXmlNode *node, MlnOctopusArray **transforms, MlnBuffer **digestMethod,
		     MlnBuffer **digestValue)
{
	MlnXmlNodeList *list = NULL;
	MlnBuffer *method = NULL;
	MlnBuffer *text = NULL;
	MlnBuffer *value = NULL;
	MlnOctopusArray *algorithms = NULL;
	int count;
	int ret;
	int err;
	int i;

	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, node, "DigestMethod", MLN_XMLDSIG_NAMESPACE, &list);
	if (ret != 0)
		goto out;
	if (list->count != 1) {
		ret = MLN_OCTOPUS_ERROR_FORMAT;
		goto out;
	}
	ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, list->nodes[0], NULL, "Algorithm", &method);
	if (ret != 0)
		goto out;
	MlnXmlListFree(MLN_HEAP_GENERAL, list);
	list = NULL;
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, node, "DigestValue", MLN_XMLDSIG_NAMESPACE, &list);
	if (ret != 0)
		goto out;
	if (list->count != 1) {
		ret = MLN_OCTOPUS_ERROR_FORMAT;
		goto out;
	}
	ret = MlnXmlGetText(MLN_HEAP_GENERAL, list->nodes[0], &text);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81026A9C(MLN_HEAP_GENERAL, text, &value);
	if (ret != 0)
		goto out;
	MlnXmlListFree(MLN_HEAP_GENERAL, list);
	list = NULL;
	err = MlnXmlFindNodes(MLN_HEAP_GENERAL, node, "Transforms/Transform", MLN_XMLDSIG_NAMESPACE,
			      &list);
	if (err != 0) {
		if (err != MLN_XML_ERROR_NOT_FOUND) {
			ret = err;
			goto out;
		}
	} else {
		count = list->count;
		algorithms = MlnImpl_8103B758(count);
		if (algorithms == NULL) {
			ret = 2;
			goto out;
		}
		for (i = 0; i < count; i++) {
			ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, list->nodes[i], NULL, "Algorithm",
						 (MlnBuffer **)&algorithms->items[i]);
			if (ret != 0)
				goto out;
		}
		MlnXmlListFree(MLN_HEAP_GENERAL, list);
		list = NULL;
	}
	*transforms = algorithms;
	algorithms = NULL;
	*digestMethod = method;
	method = NULL;
	*digestValue = value;
	value = NULL;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, method);
	MlnBufferDestroy(MLN_HEAP_GENERAL, value);
	MlnBufferDestroy(MLN_HEAP_GENERAL, text);
	MlnXmlListFree(MLN_HEAP_GENERAL, list);
	MlnImpl_8103C57C(algorithms, MLN_OCTOPUS_BUFFER);
	return ret;
}

/*
 * reads an Attribute: its name (which an array's elements must not have and
 * all others must), its type (a string without one) and its value; a list's
 * or an array's elements are Attributes of its own
 */
int MlnImpl_8103D890(MlnXmlNode *node, unsigned int anonymous, MlnOctopusAttribute **out)
{
	MlnBuffer *name = NULL;
	MlnBuffer *type = NULL;
	MlnBuffer *text = NULL;
	MlnBuffer *bytes = NULL;
	MlnOctopusAttribute *child = NULL;
	MlnXmlNodeList *children = NULL;
	MlnOctopusArray *elements = NULL;
	MlnOctopusAttribute *attribute = NULL;
	const char *path = NULL;
	unsigned int elementsAnonymous = 0;
	int kind;
	int count;
	int ret;
	int i;

	if (out == NULL || node == NULL) {
		ret = 3;
		goto out;
	}
	if (anonymous > 1) {
		ret = 3;
		goto out;
	}
	ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, node, NULL, "name", &name);
	if ((ret == 0 && anonymous) || (ret == MLN_XML_ERROR_NOT_FOUND && !anonymous)) {
		ret = MLN_OCTOPUS_ERROR_FORMAT;
		goto out;
	}
	if (MlnXmlGetAttribute(MLN_HEAP_GENERAL, node, NULL, "type", &type) != 0 ||
	    MlnBytesCompare(MLN_HEAP_GENERAL, type->size, type->data, 6, "string") == 0) {
		kind = MLN_OCTOPUS_ATTRIBUTE_STRING;
	} else if (MlnBytesCompare(MLN_HEAP_GENERAL, type->size, type->data, 5, "bytes") == 0) {
		kind = MLN_OCTOPUS_ATTRIBUTE_BYTES;
	} else if (MlnBytesCompare(MLN_HEAP_GENERAL, type->size, type->data, 3, "int") == 0) {
		kind = MLN_OCTOPUS_ATTRIBUTE_INT;
	} else if (MlnBytesCompare(MLN_HEAP_GENERAL, type->size, type->data, 4, "list") == 0) {
		kind = MLN_OCTOPUS_ATTRIBUTE_LIST;
		path = "AttributeList/Attribute";
	} else if (MlnBytesCompare(MLN_HEAP_GENERAL, type->size, type->data, 5, "array") == 0) {
		kind = MLN_OCTOPUS_ATTRIBUTE_ARRAY;
		path = "AttributeArray/Attribute";
		elementsAnonymous = 1;
	} else {
		ret = MLN_OCTOPUS_ERROR_FORMAT;
		goto out;
	}
	ret = MlnXmlGetText(MLN_HEAP_GENERAL, node, &text);
	if (ret != 0)
		goto out;
	attribute = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*attribute));
	if (attribute == NULL) {
		ret = 2;
		goto out;
	}
	MlnMemset(attribute, 0, sizeof(*attribute));
	attribute->name = name;
	attribute->type = kind;
	name = NULL;
	switch (kind) {
	case MLN_OCTOPUS_ATTRIBUTE_STRING:
		attribute->value.string = text;
		text = NULL;
		break;
	case MLN_OCTOPUS_ATTRIBUTE_BYTES:
		ret = MlnImpl_81026A9C(MLN_HEAP_GENERAL, text, &bytes);
		if (ret != 0)
			goto out;
		attribute->value.bytes = bytes;
		bytes = NULL;
		break;
	case MLN_OCTOPUS_ATTRIBUTE_LIST:
	case MLN_OCTOPUS_ATTRIBUTE_ARRAY:
		ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, node, path, MLN_OCTOPUS_NAMESPACE, &children);
		if (ret != 0)
			goto out;
		count = children->count;
		if (count == 0) {
			ret = MLN_OCTOPUS_ERROR_FORMAT;
			goto out;
		}
		elements = MlnImpl_8103B758(count);
		if (elements == NULL) {
			ret = 2;
			goto out;
		}
		for (i = 0; i < count; i++) {
			ret = MlnImpl_8103D890(children->nodes[i], elementsAnonymous, &child);
			if (ret != 0)
				goto out;
			elements->items[i] = child;
			child = NULL;
		}
		attribute->value.attributes = elements;
		elements = NULL;
		break;
	default:
		ret = MlnImpl_81009B50(MLN_HEAP_GENERAL, text->size, text->data,
				       &attribute->value.integer);
		if (ret != 0)
			goto out;
		break;
	}
	*out = attribute;
	attribute = NULL;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, type);
	MlnBufferDestroy(MLN_HEAP_GENERAL, name);
	MlnBufferDestroy(MLN_HEAP_GENERAL, text);
	MlnBufferDestroy(MLN_HEAP_GENERAL, bytes);
	MlnImpl_8103C57C(elements, MLN_OCTOPUS_ATTRIBUTE);
	MlnFree(MLN_HEAP_GENERAL, child);
	MlnImpl_8103BA60(attribute);
	MlnXmlListFree(MLN_HEAP_GENERAL, children);
	return ret;
}

/* reads a node's AttributeList; NULL without one */
MLN_ALWAYS_INLINE int MlnOctopusAttributeListParse(MlnXmlNode *node, MlnOctopusArray **out)
{
	MlnXmlNodeList *list = NULL;
	MlnOctopusAttribute *attribute = NULL;
	MlnOctopusArray *attributes = NULL;
	int count;
	int ret;
	int i;

	if (MlnXmlFindNodes(MLN_HEAP_GENERAL, node, "AttributeList/Attribute", MLN_OCTOPUS_NAMESPACE,
			    &list) == 0) {
		count = list->count;
		if (count > 0) {
			attributes = MlnImpl_8103B758(count);
			if (attributes == NULL) {
				ret = 2;
				goto out;
			}
			for (i = 0; i < count; i++) {
				ret = MlnImpl_8103D890(list->nodes[i], 0, &attribute);
				if (ret != 0)
					goto out;
				attributes->items[i] = attribute;
				attribute = NULL;
			}
		}
	}
	*out = attributes;
	attributes = NULL;
	ret = 0;
out:
	MlnXmlListFree(MLN_HEAP_GENERAL, list);
	MlnImpl_8103C57C(attributes, MLN_OCTOPUS_ATTRIBUTE);
	return ret;
}

__attribute__((noinline)) int MlnImpl_8103DCBC(MlnXmlNode *node, MlnOctopusArray **out)
{
	return MlnOctopusAttributeListParse(node, out);
}

/* whether buffer holds string, as Sony compares the names it reads */
#define MLN_IS(buffer, string) \
	(MlnBytesCompare(MLN_HEAP_GENERAL, (buffer)->size, (buffer)->data, MlnStrlen(string), \
			 string) == 0)

#define MLN_XMLENC_RSA_1_5		"http://www.w3.org/2001/04/xmlenc#rsa-1_5"
#define MLN_XMLENC_RSA_OAEP_MGF1P	"http://www.w3.org/2001/04/xmlenc#rsa-oaep-mgf1p"
#define MLN_XMLENC_AES128_CBC		"http://www.w3.org/2001/04/xmlenc#aes128-cbc"

/*
 * reads a key (pair: a key pair, with its pair attribute): its uid, usage,
 * and KeyData, either base64 or an xmlenc EncryptedData
 */
int MlnImpl_8103DDB0(MlnXmlNode *node, int pair, void **out)
{
	MlnXmlNodeList *keyData = NULL;
	MlnXmlNodeList *encrypted = NULL;
	MlnBuffer *uid = NULL;
	MlnBuffer *keyPair = NULL;
	MlnBuffer *data = NULL;
	MlnBuffer *keyName = NULL;
	MlnBuffer *oaepParams = NULL;
	MlnBuffer *encryptedKey = NULL;
	MlnBuffer *attribute = NULL;
	MlnBuffer *unused = NULL;
	MlnBuffer *method;
	MlnBuffer *keyMethod;
	MlnXmlNode *keyDataNode;
	MlnOctopusKeyPair *newPair;
	MlnOctopusKey *key;
	int usage;
	int encoding;
	int format;
	int encryptionMethod;
	int keyEncryptionMethod;
	int ret;

	ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, node, NULL, "uid", &uid);
	if (ret != 0)
		goto out;
	if (pair) {
		ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, node, NULL, "pair", &keyPair);
		if (ret != 0)
			goto out;
	}
	if (MlnXmlGetAttribute(MLN_HEAP_GENERAL, node, NULL, "usage", &attribute) != 0) {
		usage = 2;
	} else {
		if (MlnBytesCompare(MLN_HEAP_GENERAL, attribute->size, attribute->data, 10,
				    "KeySharing") == 0) {
			usage = 0;
		} else if (MlnBytesCompare(MLN_HEAP_GENERAL, attribute->size, attribute->data, 15,
					   "Confidentiality") == 0) {
			usage = 1;
		} else {
			MlnBufferDestroy(MLN_HEAP_GENERAL, attribute);
			ret = MLN_OCTOPUS_ERROR_FORMAT;
			attribute = NULL;
			goto out;
		}
		MlnBufferDestroy(MLN_HEAP_GENERAL, attribute);
		attribute = NULL;
	}
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, node, "KeyData", MLN_OCTOPUS_NAMESPACE, &keyData);
	if (ret != 0)
		goto out;
	encoding = keyData->count;
	if (encoding != 1) {
		ret = MLN_OCTOPUS_ERROR_FORMAT;
		goto out;
	}
	keyDataNode = keyData->nodes[0];
	/* encoding: 0 xmlenc, 1 base64, 2 base64 by default */
	if (MlnXmlGetAttribute(MLN_HEAP_GENERAL, keyDataNode, NULL, "encoding", &attribute) != 0) {
		encoding = 2;
	} else {
		if (MLN_IS(attribute, "xmlenc"))
			encoding = 0;
		else if (!MLN_IS(attribute, "base64"))
			goto format;
		MlnBufferDestroy(MLN_HEAP_GENERAL, attribute);
		attribute = NULL;
	}
	ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, keyDataNode, NULL, "format", &attribute);
	if (ret != 0)
		goto out;
	if (MLN_IS(attribute, "PKCS#8"))
		format = 0;
	else if (MLN_IS(attribute, "X509SPKI"))
		format = 1;
	else if (MLN_IS(attribute, "RAW"))
		format = 2;
	else
		goto format;
	MlnBufferDestroy(MLN_HEAP_GENERAL, attribute);
	attribute = NULL;
	if (encoding != 0) {
		ret = MlnXmlGetText(MLN_HEAP_GENERAL, keyDataNode, &attribute);
		if (ret != 0)
			goto out;
		ret = MlnImpl_81026A9C(MLN_HEAP_GENERAL, attribute, &data);
		if (ret != 0)
			goto out;
		keyEncryptionMethod = 0;
		MlnBufferDestroy(MLN_HEAP_GENERAL, attribute);
		encryptionMethod = 0;
		attribute = NULL;
	} else {
		ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, keyDataNode, "EncryptedData",
				      "http://www.w3.org/2001/04/xmlenc#", &encrypted);
		if (ret != 0)
			goto out;
		if (encrypted->count != 1) {
			ret = MLN_OCTOPUS_ERROR_FORMAT;
			goto out;
		}
		ret = MlnImpl_81082230(encrypted->nodes[0], &method, &oaepParams, &keyName, &keyMethod,
				       &encryptedKey, &data);
		if (ret != 0)
			goto out;
		encryptionMethod = 0;
		if (method != NULL) {
			if (MLN_IS(method, MLN_XMLENC_RSA_1_5))
				encryptionMethod = 2;
			else if (MLN_IS(method, MLN_XMLENC_RSA_OAEP_MGF1P))
				encryptionMethod = 4;
			else if (MLN_IS(method, MLN_XMLENC_AES128_CBC))
				encryptionMethod = 1;
			else
				encryptionMethod = -1;
		}
		keyEncryptionMethod = 0;
		if (keyMethod != NULL) {
			if (MLN_IS(keyMethod, MLN_XMLENC_RSA_1_5)) {
				keyEncryptionMethod = 2;
			} else if (MLN_IS(keyMethod, MLN_XMLENC_AES128_CBC)) {
				keyEncryptionMethod = 1;
			} else {
				MlnBufferDestroy(MLN_HEAP_GENERAL, method);
				ret = MLN_OCTOPUS_ERROR_FORMAT;
				MlnBufferDestroy(MLN_HEAP_GENERAL, keyMethod);
				MlnXmlListFree(MLN_HEAP_GENERAL, encrypted);
				encrypted = NULL;
				goto out;
			}
		}
		MlnBufferDestroy(MLN_HEAP_GENERAL, method);
		MlnBufferDestroy(MLN_HEAP_GENERAL, keyMethod);
		MlnXmlListFree(MLN_HEAP_GENERAL, encrypted);
		encrypted = NULL;
		if (encryptionMethod < 0) {
			ret = MLN_OCTOPUS_ERROR_FORMAT;
			goto out;
		}
	}
	if (pair) {
		newPair = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*newPair));
		if (newPair == NULL) {
			ret = 2;
			goto out;
		}
		ret = MlnImpl_8103CE44(newPair, keyPair, uid, usage, format, encryptionMethod,
				       oaepParams, keyName, keyEncryptionMethod, encryptedKey, data);
		if (ret == 0) {
			keyPair = NULL;
			uid = NULL;
			keyName = NULL;
			oaepParams = NULL;
			encryptedKey = NULL;
			data = NULL;
			*out = newPair;
		}
		goto out;
	}
	key = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*key));
	if (key == NULL) {
		ret = 2;
		goto out;
	}
	ret = MlnImpl_8103CDE0(key, uid, usage, format, encryptionMethod, oaepParams, keyName,
			       keyEncryptionMethod, encryptedKey, data);
	if (ret == 0) {
		uid = NULL;
		keyName = NULL;
		oaepParams = NULL;
		encryptedKey = NULL;
		data = NULL;
		*out = key;
	}
	goto out;
format:
	MlnBufferDestroy(MLN_HEAP_GENERAL, attribute);
	attribute = NULL;
	ret = MLN_OCTOPUS_ERROR_FORMAT;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, uid);
	MlnBufferDestroy(MLN_HEAP_GENERAL, keyPair);
	MlnBufferDestroy(MLN_HEAP_GENERAL, keyName);
	MlnBufferDestroy(MLN_HEAP_GENERAL, oaepParams);
	MlnBufferDestroy(MLN_HEAP_GENERAL, encryptedKey);
	MlnBufferDestroy(MLN_HEAP_GENERAL, data);
	MlnBufferDestroy(MLN_HEAP_GENERAL, attribute);
	MlnBufferDestroy(MLN_HEAP_GENERAL, unused);
	MlnXmlListFree(MLN_HEAP_GENERAL, keyData);
	MlnXmlListFree(MLN_HEAP_GENERAL, encrypted);
	return ret;
}

/* the digest of data of digestSize bytes: SHA-1 (20) or SHA-256 (32); none for 0 */
int MlnImpl_8103E4FC(const void *data, unsigned int size, unsigned char *digest,
		     int digestSize)
{
	if (data == NULL)
		return 3;
	switch (digestSize) {
	case 0:
		return 0;
	case 20:
		return MlnImpl_810570A8(data, size, digest);
	case 32:
		return MlnImpl_8105713C(data, size, digest);
	default:
		return 3;
	}
}

#define MLN_OCTOPUS_CBS	"http://www.octopus-drm.com/octopus/specs/cbs-1_0"

/* the errors of a token that is not well formed: its XML's and its signature's */
MLN_ALWAYS_INLINE int MlnOctopusFormatError(int ret)
{
	switch (ret & 0xFFFF0000) {
	case 0x10010000:
	case 0x22040000:
	case 0x220A0000:
	case 0x23070000:
		return MLN_OCTOPUS_ERROR_FORMAT;
	}
	return ret;
}

/*
 * reads a SAML 1.0 Assertion, its Conditions, Attributes and NameIdentifier,
 * and when verify is set checks its enveloped signature
 */
int MlnImpl_8103E540(const MlnBuffer *token, int type, MlnOctopusSamlAssertion **out,
		     int verify)
{
	MlnXmlNode *document = NULL;
	MlnXmlNodeList *assertions = NULL;
	MlnXmlNodeList *list = NULL;
	MlnXmlNodeList *values = NULL;
	MlnXmlNodeList *transforms = NULL;
	MlnBuffer *value = NULL;
	MlnBuffer *name = NULL;
	unsigned char *digest;
	int digestType;
	int verified;
	int digestSize;
	unsigned char digestBuffer[32];
	MlnOctopusSamlAssertion *assertion;
	MlnOctopusSamlAttribute *attribute = NULL;
	MlnXmlNode *node;
	MlnXmlNode *conditions;
	MlnXmlNode *signature;
	int usage;
	int count;
	int ret;
	int i;

	digest = digestBuffer;
	digestType = 3;
	assertion = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*assertion));
	if (assertion == NULL) {
		ret = 2;
		goto out;
	}
	MlnMemset(assertion, 0, sizeof(*assertion));
	ret = MlnXmlParse(MLN_HEAP_GENERAL, token->size, token->data, &document);
	if (ret != 0)
		goto out;
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, document, "Assertion", MLN_SAML_NAMESPACE,
			      &assertions);
	if (ret != 0)
		goto out;
	if (assertions->count != 1)
		goto format;
	node = assertions->nodes[0];

	ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, node, NULL, "MajorVersion", &value);
	if (ret != 0)
		goto out;
	if (MlnBytesCompare(MLN_HEAP_GENERAL, value->size, value->data, 1, "1") != 0)
		goto format;
	MlnBufferDestroy(MLN_HEAP_GENERAL, value);
	value = NULL;
	ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, node, NULL, "MinorVersion", &value);
	if (ret != 0)
		goto out;
	if (MlnBytesCompare(MLN_HEAP_GENERAL, value->size, value->data, 1, "1") != 0)
		goto format;
	MlnBufferDestroy(MLN_HEAP_GENERAL, value);
	value = NULL;
	ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, node, NULL, "AssertionID", &value);
	if (ret != 0)
		goto out;
	assertion->assertionId = value;
	value = NULL;
	ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, node, NULL, "IssueInstant", &value);
	if (ret != 0)
		goto out;
	ret = MlnXmlParseDateTime(MLN_HEAP_GENERAL, value->size, (const char *)value->data,
				  &assertion->issueInstant);
	if (ret != 0)
		goto out;
	MlnBufferDestroy(MLN_HEAP_GENERAL, value);
	value = NULL;

	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, node, "Conditions", MLN_SAML_NAMESPACE, &list);
	if (ret == 0) {
		if (list->count != 1)
			goto format;
		conditions = list->nodes[0];
		ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, conditions, NULL, "NotBefore", &value);
		if (ret == 0) {
			assertion->notBefore = MlnAllocate(MLN_HEAP_GENERAL, sizeof(MlnDateTime));
			if (assertion->notBefore == NULL)
				goto nomem;
			ret = MlnXmlParseDateTime(MLN_HEAP_GENERAL, value->size,
						  (const char *)value->data, assertion->notBefore);
			if (ret != 0)
				goto out;
		} else if (ret != MLN_XML_ERROR_NOT_FOUND) {
			goto out;
		}
		MlnBufferDestroy(MLN_HEAP_GENERAL, value);
		value = NULL;
		ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, conditions, NULL, "NotOnOrAfter",
					 &value);
		if (ret == 0) {
			assertion->notOnOrAfter = MlnAllocate(MLN_HEAP_GENERAL, sizeof(MlnDateTime));
			if (assertion->notOnOrAfter == NULL)
				goto nomem;
			ret = MlnXmlParseDateTime(MLN_HEAP_GENERAL, value->size,
						  (const char *)value->data, assertion->notOnOrAfter);
			if (ret != 0)
				goto out;
		} else if (ret != MLN_XML_ERROR_NOT_FOUND) {
			goto out;
		}
		MlnBufferDestroy(MLN_HEAP_GENERAL, value);
		value = NULL;
	} else if (ret != MLN_XML_ERROR_NOT_FOUND) {
		goto out;
	}
	MlnXmlListFree(MLN_HEAP_GENERAL, list);
	list = NULL;

	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, node, "AttributeStatement/Attribute",
			      MLN_SAML_NAMESPACE, &list);
	if (ret == 0) {
		count = list->count;
		assertion->attributes = MlnImpl_8103B758(count);
		if (assertion->attributes == NULL)
			goto nomem;
		for (i = 0; i < count; i++) {
			MlnXmlNode *item = list->nodes[i];

			attribute = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*attribute));
			if (attribute == NULL) {
				ret = 2;
				goto out;
			}
			MlnMemset(attribute, 0, sizeof(*attribute));
			ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, item, NULL, "AttributeNamespace",
						 &value);
			if (ret != 0)
				goto out;
			ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, item, NULL, "AttributeName",
						 &name);
			if (ret != 0)
				goto out;
			/* the name is AttributeNamespace:AttributeName */
			attribute->name = MlnBufferCreate(MLN_HEAP_GENERAL,
							  value->size + name->size + 1, NULL);
			if (attribute->name == NULL) {
				ret = 2;
				goto out;
			}
			MlnMemcpy(attribute->name->data, value->data, value->size);
			attribute->name->data[value->size] = ':';
			MlnMemcpy(attribute->name->data + value->size + 1, name->data, name->size);
			MlnBufferDestroy(MLN_HEAP_GENERAL, value);
			value = NULL;
			MlnBufferDestroy(MLN_HEAP_GENERAL, name);
			name = NULL;

			ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, item, "AttributeValue",
					      MLN_SAML_NAMESPACE, &values);
			if (ret != 0)
				goto out;
			if (values->count != 1) {
				ret = MLN_OCTOPUS_ERROR_FORMAT;
				goto out;
			}
			ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, values->nodes[0], NULL,
						 "xsi:type", &value);
			if (ret == 0) {
				if (MlnBytesCompare(MLN_HEAP_GENERAL, value->size, value->data, 22,
						    "xsd:nonNegativeInteger") == 0) {
					MlnBufferDestroy(MLN_HEAP_GENERAL, value);
					value = NULL;
					ret = MlnXmlGetText(MLN_HEAP_GENERAL, values->nodes[0], &value);
					if (ret != 0)
						goto out;
					attribute->type = MLN_OCTOPUS_SAML_INTEGER;
					ret = MlnImpl_81009B50(MLN_HEAP_GENERAL, value->size, value->data,
							       (int *)&attribute->value.integer);
					if (ret != 0)
						goto out;
					goto next;
				}
				MlnBufferDestroy(MLN_HEAP_GENERAL, value);
				value = NULL;
				ret = MlnXmlGetText(MLN_HEAP_GENERAL, values->nodes[0], &value);
				if (ret != 0)
					goto out;
				attribute->type = MLN_OCTOPUS_SAML_STRING;
			} else if (ret == MLN_XML_ERROR_NOT_FOUND) {
				ret = MlnXmlGetText(MLN_HEAP_GENERAL, values->nodes[0], &value);
				if (ret == MLN_XML_ERROR_NOT_FOUND) {
					attribute->type = MLN_OCTOPUS_SAML_EMPTY;
					goto next;
				}
				if (ret != 0)
					goto out;
				attribute->type = MLN_OCTOPUS_SAML_STRING;
			} else {
				goto out;
			}
			attribute->value.string = MlnBufferCreate(MLN_HEAP_GENERAL, value->size,
								  value->data);
			if (attribute->value.string == NULL) {
				ret = 2;
				goto out;
			}
next:
			MlnBufferDestroy(MLN_HEAP_GENERAL, value);
			value = NULL;
			MlnXmlListFree(MLN_HEAP_GENERAL, values);
			values = NULL;
			assertion->attributes->items[i] = attribute;
		}
		attribute = NULL;
	} else if (ret != MLN_XML_ERROR_NOT_FOUND) {
		goto out;
	}
	MlnXmlListFree(MLN_HEAP_GENERAL, list);
	list = NULL;

	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, node, "AttributeStatement/Subject/NameIdentifier",
			      MLN_SAML_NAMESPACE, &list);
	if (ret == 0) {
		ret = MlnXmlGetText(MLN_HEAP_GENERAL, list->nodes[0], &assertion->nameIdentifier);
		if (ret != 0)
			goto out;
		MlnXmlListFree(MLN_HEAP_GENERAL, list);
		list = NULL;
	}
	if (!verify) {
		*out = assertion;
		assertion = NULL;
		goto out;
	}

	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, node, "Signature", MLN_XMLDSIG_NAMESPACE, &list);
	if (ret != 0)
		goto out;
	if (list->count != 1)
		goto format;
	signature = list->nodes[0];
	ret = MlnImpl_8105BF78(signature, &digestSize);
	if (ret != 0) {
		if (ret == 0x220A0006)
			ret = MLN_OCTOPUS_ERROR_FORMAT;
		goto out;
	}
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, signature, "SignedInfo/Reference/Transforms",
			      MLN_XMLDSIG_NAMESPACE, &values);
	if (ret != 0)
		goto out;
	if (values->count != 1)
		goto format;
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, values->nodes[0], "Transform",
			      MLN_XMLDSIG_NAMESPACE, &transforms);
	if (ret != 0)
		goto out;
	if (transforms->count != 2)
		goto format;
	ret = MlnImpl_81059558(node, transforms->nodes[0], &value);
	if (ret != 0)
		goto out;
	ret = MlnImpl_8103E4FC(value->data, value->size, digest, digestSize);
	if (ret != 0)
		goto out;
	MlnBufferDestroy(MLN_HEAP_GENERAL, value);
	value = NULL;
	MlnXmlListFree(MLN_HEAP_GENERAL, values);
	values = NULL;
	MlnXmlListFree(MLN_HEAP_GENERAL, transforms);
	transforms = NULL;

	if (type == 0) {
		usage = 1;
	} else {
		switch (type) {
		case 1:
			usage = 8;
			break;
		case 2:
			usage = 2;
			break;
		case 3:
			usage = 7;
			break;
		default:
			ret = 3;
			goto out;
		}
	}
	ret = MlnImpl_8105BA14(signature, usage, 1, &assertion->assertionId, &digest,
			       &digestType, 0, &verified);
	if (ret != 0)
		goto out;
	*out = assertion;
	assertion = NULL;
	goto out;
nomem:
	ret = 2;
	goto out;
format:
	ret = MLN_OCTOPUS_ERROR_FORMAT;
out:
	ret = MlnOctopusFormatError(ret);
	MlnBufferDestroy(MLN_HEAP_GENERAL, value);
	MlnBufferDestroy(MLN_HEAP_GENERAL, name);
	MlnImpl_8103B878(attribute);
	MlnXmlListFree(MLN_HEAP_GENERAL, assertions);
	MlnXmlListFree(MLN_HEAP_GENERAL, list);
	MlnXmlListFree(MLN_HEAP_GENERAL, values);
	MlnXmlListFree(MLN_HEAP_GENERAL, transforms);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, document);
	MlnImpl_8103CD0C(assertion);
	return ret;
}

/* whether a reference's one transform is Octopus's canonical byte sequence */
int MlnImpl_8103EF84(const MlnOctopusArray *transforms)
{
	const MlnBuffer *algorithm = transforms->items[0];
	unsigned int length = MlnStrlen(MLN_OCTOPUS_CBS);
	int ret;

	if (algorithm->size != length)
		return MLN_OCTOPUS_ERROR_FORMAT;
	ret = MLN_OCTOPUS_ERROR_FORMAT;
	if (MlnMemcmp(algorithm->data, MLN_OCTOPUS_CBS, length) == 0)
		ret = 0;
	return ret;
}

/* reads the SecretKeys, PublicKeys and PrivateKeys of ScubaKeys */
MLN_ALWAYS_INLINE int MlnOctopusScubaKeysParse(MlnXmlNode *node,
						MlnOctopusScubaKeys **out)
{
	MlnXmlNodeList *list = NULL;
	MlnOctopusArray *secretKeys = NULL;
	MlnOctopusArray *privateKeys = NULL;
	MlnOctopusArray *publicKeys = NULL;
	MlnOctopusScubaKeys *keys;
	MlnOctopusKey *key;
	MlnOctopusKeyPair *pair;
	int count;
	int ret;
	int i;

	if (MlnXmlFindNodes(MLN_HEAP_GENERAL, node, "SecretKey", MLN_OCTOPUS_NAMESPACE,
			    &list) == 0) {
		count = list->count;
		secretKeys = MlnImpl_8103B758(count);
		if (secretKeys == NULL) {
			ret = 2;
			goto out;
		}
		for (i = 0; i < count; i++) {
			ret = MlnImpl_8103DDB0(list->nodes[i], 0, (void **)&key);
			if (ret != 0)
				goto out;
			key->state = 5;
			secretKeys->items[i] = key;
		}
		MlnXmlListFree(MLN_HEAP_GENERAL, list);
		list = NULL;
	}
	if (MlnXmlFindNodes(MLN_HEAP_GENERAL, node, "PublicKey", MLN_OCTOPUS_NAMESPACE,
			    &list) == 0) {
		count = list->count;
		publicKeys = MlnImpl_8103B758(count);
		if (publicKeys == NULL) {
			ret = 2;
			goto out;
		}
		for (i = 0; i < count; i++) {
			ret = MlnImpl_8103DDB0(list->nodes[i], 1, (void **)&pair);
			if (ret != 0)
				goto out;
			pair->key.state = 5;
			publicKeys->items[i] = pair;
		}
		MlnXmlListFree(MLN_HEAP_GENERAL, list);
		list = NULL;
	}
	if (MlnXmlFindNodes(MLN_HEAP_GENERAL, node, "PrivateKey", MLN_OCTOPUS_NAMESPACE,
			    &list) == 0) {
		count = list->count;
		privateKeys = MlnImpl_8103B758(count);
		if (privateKeys == NULL) {
			ret = 2;
			goto out;
		}
		for (i = 0; i < count; i++) {
			ret = MlnImpl_8103DDB0(list->nodes[i], 1, (void **)&pair);
			if (ret != 0)
				goto out;
			privateKeys->items[i] = pair;
			pair->key.state = 5;
		}
		MlnXmlListFree(MLN_HEAP_GENERAL, list);
		list = NULL;
	}
	keys = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*keys));
	if (keys == NULL) {
		ret = 2;
		goto out;
	}
	keys->secretKeys = secretKeys;
	keys->privateKeys = privateKeys;
	keys->publicKeys = publicKeys;
	*out = keys;
	secretKeys = NULL;
	privateKeys = NULL;
	publicKeys = NULL;
	ret = 0;
out:
	MlnXmlListFree(MLN_HEAP_GENERAL, list);
	MlnImpl_8103C57C(secretKeys, MLN_OCTOPUS_KEY);
	MlnImpl_8103C57C(privateKeys, MLN_OCTOPUS_KEY_PAIR);
	MlnImpl_8103C57C(publicKeys, MLN_OCTOPUS_KEY_PAIR);
	return ret;
}

/* reads an Extension: its Digest of the extended object, or ScubaKeys */
int MlnImpl_8103EFDC(MlnXmlNode *node, MlnOctopusExtension **out)
{
	MlnOctopusExtension *extension = NULL;
	MlnOctopusScubaKeys *keys;
	MlnXmlNodeList *list = NULL;
	MlnOctopusArray *transforms = NULL;
	MlnBuffer *uid = NULL;
	MlnBuffer *subject = NULL;
	MlnBuffer *digestMethod = NULL;
	MlnBuffer *digestValue = NULL;
	MlnBuffer *value;
	int critical;
	int ret;

	ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, node, NULL, "uid", &uid);
	if (ret != 0)
		goto out;
	ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, node, NULL, "critical", &value);
	if (ret == 0) {
		critical = MlnBytesCompare(MLN_HEAP_GENERAL, value->size, value->data, 4,
					   "true") == 0;
		MlnBufferDestroy(MLN_HEAP_GENERAL, value);
	} else if (ret == 2) {
		goto nomem;
	} else {
		critical = 0;
	}
	if (MlnXmlGetAttribute(MLN_HEAP_GENERAL, node, NULL, "subject", &subject) == 2)
		goto nomem;
	/* Sony drops the subject here (and leaks it) */
	subject = NULL;

	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, node, "Digest", MLN_OCTOPUS_NAMESPACE, &list);
	if (ret == 0) {
		if (list->count != 1) {
			ret = MLN_OCTOPUS_ERROR_FORMAT;
			goto out;
		}
		ret = MlnImpl_8103D628(list->nodes[0], &transforms, &digestMethod, &digestValue);
		if (ret != 0)
			goto out;
		if (transforms != NULL) {
			if (transforms->count != 1) {
				ret = MLN_OCTOPUS_ERROR_FORMAT;
				goto out;
			}
			ret = MlnImpl_8103EF84(transforms);
			if (ret != 0)
				goto out;
		}
		MlnXmlListFree(MLN_HEAP_GENERAL, list);
		list = NULL;
	} else if (ret == 2) {
		goto nomem;
	}

	extension = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*extension));
	if (extension == NULL)
		goto nomem;
	extension->critical = critical;
	extension->type = MLN_OCTOPUS_EXTENSION_DIGEST;
	extension->subject = subject;
	extension->digestMethod = digestMethod;
	extension->digestValue = digestValue;
	extension->scubaKeys = NULL;
	extension->uid = uid;
	extension->transforms = transforms;
	uid = NULL;
	transforms = NULL;
	digestMethod = NULL;
	digestValue = NULL;
	subject = NULL;

	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, node, "ScubaKeys", MLN_OCTOPUS_NAMESPACE,
			      &list);
	if (ret == 0) {
		if (list->count != 1) {
			ret = MLN_OCTOPUS_ERROR_FORMAT;
			goto out;
		}
		extension->type = MLN_OCTOPUS_EXTENSION_SCUBA;
		ret = MlnOctopusScubaKeysParse(list->nodes[0], &keys);
		if (ret != 0)
			goto out;
		extension->scubaKeys = keys;
	} else if (ret == 2) {
		goto out;
	}
	*out = extension;
	extension = NULL;
	ret = 0;
	goto out;
nomem:
	extension = NULL;
	ret = 2;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, uid);
	MlnBufferDestroy(MLN_HEAP_GENERAL, subject);
	MlnBufferDestroy(MLN_HEAP_GENERAL, digestMethod);
	MlnBufferDestroy(MLN_HEAP_GENERAL, digestValue);
	MlnXmlListFree(MLN_HEAP_GENERAL, list);
	MlnImpl_8103C0B8(extension);
	MlnImpl_8103C57C(transforms, MLN_OCTOPUS_BUFFER);
	return ret;
}

/* reads the Extensions of an ExtensionList */
MLN_ALWAYS_INLINE int MlnOctopusExtensionListParse(MlnXmlNodeList *list, MlnOctopusArray **out)
{
	MlnOctopusExtension *extension = NULL;
	MlnOctopusArray *extensions = NULL;
	int ret;
	int i;

	if (list == NULL || list->count <= 0) {
		ret = 5;
		goto out;
	}
	extensions = MlnImpl_8103B758(list->count);
	if (extensions == NULL) {
		ret = 2;
		goto out;
	}
	for (i = 0; i < extensions->count; i++) {
		ret = MlnImpl_8103EFDC(list->nodes[i], &extension);
		if (ret != 0)
			goto out;
		extensions->items[i] = extension;
		extension = NULL;
	}
	*out = extensions;
	extensions = NULL;
	ret = 0;
out:
	MlnImpl_8103C57C(extensions, MLN_OCTOPUS_EXTENSION);
	return ret;
}

__attribute__((noinline)) int MlnImpl_8103F62C(MlnXmlNodeList *list, MlnOctopusArray **out)
{
	return MlnOctopusExtensionListParse(list, out);
}

/*
 * reads a bundle's Node: its uid, Id, attributes and extensions, of which
 * exactly one carries ScubaKeys holding public keys only
 */
MLN_ALWAYS_INLINE int MlnOctopusNodeParse(MlnXmlNode *document, MlnOctopusNode **out,
					  MlnBuffer **id)
{
	MlnXmlNodeList *list = NULL;
	MlnXmlNodeList *extensionList = NULL;
	MlnOctopusNode *node = NULL;
	MlnOctopusArray *attributes = NULL;
	MlnOctopusArray *extensions = NULL;
	MlnOctopusExtension *extension;
	MlnOctopusScubaKeys *keys;
	MlnBuffer *uid = NULL;
	MlnBuffer *nodeId = NULL;
	/* Sony's code frees three more buffers it never sets */
	MlnBuffer *spare1 = NULL;
	MlnBuffer *spare2 = NULL;
	MlnBuffer *spare3 = NULL;
	MlnXmlNode *item;
	int scubaKeys;
	int ret;
	int i;

	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, document, "/Bundle/Node", MLN_OCTOPUS_NAMESPACE,
			      &list);
	if (ret != 0)
		goto out;
	if (list->count != 1) {
		ret = MLN_OCTOPUS_ERROR_FORMAT;
		goto out;
	}
	item = list->nodes[0];
	ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, item, NULL, "uid", &uid);
	if (ret != 0)
		goto out;
	ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, item, NULL, "Id", &nodeId);
	if (ret != 0)
		goto out;
	ret = MlnImpl_8103DCBC(item, &attributes);
	if (ret != 0)
		goto out;
	if (MlnXmlFindNodes(MLN_HEAP_GENERAL, item, "ExtensionList/Extension",
			    MLN_OCTOPUS_NAMESPACE, &extensionList) == 0) {
		ret = MlnImpl_8103F62C(extensionList, &extensions);
		if (ret != 0)
			goto out;
		if (extensions->count <= 0) {
			ret = MLN_OCTOPUS_ERROR_FORMAT;
			goto out;
		}
		scubaKeys = 0;
		for (i = 0; i < extensions->count; i++) {
			extension = extensions->items[i];
			if (extension == NULL) {
				ret = 5;
				goto out;
			}
			if (extension->type == MLN_OCTOPUS_EXTENSION_SCUBA) {
				scubaKeys++;
				if (extension->digestValue == NULL) {
					ret = MLN_OCTOPUS_ERROR_FORMAT;
					goto out;
				}
				keys = extension->scubaKeys;
				if (keys->secretKeys != NULL || keys->privateKeys != NULL ||
				    keys->publicKeys == NULL) {
					ret = MLN_OCTOPUS_ERROR_FORMAT;
					goto out;
				}
			}
		}
		if (scubaKeys != 1) {
			ret = MLN_OCTOPUS_ERROR_FORMAT;
			goto out;
		}
	}
	node = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*node));
	if (node == NULL) {
		ret = 2;
		goto out;
	}
	node->uid = uid;
	node->attributes = attributes;
	node->extensions = extensions;
	node->scubaKeys = NULL;
	*id = nodeId;
	*out = node;
	attributes = NULL;
	uid = NULL;
	extensions = NULL;
	nodeId = NULL;
	ret = 0;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, nodeId);
	MlnBufferDestroy(MLN_HEAP_GENERAL, uid);
	MlnBufferDestroy(MLN_HEAP_GENERAL, spare1);
	MlnBufferDestroy(MLN_HEAP_GENERAL, spare2);
	MlnBufferDestroy(MLN_HEAP_GENERAL, spare3);
	MlnXmlListFree(MLN_HEAP_GENERAL, list);
	MlnXmlListFree(MLN_HEAP_GENERAL, extensionList);
	MlnImpl_8103C57C(attributes, MLN_OCTOPUS_ATTRIBUTE);
	MlnImpl_8103C57C(extensions, MLN_OCTOPUS_EXTENSION);
	return ret;
}

/* hashes data with the digest of the given size into digest */
MLN_ALWAYS_INLINE int MlnOctopusDigest(const MlnBuffer *data, int digestSize,
				       unsigned char *digest)
{
	if (data->data == NULL)
		return 3;
	if (digestSize == 20)
		return MlnImpl_810570A8(data->data, data->size, digest);
	if (digestSize == 32)
		return MlnImpl_8105713C(data->data, data->size, digest);
	if (digestSize != 0)
		return 3;
	return 0;
}

/*
 * reads a bundle's personality (type 0) or user (type 1) Node, and when
 * verify is set checks its ScubaKeys digests and its signatures
 */
int MlnImpl_8103F6F0(const MlnBuffer *token, MlnOctopusNode **out, int type, int verify)
{
	MlnXmlNode *document = NULL;
	MlnBuffer *id = NULL;
	MlnBuffer *data = NULL;
	MlnXmlNodeList *bundles = NULL;
	MlnXmlNodeList *signatures = NULL;
	unsigned char *digest;
	int digestType;
	int verifiedSignature;
	MlnBuffer expected;
	unsigned char digestBuffer[32];
	MlnOctopusNode *node = NULL;
	MlnOctopusArray *attributes;
	MlnOctopusAttribute *attribute;
	MlnOctopusExtension *extension;
	MlnXmlNode *signature;
	int digestSize;
	int verified;
	int usage;
	int count;
	int ret;
	int i;

	digest = digestBuffer;
	digestType = 4;
	if (type == 0) {
		usage = 19;
		expected.size = 11;
		expected.data = (unsigned char *)"personality";
	} else if (type == 1) {
		usage = 3;
		expected.size = 4;
		expected.data = (unsigned char *)"user";
	} else {
		ret = 3;
		goto out;
	}
	ret = MlnXmlParse(MLN_HEAP_GENERAL, token->size, token->data, &document);
	if (ret != 0)
		goto fail;
	ret = MlnOctopusNodeParse(document, &node, &id);
	if (ret != 0)
		goto fail;

	attributes = node->attributes;
	if (attributes == NULL)
		goto format;
	for (i = 0; i < node->attributes->count; i++) {
		attribute = node->attributes->items[i];
		if (MlnBytesCompare(MLN_HEAP_GENERAL, attribute->name->size, attribute->name->data,
				    35, "urn:marlin:core:node:attribute:type") == 0 &&
		    attribute->type == MLN_OCTOPUS_ATTRIBUTE_STRING &&
		    MlnBufferCompareKey(MLN_HEAP_GENERAL, attribute->value.string, &expected) == 0)
			break;
	}
	if (i == node->attributes->count)
		goto format;
	if (!verify)
		goto done;

	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, document, "/Bundle", MLN_OCTOPUS_NAMESPACE,
			      &bundles);
	if (ret != 0)
		goto fail;
	if (bundles->count != 1)
		goto format;
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, bundles->nodes[0], "Signature",
			      MLN_XMLDSIG_NAMESPACE, &signatures);
	MlnXmlListFree(MLN_HEAP_GENERAL, bundles);
	bundles = NULL;
	if (ret != 0) {
		if (ret == MLN_XML_ERROR_NOT_FOUND)
			goto unsigned_;
		goto fail;
	}

	/* each ScubaKeys extension's digest must be the digest of its keys */
	if (node->extensions != NULL) {
		count = node->extensions->count;
		for (i = 0; i < count; i++) {
			extension = node->extensions->items[i];
			if (extension->type != MLN_OCTOPUS_EXTENSION_SCUBA)
				continue;
			ret = MlnImpl_81061330(extension->scubaKeys, &data);
			if (ret != 0)
				goto fail;
			if (extension->digestMethod == NULL) {
				digestSize = -1;
				goto unsupported;
			}
			if (MlnBytesCompare(MLN_HEAP_GENERAL, 38,
					    "http://www.w3.org/2000/09/xmldsig#sha1",
					    extension->digestMethod->size,
					    extension->digestMethod->data) == 0) {
				digestSize = 20;
				if (data->data == NULL)
					goto unsupported;
				ret = MlnImpl_810570A8(data->data, data->size, digest);
			} else if (MlnBytesCompare(MLN_HEAP_GENERAL, 39,
						   "http://www.w3.org/2001/04/xmlenc#sha256",
						   extension->digestMethod->size,
						   extension->digestMethod->data) == 0) {
				digestSize = 32;
				if (data->data == NULL)
					goto unsupported;
				ret = MlnImpl_8105713C(data->data, data->size, digest);
			} else {
				digestSize = -1;
				goto unsupported;
			}
			if (ret != 0)
				goto fail;
			MlnBufferDestroy(MLN_HEAP_GENERAL, data);
			data = NULL;
			if (extension->digestValue == NULL ||
			    extension->digestValue->size != (unsigned int)digestSize ||
			    MlnMemcmp(digest, extension->digestValue->data,
				      extension->digestValue->size) != 0)
				goto format;
		}
	}

	/* one of the signatures must verify */
	ret = MlnImpl_810611BC(node, &data);
	if (ret != 0)
		goto fail;
	count = signatures->count;
	if (count <= 0)
		goto unsigned_;
	verifiedSignature = 0;
	for (i = 0; i < count; i++) {
		signature = signatures->nodes[i];
		ret = MlnImpl_8105BF78(signature, &digestSize);
		if (ret != 0) {
			if (ret == 0x220A0006)
				ret = MLN_OCTOPUS_ERROR_FORMAT;
			goto fail;
		}
		ret = MlnOctopusDigest(data, digestSize, digest);
		if (ret == 3)
			goto out;
		if (ret != 0)
			goto fail;
		ret = MlnImpl_8105BA14(signatures->nodes[i], usage, 1, &id, &digest, &digestType,
				       0, &verified);
		if (ret == 0)
			verifiedSignature = 1;
		else if (ret != 0x220A0003)
			goto fail;
	}
	if (!verifiedSignature)
		goto unsigned_;
done:
	*out = node;
	node = NULL;
	ret = 0;
	goto out;
unsupported:
	ret = 3;
	goto out;
unsigned_:
	ret = 0x230F0003;
	goto out;
format:
	ret = MLN_OCTOPUS_ERROR_FORMAT;
	goto out;
fail:
	ret = MlnOctopusFormatError(ret);
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, id);
	MlnBufferDestroy(MLN_HEAP_GENERAL, data);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, document);
	MlnXmlListFree(MLN_HEAP_GENERAL, bundles);
	MlnXmlListFree(MLN_HEAP_GENERAL, signatures);
	MlnImpl_8103B8F8(node);
	return ret;
}

/* reads a Control: its attributes, extensions and ControlProgram's CodeModule */
int MlnImpl_8103FF44(MlnXmlNode *node, MlnOctopusControl **out)
{
	MlnXmlNodeList *modules = NULL;
	MlnXmlNodeList *list = NULL;
	MlnBuffer *uid = NULL;
	MlnBuffer *protocol = NULL;
	MlnBuffer *type = NULL;
	MlnBuffer *codeModule = NULL;
	MlnBuffer *text = NULL;
	MlnOctopusArray *attributes = NULL;
	MlnOctopusArray *extensions = NULL;
	MlnOctopusControl *control;
	MlnXmlNode *program;
	MlnXmlNode *module;
	int ret;

	ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, node, NULL, "uid", &uid);
	if (ret != 0)
		goto out;
	ret = MlnOctopusAttributeListParse(node, &attributes);
	if (ret != 0)
		goto out;
	if (MlnXmlFindNodes(MLN_HEAP_GENERAL, node, "ExtensionList/Extension",
			    MLN_OCTOPUS_NAMESPACE, &list) == 0) {
		ret = MlnOctopusExtensionListParse(list, &extensions);
		if (ret != 0)
			goto out;
		MlnXmlListFree(MLN_HEAP_GENERAL, list);
		list = NULL;
	}
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, node, "ControlProgram", MLN_OCTOPUS_NAMESPACE,
			      &list);
	if (ret != 0)
		goto out;
	if (list->count != 1)
		goto format;
	program = list->nodes[0];
	ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, program, NULL, "protocol", &protocol);
	if (ret != 0)
		goto out;
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, program, "CodeModule", MLN_OCTOPUS_NAMESPACE,
			      &modules);
	if (ret != 0)
		goto out;
	if (modules->count != 1)
		goto format;
	MlnXmlListFree(MLN_HEAP_GENERAL, list);
	list = NULL;
	module = modules->nodes[0];
	ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, module, NULL, "type", &type);
	if (ret != 0)
		goto out;
	ret = MlnXmlGetText(MLN_HEAP_GENERAL, module, &text);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81026A9C(MLN_HEAP_GENERAL, text, &codeModule);
	if (ret != 0)
		goto out;
	control = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*control));
	if (control == NULL) {
		ret = 2;
		goto out;
	}
	control->extensions = extensions;
	extensions = NULL;
	control->attributes = attributes;
	attributes = NULL;
	control->uid = uid;
	control->signer = NULL;
	uid = NULL;
	control->protocol = protocol;
	protocol = NULL;
	control->type = type;
	control->codeModule = codeModule;
	type = NULL;
	codeModule = NULL;
	*out = control;
	goto out;
format:
	ret = MLN_OCTOPUS_ERROR_FORMAT;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, uid);
	MlnImpl_8103C57C(attributes, MLN_OCTOPUS_ATTRIBUTE);
	MlnImpl_8103C57C(extensions, MLN_OCTOPUS_EXTENSION);
	MlnBufferDestroy(MLN_HEAP_GENERAL, protocol);
	MlnBufferDestroy(MLN_HEAP_GENERAL, type);
	MlnBufferDestroy(MLN_HEAP_GENERAL, codeModule);
	MlnBufferDestroy(MLN_HEAP_GENERAL, text);
	MlnXmlListFree(MLN_HEAP_GENERAL, modules);
	MlnXmlListFree(MLN_HEAP_GENERAL, list);
	return ret;
}

int MlnImpl_81040368(MlnOctopusSignedBundle *signedBundle)
{
	int count;
	int i;

	if (signedBundle != NULL) {
		if (signedBundle->bundle != NULL) {
			count = signedBundle->bundle->contentKeyCount;
			MlnImpl_8103CAD8(signedBundle->bundle);
			if (signedBundle->signatures != NULL) {
				for (i = 0; i < count; i++) {
					MlnBufferDestroy(MLN_HEAP_GENERAL,
							 signedBundle->signatures[i].signedInfo);
					MlnBufferDestroy(MLN_HEAP_GENERAL,
							 signedBundle->signatures[i].signature);
				}
				MlnFree(MLN_HEAP_GENERAL, signedBundle->signatures);
			}
		}
		MlnFree(MLN_HEAP_GENERAL, signedBundle);
	}
	return 0;
}
