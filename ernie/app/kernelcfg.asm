; The kernel's configuration (the configurator's output): the objects' counts, the tasks'
; entries, attributes and stacks, the cyclic handler, the fixed-size pools and the
; initialisers kernelStart runs. Tables are indexed by ID from 1, so kernel.asm's base for
; each is one entry before its first. Written from the original.

	NAME	kernelcfg

	PUBLIC	_kernelConfig, _rtos_task_table, _rtos_init_table, _rtos_task_count
	EXTRN	_task1, _task2, _task3, _hostTask, _task5, _task6, _timerTick
	EXTRN	_cycInit, _flgInit, _mbxInit, _mpfInit
	EXTRN	kHooks, kMpfBlocks1, kMpfBlocks2

@@KCFG	CSEG	BASE
_kernelConfig:
$_IF (ERNIE_HW>=0x00800000||ERNIE_HW==0x00723030)
; USS-1001 and DOL-1002: the slim's kernel build
	DB	1BH, 01H, 06H, 00H, 03H, 54H, 02H, 01H	; nothing reads these
	DW	1, 1, 0, 0
$ELSE
	DB	17H, 01H, 10H, 12H, 02H, 54H, 30H, 04H	; nothing reads these
	DW	0, 1, 0, 0
$ENDIF
; the cyclic handler: its address, its CS (and TA_STA in the high byte), its period
	DW	LOWW(_timerTick), HIGHW(_timerTick)
	DW	1, 0
; the fixed-size pools: their memory (far pointers), block sizes and block counts
	DW	LOWW(kMpfBlocks1)
	DW	HIGHW(kMpfBlocks1)
	DW	LOWW(kMpfBlocks2)
	DW	HIGHW(kMpfBlocks2)
	DW	0CH, 0F0H
	DW	8, 2
; the tasks: entry, then CS (bits 0-3), priority (bits 10-14), bit 15 not activated at start
_rtos_task_table:
	DW	LOWW(_task1), 0401H
	DW	LOWW(_task2), 8801H
	DW	LOWW(_task3), 8401H
	DW	LOWW(_hostTask), 8401H
$_IF (ERNIE_FW>=0x00090502)
	DW	LOWW(_task5), 8C02H
	DW	LOWW(_task6), 8C02H
$ELSE
; 0x00090206 links task5 in the first bank
	DW	LOWW(_task5), 8C01H
	DW	LOWW(_task6), 8C02H
$ENDIF
	DW	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0	; their extended information
$_IF (ERNIE_HW>=0x00800000)
; USS-1001: the stacks below its DTC's vectors
$_IF (ERNIE_FW>=0x01050000)
; USS-1002: tasks 2 and 4 0x40 bytes more, task 5 0x80 less
	DW	0B41CH, 0B5B8H, 0B714H, 0B830H, 0B98CH, 0BA28H	; their stacks' tops
$ELSE
	DW	0B41CH, 0B5B8H, 0B6D4H, 0B7F0H, 0B90CH, 0BA28H	; their stacks' tops
$ENDIF
	DW	0BAEAH			; the kernel's stack
$ELSE
$_IF (ERNIE_HW==0x00723030)
; DOL-1002: tasks 2-6 and the kernel with larger stacks
	DW	0D3DCH, 0D516H, 0D5BAH, 0D696H, 0D76AH, 0D83EH	; their stacks' tops
	DW	0D900H			; the kernel's stack
$ELSE
$_IF (ERNIE_FW>=0x00090206)
	DW	0D3DCH, 0D4F8H, 0D59CH, 0D678H, 0D74CH, 0D828H	; their stacks' tops
	DW	0D8EAH			; the kernel's stack
$ELSE
; 0x00090009: task 4's stack 0x38 bytes smaller, the kernel's 8 bytes higher
	DW	0D41CH, 0D538H, 0D5DCH, 0D680H, 0D754H, 0D830H
	DW	0D8F2H
$ENDIF
$ENDIF
$ENDIF
	DW	LOWW(kHooks)		; the trace hooks
	DW	HIGHW(kHooks)
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
$_IF (ERNIE_FW<0x00080502)
	DB	2			; mailboxes (0x00080307 has two)
$ELSE
	DB	1			; mailboxes
$ENDIF
	DB	0			; mailboxes with prioritised messages
	DB	2			; fixed-size pools
	DB	1			; cyclic handlers
	DB	3			; priorities
	DB	2

	END
