/* Copyright (C) 2011, 2012 The uOFW team
   See the file COPYING for copying permission.
*/

#ifndef COMMON_INCLUDED
# error "Only include common_imp.h or common_header.h!"
#endif

#define COP0_CTRL_EPC            0
#define COP0_CTRL_STATUS         2
#define COP0_CTRL_CAUSE          3
#define COP0_CTRL_V0             4
#define COP0_CTRL_V1             5
#define COP0_CTRL_EXC_TABLE      8
#define COP0_CTRL_NMI_HANDLER    9
#define COP0_CTRL_SC_TABLE      12
#define COP0_CTRL_IS_INTERRUPT  13
#define COP0_CTRL_SP_KERNEL     14
#define COP0_CTRL_SP_USER       15
#define COP0_CTRL_TCB           16
#define COP0_CTRL_NMI_TABLE     18
#define COP0_CTRL_23            23
#define COP0_CTRL_PROFILER_BASE 25

#define COP0_STATE_COUNT    9
#define COP0_STATE_COMPARE 11
#define COP0_STATE_STATUS  12
#define COP0_STATE_SCCODE  21
#define COP0_STATE_CPUID   22

#define pspCop0StateGet(reg) ({ int _ret; __asm__ __volatile__("mfc0 %0, $%1" : "=r" (_ret) : "i" (reg)); _ret; })
#define pspCop0StateSet(reg, val) __asm__ __volatile__("mtc0 %0, $%1" : : "r" (val), "i" (reg))
#define pspCop0CtrlGet(reg) ({ int _ret; __asm__ __volatile__("cfc0 %0, $%1" : "=r" (_ret) : "i" (reg)); _ret; })
#define pspCop0CtrlSet(reg, val) __asm__ __volatile__("ctc0 %0, $%1" : : "r" (val), "i" (reg))

