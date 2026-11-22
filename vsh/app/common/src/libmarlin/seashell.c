/*
 * SeaShell, the secure database (0x8107A4F8-): objects kept as an ASN.1 tree,
 * each with its attributes (a name, an owner, a type, flags and dates), the
 * paths to them, a database made anew and loaded, the expired objects pruned
 * and the tree encoded. The file's name is ours.
 */
#include "mln_internal.h"

/* a path opened, freed */
int MlnImpl_8107A4F8(MlnSeaShellHandle *handle)
{
	if (handle != NULL) {
		MlnFree(MLN_HEAP_GENERAL, handle->path);
		MlnFree(MLN_HEAP_GENERAL, handle);
	}
	return 0;
}

/* a big-endian word of the attributes' OCTET STRING */
#define MLN_SEASHELL_PUT_WORD(p, value) do {	\
	(p)[3] = (value);			\
	(p)[0] = (value) >> 24;			\
	(p)[1] = (value) >> 16;			\
	(p)[2] = (value) >> 8;			\
	(p) += 4;				\
} while (0)

/* an object's attributes as their SEQUENCE: the fields present, the name and the owner */
int MlnImpl_8107A524(const MlnSeaShellAttributes *attributes, MlnAsn1Node **out)
{
	MlnAsn1Node *sequence = NULL;
	MlnAsn1Node *node = NULL;
	unsigned char fields[2 + 1 + 4 * 4];
	unsigned char *p;
	unsigned char present;
	unsigned int value;
	int ret;

	if (attributes == NULL || out == NULL) {
		ret = 5;
		goto out;
	}
	ret = MlnImpl_81037E6C(MLN_HEAP_GENERAL, &sequence);
	if (ret != 0)
		goto out;
	p = &fields[2];
	present = 0;
	if (attributes->type != 0) {
		*p++ = attributes->type;
		present = MLN_SEASHELL_HAS_TYPE;
	}
	value = attributes->flags;
	if (value != 0) {
		MLN_SEASHELL_PUT_WORD(p, value);
		present |= MLN_SEASHELL_HAS_FLAGS;
	}
	value = attributes->creationDate;
	if (value != 0) {
		MLN_SEASHELL_PUT_WORD(p, value);
		present |= MLN_SEASHELL_HAS_CREATION_DATE;
	}
	value = attributes->modificationDate;
	if (value != 0) {
		MLN_SEASHELL_PUT_WORD(p, value);
		present |= MLN_SEASHELL_HAS_MODIFICATION_DATE;
	}
	value = attributes->expirationDate;
	if (value != 0) {
		MLN_SEASHELL_PUT_WORD(p, value);
		present |= MLN_SEASHELL_HAS_EXPIRATION_DATE;
	}
	fields[0] = 0;			/* the fields present, big-endian */
	fields[1] = present;
	ret = MlnImpl_81037D68(MLN_HEAP_GENERAL, p - fields, fields, &node);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81037948(MLN_HEAP_GENERAL, sequence, node);
	if (ret != 0)
		goto out;
	node = NULL;
	if (attributes->name.data == NULL) {
		ret = 5;
		goto out;
	}
	ret = MlnImpl_81037D68(MLN_HEAP_GENERAL, attributes->name.size, attributes->name.data,
			       &node);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81037948(MLN_HEAP_GENERAL, sequence, node);
	if (ret != 0)
		goto out;
	node = NULL;
	if (attributes->owner.data != NULL) {
		ret = MlnImpl_81037D68(MLN_HEAP_GENERAL, attributes->owner.size,
				       attributes->owner.data, &node);
		if (ret != 0)
			goto out;
		ret = MlnImpl_81037948(MLN_HEAP_GENERAL, sequence, node);
		if (ret != 0)
			goto out;
		node = NULL;
	}
	*out = sequence;
	sequence = NULL;
out:
	MlnImpl_81036FC8(MLN_HEAP_GENERAL, sequence);
	MlnImpl_81036FC8(MLN_HEAP_GENERAL, node);
	return ret;
}

/* a new object of a name and type, owned by Marlin's SeaShell service if asked */
int MlnImpl_8107A748(const char *name, unsigned int type, int marlin, MlnAsn1Node **out)
{
	MlnSeaShellAttributes attributes;
	MlnAsn1Node *object = NULL;
	MlnAsn1Node *node = NULL;
	int ret;

	if (out == NULL || name == NULL) {
		ret = 5;
		goto out;
	}
	ret = MlnImpl_81037E6C(MLN_HEAP_GENERAL, &object);
	if (ret != 0)
		goto out;
	MlnMemset(&attributes, 0, sizeof(attributes));
	attributes.name.data = (unsigned char *)name;
	attributes.name.size = MlnStrlen(name) + 1;
	if (marlin) {
		attributes.owner.size = MlnStrlen(MLN_SEASHELL_OWNER) + 1;
		attributes.owner.data = (unsigned char *)MLN_SEASHELL_OWNER;
	}
	attributes.type = type;
	ret = MlnImpl_8107A524(&attributes, &node);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81037948(MLN_HEAP_GENERAL, object, node);
	if (ret != 0)
		goto out;
	node = NULL;
	*out = object;
	object = NULL;
out:
	MlnImpl_81036FC8(MLN_HEAP_GENERAL, object);
	MlnImpl_81036FC8(MLN_HEAP_GENERAL, node);
	return ret;
}

/* an object given an empty container value, [1] SEQUENCE OF object */
int MlnImpl_8107A850(MlnAsn1Node *object)
{
	MlnAsn1Node *objects = NULL;
	int ret;

	if (object == NULL) {
		ret = 5;
		goto out;
	}
	ret = MlnImpl_81037E6C(MLN_HEAP_GENERAL, &objects);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81038470(MLN_HEAP_GENERAL, 1, objects);
	if (ret != 0)
		goto out;
	if (object->length > 1)
		ret = MlnImpl_81037A80(MLN_HEAP_GENERAL, object, 1, objects, NULL);
	else
		ret = MlnImpl_81037948(MLN_HEAP_GENERAL, object, objects);
	if (ret == 0)
		objects = NULL;
out:
	MlnImpl_81036FC8(MLN_HEAP_GENERAL, objects);
	return ret;
}

/* a big-endian word of the attributes' OCTET STRING */
#define MLN_SEASHELL_GET_WORD(p) \
	((unsigned int)(p)[0] << 24 | (unsigned int)(p)[1] << 16 | (unsigned int)(p)[2] << 8 | \
	 (unsigned int)(p)[3])

