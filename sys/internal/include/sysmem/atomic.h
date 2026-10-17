#ifndef SYSCLIB_ATOMIC_H
#define SYSCLIB_ATOMIC_H

#include <stdint.h>
#include <sysmem/barrier.h>

static __inline__ __attribute__((always_inline)) void AtomicFullBarrier(void)
{
	dmb();
}

static __inline__ __attribute__((always_inline))
uint32_t AtomicLoadReserved32(const volatile uint32_t *address)
{
	uint32_t value;

	__asm__ volatile("ldrex %0, [%1]"
			 : "=&r" (value) : "r" (address) : "memory");
	return value;
}

static __inline__ __attribute__((always_inline))
uint32_t AtomicStoreReserved32(volatile uint32_t *address, uint32_t value)
{
	uint32_t failed;

	__asm__ volatile("strex %0, %2, [%1]"
			 : "=&r" (failed) : "r" (address), "r" (value) : "memory");
	return failed;
}

static __inline__ __attribute__((always_inline))
uint64_t AtomicLoadReserved64(const volatile uint64_t *address)
{
	uint64_t value;

	__asm__ volatile("ldrexd %Q0, %R0, [%1]"
			 : "=&r" (value) : "r" (address) : "memory");
	return value;
}

static __inline__ __attribute__((always_inline))
uint32_t AtomicStoreReserved64(volatile uint64_t *address, uint64_t value)
{
	uint32_t failed;

	__asm__ volatile("strexd %0, %Q2, %R2, [%1]"
			 : "=&r" (failed) : "r" (address), "r" (value) : "memory");
	return failed;
}

/* Keep the exclusive reservation and full-system ordering of Sony's helper. */
static __inline__ __attribute__((always_inline))
uint32_t AtomicCompareAndSwap32(uint32_t *address, uint32_t expected,
			      uint32_t replacement)
{
	uint32_t previous;
	uint32_t failed;

	dmb();
	do {
		__asm__ volatile("ldrex %0, [%1]"
				 : "=&r" (previous) : "r" (address) : "memory");
		if (previous != expected)
			break;
		__asm__ volatile("strex %0, %2, [%1]"
				 : "=&r" (failed)
				 : "r" (address), "r" (replacement) : "memory");
	} while (failed);
	dmb();
	return previous;
}

enum AtomicOperation {
	ATOMIC_SET,
	ATOMIC_ADD,
	ATOMIC_SUBTRACT,
	ATOMIC_AND,
	ATOMIC_OR,
	ATOMIC_XOR,
	ATOMIC_CLEAR,
};

static __inline__ __attribute__((always_inline))
uint32_t AtomicLoadReserved8(const volatile uint8_t *address)
{
	uint32_t value;

	__asm__ volatile("ldrexb %0, [%1]"
			 : "=&r" (value) : "r" (address) : "memory");
	return value;
}

static __inline__ __attribute__((always_inline))
uint32_t AtomicStoreReserved8(volatile uint8_t *address, uint32_t value)
{
	uint32_t failed;

	__asm__ volatile("strexb %0, %2, [%1]"
			 : "=&r" (failed) : "r" (address), "r" (value) : "memory");
	return failed;
}

static __inline__ __attribute__((always_inline))
uint32_t AtomicLoadReserved16(const volatile uint16_t *address)
{
	uint32_t value;

	__asm__ volatile("ldrexh %0, [%1]"
			 : "=&r" (value) : "r" (address) : "memory");
	return value;
}

static __inline__ __attribute__((always_inline))
uint32_t AtomicStoreReserved16(volatile uint16_t *address, uint32_t value)
{
	uint32_t failed;

	__asm__ volatile("strexh %0, %2, [%1]"
			 : "=&r" (failed) : "r" (address), "r" (value) : "memory");
	return failed;
}

static __inline__ __attribute__((always_inline))
uint32_t AtomicApplyOperation(uint32_t previous, uint32_t operand,
		enum AtomicOperation operation)
{
	switch (operation) {
	case ATOMIC_SET:
		return operand;
	case ATOMIC_ADD:
		return previous + operand;
	case ATOMIC_SUBTRACT:
		return previous - operand;
	case ATOMIC_AND:
		return previous & operand;
	case ATOMIC_OR:
		return previous | operand;
	case ATOMIC_XOR:
		return previous ^ operand;
	case ATOMIC_CLEAR:
		return previous & ~operand;
	}
	return previous;
}

