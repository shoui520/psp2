/*
 * The platform's session id from the secure clock, and Octopus objects written as their
 * canonical byte sequences: an array as its count, big-endian, and then its items in their
 * sorted order (0x8105F2C4-). The file's name is ours.
 */
#include "mln_internal.h"

#define MLN_CANONICAL_ERROR_CLOCK	0x230E0001

/* 1970-01-01 00:00:00 in RTC ticks */
#define MLN_UNIX_EPOCH_TICK		62135596800000000ULL
#define MLN_SECOND_TICKS		1000000

/* an object sorted by its first or its second field */
typedef struct MlnCanonicalItem {
	const MlnBuffer *first;
	const MlnBuffer *second;
} MlnCanonicalItem;

/* a size, big-endian */
static inline int MlnCanonicalWriteSize(MlnTextBuffer *out, unsigned int size)
{
	unsigned char bytes[4];

	bytes[0] = size >> 24;
	bytes[1] = size >> 16;
	bytes[2] = size >> 8;
	bytes[3] = size;
	return MlnTextBufferAppend(MLN_HEAP_GENERAL, out, sizeof(bytes), bytes);
}

/* a buffer: its size, then its bytes */
static inline int MlnCanonicalWriteBuffer(MlnTextBuffer *out, const MlnBuffer *buffer)
{
	int ret;

	ret = MlnCanonicalWriteSize(out, buffer->size);
	if (ret != 0)
		return ret;
	return MlnTextBufferAppend(MLN_HEAP_GENERAL, out, buffer->size, buffer->data);
}

/* a buffer that must be given */
static inline int MlnCanonicalWriteData(MlnTextBuffer *out, const MlnBuffer *buffer)
{
	if (buffer == NULL || out == NULL)
		return 3;
	return MlnCanonicalWriteBuffer(out, buffer);
}

/* a byte sequence: its size, then its bytes */
static inline int MlnCanonicalWriteBytes(MlnTextBuffer *out, unsigned int size,
					 const void *data)
{
	int ret;

	ret = MlnCanonicalWriteSize(out, size);
	if (ret != 0)
		return ret;
	return MlnTextBufferAppend(MLN_HEAP_GENERAL, out, size, data);
}

/*
 * The platform's session: its id the secure clock in seconds since 1970-01-01, its flags
 * none.
 */
int MlnImpl_8105F2C4(int *id, unsigned int *flags)
{
	unsigned long long tick;

	if (sceDrmBridgeGetCurrentSecureTick(&tick) != 0)
		return MLN_CANONICAL_ERROR_CLOCK;
	tick = (tick - MLN_UNIX_EPOCH_TICK) / MLN_SECOND_TICKS;
	*flags = 0;
	*id = tick;
	return 0;
}

int MlnImpl_8105F334(void)
{
	return 0;
}

/* the orders of the arrays' items */
int MlnImpl_8105F33C(const void *a, const void *b)
{
	return MlnBufferCompare(MLN_HEAP_GENERAL, ((const MlnCanonicalItem *)a)->second,
				((const MlnCanonicalItem *)b)->second);
}

int MlnImpl_8105F358(const void *a, const void *b)
{
	return MlnBufferCompare(MLN_HEAP_GENERAL, ((const MlnCanonicalItem *)a)->first,
				((const MlnCanonicalItem *)b)->first);
}

int MlnImpl_8105F374(const void *a, const void *b)
{
	return MlnBufferCompare(MLN_HEAP_GENERAL, ((const MlnCanonicalItem *)a)->first,
				((const MlnCanonicalItem *)b)->first);
}

int MlnImpl_8105F390(const void *a, const void *b)
{
	return MlnBufferCompare(MLN_HEAP_GENERAL, ((const MlnCanonicalItem *)a)->first,
				((const MlnCanonicalItem *)b)->first);
}