/* an object's attributes read; the name and owner point into the tree */
__attribute__((noinline))
int MlnImpl_8107A8F8(const MlnAsn1Node *object, MlnSeaShellAttributes *attributes)
{
	const MlnAsn1Node *sequence;
	const MlnAsn1Node *fields;
	const MlnAsn1Node *node;
	const unsigned char *p;
	const unsigned char *end;
	unsigned int present;

	MlnMemset(attributes, 0, sizeof(*attributes));
	if (object == NULL || !object->constructed || object->length == 0)
		return MLN_SEASHELL_ERROR_FORMAT;
	sequence = object->children[0];
	if (sequence == NULL || !sequence->constructed || sequence->length == 0)
		return MLN_SEASHELL_ERROR_FORMAT;
	fields = sequence->children[0];
	if (fields == NULL)
		return MLN_SEASHELL_ERROR_FORMAT;
	p = fields->data + 2;
	end = fields->data + fields->length;
	present = fields->data[0] << 8 | fields->data[1];	/* big-endian */
	if ((present & MLN_SEASHELL_HAS_TYPE) && end - p > 0)
		attributes->type = *p++;
	if ((present & MLN_SEASHELL_HAS_FLAGS) && end - p > 3) {
		attributes->flags = MLN_SEASHELL_GET_WORD(p);
		p += 4;
	}
	if ((present & MLN_SEASHELL_HAS_CREATION_DATE) && end - p > 3) {
		attributes->creationDate = MLN_SEASHELL_GET_WORD(p);
		p += 4;
	}
	if ((present & MLN_SEASHELL_HAS_MODIFICATION_DATE) && end - p > 3) {
		attributes->modificationDate = MLN_SEASHELL_GET_WORD(p);
		p += 4;
	}
	if ((present & MLN_SEASHELL_HAS_EXPIRATION_DATE) && end - p > 3)
		attributes->expirationDate = MLN_SEASHELL_GET_WORD(p);
	if (!sequence->constructed || sequence->length <= 1)
		return MLN_SEASHELL_ERROR_FORMAT;
	node = sequence->children[1];
	if (node == NULL)
		return MLN_SEASHELL_ERROR_FORMAT;
	attributes->name.size = node->length;
	attributes->name.data = node->data;
	if (sequence->constructed && sequence->length > 2) {
		node = sequence->children[2];
		if (node != NULL) {
			attributes->owner.size = node->length;
			attributes->owner.data = node->data;
		}
	}
	return 0;
}

/* an object's attributes, given both */
__attribute__((noinline))
int MlnImpl_8107AB30(const MlnAsn1Node *object, MlnSeaShellAttributes *attributes)
{
	if (object == NULL || attributes == NULL)
		return 5;
	return MlnImpl_8107A8F8(object, attributes);
}

/* whether a string starts with a prefix */
__attribute__((noinline))
int MlnImpl_8107AB50(const char *s, const char *prefix)
{
	int length = MlnStrlen(s);
	int prefixLength = MlnStrlen(prefix);

	return length >= prefixLength && MlnMemcmp(s, prefix, prefixLength) == 0;
}

/* the objects below a container that expired before a time (minutes) removed */
int MlnImpl_8107AB98(MlnAsn1Node *object, unsigned int now)
{
	MlnSeaShellAttributes attributes;
	MlnAsn1Node *objects;
	MlnAsn1Node *child;
	unsigned int i;
	int ret;

	if (object == NULL)
		return 5;
	ret = MlnImpl_8107A8F8(object, &attributes);
	if (ret != 0)
		return ret;
	if (attributes.type != MLN_SEASHELL_TYPE_CONTAINER)
		return MLN_SEASHELL_ERROR_PATH;
	objects = MlnImpl_81037B80(MLN_HEAP_GENERAL, object, 2, 1);	/* [1] */
	if (objects == NULL)
		return MLN_SEASHELL_ERROR_FORMAT;
	if (objects->length == 0 || !objects->constructed)
		return 0;
	child = objects->children[0];
	if (child == NULL)
		return 0;
	for (i = 0;;) {
		ret = MlnImpl_8107A8F8(child, &attributes);
		if (ret != 0)
			return ret;
		if (attributes.expirationDate != 0 && attributes.expirationDate < now) {
			ret = MlnImpl_810379E0(MLN_HEAP_GENERAL, objects, i--);
			if (ret != 0)
				return ret;
		} else if (attributes.type == MLN_SEASHELL_TYPE_CONTAINER) {
			ret = MlnImpl_8107AB98(child, now);
			if (ret != 0)
				return ret;
		}
		if (++i >= objects->length || !objects->constructed)
			return 0;
		child = objects->children[i];
		if (child == NULL)
			return 0;
	}
}

/* a database encoded, its expired objects removed first when the time is known */
int MlnImpl_8107ACD4(MlnSeaShellDatabase *database, unsigned int *size, unsigned char **data)
{
	unsigned char *encoding = NULL;
	unsigned int now = 0;
	unsigned int length;
	MlnAsn1Node *root;
	MlnAsn1Node *object;
	int mode;
	int ret;

	if (database == NULL) {
		ret = 4;
		goto out;
	}
	if (size == NULL || data == NULL) {
		ret = 3;
		goto out;
	}
	ret = MlnImpl_81052D84((int *)&now, &mode);
	if (ret != 0x220B0001) {
		if (ret != 0)
			goto out;
		root = database->root;
		if (root == NULL || !root->constructed || root->length <= 2 ||
		    (object = root->children[2]) == NULL) {
			ret = MLN_SEASHELL_ERROR_FORMAT;
			goto out;
		}
		ret = MlnImpl_8107AB98(object, now / 60);
		if (ret != 0)
			goto out;
	}
	ret = MlnImpl_810368D0(MLN_HEAP_GENERAL, database->root, &encoding, &length);
	if (ret == 0) {
		*size = length;
		*data = encoding;
		encoding = NULL;
	} else if (ret != 2) {
		ret = MLN_SEASHELL_ERROR_FORMAT;
	}
out:
	MlnFree(MLN_HEAP_GENERAL, encoding);
	return ret;
}

/* a path's components, at most MLN_SEASHELL_PATH_MAX, in a copy cut at its '/'s */
int MlnImpl_8107AE00(const char *path, char **copy, int *count, char **components)
{
	char *s = NULL;
	unsigned int size;
	int n;
	int i;
	int ret;

	if (path == NULL || copy == NULL || count == NULL || components == NULL) {
		ret = 3;
		goto out;
	}
	size = MlnStrlen(path) + 1;
	s = MlnAllocate(MLN_HEAP_GENERAL, size);
	if (s == NULL) {
		ret = 2;
		goto out;
	}
	MlnMemcpy(s, path, size);
	n = 0;
	for (i = 0; s[i] != '\0' && n < MLN_SEASHELL_PATH_MAX; i++) {
		if (s[i] == '/')
			s[i] = '\0';
		else if (i == 0 || s[i - 1] == '\0')
			components[n++] = &s[i];
	}
	*copy = s;
	s = NULL;
	ret = 0;
	*count = n;
out:
	MlnFree(MLN_HEAP_GENERAL, s);
	return ret;
}

/*
 * A path made absolute, under the path a handle opened when it is relative,
 * and its last component given apart when it names an attribute ("@Flags").
 */
