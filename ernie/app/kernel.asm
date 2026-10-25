; The real-time kernel: a µITRON 4.0 kernel for the 78K0R (written in assembly; the service
; calls pass µITRON's function codes in BC to the trace hooks). Written from the original.
;
; Tasks and cyclic handlers share one table of control blocks, indexed by ID (tasks first,
; the cyclic handlers after them). A task is on one ready queue per priority (a circular
; list through taskNext/taskPrev, kReadyMap's bit set while it is not empty), on the
; timeout queue while it waits with a timeout (taskTmoNext/taskTmoPrev, each entry's count
; relative to the one before it), and on its object's wait queue. The configuration (the
; objects' counts and initial values, from 0xd5fe on) is the configurator's; the RAM below
; is laid out by it.
;
; A service call: CPU_LOCK, serviceEnter (on the kernel stack unless an interrupt is being
; handled, the trace hook told), the work, then _serviceExit with the error code in HL, or
; _dispatch with it in kResult when the call may have readied a task of higher priority.

	NAME	kernel

	PUBLIC	_iact_tsk, _act_tsk, _ichg_pri, _chg_pri, _clr_flg, _dis_dsp, _dly_tsk
	PUBLIC	_ena_dsp, _ext_tsk, _get_mpf, _pget_mpf, _tget_mpf, _rcv_mbx, _prcv_mbx
	PUBLIC	_trcv_mbx, _ref_flg, _ref_mbx, _rel_mpf, _ret_int, _iset_flg, _set_flg
	PUBLIC	_snd_mbx, _sta_cyc, _isig_tim, _wai_flg, _pol_flg, _twai_flg
	PUBLIC	__kernel_int_entry, _kernelStart
	PUBLIC	_cycInit, _flgInit, _mbxInit, _mpfInit
	PUBLIC	_readyAndDispatch, _waitEnter, _restoreRegs, _retIntIdle, _dispatch
	PUBLIC	_serviceExit, _taskStart, _kernelUnused
	PUBLIC	_traceServiceEnter, _traceServiceExit, _traceInterruptEnter
	PUBLIC	_traceInterruptExit, _traceTaskStart, _traceDispatch
	EXTRN	_kernelInitHook, _standby
	EXTRN	@SEGDE, @RTARG0, @RTARG2, @RTARG4, @RTARG6

; the configuration (page 0)
CYC_HANDLER	EQU	0D60AH		; a cyclic handler's address and its CS
CYC_SEG		EQU	0D60CH
CYC_TIME	EQU	0D60EH		; its period, 32 bits
MPF_AREA	EQU	0D612H		; a fixed-size pool's memory (a far pointer)
MPF_SIZE	EQU	0D61CH		; its block size
MPF_BLOCKS	EQU	0D620H		; its block count
TASK_ENTRY	EQU	0D622H		; a task's entry
TASK_ATTR	EQU	0D624H		; its CS (bits 0-3), TA_* (bits 4-7) and priority (bits 10-14)
TASK_EXINF	EQU	0D63AH		; its extended information, 32 bits
TASK_STACK	EQU	0D654H		; the top of its stack
KERNEL_SP	EQU	0D662H		; the kernel's stack
HOOK_TABLE	EQU	0D664H		; where kHooks is (a far pointer)
INIT_TABLE	EQU	0D668H		; the objects' initialisers, ended by a CS of 0xff
FLG_ATTR	EQU	0D67BH		; an event flag's attributes (TA_CLR 4)
TASK_COUNT	EQU	0D682H
FLG_COUNT	EQU	0D684H
MBX_COUNT	EQU	0D685H
MBXPRI_COUNT	EQU	0D686H		; the mailboxes with prioritised messages
MPF_COUNT	EQU	0D687H
CYC_COUNT	EQU	0D688H
PRI_COUNT	EQU	0D689H

; the kernel's RAM (page 0xf)
kTimerHead	EQU	0DD46H		; the timeout queue's first task (and at +1 the next tick's)
kTick		EQU	0DD48H		; isig_tim's ticks not handled yet, bit 7 while it runs
kFlags		EQU	0DD49H		; 1 dispatching disabled, 8 a dispatch pending, 0x20 a
					; tick queueing, 0x80 outside the kernel
kRunTask	EQU	0DD4AH		; the running task, 0xff none
kTopPri		EQU	0DD4BH		; the highest priority ready
kNest		EQU	0DD4CH		; service calls running
kIntNest	EQU	0DD4DH		; interrupts being handled
kReadyMap	EQU	0DD4EH		; a bit for each priority with a ready task
kTrace		EQU	0DD50H		; the trace hooks' output
kReadyHead	EQU	0DD52H		; each priority's first ready task (from 1)
kHooks		EQU	0DD56H		; the trace hooks (six far pointers)
taskTmo		EQU	0DD6EH		; each task's timeout, 32 bits
taskWaitPkt	EQU	0DD8AH		; where a waiting task gets its result (a far pointer)
taskSp		EQU	0DDA4H		; a task's stack while it does not run, 0 not started
taskWaitPtn	EQU	0DDB0H		; the flag pattern it waits for
taskWaitObj	EQU	0DDBCH		; the object it waits on, then its result
taskState	EQU	0DDC9H		; its wait state (bits 0-4), 0x20 preempted by an interrupt
taskWait	EQU	0DDD0H		; TWF_ORW 1, 0x10 timed, 0x20 queued, 0x40 released
taskTmoNext	EQU	0DDD7H
taskTmoPrev	EQU	0DDDEH
taskPri		EQU	0DDE5H
taskWupCnt	EQU	0DDEBH
taskSusCnt	EQU	0DDF1H
taskActCnt	EQU	0DDF7H		; activations queued
taskNext	EQU	0DDFDH		; the queue the task is on, 0xff dormant
taskPrev	EQU	0DE03H
flgPattern	EQU	0DE08H
semWait		EQU	0DE09H		; (no semaphores are configured)
flgWaitTask	EQU	0DE15H		; the task waiting on a flag, 0xff none
mbxHead		EQU	0DE18H		; a mailbox's first message (a far pointer, CS 0xff none)
mbxHeadSeg	EQU	0DE1AH
mbxWait		EQU	0DE1BH		; its waiting tasks
mbxTail		EQU	0DE1CH
mbxTailSeg	EQU	0DE1EH
mpfFree		EQU	0DE20H		; a pool's first free block (a far pointer)
mpfFreeSeg	EQU	0DE22H
mpfWait		EQU	0DE23H
mbxMsgCount	EQU	0DE23H		; a prioritised mailbox's count of messages

; the kernel's short direct work registers, and the compiler's
kResult		EQU	0FFE74H		; a service's result, or an index it keeps
kTemp		EQU	0FFE76H
kTimeout	EQU	0FFE78H		; a service's timeout, 32 bits
regVar		EQU	0FFEB4H		; the compiler's register variables (16 bytes)
SEGAX		EQU	0FFED4H

; interrupts of level 1 and below masked, unless the caller runs at level 0
CPU_LOCK	MACRO
	MOV	A, PSW
	MOV	X, A
	AND	A, #6
	SKZ
	XOR	A, #2
	XOR	A, X
	MOV	PSW, A
	ENDM

@@KERNEL	CSEG	BASE

; ER act_tsk(ID tskid)
_iact_tsk:
	MOVW	BC, #0FF8FH
	BR	$?actTsk
_act_tsk:
	MOVW	BC, #0FFF9H
?actTsk:
	PUSH	PSW
	PUSH	HL
	PUSH	AX
	CPU_LOCK
	POP	AX
	CMP0	X
	BNZ	$?actTsk1
	MOV	ES, #0FH
	MOV	X, ES:!kRunTask
?actTsk1:
	CALL	$!_serviceEnter
	MOVW	BC, AX
	CALL	$!_taskActivate
	BR	$!_dispatch

; ER chg_pri(ID tskid, PRI tskpri)
_ichg_pri:
	MOVW	BC, #0FFF3H
	BR	$?chgPri
_chg_pri:
	MOVW	BC, #0FFF3H
?chgPri:
	PUSH	PSW
	PUSH	HL
	PUSH	AX
	CPU_LOCK
	POP	AX
	CMP0	X
	BNZ	$?chgPri1
	MOV	ES, #0FH
	MOV	X, ES:!kRunTask
?chgPri1:
	CALL	$!_serviceEnter
	MOVW	BC, AX
	MOV	A, ES:taskNext[C]
	CMP	A, #0FFH
	BZ	$?chgPriDormant
	MOV	A, [HL+0CH]
	CMP0	A
	BNZ	$?chgPri2
	PUSH	BC			; TPRI_INI: the configured priority
	SHLW	BC, 2
	MOV	ES, #0
	MOVW	AX, ES:TASK_ATTR[BC]
	MOV	ES, #0FH
	SHL	A, 1
	SHR	A, 3
	POP	BC
?chgPri2:
	MOV	X, A
	MOV	A, ES:taskState[C]
	AND	A, #0DFH
	CMP	A, #0
	BNZ	$?chgPriWait
	PUSH	AX			; ready: requeued at its new priority
	PUSH	BC
	CALL	$!_readyRemove
	POP	BC
	POP	AX
	XCH	A, X
	MOV	ES:taskPri[C], A
	XCH	A, X
	MOVW	kResult, #0
	BR	$!_readyAndDispatch
?chgPriWait:
	MOV	A, X
	MOV	ES:taskPri[C], A
	MOVW	HL, #0
	BR	$!_serviceExit
?chgPriDormant:
	MOVW	HL, #0FFD7H		; E_OBJ
	BR	$!_serviceExit

; ER clr_flg(ID flgid, FLGPTN clrptn)
_clr_flg:
	PUSH	PSW
	PUSH	HL
	PUSH	AX
	CPU_LOCK
	POP	AX
	MOVW	BC, #0FFD4H
	CALL	$!_serviceEnter
	MOVW	BC, AX
	SHL	C, 1
	MOVW	AX, [HL+0CH]
	MOVW	DE, AX
	MOVW	AX, ES:flgPattern[C]
	AND	A, D
	XCH	A, X
	AND	A, E
	XCH	A, X
	MOVW	ES:flgPattern[C], AX
	MOVW	HL, #0
	BR	$!_serviceExit

; ER dis_dsp(void)
_dis_dsp:
	PUSH	PSW
	PUSH	HL
	CPU_LOCK
	MOVW	BC, #0FFA5H
	CALL	$!_serviceEnter
	MOV	A, ES:!kFlags
	OR	A, #1
	MOV	ES:!kFlags, A
	MOVW	HL, #0
	BR	$!_serviceExit

; ER dly_tsk(RELTIM dlytim)
_dly_tsk:
	PUSH	PSW
	PUSH	HL
	PUSH	AX
	CPU_LOCK
	POP	AX
	MOVW	kTimeout, AX
	MOVW	AX, BC
	MOVW	kTimeout+2, AX
	MOVW	BC, #0FFE7H
	CALL	$!_serviceEnter
	CLRB	B
	MOV	C, ES:!kRunTask
	MOV	ES:taskState[C], #2
	PUSH	BC
	CALL	$!_timerInsert
	POP	BC
	CALL	$!_readyRemove
	MOVW	kResult, #0FFFFH
	BR	$!_dispatch

; ER ena_dsp(void)
_ena_dsp:
	PUSH	PSW
	PUSH	HL
	CPU_LOCK
	MOVW	BC, #0FFA4H
	CALL	$!_serviceEnter
	MOV	A, ES:!kFlags
	AND	A, #0FEH
	MOV	ES:!kFlags, A
	MOVW	kResult, #0
	BR	$!_dispatch

; void ext_tsk(void): dormant, or started again for a queued activation
_ext_tsk:
	PUSH	PSW
	PUSH	HL
	CPU_LOCK
	MOVW	BC, #0FFF6H
	CALL	$!_serviceEnter
	CLRW	BC
	MOV	C, ES:!kRunTask
	PUSH	BC
	CALL	$!_readyRemove
	MOV	ES:!kRunTask, #0FFH
	POP	BC
	PUSH	BC
	CALL	$!?taskReset
	POP	BC
	MOV	A, ES:taskActCnt[C]
	CMP0	A
	BZ	$?extTsk1
	DEC	A
	MOV	ES:taskActCnt[C], A
	MOV	A, ES:taskState[C]
	CALL	$!_makeReady
?extTsk1:
	MOVW	kResult, #0
	BR	$!_dispatch

; ER get_mpf(ID mpfid, VP *p_blk), pget_mpf, tget_mpf(..., TMO tmout)
_get_mpf:
	PUSH	PSW
	PUSH	HL
	PUSH	AX
	CPU_LOCK
	POP	AX
	MOVW	kTimeout, #0FFFFH
	MOVW	kTimeout+2, #0FFFFH
	MOVW	BC, #0FFB7H
	BR	$?getMpf
_pget_mpf:
	PUSH	PSW
	PUSH	HL
	PUSH	AX
	CPU_LOCK
	POP	AX
	MOVW	kTimeout, #0
	MOVW	kTimeout+2, #0
	MOVW	BC, #0FFB6H
	BR	$?getMpf
_tget_mpf:
	PUSH	PSW
	PUSH	HL
	PUSH	AX
	CPU_LOCK
	POP	AX
	MOVW	DE, AX
	MOVW	AX, [SP+0CH]
	MOVW	kTimeout, AX
	MOVW	AX, [SP+0EH]
	MOVW	kTimeout+2, AX
	MOVW	AX, DE
	MOVW	BC, #0FFB5H
?getMpf:
	CALL	$!_serviceEnter
	MOVW	BC, AX
	MOVW	kResult, AX
	SHLW	BC, 2
	MOV	A, ES:mpfFreeSeg[BC]
	CMP	A, #0FFH
	BNZ	$?getMpfBlock
	CLRW	AX			; no block free: wait, unless polling
	CMPW	AX, kTimeout+2
	BNZ	$?getMpfWait
	CMPW	AX, kTimeout
	BZ	$?getMpfTimeout
?getMpfWait:
	MOV	C, ES:!kRunTask
	CLRB	B
	DECW	AX
	CMPW	AX, kTimeout+2
	BNZ	$?getMpfTimer
	CMPW	AX, kTimeout
	BZ	$?getMpfForever
?getMpfTimer:
	PUSH	BC
	PUSH	HL
	CALL	$!_timerInsert
	POP	HL
	POP	BC
?getMpfForever:
	MOV	ES:taskState[BC], #0EH
	MOVW	AX, kResult
	PUSH	BC
	SHLW	BC, 1
	MOVW	ES:taskWaitObj[BC], AX
	SHLW	BC, 1
	MOV	A, [HL+0EH]
	MOV	ES:taskWaitPkt+2[BC], A
	MOVW	AX, [HL+0CH]
	MOVW	ES:taskWaitPkt[BC], AX
	POP	BC
	PUSH	BC
	CALL	$!_readyRemove
	MOVW	AX, kResult
	SHLW	AX, 2
	ADDW	AX, #mpfWait
	MOVW	DE, AX
	POP	BC
	MOVW	kResult, #0FFFFH
	BR	$!_waitEnter
?getMpfTimeout:
	MOVW	HL, #0FFCEH		; E_TMOUT
	BR	$!_serviceExit
?getMpfBlock:
	MOV	kTemp, A
	MOVW	AX, ES:mpfFree[BC]
	MOVW	DE, AX
	MOV	ES, kTemp
	MOV	A, ES:[DE+2]
	CMP	A, #0FFH
	BNZ	$?getMpfNext
	MOV	ES, #0FH
	MOV	ES:mpfFreeSeg[BC], A
	BR	$?getMpfDone
?getMpfNext:
	MOV	kTemp+1, A
	MOVW	AX, ES:[DE+0]
	MOV	ES, #0FH
	MOVW	ES:mpfFree[BC], AX
	MOV	A, kTemp+1
	MOV	ES:mpfFreeSeg[BC], A
?getMpfDone:
	MOV	A, [HL+0EH]
	MOV	ES, A
	MOVW	AX, [HL+0CH]
	MOVW	HL, AX
	MOV	A, kTemp
	MOV	ES:[HL+2], A
	MOVW	AX, DE
	MOVW	ES:[HL+0], AX
	MOV	ES, #0FH
	MOVW	HL, #0
	BR	$!_serviceExit

; the cyclic handlers' control blocks: TA_STA ones started
_cycInit:
	MOV	ES, #0
	MOV	A, ES:!CYC_COUNT
	MOV	kResult, A
	ADD	A, ES:!TASK_COUNT
	MOV	kResult+1, A
?cyc:
	MOV	C, kResult
	CLRB	B
	MOV	ES, #0
	SHLW	BC, 2
	MOVW	AX, ES:CYC_TIME[BC]
	MOVW	DE, AX
	MOVW	AX, ES:CYC_TIME+2[BC]
	MOVW	HL, AX
	MOVW	AX, ES:CYC_SEG[BC]
	SHR	A, 2
	CMP0	A
	MOV	ES, #0FH
	MOV	C, kResult+1
	CLRB	B
	MOV	ES:taskState[BC], A
	MOV	ES:taskWait[BC], #0
	BNZ	$?cycStart
	SHLW	BC, 2
	MOVW	AX, DE
	MOVW	ES:taskTmo[BC], AX
	MOVW	AX, HL
	MOVW	ES:taskTmo+2[BC], AX
	BR	$?cycNext
?cycStart:
	MOVW	AX, DE
	MOVW	kTimeout, AX
	MOVW	AX, HL
	MOVW	kTimeout+2, AX
	MOV	ES, #0FH
	MOV	C, kResult+1
	CLRB	B
	CALL	$!_timerInsert
?cycNext:
	DEC	kResult+1
	DEC	kResult
	BNZ	$?cyc
	RET

; the event flags: no task waiting, the patterns 0
_flgInit:
	MOV	ES, #0
	MOV	A, ES:!FLG_COUNT
	MOV	C, A
	MOV	B, A
	SHL	B, 1
	MOV	ES, #0FH
	CLRW	AX
?flg:
	MOV	ES:flgWaitTask[C], #0FFH
	MOVW	ES:flgPattern[B], AX
	DEC	B
	DEC	B
	DEC	C
	BNZ	$?flg
	RET

; the mailboxes: empty, nothing waiting
_mbxInit:
	CLRW	AX
	MOV	ES, #0
	MOV	X, ES:!MBX_COUNT
	MOVW	kResult, AX
	MOV	ES, #0FH
?mbx:
	MOVW	BC, kResult
	SHLW	BC, 2
	MOVW	AX, #0FFFFH
	MOVW	ES:mbxHeadSeg[BC], AX
	MOVW	ES:mbxTailSeg[BC], AX
	DEC	kResult
	BNZ	$?mbx
	MOV	ES, #0
	MOV	A, ES:!MBXPRI_COUNT
	MOV	ES, #0FH
	CMP0	A
	BZ	$?mbxEnd
	MOV	C, A
?mbxPri:
	MOV	ES:mbxMsgCount[C], #0
	DEC	C
	BNZ	$?mbxPri
?mbxEnd:
	RET

; the fixed-size pools: each pool's blocks linked into its free list
_mpfInit:
	CLRW	AX
	MOV	ES, #0
	MOV	A, ES:!MPF_COUNT
	MOV	C, A
	MOV	B, A
	SHL	B, 1
	XCH	A, X
	SHLW	AX, 2
	MOVW	DE, AX
	ADDW	AX, #mpfFree
	MOVW	HL, AX
	MOVW	AX, DE
	ADDW	AX, #MPF_AREA
	MOVW	DE, AX
?mpf:
	MOV	ES, #0
	MOVW	AX, ES:[DE+2]
	MOV	ES, #0FH
	MOV	A, #0FFH
	MOVW	ES:[HL+2], AX
	MOVW	kResult, AX
	MOVW	AX, DE
	MOVW	kTemp, AX
	MOVW	AX, HL
	MOVW	kTimeout, AX
	MOV	ES, #0
	MOVW	AX, ES:[DE+0]
	MOV	ES, #0FH
	MOVW	ES:[HL+0], AX
	MOVW	DE, AX
	MOV	ES, #0
	MOVW	AX, ES:MPF_BLOCKS[B]
	MOVW	HL, AX
	MOVW	AX, ES:MPF_SIZE[B]
	XCHW	AX, BC
	MOVW	kTimeout+2, AX
	MOV	ES, kResult
?mpfBlock:
	MOVW	AX, DE
	ADDW	AX, BC
	XCHW	AX, HL
	SUBW	AX, #1
	XCHW	AX, HL
	BZ	$?mpfLast
	MOVW	ES:[DE+0], AX
	PUSH	AX
	MOV	A, kResult
	MOV	ES:[DE+2], A
	POP	AX
	MOVW	DE, AX
	BR	$?mpfBlock
?mpfLast:
	MOV	A, #0FFH
	MOV	ES:[DE+2], A
	MOVW	DE, kTemp
	MOVW	HL, kTimeout
	MOVW	BC, kTimeout+2
	DEC	B
	DEC	B
	MOVW	AX, DE
	SUBW	AX, #4
	MOVW	DE, AX
	MOVW	AX, HL
	SUBW	AX, #4
	MOVW	HL, AX
	DEC	C
	BNZ	$?mpf
	MOV	ES, #0FH
	RET

; an interrupt handler's entry (#pragma rtos_interrupt): its registers saved under the
; return address, the kernel stack taken for the outermost interrupt, the trace hook told
__kernel_int_entry:
	PUSH	AX
	MOV	A, PSW
	MOV	kTemp+1, A
	AND	A, #0FBH
	OR	A, #82H
	MOV	PSW, A
	PUSH	HL
	PUSH	DE
	MOVW	AX, [SP+6]
	MOVW	DE, AX
	MOVW	AX, [SP+8]
	MOVW	HL, AX
	MOVW	AX, BC
	MOVW	[SP+6], AX
	MOV	A, ES
	MOV	X, A
	MOV	A, CS
	MOVW	[SP+8], AX
	MOV	ES, #0FH
	MOV	A, ES:!kFlags
	AND	A, #7FH
	MOV	ES:!kFlags, A
	MOVW	AX, [SP+0AH]
	MOVW	kResult, AX
	MOV	A, [SP+0CH]
	MOV	kTemp, A
	MOV	A, ES:!kIntNest
	INC	A
	MOV	ES:!kIntNest, A
	CMP	A, #1
	BNZ	$?intNested
	CMP0	ES:!kNest
	BNZ	$?intNested
	MOVW	BC, !0FFF8H
	MOV	ES, #0
	MOVW	AX, ES:!KERNEL_SP
	MOVW	SP, AX
	PUSH	BC
	MOV	ES, #0FH
?intNested:
	PUSH	HL
	PUSH	DE
	MOVW	AX, #kHooks
	MOVW	DE, AX
	MOV	A, ES:[DE+0AH]
	MOV	CS, A
	MOVW	AX, ES:[DE+8]
	MOVW	DE, AX
	CALL	DE
	MOV	A, ES:!kFlags
	OR	A, #80H
	MOV	ES:!kFlags, A
	MOV	A, kTemp+1
	OR	A, #80H
	MOV	PSW, A
	RET

; A, the task's state: readied (its priority's queue, kReadyMap), then the dispatcher
_readyAndDispatch:
	MOV	D, A
	MOVW	AX, #0
	PUSH	AX
	MOVW	AX, #LOWW(_dispatch)
	PUSH	AX
	MOV	A, D

