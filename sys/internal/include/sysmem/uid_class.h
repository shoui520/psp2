/* SceSysmem UID class and object interfaces. */
#pragma once

#include <scetypes.h>

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
#ifndef __cplusplus
_Static_assert(sizeof(SceClass) == 0x2c, "SceClass");
#endif

#define SCE_CLASS_MAGIC		0xabce9da5	/* SceClass.magic */

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
#ifndef __cplusplus
_Static_assert(sizeof(SceObjectBase) == 8, "SceObjectBase");
#endif

SceClass *SceSysmemForDriver_85336A1C(void);	/* vitasdk: ksceKernelGetUIDClass */
SceClass *SceSysmemForKernel_E4193EB6(void);	/* vitasdk: ksceKernelGetUIDDLinkClass */
SceClass *SceSysmemForKernel_7C878E94(void);	/* vitasdk: ksceKernelGetUIDHeapClass */
SceClass *SceSysmemForKernel_86681B64(void);	/* vitasdk: ksceKernelGetUIDMemBlockClass */
int sceUIDClassInitClass(SceClass *cls, const char *name, void *uidclass,
			SceSize itemsize, SceClassCallback create, SceClassCallback destroy);
/* vitasdk: ksceKernelFindClassByName */
int SceSysmemForKernel_7D87F706(const char *name, SceClass **cls);

/* an entry's info, in the entry info walks */
typedef struct SceKernelUIDEntryInfo {
	SceSize size;				/* 0x24 */
	SceUID uid;
	SceObjectBase *object;
	SceClass *cls;
	SceUID guid;				/* 0x10 */
	SceUInt16 linkCount;
	SceUInt16 useCount;
	SceUInt32 attr;
	const char *name;
	SceUID pid;				/* 0x20 */
} SceKernelUIDEntryInfo;

int sceGUIDGetEntryInfoAll(SceUInt32 level, SceUInt32 state, SceKernelUIDEntryInfo *infos,
				SceSize size, SceSize *ret_num);
int PUIDGetPUIDVectorByGUID(SceUID pid, SceUID guid, SceUInt32 level, SceUID *vector, SceSize size, SceSize *ret_num);
int SceSysmemForKernel_02DAD732(void);
int scePUIDGetUIDVectorByClass(SceUID pid, SceClass *cls, SceUInt32 level, SceUID *vector, SceSize num, SceSize *ret_num);
int SceSysmemForKernel_16A54BFE(void);
SceUID SceSysmemForKernel_188C8742(SceUID pid, SceUID puid);
SceClass *sceGUIDtoClass(SceUID uid);
SceUID sceKernelAllocPartitionStackMemBlock(const char *name, SceSize size, SceSize extraLow);
SceUInt32 sceKernelCheckOpenVMDomain(void);	/* sceKernelCheckOpenVMDomain: positive while the process holds the VM domain */
int sceKernelAddressSpaceSetProcess(SceUID guid, SceUID pid);
int SceSysmemForKernel_7F7D5C4A(SceClass *cls, SceClass *base);
SceUID sceUIDCreateEntryHeap(SceUID pid, SceUID heapGuid);
int scePUIDReferObjectWithClass(SceUID pid, SceUID puid, SceClass *cls, SceObjectBase **out);
int SceSysmemForKernel_B591E49F(unsigned int type, SceSize size);
SceSize SceSysmemForKernel_BC36755F(void);
SceUID SceSysmemForKernel_BDE00106(const char *name);
int sceUIDDeleteEntryHeap(SceUID guid);
void *SceSysmemForKernel_FA6C098C(SceUID uid);
int UIDEntryReleaseObject(SceUID pid, SceUID puid);
int sceKernelAddressSpaceSetName(SceUID guid, const char *name);
int sceKernelObjectHeapSetResourceLimit(SceUID pid, int kind, unsigned int type, SceUInt32 max, SceUInt32 reserve);
int sceKernelSysmemModuleStartAfterProcessmgr(void);
int scePUIDGetAttr(SceUID pid, SceUID puid, SceUInt32 *attr);

#ifdef __cplusplus
}
#endif
