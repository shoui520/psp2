Supervisor:
	or r24, sa40, #0x00000000
	xor.testtaz.chan0 !c0, p0, r24, #1
	p0 ba SPRV_WriteReg
	nop
Supervisor_1:
	phas.end #0, #0, pixel, none, perinstance

SPRV_WriteReg:
	or r24, sa41, #0x00000000
	or r25, sa42, #0x00000000
	str r24, r25
	ba Supervisor_1