; A, the task's state, C the task: on its ready queue, unless released by another wait
_makeReady:
	AND	A, #40H
	SKZ
	BR	$!?queueRet
	CLRW	AX
	MOV	A, ES:taskPri[C]
	XCH	A, X
	MOVW	HL, #1
	ADDW	AX, #kReadyHead
	MOVW	DE, AX

; C on the queue whose first task DE points to, at its end; HL 1: a ready queue
_queueInsert:
	MOV	A, ES:[DE]
	CMP	A, #0FFH
	BZ	$?queueEmpty
	MOV	ES:taskNext[C], A
	MOV	B, A
	MOV	A, ES:taskPrev[B]
	MOV	ES:taskPrev[C], A
	XCH	A, C
	MOV	ES:taskNext[C], A
	MOV	ES:taskPrev[B], A
	RET
?queueEmpty:
	MOV	A, C
	MOV	ES:taskPrev[C], A
	MOV	ES:taskNext[C], A
	MOV	ES:[DE], A
	MOVW	AX, HL
	CMP0	X
	BZ	$?queueRet
	MOV	A, ES:taskPri[C]	; the priority's bit, and the highest priority
	MOV	D, A
	MOV	B, #1
	SUB	A, #8
	BNC	$?queueBit
	CLRB	B
	MOV	A, D
