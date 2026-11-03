/*
 * SceGpuEs4User_86FC3202: the fixed four-instruction USSE sequence before a
 * vertex stream load (Sony's eight words 00 00 FA440700 / 00 FCA00000 /
 * 200000 FCA00000 / 81200000 FB260000): a phase, two task-control loads of
 * slot 0 and 1 with address 0 (mov.skipinv), and the pixel event end with
 * free phases. SceGpuEs4User_53CACDAB emits the same phase and mov.skipinv per
 * address for six slots, then emitpix.end.freep with the count.
 */
phas #0, #0, pixel, end, parallel
mov.skipinv r0, #0
mov.skipinv r1, #0
emitpix.end.freep #0, r0, #0, r0, #0x00000004
