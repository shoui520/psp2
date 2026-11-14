// cppsemantics.cpp: the C++ side of the ARM exception handling ABI: the exception objects and
// their emergency buffer, throw and rethrow, catch and cleanup, the matching of a thrown type
// against a handler's, exception_ptr's reference counts, and the per-thread exception state.
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <exception>
#include <typeinfo>
#include "libc_internal.h"
#include "cxxabi_internal.h"

#define EMERGENCY_BUFFER_SIZE	0x88	/* one exception object when malloc fails */
#define EMERGENCY_OBJECT_MAX	(EMERGENCY_BUFFER_SIZE - 4)

extern "C" {
__cxa_eh_globals *__rt_eh_globals_addr(void);
void __cxa_bad_typeid(void);
int __derived_to_base_conversion(void **objptr, void **foundptr,
	const __cxxabiv1::__class_type_info *derived, const _STD type_info *base,
	int flags, int offset);
}

static const char ARMCPP_EXCEPTION_CLASS[8] = {'A', 'R', 'M', '\0', 'C', '+', '+', '\0'};
static const char ARMCPP_DEPENDENT_CLASS[8] = {'A', 'R', 'M', '\0', 'C', '+', '+', '\1'};

static void *null_pointer[2] = {0, 0};	/* what nullptr is caught as by a pointer */
static long null_data_member_pointer = -1;	/* ... and by a pointer to data member */

static inline __cxa_exception *ucbp_to_ep(_Unwind_Control_Block *ucbp)
	{	// the header around an exception's UCB
	return ((__cxa_exception *)((char *)ucbp - offsetof(__cxa_exception, ucb)));
	}

static inline __cxa_exception *object_to_ep(void *object)
	{	// the header in front of a thrown object
	return ((__cxa_exception *)object - 1);
	}

bool is_foreign_exception(_Unwind_Control_Block *ucbp)
	{	// test whether another language threw the exception
	return (memcmp(ucbp->exception_class, ARMCPP_EXCEPTION_CLASS, 7) != 0);
	}

extern "C" void *__ARM_exceptions_buffer_init(void)
	{	// get this thread's emergency buffer, free
	unsigned char *buffer = (unsigned char *)_Gettlsbuf(EMERGENCY_BUFFER_SIZE);

	if (buffer == 0)
		return (0);
	buffer[0] = 0;
	return (buffer);
	}

_STD_BEGIN
void _Debug_message(const char *, const char *);
_STD_END

static void default_unexpected_handler();
static void default_terminate_handler();
static void exception_cleanup(_Unwind_Reason_Code, _Unwind_Control_Block *);
static void dependent_exception_cleanup(_Unwind_Reason_Code, _Unwind_Control_Block *);

static inline void eh_globals_init(__cxa_eh_globals *g)
	{	// give a thread its initial exception state
	g->unexpectedHandler = default_unexpected_handler;
	g->uncaughtExceptions = 0;
	g->implementation_ever_called_terminate = false;
	g->terminateHandler = default_terminate_handler;
	g->call_hook = 0;
	g->caughtExceptions = 0;
	g->propagatingExceptions = 0;
	g->emergency_buffer = __ARM_exceptions_buffer_init();
	}

extern "C" __cxa_eh_globals *__cxa_get_globals()
	{	// get this thread's exception state, initialized
	__cxa_eh_globals *g = __rt_eh_globals_addr();

	if (!g->initialized)
		{	// first use in this thread
		g->initialized = 1;
		eh_globals_init(g);
		}
	else if (g->terminateHandler == 0)
		eh_globals_init(g);
	return (g);
	}

extern "C" const _STD type_info *__cxa_current_exception_type()
	{	// get the type of the exception being handled
	__cxa_exception *ep = __cxa_get_globals()->caughtExceptions;

	if (ep == 0 || is_foreign_exception(&ep->ucb))
		return (0);
	return (ep->exceptionType);
	}

extern "C" void *__ARM_exceptions_buffer_allocate(void *buffer, size_t size)
	{	// take the emergency buffer for an exception object
	unsigned char *p = (unsigned char *)buffer;

	if (EMERGENCY_OBJECT_MAX < size || p == 0 || p[0] != 0)
		return (0);
	p[0] = 1;
	return (p + 4);
	}