?queueBit:
	MOV	C, #1
	CMP0	A
	BZ	$?queueSet
?queueShift:
	SHL	C, 1
	DEC	A
	BNZ	$?queueShift
?queueSet:
	MOV	A, ES:kReadyMap[B]
	OR	A, C
	MOV	ES:kReadyMap[B], A
	MOV	A, ES:!kTopPri
	CMP	A, D
	BNH	$?queueRet
	MOV	A, D
	MOV	ES:!kTopPri, A
	MOV	A, ES:!kFlags
	AND	A, #0F7H
	MOV	ES:!kFlags, A
?queueRet:
	RET

; task BC on the timeout queue, kTimeout ticks from now
_timerInsert:
	MOV	A, ES:taskWait[BC]
	MOV	X, A
	AND	A, #30H
	BZ	$?timerInsert1
	XCH	A, X
	PUSH	BC
	CALL	$!_timerRemove
	POP	BC
?timerInsert1:
	MOV	A, #10H
	MOV	ES:taskWait[BC], A
	PUSH	BC
	SHLW	BC, 2
	MOVW	AX, kTimeout
	MOVW	ES:taskTmo[BC], AX
	MOVW	AX, kTimeout+2
	MOVW	ES:taskTmo+2[BC], AX
	POP	BC
	CLRW	AX
	MOV	A, ES:!kTimerHead
	CMP	A, #0FFH
	BZ	$?timerEmpty
	MOV	ES:taskTmoNext[C], A
	MOV	B, A
	MOV	A, ES:taskTmoPrev[B]
	MOV	ES:taskTmoPrev[C], A
	XCH	A, C
	MOV	ES:taskTmoNext[C], A
	MOV	ES:taskTmoPrev[B], A
	RET
?timerEmpty:
	MOV	A, C
	MOV	ES:taskTmoPrev[C], A
	MOV	ES:taskTmoNext[C], A
	MOV	ES:!kTimerHead, A
	RET

; the running task, C, on the wait queue DE points to
_waitEnter:
	MOVW	HL, #0
	CALL	$!_queueInsert
	BR	$!_dispatch

; ER rcv_mbx(ID mbxid, T_MSG **ppk_msg), prcv_mbx, trcv_mbx(..., TMO tmout)
_rcv_mbx:
	PUSH	PSW
	PUSH	HL
	PUSH	AX
	CPU_LOCK
	POP	AX
	MOVW	kTimeout, #0FFFFH
	MOVW	kTimeout+2, #0FFFFH
	MOVW	BC, #0FFBFH
	BR	$?rcvMbx
_prcv_mbx:
	PUSH	PSW
	PUSH	HL
	PUSH	AX
	CPU_LOCK
	POP	AX
	MOVW	kTimeout, #0
	MOVW	kTimeout+2, #0
	MOVW	BC, #0FFBEH
	BR	$?rcvMbx
_trcv_mbx:
	PUSH	PSW
	PUSH	HL
	PUSH	AX
	CPU_LOCK
	POP	AX
	XCHW	AX, DE
	MOVW	AX, [SP+0CH]
	MOVW	kTimeout, AX
	MOVW	AX, [SP+0EH]
	MOVW	kTimeout+2, AX
	MOVW	BC, #0FFBDH
	XCHW	AX, DE
?rcvMbx:
	CALL	$!_serviceEnter
	MOVW	BC, AX
	MOVW	kResult, AX
	SHLW	BC, 2
	MOV	A, ES:mbxHeadSeg[BC]
	CMP	A, #0FFH
	BNZ	$?rcvMsg
	CLRW	AX			; empty: wait, unless polling
	CMPW	AX, kTimeout+2
	BNZ	$?rcvWait
	CMPW	AX, kTimeout
	BZ	$?rcvTimeout
?rcvWait:
	MOV	C, ES:!kRunTask
	CLRB	B
	DECW	AX
	CMPW	AX, kTimeout+2
	BNZ	$?rcvTimer
	CMPW	AX, kTimeout
	BZ	$?rcvForever
?rcvTimer:
	PUSH	BC
	PUSH	HL
	CALL	$!_timerInsert
	POP	HL
	POP	BC
?rcvForever:
	MOV	ES:taskState[BC], #7
	MOVW	AX, kResult
	PUSH	BC
	SHLW	BC, 1
	MOVW	ES:taskWaitObj[BC], AX
	SHLW	BC, 1
	MOV	A, [HL+0EH]
	MOV	ES:taskWaitPkt+2[BC], A
	MOVW	AX, [HL+0CH]
	MOVW	ES:taskWaitPkt[BC], AX
	POP	BC
	PUSH	BC
	CALL	$!_readyRemove
	MOVW	AX, kResult
	SHLW	AX, 2
	ADDW	AX, #mbxWait
	MOVW	DE, AX
	POP	BC
	MOVW	kResult, #0FFFFH
	BR	$!_waitEnter
?rcvTimeout:
	MOVW	HL, #0FFCEH		; E_TMOUT
	BR	$!_serviceExit
?rcvMsg:
	MOV	kTemp, A
	MOVW	AX, ES:mbxHead[BC]
	MOVW	DE, AX
	MOV	ES, #0
	MOV	A, ES:!MBXPRI_COUNT
	CMP	A, kResult
	BC	$?rcvMsg1
	PUSH	BC
	MOV	C, kResult
	MOV	ES, #0FH
	MOV	A, ES:mbxMsgCount[C]
	INC	A
	MOV	ES:mbxMsgCount[C], A
	POP	BC
?rcvMsg1:
	MOV	ES, kTemp
	MOV	A, ES:[DE+2]
	CMP	A, #0FFH
	BNZ	$?rcvNext
	MOV	ES, #0FH
	MOV	ES:mbxTailSeg[BC], A
	MOV	ES:mbxHeadSeg[BC], A
	BR	$?rcvDone
?rcvNext:
	MOV	kTemp+1, A
	MOVW	AX, ES:[DE+0]
	MOV	ES, #0FH
	MOVW	ES:mbxHead[BC], AX
	MOV	A, kTemp+1
	MOV	ES:mbxHeadSeg[BC], A
?rcvDone:
	MOV	A, [HL+0EH]
	MOV	ES, A
	MOVW	AX, [HL+0CH]
	MOVW	HL, AX
	MOV	A, kTemp
	MOV	ES:[HL+2], A
	MOVW	AX, DE
	MOVW	ES:[HL+0], AX
	MOV	ES, #0FH
	MOVW	HL, #0
	BR	$!_serviceExit

; ER ref_flg(ID flgid, T_RFLG *pk_rflg)
_ref_flg:
	PUSH	PSW
	PUSH	HL
	PUSH	AX
	CPU_LOCK
	POP	AX
	MOVW	BC, #0FFD0H
	CALL	$!_serviceEnter
	MOVW	BC, AX
	MOV	A, ES:flgWaitTask[C]
	PUSH	AX
	SHL	C, 1
	MOVW	AX, ES:flgPattern[C]
	PUSH	AX
	MOVW	AX, [HL+0CH]
	MOVW	DE, AX
	MOV	A, [HL+0EH]
	MOV	ES, A
	POP	AX
	MOVW	ES:[DE+2], AX
	POP	AX
	CMP	A, #0FFH
	SKNZ
	CLRB	A
	MOV	ES:[DE+0], A
	MOV	ES, #0FH
	MOVW	HL, #0
	BR	$!_serviceExit

; ER ref_mbx(ID mbxid, T_RMBX *pk_rmbx)
_ref_mbx:
	PUSH	PSW
	PUSH	HL
	PUSH	AX
	CPU_LOCK
	POP	AX
	MOVW	BC, #0FFBCH
	CALL	$!_serviceEnter
	MOVW	BC, AX
	MOV	A, [HL+0EH]
	MOV	kResult, A
	MOVW	AX, [HL+0CH]
	MOVW	HL, AX
	SHLW	BC, 2
	MOVW	AX, ES:mbxHeadSeg[BC]
	MOVW	kTemp, AX
	XCH	A, X
	CMP	A, #0FFH
	BNZ	$?refMbxMsg
	MOV	A, X
	CMP	A, #0FFH
	SKNZ
	CLRB	A
	MOV	ES, kResult
	MOV	ES:[HL+0], A
	CLRW	AX
	MOVW	ES:[HL+4], AX
	MOVW	ES:[HL+2], AX
	BR	$?refMbxEnd
?refMbxMsg:
	MOVW	AX, ES:mbxHead[BC]
	MOV	ES, kResult
	MOVW	ES:[HL+2], AX
	MOVW	AX, kTemp
	CLRB	A
	MOVW	ES:[HL+4], AX
	MOV	ES:[HL+0], A
?refMbxEnd:
	MOV	ES, #0FH
	MOVW	HL, #0
	BR	$!_serviceExit

; ER rel_mpf(ID mpfid, VP blk): to a waiting task, else to the free list
_rel_mpf:
	PUSH	PSW
	PUSH	HL
	PUSH	AX
	CPU_LOCK
	POP	AX
	MOVW	BC, #0FFB9H
	CALL	$!_serviceEnter
	MOVW	BC, AX
	SHLW	BC, 2
	MOVW	AX, ES:mpfFreeSeg[BC]
	CMP	A, #0FFH
	BZ	$?relMpfFree
	MOV	X, A
	CLRB	A
	XCHW	AX, BC
	ADDW	AX, #mpfWait
	MOVW	DE, AX
	PUSH	BC
	CALL	$!_queueRemove
	POP	BC
	PUSH	BC
	MOV	A, ES:taskState[BC]
	AND	A, #0E0H
	MOV	ES:taskState[BC], A
	PUSH	AX
	MOV	A, ES:taskWait[BC]
	OR	A, #40H
	MOV	ES:taskWait[BC], A
	SHLW	BC, 1
	CLRW	AX
	MOVW	ES:taskWaitObj[BC], AX
	SHLW	BC, 1
	MOVW	AX, ES:taskWaitPkt[BC]
	MOVW	DE, AX
	MOV	A, ES:taskWaitPkt+2[BC]
	MOV	ES, A
	MOV	A, [HL+0EH]
	MOV	ES:[DE+2], A
	MOVW	AX, [HL+0CH]
	MOVW	ES:[DE+0], AX
	POP	AX
	POP	BC
	MOVW	kResult, #0
	MOV	ES, #0FH
	BR	$!_readyAndDispatch
