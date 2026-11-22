/*
 * The license store: the core's state records by step, the links table, the
 * stored records and the digest the core keeps (0x810436e4-)
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
int MlnImpl_81043860(const MlnBuffer *id, const MlnRecordArray *records, MlnRecord **out,
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
MlnStoreDigest *MlnImpl_81043A08(void)
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
