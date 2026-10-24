#include "contrib/netbsd-2.1/sys/sys/mallocvar.h"

/*
 * SCE: net_ps keeps no malloc types: every type is NULL (in_pcbinit and ip_init pass hashinit
 * a NULL type), and nothing defines one
 */
#undef	MALLOC_DEFINE_LIMIT
#undef	MALLOC_DEFINE
#undef	MALLOC_DECLARE
#define	MALLOC_DEFINE_LIMIT(type, shortdesc, longdesc, limit)
#define	MALLOC_DEFINE(type, shortdesc, longdesc)
#define	MALLOC_DECLARE(type)

#define	M_1394DATA	((struct malloc_type *)0)
#define	M_DEVBUF	((struct malloc_type *)0)
#define	M_DMAMAP	((struct malloc_type *)0)
#define	M_EMULDATA	((struct malloc_type *)0)
#define	M_FILE		((struct malloc_type *)0)
#define	M_FREE		((struct malloc_type *)0)
#define	M_FTABLE	((struct malloc_type *)0)
#define	M_IFADDR	((struct malloc_type *)0)
#define	M_IFMADDR	((struct malloc_type *)0)
#define	M_IFMEDIA	((struct malloc_type *)0)
#define	M_IOCTLOPS	((struct malloc_type *)0)
#define	M_IOV		((struct malloc_type *)0)
#define	M_IPMADDR	((struct malloc_type *)0)
#define	M_IPMOPTS	((struct malloc_type *)0)
#define	M_KEVENT	((struct malloc_type *)0)
#define	M_MBUF		((struct malloc_type *)0)
#define	M_MRTABLE	((struct malloc_type *)0)
#define	M_NETADDR	((struct malloc_type *)0)
#define	M_PACKET_TAGS	((struct malloc_type *)0)
#define	M_PCB		((struct malloc_type *)0)
#define	M_PROC		((struct malloc_type *)0)
#define	M_RTABLE	((struct malloc_type *)0)
#define	M_SESSION	((struct malloc_type *)0)
#define	M_SOFTINTR	((struct malloc_type *)0)
#define	M_SONAME	((struct malloc_type *)0)
#define	M_SOOPTS	((struct malloc_type *)0)
#define	M_SUBPROC	((struct malloc_type *)0)
#define	M_SYSCTLDATA	((struct malloc_type *)0)
#define	M_SYSCTLNODE	((struct malloc_type *)0)
#define	M_TEMP		((struct malloc_type *)0)
#define	M_UFSMNT	((struct malloc_type *)0)