extern "C" void *__ARM_exceptions_buffer_free(void *buffer, void *addr)
	{	// give the emergency buffer back, if it holds addr
	unsigned char *p = (unsigned char *)buffer;

	if (p == 0 || addr != p + 4)
		return (0);
	p[0] = 0;
	return (p);
	}

void eh_call_terminate(_Unwind_Control_Block *ucbp)
	{	// terminate with the handler that was current at the throw
	__cxa_eh_globals *g = __cxa_get_globals();
	_STD terminate_handler handler;

	if (ucbp == 0)
		handler = g->terminateHandler;
	else if (is_foreign_exception(ucbp))
		handler = g->terminateHandler;
	else
		handler = ucbp_to_ep(ucbp)->terminateHandler;
	g->call_hook = handler;
	g->implementation_ever_called_terminate = true;
	_STD terminate();
	}

extern "C" void *__cxa_allocate_exception(size_t size)
	{	// allocate a thrown object and its header
	size_t total = size + sizeof (__cxa_exception);
	__cxa_eh_globals *g = __cxa_get_globals();
	__cxa_exception *ep = (__cxa_exception *)malloc(total);

	if (ep == 0)
		{	// use the emergency buffer
		ep = (__cxa_exception *)__ARM_exceptions_buffer_allocate(g->emergency_buffer,
			total);
		if (ep == 0)
			eh_call_terminate(0);
		}
	memcpy(ep->ucb.exception_class, ARMCPP_EXCEPTION_CLASS, 8);
	ep->ucb.exception_cleanup = 0;
	ep->nextCaughtException = 0;
	ep->nextPropagatingException = 0;
	ep->terminateHandler = g->terminateHandler;
	ep->ucb.unwinder_cache.reserved1 = 0;
	ep->handlerCount = 0;
	ep->propagationCount = 0;
	ep->unexpectedHandler = g->unexpectedHandler;
	return (ep + 1);
	}

bool exception_class_matches(const unsigned int *class1, const unsigned int *class2)
	{	// compare two exception classes
	return (class1[0] == class2[0] && class1[1] == class2[1]);
	}

__cxa_exception *get_foreign_intermediary(__cxa_exception *head_ep,
	_Unwind_Control_Block *ucbp)
	{	// get the C++ header that stands for a foreign exception
	if (head_ep != 0
		&& exception_class_matches((const unsigned int *)head_ep->ucb.exception_class,
			(const unsigned int *)ucbp->exception_class)
		&& head_ep->exceptionType == (const _STD type_info *)ucbp)
		return (head_ep);	// this one's own

	__cxa_exception *ep = object_to_ep(__cxa_allocate_exception(0));

	for (int i = 0; i < 8; ++i)
		ep->ucb.exception_class[i] = ucbp->exception_class[i];
	ep->propagationCount = 0;
	ep->nextCaughtException = 0;
	ep->exceptionType = (const _STD type_info *)ucbp;
	ep->handlerCount = 0;
	return (ep);
	}

extern "C" void *__cxa_begin_catch(_Unwind_Control_Block *ucbp)
	{	// start handling an exception, and get the object caught
	void *object = (void *)ucbp->barrier_cache.bitpattern[0];
	__cxa_eh_globals *g = __cxa_get_globals();
	__cxa_exception *ep;

	if (is_foreign_exception(ucbp))
		ep = get_foreign_intermediary(g->caughtExceptions, ucbp);
	else
		{	// a C++ exception stops propagating
		ep = ucbp_to_ep(ucbp);
		--ep->propagationCount;
		--g->uncaughtExceptions;
		}
	++ep->handlerCount;
	if (g->caughtExceptions != ep)
		{	// push it on the caught stack
		ep->nextCaughtException = g->caughtExceptions;
		g->caughtExceptions = ep;
		}
	_Unwind_Complete(ucbp);
	return (object);
	}

