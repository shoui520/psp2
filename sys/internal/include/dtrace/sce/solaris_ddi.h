/*
 * Solaris device driver interfaces (Sony's dtrace/sce/solaris_ddi.c): the
 * DTrace providers attach as pseudo devices, their minor numbers and soft
 * state kept here.
 */
#pragma once

#include <dtrace/sce/solaris_types.h>

#define DDI_SUCCESS	0
#define DDI_FAILURE	(-1)

#define DDI_PROP_SUCCESS	0
#define DDI_PROP_NOT_FOUND	1
#define DDI_PROP_DONTPASS	0x0001
#define DDI_DEV_T_ANY		((dev_t)-2)

#define DDI_PSEUDO	"ddi_pseudo"
#define S_IFCHR		0x2000

typedef enum {
	DDI_ATTACH = 0,
	DDI_RESUME = 1,
	DDI_PM_RESUME = 2
} ddi_attach_cmd_t;

typedef enum {
	DDI_DETACH = 0,
	DDI_SUSPEND = 1,
	DDI_PM_SUSPEND = 2,
	DDI_HOTPLUG_DETACH = 3
} ddi_detach_cmd_t;

typedef enum {
	DDI_INFO_DEVT2DEVINFO = 0,
	DDI_INFO_DEVT2INSTANCE = 1
} ddi_info_cmd_t;

/* a provider's device: DTrace's clients keep one per provider (dtrace_client.c) */
struct dev_info {
	int attached;		/* set by the client code once attach succeeds */
};

typedef struct dev_info dev_info_t;

int ddi_soft_state_init(void **state_p, size_t size, size_t n_items);
int ddi_soft_state_zalloc(void *state, int item);
void *ddi_get_soft_state(void *state, int item);
int ddi_get_last_soft_state(void **state_p);
void ddi_soft_state_free(void *state, int item);
void ddi_soft_state_fini(void **state_p);

void ddi_report_dev(dev_info_t *dip);
major_t getmajor(dev_t dev);
int ddi_create_minor_node(dev_info_t *dip, const char *name, int spec_type,
    minor_t minor_num, const char *node_type, int flag);
void ddi_remove_minor_node(dev_info_t *dip, const char *name);
major_t getemajor(dev_t dev);
minor_t getminor(dev_t dev);
dev_t makedevice(major_t maj, minor_t min);
int ddi_prop_lookup_int_array(dev_t match_dev, dev_info_t *dip, uint_t flags,
    const char *name, int **data, uint_t *nelements);
int ddi_prop_free(void *data);

/* the device a dev_info_t stands for is in its address: Sony's major number from it */
#define ddi_driver_major(dip)	getmajor((dev_t)(dip))

/* a D program's getmajor() and getminor(), worked out in place */
#define major(dev)	(((dev) >> 8) & 0xff)
#define minor(dev)	((dev) & 0xff)
#define ddi_getprop(dev, dip, flags, name, defvalue)	(defvalue)
#define nulldev		NULL

/* the kernel debugger's hold on DTrace: there is none to refuse it */
typedef enum {
	KDI_DTSET_DTRACE_ACTIVATE,
	KDI_DTSET_DTRACE_DEACTIVATE,
	KDI_DTSET_KMDB_BPT_ACTIVATE,
	KDI_DTSET_KMDB_BPT_DEACTIVATE
} kdi_dtrace_set_t;

int kdi_dtrace_set(kdi_dtrace_set_t transition);
