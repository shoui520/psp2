/*
 * The license store: the core's state records by step, the links table, the
 * stored records and the digest the core keeps (0x810436e4-0x8104a444)
 */

#include "mln_internal.h"

/*
 * checks two ids against the stored nodes: 6 when either is a damaged record,
 * 0x230A0011 when either is missing, else the first id's result
 */
__attribute__((noinline)) int MlnImpl_810436E4(const MlnBuffer *first, const MlnBuffer *second)
{
	unsigned char digest[20];
	int firstRet = 0;
	int ret;

	if (second == NULL)
		return 5;
	MlnMemset(digest, 0, sizeof(digest));
	if (first != NULL) {
		ret = MlnImpl_810570A8(first->data, first->size, digest);
		if (ret != 0)
			return ret;
		firstRet = MlnImpl_81046F98(2, digest, NULL, NULL, NULL, 0);
	}
	ret = MlnImpl_810570A8(second->data, second->size, digest);
	if (ret != 0)
		return ret;
	ret = MlnImpl_81046F98(2, digest, NULL, NULL, NULL, 0);
	if (firstRet == 6 || ret == 6)
		return 6;
	if (firstRet == 0x230A0011 || ret == 0x230A0011)
		return 0x230A0011;
	if (firstRet != 0)
		return firstRet;
	return ret;
}

/*
 * deletes link's record, then the records of the links after it that are
 * damaged, unlinking those from links; 6 when the list is done
 */
__attribute__((noinline)) int MlnImpl_810437BC(MlnStoreLink **links, MlnStoreLink *link)
{
	MlnStoreLink *next;
	int ret;

	ret = MlnImpl_8105DC0C(7, link->key, NULL);
	if (ret != 0)
		return ret;
	link = link->next;
	while (link != NULL) {
		ret = MlnImpl_8105DEDC(7, link->key, NULL);
		if (ret == 6) {
			next = link->next;
			ret = MlnImpl_8105DC0C(7, link->key, NULL);
			if (ret != 0)
				return ret;
			MlnImpl_81049E34(links, link);
			link = next;
		} else if (ret == 1 || ret == 0) {
			link = link->next;
		} else {
			return ret;
		}
	}
	return 6;
}

/*
 * the record with id: from records when it is there (a broken entry there is
 * freed and gives 5), else the one stored in table 11, *stored then set
 */
__attribute__((noinline)) int MlnImpl_81043860(const MlnBuffer *id, const MlnRecordArray *records, MlnRecord **out,
		     int *stored)
{
	MlnRecord *record = NULL;
	MlnBuffer *buffer;
	MlnBuffer *data = NULL;
	int ret;
	int i;

	if (id == NULL || out == NULL) {
		ret = 3;
		goto out;
	}
	if (stored != NULL)
		*stored = 0;
	if (records != NULL && records->count > 0) {
		i = 0;
		for (;;) {
			record = records->records[i];
			if (record == NULL || record->header == NULL || record->header->id == NULL) {
				ret = 5;
				goto out;
			}
			i++;
			if (MlnBufferCompareKey(MLN_HEAP_GENERAL, record->header->id, id) == 0)
				break;
			record = NULL;
			if (i >= records->count)
				break;
		}
		if (record != NULL)
			goto found;
	}
	buffer = NULL;
	ret = MlnImpl_8105DEDC(11, 0, &buffer);
	if (ret != 0) {
		MlnBufferDestroy(MLN_HEAP_GENERAL, buffer);
		goto out;
	}
	data = buffer;
	buffer = NULL;
	MlnBufferDestroy(MLN_HEAP_GENERAL, buffer);
	ret = MlnImpl_8103D4AC(data, id, &record);
	if (ret != 0)
		goto out;
	if (record == NULL) {
		ret = 5;
		goto out;
	}
	if (stored != NULL)
		*stored = 1;
found:
	ret = 0;
	*out = record;
	record = NULL;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, data);
	MlnImpl_8103D5B8(record);
	return ret;
}

/* the digest kept in the shared state, made on first use */
__attribute__((noinline)) MlnStoreDigest *MlnImpl_81043A08(void)
{
	MlnShared *shared = MlnImpl_81006EAC();
	MlnStoreDigest *digest = shared->buffers[1];

	if (digest == NULL) {
		digest = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*digest));
		if (digest != NULL) {
			shared->buffers[1] = digest;
			MlnMemset(digest->bytes, 0, sizeof(digest->bytes));
			MlnMemset(&digest->buffer, 0, sizeof(digest->buffer));
		}
	}
	return digest;
}

unsigned int MlnImpl_81043A60(int kind, MlnBuffer **out)
{
	return MlnImpl_8105E060(kind, out);
}

unsigned int MlnImpl_81043A6C(const MlnBuffer *state)
{
	return MlnImpl_8105E5E4(state);
}

unsigned int MlnImpl_81043A78(void)
{
	return MlnImpl_8105EA2C();
}

/* the core's state tables by step: 1, 2, 3 */
#define MLN_STORE_STATE_TABLE(step, table)	\
	switch (step) {				\
	case 1:					\
		table = 3;			\
		break;				\
	case 2:					\
		table = 15;			\
		break;				\
	case 3:					\
		table = 17;			\
		break;				\
	default:				\
		return 3;			\
	}

/* stores a step's state */
int MlnImpl_81043A84(int step, unsigned int size, const void *data)
{
	MlnBuffer buffer;
	int table;

	if (data == NULL)
		return 3;
	MLN_STORE_STATE_TABLE(step, table);
	buffer.size = size;
	buffer.data = (void *)data;
	return MlnImpl_8105DC0C(table, 0, &buffer);
}

/* reads a step's state; a missing or damaged one marks the core broken */
int MlnImpl_81043AE4(int step, MlnBuffer **out)
{
	MlnBuffer *buffer = NULL;
	int table;
	int ret;

	MLN_STORE_STATE_TABLE(step, table);
	ret = MlnImpl_8105DEDC(table, 0, &buffer);
	if (ret == 0) {
		*out = buffer;
		return ret;
	}
	if (ret == 1) {
		ret = 0x230A0002;
		goto broken;
	}
	if (ret == 6 || ret == 0x230A0002) {
broken:
		MlnImpl_810485B4();
	}
	if (buffer != NULL)
		MlnBufferDestroy(MLN_HEAP_GENERAL, buffer);
	return ret;
}

/* the base node, from the first step's state */
unsigned int MlnImpl_81043B94(MlnOctopusNode **base)
{
	MlnOctopusNode *node = NULL;
	MlnBuffer *buffer = NULL;
	int ret;

	ret = MlnImpl_8105DEDC(3, 0, &buffer);
	if (ret == 0) {
		ret = MlnImpl_810429F8(buffer, 0, &node);
		if (ret == 0) {
			MlnBufferDestroy(MLN_HEAP_GENERAL, buffer);
			buffer = NULL;
			*base = node;
			node = NULL;
			goto out;
		}
		if (ret == 2)
			goto out;
		ret = 0x230A0004;
	} else if (ret == 1) {
		ret = 0x230A0002;
	} else if (ret != 6 && ret != 0x230A0002 && ret != 0x230A0004) {
		goto out;
	}
	MlnImpl_810485B4();
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, buffer);
	if (node != NULL)
		MlnImpl_8103B8F8(node);
	return ret;
}

/* whether a step's state is stored and sound */
unsigned int MlnImpl_81043C70(int step)
{
	int table;
	int ret;

	MLN_STORE_STATE_TABLE(step, table);
	ret = MlnImpl_8105DEDC(table, 0, NULL);
	if (ret == 1) {
		ret = 0x230A0002;
		goto broken;
	}
	if (ret == 6 || ret == 0x230A0002) {
broken:
		MlnImpl_810485B4();
	}
	return ret;
}

/* the saved state record (table 13); a damaged one is deleted */
unsigned int MlnImpl_81043CF0(MlnBuffer **state)
{
	MlnBuffer *buffer = NULL;
	int ret;

	ret = MlnImpl_8105DEDC(13, 0, &buffer);
	if (ret == 6) {
		buffer = NULL;
		MlnImpl_8105DC0C(13, 0, NULL);
		ret = 0x230A000E;
	} else if (ret == 1) {
		ret = 0x230A000E;
	} else if (ret == 0) {
		*state = buffer;
		buffer = NULL;
	}
	MlnBufferDestroy(MLN_HEAP_GENERAL, buffer);
	return ret;
}

unsigned int MlnImpl_81043D7C(const MlnBuffer *state)
{
	return MlnImpl_8105DC0C(13, 0, state);
}

/* whether the saved state record is there; a damaged one is deleted */
int MlnImpl_81043D94(void)
{
	int ret;

	ret = MlnImpl_8105DEDC(13, 0, NULL);
	if (ret == 6) {
		MlnImpl_8105DC0C(13, 0, NULL);
		return 0x230A000E;
	}
	if (ret == 1)
		return 0x230A000E;
	return ret;
}

/*
 * stores a link to to (from, the record, value1 and value2 as given) in the
 * links table and the link list; without values the link is keyed by the
 * core's digest when it has one
 */
int MlnImpl_81043DE0(const MlnBuffer *to, const MlnBuffer *from, const MlnBuffer *record,
		     const MlnBuffer *value1, const MlnBuffer *value2)
{
	MlnStoreLink *links = NULL;
	MlnStoreLink *loaded;
	const MlnBuffer *digest;
	unsigned int key = 0;
	int ret;

	if (value2 == NULL) {
		digest = MlnImpl_81047E34();
		if (digest == NULL) {
			ret = 2;
			goto out;
		}
		if (digest->size == 0 || digest->data == NULL)
			digest = NULL;
	} else {
		if (value1 == NULL) {
			ret = 5;
			goto out;
		}
		digest = NULL;
	}
	loaded = NULL;
	ret = MlnImpl_81049874(&loaded);
	if (ret == 6) {
		MlnImpl_810475F4(7);
		MlnImpl_81049918(loaded);
		goto out;
	}
	if (ret != 0) {
		MlnImpl_81049918(loaded);
		goto out;
	}
	links = loaded;
	loaded = NULL;
	MlnImpl_81049918(loaded);
	ret = MlnImpl_81049978(&links, from, to, digest, value1, value2, &key);
	if (ret != 0)
		goto out;
	ret = MlnImpl_8105DC0C(7, key, record);
	if (ret != 0)
		goto out;
	ret = MlnImpl_8104990C(links);
out:
	MlnImpl_81049918(links);
	return ret;
}

/*
 * removes the links whose record holds to: 0x230A0003 when none did, 6 when
 * the link list was damaged
 */
int MlnImpl_81043F1C(const MlnBuffer *to)
{
	MlnStoreLink *links = NULL;
	MlnStoreLink *loaded;
	MlnStoreLink *link;
	MlnBuffer *record = NULL;
	int removed;
	int damaged = 0;
	unsigned int key;
	int ret;

	if (to == NULL) {
		ret = 3;
		goto out;
	}
	removed = 0;
	for (key = 1; key != 8; key++) {
		ret = MlnImpl_8105DEDC(7, key, &record);
		if (ret == 6) {
			if (MlnImpl_81049F14(key) == 0)
				MlnImpl_8105DC0C(7, key, NULL);
			goto done;
		}
		if (ret != 1 && ret != 0)
			goto done;
		if (record != NULL && MlnBufferCompare(MLN_HEAP_GENERAL, record, to) == 0) {
			loaded = NULL;
			ret = MlnImpl_81049874(&loaded);
			if (ret == 6) {
				MlnImpl_810475F4(7);
				MlnImpl_81049918(loaded);
				goto done;
			}
			if (ret != 0) {
				MlnImpl_81049918(loaded);
				goto done;
			}
			links = loaded;
			loaded = NULL;
			MlnImpl_81049918(loaded);
			for (link = links; link != NULL; link = link->next)
				if (link->key == key)
					break;
			if (link != NULL) {
				ret = MlnImpl_81049E34(&links, link);
				if (ret == 6)
					damaged = 1;
				else if (ret != 0)
					goto done;
				ret = MlnImpl_8105DC0C(7, key, NULL);
				if (ret != 0)
					goto done;
				ret = MlnImpl_8104990C(links);
				if (ret != 0)
					goto done;
				removed = 1;
			}
			MlnImpl_81049918(links);
			links = NULL;
		}
		MlnBufferDestroy(MLN_HEAP_GENERAL, record);
		record = NULL;
	}
	ret = removed ? 0 : 0x230A0003;
done:
	if (damaged)
		ret = 6;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, record);
	MlnImpl_81049918(links);
	return ret;
}

/*
 * the link list with its damaged or dangling links dropped (6 when any was,
 * the list then saved), and its length; 0x230A0012 past 8 links
 */
