/*
 * The key store (0x81070920-): the keys of a control's run, by uid. The keys
 * the HCI holds itself (its device keys) are named by the uids it lists; the
 * others are added from the bundle's ScubaKeys and content keys, and loaded
 * into the HCI on first use, their KeyData decrypted with the key their
 * EncryptedKey's KeyName names. The file's name is ours.
 */
#include "mln_internal.h"

#define MLN_KEY_ERROR_DUPLICATE		0x23040001	/* a key of that uid and name is stored */
#define MLN_KEY_ERROR_NOT_FOUND		0x23040002
#define MLN_KEY_ERROR_FAILED		0x23040003	/* the HCI failed */
#define MLN_KEY_ERROR_NOT_STORED	0x23040005	/* no such key to remove */
#define MLN_KEY_ERROR_SIZE		0x23040007	/* the result's size is not the buffer's */

/* the HCI's key kinds and the usages of a key it imports */
#define MLN_HCI_KEY_IMPORT		0	/* the data is the key */
#define MLN_HCI_KEY_OAEP		1	/* RSA-OAEP with the given parameters */
#define MLN_HCI_KEY_RSA			2	/* RSA without parameters */
#define MLN_HCI_DEVICE_KEY_AES		2
#define MLN_HCI_DEVICE_KEY_RSA		3
#define MLN_HCI_USAGE_WRAP		7

/* a key's format (MlnOctopusKey.format) */
#define MLN_KEY_FORMAT_X509SPKI		1

/* the encryption methods of a key's EncryptedData or EncryptedKey */
#define MLN_KEY_METHOD_RSA		1
#define MLN_KEY_METHOD_AES		2
#define MLN_KEY_METHOD_OAEP		4

/* the mask of a scrambled buffer: each byte picks the next byte's mask */
static const unsigned char MlnKeyMask[11] = {
	0xf9, 0xa3, 0x97, 0xa2, 0x55, 0xbe, 0xf1, 0x53, 0xfc, 0x6b, 0x79,
};

/* a key's data loaded into the HCI with the key that wraps it */
__attribute__((noinline))
unsigned int MlnImpl_81070920(int method, const MlnBuffer *oaepParams, const MlnBuffer *data,
			      void *wrapping, int usage, void **out)
{
	void *key = NULL;
	unsigned int ret;

	if (data == NULL || wrapping == NULL || out == NULL) {
		ret = 3;
		goto out;
	}
	if (method == MLN_KEY_METHOD_RSA) {
		ret = MlnImpl_81080E80(wrapping, MLN_HCI_KEY_RSA, NULL, data, usage, &key);
	} else if (method == MLN_KEY_METHOD_OAEP) {
		if (oaepParams == NULL) {
			ret = 3;
			goto out;
		}
		ret = MlnImpl_81080E80(wrapping, MLN_HCI_KEY_OAEP, oaepParams, data, usage, &key);
	} else {
		ret = MlnImpl_81080E80(wrapping, MLN_HCI_KEY_IMPORT, NULL, data, usage, &key);
	}
	if (ret != 0) {
		if (ret != 2)
			ret = MLN_KEY_ERROR_FAILED;
		goto out;
	}
	*out = key;
	key = NULL;
	ret = 0;
out:
	MlnImpl_81080C80(key);
	return ret;
}

/*
 * A key added: under its own uid, or a key pair's under the uid of its pair.
 * A second key of a uid goes behind the first, unless one there has its name.
 */