/* an array: its count, then each item in the order compare sorts them */
static inline int MlnCanonicalWriteArray(const MlnOctopusArray *array,
					 MlnCanonicalCompare compare, MlnCanonicalWrite write,
					 MlnTextBuffer *out)
{
	unsigned char count[4];
	int *order;
	int n = array->count;
	int min;
	int ret;
	int t;
	int i;
	int j;

	count[0] = n >> 24;
	count[1] = n >> 16;
	count[2] = n >> 8;
	count[3] = n;
	ret = MlnTextBufferAppend(MLN_HEAP_GENERAL, out, sizeof(count), count);
	if (ret != 0)
		return ret;
	if (n <= 1)
		return write(array->items[0], out);

	order = MlnAllocate(MLN_HEAP_GENERAL, n * sizeof(int));
	if (order == NULL)
		return 2;
	for (i = 0; i < n; i++)
		order[i] = i;
	for (i = 0; i < n - 1; i++) {
		min = i;
		for (j = i + 1; j < n; j++) {
			if (compare(array->items[order[min]], array->items[order[j]]) > 0)
				min = j;
		}
		if (min != i) {
			t = order[min];
			order[min] = order[i];
			order[i] = t;
		}
		ret = write(array->items[order[i]], out);
		if (ret != 0)
			goto out;
	}
	ret = write(array->items[order[n - 1]], out);
out:
	MlnFree(MLN_HEAP_GENERAL, order);
	return ret;
}

/* write an array */
int MlnImpl_8105F3AC(const MlnOctopusArray *array, MlnCanonicalCompare compare,
		     MlnCanonicalWrite write, MlnTextBuffer *out)
{
	return MlnCanonicalWriteArray(array, compare, write, out);
}

/* write a byte sequence */
int MlnImpl_8105F5F4(const MlnBuffer *buffer, MlnTextBuffer *out)
{
	return MlnCanonicalWriteBuffer(out, buffer);
}

/* write a key: its usage, its format and its data */
__attribute__((noinline)) int MlnImpl_8105F658(const MlnOctopusKey *key, MlnTextBuffer *out)
{
	int ret;

	switch (key->usage) {
	case 0:
		ret = MlnCanonicalWriteBytes(out, 10, "KeySharing");
		break;
	case 1:
		ret = MlnCanonicalWriteBytes(out, 15, "Confidentiality");
		break;
	case 2:
		ret = MlnCanonicalWriteSize(out, 0);
		break;
	default:
		return 5;
	}
	if (ret != 0)
		return ret;
	switch (key->format) {
	case 0:
		ret = MlnCanonicalWriteBytes(out, 6, "PKCS#8");
		break;
	case 1:
		ret = MlnCanonicalWriteBytes(out, 8, "X509SPKI");
		break;
	case 2:
		ret = MlnCanonicalWriteBytes(out, 3, "RAW");
		break;
	default:
		return 5;
	}
	if (ret != 0)
		return ret;
	return MlnCanonicalWriteBuffer(out, key->data);
}

/* write a public key: its uid, the key and its pair's uid */
int MlnImpl_8105F8D4(const MlnOctopusKeyPair *pair, MlnTextBuffer *out)
{
	int ret;

	ret = MlnCanonicalWriteBuffer(out, pair->key.uid);
	if (ret != 0)
		return ret;
	ret = MlnImpl_8105F658(&pair->key, out);
	if (ret != 0)
		return ret;
	return MlnCanonicalWriteBuffer(out, pair->pair);
}

/* write a secret key: its uid and the key */
int MlnImpl_8105F9A4(const MlnOctopusKey *key, MlnTextBuffer *out)
{
	int ret;

	ret = MlnCanonicalWriteBuffer(out, key->uid);
	if (ret != 0)
		return ret;
	return MlnImpl_8105F658(key, out);
}