unsigned int MlnImpl_8104411C(int *count, MlnStoreLink **out)
{
	MlnStoreLink *links = NULL;
	MlnStoreLink *loaded;
	MlnStoreLink *link;
	MlnStoreLink *next;
	int damaged;
	int n;
	int ret;

	if (count == NULL || out == NULL) {
		ret = 5;
		goto out;
	}
	loaded = NULL;
	ret = MlnImpl_81049874(&loaded);
	if (ret == 6) {
		MlnImpl_810475F4(7);
		MlnImpl_81049918(loaded);
		goto out;
	}
	if (ret != 0) {
		MlnImpl_81049918(loaded);
		goto out;
	}
	links = loaded;
	loaded = NULL;
	MlnImpl_81049918(loaded);
	n = 0;
	damaged = 0;
	link = links;
	while (link != NULL) {
		ret = MlnImpl_8105DEDC(7, link->key, NULL);
		if (ret == 1 || ret == 6)
			ret = MlnImpl_810437BC(&links, link);
		else if (ret == 0 && link->byValue == 1)
			ret = MlnImpl_810436E4(link->from, link->to);
		if (ret == 6) {
			next = link->next;
			damaged = 1;
			MlnImpl_81049E34(&links, link);
			link = next;
		} else if (ret == 0x230A0011) {
			next = link->next;
			MlnImpl_81049E34(&links, link);
			link = next;
		} else if (ret == 0) {
			link = link->next;
			n++;
		}
	}
	if (damaged == 1) {
		ret = MlnImpl_8104990C(links);
		if (ret == 0)
			ret = 6;
		goto out;
	}
	if (n > 8) {
		ret = 0x230A0012;
		goto out;
	}
	*count = n;
	*out = links;
	links = NULL;
out:
	MlnImpl_81049918(links);
	return ret;
}

/* takes the first link off links and reads its record */
unsigned int MlnImpl_810442D4(MlnStoreLink **links, MlnBuffer **record)
{
	MlnStoreLink *link = *links;
	MlnBuffer *buffer = NULL;
	int ret;

	if (link == NULL) {
		ret = 5;
		goto out;
	}
	ret = MlnImpl_8105DEDC(7, link->key, &buffer);
	if (ret != 0) {
		ret = 5;
		goto out;
	}
	*links = link->next;
	link->next = NULL;
	MlnImpl_81049918(link);
	*record = buffer;
	buffer = NULL;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, buffer);
	return ret;
}

int MlnImpl_8104435C(MlnStoreLink *links)
{
	MlnImpl_81049918(links);
	return 0;
}

/* the record of the link with a 20-byte digest; a damaged one gives 6 */
unsigned int MlnImpl_8104436C(const MlnBuffer *digest, MlnBuffer **record)
{
	MlnStoreLink *links = NULL;
	MlnStoreLink *found = NULL;
	MlnStoreLink *loaded;
	MlnBuffer *buffer;
	unsigned int key;
	int ret;

	if (digest == NULL || digest->size != 20 || digest->data == NULL || record == NULL) {
		ret = 3;
		goto out;
	}
	loaded = NULL;
	ret = MlnImpl_81049874(&loaded);
	if (ret == 6) {
		MlnImpl_810475F4(7);
		MlnImpl_81049918(loaded);
		goto out;
	}
	if (ret != 0) {
		MlnImpl_81049918(loaded);
		goto out;
	}
	links = loaded;
	loaded = NULL;
	MlnImpl_81049918(loaded);
	ret = MlnImpl_81049D54(links, digest, &found);
	if (ret != 0)
		goto out;
	key = found->key;
	buffer = NULL;
	if (key > 8) {
		ret = 5;
	} else {
		ret = MlnImpl_8105DEDC(7, key, &buffer);
		if (ret == 1 || ret == 6) {
			if (MlnImpl_81049F14(key) == 0)
				MlnImpl_8105DC0C(7, key, NULL);
			ret = 6;
		} else if (ret == 0) {
			*record = buffer;
			buffer = NULL;
		}
	}
	MlnBufferDestroy(MLN_HEAP_GENERAL, buffer);
out:
	MlnImpl_81049918(links);
	return ret;
}

/*
 * the ends of the by-value links whose values are first and second, as
 * from, to pairs (count buffers); 0x230A0003 when none is
 */
unsigned int MlnImpl_810444DC(const MlnBuffer *first, const MlnBuffer *second, unsigned int *count,
			      MlnBuffer ***entries)
{
	MlnStoreLink *matches[8];
	MlnStoreLink *links = NULL;
	MlnStoreLink *loaded;
	MlnStoreLink *link;
	MlnStoreLink *next;
	MlnBuffer **array;
	int damaged;
	int n;
	int i;
	int ret;

	if (first == NULL || first->size == 0 || first->data == NULL || second == NULL ||
	    second->size == 0 || second->data == NULL || entries == NULL) {
		ret = 3;
		goto out;
	}
	loaded = NULL;
	ret = MlnImpl_81049874(&loaded);
	if (ret == 6) {
		MlnImpl_810475F4(7);
		MlnImpl_81049918(loaded);
		goto out;
	}
	if (ret != 0) {
		MlnImpl_81049918(loaded);
		goto out;
	}
	links = loaded;
	loaded = NULL;
	MlnImpl_81049918(loaded);
	MlnMemset(matches, 0, sizeof(matches));
	if (links == NULL) {
		ret = 0x230A0003;
		goto out;
	}
	damaged = 0;
	n = 0;
	link = links;
	while (link != NULL) {
		if (link->byValue != 1 ||
		    MlnBufferCompare(MLN_HEAP_GENERAL, link->value1, first) != 0 ||
		    MlnBufferCompare(MLN_HEAP_GENERAL, link->value2, second) != 0) {
			link = link->next;
			continue;
		}
		ret = MlnImpl_8105DEDC(7, link->key, NULL);
		if (ret == 1 || ret == 6)
			ret = MlnImpl_810437BC(&links, link);
		else if (ret == 0 && link->byValue == 1)
			ret = MlnImpl_810436E4(link->from, link->to);
		if (ret == 6) {
			next = link->next;
			MlnImpl_81049E34(&links, link);
			damaged = 1;
			link = next;
		} else if (ret == 0x230A0011) {
			link = link->next;
		} else if (ret == 0) {
			matches[n++] = link;
			link = link->next;
		}
	}
	if (damaged == 1) {
		ret = MlnImpl_8104990C(links);
		if (ret == 0)
			ret = 6;
		goto out;
	}
	if (n == 0) {
		ret = 0x230A0003;
		goto out;
	}
	array = MlnAllocate(MLN_HEAP_GENERAL, n * 2 * sizeof(*array));
	if (array == NULL) {
		ret = 2;
		goto out;
	}
	MlnMemset(array, 0, n * 2 * sizeof(*array));
	for (i = 0; i < n; i++) {
		if (matches[i]->from != NULL) {
			array[i * 2] = MlnBufferCreate(MLN_HEAP_GENERAL, matches[i]->from->size,
						       matches[i]->from->data);
			if (array[i * 2] == NULL)
				goto fail;
		}
		array[i * 2 + 1] = MlnBufferCreate(MLN_HEAP_GENERAL, matches[i]->to->size,
						   matches[i]->to->data);
		if (array[i * 2 + 1] == NULL)
			goto fail;
	}
	*count = n * 2;
	*entries = array;
	goto out;
fail:
	for (i = 0; i < n * 2; i++)
		MlnBufferDestroy(MLN_HEAP_GENERAL, array[i]);
	MlnFree(MLN_HEAP_GENERAL, array);
	ret = 2;
out:
	MlnImpl_81049918(links);
	return ret;
}

/* the record of the link from from to to; a damaged one gives 6 */
int MlnImpl_81044824(const MlnBuffer *from, const MlnBuffer *to, MlnBuffer **record)
{
	MlnStoreLink *links = NULL;
	MlnStoreLink *found = NULL;
	MlnStoreLink *loaded;
	MlnBuffer *buffer;
	unsigned int key;
	int ret;

	if (to == NULL || to->size == 0 || to->data == NULL) {
		ret = 3;
		goto out;
	}
	loaded = NULL;
	ret = MlnImpl_81049874(&loaded);
	if (ret == 6) {
		MlnImpl_810475F4(7);
		MlnImpl_81049918(loaded);
		goto out;
	}
	if (ret != 0) {
		MlnImpl_81049918(loaded);
		goto out;
	}
	links = loaded;
	loaded = NULL;
	MlnImpl_81049918(loaded);
	ret = MlnImpl_81049DDC(links, from, to, &found);
	if (ret != 0)
		goto out;
	key = found->key;
	buffer = NULL;
	if (key > 8) {
		ret = 5;
	} else {
		ret = MlnImpl_8105DEDC(7, key, &buffer);
		if (ret == 1 || ret == 6) {
			if (MlnImpl_81049F14(key) == 0)
				MlnImpl_8105DC0C(7, key, NULL);
			ret = 6;
		} else if (ret == 0) {
			*record = buffer;
			buffer = NULL;
		}
	}
	MlnBufferDestroy(MLN_HEAP_GENERAL, buffer);
out:
	MlnImpl_81049918(links);
	return ret;
}

/* sets the values of the link from first to second, and saves the list */
unsigned int MlnImpl_81044984(MlnBuffer *key, MlnBuffer *value, const MlnBuffer *first,
			      const MlnBuffer *second)
{
	MlnStoreLink *links = NULL;
	MlnStoreLink *loaded = NULL;
	int ret;

	ret = MlnImpl_81049874(&loaded);
	if (ret == 6) {
		MlnImpl_810475F4(7);
		MlnImpl_81049918(loaded);
		goto out;
	}
	if (ret != 0) {
		MlnImpl_81049918(loaded);
		goto out;
	}
	links = loaded;
	loaded = NULL;
	MlnImpl_81049918(loaded);
	ret = MlnImpl_8104A300(links, key, value, first, second);
	if (ret == 0)
		ret = MlnImpl_8104990C(links);
out:
	MlnImpl_81049918(links);
	return ret;
}

/* the third step's state; a missing or damaged one marks the core broken */
static inline __attribute__((always_inline)) int MlnStoreReadToken(MlnBuffer **out)
{
	MlnBufferGroups *groups = NULL;
	MlnBuffer *buffer = NULL;
	int ret;

	ret = MlnImpl_8105DEDC(17, 0, &buffer);
	if (ret == 0) {
		*out = buffer;
		buffer = NULL;
	} else if (ret == 1) {
		ret = 0x230A0002;
		MlnImpl_810485B4();
	} else if (ret == 6 || ret == 0x230A0002) {
		MlnImpl_810485B4();
	}
	MlnBufferDestroy(MLN_HEAP_GENERAL, buffer);
	MlnImpl_81042A88(groups);
	return ret;
}

/* the SAML assertion in the third step's state (kind 0; 2 and 3 have none) */
static inline __attribute__((always_inline)) int MlnStoreReadAssertion(int kind,
								   MlnOctopusSamlAssertion **out)
{
	MlnOctopusSamlAssertion *assertion = NULL;
	MlnBuffer *token = NULL;
	int ret;

	if (kind != 0) {
		if (kind == 2 || kind == 3)
			ret = 0x230A000B;
		else
			ret = 3;
		goto out;
	}
	ret = MlnStoreReadToken(&token);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81042B18(token, 0, &assertion, 0);
	if (ret == MLN_OCTOPUS_ERROR_FORMAT) {
		MlnImpl_810485B4();
		ret = 0x230A0004;
	} else if (ret == 0) {
		*out = assertion;
		assertion = NULL;
	}
out:
	MlnImpl_8103CD0C(assertion);
	MlnBufferDestroy(MLN_HEAP_GENERAL, token);
	return ret;
}

/* a against b by date and time to the second: -1, 0 or 1 */
static inline __attribute__((always_inline)) int MlnStoreTimeCompare(MlnDateTime a, MlnDateTime b)
{
	if (a.year != b.year)
		return a.year > b.year ? 1 : -1;
	if (a.month != b.month)
		return a.month > b.month ? 1 : -1;
	if (a.day != b.day)
		return a.day > b.day ? 1 : -1;
	if (a.hour != b.hour)
		return a.hour > b.hour ? 1 : -1;
	if (a.minute != b.minute)
		return a.minute > b.minute ? 1 : -1;
	if (a.second != b.second)
		return a.second > b.second ? 1 : -1;
	return 0;
}

/*
 * whether the SAML assertion in the third step's state is valid now:
 * 0x230A0004 before its NotBefore or after its NotOnOrAfter
 */
int MlnImpl_81044A34(int kind)
{
	MlnOctopusSamlAssertion *assertion = NULL;
	MlnDateTime now;
	unsigned int seconds;
	int ret;

	ret = MlnStoreReadAssertion(kind, &assertion);
	if (ret != 0)
		goto out;
	if (assertion->notBefore == NULL && assertion->notOnOrAfter == NULL)
		goto out;
	ret = MlnImpl_81052EBC(&seconds);
	if (ret != 0) {
		ret = ret == 5 ? 5 : 0x230A0004;
		goto out;
	}
	if (MlnTimeFromSeconds(&seconds, 0, 0, &now) != 0) {
		ret = 5;
		goto out;
	}
	if (assertion->notBefore != NULL && MlnStoreTimeCompare(*assertion->notBefore, now) > 0)
		goto invalid;
	if (assertion->notOnOrAfter != NULL &&
	    MlnStoreTimeCompare(now, *assertion->notOnOrAfter) > 0)
		goto invalid;
	goto out;
invalid:
	ret = 0x230A0004;
out:
	MlnImpl_8103CD0C(assertion);
	return ret;
}