?relMpfFree:
	MOV	A, X
	MOV	kTemp, A
	CMP	A, #0FFH
	MOV	A, [HL+0EH]
	MOV	kTemp+1, A
	MOV	ES, A
	MOVW	AX, [HL+0CH]
	MOVW	HL, AX
	MOV	A, kTemp
	MOV	ES:[HL+2], A
	BZ	$?relMpfEmpty
	MOV	ES, #0FH
	MOVW	AX, ES:mpfFree[BC]
	MOV	ES, kTemp+1
	MOVW	ES:[HL+0], AX
?relMpfEmpty:
	MOV	A, ES
	MOV	ES, #0FH
	MOV	ES:mpfFreeSeg[BC], A
	MOVW	AX, HL
	MOVW	ES:mpfFree[BC], AX
	MOVW	HL, #0
	BR	$!_serviceExit

; an interrupt handler's end (#pragma rtos_interrupt): leaving the outermost interrupt, the
; dispatcher runs when a task of higher priority became ready
_ret_int:
	CPU_LOCK
	MOV	ES, #0FH
	MOV	A, ES:!kFlags
	AND	A, #7FH
	MOV	ES:!kFlags, A
	DEC	ES:!kIntNest
	BNZ	$?retIntExit
	CMP0	ES:!kNest
	BNZ	$?retIntExit
	MOV	A, ES:!kFlags
	AND	A, #0DH
	BZ	$_retIntDispatch
	CMP	ES:!kRunTask, #0FFH
	SKNZ
	BR	$!_retIntIdle
	POP	AX
	MOVW	SP, AX
?retIntExit:
	MOVW	AX, #kHooks
	MOVW	DE, AX
	MOV	A, ES:[DE+0EH]
	MOV	CS, A
	MOVW	AX, ES:[DE+0CH]
	MOVW	DE, AX
	CALL	DE
	MOV	A, ES:!kFlags
	OR	A, #80H
	MOV	ES:!kFlags, A
	POP	DE
	POP	HL
	POP	AX
	MOVW	kResult, AX
	POP	BC
	POP	AX
	MOV	CS, A
	XCH	A, X
	MOV	ES, A
	MOVW	AX, kResult
	NOP
	NOP
	RETI
; a task preempted by an interrupt resumed: the compiler's work registers back
_restoreRegs:
	POP	AX
	MOVW	@SEGDE, AX
	POP	AX
	MOVW	SEGAX, AX
	POP	AX
	MOVW	@RTARG6, AX
	POP	AX
	MOVW	@RTARG4, AX
	POP	AX
	MOVW	@RTARG2, AX
	POP	AX
	MOVW	@RTARG0, AX
	BR	$?retIntExit

; the running task preempted: its work registers saved on its stack, then the dispatcher
_retIntDispatch:
	CMP	ES:!kRunTask, #0FFH
	BZ	$?retIntSwitch
	MOVW	AX, SP
	POP	AX
	MOVW	SP, AX
	MOVW	AX, @RTARG0
	PUSH	AX
	MOVW	AX, @RTARG2
	PUSH	AX
	MOVW	AX, @RTARG4
	PUSH	AX
	MOVW	AX, @RTARG6
	PUSH	AX
	MOVW	AX, SEGAX
	PUSH	AX
	MOVW	AX, @SEGDE
	PUSH	AX
	MOVW	BC, !0FFF8H
	MOV	ES, #0
	MOVW	AX, ES:!KERNEL_SP
	MOV	ES, #0FH
	MOVW	SP, AX
	PUSH	BC
	MOV	A, ES:!kRunTask
	MOV	C, A
	MOV	A, ES:taskState[C]
	OR	A, #20H
	MOV	ES:taskState[C], A
?retIntSwitch:
	MOV	ES:!kNest, #1
	CLRW	AX
	MOVW	kResult, AX
	MOV	A, ES:!kFlags
	AND	A, #7FH
	MOV	ES:!kFlags, A
	BR	$!?schedule
_retIntIdle:
	MOVW	AX, #kHooks
	MOVW	DE, AX
	MOV	A, ES:[DE+0EH]
	MOV	CS, A
	MOVW	AX, ES:[DE+0CH]
	MOVW	DE, AX
	CALL	DE
	MOV	ES, #0
	MOVW	AX, ES:!KERNEL_SP
	MOV	ES, #0FH
	MOVW	SP, AX
	BR	$!?idle

; the kernel's start (after main): the control blocks, the tasks with TA_ACT activated, the
; objects' initialisers, the trace hooks, kernelInitHook, then the first dispatch
_kernelStart:
	MOV	A, PSW
	MOV	X, A
	AND	A, #6
	SKZ
	XOR	A, #2
	XOR	A, X
	AND	A, #0D7H
	MOV	PSW, A
	EI
	MOV	ES, #0FH
	ONEW	AX
	MOVW	ES:!kNest, AX
	CLRW	AX
	MOVW	ES:!kTick, AX
	MOVW	ES:!kReadyMap, AX
	MOVW	kResult, AX
	DECW	AX
	MOVW	ES:!kTimerHead, AX
	MOVW	ES:!kRunTask, AX
	MOV	ES, #0
	MOVW	AX, ES:!KERNEL_SP
	MOVW	SP, AX
	MOV	C, ES:!PRI_COUNT
	MOV	A, ES:!TASK_COUNT
	MOV	kTemp, A
	MOV	ES, #0FH
?startPri:
	MOV	ES:kReadyHead[C], #0FFH
	DEC	C
	BNZ	$?startPri
	CLRW	BC
?startTask:
	INC	C
	PUSH	BC
	CALL	$!_taskInit
	POP	BC
	SHLW	AX, 1
	BC	$?startTaskNext
	PUSH	BC
	CALL	$!_taskActivate
	POP	BC
?startTaskNext:
	MOV	A, C
	CMP	A, kTemp
	BNZ	$?startTask
	MOVW	AX, #INIT_TABLE
	SUBW	AX, #4
	MOVW	HL, AX
?startInit:
	MOV	ES, #0
	MOVW	AX, HL
	ADDW	AX, #4
	MOVW	HL, AX
	MOV	A, ES:[HL+2]
	MOV	CS, A
	CMP	A, #0FFH
	BZ	$?startInitEnd
	MOVW	AX, ES:[HL+0]
	MOV	ES, #0FH
	PUSH	HL
	CALL	AX
	POP	HL
	BR	$?startInit
?startInitEnd:
	CALL	$!_hookInit
	CALL	!!_kernelInitHook
	MOV	ES, #0FH
	BR	$!_dispatch

_kernelUnused:
	DB	30H, 0FEH, 0D5H, 00H	; nothing reads these

; the trace hooks in kHooks
_hookInit:
	MOV	ES, #0
	MOVW	AX, #HOOK_TABLE
	MOVW	DE, AX
	MOVW	AX, ES:[DE+0]
	MOVW	BC, AX
	MOV	A, ES:[DE+2]
	MOV	ES, A
	MOVW	AX, #LOWW(_traceServiceEnter)
	MOVW	ES:0[BC], AX
	MOVW	AX, #0
	MOVW	ES:2[BC], AX
	MOVW	AX, #LOWW(_traceServiceExit)
	MOVW	ES:4[BC], AX
	MOVW	AX, #0
	MOVW	ES:6[BC], AX
	MOVW	AX, #LOWW(_traceInterruptEnter)
	MOVW	ES:8[BC], AX
	MOVW	AX, #0
	MOVW	ES:0AH[BC], AX
	MOVW	AX, #LOWW(_traceInterruptExit)
	MOVW	ES:0CH[BC], AX
	MOVW	AX, #0
	MOVW	ES:0EH[BC], AX
	MOVW	AX, #LOWW(_traceTaskStart)
	MOVW	ES:10H[BC], AX
	MOVW	AX, #0
	MOVW	ES:12H[BC], AX
	MOVW	AX, #LOWW(_traceDispatch)
	MOVW	ES:14H[BC], AX
	MOVW	AX, #0
	MOVW	ES:16H[BC], AX
	MOV	ES, #0FH
	RET

; the end of a service that may have readied a task: dispatching, if this is the outermost
; service, dispatching is enabled and one is pending
_dispatch:
	MOVW	AX, ES:!kNest
	CMPW	AX, #1
	BNZ	$?dispatchNone
	MOV	A, ES:!kFlags
	CMP0	A
	BZ	$?dispatchNow
?dispatchNone:
	MOVW	HL, kResult
	BR	$!_serviceExit
?dispatchNow:
	MOVW	HL, kResult
	NOP
	NOP
	MOV	A, PSW
	MOV	X, A
	MOV	A, #6
	OR	A, X
	MOV	PSW, A
	MOV	A, X
	MOV	PSW, A
	MOVW	AX, HL
	MOVW	kResult, AX
	MOV	ES:!kFlags, #8
; the ready task of the highest priority, found through kReadyMap when kTopPri is stale
?schedule:
	MOV	A, ES:!kTopPri
	CMP	A, #0FFH
	SKNZ
	BR	$!?noTask
	MOV	C, A
	MOV	A, ES:kReadyHead[C]
	MOV	C, A
	CMP	A, #0FFH
	BNZ	$?scheduleFound
	MOVW	DE, ES:!kReadyMap
	CLRB	B
	MOV	C, #0FFH
	MOVW	AX, DE
	OR	A, X
	BZ	$?scheduleFound
	MOV	A, E
	CMP0	A
	BNZ	$?scheduleBit4
	MOV	A, D
	MOV	E, A
	MOV	B, #8
?scheduleBit4:
	MOV	A, E
	AND	A, #0FH
	BNZ	$?scheduleBit2
	MOV	A, E
	SHR	A, 4
	MOV	E, A
	MOV	A, B
	ADD	A, #4
	MOV	B, A
?scheduleBit2:
	MOV	A, E
	AND	A, #3
	BNZ	$?scheduleBit1
	MOV	A, E
	SHR	A, 2
	MOV	E, A
	MOV	A, B
	ADD	A, #2
	MOV	B, A
?scheduleBit1:
	MOV	A, E
	AND	A, #1
	BNZ	$?scheduleBit
	MOV	A, B
	INC	A
	MOV	B, A
?scheduleBit:
	MOV	A, B
	MOV	ES:!kTopPri, A
	MOV	A, ES:kReadyHead[B]
	MOV	C, A
?scheduleFound:
	MOV	B, ES:!kRunTask
	MOV	A, B
	CMP	A, #0FFH
	MOV	kTemp, A
	BZ	$?switchTo
	SUB	A, C
	MOV	kTemp, A
	MOVW	DE, kResult
	SKNZ
	BR	$!?sameTask
	POP	AX			; the running task's context saved on its stack
	MOVW	SP, AX
	PUSH	DE
	MOV	A, ES:taskState[B]
	PUSH	AX
	MOVW	DE, regVar
	PUSH	DE
	MOVW	DE, regVar+2
	PUSH	DE
	MOVW	DE, regVar+4
	PUSH	DE
	MOVW	DE, regVar+6
	PUSH	DE
	MOVW	DE, regVar+8
	PUSH	DE
	MOVW	DE, regVar+10
	PUSH	DE
	MOVW	DE, regVar+12
	PUSH	DE
	MOVW	DE, regVar+14
	PUSH	DE
	SHL	B, 1
	MOVW	AX, SP
	MOVW	ES:taskSp[B], AX