int MlnImpl_8107AF0C(MlnSeaShellRecords *records, int handle, const char *path,
		     char **absolute, char **attribute)
{
	MlnSeaShellHandle *opened;
	char *full = NULL;
	char *name;
	unsigned int size;
	int length;
	int i;
	int ret;

	if (records == NULL) {
		ret = 4;
		goto out;
	}
	if (absolute == NULL || path == NULL) {
		ret = 3;
		goto out;
	}
	if (handle == 0 || path[0] == '/') {
		size = MlnStrlen(path) + 1;
		if (path[0] == '/') {
			full = MlnAllocate(MLN_HEAP_GENERAL, size);
			if (full == NULL) {
				ret = 2;
				goto out;
			}
			MlnMemcpy(full, path, size);
		} else {
			full = MlnAllocate(MLN_HEAP_GENERAL, size + 1);
			if (full == NULL) {
				ret = 2;
				goto out;
			}
			MlnMemset(full, 0, size + 1);
			full[0] = '/';
			MlnMemcpy(full + 1, path, size);
		}
	} else {
		opened = records->handles;
		if (opened == NULL)
			goto unknown;
		while (opened->handle != handle) {
			opened = opened->next;
			if (opened == NULL)
				goto unknown;
		}
		if (MlnStrlen(opened->path) > -3u - MlnStrlen(path)) {
			ret = 5;
			goto out;
		}
		size = MlnStrlen(opened->path) + MlnStrlen(path) + 2;
		full = MlnAllocate(MLN_HEAP_GENERAL, size);
		if (full == NULL) {
			ret = 2;
			goto out;
		}
		MlnMemset(full, 0, size);
		MlnStrncpy(full, opened->path, size);
		size -= MlnStrlen(opened->path);
		MlnStrncat(full, "/", size);
		MlnStrncat(full, path, size - 1);
	}
	name = NULL;
	length = MlnStrlen(full);
	if (length > 0)
		for (i = length - 1;; i--) {
			if (full[i] == '/') {
				if (full[i + 1] == '@') {
					full[i] = '\0';
					name = &full[i + 1];
				}
				break;
			}
			if (i == 0)
				break;
		}
	*absolute = full;
	full = NULL;
	ret = 0;
	*attribute = name;
	goto out;
unknown:
	ret = MLN_SEASHELL_ERROR_PATH;
out:
	MlnFree(MLN_HEAP_GENERAL, full);
	return ret;
}

/*
 * Whether an owner may use an object: the nearest of its ancestors (the path
 * to it, the object itself last) that has an owner is the owner's. With self,
 * an object of its own whose flags have bit 0 set is open to anyone.
 */
int MlnImpl_8107B144(MlnAsn1Node **ancestors, const MlnBuffer *owner, int index, int self)
{
	MlnSeaShellAttributes attributes;
	MlnAsn1Node *object;
	int i;
	int ret;

	if (ancestors == NULL || owner == NULL)
		return 5;
	if (owner->data == NULL || index < 0)
		return MLN_SEASHELL_ERROR_ACCESS;
	object = ancestors[index];
	if (object == NULL)
		return 5;
	i = index;
	for (;;) {
		ret = MlnImpl_8107A8F8(object, &attributes);
		if (ret != 0)
			break;
		if (attributes.owner.data != NULL &&
		    ((self && i == index && (attributes.flags & 1)) ||
		     MlnBytesCompare(MLN_HEAP_GENERAL, attributes.owner.size - 1,
				     attributes.owner.data, owner->size, owner->data) == 0))
			break;
		if (i-- == 0)
			return MLN_SEASHELL_ERROR_ACCESS;
		object = ancestors[i];
		if (object == NULL)
			return 5;
	}
	return ret;
}

/* an ASN.1 error of the tree's code as SeaShell's: 5 */
static inline __attribute__((always_inline)) int MlnSeaShellAsn1Error(int ret)
{
	if (ret >> 16 != 0)
		ret = 5;
	return ret;
}

/*
 * A database made anew, encoded: its version, its path and its one object, an
 * empty container named by the path's last component and owned by Marlin's
 * SeaShell service. Without a path, Marlin's database.
 */
int MlnImpl_8107B228(const char *path, unsigned int *size, unsigned char **data)
{
	MlnAsn1Node *root = NULL;
	MlnAsn1Node *node = NULL;
	const char *rest;
	const char *name;
	int pathLength;
	int length;
	int i;
	int ret;

	if (data == NULL || size == NULL) {
		ret = 3;
		goto out;
	}
	if (path != NULL) {
		pathLength = MlnStrlen(path);
		length = MlnStrlen(MLN_SEASHELL_ROOT);
		if (pathLength < length || MlnMemcmp(path, MLN_SEASHELL_ROOT, length) != 0)
			goto path;
		rest = &path[MlnStrlen(MLN_SEASHELL_ROOT)];
	} else {
		rest = MLN_SEASHELL_ROOT;
	}
	i = MlnStrlen(rest) - 1;
	if (i < 0)
		goto path;
	while (rest[i] != '/')
		if (i-- == 0)
			goto path;
	name = &rest[i + 1];
	if (name == NULL || MlnStrlen(name) == 0)
		goto path;
	ret = MlnImpl_81037E6C(MLN_HEAP_GENERAL, &root);
	if (ret != 0)
		goto asn1;
	ret = MlnImpl_81037BF4(MLN_HEAP_GENERAL, MLN_SEASHELL_VERSION, &node);
	if (ret != 0)
		goto asn1;
	ret = MlnImpl_81037948(MLN_HEAP_GENERAL, root, node);
	if (ret != 0)
		goto asn1;
	node = NULL;
	ret = MlnImpl_81037D68(MLN_HEAP_GENERAL, MlnStrlen(rest) + 1, rest, &node);
	if (ret != 0)
		goto asn1;
	ret = MlnImpl_81037948(MLN_HEAP_GENERAL, root, node);
	if (ret != 0)
		goto asn1;
	node = NULL;
	ret = MlnImpl_8107A748(name, MLN_SEASHELL_TYPE_CONTAINER, 1, &node);
	if (ret != 0)
		goto out;
	ret = MlnImpl_8107A850(node);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81037948(MLN_HEAP_GENERAL, root, node);
	if (ret != 0)
		goto asn1;
	node = NULL;
	ret = MlnImpl_810368D0(MLN_HEAP_GENERAL, root, data, size);
	if (ret == 0)
		goto out;
asn1:
	ret = MlnSeaShellAsn1Error(ret);
	goto out;
path:
	ret = MLN_SEASHELL_ERROR_PATH;
out:
	MlnImpl_81036FC8(MLN_HEAP_GENERAL, root);
	MlnImpl_81036FC8(MLN_HEAP_GENERAL, node);
	return ret;
}