__attribute__((noinline)) int MlnImpl_81044D64(MlnBuffer **out)
{
	return MlnStoreReadToken(out);
}

__attribute__((noinline)) int MlnImpl_81044DEC(int kind, MlnOctopusSamlAssertion **out)
{
	return MlnStoreReadAssertion(kind, out);
}

/* appends a copy of key to record's items, as the current one */
static inline __attribute__((always_inline)) int MlnStoreRecordAppend(MlnRecord *record,
								     const MlnBuffer *key)
{
	MlnRecordItem *item;
	MlnBuffer *data;

	item = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*item));
	if (item == NULL) {
		MlnFree(MLN_HEAP_GENERAL, item);
		return 2;
	}
	data = MlnBufferCreate(MLN_HEAP_GENERAL, key->size, key->data);
	item->data = data;
	if (data == NULL) {
		MlnBufferDestroy(MLN_HEAP_GENERAL, data);
		MlnFree(MLN_HEAP_GENERAL, item);
		return 2;
	}
	item->next = NULL;
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
	MlnFree(MLN_HEAP_GENERAL, NULL);
	return 0;
}

/*
 * applies updates to the stored records (table 11): each finds or makes its
 * record, stops the run when the record's tag is not older than its own, and
 * adds its keys in order and removes the ones it lists
 */
int MlnImpl_81044F44(const MlnRecordUpdates *updates)
{
	MlnRecordArray array = { 0, NULL };
	MlnRecord *record = NULL;
	MlnBuffer *serialized = NULL;
	MlnBuffer *buffer;
	MlnBuffer *data = NULL;
	MlnBuffer *key;
	MlnRecordUpdate *update;
	MlnRecordItem *item;
	MlnRecordItem *next;
	MlnRecordKeys *keys;
	unsigned int count;
	unsigned int i;
	int stored = 0;
	int k;
	int ret;

	if (updates == NULL) {
		ret = 3;
		goto out;
	}
	count = updates->count;
	if (count == 0) {
		ret = 0;
		goto out;
	}
	buffer = NULL;
	ret = MlnImpl_8105DEDC(11, 0, &buffer);
	if (ret == 0) {
		data = buffer;
		buffer = NULL;
	}
	MlnBufferDestroy(MLN_HEAP_GENERAL, buffer);
	if (ret != 0) {
		if (ret != 1)
			goto fail;
		MlnBufferDestroy(MLN_HEAP_GENERAL, data);
		data = NULL;
	}
	array.count = count;
	ret = MlnImpl_8103D30C(data, &array);
	if (ret != 0)
		goto fail;
	array.count -= count;
	MlnBufferDestroy(MLN_HEAP_GENERAL, data);
	if (updates->count == 0)
		goto save;
	update = updates->updates[0];
	if (update == NULL || update->id == NULL)
		goto invalid;
	i = 0;
	for (;;) {
		ret = MlnImpl_81043860(update->id, &array, &record, &stored);
		if (ret != 0) {
			if (ret != 1)
				goto out;
			record = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*record));
			if (record == NULL) {
				MlnImpl_8103D5B8(record);
				ret = 2;
				goto out;
			}
			MlnMemset(record, 0, sizeof(*record));
			MlnImpl_8103D5B8(NULL);
			stored = 1;
		} else if (update->reset != 0) {
			for (item = record->first; item != NULL; item = next) {
				next = item->next;
				MlnBufferDestroy(MLN_HEAP_GENERAL, item->data);
				MlnFree(MLN_HEAP_GENERAL, item);
			}
			record->count = 0;
			record->last = NULL;
			record->current = NULL;
			record->first = NULL;
		}
		if (record->header == NULL) {
			record->header = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*record->header));
			if (record->header == NULL)
				goto nomem;
			record->header->count = 0;
			record->header->id = MlnBufferCreate(MLN_HEAP_GENERAL, update->id->size,
							     update->id->data);
		} else if ((int)record->header->tag >= (int)update->tag) {
			goto save;
		}
		record->header->tag = update->tag;
		keys = update->add;
		if (keys != NULL) {
			if (record->count == 0) {
				for (k = 0; k < keys->count; k++)
					if (MlnStoreRecordAppend(record, keys->keys[k]) != 0)
						goto nomem;
			} else {
				record->current = record->first;
				for (k = 0; k < keys->count; k++) {
					key = keys->keys[k];
					while (record->current != NULL &&
					       MlnBufferCompareKey(MLN_HEAP_GENERAL, record->current->data,
								   key) <= 0)
						record->current = record->current->next;
					if (record->current == NULL) {
						if (MlnStoreRecordAppend(record, key) != 0)
							goto nomem;
						continue;
					}
					item = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*item));
					if (item == NULL ||
					    (item->data = MlnBufferCreate(MLN_HEAP_GENERAL, key->size,
									  key->data)) == NULL) {
						MlnFree(MLN_HEAP_GENERAL, item);
						goto nomem;
					}
					item->next = record->current;
					item->previous = record->current->previous;
					if (record->current->previous != NULL)
						record->current->previous->next = item;
					else
						record->first = item;
					record->current->previous = item;
					record->current = item;
					record->count++;
					MlnFree(MLN_HEAP_GENERAL, NULL);
				}
			}
		}
		keys = update->remove;
		if (keys != NULL && record->count > 0) {
			record->current = record->first;
			for (k = 0; k < keys->count; k++) {
				while (record->current != NULL &&
				       MlnBufferCompareKey(MLN_HEAP_GENERAL, record->current->data,
							   keys->keys[k]) != 0)
					record->current = record->current->next;
				item = record->current;
				if (item == NULL)
					continue;
				if (item->next != NULL)
					item->next->previous = item->previous;
				else
					record->last = item->previous;
				if (item->previous != NULL)
					item->previous->next = item->next;
				else
					record->first = item->next;
				record->current = item->next;
				record->count--;
				MlnBufferDestroy(MLN_HEAP_GENERAL, item->data);
				MlnFree(MLN_HEAP_GENERAL, item);
			}
		}
		if (stored) {
			array.records[array.count++] = record;
			record = NULL;
		}
		i++;
		if (i >= updates->count)
			goto save;
		update = updates->updates[i];
		if (update == NULL || update->id == NULL)
			goto invalid;
	}
save:
	ret = MlnImpl_8103D0DC(&array, &serialized);
	if (ret == 0)
		ret = MlnImpl_8105DC0C(11, 0, serialized);
	goto out;
invalid:
	ret = 5;
	goto out;
nomem:
	ret = 2;
	goto out;
fail:
	MlnBufferDestroy(MLN_HEAP_GENERAL, data);
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, serialized);
	if (record != NULL && stored != 0)
		MlnImpl_8103D5B8(record);
	if (array.records != NULL) {
		for (k = 0; k < array.count; k++)
			MlnImpl_8103D5B8(array.records[k]);
		MlnFree(MLN_HEAP_GENERAL, array.records);
	}
	return ret;
}

/* reads the stored records (table 11) as one buffer */
int MlnImpl_810455D8(MlnBuffer **out)
{
	MlnBuffer *buffer;
	int ret;

	buffer = NULL;
	if (out == NULL) {
		ret = 3;
	} else {
		ret = MlnImpl_8105DEDC(11, 0, &buffer);
		if (ret == 0) {
			*out = buffer;
			buffer = NULL;
		}
	}
	MlnBufferDestroy(MLN_HEAP_GENERAL, buffer);
	return ret;
}

/* reads the tag of the stored record with id */
int MlnImpl_81045630(const MlnBuffer *id, unsigned int *tag)
{
	MlnRecord *record = NULL;
	int ret;

	if (id == NULL || tag == NULL) {
		ret = 3;
	} else {
		ret = MlnImpl_81043860(id, NULL, &record, NULL);
		if (ret == 0)
			*tag = record->header->tag;
		else if (ret == 1)
			ret = 0x230A0008;
	}
	MlnImpl_8103D5B8(record);
	return ret;
}

/* stores a key pair (table 12, keys 1 and 2) */
int MlnImpl_810456B0(const MlnStoreKeyPair *pair)
{
	int ret;

	if (pair == NULL)
		return 0;
	if (pair->version != 2 || pair->first == NULL)
		return 3;
	ret = MlnImpl_8105DC0C(12, 1, pair->first);
	if (ret != 0)
		return ret;
	return MlnImpl_8105DC0C(12, 2, pair->second);
}

/* reads the key pair (table 12) as an array of its two buffers, NULL when none */
int MlnImpl_8104570C(MlnArray **out)
{
	MlnBuffer *buffers[2] = { NULL, NULL };
	MlnArray *array = NULL;
	int ret;

	ret = MlnImpl_8105DEDC(12, 1, &buffers[0]);
	if (ret != 1 && ret != 0)
		goto out;
	ret = MlnImpl_8105DEDC(12, 2, &buffers[1]);
	if (ret != 1 && ret != 0)
		goto out;
	if (buffers[0] != NULL) {
		array = MlnWordArrayCreate(MLN_HEAP_GENERAL, 2, (const unsigned int *)buffers);
		if (array == NULL) {
			ret = 2;
			goto out;
		}
		buffers[0] = NULL;
		buffers[1] = NULL;
	}
	ret = 0;
	*out = array;
	array = NULL;
out:
	MlnArrayDestroy(MLN_HEAP_GENERAL, array);
	MlnBufferDestroy(MLN_HEAP_GENERAL, buffers[1]);
	MlnBufferDestroy(MLN_HEAP_GENERAL, buffers[0]);
	return ret;
}

/*
 * stores value under the digest of data in table 2, with the device id (its
 * first ten bytes when the id is neither zero at the start nor at the end)
 */
int MlnImpl_810457D4(const MlnBuffer *data, const MlnBuffer *value)
{
	unsigned char zero[10];
	unsigned char digest[20];
	unsigned char shortId[20];
	const MlnBuffer *device;
	const unsigned char *id;

	device = MlnImpl_81047E34();
	if (device == NULL)
		return 2;
	MlnMemset(zero, 0, sizeof(zero));
	id = device->data;
	if (id != NULL) {
		if (device->size != sizeof(shortId))
			return 5;
		if (MlnMemcmp(id, zero, 10) != 0 && MlnMemcmp(id + 12, zero, 8) != 0) {
			MlnMemset(shortId, 0, sizeof(shortId));
			MlnMemcpy(shortId, id, 10);
			id = shortId;
		}
	}
	MlnImpl_810570A8(data->data, data->size, digest);
	return MlnImpl_810467C0(2, digest, NULL, id, *value);
}

/* removes the link from context to data, and its record (table 7) */
unsigned int MlnImpl_810458B0(const MlnBuffer *context, const MlnBuffer *data)
{
	MlnStoreLink *links = NULL;
	MlnStoreLink *loaded;
	MlnStoreLink *found = NULL;
	unsigned int key;
	int damaged;
	int ret;

	if (data == NULL || data->size == 0 || data->data == NULL) {
		ret = 3;
		goto out;
	}
	loaded = NULL;
	ret = MlnImpl_81049874(&loaded);
	if (ret == 6) {
		MlnImpl_810475F4(7);
		MlnImpl_81049918(loaded);
		goto out;
	}
	if (ret != 0) {
		MlnImpl_81049918(loaded);
		goto out;
	}
	links = loaded;
	loaded = NULL;
	MlnImpl_81049918(loaded);
	ret = MlnImpl_81049DDC(links, context, data, &found);
	if (ret != 0)
		goto out;
	key = found->key;
	ret = MlnImpl_81049E34(&links, found);
	if (ret == 6)
		damaged = 1;
	else if (ret != 0)
		goto out;
	else
		damaged = 0;
	ret = MlnImpl_8104990C(links);
	if (ret == 0)
		ret = MlnImpl_8105DC0C(7, key, NULL);
	if (damaged == 1)
		ret = 6;
out:
	MlnImpl_81049918(links);
	return ret;
}

/* reads the record stored under the digest of data (table 6) */
int MlnImpl_810459F0(const MlnBuffer *data, MlnBuffer **out)
{
	unsigned char digest[20];

	if (out == NULL)
		return 3;
	MlnImpl_810570A8(data->data, data->size, digest);
	return MlnImpl_81046F98(2, digest, NULL, NULL, out, 1);
}

/* removes the record stored under the digest of data (table 6) */
int MlnImpl_81045A3C(const MlnBuffer *data)
{
	unsigned char digest[20];

	if (data == NULL)
		return 3;
	MlnImpl_810570A8(data->data, data->size, digest);
	return MlnImpl_81046BFC(2, digest, NULL, NULL, 1);
}