extern "C" void __cxa_call_terminate(_Unwind_Control_Block *ucbp)
	{	// terminate while an exception propagates
	if (ucbp != 0)
		__cxa_begin_catch(ucbp);
	eh_call_terminate(ucbp);
	}

bool is_dependent_exception(_Unwind_Control_Block *ucbp)
	{	// test whether the exception rethrows a primary exception
	return (memcmp(ucbp->exception_class, ARMCPP_DEPENDENT_CLASS, 8) == 0);
	}

extern "C" __cxa_type_match_result __cxa_type_match(_Unwind_Control_Block *ucbp,
	const _STD type_info *match_type, bool is_reference_type, void **matched_object)
	{	// test whether a handler of match_type catches the exception
	if (is_foreign_exception(ucbp))
		return (ctm_failed);

	__cxa_exception *ep = ucbp_to_ep(ucbp);

	if (is_dependent_exception(ucbp))
		ep = object_to_ep(ep->primaryException);

	const _STD type_info *thrown_type = ep->exceptionType;
	void *object = ep + 1;
	void *original_object = object;
	int pointer_depth = 0;
	bool all_const = true;	// every outer pointer level is const
	const _STD type_info *thrown_kind;

	for (;;)
		{	// strip matching pointer levels
		if (*thrown_type == *match_type)
			{	// an exact match
			*matched_object = original_object;
			return (ctm_succeeded);
			}

		thrown_kind = &typeid(*thrown_type);
		const _STD type_info *match_kind = &typeid(*match_type);
		bool match_is_pointer =
			*match_kind == typeid(__cxxabiv1::__pointer_type_info);
		bool match_is_member =
			*match_kind == typeid(__cxxabiv1::__pointer_to_member_type_info);
		bool thrown_is_nullptr = *thrown_type == typeid(decltype(nullptr));

		if ((match_is_pointer || match_is_member) && thrown_is_nullptr)
			{	// nullptr converts to any pointer
			void *null_object = null_pointer;

			if (match_is_member)
				{	// a pointer to data member's null is -1
				const __cxxabiv1::__pbase_type_info *member =
					(const __cxxabiv1::__pbase_type_info *)match_type;

				if (!(typeid(*member->__pointee)
					== typeid(__cxxabiv1::__function_type_info)))
					null_object = &null_data_member_pointer;
				}
			*matched_object = null_object;
			return (ctm_succeeded);
			}

		if ((*thrown_kind == typeid(__cxxabiv1::__pointer_type_info)
			|| *match_kind == typeid(__cxxabiv1::__pointer_type_info))
			&& *thrown_kind != *match_kind)
			return (ctm_failed);	// a pointer and a non-pointer
		if (!(*thrown_kind == typeid(__cxxabiv1::__pointer_type_info)))
			break;

		const __cxxabiv1::__pbase_type_info *thrown_pointer =
			(const __cxxabiv1::__pbase_type_info *)thrown_type;
		const __cxxabiv1::__pbase_type_info *match_pointer =
			(const __cxxabiv1::__pbase_type_info *)match_type;
		unsigned int match_flags = match_pointer->__flags;
		unsigned int thrown_quals = thrown_pointer->__flags
			& (__cxxabiv1::__pbase_type_info::__const_mask
				| __cxxabiv1::__pbase_type_info::__volatile_mask);
		unsigned int match_quals = match_flags
			& (__cxxabiv1::__pbase_type_info::__const_mask
				| __cxxabiv1::__pbase_type_info::__volatile_mask);

		if ((thrown_quals & ~match_quals) != 0)
			return (ctm_failed);	// the conversion drops qualifiers
		if ((match_quals & ~thrown_quals) != 0 && !all_const)
			return (ctm_failed);	// qualifiers added below a non-const level
		if ((match_flags & __cxxabiv1::__pbase_type_info::__const_mask) == 0)
			all_const = false;
		thrown_type = thrown_pointer->__pointee;
		match_type = match_pointer->__pointee;
		++pointer_depth;
		if (object != 0)
			object = *(void **)object;
		}

	if (pointer_depth == 1 && *match_type == typeid(void))
		{	// any object pointer converts to void *
		if (is_reference_type)
			return (ctm_failed);
		*matched_object = original_object;
		return (ctm_succeeded);
		}

	void *found;

	if ((pointer_depth == 0 || (pointer_depth == 1 && !is_reference_type))
		&& (*thrown_kind == typeid(__cxxabiv1::__si_class_type_info)
			|| *thrown_kind == typeid(__cxxabiv1::__vmi_class_type_info))
		&& __derived_to_base_conversion(&object, &found,
			(const __cxxabiv1::__class_type_info *)thrown_type, match_type, 0, 0))
		{	// a base class of the thrown class
		*matched_object = found;
		return (pointer_depth == 0 ? ctm_succeeded : ctm_succeeded_with_ptr_to_base);
		}
	return (ctm_failed);
	}

