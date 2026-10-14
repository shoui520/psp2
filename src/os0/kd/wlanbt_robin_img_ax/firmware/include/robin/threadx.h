/*
 * ThreadX services provided by the 88W8787 Mask ROM (0x03f00000).
 *
 * The ROM contents are not part of the firmware image, so these entry points
 * are identified by how the firmware uses them: main() hands over to the
 * kernel after installing _tx_initialize_low_level / tx_application_define,
 * interrupt control takes TX_INT_DISABLE (0x80), and timers are driven with
 * the tx_timer_create/activate/deactivate/change sequence on a 44-byte
 * TX_TIMER. Addresses are in boot/rom_syms.ld.
 */
#ifndef ROBIN_THREADX_H
#define ROBIN_THREADX_H

#include <stdint.h>

typedef unsigned long ULONG;
typedef unsigned int UINT;
typedef char CHAR;
typedef struct TX_THREAD_STRUCT TX_THREAD;

/*
 * Control blocks are laid out by the Mask ROM (ThreadX 5.x for ARM9). Only the
 * sizes the firmware reserves for them are known: its OSA workspace embeds a
 * byte or block pool in 52 bytes, a semaphore in 28 and a thread in 148.
 */
typedef struct TX_BYTE_POOL_STRUCT { ULONG words[13]; } TX_BYTE_POOL;
typedef struct TX_BLOCK_POOL_STRUCT { ULONG words[13]; } TX_BLOCK_POOL;
typedef struct TX_SEMAPHORE_STRUCT { ULONG words[7]; } TX_SEMAPHORE;
typedef struct TX_QUEUE_STRUCT TX_QUEUE;
typedef struct TX_EVENT_FLAGS_GROUP_STRUCT TX_EVENT_FLAGS_GROUP;
struct TX_THREAD_STRUCT { ULONG words[37]; };

#define TX_SUCCESS	0
#define TX_INT_DISABLE	0x80
#define TX_AUTO_ACTIVATE	1
#define TX_DONT_START		0
#define TX_AND			2
#define TX_AND_CLEAR		3
#define TX_OR			0
#define TX_OR_CLEAR		1
#define TX_NO_WAIT		0
#define TX_WAIT_FOREVER		0xffffffffUL

typedef struct TX_TIMER_INTERNAL_STRUCT {
	ULONG tx_timer_internal_remaining_ticks;
	ULONG tx_timer_internal_re_initialize_ticks;
	void (*tx_timer_internal_timeout_function)(ULONG);
	ULONG tx_timer_internal_timeout_param;
	struct TX_TIMER_INTERNAL_STRUCT *tx_timer_internal_active_next;
	struct TX_TIMER_INTERNAL_STRUCT *tx_timer_internal_active_previous;
	struct TX_TIMER_INTERNAL_STRUCT **tx_timer_internal_list_head;
} TX_TIMER_INTERNAL;

typedef struct TX_TIMER_STRUCT {
	ULONG tx_timer_id;
	CHAR *tx_timer_name;
	TX_TIMER_INTERNAL tx_timer_internal;
	struct TX_TIMER_STRUCT *tx_timer_created_next;
	struct TX_TIMER_STRUCT *tx_timer_created_previous;
} TX_TIMER;

void tx_kernel_enter(void);
UINT tx_interrupt_control(UINT new_posture);
ULONG tx_time_get(void);
void _tx_timer_interrupt(void);

/* Callable ROM interfaces; the private TX_THREAD layout stays opaque. */
UINT tx_thread_suspend(TX_THREAD *thread);
UINT tx_thread_resume(TX_THREAD *thread);
UINT tx_thread_sleep(ULONG ticks);
UINT tx_thread_priority_change(TX_THREAD *thread, UINT priority, UINT *previous);
TX_THREAD *tx_thread_identify(void);
void tx_thread_relinquish(void);

UINT tx_thread_create(TX_THREAD *thread, CHAR *name, void (*entry)(ULONG),
		      ULONG input, void *stack_start, ULONG stack_size,
		      UINT priority, UINT preempt_threshold, ULONG time_slice,
		      UINT auto_start);

UINT tx_byte_pool_create(TX_BYTE_POOL *pool, CHAR *name, void *start, ULONG size);
UINT tx_byte_pool_delete(TX_BYTE_POOL *pool);
UINT tx_byte_allocate(TX_BYTE_POOL *pool, void **memory, ULONG size, ULONG wait);
UINT tx_byte_release(void *memory);
UINT tx_block_pool_create(TX_BLOCK_POOL *pool, CHAR *name, ULONG block_size,
			  void *start, ULONG size);
UINT tx_block_pool_delete(TX_BLOCK_POOL *pool);
UINT tx_block_allocate(TX_BLOCK_POOL *pool, void **block, ULONG wait);
UINT tx_block_release(void *block);

UINT tx_queue_create(TX_QUEUE *queue, CHAR *name, UINT message_size,
		     void *start, ULONG size);
UINT tx_queue_send(TX_QUEUE *queue, void *message, ULONG wait);
UINT tx_queue_receive(TX_QUEUE *queue, void *message, ULONG wait);

UINT tx_event_flags_create(TX_EVENT_FLAGS_GROUP *group, CHAR *name);
UINT tx_event_flags_set(TX_EVENT_FLAGS_GROUP *group, ULONG flags, UINT option);
UINT tx_event_flags_get(TX_EVENT_FLAGS_GROUP *group, ULONG requested, UINT option,
			ULONG *actual, ULONG wait);
UINT tx_event_flags_info_get(TX_EVENT_FLAGS_GROUP *group, CHAR **name,
			     ULONG *current, TX_THREAD **first_suspended,
			     ULONG *suspended_count,
			     TX_EVENT_FLAGS_GROUP **next_group);

UINT tx_semaphore_create(TX_SEMAPHORE *semaphore, CHAR *name, ULONG initial_count);
UINT tx_semaphore_get(TX_SEMAPHORE *semaphore, ULONG wait);
UINT tx_semaphore_put(TX_SEMAPHORE *semaphore);

UINT tx_timer_create(TX_TIMER *timer, CHAR *name, void (*expiration_function)(ULONG),
		     ULONG expiration_input, ULONG initial_ticks, ULONG reschedule_ticks,
		     UINT auto_activate);
UINT tx_timer_activate(TX_TIMER *timer);
UINT tx_timer_deactivate(TX_TIMER *timer);
UINT tx_timer_change(TX_TIMER *timer, ULONG initial_ticks, ULONG reschedule_ticks);

/* ThreadX kernel state kept by the ROM in DTCM; nonzero while in an ISR. */
extern volatile ULONG _tx_thread_system_state;

#endif /* ROBIN_THREADX_H */