/* drops the record with key from table when it is there or damaged */
static inline __attribute__((always_inline)) int MlnStoreRecordDrop(int table, unsigned int key,
								   int *damaged)
{
	int ret;

	ret = MlnImpl_8105DEDC(table, key, NULL);
	if (ret == 0 || ret == 6) {
		ret = MlnImpl_8105DC0C(table, key, NULL);
		if (ret == 2)
			return 2;
		*damaged = 1;
		return 0;
	}
	if (ret != 1)
		return ret;
	return 0;
}

/*
 * checks the links against their records (table 7): drops each link whose
 * record is missing or damaged, then each record (keys 1 to 7) no link names
 * or that is damaged; 6 when anything was dropped. With no links at all,
 * every record is dropped first
 */
unsigned int MlnImpl_81045A84(void)
{
	MlnStoreLink *links = NULL;
	MlnStoreLink *link;
	MlnStoreLink *next;
	unsigned int key;
	int damaged = 0;
	int ret;

	ret = MlnImpl_810476C0(2);
	if (ret == 6)
		damaged = 1;
	else if (ret != 0)
		goto out;
	ret = MlnImpl_81049874(&links);
	if (ret != 0) {
		if (ret == 6)
			MlnImpl_810475F4(7);
		goto done;
	}
	if (links == NULL) {
		/* no links: every record is stale */
		if ((ret = MlnStoreRecordDrop(7, 1, &damaged)) != 0 ||
		    (ret = MlnStoreRecordDrop(7, 2, &damaged)) != 0 ||
		    (ret = MlnStoreRecordDrop(7, 3, &damaged)) != 0 ||
		    (ret = MlnStoreRecordDrop(7, 4, &damaged)) != 0 ||
		    (ret = MlnStoreRecordDrop(7, 5, &damaged)) != 0 ||
		    (ret = MlnStoreRecordDrop(7, 6, &damaged)) != 0 ||
		    (ret = MlnStoreRecordDrop(7, 7, &damaged)) != 0)
			goto done;
		if (damaged == 1)
			goto done;
	}
	for (link = links; link != NULL; link = next) {
		ret = MlnImpl_8105DEDC(7, link->key, NULL);
		if (ret == 1 || ret == 6) {
			ret = MlnImpl_8105DC0C(7, link->key, NULL);
			if (ret == 2)
				goto done;
			next = link->next;
			MlnImpl_81049E34(&links, link);
			damaged = 1;
		} else if (ret != 0) {
			goto done;
		} else {
			next = link->next;
		}
	}
	ret = MlnImpl_8104990C(links);
	if (ret != 0)
		goto done;
	for (key = 1; key < 8; key++) {
		ret = MlnImpl_8105DEDC(7, key, NULL);
		if (ret == 0 || ret == 6) {
			for (link = links; link != NULL; link = link->next)
				if (link->key == key)
					break;
			if (link == NULL || ret == 6) {
				ret = MlnImpl_8105DC0C(7, key, NULL);
				if (ret == 2)
					goto done;
				damaged = 1;
			}
		} else if (ret != 1) {
			goto done;
		} else {
			ret = 0;
		}
	}
done:
	if (damaged == 1)
		ret = 6;
out:
	MlnImpl_81049918(links);
	return ret;
}

/* reads table 14, NULL when it is empty */
int MlnImpl_81045E74(MlnBuffer **out)
{
	MlnBuffer *buffer = NULL;
	int ret;

	ret = MlnImpl_8105DEDC(14, 0, &buffer);
	if (ret == 1 || ret == 0) {
		*out = buffer;
		buffer = NULL;
		ret = 0;
	}
	MlnBufferDestroy(MLN_HEAP_GENERAL, buffer);
	return ret;
}

/* writes table 14 */
int MlnImpl_81045EE0(const MlnBuffer *data)
{
	return MlnImpl_8105DC0C(14, 0, data);
}

/*
 * reads record 0 of table (2 or 6); when it is missing, drops records 1 to 7,
 * and when it is damaged, drops all eight; 6 when anything was dropped
 */
int MlnImpl_81045EF8(int table, MlnBuffer **out)
{
	MlnBuffer *buffer = NULL;
	int damaged;
	int ret;

	if (out == NULL || (table != 2 && table != 6)) {
		ret = 5;
		goto out;
	}
	ret = MlnImpl_8105DEDC(table, 0, &buffer);
	if (ret == 1) {
		damaged = 0;
		if ((ret = MlnStoreRecordDrop(table, 1, &damaged)) != 0 ||
		    (ret = MlnStoreRecordDrop(table, 2, &damaged)) != 0 ||
		    (ret = MlnStoreRecordDrop(table, 3, &damaged)) != 0 ||
		    (ret = MlnStoreRecordDrop(table, 4, &damaged)) != 0 ||
		    (ret = MlnStoreRecordDrop(table, 5, &damaged)) != 0 ||
		    (ret = MlnStoreRecordDrop(table, 6, &damaged)) != 0 ||
		    (ret = MlnStoreRecordDrop(table, 7, &damaged)) != 0)
			goto out;
		ret = damaged == 1 ? 6 : 0;
	} else if (ret == 6) {
		if (MlnImpl_8105DC0C(table, 1, NULL) == 2 ||
		    MlnImpl_8105DC0C(table, 2, NULL) == 2 ||
		    MlnImpl_8105DC0C(table, 3, NULL) == 2 ||
		    MlnImpl_8105DC0C(table, 4, NULL) == 2 ||
		    MlnImpl_8105DC0C(table, 5, NULL) == 2 ||
		    MlnImpl_8105DC0C(table, 6, NULL) == 2 ||
		    MlnImpl_8105DC0C(table, 7, NULL) == 2 ||
		    MlnImpl_8105DC0C(table, 0, NULL) == 2)
			ret = 2;
	} else if (ret == 0) {
		*out = buffer;
		buffer = NULL;
	}
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, buffer);
	return ret;
}

/*
 * drops the 64-byte index entry at entry from index (record 0 of table) and
 * the record the entry names (its big-endian key at 0x3C)
 */
int MlnImpl_81046264(int table, const unsigned char *entry, const MlnBuffer *index)
{
	MlnBuffer *buffer = NULL;
	unsigned int offset;
	unsigned int key;
	int ret;

	if (entry == NULL || index == NULL) {
		ret = 5;
		goto out;
	}
	key = entry[0x3C] << 24 | entry[0x3D] << 16 | entry[0x3E] << 8 | entry[0x3F];
	buffer = MlnBufferCreate(MLN_HEAP_GENERAL, index->size - 0x40, NULL);
	if (buffer == NULL) {
		ret = 2;
		goto out;
	}
	offset = entry - index->data;
	MlnMemcpy(buffer->data, index->data, offset);
	MlnMemcpy(buffer->data + offset, index->data + offset + 0x40, index->size - 0x40 - offset);
	ret = MlnImpl_8105DC0C(table, key, NULL);
	if (ret == 0)
		ret = MlnImpl_8105DC0C(table, 0, buffer);
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, buffer);
	return ret;
}

/* makes a set of count bits, all clear */
static inline __attribute__((always_inline)) int MlnBitsetCreate(int count, MlnBitset **out)
{
	MlnBitset *set;
	int size;
	int ret;

	size = (count + 7) / 8;
	set = MlnAllocate(MLN_HEAP_GENERAL, size + 8);
	if (set == NULL) {
		ret = 2;
	} else {
		set->count = count;
		MlnMemset(set->bits, 0, size);
		ret = 0;
		*out = set;
		set = NULL;
	}
	MlnFree(MLN_HEAP_GENERAL, set);
	return ret;
}

static inline __attribute__((always_inline)) int MlnBitsetDestroy(MlnBitset *set)
{
	MlnFree(MLN_HEAP_GENERAL, set);
	return 0;
}

/* sets or clears bit index (bit 0 is the top bit of the first byte) */
static inline __attribute__((always_inline)) int MlnBitsetSet(MlnBitset *set, int index,
							     int value)
{
	if (index < 0 || index >= set->count)
		return 5;
	if (value != 0)
		set->bits[index >> 3] |= 0x80 >> (index & 7);
	else
		set->bits[index >> 3] &= 0xFF7F >> (index & 7);
	return 0;
}

__attribute__((noinline)) int MlnImpl_81046354(int count, MlnBitset **out)
{
	return MlnBitsetCreate(count, out);
}

__attribute__((noinline)) int MlnImpl_810463B4(MlnBitset *set)
{
	return MlnBitsetDestroy(set);
}

__attribute__((noinline)) int MlnImpl_810463CC(MlnBitset *set, int index, int value)
{
	return MlnBitsetSet(set, index, value);
}

/* finds the first bit that is set (value nonzero) or clear, -1 when none */
__attribute__((noinline)) int MlnImpl_81046420(const MlnBitset *set, int value, int *index)
{
	unsigned int byte;
	int bytes;
	int found = -1;
	int i;

	bytes = (set->count + 7) >> 3;
	if (value == 0) {
		for (i = 0; i < bytes; i++) {
			byte = set->bits[i];
			if (byte == 0xFF)
				continue;
			if ((byte & 0x80) == 0)
				found = i * 8;
			else if ((byte & 0x40) == 0)
				found = i * 8 + 1;
			else if ((byte & 0x20) == 0)
				found = i * 8 + 2;
			else if ((byte & 0x10) == 0)
				found = i * 8 + 3;
			else if ((byte & 0x08) == 0)
				found = i * 8 + 4;
			else if ((byte & 0x04) == 0)
				found = i * 8 + 5;
			else if ((byte & 0x02) == 0)
				found = i * 8 + 6;
			else if ((byte & 0x01) == 0)
				found = i * 8 + 7;
			else
				continue;
			break;
		}
	} else {
		for (i = 0; i < bytes; i++) {
			byte = set->bits[i];
			if (byte == 0)
				continue;
			if ((byte & 0x80) != 0)
				found = i * 8;
			else if ((byte & 0x40) != 0)
				found = i * 8 + 1;
			else if ((byte & 0x20) != 0)
				found = i * 8 + 2;
			else if ((byte & 0x10) != 0)
				found = i * 8 + 3;
			else if ((byte & 0x08) != 0)
				found = i * 8 + 4;
			else if ((byte & 0x04) != 0)
				found = i * 8 + 5;
			else if ((byte & 0x02) != 0)
				found = i * 8 + 6;
			else if ((byte & 0x01) != 0)
				found = i * 8 + 7;
			else
				continue;
			break;
		}
	}
	if (found >= set->count)
		found = -1;
	*index = found;
	return 0;
}

/*
 * finds the 64-byte entry of index whose ids (at 0x00, 0x14 and 0x28) equal
 * the given ones (NULL matches any); without one, the first key from 1 to 7
 * no entry uses
 */
int MlnImpl_8104657C(int kind, const MlnBuffer *index, const unsigned char *id1,
		     const unsigned char *id2, const unsigned char *id3,
		     const unsigned char **entry, int *free)
{
	MlnBitset *used = NULL;
	const unsigned char *p;
	unsigned int count;
	unsigned int i;
	int slot;
	int key;
	int ret;

	if (index == NULL || (id1 == NULL && id2 == NULL && id3 == NULL)) {
		ret = 5;
		goto out;
	}
	if (kind != 1 && kind != 2) {
		ret = 5;
		goto out;
	}
	ret = MlnBitsetCreate(8, &used);
	if (ret != 0)
		goto out;
	ret = MlnBitsetSet(used, 0, 1);
	if (ret != 0)
		goto out;
	if ((index->size & 0x3F) != 0) {
		ret = 6;
		goto out;
	}
	count = index->size >> 6;
	p = index->data;
	for (i = 0; i < count; i++, p += 0x40) {
		key = p[0x3C] << 24 | p[0x3D] << 16 | p[0x3E] << 8 | p[0x3F];
		if ((id1 == NULL || MlnMemcmp(p, id1, 20) == 0) &&
		    (id2 == NULL || MlnMemcmp(p + 0x14, id2, 20) == 0) &&
		    (id3 == NULL || MlnMemcmp(p + 0x28, id3, 20) == 0))
			break;
		if (MlnBitsetSet(used, key, 1) != 0) {
			ret = 6;
			goto out;
		}
	}
	if (i != count) {
		if (entry != NULL)
			*entry = p;
		if (free != NULL)
			*free = -1;
		ret = 0;
		goto out;
	}
	ret = MlnImpl_81046420(used, 0, &slot);
	if (ret != 0)
		goto out;
	if (entry != NULL)
		*entry = NULL;
	if (free != NULL)
		*free = slot;
out:
	MlnBitsetDestroy(used);
	return ret;
}