?switchTo:
	CLRB	B
	MOV	ES, #0
	MOVW	AX, ES:!KERNEL_SP
	MOVW	SP, AX
	MOV	ES, #0FH
	MOV	A, C
	MOV	ES:!kRunTask, A
	CMP	A, #0FFH
	SKNZ
	BR	$!?noTask
	MOVW	AX, #kHooks
	MOVW	DE, AX
	MOV	A, ES:[DE+16H]
	MOV	CS, A
	MOVW	AX, ES:[DE+14H]
	MOVW	DE, AX
	MOVW	AX, BC
	CALL	DE
	PUSH	BC
	SHL	C, 1
	MOVW	AX, ES:taskSp[C]
	CMPW	AX, #0
	POP	BC
	SKNZ
	BR	$!_taskStart
	MOVW	SP, AX			; its context back
	POP	AX
	MOVW	regVar+14, AX
	POP	AX
	MOVW	regVar+12, AX
	POP	AX
	MOVW	regVar+10, AX
	POP	AX
	MOVW	regVar+8, AX
	POP	AX
	MOVW	regVar+6, AX
	POP	AX
	MOVW	regVar+4, AX
	POP	AX
	MOVW	regVar+2, AX
	POP	AX
	MOVW	regVar, AX
	POP	AX
	AND	A, #20H
	CMP0	A
	POP	DE
	BNZ	$?resumeInt
?sameTask:
	MOVW	AX, DE
	CMPW	AX, #0FFFFH
	BNZ	$?sameTask1
	PUSH	BC
	SHL	C, 1
	MOVW	AX, ES:taskWaitObj[C]
	POP	BC
?sameTask1:
	MOVW	kResult, AX
	MOV	A, ES:taskState[C]
	MOV	X, A
	AND	A, #20H
	CMP0	A
	BNZ	$?resumeInt
	CMP0	kTemp
	BNZ	$?resume1
	MOVW	AX, [SP+0]
	MOVW	DE, AX
	BR	$?resume2
?resume1:
	MOVW	DE, !0FFF8H
	MOV	ES, #0
	MOVW	AX, ES:!KERNEL_SP
	MOV	ES, #0FH
	MOVW	SP, AX
	PUSH	DE
?resume2:
	MOV	A, [DE+3]
	MOV	X, A
	AND	A, #6
	SKZ
	XOR	A, #2
	XOR	A, X
	MOV	PSW, A
	MOVW	HL, kResult
	BR	$!_serviceExit
?resumeInt:
	MOV	A, ES:taskState[C]
	AND	A, #0DFH
	MOV	ES:taskState[C], A
	MOV	ES:!kNest, #0
	CMP0	kTemp
	BNZ	$?resumeInt1
	MOV	ES, #0
	MOVW	DE, ES:!KERNEL_SP
	MOV	ES, #0FH
	DECW	DE
	DECW	DE
	MOVW	AX, [DE]
	MOVW	SP, AX
?resumeInt1:
	MOV	A, [SP+19H]
	MOV	X, A
	AND	A, #6
	SKZ
	XOR	A, #2
	XOR	A, X
	MOV	PSW, A
	BR	$!_restoreRegs
; nothing ready: the idle loop, standby between interrupts
?noTask:
	MOVW	AX, #kHooks
	MOVW	DE, AX
	MOV	A, ES:[DE+16H]
	MOV	CS, A
	MOVW	AX, ES:[DE+14H]
	MOVW	DE, AX
	MOVW	AX, #0FFFFH
	CALL	DE
	MOV	ES:!kTopPri, #0FFH
	MOV	ES:!kNest, #0
?idle:
	MOV	A, PSW
	XCH	A, X
	MOV	A, #6
	OR	A, X
	AND	A, #0D7H
	MOV	PSW, A
	NOP
	NOP
	EI
	MOV	ES, #0FH
	MOV	A, ES:!kFlags
	OR	A, #80H
	MOV	ES:!kFlags, A
	CALL	!!_standby
	BR	$?idle

; ER set_flg(ID flgid, FLGPTN setptn): the waiting task released when its pattern matches
_iset_flg:
	MOVW	BC, #0FF8AH
	BR	$?setFlg
_set_flg:
	MOVW	BC, #0FFD5H
?setFlg:
	PUSH	PSW
	PUSH	HL
	PUSH	AX
	CPU_LOCK
	POP	AX
	CALL	$!_serviceEnter
	MOVW	BC, AX
	MOVW	kResult, AX
	MOVW	AX, [HL+0CH]
	MOVW	kTemp, AX
	SHLW	BC, 1
	MOVW	AX, ES:flgPattern[BC]
	OR	A, kTemp+1
	XCH	A, X
	OR	A, kTemp
	XCH	A, X
	MOVW	ES:flgPattern[BC], AX
	MOVW	DE, AX
	MOVW	BC, kResult
	MOV	A, ES:flgWaitTask[BC]
	CMP	A, #0FFH
	BZ	$?setFlgDone
	MOV	C, A
	MOV	kTimeout, A
	MOV	A, ES:taskWait[BC]
	AND	A, #1
	SHLW	BC, 1
	MOVW	AX, ES:taskWaitPtn[BC]
	BZ	$?setFlgAnd
	AND	A, D
	XCH	A, X
	AND	A, E
	OR	A, X
	BNZ	$?setFlgRelease
?setFlgDone:
	MOVW	HL, #0
	BR	$!_serviceExit
?setFlgAnd:
	PUSH	DE
	AND	D, A
	XCH	A, X
	AND	E, A
	XCH	A, X
	CMPW	AX, DE
	POP	DE
	BNZ	$?setFlgDone
?setFlgRelease:
	MOVW	BC, kResult
	MOV	ES:flgWaitTask[BC], #0FFH
	MOV	ES, #0
	MOV	A, ES:FLG_ATTR[BC]
	AND	A, #4
	MOV	ES, #0FH
	BZ	$?setFlgKeep
	SHLW	BC, 1
	CLRW	AX
	MOVW	ES:flgPattern[BC], AX
?setFlgKeep:
	MOV	C, kTimeout
	MOV	A, ES:taskWait[BC]
	OR	A, #40H
	AND	A, #0FEH
	MOV	ES:taskWait[BC], A
	MOV	A, ES:taskState[BC]
	AND	A, #0E0H
	MOV	ES:taskState[BC], A
	MOV	kResult, A
	SHLW	BC, 1
	CLRW	AX
	MOVW	ES:taskWaitObj[BC], AX
	SHLW	BC, 1
	MOVW	AX, ES:taskWaitPkt[BC]
	MOVW	HL, AX
	MOV	A, ES:taskWaitPkt+2[BC]
	MOV	ES, A
	MOVW	AX, DE
	MOVW	ES:[HL], AX
	MOV	ES, #0FH
	MOV	C, kTimeout
	MOV	A, kResult
	MOVW	kResult, #0
	BR	$!_readyAndDispatch

; ER snd_mbx(ID mbxid, T_MSG *pk_msg): to a waiting task, else queued (by priority in a
; prioritised mailbox)
_snd_mbx:
	PUSH	PSW
	PUSH	HL
	PUSH	AX
	CPU_LOCK
	POP	AX
	MOVW	BC, #0FFC1H
	CALL	$!_serviceEnter
	MOVW	BC, AX
	MOVW	AX, [HL+6]
	PUSH	AX
	SHLW	BC, 2
	MOVW	AX, ES:mbxHeadSeg[BC]
	CMP	A, #0FFH
	BNZ	$?sndMbxWait
	MOV	A, X
	CMP	A, #0FFH
	SKZ
	BR	$!?sndMbxQueue
	MOV	A, [HL+0EH]
	MOV	ES, A
	MOVW	AX, [HL+0CH]
	MOVW	DE, AX
	MOV	A, #0FFH
	MOV	ES:[DE+2], A
	MOV	A, ES
?sndMbxFirst:
	MOV	X, A
	MOV	A, #0FFH
	MOV	ES, #0FH
	MOVW	ES:mbxHeadSeg[BC], AX
	MOVW	ES:mbxTailSeg[BC], AX
	MOVW	AX, DE
	MOVW	ES:mbxHead[BC], AX
	MOVW	ES:mbxTail[BC], AX
	POP	AX
	MOVW	HL, #0
	BR	$!_serviceExit
?sndMbxWait:
	MOV	X, A
	CLRB	A
	XCHW	AX, BC
	ADDW	AX, #mbxWait
	MOVW	DE, AX
	PUSH	BC
	CALL	$!_queueRemove
	POP	BC
	PUSH	BC
	MOV	A, ES:taskState[BC]
	AND	A, #0E0H
	MOV	ES:taskState[BC], A
	PUSH	AX
	MOV	A, ES:taskWait[BC]
	OR	A, #40H
	MOV	ES:taskWait[BC], A
	SHLW	BC, 1
	CLRW	AX
	MOVW	ES:taskWaitObj[BC], AX
	SHLW	BC, 1
	MOVW	AX, ES:taskWaitPkt[BC]
	MOVW	DE, AX
	MOV	A, ES:taskWaitPkt+2[BC]
	MOV	ES, A
	MOV	A, [HL+0EH]
	MOV	ES:[DE+2], A
	MOVW	AX, [HL+0CH]
	MOVW	ES:[DE+0], AX
	MOV	ES, #0FH
	POP	AX
	POP	BC
	POP	DE
	MOVW	kResult, #0
	BR	$!_readyAndDispatch
?sndMbxQueue:
	MOVW	AX, [HL+0CH]
	MOVW	DE, AX
	MOVW	AX, BC
	SHRW	AX, 2
	MOV	ES, #0
	MOV	A, ES:!MBXPRI_COUNT
	CMP	A, X
	BNC	$?sndMbxPri
	MOV	A, [HL+0EH]		; at the end
	MOV	ES, A
	MOV	kResult, A
	MOV	A, #0FFH
	MOV	ES:[DE+2], A
	MOV	ES, #0FH
	MOVW	AX, ES:mbxTail[BC]
	MOVW	HL, AX
	MOV	A, ES:mbxTailSeg[BC]
	MOV	ES, A
	MOVW	AX, DE
	MOVW	ES:[HL+0], AX
	MOV	A, kResult
	MOV	ES:[HL+2], A
	MOV	ES, #0FH
	MOV	ES:mbxTailSeg[BC], A
	MOVW	AX, DE
	MOVW	ES:mbxTail[BC], AX
	POP	AX
	MOVW	HL, #0
	BR	$!_serviceExit
?sndMbxPri:
	PUSH	HL			; before the first message of a lower priority
	MOV	A, [HL+0EH]
	MOV	ES, A
	MOV	A, ES:[DE+4]
	XCHW	AX, BC
	ADDW	AX, #mbxHead
	MOVW	DE, AX
	MOV	ES, #0FH
	MOV	A, ES
	MOV	kResult, A
	BR	$?sndMbxScan
?sndMbxRestart:
	MOVW	AX, BC
	CLRB	A
	SHLW	AX, 2
	ADDW	AX, #mbxHead
	MOVW	DE, AX
	MOV	ES, #0FH
	MOV	A, ES
	MOV	kResult, A
	MOV	A, ES:[DE+2]
	CMP	A, #0FFH
	BNZ	$?sndMbxScan
	POP	HL
	CLRB	B
	SHLW	BC, 2
	MOVW	AX, [HL+0CH]
	MOVW	DE, AX
	MOV	A, [HL+0EH]
	MOV	ES, A
	MOV	A, #0FFH
	MOV	ES:[DE+2], A
	MOV	A, ES
	BR	$!?sndMbxFirst