/* write a private key: its uid, the key and its pair's uid */
int MlnImpl_8105FA24(const MlnOctopusKeyPair *pair, MlnTextBuffer *out)
{
	int ret;

	ret = MlnCanonicalWriteBuffer(out, pair->key.uid);
	if (ret != 0)
		return ret;
	ret = MlnImpl_8105F658(&pair->key, out);
	if (ret != 0)
		return ret;
	return MlnCanonicalWriteBuffer(out, pair->pair);
}

/* write the private keys */
int MlnImpl_8105FAF4(const MlnOctopusArray *keys, MlnTextBuffer *out)
{
	return MlnCanonicalWriteArray(keys, MlnImpl_8105F358, (MlnCanonicalWrite)MlnImpl_8105FA24,
				      out);
}

/*
 * A digest: whether there is one, then its transforms, its method and its value (the
 * fields of the object that holds it).
 */
static inline int MlnCanonicalWriteDigest(MlnTextBuffer *out,
					  MlnOctopusArray *const *transformsField,
					  MlnBuffer *const *methodField,
					  MlnBuffer *const *valueField, unsigned char *flag)
{
	const MlnOctopusArray *transforms;
	int count;
	int ret;
	int i;

	if (*valueField == NULL) {
		*flag = 0;
		return MlnTextBufferAppend(MLN_HEAP_GENERAL, out, 1, flag);
	}
	*flag = 1;
	ret = MlnTextBufferAppend(MLN_HEAP_GENERAL, out, 1, flag);
	if (ret != 0)
		return ret;

	transforms = *transformsField;
	if (transforms != NULL) {
		count = transforms->count;
		ret = MlnCanonicalWriteSize(out, count);
		if (ret != 0)
			return ret;
		for (i = 0; i < count; i++) {
			ret = MlnCanonicalWriteBuffer(out, transforms->items[i]);
			if (ret != 0)
				return ret;
		}
	} else {
		ret = MlnCanonicalWriteSize(out, 0);
		if (ret != 0)
			return ret;
	}
	ret = MlnCanonicalWriteBuffer(out, *methodField);
	if (ret != 0)
		return ret;
	return MlnCanonicalWriteData(out, *valueField);
}

/* write a reference: its uid and its digest */
__attribute__((noinline)) int MlnImpl_8105FDE0(const MlnOctopusReference *reference,
					      MlnTextBuffer *out)
{
	unsigned char flag;
	int ret;

	ret = MlnCanonicalWriteBuffer(out, reference->uid);
	if (ret != 0)
		return ret;
	return MlnCanonicalWriteDigest(out, &reference->transforms, &reference->digestMethod,
				       &reference->digestValue, &flag);
}

/* write a reference */
int MlnImpl_81060028(const MlnOctopusReference *reference, MlnTextBuffer *out)
{
	return MlnImpl_8105FDE0(reference, out);
}

/* write a digest extension: its uid, whether it is critical and its digest */
__attribute__((noinline)) int MlnImpl_81060034(const MlnOctopusExtension *extension,
					      MlnTextBuffer *out)
{
	unsigned char flag;
	int ret;

	ret = MlnCanonicalWriteBuffer(out, extension->uid);
	if (ret != 0)
		return ret;
	flag = extension->critical != 0;
	ret = MlnTextBufferAppend(MLN_HEAP_GENERAL, out, sizeof(flag), &flag);
	if (ret != 0)
		return ret;
	return MlnCanonicalWriteDigest(out, &extension->transforms, &extension->digestMethod,
				       &extension->digestValue, &flag);
}

/* write an extension */
int MlnImpl_810602AC(const MlnOctopusExtension *extension, MlnTextBuffer *out)
{
	return MlnImpl_81060034(extension, out);
}

/* write the extensions */
int MlnImpl_810602B8(const MlnOctopusArray *extensions, MlnTextBuffer *out)
{
	return MlnCanonicalWriteArray(extensions, MlnImpl_8105F33C,
				      (MlnCanonicalWrite)MlnImpl_81060034, out);
}