unsigned int MlnImpl_81070A54(MlnKeyStore *store, MlnOctopusKey *key, MlnBuffer *pair)
{
	MlnKeyStoreEntry **link;
	MlnKeyStoreEntry *entry;
	MlnKeyStoreEntry *created;
	MlnBuffer *uid = pair;
	MlnBuffer *name;
	MlnBuffer *other;
	int kind;

	if (pair != NULL) {
		kind = key->format == MLN_KEY_FORMAT_X509SPKI ? MLN_KEY_ENTRY_PUBLIC :
								 MLN_KEY_ENTRY_PRIVATE;
	} else {
		uid = key->uid;
		kind = MLN_KEY_ENTRY_SECRET;
	}
	link = &store->entries;
	for (entry = store->entries; entry != NULL; entry = entry->next) {
		if (MlnBufferCompare(MLN_HEAP_GENERAL, entry->uid, uid) == 0)
			break;
		link = &entry->next;
	}
	if (entry != NULL) {
		name = key->keyName;
		for (entry = *link; ; entry = entry->duplicate) {
			other = entry->key->keyName;
			if (other != NULL && name != NULL &&
			    MlnBufferCompare(MLN_HEAP_GENERAL, other, name) == 0)
				return MLN_KEY_ERROR_DUPLICATE;
			if (entry->duplicate == NULL)
				break;
		}
		link = &entry->duplicate;
	}
	created = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*created));
	if (created == NULL)
		return 2;
	created->duplicate = NULL;
	created->next = NULL;
	*link = created;
	created->uid = uid;
	created->kind = kind;
	created->key = key;
	return 0;
}

/* the entries freed, each with the keys behind it */
void MlnImpl_81070B60(MlnKeyStoreEntry *entry)
{
	MlnKeyStoreEntry *duplicate;
	MlnKeyStoreEntry *next;

	for (duplicate = entry->duplicate; duplicate != NULL; duplicate = next) {
		next = duplicate->duplicate;
		MlnFree(MLN_HEAP_GENERAL, duplicate);
	}
	next = entry->next;
	MlnFree(MLN_HEAP_GENERAL, entry);
	if (next != NULL)
		MlnImpl_81070B60(next);
}

/* one of the HCI's device keys, of the kind an encryption method uses */
static inline __attribute__((always_inline)) unsigned int MlnKeyStoreDeviceKey(int method,
									     void **out)
{
	void *key = NULL;
	unsigned int ret;

	if (method == MLN_KEY_METHOD_RSA) {
		ret = MlnImpl_81080C3C(MLN_HCI_DEVICE_KEY_RSA, &key);
	} else if (method == MLN_KEY_METHOD_AES || method == MLN_KEY_METHOD_OAEP) {
		ret = MlnImpl_81080C3C(MLN_HCI_DEVICE_KEY_AES, &key);
	} else {
		ret = MLN_KEY_ERROR_FAILED;
		goto out;
	}
	if (ret != 0) {
		if ((ret >> 16) != 0)
			ret = MLN_KEY_ERROR_FAILED;
		goto out;
	}
	*out = key;
	key = NULL;
out:
	MlnImpl_81080C80(key);
	return ret;
}

/*
 * The HCI key of a uid. A device key is made for the run (*temporary 0, no
 * slot). A stored key is loaded once: the key its KeyName names is found the
 * same way, and decrypts its EncryptedKey, then its KeyData, or its KeyData
 * alone (*temporary 1, *slot where the store keeps it). The first key of the
 * uid that loads is given.
 */
