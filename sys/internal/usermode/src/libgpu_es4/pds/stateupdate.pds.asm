/*
 * SceGpuEs4User_A5E331FA's state update program: the tile rectangle and the
 * MTE copy program as attribute words, three times (each attribute store
 * writes the pair again), then the USE task word, then halt.
 *
 * The caller fills the data segment before the program runs (gxmMteWriteRegionBlock):
 * word 0 is the state register write, word 2 the caller's data word, word 4 the
 * tile rectangle, word 8 the copy program's address and word 9 its task control
 * word. A5E331FA keeps the code as immediate stores into the buffer, so only its
 * words appear in the C; this file is what they are.
 */
temp dword discard = ds1[48];
data dword STATE0, STATE1;		/* words 0 and 2: the state write */
data dword TILES0, TILES1;		/* words 4 and 6: the tile rectangle */
data dword USE0, USE1;			/* words 8 and 10: the USE task */
data dword ZERO;			/* word 1: the word the USE store repeats */

	movs	douta, STATE0, STATE1, discard, discard
	movs	douta, TILES0, TILES1, discard, discard
	movs	douta, TILES0, TILES1, discard, discard
	movs	douta, TILES0, TILES1, discard, discard
	movs	doutu, USE0, USE1, ZERO, ZERO
	halt
