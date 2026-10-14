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

#define TX_SUCCESS	0
#define TX_INT_DISABLE	0x80
#define TX_AUTO_ACTIVATE	1

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

UINT tx_timer_create(TX_TIMER *timer, CHAR *name, void (*expiration_function)(ULONG),
		     ULONG expiration_input, ULONG initial_ticks, ULONG reschedule_ticks,
		     UINT auto_activate);
UINT tx_timer_activate(TX_TIMER *timer);
UINT tx_timer_deactivate(TX_TIMER *timer);
UINT tx_timer_change(TX_TIMER *timer, ULONG initial_ticks, ULONG reschedule_ticks);

/* ThreadX kernel state kept by the ROM in DTCM; nonzero while in an ISR. */
extern volatile ULONG _tx_thread_system_state;

#endif /* ROBIN_THREADX_H */