unsigned int MlnImpl_81070D6C(MlnKeyStore *store, const MlnBuffer *uid, int method,
			      int *temporary, void **key, void ***slot)
{
	int parentTemporary = 0;
	void *parent = NULL;
	void *wrapping = NULL;
	void *loaded = NULL;
	MlnKeyStoreEntry *entry;
	MlnKeyStoreEntry *candidate;
	MlnOctopusKey *stored;
	MlnBuffer **names;
	unsigned int count;
	unsigned int ret;
	unsigned int i;
	void *value;

	if (temporary == NULL || slot == NULL || key == NULL) {
		ret = 3;
		goto out;
	}
	count = store->deviceKeyCount;
	names = store->deviceKeys;
	if (uid == NULL || names == NULL) {
		ret = 3;
		goto out;
	}
	for (i = 0; i < count; i++)
		if (MlnBufferCompare(MLN_HEAP_GENERAL, uid, names[i]) == 0)
			break;
	if (i < count) {
		ret = MlnKeyStoreDeviceKey(method, &parent);
		if (ret != 0) {
			if (ret != 2)
				ret = MLN_KEY_ERROR_FAILED;
			goto out;
		}
		*temporary = 0;
		*slot = NULL;
		*key = parent;
		ret = 0;
		goto out;
	}

	*temporary = 1;
	for (entry = store->entries; entry != NULL; entry = entry->next)
		if (MlnBufferCompare(MLN_HEAP_GENERAL, entry->uid, uid) == 0)
			break;
	if (entry == NULL) {
		ret = MLN_KEY_ERROR_NOT_FOUND;
		goto out;
	}
	for (candidate = entry; ; candidate = candidate->duplicate) {
		if (MlnImpl_81080C74(candidate->key->key) != 0)
			goto have;
		ret = MlnImpl_81070D6C(store, candidate->key->keyName,
				       candidate->key->encryptionMethod, &parentTemporary, &parent,
				       slot);
		if (ret == 0)
			break;
		if ((ret >> 16) == 0)
			goto out;
		if (candidate->duplicate == NULL) {
			ret = MLN_KEY_ERROR_FAILED;
			goto out;
		}
	}
	stored = candidate->key;
	if (stored->encryptedKey != NULL) {
		ret = MlnImpl_81070920(stored->encryptionMethod, entry->key->oaepParams,
				       stored->encryptedKey, parent, stored->state, &wrapping);
		if (ret != 0)
			goto out;
		if (parentTemporary == 0) {
			MlnImpl_81080C80(parent);
			parent = NULL;
		}
		ret = MlnImpl_81070920(candidate->key->keyEncryptionMethod, entry->key->oaepParams,
				       candidate->key->data, wrapping, candidate->key->state,
				       &loaded);
		if (ret != 0)
			goto out;
	} else {
		ret = MlnImpl_81070920(stored->encryptionMethod, entry->key->oaepParams,
				       stored->data, parent, stored->state, &loaded);
		if (ret != 0)
			goto out;
		if (parentTemporary == 0) {
			MlnImpl_81080C80(parent);
			parent = NULL;
		}
	}
	MlnImpl_81080C80(candidate->key->key);
	value = loaded;
	loaded = NULL;
	candidate->key->key = NULL;
	candidate->key->key = value;
have:
	if (candidate->key == NULL) {
		*slot = NULL;
		*key = parent;
		ret = 0;
		goto out;
	}
	*slot = &candidate->key->key;
	*key = candidate->key->key;
	ret = 0;
out:
	MlnImpl_81080C80(wrapping);
	MlnImpl_81080C80(loaded);
	return ret;
}

/* the uids of the HCI's device keys freed */
static inline __attribute__((always_inline)) void MlnKeyStoreNamesFree(MlnBuffer ***names,
								       unsigned int *count)
{
	unsigned int i;

	if (*names == NULL)
		return;
	for (i = 0; i < *count; i++)
		MlnBufferDestroy(MLN_HEAP_GENERAL, (*names)[i]);
	MlnFree(MLN_HEAP_GENERAL, *names);
}

/* a key store, with the uids of the HCI's device keys */
unsigned int MlnImpl_810710F8(MlnKeyStore **out)
{
	void *handle = NULL;
	MlnBuffer **names = NULL;
	unsigned int count = 0;
	MlnKeyStore *store;
	unsigned int ret;

	store = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*store));
	if (store == NULL) {
		ret = 2;
		MlnImpl_81080C1C(handle);
		goto out;
	}
	MlnMemset(store, 0, sizeof(*store));
	ret = MlnImpl_81080BFC(&handle);
	if (ret == 0)
		ret = MlnImpl_81081204(&names, &count);
	if (ret != 0) {
		if ((ret >> 16) != 0)
			ret = MLN_KEY_ERROR_FAILED;
	} else if (count != 0 && names != NULL) {
		store->deviceKeys = names;
		store->deviceKeyCount = count;
		*out = store;
		names = NULL;
		count = 0;
		MlnImpl_81080C1C(handle);
		goto out;
	} else {
		ret = 1;
	}
	MlnImpl_81080C1C(handle);
	MlnImpl_81071278(store);