?sndMbxScan:
	MOVW	AX, ES:[DE+0]
	MOVW	HL, AX
	MOV	A, ES:[DE+2]
	MOV	kResult+1, A
	BR	$?sndMbxCmp
?sndMbxLoop:
	MOV	A, ES			; interrupts let in between the messages
	MOV	X, A
	MOV	ES, #0FH
	MOV	A, ES:mbxMsgCount[C]
	PUSH	AX
	MOVW	AX, [SP+4]
	NOP
	NOP
	MOV	PSW, A
	AND	A, #0FBH
	OR	A, #2
	MOV	PSW, A
	MOV	A, ES:!kFlags
	AND	A, #7FH
	MOV	ES:!kFlags, A
	POP	AX
	MOVW	kResult, AX
	MOV	A, ES:mbxMsgCount[C]
	CMP	A, kResult+1
	BNZ	$?sndMbxRestart
	MOV	ES, kResult
	MOVW	AX, HL
	MOVW	DE, AX
	MOV	A, ES:[DE+2]
	MOV	kResult+1, A
	CMP	A, #0FFH
	SKNZ
	BR	$!?sndMbxTail
	MOVW	AX, ES:[DE+0]
	MOVW	HL, AX
?sndMbxCmp:
	MOV	ES, kResult+1
	MOV	A, ES:[HL+4]
	CMP	A, B
	BNH	$?sndMbxLoop
	MOVW	AX, HL
	MOVW	BC, AX
	POP	HL
	MOV	A, [HL+0EH]
	MOV	ES, A
	MOVW	AX, [HL+0CH]
	MOVW	HL, AX
	MOVW	AX, BC
	MOVW	ES:[HL+0], AX
	MOV	A, kResult+1
	MOV	ES:[HL+2], A
	MOV	A, ES
	MOV	ES, kResult
	MOV	ES:[DE+2], A
	MOVW	AX, HL
	MOVW	ES:[DE+0], AX
	BR	$!?sndMbxDone
?sndMbxTail:
	POP	HL
	MOV	A, [HL+0EH]
	MOV	ES, A
	MOVW	AX, [HL+0CH]
	MOVW	HL, AX
	MOV	A, #0FFH
	MOV	ES:[HL+2], A
	MOV	A, ES
	MOV	kResult+1, A
	MOV	ES, kResult
	MOV	ES:[DE+2], A
	MOVW	AX, HL
	MOVW	ES:[DE+0], AX
	CLRB	B
	SHLW	BC, 2
	MOV	ES, #0FH
	MOV	A, kResult+1
	MOV	ES:mbxTailSeg[BC], A
	MOVW	AX, HL
	MOVW	ES:mbxTail[BC], AX
?sndMbxDone:
	POP	AX
	MOVW	kResult, #0
	MOV	ES, #0FH
	BR	$!_dispatch

; ER sta_cyc(ID cycid)
_sta_cyc:
	PUSH	PSW
	PUSH	HL
	PUSH	AX
	CPU_LOCK
	POP	AX
	MOVW	BC, #0FFAFH
	CALL	$!_serviceEnter
	MOVW	BC, AX
	MOV	ES:taskState[BC], #1
	PUSH	BC
	MOV	ES, #0
	MOV	A, ES:!TASK_COUNT
	SUB	C, A
	CLRB	B
	SHLW	BC, 2
	MOVW	AX, ES:CYC_TIME+2[BC]
	MOVW	kTimeout+2, AX
	MOVW	AX, ES:CYC_TIME[BC]
	MOVW	kTimeout, AX
	MOV	ES, #0FH
	POP	BC
	CALL	$!_timerInsert
	MOVW	HL, #0
	BR	$!_serviceExit

; a service's start: AX its object; the caller's stack (HL) with its arguments, the trace
; hook told, kResult the caller's packet of stack arguments
_serviceEnter:
	MOV	ES, #0FH
	MOVW	DE, AX
	MOV	A, ES:!kFlags
	AND	A, #7FH
	MOV	ES:!kFlags, A
	MOVW	HL, !0FFF8H
	MOV	A, ES:!kIntNest
	CMP0	A
	BNZ	$?serviceEnter1
	MOV	ES, #0
	MOVW	AX, ES:!KERNEL_SP
	MOVW	SP, AX
	MOV	ES, #0FH
	MOVW	AX, HL
	ADDW	AX, #4
	PUSH	AX
	MOVW	AX, [HL+2]
	PUSH	AX
	MOVW	AX, [HL]
	PUSH	AX
?serviceEnter1:
	PUSH	DE
	INC	ES:!kNest
	MOVW	AX, #kHooks
	MOVW	DE, AX
	MOV	A, ES:[DE+2]
	MOV	CS, A
	MOVW	AX, ES:[DE+0]
	MOVW	DE, AX
	MOVW	AX, HL
	ADDW	AX, #8
	MOVW	kResult, AX
	MOVW	AX, [SP+0]
	CALL	DE
	POP	AX
	RET

; a service's end: HL the error code, back to the caller's stack
_serviceExit:
	MOVW	AX, #kHooks
	MOVW	DE, AX
	MOV	A, ES:[DE+6]
	MOV	CS, A
	MOVW	AX, ES:[DE+4]
	MOVW	DE, AX
	CALL	DE
	MOV	A, ES:!kIntNest
	CMP0	A
	BNZ	$?exit1
	POP	AX
	MOVW	SP, AX
?exit1:
	MOVW	AX, HL
	MOVW	BC, AX
	POP	HL
	DEC	ES:!kNest
	MOV	A, ES:!kFlags
	OR	A, #80H
	MOV	ES:!kFlags, A
	NOP
	NOP
	POP	PSW
	RET

; task C activated: readied when dormant, else its activation queued (E_QOVR past 0x7f)
_taskActivate:
	MOV	A, ES:taskNext[C]
	CMP	A, #0FFH
	BNZ	$?activateQueue
	MOV	A, ES:taskState[C]
	MOVW	kResult, #0
	BR	$!_makeReady
?activateQueue:
	MOV	A, ES:taskActCnt[C]
	CMP	A, #7FH
	BZ	$?activateOver
	INC	A
	MOV	ES:taskActCnt[C], A
	MOVW	kResult, #0
	RET
?activateOver:
	MOVW	kResult, #0FFD5H	; E_QOVR
	RET

; task C's first run: its stack from the configuration, its exinf in AX:BC, interrupts
; enabled unless its attributes say so
_taskStart:
	MOVW	AX, #kHooks
	MOVW	DE, AX
	MOV	A, ES:[DE+12H]
	MOV	CS, A
	MOVW	AX, ES:[DE+10H]
	MOVW	DE, AX
	MOVW	AX, BC
	CALL	DE
	MOVW	BC, AX
	MOV	ES, #0
	SHLW	BC, 1
	MOVW	AX, ES:TASK_STACK[BC]
	SUBW	AX, #8
	MOVW	SP, AX
	SHLW	BC, 1
	MOV	A, ES:TASK_ATTR[BC]
	MOV	X, A
	MOV	ES, #0FH
	MOV	A, ES:!kFlags
	OR	A, #80H
	MOV	ES:!kFlags, A
	MOV	ES:!kNest, #0
	MOV	A, X
	AND	A, #10H
	BNZ	$?taskStartDi
	MOV	A, PSW
	XCH	A, X
	MOV	A, #6
	OR	A, X
	AND	A, #0D7H
	MOV	PSW, A
	NOP
	NOP
	EI
	BR	$?taskStartGo
?taskStartDi:
	MOV	A, PSW
	XCH	A, X
	MOV	A, #6
	OR	A, X
	AND	A, #0D7H
	MOV	PSW, A
	DI
?taskStartGo:
	POP	BC
	POP	AX
	RET

; task BC's control block made dormant, its first frame (exinf, entry and PSW) laid on its
; stack; AX its attributes
_taskInit:
	MOV	ES:taskWait[BC], #0
	MOV	ES:taskActCnt[BC], #0
?taskReset:
	MOVW	AX, BC
	MOVW	HL, AX
	CLRW	AX
	MOV	ES:taskState[BC], A
	MOV	ES:taskSusCnt[BC], A
	MOV	ES:taskWupCnt[BC], A
	MOV	ES:taskNext[BC], #0FFH
	SHLW	BC, 1
	MOVW	ES:taskWaitObj[BC], AX
	MOVW	ES:taskSp[BC], AX
	MOV	ES, #0
	MOVW	AX, ES:TASK_STACK[BC]
	SUBW	AX, #8
	MOVW	DE, AX
	SHLW	BC, 1
	MOVW	AX, ES:TASK_ATTR[BC]
	SHL	A, 1
	SHR	A, 3
	XCHW	AX, HL
	ADDW	AX, #taskPri
	XCHW	AX, HL
	MOV	ES, #0FH
	MOV	ES:[HL], A
	MOV	ES, #0
	MOVW	AX, ES:TASK_ATTR[BC]
	MOVW	HL, AX
	SHLW	AX, 12
	SHRW	AX, 12
	MOVW	[DE+6], AX
	MOVW	AX, ES:TASK_ENTRY[BC]
	MOVW	[DE+4], AX
	MOVW	AX, ES:TASK_EXINF[BC]
	MOVW	[DE+2], AX
	MOVW	AX, ES:TASK_EXINF+2[BC]
	MOVW	[DE+0], AX
	MOVW	AX, HL
	MOV	ES, #0FH
	RET

; ER isig_tim(void): the tick. The tasks whose timeouts ran out are released, the cyclic
; handlers due are run and requeued; a tick that comes while one is handled is counted
_isig_tim:
	PUSH	PSW
	MOV	A, PSW
	AND	A, #0FBH
	OR	A, #2
	MOV	PSW, A
	MOV	ES, #0FH
	MOV	A, ES:!kTick
	INC	A
	MOV	L, A
	AND	A, #80H
	BNZ	$?tickBusy
	MOV	ES:!kTick, #80H
	BR	$?tick
?tickBusy:
	MOV	A, L
	MOV	ES:!kTick, A
	POP	PSW
	RET
?tick:
	MOV	H, #0
; the ticks counted off the first entry of the queue the last tick left
?tickNext:
	MOVW	AX, [SP+0]
	NOP
	NOP
	MOV	PSW, A
	AND	A, #0FBH
	OR	A, #2
	MOV	PSW, A
	MOV	ES, #0FH
	MOV	A, ES:!kFlags
	AND	A, #7FH
	MOV	ES:!kFlags, A
	MOV	A, ES:!kTimerHead+1
	CMP	A, #0FFH
	SKNZ
	BR	$!?tickQueue
	XCH	A, X
	CLRB	A
	MOVW	BC, AX
	MOVW	kResult, AX
	SHLW	BC, 2
	MOVW	AX, ES:taskTmo+2[BC]
	MOVW	DE, AX
	OR	A, X
	SKZ
	BR	$!?tickDec
	MOVW	AX, ES:taskTmo[BC]
	CMPW	AX, HL
	SKNH
	BR	$!?tickSub
	XCHW	AX, HL
	SUBW	AX, HL
	MOVW	HL, AX
	MOV	C, kResult
	MOV	A, ES:taskTmoNext[C]
	CMP	A, C
	BZ	$?tickLast
	MOV	B, A
	MOV	A, ES:taskTmoPrev[C]
	MOV	ES:taskTmoPrev[B], A
	XCH	A, B
	MOV	ES:taskTmoNext[B], A
	MOV	ES:!kTimerHead+1, A
	SKNZ
