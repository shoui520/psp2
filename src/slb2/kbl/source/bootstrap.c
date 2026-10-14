/* Reconstructed KBL UID-class and ModuleMgr bootstrap foundations.
 * External synchronization/heap/diagnostic entries retain their native ABI.
 */
#include <stdint.h>
#include <stddef.h>

typedef struct UIDClass UIDClass;
struct UIDClass {
	UIDClass *next;
	UIDClass *type;
	UIDClass *prev;
	const char *name;
	UIDClass *parent;
	uint32_t flags;
	uint16_t size;
	uint16_t allocationSize;
	uint32_t unk1c;
	int (*constructor)(void *);
	int (*destructor)(void *);
	uint32_t magic;
};
_Static_assert(sizeof(UIDClass) == 0x2c, "native class ABI");
extern UIDClass *g_uidClass;
extern uint8_t *g_uidSysroot;
extern uint8_t *g_bootSysroot;
extern UIDClass *g_moduleClass, *g_libraryClass, *g_stubClass;
extern uint8_t *g_kernelLibraryDB;
extern const char SceUIDClassName[], SceUIDModuleClassName[];
extern const char SceUIDLibraryClassName[], SceUIDLibStubClassName[];
extern int CoreIndex(void);
extern int CoreEnter(void *, unsigned int);
extern int CoreLeave(void *);
extern int BootSpinLock(void *);
extern void BootSpinUnlock(void *, int);
extern void *ModuleHeapAlloc(uint32_t);
extern void *KblMemset(void *, int, uint32_t);
extern int KblStrncmp(const char *, const char *, uint32_t);
extern int GetAssertLevel(void);
extern void PrintfWithInfo(int, int, const void *, const char *, ...);
extern void AssertLevel(int, int, const void *, uintptr_t);
extern void AssertWithInfo(int, const void *, uintptr_t);
extern const uint8_t RegisterZeroRecord[], RegisterDuplicateRecord[];
extern const uint8_t RegisterNameRecord[], RegisterAssertRecord[];
extern const char RegisterZeroFormat[], RegisterDuplicateFormat[];
extern const char RegisterNameFormat[];
extern const uint8_t BootAllocFailureRecord[], BootAllocAssertRecord[];
extern const uint8_t BootAllocTraceRecord[];
extern const char BootAllocFailureFormat[], BootAllocTraceFormat[];
extern const uint8_t ModuleAllocRecord[], ModuleAllocAssertRecord[];
extern const char ModuleAllocFormat[];

int __attribute__((section(".root_ctor"))) UIDClassConstructor(void *object)
{
	*(uint32_t *)object = 0;
	return 0;
}
int __attribute__((section(".root_dtor"))) UIDClassDestructor(void *object)
{
	(void)object;
	return 0;
}
#define EMPTY_CALLBACK(name, sec) \
int __attribute__((section(sec))) name(void *object) \
{ (void)object; return 0; }
int __attribute__((section(".module_ctor"))) ModuleConstructor(void *object)
{
	KblMemset((uint8_t *)object + 8, 0, 0xe8);
	return 0;
}
EMPTY_CALLBACK(ModuleDestructor, ".module_dtor")
EMPTY_CALLBACK(LibraryConstructor, ".library_ctor")
int __attribute__((section(".library_dtor"))) LibraryDestructor(void *object)
{
	*(uint32_t *)((uint8_t *)object + 8) = 0;
	return 0;
}
EMPTY_CALLBACK(StubConstructor, ".stub_ctor")
int __attribute__((section(".stub_dtor"))) StubDestructor(void *object)
{
	*(uint32_t *)((uint8_t *)object + 8) = 0;
	return 0;
}

int RegisterClass(UIDClass *type)
{
	UIDClass *first = *(UIDClass **)(g_uidSysroot + 4);
	UIDClass *entry = first;
	uint32_t largest = 0;
	uintptr_t caller = (uintptr_t)__builtin_return_address(0);

	if (!first) {
		type->next = type;
		type->prev = type;
		type->flags = (type->flags & ~0xffu) | 1;
		*(UIDClass **)(g_uidSysroot + 4) = type;
		return 0;
	}
	do {
#if FW_DEBUG
		if (!(entry->flags & 0xff))
			PrintfWithInfo(1, 11, RegisterZeroRecord,
				       RegisterZeroFormat, largest, entry, 0,
				       entry->name);
#endif
		if (entry == type) {
#if FW_DEBUG
			PrintfWithInfo(1, 11, RegisterDuplicateRecord,
				       RegisterDuplicateFormat, entry, entry);
#endif
			return 0;
		}
		if (!KblStrncmp(entry->name, type->name, 31)) {
#if FW_DEBUG
			PrintfWithInfo(1, 11, RegisterNameRecord,
				       RegisterNameFormat, type, type->name);
#endif
			return 0;
		}
		if ((entry->flags & 0xff) >= largest)
			largest = entry->flags & 0xff;
		entry = entry->next;
	} while (entry != first);
	/* Native insertion precedes the unchanged first list element. */
	type->next = entry;
	type->prev = entry->prev;
	if (entry->prev)
		entry->prev->next = type;
	entry->prev = type;
	if (largest != 0xff)
		largest = (largest + 1) & 0xff;
	type->flags = (type->flags & ~0xffu) | largest;
#if FW_DEBUG
	if (GetAssertLevel() > 0)
		AssertLevel(1, (type->flags & 0xff) == largest,
			    RegisterAssertRecord, caller);
#else
	(void)caller;
#endif
	return 0;
}