out:
	MlnKeyStoreNamesFree(&names, &count);
	return ret;
}

void MlnImpl_81071278(MlnKeyStore *store)
{
	unsigned int i;

	if (store == NULL)
		return;
	if (store->entries != NULL)
		MlnImpl_81070B60(store->entries);
	if (store->deviceKeys != NULL) {
		for (i = 0; i < store->deviceKeyCount; i++)
			MlnBufferDestroy(MLN_HEAP_GENERAL, store->deviceKeys[i]);
		MlnFree(MLN_HEAP_GENERAL, store->deviceKeys);
	}
	MlnFree(MLN_HEAP_GENERAL, store);
}

/* a secret key added, under its uid */
unsigned int MlnImpl_810712F0(MlnKeyStore *store, MlnOctopusKey *key)
{
	if (key == NULL || key->keyName == NULL)
		return 3;
	if (store == NULL)
		return 4;
	return MlnImpl_81070A54(store, key, NULL);
}

/* a public or private key added, under the uid of its pair */
unsigned int MlnImpl_8107132C(MlnKeyStore *store, MlnOctopusKeyPair *key)
{
	if (key == NULL || key->key.keyName == NULL)
		return 3;
	if (store == NULL)
		return 4;
	return MlnImpl_81070A54(store, &key->key, key->pair);
}

/*
 * A secret key removed, by its uid and name, its uid and data cleared. The
 * entry takes the place of the one behind it.
 */
unsigned int MlnImpl_81071368(MlnKeyStore *store, MlnOctopusKey *key)
{
	MlnKeyStoreEntry **link;
	MlnKeyStoreEntry *entry;
	MlnBuffer *uid;
	MlnBuffer *name;
	MlnBuffer *other;

	if (key == NULL || key->keyName == NULL)
		return 3;
	if (store == NULL)
		return 4;
	uid = key->uid;
	link = &store->entries;
	for (entry = store->entries; entry != NULL; entry = entry->next) {
		if (MlnBufferCompare(MLN_HEAP_GENERAL, entry->uid, uid) == 0)
			break;
		link = &entry->next;
	}
	if (entry == NULL)
		return MLN_KEY_ERROR_NOT_STORED;
	name = key->keyName;
	for (entry = *link; entry != NULL; entry = entry->duplicate) {
		other = entry->key->keyName;
		if (other != NULL && name != NULL &&
		    MlnBufferCompare(MLN_HEAP_GENERAL, other, name) == 0)
			break;
		link = &entry->duplicate;
	}
	if (entry == NULL)
		return MLN_KEY_ERROR_NOT_STORED;
	MlnMemset(key->uid->data, 0, key->uid->size);
	if (key->data != NULL)
		MlnMemset(key->data->data, 0, key->data->size);
	*link = entry->duplicate;
	MlnFree(MLN_HEAP_GENERAL, entry);
	return 0;
}

/*
 * A content key of a broadcast key block: the stored key of its uid is
 * replaced by the key the block gives for it.
 */
