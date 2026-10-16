/* SceSysmem UID class and object interfaces. */
#pragma once

#include <psp2kern/types.h>
#include <vitasdk/build_utils.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*SceClassCallback)(void *item);

typedef struct SceClass {
	struct SceClass *next;
	struct SceClass *root;
	struct SceClass *prev;
	const char *name;
	struct SceClass *uidclass;
	unsigned int attributes;
	unsigned short itemsize;
	unsigned short itemsize_aligned;
	unsigned int defaultObjectType;
	SceClassCallback create_cb;
	SceClassCallback destroy_cb;
	unsigned int magic;
} SceClass;
VITASDK_BUILD_ASSERT_EQ(0x2c, SceClass);

typedef struct SceObjectBase {
	union {
		uint32_t sce_reserved[2];
		struct {
			void *object;
			SceClass *sce_class;
		};
	};
	uint32_t data[];
} SceObjectBase;
VITASDK_BUILD_ASSERT_EQ(8, SceObjectBase);

SceClass *ksceKernelGetUIDClass(void);
SceClass *ksceKernelGetUIDDLinkClass(void);
SceClass *ksceKernelGetUIDHeapClass(void);
SceClass *ksceKernelGetUIDMemBlockClass(void);
int ksceUIDClassInitClass(SceClass *cls, const char *name, void *uidclass,
			SceSize itemsize, SceClassCallback create, SceClassCallback destroy);
int ksceKernelFindClassByName(const char *name, SceClass **cls);

#define ksceKernelCreateClass(cls, name, uidclass, itemsize, create, destroy) \
	ksceUIDClassInitClass(cls, name, uidclass, itemsize, create, destroy)
#define ksceKernelGetUidClass() ksceKernelGetUIDClass()
#define ksceKernelGetUidDLinkClass() ksceKernelGetUIDDLinkClass()
#define ksceKernelGetUidHeapClass() ksceKernelGetUIDHeapClass()
#define ksceKernelGetUidMemBlockClass() ksceKernelGetUIDMemBlockClass()

#ifdef __cplusplus
}
#endif
