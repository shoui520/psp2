/*
 * SceNpCommonPs4: Scope (npext/handle/scope.cpp), a Cancelable that is attached to a Handle for as long as
 * an operation runs.
 */

#include "../../np_common_ps4_internal.h"

extern "C" {

extern const void *g_scopeVtable[];	/* 0x8102a704 */

struct NpCommonPs4Scope *SceNpCommonPs4_37C3D094(struct NpCommonPs4Scope *scope)
{
	SceNpCommonPs4_EB1FBB74(&scope->cancelable);
	scope->cancelable.vtable = NP_COMMON_PS4_VTABLE(g_scopeVtable);
	scope->handle = NULL;
	return scope;
}

struct NpCommonPs4Scope *SceNpCommonPs4_5BBB1ABA(struct NpCommonPs4Scope *scope)
{
	SceNpCommonPs4_EB1FBB74(&scope->cancelable);
	scope->cancelable.vtable = NP_COMMON_PS4_VTABLE(g_scopeVtable);
	scope->handle = NULL;
	return scope;
}

struct NpCommonPs4Scope *SceNpCommonPs4_9486DE09(struct NpCommonPs4Scope *scope)
{
	struct NpCommonPs4Handle *handle;

	scope->cancelable.vtable = NP_COMMON_PS4_VTABLE(g_scopeVtable);
	handle = scope->handle;
	if (handle != NULL)
		SceNpCommonPs4_6BF7B5BE(&handle->cancelable, &scope->cancelable);
	SceNpCommonPs4_C6E3FE21(&scope->cancelable);
	return scope;
}

void SceNpCommonPs4_A3174D9A(struct NpCommonPs4Scope *scope)
{
	struct NpCommonPs4Handle *handle;

	scope->cancelable.vtable = NP_COMMON_PS4_VTABLE(g_scopeVtable);
	handle = scope->handle;
	if (handle != NULL)
		SceNpCommonPs4_6BF7B5BE(&handle->cancelable, &scope->cancelable);
	SceNpCommonPs4_C6E3FE21(&scope->cancelable);
	operator delete(scope);
}

struct NpCommonPs4Scope *SceNpCommonPs4_101259AD(struct NpCommonPs4Scope *scope)
{
	struct NpCommonPs4Handle *handle;

	scope->cancelable.vtable = NP_COMMON_PS4_VTABLE(g_scopeVtable);
	handle = scope->handle;
	if (handle != NULL)
		SceNpCommonPs4_6BF7B5BE(&handle->cancelable, &scope->cancelable);
	SceNpCommonPs4_C6E3FE21(&scope->cancelable);
	return scope;
}

int SceNpCommonPs4_DDFD56E9(struct NpCommonPs4Scope *scope, struct NpCommonPs4Handle *handle, int arg1, int arg2, int arg3)
{
	int result;
	int attached;

	if (scope->handle != NULL)
		NP_COMMON_PS4_ASSERT(0, "Begin", "npext\\handle\\scope.cpp", 33, "m_handle == 0");
	result = SceNpCommonPs4_2CA5AE6A(&scope->cancelable);
	if (result >= 0) {
		attached = SceNpCommonPs4_E70C78DB(&handle->cancelable, &scope->cancelable, arg1, arg2, arg3);
		if (attached >= 0) {
			scope->handle = handle;
			return 0;
		}
		SceNpCommonPs4_9BCAE348(&scope->cancelable);
		return attached;
	}
	return result;
}

int SceNpCommonPs4_3D0FA7C9(struct NpCommonPs4Scope *scope, int result, int arg1, int arg2, int arg3)
{
	struct NpCommonPs4Handle *handle;
	int status;

	handle = scope->handle;
	if (handle != NULL) {
		SceNpCommonPs4_6BF7B5BE(&handle->cancelable, &scope->cancelable);
		scope->handle = NULL;
	}
	status = SceNpCommonPs4_3C8364C3(&scope->cancelable, arg1, arg2, arg3);
	if (status >= 0)
		status = result;
	SceNpCommonPs4_9BCAE348(&scope->cancelable);
	return status;
}

const void *g_scopeVtable[] = {		/* 0x8102a6fc */
	NULL,
	NULL,
	(const void *)SceNpCommonPs4_A6C7D48D,
	(const void *)SceNpCommonPs4_9486DE09,
	(const void *)SceNpCommonPs4_A3174D9A,
	(const void *)npCommonPs4Breakpoint,
};

}