/* drops every record of table (2, 6 or 7), the index (record 0) last */
static inline __attribute__((always_inline)) int MlnStoreTableClear(int table)
{
	int ret;

	switch (table) {
	case 2:
	case 6:
	case 7:
		break;
	default:
		return 5;
	}
	if ((ret = MlnImpl_8105DC0C(table, 1, NULL)) != 0 ||
	    (ret = MlnImpl_8105DC0C(table, 2, NULL)) != 0 ||
	    (ret = MlnImpl_8105DC0C(table, 3, NULL)) != 0 ||
	    (ret = MlnImpl_8105DC0C(table, 4, NULL)) != 0 ||
	    (ret = MlnImpl_8105DC0C(table, 5, NULL)) != 0 ||
	    (ret = MlnImpl_8105DC0C(table, 6, NULL)) != 0 ||
	    (ret = MlnImpl_8105DC0C(table, 7, NULL)) != 0)
		return ret;
	return MlnImpl_8105DC0C(table, 0, NULL);
}

/*
 * stores value as the record of the index entry with ids id1 and id2 in
 * table 2 (kind 1) or 6 (kind 2), setting the entry's third id to id3, and
 * adds the entry, under the first free key, when there is none
 */
int MlnImpl_810467C0(int kind, const unsigned char *id1, const unsigned char *id2,
		     const unsigned char *id3, MlnBuffer value)
{
	unsigned char added[0x40];
	MlnBuffer *index = NULL;
	MlnBuffer *grown = NULL;
	const unsigned char *entry = NULL;
	int table;
	int key;
	int ret;

	if (id1 == NULL || value.data == NULL) {
		ret = 5;
		goto out;
	}
	if (kind == 1) {
		table = 2;
	} else if (kind == 2) {
		table = 6;
	} else {
		ret = 5;
		goto out;
	}
	ret = MlnImpl_8105DEDC(table, 0, &index);
	if (ret == 1) {
		index = MlnBufferCreate(MLN_HEAP_GENERAL, 0, NULL);
		if (index == NULL) {
			ret = 2;
			goto out;
		}
	} else if (ret == 6) {
		MlnStoreTableClear(table);
		goto out;
	} else if (ret != 0) {
		goto out;
	}
	ret = MlnImpl_8104657C(kind, index, id1, id2, NULL, &entry, &key);
	if (ret == 6) {
		MlnStoreTableClear(table);
		goto out;
	}
	if (ret != 0)
		goto out;
	if (entry != NULL) {
		key = entry[0x3C] << 24 | entry[0x3D] << 16 | entry[0x3E] << 8 | entry[0x3F];
		if (id3 != NULL)
			MlnMemcpy((unsigned char *)entry + 0x28, id3, 20);
		ret = MlnImpl_8105DC0C(table, key, &value);
		if (ret == 0)
			ret = MlnImpl_8105DC0C(table, 0, index);
		goto out;
	}
	if (index->size >= 7 * sizeof(added)) {
		ret = 0x230A0012;
		goto out;
	}
	MlnMemset(added, 0, sizeof(added));
	MlnMemcpy(added, id1, 20);
	if (id2 != NULL)
		MlnMemcpy(added + 0x14, id2, 20);
	if (id3 != NULL)
		MlnMemcpy(added + 0x28, id3, 20);
	added[0x3C] = key >> 24;
	added[0x3D] = key >> 16;
	added[0x3E] = key >> 8;
	added[0x3F] = key;
	grown = MlnBufferCreate(MLN_HEAP_GENERAL, index->size + sizeof(added), NULL);
	if (grown == NULL) {
		ret = 2;
		goto out;
	}
	MlnMemcpy(grown->data, index->data, index->size);
	MlnMemcpy(grown->data + index->size, added, sizeof(added));
	MlnBufferDestroy(MLN_HEAP_GENERAL, index);
	index = NULL;
	ret = MlnImpl_8105DC0C(table, key, &value);
	if (ret == 0)
		ret = MlnImpl_8105DC0C(table, 0, grown);
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, index);
	MlnBufferDestroy(MLN_HEAP_GENERAL, grown);
	return ret;
}

/*
 * removes the index entry with the given ids, and its record, from table 2
 * (kind 1) or 6 (kind 2); for kind 2 with links 1, also the links the entry
 * names
 */
__attribute__((noinline)) int MlnImpl_81046BFC(int kind, const unsigned char *id1, const unsigned char *id2,
		     const unsigned char *id3, int links)
{
	unsigned char removed[0x40];
	MlnBuffer *index = NULL;
	const unsigned char *entry = NULL;
	MlnStoreLink *loaded = NULL;
	unsigned int missing;
	int linked;
	int table;
	int ret;

	if (id1 == NULL && id2 == NULL && id3 == NULL) {
		ret = 5;
		goto out;
	}
	if (kind == 1) {
		table = 2;
		missing = 0x230A0006;
		linked = 0;
	} else if (kind == 2) {
		table = 6;
		missing = 0x230A0011;
		linked = 1;
	} else {
		ret = 5;
		goto out;
	}
	ret = MlnImpl_8105DEDC(table, 0, &index);
	if (ret == 1) {
		ret = missing;
		goto out;
	}
	if (ret == 6) {
		MlnStoreTableClear(table);
		goto out;
	}
	if (ret != 0)
		goto out;
	ret = MlnImpl_8104657C(kind, index, id1, id2, id3, &entry, NULL);
	if (ret == 6) {
		MlnStoreTableClear(table);
		goto out;
	}
	if (ret != 0)
		goto out;
	if (entry == NULL) {
		ret = missing;
		goto out;
	}
	MlnMemcpy(removed, entry, sizeof(removed));
	ret = MlnImpl_81046264(table, entry, index);
	if (ret != 0)
		goto out;
	if (linked && links == 1) {
		ret = MlnImpl_81049874(&loaded);
		if (ret == 6) {
			MlnStoreTableClear(7);
			goto out;
		}
		if (ret != 0)
			goto out;
		ret = MlnImpl_8104A110(&loaded, removed);
		if (ret != 0)
			goto out;
		ret = MlnImpl_8104990C(loaded);
	}
out:
	MlnImpl_81049918(loaded);
	MlnBufferDestroy(MLN_HEAP_GENERAL, index);
	return ret;
}

/*
 * reads the record of the index entry with the given ids from table 2
 * (kind 1) or 6 (kind 2); a damaged record loses its entry (for kind 2 with
 * links 1, its links too)
 */
__attribute__((noinline)) int MlnImpl_81046F98(int kind, const unsigned char *id1, const unsigned char *id2,
		     const unsigned char *id3, MlnBuffer **out, int links)
{
	MlnBuffer *index = NULL;
	MlnBuffer *record = NULL;
	const unsigned char *entry = NULL;
	unsigned int missing;
	unsigned int key;
	int table;
	int ret;

	if (id1 == NULL && id2 == NULL && id3 == NULL) {
		ret = 5;
		goto out;
	}
	if (kind == 1) {
		table = 2;
		missing = 0x230A0006;
	} else if (kind == 2) {
		table = 6;
		missing = 0x230A0011;
	} else {
		ret = 5;
		goto out;
	}
	ret = MlnImpl_8105DEDC(table, 0, &index);
	if (ret == 1) {
		ret = missing;
		goto out;
	}
	if (ret == 6) {
		MlnStoreTableClear(table);
		goto out;
	}
	if (ret != 0)
		goto out;
	ret = MlnImpl_8104657C(kind, index, id1, id2, id3, &entry, NULL);
	if (ret == 6) {
		MlnStoreTableClear(table);
		goto out;
	}
	if (ret != 0)
		goto out;
	if (entry == NULL) {
		ret = missing;
		goto out;
	}
	key = entry[0x3C] << 24 | entry[0x3D] << 16 | entry[0x3E] << 8 | entry[0x3F];
	ret = MlnImpl_8105DEDC(table, key, &record);
	if (ret == 1) {
		ret = missing;
		goto out;
	}
	if (ret == 6) {
		if (table == 6)
			MlnImpl_81046BFC(kind, id1, id2, id3, links);
		else if (table == 2)
			MlnImpl_81046264(2, entry, index);
		goto out;
	}
	if (ret != 0)
		goto out;
	if (out != NULL) {
		*out = record;
		record = NULL;
	}
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, index);
	MlnBufferDestroy(MLN_HEAP_GENERAL, record);
	return ret;
}

/* counts the entries of the index of table (2 or 6), dropping a damaged table */
int MlnImpl_810472BC(unsigned int *count, int table)
{
	MlnBuffer *index = NULL;
	unsigned int entries = 0;
	int ret;

	if (count == NULL || (table != 2 && table != 6)) {
		ret = 5;
		goto out;
	}
	ret = MlnImpl_8105DEDC(table, 0, &index);
	if (ret == 0) {
		entries = index->size;
		if ((entries & 0x3F) != 0)
			goto damaged;
		entries >>= 6;
	} else if (ret == 6) {
		goto damaged;
	} else if (ret != 1) {
		goto out;
	}
	*count = entries;
	ret = 0;
	goto out;
damaged:
	MlnStoreTableClear(table);
	ret = 6;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, index);
	return ret;
}

/* reads the record of entry i of the index of table 2 */
int MlnImpl_81047424(unsigned int i, int table, MlnBuffer **out)
{
	MlnBuffer *index = NULL;
	MlnBuffer *record = NULL;
	const unsigned char *entry;
	unsigned int key;
	int ret;

	if (out == NULL || table != 2) {
		ret = 5;
		goto out;
	}
	ret = MlnImpl_8105DEDC(table, 0, &index);
	if (ret == 6) {
		MlnStoreTableClear(table);
		goto out;
	}
	if (ret != 0)
		goto missing;
	if (index->size < (i + 1) << 6) {
		ret = 0x230A0007;
		goto out;
	}
	entry = index->data + (i << 6);
	key = entry[0x3C] << 24 | entry[0x3D] << 16 | entry[0x3E] << 8 | entry[0x3F];
	ret = MlnImpl_8105DEDC(table, key, &record);
	if (ret == 6) {
		MlnImpl_81046264(table, entry, index);
		goto out;
	}
	if (ret != 0)
		goto missing;
	*out = record;
	record = NULL;
	goto out;
missing:
	if (ret == 1)
		ret = 0x230A0006;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, index);
	MlnBufferDestroy(MLN_HEAP_GENERAL, record);
	return ret;
}

/* drops every record of table (2, 6 or 7) */
__attribute__((noinline)) int MlnImpl_810475F4(int table)
{
	return MlnStoreTableClear(table);
}

/* reads bit index of set */
static inline __attribute__((always_inline)) int MlnBitsetGet(const MlnBitset *set, int index,
							     int *value)
{
	if (index < 0 || index >= set->count)
		return 5;
	*value = set->bits[index >> 3] & (0x80 >> (index & 7));
	return 0;
}

/* drops record key of table when it is damaged or no index entry uses it */
static inline __attribute__((always_inline)) int MlnStoreRecordCheck(int table, int key,
								    const MlnBitset *used,
								    int *damaged)
{
	int bit;
	int ret;

	ret = MlnImpl_8105DEDC(table, key, NULL);
	if (ret == 0) {
		ret = MlnBitsetGet(used, key, &bit);
		if (ret != 0)
			return ret;
		if (bit == 0) {
			if (MlnImpl_8105DC0C(table, key, NULL) == 2)
				return 2;
			*damaged = 1;
		}
	} else if (ret == 6) {
		if (MlnImpl_8105DC0C(table, key, NULL) == 2)
			return 2;
		*damaged = 1;
	} else if (ret != 1) {
		return ret;
	}
	return 0;
}

/*
 * checks the index of table 2 (kind 1) or 6 (kind 2) against its records:
 * drops each entry whose record is missing or damaged, then each record
 * (keys 1 to 7) no entry uses or that is damaged; 6 when anything was dropped
 */
int MlnImpl_810476C0(int kind)
{
	MlnBuffer *index = NULL;
	MlnBitset *used = NULL;
	const unsigned char *entry;
	unsigned int count;
	unsigned int i;
	int damaged;
	int table;
	int key;
	int ret;

	if (kind == 1) {
		table = 2;
	} else if (kind == 2) {
		table = 6;
	} else {
		ret = 5;
		goto out;
	}
	ret = MlnImpl_81045EF8(table, &index);
	if (ret != 0)
		goto out;
	if (index == NULL) {
		ret = 0;
		goto out;
	}
	damaged = 0;
	count = index->size >> 6;
	entry = index->data;
	for (i = 0; i < count; i++, entry += 0x40) {
		key = entry[0x3C] << 24 | entry[0x3D] << 16 | entry[0x3E] << 8 | entry[0x3F];
		ret = MlnImpl_8105DEDC(table, key, NULL);
		if (ret == 1 || ret == 6) {
			MlnImpl_81046BFC(kind, entry, entry + 0x14, entry + 0x28, 1);
			damaged = 1;
		} else if (ret != 0) {
			goto out;
		}
	}
	MlnBufferDestroy(MLN_HEAP_GENERAL, index);
	index = NULL;
	ret = MlnBitsetCreate(8, &used);
	if (ret != 0)
		goto out;
	ret = MlnBitsetSet(used, 0, 1);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81045EF8(table, &index);
	if (ret != 0)
		goto out;
	if (index != NULL) {
		count = index->size >> 6;
		entry = index->data;
		for (i = 0; i < count; i++, entry += 0x40) {
			key = entry[0x3C] << 24 | entry[0x3D] << 16 | entry[0x3E] << 8 |
			      entry[0x3F];
			ret = MlnBitsetSet(used, key, 1);
			if (ret != 0)
				goto out;
		}
	}
	if ((ret = MlnStoreRecordCheck(table, 1, used, &damaged)) != 0 ||
	    (ret = MlnStoreRecordCheck(table, 2, used, &damaged)) != 0 ||
	    (ret = MlnStoreRecordCheck(table, 3, used, &damaged)) != 0 ||
	    (ret = MlnStoreRecordCheck(table, 4, used, &damaged)) != 0 ||
	    (ret = MlnStoreRecordCheck(table, 5, used, &damaged)) != 0 ||
	    (ret = MlnStoreRecordCheck(table, 6, used, &damaged)) != 0 ||
	    (ret = MlnStoreRecordCheck(table, 7, used, &damaged)) != 0)
		goto out;
	if (damaged == 1)
		ret = 6;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, index);
	MlnBitsetDestroy(used);
	return ret;
}

