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

int PUIDGetPUIDVectorByGUID(SceUID pid, SceUID guid, SceUInt32 level, SceUID *vector, SceSize size, SceSize *ret_num);
int SceSysmemForKernel_02DAD732(void);
int SceSysmemForKernel_08C05493(SceUID pid, SceClass *cls, SceUInt32 level, SceUID *vector, SceSize num, SceSize *ret_num);
int SceSysmemForKernel_16A54BFE(void);
SceUID SceSysmemForKernel_188C8742(SceUID pid, SceUID puid);
SceClass *SceSysmemForKernel_368F1C36(SceUID uid);
SceUID SceSysmemForKernel_40870E74(const char *name, SceSize size, SceSize extraLow);
SceUInt32 SceSysmemForKernel_4E6D8BC3(void);	/* sceKernelCheckOpenVMDomain: positive while the process holds the VM domain */
int SceSysmemForKernel_5E772E19(SceUID guid, SceUID pid);
int SceSysmemForKernel_7F7D5C4A(SceClass *cls, SceClass *base);
SceUID SceSysmemForKernel_89BEF058(SceUID pid, SceUID heapGuid);
int SceSysmemForKernel_9C23DFF7(SceUID pid, SceUID puid, SceClass *cls, SceObjectBase **out);
int SceSysmemForKernel_B591E49F(unsigned int type, SceSize size);
SceSize SceSysmemForKernel_BC36755F(void);
SceUID SceSysmemForKernel_BDE00106(const char *name);
int SceSysmemForKernel_F6D09BD9(SceUID guid);
void *SceSysmemForKernel_FA6C098C(SceUID uid);
int UIDEntryReleaseObject(SceUID pid, SceUID puid);
int sceKernelAddressSpaceSetName(SceUID guid, const char *name);
int sceKernelObjectHeapSetResourceLimit(SceUID pid, int kind, unsigned int type, SceUInt32 max, SceUInt32 reserve);
int sceKernelSysmemModuleStartAfterProcessmgr(void);
int scePUIDGetAttr(SceUID pid, SceUID puid, SceUInt32 *attr);

#ifdef __cplusplus
}
#endif