/* whether an attribute has a name */
static inline int MlnCanonicalIsNamed(const MlnOctopusAttribute *attribute)
{
	const MlnBuffer *name = attribute->name;

	return name != NULL && name->size != 0 && name->data != NULL;
}

int MlnImpl_8106097C(const MlnOctopusArray *attributes, MlnTextBuffer *out);

/*
 * Write an attribute: whether it has a name and its name, its type's name and its value; a
 * list's attributes are named and sorted, an array's unnamed and in their order.
 */
__attribute__((noinline)) int MlnImpl_810604F4(const MlnOctopusAttribute *attribute,
					      MlnTextBuffer *out)
{
	const MlnOctopusArray *attributes;
	unsigned char flag;
	int count;
	int ret;
	int i;

	if (attribute->name != NULL) {
		flag = 1;
		ret = MlnTextBufferAppend(MLN_HEAP_GENERAL, out, sizeof(flag), &flag);
		if (ret != 0)
			return ret;
		ret = MlnCanonicalWriteBuffer(out, attribute->name);
		if (ret != 0)
			return ret;
	} else {
		flag = 0;
		ret = MlnTextBufferAppend(MLN_HEAP_GENERAL, out, sizeof(flag), &flag);
		if (ret != 0)
			return ret;
	}

	switch (attribute->type) {
	case MLN_OCTOPUS_ATTRIBUTE_INT:
		/* Sony's size leaves "int" of the name */
		ret = MlnCanonicalWriteBytes(out, 3, "int32_t");
		if (ret != 0)
			return ret;
		return MlnCanonicalWriteSize(out, attribute->value.integer);
	case MLN_OCTOPUS_ATTRIBUTE_STRING:
		ret = MlnCanonicalWriteBytes(out, 6, "string");
		if (ret != 0)
			return ret;
		return MlnCanonicalWriteBuffer(out, attribute->value.string);
	case MLN_OCTOPUS_ATTRIBUTE_BYTES:
		ret = MlnCanonicalWriteBytes(out, 5, "bytes");
		if (ret != 0)
			return ret;
		return MlnCanonicalWriteData(out, attribute->value.bytes);
	case MLN_OCTOPUS_ATTRIBUTE_LIST:
		attributes = attribute->value.attributes;
		for (i = 0; i < attributes->count; i++) {
			if (!MlnCanonicalIsNamed(attributes->items[i]))
				return 3;
		}
		ret = MlnCanonicalWriteBytes(out, 4, "list");
		if (ret != 0)
			return ret;
		return MlnImpl_8106097C(attribute->value.attributes, out);
	case MLN_OCTOPUS_ATTRIBUTE_ARRAY:
		attributes = attribute->value.attributes;
		for (i = 0; i < attributes->count; i++) {
			if (MlnCanonicalIsNamed(attributes->items[i]))
				return 3;
		}
		ret = MlnCanonicalWriteBytes(out, 5, "array");
		if (ret != 0)
			return ret;
		attributes = attribute->value.attributes;
		count = attributes->count;
		ret = MlnCanonicalWriteSize(out, count);
		if (ret != 0)
			return ret;
		for (i = 0; i < count; i++) {
			ret = MlnImpl_810604F4(attributes->items[i], out);
			if (ret != 0)
				return ret;
		}
		return 0;
	}
	return 0;
}

/* write an attribute */
int MlnImpl_81060970(const MlnOctopusAttribute *attribute, MlnTextBuffer *out)
{
	return MlnImpl_810604F4(attribute, out);
}

/* write a list's attributes, sorted by their names */
int MlnImpl_8106097C(const MlnOctopusArray *attributes, MlnTextBuffer *out)
{
	return MlnCanonicalWriteArray(attributes, MlnImpl_8105F358,
				      (MlnCanonicalWrite)MlnImpl_810604F4, out);
}