/* removes the link whose id is digest, and its record (table 7) */
__attribute__((noinline)) int MlnImpl_81047CDC(const unsigned char *digest)
{
	MlnStoreLink *links = NULL;
	MlnStoreLink *found = NULL;
	MlnBuffer id = { 20, (unsigned char *)digest };
	unsigned int key;
	int ret;

	ret = MlnImpl_81049874(&links);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81049D54(links, &id, &found);
	if (ret != 0)
		goto out;
	key = found->key;
	ret = MlnImpl_81049E34(&links, found);
	if (ret != 0)
		goto out;
	ret = MlnImpl_8104990C(links);
	if (ret != 0)
		goto out;
	ret = MlnImpl_8105DC0C(7, key, NULL);
out:
	MlnImpl_81049918(links);
	return ret;
}

/*
 * folds a check's result into the flags: found (0) and damaged (6) set flag
 * in their word, the expected miss is 0, anything else 5
 */
__attribute__((noinline)) int MlnImpl_81047D7C(int ret, int miss, unsigned int flag, unsigned int *found,
		     unsigned int *damaged)
{
	if (ret == 0) {
		*found |= flag;
		return 0;
	}
	if (ret == 6) {
		*damaged |= flag;
		return 0;
	}
	return ret == miss ? 0 : 5;
}

/* sets the device id (20 bytes) */
unsigned int MlnImpl_81047DC4(const unsigned char *id)
{
	MlnStoreDigest *device = MlnImpl_81043A08();

	if (device == NULL)
		return 2;
	MlnMemcpy(device->bytes, id, sizeof(device->bytes));
	device->buffer.size = sizeof(device->bytes);
	device->buffer.data = device->bytes;
	return 0;
}

/* clears the device id */
unsigned int MlnImpl_81047E00(void)
{
	MlnStoreDigest *device = MlnImpl_81043A08();

	if (device == NULL)
		return 2;
	MlnMemset(device->bytes, 0, sizeof(device->bytes));
	device->buffer.size = 0;
	device->buffer.data = NULL;
	return 0;
}

/* the device id, NULL without one */
__attribute__((noinline)) const MlnBuffer *MlnImpl_81047E34(void)
{
	MlnStoreDigest *device = MlnImpl_81043A08();

	if (device == NULL)
		return NULL;
	return &device->buffer;
}

/*
 * the state flags of the device id and of what is stored for it: which
 * records are there (found) or damaged; the damaged flags win and gain a
 * summary bit per group, else two summary bits for complete sets
 */
unsigned int MlnImpl_81047E48(const MlnBuffer *id, unsigned int *flags)
{
	unsigned char zero[10];
	unsigned char headId[20];
	unsigned char tailId[20];
	unsigned int found = 0;
	unsigned int damaged = 0;
	const unsigned char *data;
	const unsigned char *tail;
	const unsigned char *head;
	const unsigned char *full;
	int headZero;
	int tailZero;
	int ret;

	if (id == NULL || id->data == NULL || id->size != 20)
		return 3;
	MlnMemset(zero, 0, sizeof(zero));
	data = id->data;
	headZero = MlnMemcmp(data, zero, 10) == 0;
	tailZero = MlnMemcmp(data + 12, zero, 8) == 0;
	if (headZero && tailZero)
		return 3;
	ret = MlnImpl_8105DEDC(12, 1, NULL);
	if (ret == 0)
		found |= 0x100;
	else if (ret == 6)
		damaged |= 0x100;
	else if (ret != 1)
		return 5;
	ret = MlnImpl_8105DEDC(12, 2, NULL);
	if (ret == 0)
		found |= 0x200;
	else if (ret == 6)
		damaged |= 0x200;
	else if (ret != 1)
		return 5;
	ret = MlnImpl_81047D7C(MlnImpl_8105DEDC(14, 0, NULL), 1, 0x400, &found, &damaged);
	if (ret != 0)
		return ret;
	ret = MlnImpl_81047D7C(MlnImpl_8105DEDC(6, 0, NULL), 1, 0x800, &found, &damaged);
	if (ret != 0)
		return ret;
	ret = MlnImpl_81047D7C(MlnImpl_8105DEDC(7, 0, NULL), 1, 0x1000, &found, &damaged);
	if (ret != 0)
		return ret;
	if (headZero) {
		tail = data;
		head = NULL;
		full = NULL;
	} else if (tailZero) {
		tail = NULL;
		head = data;
		full = data;
	} else {
		MlnMemset(headId, 0, sizeof(headId));
		MlnMemcpy(headId, data, 10);
		MlnMemset(tailId, 0, sizeof(tailId));
		MlnMemcpy(tailId + 12, data + 12, 8);
		tail = tailId;
		head = headId;
		full = data;
	}
	if ((damaged & 0x800) == 0) {
		if (tail != NULL) {
			ret = MlnImpl_81047D7C(MlnImpl_81046F98(2, NULL, NULL, tail, NULL, 1),
					       0x230A0011, 0x2000, &found, &damaged);
			if (ret != 0)
				return ret;
		}
		if (head != NULL) {
			ret = MlnImpl_81047D7C(MlnImpl_81046F98(2, NULL, NULL, head, NULL, 1),
					       0x230A0011, 0x10000, &found, &damaged);
			if (ret != 0)
				return ret;
		}
	}
	if ((damaged & 0x1000) == 0) {
		if (tail != NULL) {
			ret = MlnImpl_81047D7C(MlnImpl_810827B0(tail), 0x230A0003, 0x4000, &found,
					       &damaged);
			if (ret != 0)
				return ret;
		}
		if (full != NULL) {
			ret = MlnImpl_81047D7C(MlnImpl_810827B0(full), 0x230A0003, 0x20000, &found,
					       &damaged);
			if (ret != 0)
				return ret;
		}
	}
	if (damaged != 0) {
		if (damaged & 0x1F00)
			damaged |= 0x40000000;
		if (damaged & 0x6000)
			damaged |= 0x20000000;
		if (damaged & 0x30000)
			damaged |= 0x80000000;
		*flags = damaged;
	} else {
		if ((found & 0x60007900) == 0x7900)
			found |= 0x2000000;
		if ((found & 0xC0031900) == 0x31900)
			found |= 0x800000;
		*flags = found;
	}
	return 0;
}

/*
 * the state flags of the core: which of the steps' states (tables 15, 3 and
 * 17) and the two key states are there (found) or damaged; damaged wins
 */
__attribute__((noinline)) unsigned int MlnImpl_81048214(unsigned int *flags)
{
	unsigned int found = 0;
	unsigned int damaged = 0;
	void *handle = NULL;
	unsigned int ret;

	ret = MlnImpl_8105DEDC(15, 0, NULL);
	if (ret == 0)
		found |= 0x01;
	else if (ret == 6)
		damaged |= 0x01;
	else if (ret != 1)
		goto invalid;
	ret = MlnImpl_8105DEDC(3, 0, NULL);
	if (ret == 0)
		found |= 0x04;
	else if (ret == 6)
		damaged |= 0x04;
	else if (ret != 1)
		goto invalid;
	ret = MlnImpl_8105DEDC(17, 0, NULL);
	if (ret == 0)
		found |= 0x10;
	else if (ret == 6)
		damaged |= 0x10;
	else if (ret != 1)
		goto invalid;
	ret = MlnImpl_81080BFC(&handle);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81081374(1);
	if (ret == 0)
		found |= 0x02;
	else if (ret == 6)
		damaged |= 0x02;
	else if (ret != 0x23110005)
		goto invalid;
	ret = MlnImpl_81081374(0);
	if (ret == 0)
		found |= 0x08;
	else if (ret == 6)
		damaged |= 0x08;
	else if (ret != 0x23110005)
		goto invalid;
	if (damaged != 0) {
		*flags = damaged | 0x10000000;
	} else {
		if (found == 0x1F)
			found |= 0x1000000;
		*flags = found;
	}
	ret = 0;
	goto out;
invalid:
	ret = 5;
out:
	MlnImpl_81080C1C(handle);
	return ret;
}

/* resets the core: drops the steps' states, the key states and every link */
int MlnImpl_810485B4(void)
{
	unsigned char zero[20];
	unsigned int flags = 0;
	MlnStoreLink *links = NULL;
	void *handle = NULL;
	int ret;

	ret = MlnImpl_81048214(&flags);
	if (ret != 0)
		goto out;
	if (flags == 0) {
		ret = 0x230A0002;
		goto out;
	}
	ret = MlnImpl_8105DC0C(15, 0, NULL);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81080BFC(&handle);
	if (ret != 0)
		goto out;
	ret = MlnImpl_8108136C(0, NULL, 0);
	if (ret != 0)
		goto out;
	ret = MlnImpl_8108136C(1, NULL, 0);
	if (ret != 0)
		goto out;
	ret = MlnImpl_8105DC0C(3, 0, NULL);
	if (ret != 0)
		goto out;
	ret = MlnImpl_8105DC0C(17, 0, NULL);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81049874(&links);
	if (ret != 0)
		goto out;
	MlnMemset(zero, 0, sizeof(zero));
	ret = MlnImpl_8104A110(&links, zero);
	if (ret != 0)
		goto out;
	ret = MlnImpl_8104990C(links);
out:
	MlnImpl_81080C1C(handle);
	MlnImpl_81049918(links);
	return ret;
}

/* drops record key of tables 6 and 7 */
static inline __attribute__((always_inline)) void MlnStoreLinkSlotClear(unsigned int key)
{
	MlnImpl_8105DC0C(6, key, NULL);
	MlnImpl_8105DC0C(7, key, NULL);
}

/*
 * removes the link whose id is id, and its record (table 7); without an id,
 * drops tables 6 and 7 whole
 */
unsigned int MlnImpl_810486D4(const MlnBuffer *id)
{
	MlnStoreLink *links;
	MlnStoreLink *found;
	MlnBuffer key;
	unsigned int record;
	int ret;

	if (id == NULL || id->data == NULL) {
		MlnStoreLinkSlotClear(0);
		MlnStoreLinkSlotClear(1);
		MlnStoreLinkSlotClear(2);
		MlnStoreLinkSlotClear(3);
		MlnStoreLinkSlotClear(4);
		MlnStoreLinkSlotClear(5);
		MlnStoreLinkSlotClear(6);
		MlnStoreLinkSlotClear(7);
		return 0;
	}
	if (id->size != 20)
		return 3;
	links = NULL;
	found = NULL;
	key.size = 20;
	key.data = id->data;
	ret = MlnImpl_81049874(&links);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81049D54(links, &key, &found);
	if (ret != 0)
		goto out;
	record = found->key;
	ret = MlnImpl_81049E34(&links, found);
	if (ret != 0)
		goto out;
	ret = MlnImpl_8104990C(links);
	if (ret != 0)
		goto out;
	ret = MlnImpl_8105DC0C(7, record, NULL);
out:
	MlnImpl_81049918(links);
	if (ret == 0x230A0003)
		ret = 0;
	return ret;
}

/*
 * repairs what the core's and the id's state flags call damaged: the steps'
 * and key states, then the links (all of them, or those of the id's head
 * and tail ids) and the key pair records
 */