static __inline__ __attribute__((always_inline))
void AtomicSet8(uint8_t *address, uint8_t value)
{
	do {
		AtomicLoadReserved8(address);
	} while (AtomicStoreReserved8(address, value));
}

static __inline__ __attribute__((always_inline))
void AtomicSet16(uint16_t *address, uint16_t value)
{
	do {
		AtomicLoadReserved16(address);
	} while (AtomicStoreReserved16(address, value));
}

static __inline__ __attribute__((always_inline))
int8_t AtomicUpdate8(uint8_t *address, uint8_t operand,
		enum AtomicOperation operation, int returnUpdated)
{
	uint32_t previous, next;

	AtomicFullBarrier();
	do {
		previous = AtomicLoadReserved8(address);
		next = AtomicApplyOperation(previous, operand, operation);
	} while (AtomicStoreReserved8(address, next));
	AtomicFullBarrier();
	return (int8_t)(returnUpdated ? next : previous);
}

static __inline__ __attribute__((always_inline))
int16_t AtomicUpdate16(uint16_t *address, uint16_t operand,
		enum AtomicOperation operation, int returnUpdated)
{
	uint32_t previous, next;

	AtomicFullBarrier();
	do {
		previous = AtomicLoadReserved16(address);
		next = AtomicApplyOperation(previous, operand, operation);
	} while (AtomicStoreReserved16(address, next));
	AtomicFullBarrier();
	return (int16_t)(returnUpdated ? next : previous);
}

static __inline__ __attribute__((always_inline))
int8_t AtomicCompareAndSwap8(uint8_t *address, int8_t expected,
		uint8_t replacement)
{
	int8_t previous;

	AtomicFullBarrier();
	do {
		previous = (int8_t)AtomicLoadReserved8(address);
		if (previous != expected)
			break;
	} while (AtomicStoreReserved8(address, replacement));
	AtomicFullBarrier();
	return previous;
}

static __inline__ __attribute__((always_inline))
int16_t AtomicCompareAndSwap16(uint16_t *address, int16_t expected,
		uint16_t replacement)
{
	int16_t previous;

	AtomicFullBarrier();
	do {
		previous = (int16_t)AtomicLoadReserved16(address);
		if (previous != expected)
			break;
	} while (AtomicStoreReserved16(address, replacement));
	AtomicFullBarrier();
	return previous;
}

static __inline__ __attribute__((always_inline))
int AtomicAddWithLimit8(uint8_t *address, int32_t value, int32_t limit)
{
	int32_t next;
	int reached;

	AtomicFullBarrier();
	do {
		next = (int32_t)((uint32_t)(int32_t)(int8_t)
				AtomicLoadReserved8(address) + (uint32_t)value);
		reached = next >= limit;
		if (next > limit)
			next = limit;
	} while (AtomicStoreReserved8(address, (uint32_t)next));
	AtomicFullBarrier();
	return reached;
}

static __inline__ __attribute__((always_inline))
int AtomicAddWithLimit16(uint16_t *address, int32_t value, int32_t limit)
{
	int32_t next;
	int reached;

	AtomicFullBarrier();
	do {
		next = (int32_t)((uint32_t)(int32_t)(int16_t)
				AtomicLoadReserved16(address) + (uint32_t)value);
		reached = next >= limit;
		if (next > limit)
			next = limit;
	} while (AtomicStoreReserved16(address, (uint32_t)next));
	AtomicFullBarrier();
	return reached;
}

/* Sony decrements zero too in the byte and halfword variants. */
static __inline__ __attribute__((always_inline))
int8_t AtomicDecrementNonnegative8(uint8_t *address)
{
	uint32_t previous;

	AtomicFullBarrier();
	do {
		previous = AtomicLoadReserved8(address);
		if ((int8_t)previous < 0)
			break;
	} while (AtomicStoreReserved8(address, previous - 1));
	AtomicFullBarrier();
	return (int8_t)previous;
}

static __inline__ __attribute__((always_inline))
int16_t AtomicDecrementNonnegative16(uint16_t *address)
{
	uint32_t previous;

	AtomicFullBarrier();
	do {
		previous = AtomicLoadReserved16(address);
		if ((int16_t)previous < 0)
			break;
	} while (AtomicStoreReserved16(address, previous - 1));
	AtomicFullBarrier();
	return (int16_t)previous;
}

static __inline__ __attribute__((always_inline))
void AtomicSet32(uint32_t *address, uint32_t value)
{
	do {
		AtomicLoadReserved32(address);
	} while (AtomicStoreReserved32(address, value));
}