/* the records made, empty */
int MlnImpl_8107B450(MlnSeaShellRecords **records)
{
	MlnSeaShellRecords *new;

	if (records == NULL)
		return 3;
	new = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*new));
	if (new == NULL)
		return 2;
	MlnMemset(new, 0, sizeof(*new));
	new->nextHandle = 4096;
	*records = new;
	return 0;
}

/* the records freed: every database loaded and every path opened */
int MlnImpl_8107B4A0(MlnSeaShellRecords *records)
{
	MlnSeaShellDatabase *database, *nextDatabase;
	MlnSeaShellHandle *handle, *nextHandle;

	if (records == NULL)
		return 4;
	for (database = records->databases; database != NULL; database = nextDatabase) {
		nextDatabase = database->next;
		MlnImpl_81036FC8(MLN_HEAP_GENERAL, database->root);
		MlnFree(MLN_HEAP_GENERAL, database->data);
		MlnFree(MLN_HEAP_GENERAL, database);
	}
	for (handle = records->handles; handle != NULL; handle = nextHandle) {
		nextHandle = handle->next;
		MlnImpl_8107A4F8(handle);
	}
	MlnFree(MLN_HEAP_GENERAL, records);
	return 0;
}

/*
 * A database loaded from its encoding, copied: its version Marlin's and its path a
 * string. The records hold one database.
 */
int MlnImpl_8107B534(MlnSeaShellRecords *records, unsigned int size, const unsigned char *data)
{
	MlnSeaShellDatabase *database;
	unsigned char *copy;
	const unsigned char *next;
	int version;
	const unsigned char *path;
	unsigned int length;
	int ret;

	if (records == NULL)
		return 4;
	if (records->databases != NULL)
		return MLN_SEASHELL_ERROR_VERSION;
	if (data == NULL || size == 0)
		return 3;
	database = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*database));
	if (database == NULL)
		return 2;
	MlnMemset(database, 0, sizeof(*database));
	database->size = size;
	copy = MlnAllocate(MLN_HEAP_GENERAL, size);
	if (copy == NULL) {
		ret = 2;
		goto fail;
	}
	MlnMemcpy(copy, data, size);
	database->data = copy;
	if (MlnImpl_810378E4(MLN_HEAP_GENERAL, copy, database->size, &next,
			     &database->root) != 0 ||
	    next - database->data != database->size)
		goto format;
	if (database->root == NULL) {
		ret = 3;
		goto fail;
	}
	if (MlnImpl_81037F54(MLN_HEAP_GENERAL, MlnX509Child(database->root, 0), &version) != 0)
		goto format;
	if (version != MLN_SEASHELL_VERSION) {
		ret = MLN_SEASHELL_ERROR_VERSION;
		goto fail;
	}
	database->version = version;
	if (database->root == NULL) {
		ret = 3;
		goto fail;
	}
	ret = MlnImpl_81038218(MLN_HEAP_GENERAL, MlnX509Child(database->root, 1), &path, &length);
	if (ret != 0 || length == 0 || path[length - 1] != '\0')
		goto format;
	database->path = (const char *)path;
	database->next = records->databases;
	records->databases = database;
	return 0;

format:
	ret = MLN_SEASHELL_ERROR_FORMAT;
fail:
	MlnImpl_81036FC8(MLN_HEAP_GENERAL, database->root);
	MlnFree(MLN_HEAP_GENERAL, database->data);
	MlnFree(MLN_HEAP_GENERAL, database);
	return ret;
}

/*
 * An object, encoded, added to the container at Marlin's "sne/l" path in the
 * database loaded: each component of the path below the database a container
 * among its parent's objects, found by name.
 */
int MlnImpl_8107B730(MlnSeaShellRecords *records, unsigned int size, const unsigned char *data)
{
	const unsigned char *next;
	char *componentsCopy = NULL;
	int count;
	MlnAsn1Node *object = NULL;
	MlnBuffer name;
	MlnSeaShellAttributes attributes;
	MlnSeaShellAttributes childAttributes;
	char *components[MLN_SEASHELL_PATH_MAX];
	MlnSeaShellDatabase *database;
	MlnAsn1Node *objects;
	MlnAsn1Node *node;
	MlnAsn1Node *child;
	unsigned char *copy;
	const char *path;
	unsigned int i;
	int j;
	int ret;

	if (records == NULL) {
		copy = NULL;
		ret = 4;
		goto out;
	}
	copy = MlnAllocate(MLN_HEAP_GENERAL, size);
	if (copy == NULL) {
		ret = 2;
		goto out;
	}
	MlnMemcpy(copy, data, size);
	if (MlnImpl_81037918(MLN_HEAP_GENERAL, copy, size, &next, &object) != 0 ||
	    next - copy != size)
		goto format;
	path = MLN_SEASHELL_SNE_LIST;
	if (!MlnImpl_8107AB50(path, MLN_SEASHELL_ROOT)) {
		ret = MLN_SEASHELL_ERROR_PATH;
		goto out;
	}
	path += MlnStrlen(MLN_SEASHELL_DATABASES);
	database = records->databases;
	if (database == NULL) {
		ret = MLN_SEASHELL_ERROR_DATABASE;
		goto out;
	}
	ret = MlnImpl_8107AE00(path, &componentsCopy, &count, components);
	if (ret != 0)
		goto out;
	node = MlnX509Child(database->root, 2);
	for (i = 1; i < count - 1; i++) {
		name.size = MlnStrlen(components[i]) + 1;
		name.data = (unsigned char *)components[i];
		if (node == NULL) {
			ret = 5;
			goto out;
		}
		ret = MlnImpl_8107A8F8(node, &attributes);
		if (ret != 0)
			goto out;
		if (attributes.type != MLN_SEASHELL_TYPE_CONTAINER)
			goto path;
		objects = MlnImpl_81037B80(MLN_HEAP_GENERAL, node, 2, 1 /* [1] */);
		if (objects == NULL) {
			ret = MLN_SEASHELL_ERROR_FORMAT;
			goto out;
		}
		for (j = 0; j < (int)objects->length; j++) {
			child = MlnX509Child(objects, j);
			if (child == NULL) {
				ret = 5;
				goto out;
			}
			ret = MlnImpl_8107A8F8(child, &childAttributes);
			if (ret != 0)
				goto out;
			if (MlnBufferCompare(MLN_HEAP_GENERAL, &childAttributes.name, &name) == 0)
				break;
		}
		if (objects->length == 0 || j >= (int)objects->length || j < 0)
			goto path;
		node = MlnX509Child(objects, j);
	}
	objects = MlnImpl_81037B80(MLN_HEAP_GENERAL, node, 2, 1 /* [1] */);
	if (objects == NULL)
		goto format;
	ret = MlnImpl_81037948(MLN_HEAP_GENERAL, objects, object);
	if (ret == 0)
		object = NULL;
	goto out;

format:
	ret = MLN_SEASHELL_ERROR_FORMAT;
	goto out;
path:
	ret = MLN_SEASHELL_ERROR_PATH;
out:
	MlnFree(MLN_HEAP_GENERAL, componentsCopy);
	MlnImpl_81036FC8(MLN_HEAP_GENERAL, object);
	MlnFree(MLN_HEAP_GENERAL, copy);
	return ret;
}

