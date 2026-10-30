/*
 * Solaris device driver interfaces (Sony's dtrace/sce/solaris_ddi.c): the soft
 * state of the DTrace pseudo devices, and the device number helpers.
 */
#include <dtrace/sce/solaris_ddi.h>
#include <dtrace/sce/solaris_kmem.h>
#include <dtrace/sce/solaris_mutex.h>
#include <sceerror.h>
#include <sysmem/sysclib.h>

#define MIN_N_ITEMS	8	/* 8 void *'s == 32 bytes */

/*
 * A soft state: an array of items, grown by doubling; the arrays it outgrew
 * stay on the dirty list until the state is freed, as readers may hold them.
 */
struct i_ddi_soft_state {
	void **array;
	kmutex_t lock;
	size_t size;				/* the size of an item */
	size_t n_items;				/* the length of array */
	struct i_ddi_soft_state *next;		/* the dirty list */
};

/* the soft state ddi_get_soft_state was last asked about */
static struct i_ddi_soft_state *s_lastSoftState;

int ddi_soft_state_init(void **state_p, size_t size, size_t n_items)
{
	struct i_ddi_soft_state *ss;

	if (state_p == NULL || *state_p != NULL || size == 0)
		return SCE_ERROR_ERRNO_EINVAL;

	ss = kmem_zalloc(sizeof(*ss), KM_SLEEP);
	mutex_init(&ss->lock, "DdiSoftState", MUTEX_DRIVER, NULL);
	ss->size = size;

	if (n_items < MIN_N_ITEMS)
		ss->n_items = MIN_N_ITEMS;
	else
		ss->n_items = n_items;

	ss->array = kmem_zalloc(ss->n_items * sizeof(void *), KM_SLEEP);

	*state_p = ss;
	return 0;
}

int ddi_soft_state_zalloc(void *state, int item)
{
	struct i_ddi_soft_state *ss = state;
	void **array;
	void *new_element;

	if (ss == NULL)
		return DDI_FAILURE;

	mutex_enter(&ss->lock);
	if (ss->size == 0) {
		mutex_exit(&ss->lock);
		return DDI_FAILURE;
	}

	array = ss->array;
	if (item < ss->n_items && array[item] != NULL) {
		mutex_exit(&ss->lock);
		return DDI_FAILURE;
	}

	new_element = kmem_zalloc(ss->size, KM_SLEEP);

	if (item >= ss->n_items) {
		struct i_ddi_soft_state *dirty;
		void **new_array;
		size_t new_n_items;

		new_n_items = ss->n_items;
		while (new_n_items < (1 + item))
			new_n_items <<= 1;

		new_array = kmem_zalloc(new_n_items * sizeof(void *), KM_SLEEP);
		memcpy(new_array, array, ss->n_items * sizeof(void *));

		dirty = kmem_zalloc(sizeof(*dirty), KM_SLEEP);
		dirty->array = ss->array;
		dirty->n_items = ss->n_items;
		dirty->next = ss->next;
		ss->next = dirty;

		ss->array = (array = new_array);
		ss->n_items = new_n_items;
	}

	array[item] = new_element;

	mutex_exit(&ss->lock);
	return DDI_SUCCESS;
}

void *ddi_get_soft_state(void *state, int item)
{
	struct i_ddi_soft_state *ss = state;

	s_lastSoftState = ss;
	if (item < ss->n_items && ss->array != NULL)
		return ss->array[item];
	return NULL;
}

int ddi_get_last_soft_state(void **state_p)
{
	if (s_lastSoftState == NULL || state_p == NULL)
		return DDI_FAILURE;

	*state_p = s_lastSoftState;
	return DDI_SUCCESS;
}

void ddi_soft_state_free(void *state, int item)
{
	struct i_ddi_soft_state *ss = state;
	void **array;
	void *element = NULL;

	if (ss == NULL)
		return;

	mutex_enter(&ss->lock);
	if ((array = ss->array) != NULL && ss->size != 0 && item < ss->n_items &&
	    array[item] != NULL) {
		element = array[item];
		array[item] = NULL;
	}
	mutex_exit(&ss->lock);

	if (element != NULL)
		kmem_free(element, ss->size);
}

void ddi_soft_state_fini(void **state_p)
{
	struct i_ddi_soft_state *ss, *dirty;
	int item;

	if (state_p == NULL || (ss = *state_p) == NULL)
		return;

	if (ss->size == 0)
		return;

	if (ss->n_items > 0) {
		for (item = 0; item < ss->n_items; item++)
			ddi_soft_state_free(ss, item);
		kmem_free(ss->array, ss->n_items * sizeof(void *));
	}

	while ((dirty = ss->next) != NULL) {
		ss->next = dirty->next;
		kmem_free(dirty->array, dirty->n_items * sizeof(void *));
		kmem_free(dirty, sizeof(*dirty));
	}

	mutex_destroy(&ss->lock);
	kmem_free(ss, sizeof(*ss));

	*state_p = NULL;
}

void ddi_report_dev(dev_info_t *dip)
{
}

major_t getmajor(dev_t dev)
{
	return (dev >> 8) & 0xff;
}

int ddi_create_minor_node(dev_info_t *dip, const char *name, int spec_type,
    minor_t minor_num, const char *node_type, int flag)
{
	return DDI_SUCCESS;
}

void ddi_remove_minor_node(dev_info_t *dip, const char *name)
{
}

major_t getemajor(dev_t dev)
{
	return (dev >> 8) & 0xff;
}

minor_t getminor(dev_t dev)
{
	return dev & 0xff;
}

dev_t makedevice(major_t maj, minor_t min)
{
	return ((maj << 8) & 0xffff) | (min & 0xff);
}

int ddi_prop_lookup_int_array(dev_t match_dev, dev_info_t *dip, uint_t flags,
    const char *name, int **data, uint_t *nelements)
{
	return DDI_FAILURE;
}

int ddi_prop_free(void *data)
{
	dtrace_heap_free(data);
	return DDI_PROP_SUCCESS;
}

int kdi_dtrace_set(kdi_dtrace_set_t transition)
{
	return 0;
}
