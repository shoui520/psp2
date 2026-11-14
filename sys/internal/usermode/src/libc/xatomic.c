/*
 * xatomic.c: the C++ library's atomic operations. The lock-free ones load and store
 * exclusively (ldrex/strex), with data memory barriers before and after as the memory order
 * asks; the generic ones copy under a spin lock. An order an operation does not allow is
 * reported and the program aborted.
 */
#include <stdlib.h>
#include <string.h>
#include <intrinsics.h>
#include <libkernel/libkernel.h>

typedef enum memory_order {
	memory_order_relaxed,
	memory_order_consume,
	memory_order_acquire,
	memory_order_release,
	memory_order_acq_rel,
	memory_order_seq_cst
} memory_order;

typedef unsigned char _Atomic_flag_t;
typedef unsigned char _Uint1_t;
typedef unsigned short _Uint2_t;
typedef unsigned int _Uint4_t;
typedef unsigned long long _Uint8_t;

/* Sony's lock and flag functions call these whole. */
__attribute__((noinline)) int _Atomic_flag_test_and_set(volatile _Atomic_flag_t *,
	memory_order);
__attribute__((noinline)) void _Atomic_flag_clear(volatile _Atomic_flag_t *, memory_order);
_Uint1_t _Atomic_exchange_1(volatile _Uint1_t *, _Uint1_t, memory_order);
void _Lock_spin_lock(volatile _Atomic_flag_t *);
void _Unlock_spin_lock(volatile _Atomic_flag_t *);

#define _Invalid_memory_order(name, tgt)	\
	(sceClibPrintf("Invalid memory order in %s : Target address %p : Assertion Failed\n",	\
		name, tgt), abort())

		/* EXCLUSIVE ACCESS: ldrex/strex, ldrexd/strexd for 8 bytes */
#define _Ldrex_1(p)	__builtin_ldrex(p)
#define _Ldrex_2(p)	__builtin_ldrex(p)
#define _Ldrex_4(p)	__builtin_ldrex(p)
#define _Ldrex_8(p)	__builtin_ldrexd(p)
#define _Strex_1(v, p)	__builtin_strex(v, p)
#define _Strex_2(v, p)	__builtin_strex(v, p)
#define _Strex_4(v, p)	__builtin_strex(v, p)
#define _Strex_8(v, p)	__builtin_strexd(v, p)

#define _No_fence
#define _Fence	__builtin_dmb()

void _Lock_spin_lock(volatile _Atomic_flag_t *_Flag)
{
	while (_Atomic_flag_test_and_set(_Flag, memory_order_acquire))
		;
}

void _Unlock_spin_lock(volatile _Atomic_flag_t *_Flag)
{
	_Atomic_flag_clear(_Flag, memory_order_release);
}

		/* SPIN LOCK FOR shared_ptr ATOMIC OPERATIONS */
static volatile _Atomic_flag_t _Shared_ptr_flag;

void _Lock_shared_ptr_spin_lock(void)
{
	while (_Atomic_flag_test_and_set(&_Shared_ptr_flag, memory_order_acquire))
		;
}

void _Unlock_shared_ptr_spin_lock(void)
{
	_Atomic_flag_clear(&_Shared_ptr_flag, memory_order_release);
}

		/* GENERIC ATOMIC OPERATIONS */
void _Atomic_copy(volatile _Atomic_flag_t *_Flag, size_t _Size, volatile void *_Tgt,
	volatile const void *_Src, memory_order _Order)
{
	while (_Atomic_flag_test_and_set(_Flag, memory_order_acquire))
		;
	memcpy((void *)_Tgt, (void *)_Src, _Size);
	_Atomic_flag_clear(_Flag, memory_order_release);
}

void _Atomic_exchange(volatile _Atomic_flag_t *_Flag, size_t _Size, volatile void *_Tgt,
	volatile void *_Src, memory_order _Order)
{
	unsigned char *_Left = (unsigned char *)_Tgt;
	unsigned char *_Right = (unsigned char *)_Src;

	while (_Atomic_flag_test_and_set(_Flag, memory_order_acquire))
		;
	for (; 0 < _Size; --_Size) {	/* swap the bytes */
		unsigned char _Tmp = *_Left;

		*_Left++ = *_Right;
		*_Right++ = _Tmp;
	}
	_Atomic_flag_clear(_Flag, memory_order_release);
}