/* the database loaded unloaded, given encoded first when asked */
int MlnImpl_8107BA3C(MlnSeaShellRecords *records, int flags, unsigned int *size,
		     unsigned char **data)
{
	unsigned char *encoding = NULL;
	unsigned int length = 0;
	MlnSeaShellDatabase *database;
	int ret;

	if (records == NULL)
		return 4;
	if (flags != 0)
		return MLN_SEASHELL_ERROR_VERSION;
	database = records->databases;
	if (database == NULL)
		return MLN_SEASHELL_ERROR_DATABASE;
	if (size != NULL && data != NULL) {
		ret = MlnImpl_8107ACD4(database, &length, &encoding);
		if (ret != 0)
			return ret;
		*size = length;
		*data = encoding;
		encoding = NULL;
	}
	records->databases = database->next;
	MlnImpl_81036FC8(MLN_HEAP_GENERAL, database->root);
	MlnFree(MLN_HEAP_GENERAL, database->data);
	MlnFree(MLN_HEAP_GENERAL, database);
	return 0;
}

/* the databases changed, each given encoded with a copy of its path, marked unchanged */
int MlnImpl_8107BB04(MlnSeaShellRecords *records, int *count, MlnSeaShellExport **exports)
{
	MlnSeaShellDatabase *database;
	MlnSeaShellExport *array;
	MlnSeaShellExport *export;
	const char *name;
	unsigned int size;
	char *path;
	int n;
	int i;
	int ret;

	if (records == NULL)
		return 4;
	if (count == NULL || exports == NULL)
		return 3;
	n = 0;
	for (database = records->databases; database != NULL; database = database->next)
		if (database->modified)
			n++;
	array = NULL;
	if (n == 0)
		goto done;
	array = MlnAllocate(MLN_HEAP_GENERAL, n * sizeof(*array));
	if (array == NULL)
		return 2;
	MlnMemset(array, 0, n * sizeof(*array));
	i = 0;
	for (database = records->databases; database != NULL; database = database->next) {
		if (!database->modified)
			continue;
		export = &array[i++];
		ret = MlnImpl_8107ACD4(database, &export->size, &export->data);
		if (ret != 0)
			goto fail;
		name = database->path;
		if (name != NULL) {
			size = MlnStrlen(name) + 1;
			path = MlnAllocate(MLN_HEAP_GENERAL, size);
			if (path == NULL) {
				ret = 2;
				goto fail;
			}
			MlnMemcpy(path, name, size);
			export->path = path;
		} else {
			export->path = NULL;
		}
		database->modified = 0;
	}
done:
	*count = n;
	*exports = array;
	return 0;

fail:
	MlnImpl_8107BD34(records, n, array);
	MlnFree(MLN_HEAP_GENERAL, array);
	return ret;
}

/* a database given, freed */
int MlnImpl_8107BCE0(MlnSeaShellRecords *records, MlnSeaShellExport *export)
{
	if (records == NULL)
		return 4;
	if (export == NULL)
		return 3;
	MlnFree(MLN_HEAP_GENERAL, export->path);
	export->path = NULL;
	MlnFree(MLN_HEAP_GENERAL, export->data);
	export->data = NULL;
	return 0;
}

/* the databases given, freed */
int MlnImpl_8107BD34(MlnSeaShellRecords *records, int count, MlnSeaShellExport *exports)
{
	int i;

	if (records == NULL)
		return 4;
	if (exports == NULL)
		return 0;
	for (i = 0; i < count; i++)
		MlnImpl_8107BCE0(records, &exports[i]);
	MlnFree(MLN_HEAP_GENERAL, exports);
	return 0;
}

/* a handle for a container's path, a number from 4096 */
static inline int MlnSeaShellHandleOpen(MlnSeaShellRecords *records, const char *path,
					MlnSeaShellHandle **out)
{
	MlnSeaShellHandle *new = NULL;
	unsigned int size;
	char *copy;
	int ret;

	if (path == NULL) {
		ret = 5;
		goto out;
	}
	new = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*new));
	if (new == NULL) {
		ret = 2;
		goto out;
	}
	MlnMemset(new, 0, sizeof(*new));
	size = MlnStrlen(path) + 1;
	copy = MlnAllocate(MLN_HEAP_GENERAL, size);
	if (copy == NULL) {
		ret = 2;
		goto out;
	}
	MlnMemcpy(copy, path, size);
	new->path = copy;
	new->handle = records->nextHandle++;
	new->next = records->handles;
	records->handles = new;
	*out = new;
	new = NULL;
	ret = 0;
out:
	MlnImpl_8107A4F8(new);
	return ret;
}

/*
 * An object read, as the owner given (its signer): its attributes when the path's
 * last component names one ("@Name", "@Size", ...), the object at an index of a
 * container when it is a number, else its value. A container's value is a handle
 * for its path; the dates, flags, type and size are big-endian words.
 */
