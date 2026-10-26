; The ends of the segments the C startup copies and clears (cstart.asm has their starts): an
; empty segment of each, with its end label, linked after every other object's, so each
; label is where its segments end. Ours: CA78K0R's startup bounds them the same way.

	NAME	cstartend

	PUBLIC	_?R_INIT, _?R_INIS, _?RLINIT, _?DATA, _?DATAL, _?DATS

@@R_INIT	CSEG	UNIT64KP
_?R_INIT:
@@R_INIS	CSEG	UNIT64KP
_?R_INIS:
@@RLINIT	CSEG	UNIT64KP
_?RLINIT:
@@DATA	DSEG	BASEP
_?DATA:
@@DATAL	DSEG	UNIT64KP
_?DATAL:
@@DATS	DSEG	SADDRP
_?DATS:

	END
