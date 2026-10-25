; The kernel's configuration (the configurator's output): the objects' counts, the tasks'
; entries, attributes and stacks, the cyclic handler, the fixed-size pools and the
; initialisers kernelStart runs. Tables are indexed by ID from 1, so kernel.asm's base for
; each is one entry before its first. Written from the original.

	NAME	kernelcfg

	PUBLIC	_kernelConfig, _rtos_task_table, _rtos_init_table, _rtos_task_count
	EXTRN	_task1, _task2, _task3, _hostTask, _task5, _task6, _timerTick
	EXTRN	_cycInit, _flgInit, _mbxInit, _mpfInit

@@KCFG	CSEG	BASE
_kernelConfig:
	DB	17H, 01H, 10H, 12H, 02H, 54H, 30H, 04H	; nothing reads these
	DW	0, 1, 0, 0
; the cyclic handler: its address, its CS (and TA_STA in the high byte), its period
	DW	LOWW(_timerTick), HIGHW(_timerTick)
	DW	1, 0
; the fixed-size pools: their memory (far pointers), block sizes and block counts
	DW	0DE2CH, 0FH, 0DE8CH, 0FH
	DW	0CH, 0F0H
	DW	8, 2
; the tasks: entry, then CS (bits 0-3), priority (bits 10-14), bit 15 not activated at start
_rtos_task_table:
	DW	LOWW(_task1), 0401H
	DW	LOWW(_task2), 8801H
	DW	LOWW(_task3), 8401H
	DW	LOWW(_hostTask), 8401H
	DW	LOWW(_task5), 8C02H
	DW	LOWW(_task6), 8C02H
	DW	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0	; their extended information
	DW	0D3DCH, 0D4F8H, 0D59CH, 0D678H, 0D74CH, 0D828H	; their stacks' tops
	DW	0D8EAH			; the kernel's stack
	DW	0DD56H, 0FH		; the trace hooks (kHooks)
_rtos_init_table:
	DW	LOWW(_cycInit), 0
	DW	LOWW(_flgInit), 0
	DW	LOWW(_mbxInit), 0
	DW	LOWW(_mpfInit), 0
	DW	0FFFFH, 0FFFFH
	DB	0, 0, 0, 0, 0, 0	; the event flags' attributes
_rtos_task_count:
	DB	6, 0			; tasks
	DB	6			; event flags
	DB	1			; mailboxes
	DB	0			; mailboxes with prioritised messages
	DB	2			; fixed-size pools
	DB	1			; cyclic handlers
	DB	3			; priorities
	DB	2

	END
