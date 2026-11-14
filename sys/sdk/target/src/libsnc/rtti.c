/*
 * rtti.c: SNC's run-time type identification support (libsnc rtti.o, EDG's runtime, compiled
 * as C++): the bad_typeid and bad_cast throws, the conversion of a derived object to a base
 * that exception matching uses, and dynamic_cast. A class is walked through its type_info:
 * an __si_class_type_info has one public non-virtual base at offset 0, an
 * __vmi_class_type_info a table of bases with their offsets (or vtable slots, for virtual
 * bases) and access. Two type_infos name the same type when they are one object or their
 * names are one string.
 */
#include <string.h>
#include <typeinfo>
#include "rtti.h"

using namespace __cxxabiv1;

#define FALSE	0
#define TRUE	1

#define matching_type_info(a, b)	((a) == (b) || (a)->name() == (b)->name())

typedef enum a_result_virtuality
	{	/* how the base found so far is reached */
	rv_none,
	rv_nonvirtual,
	rv_virtual
	} a_result_virtuality;

struct a_base_class_traversal_block;

typedef int (*a_tbc_function)(void *obj, const std::type_info *type,
	a_base_class_traversal_block *tbc, __base_class_type_info *base_info);

typedef struct a_base_class_traversal_block
	{	/* a walk over a complete object's base class subobjects */
	a_tbc_function pre_function;	/* called before a class's bases */
	a_tbc_function post_function;	/* called after them */
	int reserved;
	int public_only;	/* walk the public bases only */
	int done;	/* stop the walk */
	const std::type_info *target_type;	/* dynamic_cast: the type cast to */
	const std::type_info *source_type;	/* the static type of the operand */
	void *source_obj;	/* the operand */
	void *target_obj;	/* the target subobject being walked, or NULL */
	void *result;	/* the target subobject that holds the operand */
	} a_base_class_traversal_block;

static long base_class_offset(void *obj, const __base_class_type_info *bp)
{	/* where a base class subobject is in obj */
	long offset = bp->__offset_flags >> __base_class_type_info::__offset_shift;

	if (bp->__offset_flags & __base_class_type_info::__virtual_mask)
		offset = *(long *)(*(char **)obj + offset);	/* the vtable's offset */
	return offset;
}

extern "C" void __cxa_bad_typeid(void)
{	/* typeid of a null pointer */
	throw std::bad_typeid();
}

static inline const std::type_info *typeid_of(const std::type_info *ti)
{	/* &typeid(*ti): the type_info's own type, from its vtable */
	return ti != NULL ? (*(const std::type_info ***)ti)[-1]
		: (__cxa_bad_typeid(), (const std::type_info *)NULL);
}

static int derived_to_base_conversion_r(void *obj, void **result,
	const std::type_info *derived, const std::type_info *base, unsigned int flags,
	int *ambiguous, int is_public, const std::type_info **virtual_base,
	a_result_virtuality *virtuality, int *result_is_public)
{	/* find base in derived: obj's subobject in *result, FALSE when none or ambiguous */
	int found = FALSE;

	if (*typeid_of(derived) == typeid(__si_class_type_info)) {
		const __si_class_type_info *si = (const __si_class_type_info *)derived;

		if (matching_type_info(si->__base_type, base)) {
			if ((*result != NULL && *result != obj) || *virtuality == rv_virtual
				|| *ambiguous) {
				*ambiguous = TRUE;
				*result = NULL;
			} else {
				*virtuality = rv_nonvirtual;
				*result_is_public = is_public;
				*result = obj;
				found = TRUE;
			}
		} else if (derived_to_base_conversion_r(obj, result, si->__base_type, base, flags,
			ambiguous, is_public, virtual_base, virtuality, result_is_public)
			|| *ambiguous)
			found = *ambiguous ? found : TRUE;
	} else if (*typeid_of(derived) == typeid(__vmi_class_type_info)) {
		const __vmi_class_type_info *vmi = (const __vmi_class_type_info *)derived;
		const __base_class_type_info *bp;
		unsigned int diamond = flags & __vmi_class_type_info::__diamond_shaped_mask;
		unsigned int repeat = flags & __vmi_class_type_info::__non_diamond_repeat_mask;

		for (bp = vmi->__base_info; bp < vmi->__base_info + vmi->__base_count; bp++) {
			long offset_flags = bp->__offset_flags;
			long is_virtual = offset_flags & __base_class_type_info::__virtual_mask;
			void *base_obj;
			int base_is_public;

			if (obj == NULL)
				base_obj = NULL;
			else
				base_obj = (char *)obj + base_class_offset(obj, bp);
			base_is_public = FALSE;
			if (is_public)
				base_is_public = (offset_flags & __base_class_type_info::__public_mask) != 0;
			if (matching_type_info(bp->__base_type, base)) {
				if ((*result != NULL && base_obj != *result)
					|| ((bp->__offset_flags & __base_class_type_info::__virtual_mask)
						&& *virtuality == rv_nonvirtual)
					|| *ambiguous) {
					*ambiguous = TRUE;
					*result = NULL;
					found = FALSE;
					break;
				}
				*virtuality = (bp->__offset_flags & __base_class_type_info::__virtual_mask)
					? rv_virtual : rv_nonvirtual;
				*result_is_public = base_is_public;
				*result = base_obj;
				found = TRUE;
				if (base_is_public && !(is_virtual ? diamond : repeat))
					break;	/* no other path can reach it */
			} else {
				const std::type_info *base_virtual_base = NULL;
				a_result_virtuality base_virtuality = rv_none;

				if (derived_to_base_conversion_r(base_obj, result, bp->__base_type, base,
					flags, ambiguous, base_is_public, &base_virtual_base,
					&base_virtuality, result_is_public) || *ambiguous) {
					if (*ambiguous) {
						found = FALSE;
						break;
					}
					if (base_virtual_base == NULL
						&& (bp->__offset_flags & __base_class_type_info::__virtual_mask))
						base_virtual_base = bp->__base_type;
					if (*virtuality != rv_none
						&& (base_virtuality != rv_virtual || *virtuality != rv_virtual)
						&& (*virtual_base == NULL || *virtual_base != base_virtual_base)) {
						*ambiguous = TRUE;
						found = FALSE;
						break;
					}
					*virtuality = base_virtuality;
					*virtual_base = base_virtual_base;
					found = TRUE;
					if (*result_is_public && !(is_virtual ? diamond : repeat))
						break;
				}
			}
		}
	}
	return found;
}