int _Atomic_compare_exchange_weak(volatile _Atomic_flag_t *_Flag, size_t _Size,
	volatile void *_Tgt, volatile void *_Exp, const volatile void *_Src,
	memory_order _Order1, memory_order _Order2)
{
	int _Result;

	while (_Atomic_flag_test_and_set(_Flag, memory_order_acquire))
		;
	_Result = memcmp((const void *)_Tgt, (const void *)_Exp, _Size) == 0;
	if (_Result != 0)
		memcpy((void *)_Tgt, (const void *)_Src, _Size);
	else
		memcpy((void *)_Exp, (const void *)_Tgt, _Size);
	_Atomic_flag_clear(_Flag, memory_order_release);
	return _Result;
}

int _Atomic_compare_exchange_strong(volatile _Atomic_flag_t *_Flag, size_t _Size,
	volatile void *_Tgt, volatile void *_Exp, const volatile void *_Src,
	memory_order _Order1, memory_order _Order2)
{
	int _Result;

	while (_Atomic_flag_test_and_set(_Flag, memory_order_acquire))
		;
	_Result = memcmp((const void *)_Tgt, (const void *)_Exp, _Size) == 0;
	if (_Result != 0)
		memcpy((void *)_Tgt, (const void *)_Src, _Size);
	else
		memcpy((void *)_Exp, (const void *)_Tgt, _Size);
	_Atomic_flag_clear(_Flag, memory_order_release);
	return _Result;
}

		/* LOCK-FREE PROPERTY */
int _Atomic_is_lock_free_1(void)
{
	return 1;
}

int _Atomic_is_lock_free_2(void)
{
	return 1;
}

int _Atomic_is_lock_free_4(void)
{
	return 1;
}

int _Atomic_is_lock_free_8(void)
{
	return 1;
}

		/* OPERATIONS ON N-BYTE INTEGERS */
/*
 * A plain store is atomic up to 4 bytes; 8 bytes are stored exclusively. Release orders
 * fence before the store, seq_cst before and after.
 */