unsigned int MlnImpl_8107148C(MlnKeyStore *store, MlnBuffer *uid,
			      MlnOctopusBroadcastKeyBlock *block)
{
	MlnBuffer *keyData = NULL;
	void **slot = NULL;
	int temporary = 0;
	void *handle = NULL;
	void *key = NULL;
	void *loaded = NULL;
	unsigned int ret;

	if (uid == NULL)
		return 3;
	if (store == NULL)
		return 4;
	ret = MlnImpl_81080BFC(&handle);
	if (ret != 0)
		goto fail;
	ret = MlnImpl_81070D6C(store, uid, 0, &temporary, &key, &slot);
	if (ret != 0)
		goto out;
	if (slot == NULL) {
		ret = MLN_KEY_ERROR_NOT_FOUND;
		goto out;
	}
	if (block != NULL) {
		ret = MlnImpl_810810F4(key, &keyData);
		if (ret != 0)
			goto fail;
		if (keyData == NULL)
			goto out;
		ret = MlnImpl_8107DF18(block, keyData, &loaded);
		if (ret != 0) {
			if (ret != 2)
				ret = MLN_KEY_ERROR_FAILED;
			goto out;
		}
	}
	if (keyData == NULL)
		goto out;
	ret = MlnImpl_81080C80(*slot);
	if (ret != 0)
		goto fail;
	*slot = loaded;
	loaded = NULL;
	goto out;
fail:
	if ((ret >> 16) != 0)
		ret = MLN_KEY_ERROR_FAILED;
out:
	if (temporary == 0)
		MlnImpl_81080C80(key);
	MlnImpl_81080C80(loaded);
	MlnImpl_81080C1C(handle);
	MlnBufferDestroy(MLN_HEAP_GENERAL, keyData);
	return ret;
}

/*
 * A buffer wrapped with a device key: imported into the HCI as a key, which
 * then encrypts the message; the result, of the buffer's size, replaces the
 * buffer's data.
 */
unsigned int MlnImpl_810715FC(MlnKeyStore *store, MlnBuffer *keyName, unsigned char method,
			      MlnBuffer *message, MlnBuffer *buffer)
{
	MlnBuffer *result = NULL;
	void *handle = NULL;
	void *imported = NULL;
	void *key = NULL;
	unsigned int ret;

	if (store == NULL) {
		ret = 4;
		goto out;
	}
	if (keyName == NULL || message == NULL || buffer == NULL) {
		ret = 3;
		goto out;
	}
	ret = MlnImpl_81080BFC(&handle);
	if (ret != 0)
		goto fail;
	ret = MlnKeyStoreDeviceKey(method, &key);
	if (ret != 0) {
		if (ret != 2)
			ret = MLN_KEY_ERROR_FAILED;
		goto out;
	}
	ret = MlnImpl_81080E80(key, method == MLN_KEY_METHOD_RSA ? MLN_HCI_KEY_RSA :
								  MLN_HCI_KEY_IMPORT,
			       NULL, buffer, MLN_HCI_USAGE_WRAP, &imported);
	if (ret != 0)
		goto fail;
	ret = MlnImpl_8108117C(imported, message->size, message->data, &result);
	if (ret != 0)
		goto fail;
	if (result->size != buffer->size) {
		ret = MLN_KEY_ERROR_SIZE;
		goto out;
	}
	MlnMemcpy(buffer->data, result->data, result->size);
	goto out;
fail:
	if (ret != 2)
		ret = MLN_KEY_ERROR_FAILED;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, result);
	MlnImpl_81080C80(key);
	MlnImpl_81080C80(imported);
	MlnImpl_81080C1C(handle);
	return ret;
}

/* a buffer unmasked from its last byte: a byte's result picks the next byte's mask */
__attribute__((noinline))
void MlnImpl_810717EC(unsigned char *data, unsigned int size)
{
	unsigned int n = size - 1;
	unsigned char k = 0;
	unsigned int b;

#define MLN_KEY_UNMASK	do {			\
	b = data[n] ^ MlnKeyMask[k];		\
	data[n] = b;				\
	k = b % 11;				\
} while (0)

	switch (n % 11) {
	default:
		for (;;) {
			MLN_KEY_UNMASK;
			if (n == 0)
				return;
			n--;
	case 10:	MLN_KEY_UNMASK; n--;
	case 9:		MLN_KEY_UNMASK; n--;
	case 8:		MLN_KEY_UNMASK; n--;
	case 7:		MLN_KEY_UNMASK; n--;
	case 6:		MLN_KEY_UNMASK; n--;
	case 5:		MLN_KEY_UNMASK; n--;
	case 4:		MLN_KEY_UNMASK; n--;
	case 3:		MLN_KEY_UNMASK; n--;
	case 2:		MLN_KEY_UNMASK; n--;
	case 1:		MLN_KEY_UNMASK; n--;
		}
	}
