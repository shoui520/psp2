xnu-1456.1.26
=============

Upstream: Apple's xnu, tag `xnu-1456.1.26` of
https://github.com/apple-oss-distributions/xnu (Mac OS X 10.6, 2009), files fetched
from that tag:

| File | SHA-256 |
|---|---|
| `bsd/dev/dtrace/blist.c` | c8a3593a0ce9bc042423ba006f7ba1d308640941d78bc1ff0ecb43998110ecc5 |
| `bsd/dev/dtrace/blist.h` | 055839bad4267f546c9a1915d53afd6d535df4038369e4f7893691a60b5b39d2 |
| `bsd/kern/qsort.c` | 58bbcbbefb563163a0973487dffc5b49f4102744851ec8486c41a4deeef73b02 |
| `APPLE_LICENSE` | e5881019d8766c1e88a5fe1dbca4ba40c78011d41fcb18f6e9f50df60182685b |

Kept, unchanged: the files above (blist.c and blist.h are FreeBSD's
`subr_blist.c` 1.5.2.1 and `blist.h` 1.2 under Matthew Dillon's BSD terms; qsort.c is
4.4BSD's, under the BSD licence in its header). Left out: the rest of xnu.

Used by: os0:kd/deci4p_dtracep (Sony's `dtrace/bsd/blist.c` and `dtrace/bsd/qsort.c`),
with Sony's changes as patches in its `dtrace/bsd/` folder. `blist.h` is kept for
reference: the module declares the lists in `sys/internal/include/dtrace/bsd/blist.h`.

The release: Sony's DTrace port is based on Apple's, and its blist panics sit where
xnu's are (Sony's debug records put them at xnu's lines plus Sony's own block).
blist.c is the same file in every xnu release from 1456.1.26 to 4570.1.46 (1228 lacks
Apple's `__APPLE__` changes, 6153 rewrote it), so the pin is the oldest release with
Sony's code; qsort.c is taken from the same tag.
