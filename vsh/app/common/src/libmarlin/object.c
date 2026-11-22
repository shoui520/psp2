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

/*
 * parses a record from data: a big-endian tag, item count and id length, the
 * id, then each item as a big-endian length and its bytes; *size is what it
 * took of data
 */
int MlnImpl_8103B4A0(const MlnBuffer *data, MlnRecord **out, unsigned int *size)
{
	MlnRecord *record;
	MlnRecordHeader *header;
	MlnRecordItem *item;
	MlnRecordItem *next;
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
		goto fail_items;
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
	if (record->header != NULL) {
		MlnFree(MLN_HEAP_GENERAL, record->header->id);
		MlnFree(MLN_HEAP_GENERAL, record->header);
	}
fail_items:
	for (item = record->first; item != NULL; item = next) {
		next = item->next;
		MlnBufferDestroy(MLN_HEAP_GENERAL, item->data);
		MlnFree(MLN_HEAP_GENERAL, item);
	}
	MlnFree(MLN_HEAP_GENERAL, record);
	ret = 2;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, buffer);
	MlnBufferDestroy(MLN_HEAP_GENERAL, id);
	return ret;
}

/* an empty array of count objects */
MlnOctopusArray *MlnImpl_8103B758(int count)
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

int MlnImpl_8103B878(MlnOctopusSamlAttribute *attribute)
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
int MlnImpl_8103C57C(MlnOctopusArray *array, int kind)
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

int MlnImpl_8103CDE0(MlnOctopusKey *key, MlnBuffer *uid, int usage, int format,
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

int MlnImpl_8103CE44(MlnOctopusKeyPair *pair, MlnBuffer *keyPair, MlnBuffer *uid, int usage,
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
