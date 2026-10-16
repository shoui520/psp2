; Updater C startup (CA78K0R cstart): register bank 0, stack, hdwinit, clear the
; saddr area and stack, copy initialised data (@@R_INIT/@@R_INIS) and clear @@DATA/@@DATS,
; then main() and exit().
;
; Skeleton. Not written yet. The original (reset 0x2095) follows CA78K0R's cstart.asm,
; with a warm-start check of a retention RAM block (the same: signature 0xa55a and checksum at 0xfd900; stack at 0xd500).

	NAME	cstart
	END