int MlnImpl_8107BDB8(MlnSeaShellRecords *records, const unsigned char *signer,
		     unsigned int signerSize, int handle, const char *path, unsigned char *buffer,
		     unsigned int size, int *type, unsigned int *outSize)
{
	char *absolute = NULL;
	char *attribute;
	char *componentsCopy = NULL;
	unsigned int count;
	unsigned int now;
	int mode;
	MlnBuffer owner;
	MlnBuffer name;
	MlnSeaShellAttributes attributes;
	MlnSeaShellAttributes childAttributes;
	char *components[MLN_SEASHELL_PATH_MAX];
	MlnAsn1Node *ancestors[MLN_SEASHELL_PATH_MAX + 1];
	unsigned char word[4];
	MlnSeaShellDatabase *database;
	MlnSeaShellHandle *opened;
	MlnAsn1Node *objects;
	MlnAsn1Node *node;
	MlnAsn1Node *child;
	const char *rest;
	const void *value;
	unsigned char *p;
	unsigned int length;
	unsigned int index;
	unsigned int number;
	unsigned int i;
	int prefixLength;
	int depth;
	int kind;
	int j;
	int ret;

	owner.size = signerSize;
	owner.data = (unsigned char *)signer;
	if (records == NULL) {
		ret = 4;
		goto out;
	}
	if (path == NULL || buffer == NULL) {
		ret = 3;
		goto out;
	}
	if (type == NULL || outSize == NULL) {
		ret = 3;
		goto out;
	}
	ret = MlnImpl_8107AF0C(records, handle, path, &absolute, &attribute);
	if (ret != 0)
		goto out;
	if (absolute == NULL) {
		ret = 4;
		goto out;
	}
	length = MlnStrlen(absolute);
	prefixLength = MlnStrlen(MLN_SEASHELL_ROOT);
	if ((int)length < prefixLength || MlnMemcmp(absolute, MLN_SEASHELL_ROOT, prefixLength) != 0) {
		ret = MLN_SEASHELL_ERROR_PATH;
		goto out;
	}
	rest = &absolute[MlnStrlen(MLN_SEASHELL_DATABASES)];
	database = records->databases;
	if (database == NULL) {
		ret = MLN_SEASHELL_ERROR_DATABASE;
		goto out;
	}
	ret = MlnImpl_8107AE00(rest, &componentsCopy, (int *)&count, components);
	if (ret != 0)
		goto out;
	node = MlnX509Child(database->root, 2);
	for (i = 1; i < count; i++) {
		name.size = MlnStrlen(components[i]) + 1;
		name.data = (unsigned char *)components[i];
		ancestors[i - 1] = node;
		if (node == NULL) {
			ret = 5;
			goto out;
		}
		ret = MlnImpl_8107A8F8(node, &attributes);
		if (ret != 0)
			goto out;
		if (attributes.type != MLN_SEASHELL_TYPE_CONTAINER) {
			ret = MLN_SEASHELL_ERROR_PATH;
			goto out;
		}
		objects = MlnImpl_81037B80(MLN_HEAP_GENERAL, node, 2, 1 /* [1] */);
		if (objects == NULL) {
			ret = MLN_SEASHELL_ERROR_FORMAT;
			goto out;
		}
		for (j = 0; j < (int)objects->length; j++) {
			child = MlnX509Child(objects, j);
			if (child == NULL) {
				ret = 5;
				goto out;
			}
			ret = MlnImpl_8107A8F8(child, &childAttributes);
			if (ret != 0)
				goto out;
			if (MlnBufferCompare(MLN_HEAP_GENERAL, &childAttributes.name, &name) == 0)
				break;
		}
		if (objects->length == 0 || j >= (int)objects->length || j < 0) {
			ret = MLN_SEASHELL_ERROR_PATH;
			goto out;
		}
		node = MlnX509Child(objects, j);
		if (node == NULL) {
			ret = 5;
			goto out;
		}
		ret = MlnImpl_8107A8F8(node, &attributes);
		if (ret != 0)
			goto out;
		if (attributes.expirationDate != 0) {
			now = 0;
			ret = MlnImpl_81052D84((int *)&now, &mode);
			if (ret == 0 && attributes.expirationDate < now / 60)
				ret = MLN_SEASHELL_ERROR_PATH;
			if (ret != 0)
				goto out;
		}
	}
	depth = i - 1;

	if (attribute != NULL) {
		ancestors[depth] = node;
		ret = MlnImpl_8107B144(ancestors, &owner, depth, 1);
		if (ret != 0)
			goto out;
		ret = MlnImpl_8107AB30(node, &childAttributes);
		if (ret != 0)
			goto out;
		if (MlnStrcmp(attribute, "@Name") == 0) {
			kind = 2;
			length = childAttributes.name.size;
			value = childAttributes.name.data;
			goto copy;
		}
		if (MlnStrcmp(attribute, "@Size") == 0) {
			switch (childAttributes.type) {
			case 0:
				index = 1;
				break;
			case 1:
				index = 2;
				break;
			case 2:
				index = 3;
				break;
			case 3:
				index = 4;
				break;
			default:
				ret = MLN_SEASHELL_ERROR_FORMAT;
				goto out;
			}
			objects = MlnImpl_81037B80(MLN_HEAP_GENERAL, node, 2, index);
			if (objects == NULL) {
				ret = MLN_SEASHELL_ERROR_FORMAT;
				goto out;
			}
			number = objects->length;
			goto attribute;
		}
		if (MlnStrcmp(attribute, "@Type") == 0) {
			number = childAttributes.type;
			goto attribute;
		}
		if (MlnStrcmp(attribute, "@Owner") == 0) {
			for (j = depth; j >= 0; j--) {
				ret = MlnImpl_8107AB30(ancestors[j], &childAttributes);
				if (ret != 0)
					goto out;
				if (childAttributes.owner.data != NULL) {
					kind = 2;
					length = childAttributes.owner.size;
					value = childAttributes.owner.data;
					goto copy;
				}
			}
			ret = 5;
			goto out;
		}
		if (MlnStrcmp(attribute, "@CreationDate") == 0) {
			number = childAttributes.creationDate;
			goto attribute;
		}
		if (MlnStrcmp(attribute, "@ModificationDate") == 0) {
			number = childAttributes.modificationDate;
			goto attribute;
		}
		if (MlnStrcmp(attribute, "@ExpirationDate") == 0) {
			number = childAttributes.expirationDate;
			goto attribute;
		}
		if (MlnStrcmp(attribute, "@Flags") == 0) {
			number = childAttributes.flags;
			goto attribute;
		}
		/* "@<n>": the container's object at n */
		index = MlnImpl_81052BD0(attribute + 1);
		if (childAttributes.type != MLN_SEASHELL_TYPE_CONTAINER) {
			ret = MLN_SEASHELL_ERROR_PATH;
			goto out;
		}
		objects = MlnImpl_81037B80(MLN_HEAP_GENERAL, node, 2, 1 /* [1] */);
		if (objects == NULL) {
			ret = MLN_SEASHELL_ERROR_FORMAT;
			goto out;
		}
		if (index >= objects->length) {
			ret = MLN_SEASHELL_ERROR_PATH;
			goto out;
		}
		node = objects->constructed ? objects->children[index] : NULL;
	}

	ancestors[depth] = node;
	ret = MlnImpl_8107B144(ancestors, &owner, depth, 1);
	if (ret != 0)
		goto out;
	ret = MlnImpl_8107AB30(node, &childAttributes);
	if (ret != 0)
		goto out;
	switch (childAttributes.type) {
	case MLN_SEASHELL_TYPE_CONTAINER:
		if (absolute == NULL) {
			ret = 5;
			goto out;
		}
		for (opened = records->handles; opened != NULL; opened = opened->next)
			if (MlnStrcmp(opened->path, absolute) == 0)
				break;
		if (opened == NULL) {
			ret = MlnSeaShellHandleOpen(records, absolute, &opened);
			if (ret != 0)
				goto out;
		}
		kind = 0;
		number = opened->handle;
		goto word;
	case 1:
		objects = MlnImpl_81037B80(MLN_HEAP_GENERAL, node, 2, 2);
		if (objects == NULL || objects->length != 4) {
			ret = MLN_SEASHELL_ERROR_FORMAT;
			goto out;
		}
		kind = 1;
		length = objects->length;
		value = objects->data;
		goto copy;
	case 2:
		objects = MlnImpl_81037B80(MLN_HEAP_GENERAL, node, 2, 3);
		if (objects == NULL) {
			ret = MLN_SEASHELL_ERROR_FORMAT;
			goto out;
		}
		kind = 2;
		length = objects->length;
		value = objects->data;
		goto copy;
	case 3:
		objects = MlnImpl_81037B80(MLN_HEAP_GENERAL, node, 2, 4);
		if (objects == NULL) {
			ret = MLN_SEASHELL_ERROR_FORMAT;
			goto out;
		}
		kind = 3;
		length = objects->length;
		value = objects->data;
		goto copy;
	default:
		ret = MLN_SEASHELL_ERROR_FORMAT;
		goto out;
	}

attribute:
	kind = 1;
word:
	p = word;
	MLN_SEASHELL_PUT_WORD(p, number);
	length = sizeof(word);
	value = word;
copy:
	*type = kind;
	*outSize = length;
	if (value != NULL) {
		if (length > size) {
			ret = MLN_SEASHELL_ERROR_BUFFER;
			goto out;
		}
		MlnMemcpy(buffer, value, length);
	}
out:
	MlnFree(MLN_HEAP_GENERAL, absolute);
	MlnFree(MLN_HEAP_GENERAL, componentsCopy);
	return ret;
}

