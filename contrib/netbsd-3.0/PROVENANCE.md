# NetBSD 3.0 kernel sources

Pristine upstream source: the NetBSD 3.0 release's kernel source set,
https://archive.netbsd.org/pub/NetBSD-archive/NetBSD-3.0/source/sets/syssrc.tgz
(SHA-256 716e5b7e1940fa83320f7029c59dcd7d65924c4521e2728343b32711a8627d9e), its
`usr/src/sys` folder kept here as `sys`.

Kept: the files of the network stack where net_ps follows 3.0 rather than 2.1 (`sys/sys/callout.h`,
`sys/lib/libkern`, `sys/net/if_pppoe.c`, `sys/net/if_spppsubr.c`, ...), unchanged. Everything else in the set is left out.
