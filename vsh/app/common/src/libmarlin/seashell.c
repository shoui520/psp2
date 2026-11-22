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
int MlnImpl_8107AF0C(MlnSeaShellRecords *records, void *handle, const char *path,
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
	if (handle == NULL || path[0] == '/') {
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
	new->limit = 4096;
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