#undef MLN_KEY_UNMASK
}

/* a buffer masked from its last byte: a byte's own value picks the next byte's mask */
__attribute__((noinline))
void MlnImpl_81071B3C(unsigned char *data, unsigned int size)
{
	unsigned int n = size - 1;
	unsigned char k = 0;
	unsigned int b;

#define MLN_KEY_MASK	do {			\
	b = data[n];				\
	data[n] = b ^ MlnKeyMask[k];		\
	k = b % 11;				\
} while (0)

	switch (n % 11) {
	default:
		for (;;) {
			MLN_KEY_MASK;
			if (n == 0)
				return;
			n--;
	case 10:	MLN_KEY_MASK; n--;
	case 9:		MLN_KEY_MASK; n--;
	case 8:		MLN_KEY_MASK; n--;
	case 7:		MLN_KEY_MASK; n--;
	case 6:		MLN_KEY_MASK; n--;
	case 5:		MLN_KEY_MASK; n--;
	case 4:		MLN_KEY_MASK; n--;
	case 3:		MLN_KEY_MASK; n--;
	case 2:		MLN_KEY_MASK; n--;
	case 1:		MLN_KEY_MASK; n--;
		}
	}
#undef MLN_KEY_MASK
}

/* a masked buffer replaced by its unmasked copy, the masked one cleared */
unsigned int MlnImpl_81071E8C(MlnBuffer **buffer)
{
	MlnBuffer *copy;
	MlnBuffer *spare = NULL;
	unsigned int ret;

	copy = MlnBufferCreate(MLN_HEAP_GENERAL, (*buffer)->size, (*buffer)->data);
	if (copy == NULL) {
		ret = 2;
		goto out;
	}
	MlnImpl_810717EC(copy->data, copy->size);
	if (*buffer != NULL) {
		MlnMemset((*buffer)->data, 0, (*buffer)->size);
		MlnBufferDestroy(MLN_HEAP_GENERAL, *buffer);
	}
	*buffer = copy;
	ret = 0;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, spare);
	return ret;
}

/* a masked copy of a buffer */
unsigned int MlnImpl_81071F00(const MlnBuffer *buffer, MlnBuffer **out)
{
	MlnBuffer *copy;
	MlnBuffer *spare = NULL;
	unsigned int ret;

	copy = MlnBufferCreate(MLN_HEAP_GENERAL, buffer->size, buffer->data);
	if (copy == NULL) {
		ret = 2;
		goto out;
	}
	ret = 0;
	MlnImpl_81071B3C(copy->data, copy->size);
	*out = copy;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, spare);
	return ret;
}

/* data of 32 bytes or more decrypted with a key, into a buffer */
unsigned int MlnImpl_81071F4C(void **key, const MlnBuffer *data, unsigned char *out,
			      unsigned int *size)
{
	MlnBuffer *result = NULL;
	unsigned int ret;

	if (key == NULL || data == NULL || data->size < 32 || out == NULL) {
		ret = 3;
		goto done;
	}
	ret = MlnImpl_81080D18(*key, 2, 1, NULL, NULL, data, &result);
	if (ret != 0) {
		if ((ret >> 16) != 0)
			ret = MLN_KEY_ERROR_FAILED;
		goto done;
	}
	MlnMemcpy(out, result->data, result->size);
	*size = result->size;
done:
	MlnBufferDestroy(MLN_HEAP_GENERAL, result);
	return ret;
}