unsigned int MlnImpl_810488A8(const MlnBuffer *id)
{
	unsigned char headId[20];
	unsigned char tailId[20];
	unsigned int idFlags = 0;
	unsigned int coreFlags = 0;
	void *handle = NULL;
	const unsigned char *data;
	unsigned int repair;
	int ret;

	if (id == NULL || (data = id->data) == NULL || id->size != 20) {
		ret = 3;
		goto out;
	}
	ret = MlnImpl_81048214(&coreFlags);
	if (ret != 0)
		goto out;
	if (coreFlags & 0x10000000) {
		if (coreFlags & 0x01)
			MlnImpl_8105DC0C(15, 0, NULL);
		if (coreFlags & 0x02) {
			ret = MlnImpl_81080BFC(&handle);
			if (ret != 0)
				goto out;
			ret = MlnImpl_8108136C(1, NULL, 0);
			if (ret != 0)
				goto out;
		}
		if (coreFlags & 0x04)
			MlnImpl_8105DC0C(3, 0, NULL);
		if (coreFlags & 0x08) {
			if (handle == NULL) {
				ret = MlnImpl_81080BFC(&handle);
				if (ret != 0)
					goto out;
			}
			ret = MlnImpl_8108136C(0, NULL, 0);
			if (ret != 0)
				goto out;
		}
		if (coreFlags & 0x10)
			MlnImpl_8105DC0C(17, 0, NULL);
	}
	ret = MlnImpl_81047E48(id, &idFlags);
	if (ret != 0)
		goto out;
	repair = 0;
	if (idFlags & 0x60000000) {
		if (idFlags & 0x1800)
			repair |= 1;
		else if (idFlags & 0x6000)
			repair |= 2;
		if (idFlags & 0x700)
			repair |= 5;
	}
	if (idFlags & 0xC0000000) {
		if (idFlags & 0x1800)
			repair |= 1;
		else if (idFlags & 0x30000)
			repair |= 2;
		if (idFlags & 0x700)
			repair |= 5;
	}
	if (repair & 1) {
		MlnStoreLinkSlotClear(0);
		MlnStoreLinkSlotClear(1);
		MlnStoreLinkSlotClear(2);
		MlnStoreLinkSlotClear(3);
		MlnStoreLinkSlotClear(4);
		MlnStoreLinkSlotClear(5);
		MlnStoreLinkSlotClear(6);
		MlnStoreLinkSlotClear(7);
	} else if (repair & 2) {
		MlnMemset(headId, 0, sizeof(headId));
		MlnMemcpy(headId, data, 10);
		MlnMemset(tailId, 0, sizeof(tailId));
		MlnMemcpy(tailId + 12, data + 12, 8);
		if (idFlags & 0x2000) {
			ret = MlnImpl_81046BFC(2, NULL, NULL, tailId, 1);
			if (ret != 0x230A0011 && ret != 0)
				goto out;
		}
		if (idFlags & 0x4000) {
			ret = MlnImpl_81047CDC(tailId);
			if (ret != 0x230A0003 && ret != 0)
				goto out;
		}
		if (idFlags & 0x10000) {
			ret = MlnImpl_81046BFC(2, NULL, NULL, headId, 1);
			if (ret != 0x230A0011 && ret != 0)
				goto out;
		}
		if (idFlags & 0x20000) {
			ret = MlnImpl_81047CDC(data);
			if (ret != 0x230A0003 && ret != 0)
				goto out;
		}
	}
	if (repair & 4) {
		MlnImpl_8105DC0C(12, 1, NULL);
		MlnImpl_8105DC0C(12, 2, NULL);
		MlnImpl_8105DC0C(14, 0, NULL);
	}
	ret = 0;
out:
	MlnImpl_81080C1C(handle);
	return ret;
}

/* whether digest is the from or to digest of one of links: 0 when it is, else 1 */
int MlnImpl_81048CF8(const MlnStoreLink *links, const unsigned char *digest)
{
	unsigned char fromDigest[20];
	unsigned char toDigest[20];
	const MlnStoreLink *link;

	if (digest == NULL)
		return 3;
	for (link = links; link != NULL; link = link->next) {
		if (link->byValue == 1) {
			MlnMemset(fromDigest, 0, sizeof(fromDigest));
			MlnMemset(toDigest, 0, sizeof(toDigest));
			if (link->from != NULL)
				MlnImpl_810570A8(link->from->data, link->from->size, fromDigest);
			if (link->to != NULL)
				MlnImpl_810570A8(link->to->data, link->to->size, toDigest);
			if (MlnMemcmp(toDigest, digest, 20) == 0 ||
			    MlnMemcmp(fromDigest, digest, 20) == 0)
				return 0;
		} else {
			if (MlnMemcmp(link->toDigest, digest, 20) == 0 ||
			    MlnMemcmp(link->fromDigest, digest, 20) == 0)
				return 0;
		}
	}
	return 1;
}

/*
 * finds the link from from to to; without one, gives the first key (1 to
 * 7) no link uses in free, and 0x230A0012 when all are used
 */
__attribute__((noinline)) int MlnImpl_81048DF0(MlnStoreLink *links, const MlnBuffer *from, const MlnBuffer *to,
		     MlnStoreLink **found, int *free)
{
	unsigned char fromDigest[20];
	unsigned char toDigest[20];
	MlnBitset *used = NULL;
	MlnStoreLink *link;
	int hash = 1;
	int slot;
	int ret;

	if (to == NULL || found == NULL) {
		ret = 3;
		goto out;
	}
	*found = NULL;
	ret = MlnImpl_81046354(8, &used);
	if (ret != 0)
		goto out;
	ret = MlnImpl_810463CC(used, 0, 1);
	if (ret != 0)
		goto out;
	for (link = links; link != NULL; link = link->next) {
		if (link->byValue == 1) {
			if (MlnBufferCompare(MLN_HEAP_GENERAL, link->from, from) == 0 &&
			    MlnBufferCompare(MLN_HEAP_GENERAL, link->to, to) == 0)
				break;
		} else {
			if (hash == 1) {
				MlnMemset(fromDigest, 0, sizeof(fromDigest));
				MlnMemset(toDigest, 0, sizeof(toDigest));
				if (from != NULL) {
					ret = MlnImpl_810570A8(from->data, from->size, fromDigest);
					if (ret != 0)
						goto out;
				}
				ret = MlnImpl_810570A8(to->data, to->size, toDigest);
				if (ret != 0)
					goto out;
			}
			hash = 0;
			if (MlnMemcmp(link->fromDigest, fromDigest, 20) == 0 &&
			    MlnMemcmp(link->toDigest, toDigest, 20) == 0)
				break;
		}
		ret = MlnImpl_810463CC(used, link->key, 1);
		if (ret != 0)
			goto out;
	}
	if (link != NULL) {
		*found = link;
		if (free != NULL)
			*free = -1;
		ret = 0;
		goto out;
	}
	if (free == NULL)
		goto out;
	ret = MlnImpl_81046420(used, 0, &slot);
	if (ret != 0)
		goto out;
	if (slot != -1)
		*free = slot;
	else
		ret = 0x230A0012;
out:
	MlnImpl_810463B4(used);
	return ret;
}

/* stores v big-endian at p */
static inline __attribute__((always_inline)) void MlnStorePutBE32(unsigned char *p, unsigned int v)
{
	p[0] = v >> 24;
	p[1] = v >> 16;
	p[2] = v >> 8;
	p[3] = v;
}

/* appends buffer's bytes at p, when there is a buffer */
#define MLN_STORE_APPEND(p, buffer)						\
	do {									\
		if ((buffer) != NULL) {						\
			MlnMemcpy((p), (buffer)->data, (buffer)->size);		\
			(p) += (buffer)->size;					\
		}								\
	} while (0)

/*
 * stores links (record 0 of table 7): a 64-byte entry per link, its key at
 * 0x3C; a by-value link (key bit 31) has the sizes of its ids at 0x00 to
 * 0x0F and the ids after the entry, the others their three digests
 */
__attribute__((noinline)) int MlnImpl_81048FF0(const MlnStoreLink *links)
{
	unsigned char entry[0x40];
	const MlnStoreLink *link;
	MlnBuffer *buffer;
	unsigned char *p;
	unsigned int size;
	int ret;

	if (links == NULL) {
		buffer = NULL;
		ret = MlnImpl_8105DC0C(7, 0, NULL);
		goto out;
	}
	size = 0;
	for (link = links; link != NULL; link = link->next) {
		size += sizeof(entry);
		if (link->byValue != 1)
			continue;
		if (link->from != NULL)
			size += link->from->size;
		if (link->to != NULL)
			size += link->to->size;
		if (link->value1 != NULL)
			size += link->value1->size;
		if (link->value2 != NULL)
			size += link->value2->size;
	}
	buffer = MlnBufferCreate(MLN_HEAP_GENERAL, size, NULL);
	if (buffer == NULL) {
		ret = 2;
		goto out;
	}
	p = buffer->data;
	for (link = links; link != NULL; link = link->next) {
		MlnMemset(entry, 0, sizeof(entry));
		if (link->byValue == 1) {
			if (link->from != NULL)
				MlnStorePutBE32(entry + 0x00, link->from->size);
			if (link->to != NULL)
				MlnStorePutBE32(entry + 0x04, link->to->size);
			if (link->value1 != NULL)
				MlnStorePutBE32(entry + 0x08, link->value1->size);
			if (link->value2 != NULL)
				MlnStorePutBE32(entry + 0x0C, link->value2->size);
			MlnStorePutBE32(entry + 0x3C, link->key | 0x80000000);
			MlnMemcpy(p, entry, sizeof(entry));
			p += sizeof(entry);
			MLN_STORE_APPEND(p, link->from);
			MLN_STORE_APPEND(p, link->to);
			MLN_STORE_APPEND(p, link->value1);
			MLN_STORE_APPEND(p, link->value2);
		} else {
			MlnMemcpy(entry + 0x00, link->toDigest, 20);
			MlnMemcpy(entry + 0x14, link->fromDigest, 20);
			MlnMemcpy(entry + 0x28, link->idDigest, 20);
			MlnStorePutBE32(entry + 0x3C, link->key);
			MlnMemcpy(p, entry, sizeof(entry));
			p += sizeof(entry);
		}
	}
	ret = MlnImpl_8105DC0C(7, 0, buffer);
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, buffer);
	return ret;
}

/*
 * drops the index entries (table 6) of link's ends that no link of links
 * uses any more
 */
int MlnImpl_810492F8(const MlnStoreLink *links, const MlnStoreLink *link)
{
	unsigned char zero[20];
	unsigned char fromDigest[20];
	unsigned char toDigest[20];
	int ret;

	MlnMemset(zero, 0, sizeof(zero));
	MlnMemset(fromDigest, 0, sizeof(fromDigest));
	MlnMemset(toDigest, 0, sizeof(toDigest));
	if (link->byValue == 1) {
		if (link->from != NULL) {
			ret = MlnImpl_810570A8(link->from->data, link->from->size, fromDigest);
			if (ret != 0)
				return ret;
		}
		if (link->to != NULL) {
			ret = MlnImpl_810570A8(link->to->data, link->to->size, toDigest);
			if (ret != 0)
				return ret;
		}
	} else {
		MlnMemcpy(fromDigest, link->fromDigest, sizeof(fromDigest));
		MlnMemcpy(toDigest, link->toDigest, sizeof(toDigest));
	}
	if (MlnMemcmp(fromDigest, zero, sizeof(zero)) != 0 &&
	    MlnImpl_81048CF8(links, fromDigest) != 0) {
		ret = MlnImpl_81046BFC(2, fromDigest, NULL, NULL, 1);
		if (ret != 0x230A0011 && ret != 0)
			return ret;
	}
	if (MlnImpl_81048CF8(links, toDigest) != 0) {
		ret = MlnImpl_81046BFC(2, toDigest, NULL, NULL, 1);
		if (ret != 0x230A0011)
			return ret;
	}
	return 0;
}

/* frees links and their ids */
static inline __attribute__((always_inline)) void MlnStoreLinksFree(MlnStoreLink *links)
{
	MlnStoreLink *next;

	for (; links != NULL; links = next) {
		next = links->next;
		MlnBufferDestroy(MLN_HEAP_GENERAL, links->to);
		MlnBufferDestroy(MLN_HEAP_GENERAL, links->from);
		MlnBufferDestroy(MLN_HEAP_GENERAL, links->value1);
		MlnBufferDestroy(MLN_HEAP_GENERAL, links->value2);
		MlnFree(MLN_HEAP_GENERAL, links);
	}
}

/* reads big-endian at p */
#define MLN_STORE_BE32(p) ((p)[0] << 24 | (p)[1] << 16 | (p)[2] << 8 | (p)[3])

