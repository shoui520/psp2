# NetBSD 2.1 kernel sources

Pristine upstream source: the NetBSD 2.1 release's kernel source set,
https://archive.netbsd.org/pub/NetBSD-archive/NetBSD-2.1/source/sets/syssrc.tgz
(SHA-256 8d9e9453065b0a1f2189bb33368c7362a97904b4885161605f7afeefffb2c36d), its
`usr/src/sys` folder kept here as `sys`.

Kept: the network stack headers and the system headers net_ps reads (`sys/sys`,
`sys/net`, `sys/netinet`, `sys/netinet6`, `sys/altq`, `sys/uvm`, and NetBSD/arm's
`sys/arch/arm/include`), the kernel files net_ps builds (`sys/kern`), and the originals of the
copies net_ps changes, unchanged. Everything else in the set is left out.

net_ps, net_ps_dev and wlanbt include them through `sys/internal/kernel/src/net_ps/include`,
which also holds the psp2 port's `machine/` headers and Sony's changed copies (`net/if.h`,
`netinet/in.h`, `netinet6/in6.h`, `sys/lock.h`, `sys/malloc.h`, `sys/mbuf.h`, `sys/pool.h`,
`sys/proc.h`, `sys/socket.h`, `sys/socketvar.h`, `sys/systm.h`), each change marked `SCE`. A kernel file Sony changed is
patched at configure time from its pristine copy here (`net_ps/<file>_sce.patch`).