extern "C" int __derived_to_base_conversion(void **p_obj, void **p_result,
	const std::type_info *derived, const std::type_info *base, int unused1, int unused2)
{	/* convert *p_obj of type derived to its unique public base: TRUE when there is one */
	int converted = FALSE;
	int ambiguous = FALSE;
	int result_is_public = TRUE;
	void *obj = NULL;
	unsigned int flags;
	const std::type_info *virtual_base;
	a_result_virtuality virtuality;

	if (p_obj != NULL)
		obj = *p_obj;
	*p_result = NULL;
	flags = *typeid_of(derived) == typeid(__vmi_class_type_info)
		? ((const __vmi_class_type_info *)derived)->__flags
		: __vmi_class_type_info::__non_diamond_repeat_mask
			| __vmi_class_type_info::__diamond_shaped_mask;
	virtual_base = NULL;
	virtuality = rv_none;
	if (derived_to_base_conversion_r(obj, p_result, derived, base, flags, &ambiguous, TRUE,
		&virtual_base, &virtuality, &result_is_public) && result_is_public)
		converted = TRUE;
	return converted;
}

static void traverse_base_classes(void *obj, const std::type_info *type,
	a_base_class_traversal_block *tbc, __base_class_type_info *base_info)
{	/* walk obj's bases depth first, calling the block's functions on each class */
	tbc->pre_function(obj, type, tbc, base_info);
	if (*typeid_of(type) == typeid(__si_class_type_info)) {
		traverse_base_classes(obj, ((const __si_class_type_info *)type)->__base_type, tbc,
			NULL);
	} else if (*typeid_of(type) == typeid(__vmi_class_type_info)) {
		const __vmi_class_type_info *vmi = (const __vmi_class_type_info *)type;
		__base_class_type_info *bp;

		for (bp = (__base_class_type_info *)vmi->__base_info;
			bp < vmi->__base_info + vmi->__base_count; bp++) {
			long offset;

			if (tbc->public_only
				&& !(bp->__offset_flags & __base_class_type_info::__public_mask))
				continue;
			if (bp != NULL && (bp->__offset_flags & __base_class_type_info::__virtual_mask))
				offset = *(long *)(*(char **)obj
					+ (bp->__offset_flags >> __base_class_type_info::__offset_shift));
			else
				offset = bp->__offset_flags >> __base_class_type_info::__offset_shift;
			traverse_base_classes((char *)obj + offset, bp->__base_type, tbc, bp);
			if (tbc->done)
				return;
		}
	}
	if (tbc->post_function != NULL)
		tbc->post_function(obj, type, tbc, base_info);
}