/* parses the stored links (record 0 of table 7), the layout of 0x81048FF0 */
__attribute__((noinline)) int MlnImpl_81049464(MlnStoreLink **out)
{
	MlnBuffer *buffer = NULL;
	MlnStoreLink *links = NULL;
	MlnStoreLink *last = NULL;
	MlnStoreLink *link;
	const unsigned char *data;
	const unsigned char *entry;
	unsigned int fromSize;
	unsigned int toSize;
	unsigned int value1Size;
	unsigned int value2Size;
	unsigned int offset;
	unsigned int end;
	int ret;

	if (out == NULL) {
		ret = 3;
		goto out;
	}
	ret = MlnImpl_8105DEDC(7, 0, &buffer);
	if (ret == 1) {
		*out = NULL;
		return 0;
	}
	if (ret != 0)
		goto out;
	offset = 0;
	data = buffer->data;
	while (offset < buffer->size) {
		end = offset + 0x40;
		if (buffer->size < end) {
			ret = 6;
			goto fail;
		}
		entry = data + offset;
		link = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*link));
		if (link == NULL) {
			ret = 2;
			goto fail;
		}
		MlnMemset(link, 0, sizeof(*link));
		if ((signed char)entry[0x3C] >= 0) {
			MlnMemcpy(link->toDigest, entry, 20);
			MlnMemcpy(link->fromDigest, entry + 0x14, 20);
			MlnMemcpy(link->idDigest, entry + 0x28, 20);
			link->byValue = 0;
			offset = end;
		} else {
			fromSize = MLN_STORE_BE32(entry);
			toSize = MLN_STORE_BE32(entry + 4);
			value1Size = MLN_STORE_BE32(entry + 8);
			value2Size = MLN_STORE_BE32(entry + 12);
			offset = value1Size + toSize + value2Size + (fromSize + end);
			if (offset > buffer->size) {
				ret = 6;
				goto bad;
			}
			if (fromSize != 0) {
				link->from = MlnBufferCreate(MLN_HEAP_GENERAL, fromSize,
							     data + end);
				if (link->from == NULL)
					goto nomem;
			}
			if (toSize != 0) {
				link->to = MlnBufferCreate(MLN_HEAP_GENERAL, toSize,
							   data + fromSize + end);
				if (link->to == NULL)
					goto nomem;
			}
			if (value1Size != 0) {
				link->value1 = MlnBufferCreate(MLN_HEAP_GENERAL, value1Size,
							       data + (fromSize + toSize + end));
				if (link->value1 == NULL)
					goto nomem;
			}
			if (value2Size != 0) {
				link->value2 = MlnBufferCreate(MLN_HEAP_GENERAL, value2Size,
							       data + (fromSize + toSize + end +
									       value1Size));
				if (link->value2 == NULL)
					goto nomem;
			}
			link->byValue = 1;
		}
		link->key = MLN_STORE_BE32(entry + 0x3C) & ~0x80000000;
		if (links == NULL)
			links = link;
		else
			last->next = link;
		last = link;
	}
	if (offset == buffer->size) {
		*out = links;
		goto out;
	}
	ret = 6;
	goto fail;
nomem:
	ret = 2;
bad:
	MlnStoreLinksFree(link);
fail:
	MlnStoreLinksFree(links);
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, buffer);
	return ret;
}

/* loads the stored links */
__attribute__((noinline)) int MlnImpl_81049874(MlnStoreLink **links)
{
	MlnStoreLink *loaded = NULL;
	int ret;

	if (links == NULL)
		return 3;
	ret = MlnImpl_81049464(&loaded);
	if (ret != 0) {
		MlnStoreLinksFree(loaded);
		return ret;
	}
	*links = loaded;
	return 0;
}

/* stores links */
__attribute__((noinline)) int MlnImpl_8104990C(const MlnStoreLink *links)
{
	return MlnImpl_81048FF0(links);
}

__attribute__((noinline)) int MlnImpl_81049918(MlnStoreLink *links)
{
	MlnStoreLinksFree(links);
	return 0;
}

/* copies buffer into *copy, when there is a buffer: 2 when it cannot */
#define MLN_STORE_COPY(copy, buffer)						\
	do {									\
		if ((buffer) != NULL) {						\
			(copy) = MlnBufferCreate(MLN_HEAP_GENERAL, (buffer)->size,	\
						 (buffer)->data);		\
			if ((copy) == NULL)					\
				return 2;					\
		}								\
	} while (0)

/*
 * adds the link from from to to, or updates the one there: with an id the
 * link keeps the digests of its ends and the id, without one the ends
 * themselves and the two values; gives its key
 */
int MlnImpl_81049978(MlnStoreLink **links, const MlnBuffer *from, const MlnBuffer *to,
		     const MlnBuffer *id, const MlnBuffer *value1, const MlnBuffer *value2,
		     unsigned int *key)
{
	unsigned char digest[20];
	MlnStoreLink *found = NULL;
	MlnStoreLink *head;
	MlnStoreLink *link;
	MlnStoreLink *last;
	int slot = 0;
	int ret;

	if (links == NULL || to == NULL || key == NULL)
		return 3;
	head = *links;
	ret = MlnImpl_81048DF0(head, from, to, &found, &slot);
	if (ret != 0)
		return ret;
	if (found != NULL) {
		if (found->byValue == 0) {
			if (id != NULL) {
				MlnMemcpy(found->idDigest, id->data, 20);
			} else {
				MlnMemset(found->toDigest, 0, 20);
				MlnMemset(found->fromDigest, 0, 20);
				MlnMemset(found->idDigest, 0, 20);
				found->byValue = 1;
				MLN_STORE_COPY(found->from, from);
				found->to = MlnBufferCreate(MLN_HEAP_GENERAL, to->size, to->data);
				if (found->to == NULL)
					return 2;
				MLN_STORE_COPY(found->value1, value1);
				MLN_STORE_COPY(found->value2, value2);
			}
		} else {
			MlnBufferDestroy(MLN_HEAP_GENERAL, found->value1);
			found->value1 = NULL;
			MlnBufferDestroy(MLN_HEAP_GENERAL, found->value2);
			found->value2 = NULL;
			MLN_STORE_COPY(found->value1, value1);
			MLN_STORE_COPY(found->value2, value2);
		}
		*key = found->key;
		return 0;
	}
	link = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*link));
	if (link == NULL)
		return 2;
	MlnMemset(link, 0, sizeof(*link));
	link->key = slot;
	if (id != NULL) {
		if (from != NULL) {
			ret = MlnImpl_810570A8(from->data, from->size, digest);
			if (ret != 0)
				goto fail;
			MlnMemcpy(link->fromDigest, digest, 20);
		}
		ret = MlnImpl_810570A8(to->data, to->size, digest);
		if (ret != 0)
			goto fail;
		MlnMemcpy(link->toDigest, digest, 20);
		MlnMemcpy(link->idDigest, id->data, 20);
	} else {
		link->byValue = 1;
		if (from != NULL) {
			link->from = MlnBufferCreate(MLN_HEAP_GENERAL, from->size, from->data);
			if (link->from == NULL)
				goto nomem;
		}
		link->to = MlnBufferCreate(MLN_HEAP_GENERAL, to->size, to->data);
		if (link->to == NULL)
			goto nomem;
		if (value1 != NULL) {
			link->value1 = MlnBufferCreate(MLN_HEAP_GENERAL, value1->size,
						       value1->data);
			if (link->value1 == NULL)
				goto nomem;
		}
		if (value2 != NULL) {
			link->value2 = MlnBufferCreate(MLN_HEAP_GENERAL, value2->size,
						       value2->data);
			if (link->value2 == NULL)
				goto nomem;
		}
	}
	if (head != NULL) {
		for (last = head; last->next != NULL; last = last->next)
			;
		last->next = link;
	} else {
		*links = link;
	}
	*key = slot;
	return 0;
nomem:
	ret = 2;
fail:
	MlnStoreLinksFree(link);
	return ret;
}

/* finds the link whose id digest is id */
__attribute__((noinline)) int MlnImpl_81049D54(MlnStoreLink *links, const MlnBuffer *id, MlnStoreLink **found)
{
	MlnStoreLink *link;

	if (id == NULL || found == NULL)
		return 3;
	*found = NULL;
	for (link = links; link != NULL; link = link->next) {
		if (MlnMemcmp(link->idDigest, id->data, 20) == 0) {
			*found = link;
			break;
		}
	}
	if (*found == NULL)
		return 0x230A0003;
	return 0;
}

/* finds the link from from to to */
__attribute__((noinline)) int MlnImpl_81049DDC(MlnStoreLink *links, const MlnBuffer *from, const MlnBuffer *to,
		     MlnStoreLink **found)
{
	int ret;

	if (to == NULL || found == NULL)
		return 3;
	*found = NULL;
	ret = MlnImpl_81048DF0(links, from, to, found, NULL);
	if (ret != 0)
		return ret;
	if (*found == NULL)
		return 0x230A0003;
	return 0;
}

/* takes link out of links and frees it, dropping the index entries it leaves unused */
static inline __attribute__((always_inline)) int MlnStoreLinkRemove(MlnStoreLink **links,
								   MlnStoreLink *link)
{
	MlnStoreLink *previous;
	int ret;

	if (links == NULL || link == NULL)
		return 3;
	if (*links == NULL)
		return 0;
	if (*links == link) {
		*links = link->next;
	} else {
		for (previous = *links; previous->next != NULL; previous = previous->next)
			if (previous->next == link)
				break;
		if (previous->next == NULL)
			return 0;
		previous->next = link->next;
	}
	link->next = NULL;
	ret = MlnImpl_810492F8(*links, link);
	MlnStoreLinksFree(link);
	return ret;
}

__attribute__((noinline)) int MlnImpl_81049E34(MlnStoreLink **links, MlnStoreLink *link)
{
	return MlnStoreLinkRemove(links, link);
}

/* removes the link with key from the stored links */
__attribute__((noinline)) int MlnImpl_81049F14(unsigned int key)
{
	MlnStoreLink *links;
	MlnStoreLink *link;
	MlnStoreLink *previous;
	int ret;

	if (key > 8)
		return 3;
	links = NULL;
	ret = MlnImpl_81049464(&links);
	if (ret != 0)
		goto out;
	if (links == NULL)
		return 0;
	if (links->key == key) {
		link = links;
		links = link->next;
	} else {
		for (previous = links; (link = previous->next) != NULL; previous = link)
			if (link->key == key)
				break;
		if (link == NULL)
			goto out;
		previous->next = link->next;
	}
	link->next = NULL;
	ret = MlnImpl_810492F8(links, link);
	MlnStoreLinksFree(link);
	if (ret == 0)
		ret = MlnImpl_81048FF0(links);
out:
	MlnStoreLinksFree(links);
	return ret;
}

/* removes every link one of whose ends has digest, and its record (table 7) */
int MlnImpl_8104A110(MlnStoreLink **links, const unsigned char *digest)
{
	unsigned char fromDigest[20];
	unsigned char toDigest[20];
	MlnStoreLink *link;
	MlnStoreLink *next;
	int ret;

	if (links == NULL)
		return 3;
	link = *links;
	while (link != NULL) {
		next = link->next;
		if (link->byValue == 1) {
			MlnMemset(fromDigest, 0, sizeof(fromDigest));
			MlnMemset(toDigest, 0, sizeof(toDigest));
			if (link->from != NULL) {
				ret = MlnImpl_810570A8(link->from->data, link->from->size,
						       fromDigest);
				if (ret != 0)
					return ret;
			}
			if (link->to != NULL) {
				ret = MlnImpl_810570A8(link->to->data, link->to->size, toDigest);
				if (ret != 0)
					return ret;
			}
			if (MlnMemcmp(fromDigest, digest, 20) != 0 &&
			    MlnMemcmp(toDigest, digest, 20) != 0) {
				link = link->next;
				continue;
			}
		} else {
			if (MlnMemcmp(link->toDigest, digest, 20) != 0 &&
			    MlnMemcmp(link->fromDigest, digest, 20) != 0) {
				link = link->next;
				continue;
			}
		}
		MlnImpl_8105DC0C(7, link->key, NULL);
		ret = MlnStoreLinkRemove(links, link);
		if (ret != 0)
			return ret;
		link = next;
	}
	return 0;
}

/* makes the link from from to to keep its ends and the two values themselves */
int MlnImpl_8104A300(MlnStoreLink *links, const MlnBuffer *from, const MlnBuffer *to,
		     const MlnBuffer *value1, const MlnBuffer *value2)
{
	MlnStoreLink *found;
	int ret;

	if (to == NULL)
		return 3;
	found = NULL;
	ret = MlnImpl_81048DF0(links, from, to, &found, NULL);
	if (ret != 0)
		return ret;
	if (found == NULL)
		return 0x230A0003;
	MlnMemset(found->toDigest, 0, 20);
	MlnMemset(found->fromDigest, 0, 20);
	MlnMemset(found->idDigest, 0, 20);
	if (from != NULL) {
		found->from = MlnBufferCreate(MLN_HEAP_GENERAL, from->size, from->data);
		if (found->from == NULL)
			return 2;
	}
	found->to = MlnBufferCreate(MLN_HEAP_GENERAL, to->size, to->data);
	if (found->to == NULL)
		return 2;
	if (value1 != NULL) {
		found->value1 = MlnBufferCreate(MLN_HEAP_GENERAL, value1->size, value1->data);
		if (found->value1 == NULL)
			return 2;
	}
	if (value2 != NULL) {
		found->value2 = MlnBufferCreate(MLN_HEAP_GENERAL, value2->size, value2->data);
		if (found->value2 == NULL)
			return 2;
	}
	found->byValue = 1;
	return 0;
}