?tickLast:
	MOV	ES:!kTimerHead+1, #0FFH
	MOV	A, ES:taskWait[C]
	MOV	kTemp, A
	AND	A, #8FH
	MOV	ES:taskWait[C], A
	AND	kTemp, #40H
	BNZ	$?tickNext
	MOV	ES, #0
	MOV	A, ES:!TASK_COUNT
	CMP	A, C
	BC	$?tickCyc
	MOV	ES, #0FH		; a task: E_TMOUT, but E_OK from dly_tsk
	MOV	A, ES:taskState[C]
	MOV	kTemp, A
	MOVW	BC, kResult
	CALL	$!_waitRelease
	MOVW	BC, kResult
	MOV	A, kTemp
	AND	A, #1FH
	CMP	A, #2
	CLRW	AX
	SKZ
	MOVW	AX, #0FFCEH
	SHL	C, 1
	MOVW	ES:taskWaitObj[C], AX
	MOVW	BC, kResult
	MOV	A, kTemp
	AND	A, #0E0H
	MOV	ES:taskState[C], A
	PUSH	HL
	CALL	$!_makeReady
	POP	HL
	BR	$!?tickNext
?tickCyc:
	MOV	X, A			; a cyclic handler: requeued, then run
	MOV	ES, #0FH
	MOV	A, ES:taskState[C]
	AND	A, #1
	MOV	A, X
	SKNZ
	BR	$!?tickNext
	SUB	C, A
	CLRB	B
	MOV	ES, #0
	SHLW	BC, 2
	MOVW	AX, ES:CYC_TIME[BC]
	MOVW	kTimeout, AX
	MOVW	AX, ES:CYC_TIME+2[BC]
	MOVW	kTimeout+2, AX
	MOV	ES, #0FH
	MOVW	AX, BC
	MOVW	kTemp, AX
	MOVW	BC, kResult
	PUSH	HL
	CALL	$!_timerInsert
	POP	HL
	MOVW	BC, kTemp
	MOV	A, ES:!kFlags
	OR	A, #80H
	MOV	ES:!kFlags, A
	MOV	ES, #0
	MOV	A, ES:CYC_SEG[BC]
	MOV	CS, A
	MOVW	AX, ES:CYC_HANDLER[BC]
	CALL	AX
	MOV	ES, #0FH
	MOV	A, ES:!kFlags
	AND	A, #7FH
	MOV	ES:!kFlags, A
	BR	$!?tickNext
?tickDone:
	MOV	A, ES:!kTick
	AND	A, #7FH
	MOV	ES:!kTick, A
	POP	PSW
	RET
?tickDec:
	MOVW	AX, ES:taskTmo[BC]
?tickSub:
	SUBW	AX, HL
	MOVW	ES:taskTmo[BC], AX
	BNC	$?tickQueue
	MOVW	AX, DE
	DECW	AX
	MOVW	ES:taskTmo+2[BC], AX
; the timeout queue moved, entry by entry, into the next tick's queue in order
?tickQueue:
	MOVW	AX, [SP+0]
	NOP
	NOP
	MOV	PSW, A
	AND	A, #0FBH
	OR	A, #2
	MOV	PSW, A
	MOV	ES, #0FH
	MOV	A, ES:!kFlags
	AND	A, #7FH
	MOV	ES:!kFlags, A
	MOV	A, ES:!kTimerHead
	CMP	A, #0FFH
	BZ	$?tickDone
	MOV	X, A
	CLRB	A
	MOVW	BC, AX
	MOVW	kResult, AX
	MOV	A, ES:taskTmoNext[C]
	CMP	A, C
	BZ	$?queueLast
	MOV	B, A
	MOV	A, ES:taskTmoPrev[C]
	MOV	ES:taskTmoPrev[B], A
	XCH	A, B
	MOV	ES:taskTmoNext[B], A
	MOV	ES:!kTimerHead, A
	SKNZ
?queueLast:
	MOV	ES:!kTimerHead, #0FFH
	MOV	A, ES:taskWait[C]
	MOV	kTemp, A
	AND	A, #8FH
	MOV	ES:taskWait[C], A
	AND	kTemp, #40H
	BNZ	$?tickQueue
	CLRB	B
	SHLW	BC, 2
	MOVW	AX, ES:taskTmo+2[BC]
	MOVW	DE, AX
	OR	A, X
	MOVW	AX, ES:taskTmo[BC]
	MOVW	HL, AX
	BNZ	$?queueTask
	OR	A, X
	BNZ	$?queueTask
	MOV	C, kResult		; already out: released
	CLRW	AX
	SHL	C, 1
	MOVW	ES:taskWaitObj[C], AX
	MOV	C, kResult
	MOV	A, ES:taskState[C]
	AND	A, #0E0H
	MOV	ES:taskState[C], A
	CALL	$!_makeReady
	BR	$!?tickQueue
?queueRescan:
	MOV	A, C
	MOV	kResult, A
	CLRB	B
	SHLW	BC, 2
	MOVW	AX, ES:taskTmo+2[BC]
	MOVW	DE, AX
	MOVW	AX, ES:taskTmo[BC]
	MOVW	HL, AX
?queueTask:
	MOV	A, ES:!kTimerHead+1
	CMP	A, #0FFH
	BNZ	$?queueInsert
	MOV	C, kResult
	MOV	A, C
	MOV	ES:taskTmoNext[C], A
	MOV	ES:taskTmoPrev[C], A
	MOV	ES:!kTimerHead+1, A
	MOV	A, ES:taskWait[C]
	OR	A, #20H
	MOV	ES:taskWait[C], A
	BR	$!?tickQueue
?queueInsert:
	MOV	X, A
	MOV	A, ES:!kFlags
	OR	A, #20H
	MOV	ES:!kFlags, A
	MOV	A, X
?queueScan:
	CLRW	BC
	MOV	C, A
	SHLW	BC, 2
	MOV	kResult+1, A
	MOVW	AX, ES:taskTmo+2[BC]
	CMPW	AX, DE
	SKNH
	BR	$!?queueBefore
	BC	$?queueAfterHigh
	MOVW	AX, ES:taskTmo[BC]
	CMPW	AX, HL
	SKNH
	BR	$!?queueBefore2
	XCHW	AX, HL
	SUBW	AX, HL
	MOVW	HL, AX
	CLRW	AX
	MOVW	DE, AX
	BR	$?queueScanNext
?queueAfterHigh:
	XCHW	AX, DE
	SUBW	AX, DE
	MOVW	DE, AX
	MOVW	AX, ES:taskTmo[BC]
	XCHW	AX, HL
	SUBW	AX, HL
	MOVW	HL, AX
	SKNC
	DECW	DE
?queueScanNext:
	MOVW	BC, kResult
	MOVW	AX, [SP+0]
	NOP
	NOP
	MOV	PSW, A
	AND	A, #0FBH
	OR	A, #2
	MOV	PSW, A
	MOV	ES, #0FH
	MOV	A, ES:!kFlags
	AND	A, #7FH
	MOV	ES:!kFlags, A
	MOVW	AX, BC
	MOVW	kResult, AX
	MOV	A, ES:!kFlags
	AND	A, #20H
	SKNZ
	BR	$!?queueRescan
	MOV	X, ES:!kTimerHead+1
	MOV	A, ES:taskTmoNext[B]
	CMP	A, X
	BNZ	$?queueScan
	MOV	A, ES:taskWait[C]	; at the end
	MOV	kTemp, A
	AND	A, #50H
	SKZ
	BR	$!?queueEnd
	MOV	A, kTemp
	OR	A, #20H
	MOV	ES:taskWait[C], A
	MOV	A, X
	MOV	ES:taskTmoNext[C], A
	XCH	A, B
	MOV	ES:taskTmoPrev[C], A
	XCH	A, C
	MOV	ES:taskTmoNext[C], A
	MOV	ES:taskTmoPrev[B], A
	CLRW	BC
	MOV	C, A
	SHLW	BC, 2
	MOVW	AX, DE
	MOVW	ES:taskTmo+2[BC], AX
	MOVW	AX, HL
	MOVW	ES:taskTmo[BC], AX
	BR	$!?queueEnd
?queueBefore:
	SUBW	AX, DE
	MOVW	kTemp, AX
	PUSH	BC
	MOV	C, kResult
	MOV	A, ES:taskWait[C]
	AND	A, #50H
	POP	BC
	BNZ	$?queueEnd
	MOVW	AX, ES:taskTmo[BC]
	SUBW	AX, HL
	MOVW	ES:taskTmo[BC], AX
	MOVW	AX, kTemp
	SKNC
	DECW	AX
	MOVW	ES:taskTmo+2[BC], AX
	BR	$?queueLink
?queueBefore2:
	PUSH	AX
	PUSH	BC
	MOV	C, kResult
	MOV	A, ES:taskWait[C]
	AND	A, #50H
	POP	BC
	POP	AX
	BNZ	$?queueEnd
	SUBW	AX, HL
	MOVW	ES:taskTmo[BC], AX
	CLRW	AX
	MOVW	ES:taskTmo+2[BC], AX
?queueLink:
	CLRW	BC
	MOV	C, kResult
	SHLW	BC, 2
	MOVW	AX, DE
	MOVW	ES:taskTmo+2[BC], AX
	MOVW	AX, HL
	MOVW	ES:taskTmo[BC], AX
	MOVW	BC, kResult
	MOV	A, ES:!kTimerHead+1
	CMP	A, B
	MOV	A, B
	MOV	ES:taskTmoNext[C], A
	MOV	A, ES:taskTmoPrev[B]
	MOV	ES:taskTmoPrev[C], A
	XCH	A, C
	MOV	ES:taskTmoNext[C], A
	MOV	ES:taskTmoPrev[B], A
	SKNZ
	MOV	ES:!kTimerHead+1, A
	MOV	C, A
	MOV	A, ES:taskWait[C]
	OR	A, #20H
	MOV	ES:taskWait[C], A
?queueEnd:
	MOV	A, ES:!kFlags
	AND	A, #0DFH
	MOV	ES:!kFlags, A
	BR	$!?tickQueue