static __base_class_type_info *find_base_class_at_addr(void *obj, void *addr,
	const std::type_info *type, const std::type_info *base, int *is_public)
{	/* find the base subobject of type base at addr: *is_public when it is reached publicly */
	__base_class_type_info *found = NULL;

	if (*typeid_of(type) == typeid(__si_class_type_info)) {
		const __si_class_type_info *si = (const __si_class_type_info *)type;

		if (obj == addr && matching_type_info(si->__base_type, base))
			*is_public = TRUE;
		else
			return find_base_class_at_addr(obj, addr, si->__base_type, base, is_public);
	} else if (*typeid_of(type) == typeid(__vmi_class_type_info)) {
		const __vmi_class_type_info *vmi = (const __vmi_class_type_info *)type;
		__base_class_type_info *bp;

		for (bp = (__base_class_type_info *)vmi->__base_info;
			bp < vmi->__base_info + vmi->__base_count; bp++) {
			void *base_obj = (char *)obj + base_class_offset(obj, bp);

			if (base_obj == addr && matching_type_info(bp->__base_type, base)) {
				found = bp;
				if (bp->__offset_flags & __base_class_type_info::__public_mask)
					*is_public = TRUE;
				break;
			}
			if (bp->__offset_flags & __base_class_type_info::__public_mask) {
				found = find_base_class_at_addr(base_obj, addr, bp->__base_type, base,
					is_public);
				if (*is_public)
					break;
			}
		}
	}
	return found;
}

static int tbc_downcast(void *obj, const std::type_info *type,
	a_base_class_traversal_block *tbc, __base_class_type_info *base_info);
static int tbc_post_downcast(void *obj, const std::type_info *type,
	a_base_class_traversal_block *tbc, __base_class_type_info *base_info);

extern "C" void *__dynamic_cast(void *ptr, const std::type_info *source_type,
	const std::type_info *target_type)
{	/* dynamic_cast<target_type *>(ptr); a null target_type casts to void * */
	long *vtbl = *(long **)ptr;
	const std::type_info *dynamic_type = (const std::type_info *)vtbl[-1];
	void *complete_obj = (char *)ptr + vtbl[-2];
	void *result = complete_obj;
	int is_public;

	if (target_type != NULL) {	/* a target subobject that holds ptr: a downcast */
		a_base_class_traversal_block tbc;

		memset(&tbc.reserved, 0, 3 * sizeof(int));
		tbc.target_type = target_type;
		tbc.target_obj = NULL;
		tbc.result = NULL;
		tbc.pre_function = tbc_downcast;
		tbc.post_function = tbc_post_downcast;
		tbc.source_type = source_type;
		tbc.source_obj = ptr;
		traverse_base_classes(complete_obj, dynamic_type, &tbc, NULL);
		result = tbc.result;
	}
	if (result == NULL) {	/* a cross cast through the complete object */
		if (dynamic_type == source_type)
			is_public = TRUE;
		else {
			is_public = FALSE;
			find_base_class_at_addr(complete_obj, ptr, dynamic_type, source_type,
				&is_public);
			if (!is_public)
				return result;
		}
		if (matching_type_info(dynamic_type, target_type)) {
			result = complete_obj;
		} else {
			void *found = NULL;
			int ambiguous = FALSE;
			int result_is_public = TRUE;
			unsigned int flags;
			const std::type_info *virtual_base;
			a_result_virtuality virtuality;

			flags = *typeid_of(dynamic_type) == typeid(__vmi_class_type_info)
				? ((const __vmi_class_type_info *)dynamic_type)->__flags
				: __vmi_class_type_info::__non_diamond_repeat_mask
					| __vmi_class_type_info::__diamond_shaped_mask;
			virtual_base = NULL;
			virtuality = rv_none;
			if (derived_to_base_conversion_r(complete_obj, &found, dynamic_type,
				target_type, flags, &ambiguous, TRUE, &virtual_base, &virtuality,
				&result_is_public) && result_is_public)
				result = found;
		}
	}
	return result;
}

extern "C" void __cxa_bad_cast(void)
{	/* a failed dynamic_cast to a reference */
	throw std::bad_cast();
}

static int tbc_downcast(void *obj, const std::type_info *type,
	a_base_class_traversal_block *tbc, __base_class_type_info *base_info)
{	/* entering a class: a target subobject, or the operand inside one */
	if (matching_type_info(tbc->target_type, type)) {
		tbc->target_obj = obj;
		tbc->public_only = TRUE;
		return TRUE;
	}
	if (tbc->target_obj != NULL && obj == tbc->source_obj
		&& matching_type_info(type, tbc->source_type)) {
		if (tbc->result != NULL) {	/* in two targets: ambiguous */
			tbc->result = NULL;
			tbc->done = TRUE;
			return TRUE;
		}
		tbc->result = tbc->target_obj;
	}
}

static int tbc_post_downcast(void *obj, const std::type_info *type,
	a_base_class_traversal_block *tbc, __base_class_type_info *base_info)
{	/* leaving a class: the end of a target subobject */
	if (matching_type_info(tbc->target_type, type)) {
		tbc->target_obj = NULL;
		tbc->public_only = FALSE;
		return FALSE;
	}
}
