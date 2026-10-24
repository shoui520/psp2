/* NetBSD's kernel utility declarations, shared by the vendored sources. */
#include "contrib/netbsd-3.0/sys/lib/libkern/libkern.h"

/*
 * SCE: the original calls memcpy, memcmp, memset, strcpy, strcmp, strlen and ffs, which
 * libkern.h makes GCC's built-ins.
 */
#undef memcpy
#undef memcmp
#undef memset
#undef strcpy
#undef strcmp
#undef strlen
#undef ffs

/*
 * SCE: in NetBSD's own files (NET_PS_NETBSD, set by the build), a copy of no bytes calls nothing
 * (m_pulldown, m_tag_copy: the length is tested, signed, before every call)
 */
#ifdef NET_PS_NETBSD
#define	memcpy(d, s, l)		((l) > 0 ? memcpy((d), (s), (l)) : (d))
#endif

/* SCE: the stack's one random number source is arc4random (igmp_input, in_pcbinit) */
#define	random()		arc4random()