extern "C" void __cxa_free_exception(void *object)
	{	// free a thrown object and its header
	void *buffer = __cxa_get_globals()->emergency_buffer;
	__cxa_exception *ep = object_to_ep(object);

	if (__ARM_exceptions_buffer_free(buffer, ep) == 0)
		free(ep);
	}

extern "C" void __cxa_end_catch()
	{	// finish handling the current exception
	__cxa_eh_globals *g = __cxa_get_globals();
	__cxa_exception *ep = g->caughtExceptions;

	if (ep == 0)
		_STD terminate();
	if (--ep->handlerCount == 0)
		{	// its last handler is done
		_Unwind_Control_Block *ucbp = &ep->ucb;
		bool foreign = is_foreign_exception(ucbp);
		int propagation_count = ep->propagationCount;

		g->caughtExceptions = ep->nextCaughtException;
		if (foreign)
			{	// free the intermediary
			ucbp = (_Unwind_Control_Block *)ep->exceptionType;
			__cxa_free_exception(ep + 1);
			}
		else
			ep->nextCaughtException = 0;
		if (propagation_count == 0)
			_Unwind_DeleteException(ucbp);
		}
	}

extern "C" void __cxa_call_unexpected(_Unwind_Control_Block *ucbp)
	{	// call unexpected for a violated exception specification
	unsigned int rtti_count = ucbp->barrier_cache.bitpattern[1];
	unsigned int rtti_stride = ucbp->barrier_cache.bitpattern[3];
	unsigned int *rtti_offsets = (unsigned int *)ucbp->barrier_cache.bitpattern[4];
	__cxa_eh_globals *g = __cxa_get_globals();
	__cxa_exception *ep = ucbp_to_ep(ucbp);

	__cxa_begin_catch(ucbp);
	_TRY_BEGIN
	g->call_hook = is_foreign_exception(ucbp)
		? g->unexpectedHandler : ep->unexpectedHandler;
	_STD unexpected();
	_CATCH_ALL
	__cxa_exception *new_ep = g->caughtExceptions;

	if (new_ep == ep)
		__cxa_end_catch();	// rethrown: finish only this handler
	else
		{	// finish the original exception, keeping the new one caught
		g->caughtExceptions = new_ep->nextCaughtException;
		_TRY_BEGIN
		__cxa_end_catch();
		_CATCH_ALL
		_STD terminate();
		_CATCH_END
		new_ep->nextCaughtException = g->caughtExceptions;
		g->caughtExceptions = new_ep;
		}

	bool bad_exception_allowed = false;

	for (unsigned int i = 0; i < rtti_count; ++i)
		{	// throw the new exception on if the specification allows it
		const _STD type_info *rtti =
			(const _STD type_info *)((char *)rtti_offsets + *rtti_offsets);
		void *matched_object;

		if (__cxa_type_match(&new_ep->ucb, rtti, false, &matched_object))
			throw;
		if (typeid(_STD bad_exception) == *rtti)
			bad_exception_allowed = true;
		rtti_offsets = (unsigned int *)((char *)rtti_offsets + rtti_stride);
		}
	if (bad_exception_allowed)
		throw _STD bad_exception();
	eh_call_terminate(&new_ep->ucb);
	_CATCH_END
	}