int UIDClassInitClass(UIDClass *type, const char *name, UIDClass *parent,
		      uint32_t size, int (*constructor)(void *),
		      int (*destructor)(void *))
{
	uint32_t rounded = size <= 4 ? 4 : size <= 8 ? 8 :
			   size <= 16 ? 16 : (size + 31) & 0xffe0;

	type->size = size;
	type->name = name;
	type->type = g_uidClass;
	type->parent = parent;
	type->allocationSize = rounded;
	type->constructor = constructor;
	type->destructor = destructor;
	type->magic = 0xabce9da5;
	type->flags = 0;
	type->unk1c = 0;
	RegisterClass(type);
	return 0;
}

void *SysrootBootAllocate(uint32_t size)
{
	uint8_t *root = g_bootSysroot;
	int flags = BootSpinLock(root + 0x10);
	uint32_t extent = *(uint32_t *)(root + 0x14);
	uintptr_t cursor = *(uint32_t *)(root + 0x18);
	uint32_t end = cursor + size;
	uintptr_t caller = (uintptr_t)__builtin_return_address(0);

	if (end >= (uint32_t)(uintptr_t)root + extent) {
#if FW_DEBUG
		PrintfWithInfo(1, 11, BootAllocFailureRecord,
			       BootAllocFailureFormat, cursor, size, root, extent);
#endif
		BootSpinUnlock(g_bootSysroot + 0x10, flags);
#if FW_DEBUG
		if (GetAssertLevel() > 0)
			AssertLevel(1, 0, BootAllocAssertRecord, caller);
#endif
		return NULL;
	}
	*(uint32_t *)(root + 0x18) = (end + 31) & ~31u;
	BootSpinUnlock(g_bootSysroot + 0x10, flags);
#if FW_DEBUG
	if (*(uint32_t *)(root + 0x28) > 2)
		PrintfWithInfo(1, 11, BootAllocTraceRecord,
			       BootAllocTraceFormat, cursor, size, caller);
#else
	(void)caller;
#endif
	return (void *)cursor;
}

int InitializeUIDClass(void *bootContext)
{
	UIDClass *type;

	CoreEnter(g_bootSysroot + 0x2c, 0);
	if (!CoreIndex()) {
		type = SysrootBootAllocate(sizeof(*type));
		*(UIDClass **)((uint8_t *)bootContext + 0x98) = type;
		g_uidClass = type;
		type->flags = 0;
		type->type = type;
		type->constructor = UIDClassConstructor;
		type->destructor = UIDClassDestructor;
		type->parent = NULL;
		type->unk1c = 0;
		type->magic = 0xabce9da5;
		type->name = SceUIDClassName;
		type->size = 8;
		type->allocationSize = 8;
		RegisterClass(type);
		*(UIDClass **)(g_uidSysroot + 0x98) = g_uidClass;
	}
	CoreLeave(g_bootSysroot + 0x2c);
	return 0;
}

void SetModulePrivate(uint32_t index, void *value)
{
	*(void **)(g_bootSysroot + 0x34 + index * 4) = value;
}

int sceKernelModulemgrStart(void *bootContext)
{
	uint8_t *block;
	uintptr_t caller = (uintptr_t)__builtin_return_address(0);

	CoreEnter(*(void **)((uint8_t *)bootContext + 0x200), 0);
	if (!CoreIndex()) {
		block = ModuleHeapAlloc(0xa8);
#if FW_DEBUG
		if (!block) {
			PrintfWithInfo(1, 11, ModuleAllocRecord,
				       ModuleAllocFormat);
			AssertWithInfo(0, ModuleAllocAssertRecord, caller);
		}
#else
		(void)caller;
#endif
		KblMemset(block, 0, 0xa8);
		SetModulePrivate(9, block);
		*(uint32_t *)(block + 0x84) = 0x10005;
		g_kernelLibraryDB = block + 0x84;
		g_moduleClass = (UIDClass *)block;
		g_libraryClass = (UIDClass *)(block + 0x2c);
		g_stubClass = (UIDClass *)(block + 0x58);
		UIDClassInitClass(g_moduleClass, SceUIDModuleClassName, g_uidClass,
				  0xf4, ModuleConstructor, ModuleDestructor);
		UIDClassInitClass(g_libraryClass, SceUIDLibraryClassName, g_uidClass,
				  0x10, LibraryConstructor, LibraryDestructor);
		UIDClassInitClass(g_stubClass, SceUIDLibStubClassName, g_uidClass,
				  0x10, StubConstructor, StubDestructor);
	}
	return CoreLeave(*(void **)((uint8_t *)bootContext + 0x200));
}
