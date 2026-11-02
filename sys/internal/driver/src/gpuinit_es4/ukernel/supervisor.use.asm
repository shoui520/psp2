/* Master: supervisor-mode program (register writes, see JumpToSupervisor). */

/* Supervisor entry: sa40 == 1 means write sa42 to register sa41. */
Supervisor:
	or r24, sa40, #0x00000000
	xor.testtaz.chan0 !c0, p0, r24, #1
	p0 ba SPRV_WriteReg
	nop
Supervisor_1:
	phas.end #0, #0, pixel, none, perinstance

/* Register sa41 = sa42. */
SPRV_WriteReg:
	or r24, sa41, #0x00000000
	or r25, sa42, #0x00000000
	str r24, r25
	ba Supervisor_1