#define _STORE_PLAIN(N)	*_Tgt = _Value
#define _STORE_EXCLUSIVE(N)	\
	do	\
		(void)_Ldrex_##N(_Tgt);	\
	while (_Strex_##N(_Value, _Tgt) != 0)

#define _ATOMIC_STORE(N, name, kind, what)	\
void name(volatile _Uint##N##_t *_Tgt, _Uint##N##_t _Value, memory_order _Order)	\
{	\
	switch (_Order) {	\
	case memory_order_relaxed:	\
		kind(N);	\
		break;	\
	case memory_order_release:	\
		__builtin_dmb();	\
		kind(N);	\
		break;	\
	case memory_order_seq_cst:	\
		__builtin_dmb();	\
		kind(N);	\
		__builtin_dmb();	\
		break;	\
	default:	\
		_Invalid_memory_order(what, _Tgt);	\
	}	\
}

/* A load is atomic up to 4 bytes; 8 bytes are loaded exclusively. */
#define _LOAD_PLAIN(N)	_Value = *_Tgt
#define _LOAD_EXCLUSIVE(N)	_Value = _Ldrex_##N(_Tgt)

#define _ATOMIC_LOAD(N, kind)	\
_Uint##N##_t _Atomic_load_##N(volatile _Uint##N##_t *_Tgt, memory_order _Order)	\
{	\
	_Uint##N##_t _Value;	\
	\
	switch (_Order) {	\
	case memory_order_relaxed:	\
		kind(N);	\
		break;	\
	case memory_order_consume:	\
	case memory_order_acquire:	\
		kind(N);	\
		__builtin_dmb();	\
		break;	\
	case memory_order_seq_cst:	\
		__builtin_dmb();	\
		kind(N);	\
		__builtin_dmb();	\
		break;	\
	default:	\
		_Invalid_memory_order("atomic_load_explicit", _Tgt);	\
	}	\
	return _Value;	\
}

/* A read-modify-write: load exclusively, store the new value, again until the store holds. */
#define _RMW(N, newval)	\
	do	\
		_Old = _Ldrex_##N(_Tgt);	\
	while (_Strex_##N(newval, _Tgt) != 0)

#define _ATOMIC_RMW(N, name, newval, what)	\
_Uint##N##_t name(volatile _Uint##N##_t *_Tgt, _Uint##N##_t _Value, memory_order _Order)	\
{	\
	_Uint##N##_t _Old;	\
	\
	switch (_Order) {	\
	case memory_order_relaxed:	\
	case memory_order_consume:	\
		_RMW(N, newval);	\
		break;	\
	case memory_order_acquire:	\
		_RMW(N, newval);	\
		__builtin_dmb();	\
		break;	\
	case memory_order_release:	\
		__builtin_dmb();	\
		_RMW(N, newval);	\
		break;	\
	case memory_order_acq_rel:	\
	case memory_order_seq_cst:	\
		__builtin_dmb();	\
		_RMW(N, newval);	\
		__builtin_dmb();	\
		break;	\
	default:	\
		_Invalid_memory_order(what, _Tgt);	\
	}	\
	return _Old;	\
}

/*
 * Compare and exchange with the fences of one success order (before, after) and one
 * failure order (fail): the weak form tries once, the strong form until the store holds or
 * the values differ. On a difference the value found is stored in *_Exp.
 */
#define _CAS(N, before, after, fail)	\
	if (_Weak) {	\
		_Uint##N##_t _Expected = *_Exp;	\
		_Uint##N##_t _Old;	\
		int _Status;	\
	\
		before;	\
		_Old = _Ldrex_##N(_Tgt);	\
		if (_Old == _Expected) {	\
			_Status = _Strex_##N(_Value, _Tgt);	\
			after;	\
			return _Status == 0;	\
		}	\
		fail;	\
		*_Exp = _Old;	\
		return 0;	\
	} else {	\
		_Uint##N##_t _Expected = *_Exp;	\
		_Uint##N##_t _Old;	\
	\
		before;	\
		for (;;) {	\
			_Old = _Ldrex_##N(_Tgt);	\
			if (_Old != _Expected) {	\
				fail;	\
				*_Exp = _Old;	\
				return 0;	\
			}	\
			if (_Strex_##N(_Value, _Tgt) == 0)	\
				break;	\
		}	\
		after;	\
		return 1;	\
	}

#define _CAS_INVALID()	\
	if (_Weak)	\
		_Invalid_memory_order("atomic_compare_exchange_weak_explicit", _Tgt);	\
	else	\
		_Invalid_memory_order("atomic_compare_exchange_strong_explicit", _Tgt);	\
	return 0

#define _CAS_PARAMS(N)	\
	volatile _Uint##N##_t *_Tgt, _Uint##N##_t *_Exp, _Uint##N##_t _Value, int _Weak

/* One compare and exchange per success order, choosing the failure order's fences. */
#define _ATOMIC_COMPARE_EXCHANGE(N)	\
static int _Compare_exchange_relaxed_##N(_CAS_PARAMS(N), memory_order _Order1,	\
	memory_order _Order2)	\
{	\
	if (_Order2 == memory_order_relaxed	\
	    || (_Order2 == memory_order_consume && _Order1 != memory_order_relaxed)) {	\
		_CAS(N, _No_fence, _No_fence, _No_fence)	\
	}	\
	_CAS_INVALID();	\
}	\
	\
static int _Compare_exchange_acquire_##N(_CAS_PARAMS(N), memory_order _Order2)	\
{	\
	switch (_Order2) {	\
	case memory_order_relaxed:	\
		_CAS(N, _No_fence, _Fence, _No_fence)	\
	case memory_order_consume:	\
	case memory_order_acquire:	\
		_CAS(N, _No_fence, _Fence, _Fence)	\
	default:	\
		_CAS_INVALID();	\
	}	\
}	\
	\
static int _Compare_exchange_release_##N(_CAS_PARAMS(N), memory_order _Order2)	\
{	\
	switch (_Order2) {	\
	case memory_order_relaxed:	\
		_CAS(N, _Fence, _No_fence, _No_fence)	\
	case memory_order_consume:	\
	case memory_order_acquire:	\
		_CAS(N, _Fence, _No_fence, _Fence)	\
	default:	\
		_CAS_INVALID();	\
	}	\
}	\
	\
static int _Compare_exchange_acq_rel_##N(_CAS_PARAMS(N), memory_order _Order2)	\
{	\
	switch (_Order2) {	\
	case memory_order_relaxed:	\
		_CAS(N, _Fence, _Fence, _No_fence)	\
	case memory_order_consume:	\
	case memory_order_acquire:	\
		_CAS(N, _Fence, _Fence, _Fence)	\
	default:	\
		_CAS_INVALID();	\
	}	\
}	\
	\
static int _Compare_exchange_seq_cst_##N(_CAS_PARAMS(N), memory_order _Order1,	\
	memory_order _Order2)	\
{	\
	switch (_Order2) {	\
	case memory_order_relaxed:	\
		_CAS(N, _Fence, _Fence, _No_fence)	\
	case memory_order_seq_cst:	\
		if (_Order1 == memory_order_acq_rel)	\
			break;	\
	case memory_order_consume:	\
	case memory_order_acquire:	\
		_CAS(N, _Fence, _Fence, _Fence)	\
	}	\
	_CAS_INVALID();	\
}	\
	\
_ATOMIC_COMPARE_EXCHANGE_ENTRY(N, strong, 0)	\
_ATOMIC_COMPARE_EXCHANGE_ENTRY(N, weak, 1)

#define _ATOMIC_COMPARE_EXCHANGE_ENTRY(N, form, weak)	\
int _Atomic_compare_exchange_##form##_##N(volatile _Uint##N##_t *_Tgt, _Uint##N##_t *_Exp,	\
	_Uint##N##_t _Value, memory_order _Order1, memory_order _Order2)	\
{	\
	switch (_Order1) {	\
	case memory_order_relaxed:	\
	case memory_order_consume:	\
		return _Compare_exchange_relaxed_##N(_Tgt, _Exp, _Value, weak, _Order1, _Order2);	\
	case memory_order_acquire:	\
		return _Compare_exchange_acquire_##N(_Tgt, _Exp, _Value, weak, _Order2);	\
	case memory_order_release:	\
		return _Compare_exchange_release_##N(_Tgt, _Exp, _Value, weak, _Order2);	\
	case memory_order_acq_rel:	\
		return _Compare_exchange_acq_rel_##N(_Tgt, _Exp, _Value, weak, _Order2);	\
	case memory_order_seq_cst:	\
		return _Compare_exchange_seq_cst_##N(_Tgt, _Exp, _Value, weak, _Order1, _Order2);	\
	default:	\
		_Invalid_memory_order("atomic_compare_exchange_" #form "_explicit", _Tgt);	\
		return 0;	\
	}	\
}

#define _ATOMIC_FETCH(N)	\
_ATOMIC_RMW(N, _Atomic_fetch_add_##N, _Old + _Value, "atomic_fetch_add_explicit")	\
_ATOMIC_RMW(N, _Atomic_fetch_sub_##N, _Old - _Value, "atomic_fetch_sub_explicit")	\
_ATOMIC_RMW(N, _Atomic_fetch_and_##N, _Old & _Value, "atomic_fetch_and_explicit")	\
_ATOMIC_RMW(N, _Atomic_fetch_or_##N, _Old | _Value, "atomic_fetch_or_explicit")	\
_ATOMIC_RMW(N, _Atomic_fetch_xor_##N, _Old ^ _Value, "atomic_fetch_xor_explicit")

_ATOMIC_STORE(1, _Atomic_store_1, _STORE_PLAIN, "atomic_store_explicit")
static __attribute__((noinline)) _ATOMIC_STORE(1, _Atomic_clear_1, _STORE_PLAIN, "atomic_clear_explicit")
_ATOMIC_LOAD(1, _LOAD_PLAIN)
_ATOMIC_RMW(1, _Atomic_exchange_1, _Value, "atomic_exchange_explicit")
_ATOMIC_COMPARE_EXCHANGE(1)
_ATOMIC_FETCH(1)

_ATOMIC_STORE(2, _Atomic_store_2, _STORE_PLAIN, "atomic_store_explicit")
_ATOMIC_LOAD(2, _LOAD_PLAIN)
_ATOMIC_RMW(2, _Atomic_exchange_2, _Value, "atomic_exchange_explicit")
_ATOMIC_COMPARE_EXCHANGE(2)
_ATOMIC_FETCH(2)

_ATOMIC_STORE(4, _Atomic_store_4, _STORE_PLAIN, "atomic_store_explicit")
_ATOMIC_LOAD(4, _LOAD_PLAIN)
_ATOMIC_RMW(4, _Atomic_exchange_4, _Value, "atomic_exchange_explicit")
_ATOMIC_COMPARE_EXCHANGE(4)
_ATOMIC_FETCH(4)

_ATOMIC_STORE(8, _Atomic_store_8, _STORE_EXCLUSIVE, "atomic_store_explicit")
_ATOMIC_LOAD(8, _LOAD_EXCLUSIVE)
_ATOMIC_RMW(8, _Atomic_exchange_8, _Value, "atomic_exchange_explicit")
_ATOMIC_COMPARE_EXCHANGE(8)
_ATOMIC_FETCH(8)

		/* OPERATIONS ON _Atomic_flag_t */
int _Atomic_flag_test_and_set(volatile _Atomic_flag_t *_Flag, memory_order _Order)
{
	return _Atomic_exchange_1(_Flag, 1, _Order);
}

void _Atomic_flag_clear(volatile _Atomic_flag_t *_Flag, memory_order _Order)
{
	_Atomic_clear_1(_Flag, 0, _Order);
}

		/* FENCES */
void _Atomic_thread_fence(memory_order _Order)
{
	if (_Order != memory_order_relaxed)
		__builtin_dmb();
}

void _Atomic_signal_fence(memory_order _Order)
{
}