static __inline__ __attribute__((always_inline))
uint32_t AtomicUpdate32(uint32_t *address, uint32_t operand,
		enum AtomicOperation operation, int returnUpdated)
{
	uint32_t previous, next;

	AtomicFullBarrier();
	do {
		previous = AtomicLoadReserved32(address);
		switch (operation) {
		case ATOMIC_SET:
			next = operand;
			break;
		case ATOMIC_ADD:
			next = previous + operand;
			break;
		case ATOMIC_SUBTRACT:
			next = previous - operand;
			break;
		case ATOMIC_AND:
			next = previous & operand;
			break;
		case ATOMIC_OR:
			next = previous | operand;
			break;
		case ATOMIC_XOR:
			next = previous ^ operand;
			break;
		case ATOMIC_CLEAR:
			next = previous & ~operand;
			break;
		}
	} while (AtomicStoreReserved32(address, next));
	AtomicFullBarrier();
	return returnUpdated ? next : previous;
}

static __inline__ __attribute__((always_inline))
int AtomicAddWithLimit32(uint32_t *address, uint32_t value, int32_t limit)
{
	uint32_t next;
	int reached;

	AtomicFullBarrier();
	do {
		next = AtomicLoadReserved32(address) + value;
		reached = (int32_t)next >= limit;
		if ((int32_t)next > limit)
			next = (uint32_t)limit;
	} while (AtomicStoreReserved32(address, next));
	AtomicFullBarrier();
	return reached;
}

static __inline__ __attribute__((always_inline))
uint32_t AtomicDecrementPositive32(uint32_t *address)
{
	uint32_t previous;

	AtomicFullBarrier();
	do {
		previous = AtomicLoadReserved32(address);
		if ((int32_t)previous <= 0)
			break;
	} while (AtomicStoreReserved32(address, previous - 1));
	AtomicFullBarrier();
	return previous;
}

static __inline__ __attribute__((always_inline))
void AtomicSet64(uint64_t *address, uint64_t value)
{
	do {
		AtomicLoadReserved64(address);
	} while (AtomicStoreReserved64(address, value));
}

static __inline__ __attribute__((always_inline))
uint64_t AtomicUpdate64(uint64_t *address, uint64_t operand,
		enum AtomicOperation operation, int returnUpdated)
{
	uint64_t previous, next;

	AtomicFullBarrier();
	do {
		previous = AtomicLoadReserved64(address);
		switch (operation) {
		case ATOMIC_SET:
			next = operand;
			break;
		case ATOMIC_ADD:
			next = previous + operand;
			break;
		case ATOMIC_SUBTRACT:
			next = previous - operand;
			break;
		case ATOMIC_AND:
			next = previous & operand;
			break;
		case ATOMIC_OR:
			next = previous | operand;
			break;
		case ATOMIC_XOR:
			next = previous ^ operand;
			break;
		case ATOMIC_CLEAR:
			next = previous & ~operand;
			break;
		}
	} while (AtomicStoreReserved64(address, next));
	AtomicFullBarrier();
	return returnUpdated ? next : previous;
}

static __inline__ __attribute__((always_inline))
uint64_t AtomicCompareAndSwap64(uint64_t *address, uint64_t expected,
		uint64_t replacement)
{
	uint64_t previous;

	AtomicFullBarrier();
	do {
		previous = AtomicLoadReserved64(address);
		if (previous != expected)
			break;
	} while (AtomicStoreReserved64(address, replacement));
	AtomicFullBarrier();
	return previous;
}

static __inline__ __attribute__((always_inline))
int AtomicAddWithLimit64(uint64_t *address, uint64_t value, int64_t limit)
{
	uint64_t next;
	int reached;

	AtomicFullBarrier();
	do {
		next = AtomicLoadReserved64(address) + value;
		reached = (int64_t)next >= limit;
		if ((int64_t)next > limit)
			next = (uint64_t)limit;
	} while (AtomicStoreReserved64(address, next));
	AtomicFullBarrier();
	return reached;
}

static __inline__ __attribute__((always_inline))
uint64_t AtomicDecrementPositive64(uint64_t *address)
{
	uint64_t previous;

	AtomicFullBarrier();
	do {
		previous = AtomicLoadReserved64(address);
		if ((int64_t)previous <= 0)
			break;
	} while (AtomicStoreReserved64(address, previous - 1));
	AtomicFullBarrier();
	return previous;
}

#endif
