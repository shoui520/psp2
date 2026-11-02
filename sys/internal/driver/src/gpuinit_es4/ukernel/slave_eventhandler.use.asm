SlaveEventHandler:
	smlsi #0, #0, #0, #1, incrementmode, incrementmode, incrementmode, incrementmode, #0, #0, #0
	mov r15, #0x00000000
	mov r22, #0x00000000
	phas #0, #0, pixel, end, parallel
	or.end r0, r0, #0x00000000

JumpToSlaveSupervisor:
	mov r31, pclink
	shl r31, r31, #0x00000004
	sprvv.end
	smlsi #0, #0, #0, #1, incrementmode, incrementmode, incrementmode, incrementmode, #0, #0, #0
	shr r31, r31, #0x00000004
	mov pclink, r31
	nop
	lapc