/* the index of a child of a container, -1 when it is none of its */
static inline int MlnSeaShellChildIndex(const MlnAsn1Node *parent, const MlnAsn1Node *child)
{
	int i;

	for (i = 0; i != (int)parent->length; i++)
		if (parent->children[i] == child)
			return i;
	return -1;
}

/*
 * An object written, as the owner given (its signer): an attribute when the
 * path's last component names one (@Owner, @Flags and @ExpirationDate; the
 * others are read-only), else its value, set or made anew, or with no data the
 * object deleted and the handles below it closed. A container missing on the
 * way is made where the signer owns one of its ancestors. The database is marked
 * changed.
 */
int MlnImpl_8107C5F0(MlnSeaShellRecords *records, const unsigned char *signer,
		     unsigned int signerSize, int handle, const char *path,
		     const unsigned char *data, unsigned int size, int type)
{
	char *absolute = NULL;
	char *attribute;
	char *componentsCopy = NULL;
	unsigned int count;
	MlnAsn1Node *node;
	MlnAsn1Node *new = NULL;
	MlnBuffer owner;
	MlnBuffer name;
	MlnSeaShellAttributes attributes;
	MlnSeaShellAttributes childAttributes;
	MlnAsn1Node *ancestors[MLN_SEASHELL_PATH_MAX];
	char *components[MLN_SEASHELL_PATH_MAX];
	MlnSeaShellDatabase *database;
	MlnSeaShellHandle **link;
	MlnSeaShellHandle *opened;
	MlnSeaShellHandle *next;
	MlnAsn1Node *made = NULL;
	MlnAsn1Node *parent;
	MlnAsn1Node *child;
	const char *rest;
	unsigned int index;
	unsigned int i;
	int prefixLength;
	int length;
	int depth;
	int j;
	int ret;

	owner.size = signerSize;
	owner.data = (unsigned char *)signer;
	if (records == NULL) {
		ret = 4;
		goto out;
	}
	if (path == NULL) {
		ret = 3;
		goto out;
	}
	ret = MlnImpl_8107AF0C(records, handle, path, &absolute, &attribute);
	if (ret != 0)
		goto out;
	if (absolute == NULL) {
		ret = 4;
		goto out;
	}
	length = MlnStrlen(absolute);
	prefixLength = MlnStrlen(MLN_SEASHELL_ROOT);
	if (length < prefixLength || MlnMemcmp(absolute, MLN_SEASHELL_ROOT, prefixLength) != 0)
		goto path;
	rest = &absolute[MlnStrlen(MLN_SEASHELL_DATABASES)];
	database = records->databases;
	if (database == NULL) {
		ret = MLN_SEASHELL_ERROR_DATABASE;
		goto out;
	}
	ret = MlnImpl_8107AE00(rest, &componentsCopy, (int *)&count, components);
	if (ret != 0)
		goto out;
	if (count == 0) {
		ret = 5;
		goto out;
	}
	parent = database->root;
	database->modified = 1;
	node = MlnX509Child(parent, 2);
	if (count == 1) {
		depth = 0;
		if (attribute != NULL)
			goto attribute;
		if (parent == NULL) {
			ret = 5;
			goto out;
		}
		goto value;
	}
	for (i = 1;; i++) {
		depth = i;
		name.size = MlnStrlen(components[i]) + 1;
		name.data = (unsigned char *)components[i];
		if (node == NULL) {
			/* the container missing made, where the signer owns an ancestor */
			if (data == NULL)
				goto path;
			if (owner.data == NULL) {
				ret = MLN_SEASHELL_ERROR_ACCESS;
				goto out;
			}
			for (j = i - 2;; j--) {
				if (j < 0) {
					ret = MLN_SEASHELL_ERROR_ACCESS;
					goto out;
				}
				if (ancestors[j] == NULL) {
					ret = 5;
					goto out;
				}
				ret = MlnImpl_8107A8F8(ancestors[j], &childAttributes);
				if (ret != 0)
					goto out;
				if (childAttributes.owner.data != NULL &&
				    MlnBytesCompare(MLN_HEAP_GENERAL, childAttributes.owner.size - 1,
						    childAttributes.owner.data, owner.size,
						    owner.data) == 0)
					break;
			}
			ret = MlnImpl_8107A748(components[i - 1], MLN_SEASHELL_TYPE_CONTAINER, 0, &node);
			if (ret != 0)
				goto out;
			made = node;
			ret = MlnImpl_8107A850(made);
			if (ret != 0)
				goto out;
			ret = MlnImpl_81037948(MLN_HEAP_GENERAL, parent, made);
			if (ret != 0) {
				ret = MlnSeaShellAsn1Error(ret);
				goto out;
			}
			made = NULL;
		}
		ancestors[i - 1] = node;
		if (node == NULL) {
			ret = 5;
			goto out;
		}
		ret = MlnImpl_8107A8F8(node, &attributes);
		if (ret != 0)
			goto out;
		if (attributes.type != MLN_SEASHELL_TYPE_CONTAINER)
			goto path;
		parent = MlnImpl_81037B80(MLN_HEAP_GENERAL, node, 2, 1 /* [1] */);
		if (parent == NULL) {
			ret = MLN_SEASHELL_ERROR_FORMAT;
			goto out;
		}
		for (j = 0; j < (int)parent->length; j++) {
			child = MlnX509Child(parent, j);
			if (child == NULL) {
				ret = 5;
				goto out;
			}
			ret = MlnImpl_8107A8F8(child, &childAttributes);
			if (ret != 0)
				goto out;
			if (MlnBufferCompare(MLN_HEAP_GENERAL, &childAttributes.name, &name) == 0)
				break;
		}
		if (parent->length == 0 || j >= (int)parent->length || j < 0)
			node = NULL;
		else
			node = MlnX509Child(parent, j);
		if (i >= count - 1)
			break;
	}
	if (attribute == NULL)
		goto value;

attribute:
	ancestors[depth] = node;
	ret = MlnImpl_8107B144(ancestors, &owner, depth, 0);
	if (ret != 0)
		goto out;
	ret = MlnImpl_8107AB30(node, &childAttributes);
	if (ret != 0)
		goto out;
	if (MlnStrcmp(attribute, "@Owner") == 0) {
		if (MlnStrcmp(MLN_SEASHELL_ROOT, absolute) == 0) {
			ret = MLN_SEASHELL_ERROR_ACCESS;
			goto out;
		}
		childAttributes.owner.size = data != NULL ? size : 0;
		childAttributes.owner.data = (unsigned char *)data;
		ret = MlnImpl_8107A524(&childAttributes, &new);
		if (ret != 0)
			goto out;
		goto replace;
	}
	if (MlnStrcmp(attribute, "@Flags") == 0) {
		if (data == NULL) {
			childAttributes.flags = 0;
		} else {
			if (size != 4) {
				ret = 3;
				goto out;
			}
			childAttributes.flags = MLN_SEASHELL_GET_WORD(data);
		}
		goto attributes;
	}
	if (MlnStrcmp(attribute, "@ExpirationDate") == 0) {
		if (data == NULL) {
			childAttributes.expirationDate = 0;
		} else {
			if (size != 4) {
				ret = 3;
				goto out;
			}
			childAttributes.expirationDate = MLN_SEASHELL_GET_WORD(data);
		}
		goto attributes;
	}
	if (MlnStrcmp(attribute, "@Name") == 0 || MlnStrcmp(attribute, "@Size") == 0 ||
	    MlnStrcmp(attribute, "@Type") == 0 || MlnStrcmp(attribute, "@CreationDate") == 0 ||
	    MlnStrcmp(attribute, "@ModificationDate") == 0) {
		ret = MLN_SEASHELL_ERROR_ACCESS;
		goto out;
	}
	/* "@<n>": the container's object at n */
	index = MlnImpl_81052BD0(attribute + 1);
	if (childAttributes.type != MLN_SEASHELL_TYPE_CONTAINER)
		goto path;
	child = MlnImpl_81037B80(MLN_HEAP_GENERAL, node, 2, 1 /* [1] */);
	if (child == NULL) {
		ret = MLN_SEASHELL_ERROR_FORMAT;
		goto out;
	}
	if (index >= child->length)
		goto path;
	parent = child;
	node = parent->constructed ? parent->children[index] : NULL;

value:
	if (data != NULL && node != NULL) {
		/* the object's value set */
		ancestors[depth] = node;
		ret = MlnImpl_8107B144(ancestors, &owner, depth, 0);
		if (ret != 0)
			goto out;
		ret = MlnImpl_8107AB30(node, &childAttributes);
		if (ret != 0)
			goto out;
		if (childAttributes.type != type) {
			ret = MLN_SEASHELL_ERROR_TYPE;
			goto out;
		}
		switch (type) {
		case MLN_SEASHELL_TYPE_CONTAINER:
			goto out;
		case 1:
			if (size != 4) {
				ret = 3;
				goto out;
			}
			ret = MlnImpl_81086B18(MLN_SEASHELL_GET_WORD(data), node);
			goto out;
		case 2:
			if (data[size - 1] != '\0' || MlnStrlen((const char *)data) != size - 1) {
				ret = 3;
				goto out;
			}
			ret = MlnImpl_81086BE4(data, size, node);
			goto out;
		case 3:
			ret = MlnImpl_81086C9C(data, size, node);
			goto out;
		default:
			ret = 3;
			goto out;
		}
	}
	if (data != NULL) {
		/* the object made, of the path's last component */
		ret = MlnImpl_8107B144(ancestors, &owner, depth - 1, 0);
		if (ret != 0)
			goto out;
		switch (type) {
		case MLN_SEASHELL_TYPE_CONTAINER:
			if (size != 0) {
				ret = 3;
				goto out;
			}
			ret = MlnImpl_8107A748(components[count - 1], 0, 0, &new);
			if (ret != 0)
				goto out;
			ret = MlnImpl_8107A850(new);
			break;
		case 1:
			if (size != 4) {
				ret = 3;
				goto out;
			}
			ret = MlnImpl_8107A748(components[count - 1], 1, 0, &new);
			if (ret != 0)
				goto out;
			ret = MlnImpl_81086B18(MLN_SEASHELL_GET_WORD(data), new);
			break;
		case 2:
			if (data[size - 1] != '\0' || MlnStrlen((const char *)data) != size - 1) {
				ret = 3;
				goto out;
			}
			ret = MlnImpl_8107A748(components[count - 1], 2, 0, &new);
			if (ret != 0)
				goto out;
			ret = MlnImpl_81086BE4(data, size, new);
			break;
		case 3:
			ret = MlnImpl_8107A748(components[count - 1], 3, 0, &new);
			if (ret != 0)
				goto out;
			ret = MlnImpl_81086C9C(data, size, new);
			break;
		default:
			ret = 3;
			goto out;
		}
		if (ret != 0)
			goto out;
		ret = MlnImpl_81037948(MLN_HEAP_GENERAL, parent, new);
		if (ret == 0)
			new = NULL;
		goto out;
	}
	/* the object deleted and the handles at or below it closed */
	if (node == NULL)
		goto path;
	ret = MlnImpl_8107B144(ancestors, &owner, depth - 1, 0);
	if (ret != 0)
		goto out;
	j = MlnSeaShellChildIndex(parent, node);
	if (j < 0 || absolute == NULL) {
		ret = 5;
		goto out;
	}
	length = MlnStrlen(absolute);
	link = &records->handles;
	for (opened = *link; opened != NULL; opened = next) {
		if (opened->path != NULL && MlnImpl_8107AB50(opened->path, absolute) &&
		    (opened->path[length] == '\0' || opened->path[length] == '/')) {
			next = opened->next;
			MlnImpl_8107A4F8(opened);
			*link = next;
		} else {
			next = opened->next;
			link = &opened->next;
		}
	}
	ret = MlnImpl_810379E0(MLN_HEAP_GENERAL, parent, j);
	goto out;

attributes:
	ret = MlnImpl_8107A524(&childAttributes, &new);
	if (ret != 0)
		goto out;
replace:
	ret = MlnImpl_81037A80(MLN_HEAP_GENERAL, node, 0, new, NULL);
	if (ret == 0)
		new = NULL;
	goto out;
path:
	ret = MLN_SEASHELL_ERROR_PATH;
out:
	MlnImpl_81036FC8(MLN_HEAP_GENERAL, new);
	MlnImpl_81036FC8(MLN_HEAP_GENERAL, made);
	MlnFree(MLN_HEAP_GENERAL, absolute);
	MlnFree(MLN_HEAP_GENERAL, componentsCopy);
	return ret;
}
