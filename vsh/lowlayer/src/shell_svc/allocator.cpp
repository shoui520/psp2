#include <stddef.h>
#include <libkernel/libkernel.h>

/*
 * The library's C++ runtime: operator delete frees into the library's mspace, and a pure
 * virtual call stops at a breakpoint. The library never creates the mspace or allocates
 * from it (its MspaceCreate, MspaceMalloc and MspaceDestroy imports are unused), so only
 * the destructors' deletes reach it. The file and helper names are ours.
 */

/* Only the stripped allocator set it, so it is external: the compiler must load it. */
void *g_shellSvcMspace;

static __attribute__((noinline)) void deallocate(void *pointer);

void operator delete(void *pointer)
{
	deallocate(pointer);
}

extern "C" void __cxa_pure_virtual(void)
{
	__breakpoint(0);
}

void *operator new(size_t size, void *where)
{
	return where;
}

static void deallocate(void *pointer)
{
	sceClibMspaceFree(g_shellSvcMspace, pointer);
}
