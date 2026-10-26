/*
 * The interrupt handlers (ST's stm32f10x_it): each exception and interrupt calls the
 * handler a driver registered for it.
 */
#ifndef STM32F10X_IT_H
#define STM32F10X_IT_H

#include "stm32f10x_type.h"
#include "Callback.h"

typedef Callback<void ()> IrqHandler;

/* the exception's handler (its vector number: 11 SVCall, 14 PendSV, 15 SysTick, from 16
   the interrupts), at the priority given; an interrupt is enabled */
void RegisterIrqHandler(s8 exception, IrqHandler::Func func, void *arg, u8 preemption, u8 sub);

/* the EXTI line's handler, the line masked: lines 0-4 have interrupts of their own, 5-9
   and 10-15 share one */
void RegisterExtiHandler(s8 line, IrqHandler::Func func, void *arg, u8 preemption, u8 sub);

/* the shared interrupts' priorities: EXTI9_5, EXTI15_10, ADC1_2, two bytes each
   (preemption, sub) */
void SetSharedIrqPriorities(const u8 *priority);

#endif