extern "C" void __cxa_decrement_exception_refcount(void *object) _THROW0()
	{	// drop a reference to a thrown object, freeing it with the last
	if (object != 0)
		{	// count down
		__cxa_exception *ep = object_to_ep(object);
		unsigned int count;

		do
			count = __builtin_ldrex(&ep->referenceCount) - 1;
		while (__builtin_strex(count, &ep->referenceCount) != 0);
		if (count == 0)
			{	// the last reference
			if (ep->exceptionDestructor != 0)
				ep->exceptionDestructor(object);
			__cxa_free_exception(object);
			}
		}
	}

extern "C" void __cxa_throw(void *object, const _STD type_info *type,
	void (*destructor)(void *))
	{	// throw an object
	__cxa_exception *ep = object_to_ep(object);

	ep->exceptionDestructor = destructor;
	ep->ucb.exception_cleanup = exception_cleanup;
	ep->exceptionType = type;
	ep->propagationCount = 1;
	_Unwind_Control_Block *ucbp = &ep->ucb;
	ep->referenceCount = 1;
	++__cxa_get_globals()->uncaughtExceptions;
	_Unwind_Activity(ucbp, (_UASUBSYS_CPP << 4) | _UAACT_STARTING, (unsigned int)type);
	_Unwind_RaiseException(ucbp);
	__cxa_call_terminate(ucbp);
	}

extern "C" const _STD type_info *__cxa_exception_type(_Unwind_Control_Block *ucbp)
	{	// get the type of an exception
	return (ucbp_to_ep(ucbp)->exceptionType);
	}

extern "C" void __cxa_rethrow()
	{	// rethrow the exception being handled
	__cxa_eh_globals *g = __cxa_get_globals();
	__cxa_exception *ep = g->caughtExceptions;

	if (ep == 0)
		eh_call_terminate(0);
	++ep->propagationCount;

	_Unwind_Control_Block *ucbp = &ep->ucb;
	bool foreign = false;

	if (is_foreign_exception(ucbp))
		{	// the foreign exception itself
		ucbp = (_Unwind_Control_Block *)ep->exceptionType;
		foreign = true;
		}
	else
		++g->uncaughtExceptions;
	_Unwind_Activity(ucbp, (_UASUBSYS_CPP << 4) | _UAACT_STARTING,
		foreign ? 0 : (unsigned int)ep->exceptionType);
	_Unwind_RaiseException(ucbp);
	__cxa_call_terminate(ucbp);
	}

extern "C" bool __cxa_begin_cleanup(_Unwind_Control_Block *ucbp)
	{	// start running a cleanup for a propagating exception
	__cxa_eh_globals *g = __cxa_get_globals();
	__cxa_exception *ep = ucbp_to_ep(ucbp);

	if (is_foreign_exception(ucbp))
		{	// stand a header in for it
		ep = get_foreign_intermediary(g->propagatingExceptions, ucbp);
		++ep->propagationCount;
		}
	if (ep->propagationCount == 1)
		{	// push it on the propagating stack
		ep->nextPropagatingException = g->propagatingExceptions;
		g->propagatingExceptions = ep;
		}
	return (true);
	}

extern "C" _Unwind_Control_Block *__ARM_cxa_end_cleanup()
	{	// finish a cleanup, and get the exception to resume
	__cxa_eh_globals *g = __cxa_get_globals();
	__cxa_exception *ep = g->propagatingExceptions;

	if (ep == 0)
		_STD terminate();

	_Unwind_Control_Block *ucbp = &ep->ucb;

	if (is_foreign_exception(ucbp))
		{	// the foreign exception itself
		ucbp = (_Unwind_Control_Block *)ep->exceptionType;
		if (ep->propagationCount == 1)
			{	// pop and free the intermediary
			g->propagatingExceptions = ep->nextPropagatingException;
			__cxa_free_exception(ep + 1);
			}
		else
			--ep->propagationCount;
		}
	else if (ep->propagationCount == 1)
		g->propagatingExceptions = ep->nextPropagatingException;
	return (ucbp);
	}

extern "C" void *__cxa_get_exception_ptr(_Unwind_Control_Block *ucbp)
	{	// get the object a handler will catch
	return ((void *)ucbp->barrier_cache.bitpattern[0]);
	}