; the trace hooks: each writes its event (and the service's caller) to kTrace
_traceServiceEnter:
	PUSH	AX
	MOV	A, #1
	XCH	A, X
	MOV	A, C
	MOVW	ES:!kTrace, AX
	POP	AX
	MOV	A, #3
	XCH	A, X
	MOVW	ES:!kTrace, AX
	MOVW	DE, kResult
	MOV	A, [DE+2]
	MOV	X, A
	PUSH	AX
	MOVW	AX, [DE+0]
	SUBW	AX, #4
	MOVW	DE, AX
	AND	A, #0F0H
	MOV	B, A
	POP	AX
	MOV	A, B
	XCH	A, X
	MOVW	ES:!kTrace, AX
	MOVW	AX, DE
	SHLW	AX, 4
	MOVW	ES:!kTrace, AX
	RET

_traceServiceExit:
	MOV	A, #5
	XCH	A, X
	MOV	A, H
	MOVW	ES:!kTrace, AX
	MOV	A, #7
	XCH	A, X
	MOV	A, L
	MOVW	ES:!kTrace, AX
	RET

_traceInterruptEnter:
	CLRW	AX
	MOV	A, #9
	XCH	A, X
	MOVW	ES:!kTrace, AX
	MOV	A, kResult+1
	AND	A, #0F0H
	MOV	X, A
	MOV	A, kTemp
	MOVW	ES:!kTrace, AX
	MOVW	AX, kResult
	SHLW	AX, 4
	MOVW	ES:!kTrace, AX
	RET

_traceInterruptExit:
	CLRW	AX
	MOV	A, #0BH
	XCH	A, X
	MOVW	ES:!kTrace, AX
	RET

_traceTaskStart:
	PUSH	AX
	MOVW	BC, AX
	MOV	A, #0DH
	XCH	A, X
	MOVW	ES:!kTrace, AX
	MOV	ES, #0
	SHLW	BC, 2
	MOV	A, ES:TASK_ATTR[BC]
	AND	A, #0FH
	XCH	A, X
	PUSH	AX
	MOVW	AX, ES:TASK_ENTRY[BC]
	MOVW	DE, AX
	AND	A, #0F0H
	MOV	B, A
	POP	AX
	MOV	A, B
	XCH	A, X
	MOV	ES, #0FH
	MOVW	ES:!kTrace, AX
	MOVW	AX, DE
	SHLW	AX, 4
	MOVW	ES:!kTrace, AX
	POP	AX
	RET

_traceDispatch:
	MOV	A, #0FH
	XCH	A, X
	MOVW	ES:!kTrace, AX
	RET

; task BC's wait (A, its state) ended by its timeout: off its object's wait queue
_waitRelease:
	AND	A, #1FH
	DEC	A
	MOV	X, A
	CLRB	A
	SHLW	AX, 2
	ADDW	AX, #LOWW(_waitReleaseTable)
	MOVW	DE, AX
	MOV	ES, #0
	MOV	A, ES:[DE+2]
	MOV	CS, A
	MOVW	AX, ES:[DE+0]
	MOV	ES, #0FH
	BR	AX

; by wait state, from 1: what takes a task off that state's wait queue
_waitReleaseTable:
	DW	LOWW(?waitNone), 0	; 1 sleep
	DW	LOWW(?waitNone), 0	; 2 dly_tsk
	DW	LOWW(_semWaitCancel), 0
	DW	LOWW(_flgWaitCancel), 0
	DW	0, 0
	DW	0, 0
	DW	LOWW(_mbxWaitCancel), 0
	DW	0, 0
	DW	0, 0
	DW	0, 0
	DW	0, 0
	DW	0, 0
	DW	0, 0
	DW	LOWW(_mpfWaitCancel), 0
	DW	0, 0
?waitNone:
	RET

_flgWaitCancel:
	SHL	C, 1
	MOVW	AX, ES:taskWaitObj[C]
	CLRB	A
	ADDW	AX, #flgWaitTask
	MOVW	DE, AX
	MOV	A, #0FFH
	MOV	ES:[DE+0], A
	RET

_mbxWaitCancel:
	PUSH	BC
	SHL	C, 1
	MOVW	AX, ES:taskWaitObj[C]
	CLRB	A
	SHLW	AX, 2
	ADDW	AX, #mbxWait
	MOVW	DE, AX
	POP	BC
	BR	$!_queueRemove

_mpfWaitCancel:
	PUSH	BC
	SHL	C, 1
	MOVW	AX, ES:taskWaitObj[C]
	CLRB	A
	SHLW	AX, 2
	ADDW	AX, #mpfWait
	MOVW	DE, AX
	POP	BC
	BR	$!_queueRemove

; task C off its ready queue (its priority's bit cleared when the queue empties)
_readyRemove:
	CLRW	AX
	MOV	A, ES:taskPri[C]
	XCH	A, X
	ADDW	AX, #kReadyHead
	MOVW	DE, AX
	PUSH	BC
	CALL	$!_queueRemove
	POP	BC
	PUSH	BC
	CMP0	A
	BNZ	$?readyRemove1
	MOV	A, ES:taskPri[C]
	MOV	D, A
	MOV	B, #1
	SUB	A, #8
	BNC	$?readyBit
	CLRB	B
	MOV	A, D
?readyBit:
	MOV	C, #1
	CMP0	A
	BZ	$?readyClear
?readyShift:
	SHL	C, 1
	DEC	A
	BNZ	$?readyShift
?readyClear:
	MOV	A, ES:kReadyMap[B]
	XOR	A, C
	MOV	ES:kReadyMap[B], A
?readyRemove1:
	MOV	A, ES:!kRunTask
	POP	BC
	CMP	A, C
	BNZ	$?readyRemoveEnd
	MOV	A, ES:!kFlags
	AND	A, #0F7H
	MOV	ES:!kFlags, A
?readyRemoveEnd:
	RET

; task C off the queue whose first task DE points to; A 0 when the queue emptied
_queueRemove:
	MOV	A, ES:taskNext[C]
	CMP	A, C
	BZ	$?queueRemoveLast
	MOV	X, A
	MOV	A, ES:[DE]
	CMP	A, C
	BNZ	$?queueRemove1
	MOV	A, X
	MOV	ES:[DE], A
?queueRemove1:
	MOV	A, ES:taskPrev[C]
	XCH	A, C
	XCH	A, X
	MOV	ES:taskNext[C], A
	XCH	A, C
	MOV	ES:taskPrev[C], A
	RET
?queueRemoveLast:
	MOV	ES:[DE+0], #0FFH
	CLRB	A
	RET

_semWaitCancel:
	PUSH	BC
	SHL	C, 1
	MOVW	AX, ES:taskWaitObj[C]
	CLRB	A
	ADDW	AX, #semWait
	MOVW	DE, AX
	POP	BC
	BR	$!_queueRemove

; task C off the timeout queue (A, its taskWait: 0x20 the next tick's queue), its count
; given to the entry after it
_timerRemove:
	MOV	ES:taskWait[C], #0
	AND	A, #20H
	CLRB	B
	SKZ
	ONEB	B
	MOV	A, ES:!kFlags
	AND	A, #0DFH
	MOV	ES:!kFlags, A
	MOV	A, ES:taskTmoNext[C]
	CMP	A, C
	BZ	$?timerRemoveLast
	MOV	X, A
	MOV	A, ES:kTimerHead[B]
	CMP	A, C
	XCH	A, X
	BNZ	$?timerRemove1
	MOV	ES:kTimerHead[B], A
	BR	$?timerRemove2
?timerRemove1:
	CMP	A, X
	BZ	$?timerUnlink
?timerRemove2:
	CMP0	B
	MOV	X, A
	BZ	$?timerUnlink
	MOV	A, C
	MOVW	DE, AX
	CLRB	A
	CLRB	B
	XCHW	AX, BC
	SHLW	AX, 2
	SHLW	BC, 2
	ADDW	AX, #taskTmo
	MOVW	HL, AX
	MOVW	AX, ES:taskTmo[BC]
	ADDW	AX, ES:[HL+0]
	MOVW	ES:taskTmo[BC], AX
	MOVW	AX, ES:taskTmo+2[BC]
	SKNC
	INCW	AX
	ADDW	AX, ES:[HL+2]
	MOVW	ES:taskTmo+2[BC], AX
	MOVW	AX, DE
	MOV	C, A
?timerUnlink:
	MOV	A, ES:taskTmoPrev[C]
	XCH	A, C
	XCH	A, X
	MOV	ES:taskTmoNext[C], A
	XCH	A, C
	MOV	ES:taskTmoPrev[C], A
	RET
?timerRemoveLast:
	MOV	ES:kTimerHead[B], #0FFH
	RET

; ER wai_flg(ID flgid, FLGPTN waiptn, MODE wfmode, FLGPTN *p_flgptn), pol_flg,
; twai_flg(..., TMO tmout)
_wai_flg:
	PUSH	PSW
	PUSH	HL
	PUSH	AX
	CPU_LOCK
	POP	AX
	MOVW	kTimeout, #0FFFFH
	MOVW	kTimeout+2, #0FFFFH
	MOVW	BC, #0FFD3H
	BR	$?waiFlg
_pol_flg:
	PUSH	PSW
	PUSH	HL
	PUSH	AX
	CPU_LOCK
	POP	AX
	MOVW	kTimeout, #0
	MOVW	kTimeout+2, #0
	MOVW	BC, #0FFD2H
	BR	$?waiFlg
_twai_flg:
	PUSH	PSW
	PUSH	HL
	PUSH	AX
	CPU_LOCK
	POP	AX
	MOVW	DE, AX
	MOVW	AX, [SP+10H]
	MOVW	kTimeout, AX
	MOVW	AX, [SP+12H]
	MOVW	kTimeout+2, AX
	MOVW	AX, DE
	MOVW	BC, #0FFD1H
?waiFlg:
	CALL	$!_serviceEnter
	MOVW	BC, AX
	MOVW	kResult, AX
	MOV	A, ES:flgWaitTask[BC]
	CMP	A, #0FFH
	BNZ	$?waiFlgBusy
	MOVW	AX, [HL+0CH]
	MOVW	kTemp, AX
	MOV	A, [HL+0EH]
	CMP	A, #1
	SHLW	BC, 1
	MOVW	AX, ES:flgPattern[BC]
	MOVW	DE, AX
	BNZ	$?waiFlgAnd
	AND	A, kTemp+1		; TWF_ORW
	XCH	A, X
	AND	A, kTemp
	OR	A, X
	BZ	$?waiFlgWait
?waiFlgMatch:
	MOV	A, [HL+12H]
	MOV	ES, A
	MOVW	AX, [HL+10H]
	MOVW	HL, AX
	MOVW	AX, DE
	MOVW	ES:[HL], AX
	MOVW	BC, kResult
	MOV	ES, #0
	MOV	A, ES:FLG_ATTR[BC]
	AND	A, #4
	MOV	ES, #0FH
	BZ	$?waiFlgKeep
	SHLW	BC, 1
	CLRW	AX
	MOVW	ES:flgPattern[BC], AX
?waiFlgKeep:
	MOVW	HL, #0
	BR	$!_serviceExit
?waiFlgBusy:
	MOVW	HL, #0FFE4H		; E_ILUSE: a task waits already
	BR	$!_serviceExit
?waiFlgAnd:
	AND	A, kTemp+1		; TWF_ANDW
	XCH	A, X
	AND	A, kTemp
	XCH	A, X
	CMPW	AX, kTemp
	BZ	$?waiFlgMatch
?waiFlgWait:
	CLRW	AX
	CMPW	AX, kTimeout+2
	BNZ	$?waiFlgWait1
	CMPW	AX, kTimeout
	BZ	$?waiFlgTimeout
?waiFlgWait1:
	MOV	C, ES:!kRunTask
	CLRB	B
	MOVW	AX, BC
	MOVW	kTemp, AX
	MOVW	AX, #0FFFFH
	CMPW	AX, kTimeout+2
	BNZ	$?waiFlgTimer
	CMPW	AX, kTimeout
	BZ	$?waiFlgForever
?waiFlgTimer:
	PUSH	HL
	CALL	$!_timerInsert
	POP	HL
	MOVW	BC, kTemp
?waiFlgForever:
	MOV	ES:taskState[BC], #4
	MOV	A, [HL+0EH]
	MOV	X, A
	MOV	A, ES:taskWait[BC]
	OR	A, X
	MOV	ES:taskWait[BC], A
	MOVW	AX, kResult
	SHLW	BC, 1
	MOVW	ES:taskWaitObj[BC], AX
	MOVW	AX, [HL+0CH]
	MOVW	ES:taskWaitPtn[BC], AX
	SHLW	BC, 1
	MOV	A, [HL+12H]
	MOV	ES:taskWaitPkt+2[BC], A
	MOVW	AX, [HL+10H]
	MOVW	ES:taskWaitPkt[BC], AX
	MOVW	BC, kTemp
	CALL	$!_readyRemove
	MOVW	BC, kResult
	MOV	A, kTemp
	MOV	ES:flgWaitTask[BC], A
	MOVW	kResult, #0FFFFH
	BR	$!_dispatch
?waiFlgTimeout:
	MOVW	HL, #0FFCEH		; E_TMOUT
	BR	$!_serviceExit

	END
