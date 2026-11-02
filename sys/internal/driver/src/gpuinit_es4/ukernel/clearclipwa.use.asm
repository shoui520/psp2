/* Clear clip workaround (BRN31542): the USE tasks of pds/program9*.pds.asm.
 * Floats as bits: 0x3F800000 = 1.0, 0xBF800000 = -1.0, 0x3727C5AC = 1e-5. */

ClearClipWA:
	/* +0x0: 13 words of TA state */
	phas #0, #0, pixel, end, parallel
	mov.skipinv o0, #0x00001F09
	mov.skipinv o1, #0x00000000
	mov.skipinv o2, #0x00000000
	mov.skipinv o3, #0x00000000
	mov.skipinv o4, #0x3F800000
	mov.skipinv o5, #0x00000000
	mov.skipinv o6, #0x3F800000
	mov.skipinv o7, #0x00000000
	mov.skipinv o8, #0x3F800000
	mov.skipinv o9, #0x00000000
	mov.skipinv o10, #0x04001000
	mov.skipinv o11, #0x3727C5AC
	mov.skipinv o12, #0x00000000
	emitst.freep #0 /* incp */, #13
	or.skipinv.end r0, r0, #0x00000000
	/* +0x80: one vertex per instance g25 (0-2): (-1,1), (1,1), (1,-1) */
	phas #0, #0, pixel, end, parallel
	or.skipinv r0, g25, #0x00000000
	xor.skipinv.testtaz.chan0 !c0, p0, r0, #0
	p0 mov.skipinv o0, #0xBF800000
	p0 mov.skipinv o1, #0x3F800000
	p0 mov.skipinv o2, #0xB727C5AC
	p0 mov.skipinv o3, #0x3F800000
	xor.skipinv.testtaz.chan0 !c0, p0, r0, #1
	p0 mov.skipinv o0, #0x3F800000
	p0 mov.skipinv o1, #0x3F800000
	p0 mov.skipinv o2, #0x3727C5AC
	p0 mov.skipinv o3, #0x3F800000
	xor.skipinv.testtaz.chan0 !c0, p0, r0, #2
	p0 mov.skipinv o0, #0x3F800000
	p0 mov.skipinv o1, #0xBF800000
	p0 mov.skipinv o2, #0x3727C5AC
	p0 mov.skipinv o3, #0x3F800000
	emitvtx.freep #0 /* incp */, #0
	or.end r0, r0, #0x00000000
	/* +0x118: 2 words of TA state */
	phas #0, #0, pixel, end, parallel
	mov.skipinv o0, #0x00002000
	mov.skipinv o1, #0x00140014
	emitst.freep #0 /* incp */, #2
	or.end r0, r0, #0x00000000