extern "C" __cxa_exception *__cxa_allocate_dependent_exception()
	{	// allocate a header that rethrows a primary exception
	__cxa_eh_globals *g = __cxa_get_globals();
	__cxa_exception *ep = (__cxa_exception *)malloc(sizeof (__cxa_exception));

	if (ep == 0)
		{	// use the emergency buffer
		ep = (__cxa_exception *)__ARM_exceptions_buffer_allocate(g->emergency_buffer,
			sizeof (__cxa_exception));
		if (ep == 0)
			eh_call_terminate(0);
		}
	memset(ep, 0, sizeof (__cxa_exception));
	return (ep);
	}

extern "C" void __cxa_free_dependent_exception(__cxa_exception *ep)
	{	// free a dependent exception's header
	void *buffer = __cxa_get_globals()->emergency_buffer;

	if (__ARM_exceptions_buffer_free(buffer, ep) == 0)
		free(ep);
	}

extern "C" void __cxa_increment_exception_refcount(void *object)
	{	// add a reference to a thrown object
	if (object != 0)
		{	// count up
		__cxa_exception *ep = object_to_ep(object);
		unsigned int count;

		do
			count = __builtin_ldrex(&ep->referenceCount) + 1;
		while (__builtin_strex(count, &ep->referenceCount) != 0);
		}
	}

extern "C" void *__cxa_current_primary_exception() _THROW0()
	{	// get a reference to the primary exception being handled
	__cxa_exception *ep = __cxa_get_globals()->caughtExceptions;

	if (ep == 0 || is_foreign_exception(&ep->ucb))
		return (0);
	if (is_dependent_exception(&ep->ucb))
		ep = object_to_ep(ep->primaryException);

	void *object = ep + 1;

	__cxa_increment_exception_refcount(object);
	return (object);
	}

extern "C" void __cxa_rethrow_primary_exception(void *object)
	{	// rethrow a primary exception through a dependent one
	if (object == 0)
		return;

	__cxa_eh_globals *g = __cxa_get_globals();
	__cxa_exception *dep = __cxa_allocate_dependent_exception();

	dep->primaryException = object;
	__cxa_increment_exception_refcount(object);
	dep->exceptionType = object_to_ep(object)->exceptionType;
	dep->unexpectedHandler = g->unexpectedHandler;
	dep->terminateHandler = g->terminateHandler;
	memcpy(dep->ucb.exception_class, ARMCPP_DEPENDENT_CLASS, 8);
	dep->propagationCount = 1;
	dep->ucb.exception_cleanup = dependent_exception_cleanup;
	++g->uncaughtExceptions;
	_Unwind_RaiseException(&dep->ucb);
	__cxa_call_terminate(&dep->ucb);
	}

static void default_unexpected_handler()
	{	// unexpected's default: terminate
	_STD terminate();
	}

static void default_terminate_handler()
	{	// terminate's default: name the exception, then abort
	__cxa_exception *ep = __cxa_get_globals()->caughtExceptions;

	if (ep != 0 && !is_foreign_exception(&ep->ucb) && ep->exceptionType != 0)
		_STD _Debug_message("exception: ", ep->exceptionType->name());
	abort();
	}

static void exception_cleanup(_Unwind_Reason_Code, _Unwind_Control_Block *ucbp)
	{	// the unwinder's cleanup of a C++ exception
	__cxa_eh_globals *g = __cxa_get_globals();
	__cxa_exception *ep = ucbp_to_ep(ucbp);

	if (ep->handlerCount != 0)
		{	// still caught: it only stops propagating
		--ep->propagationCount;
		--g->uncaughtExceptions;
		}
	else
		__cxa_decrement_exception_refcount(ep + 1);
	}

static void dependent_exception_cleanup(_Unwind_Reason_Code, _Unwind_Control_Block *ucbp)
	{	// the unwinder's cleanup of a dependent exception
	__cxa_exception *dep = ucbp_to_ep(ucbp);

	__cxa_decrement_exception_refcount(dep->primaryException);
	__cxa_free_dependent_exception(dep);
	}
