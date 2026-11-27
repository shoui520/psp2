/*
 * BSD domain/protosw registry and SceNetInet socket-abort hooks.
 */
#include "pspnet_inet_internal.h"

InetDomain *g_domains;

u32 g_inet_ticks;
static u32 g_pffast_ticks;
static InetCallout g_pffast_callout;
static InetCallout g_pfslow_callout;

static void INET_NOINLINE inetSoabortPre(InetThreadInfo *ti)
{
    InetSocket *so = ti->active_sock;
    while (so != NULL) {
        if ((so->so_state & INET_SS_ABORTED) != 0) {
            so->so_state &= (u16)~INET_SS_ABORTED;
            ti->slp_flags |= 2U;
        }
        so = so->so_owner_next;
    }
}

static void INET_NOINLINE inetSoabortPost(InetSocket *so)
{
    InetThreadInfo *ti = so->so_owner_ti;
    if (ti == NULL || ti->wchan == 0) {
        so->so_state |= INET_SS_ABORTED;
        return;
    }
    ti->wchan = 0;
    ti->slp_flags |= 2U;
    sceKernelSignalSema(ti->sema_id, 1);
}

int INET_NOINLINE inetSoabortInit(void)
{
    return sceNet_lib_E155112D(inetSoabortPre, inetSoabortPost);
}

int INET_NOINLINE inetSoabortTerm(void)
{
    return sceNet_lib_41621EB0();
}

int INET_NOINLINE inetDomainInit(void)
{
    InetDomain *dp;
    u32 maxLinkHdr;
    u32 maxProtoHdr;
    u32 maxHdr;
    u32 maxDatelen;

    g_inet_ticks = 0;
    g_pffast_ticks = 0;

    g_arpdomain.dom_next = &g_inetdomain;
    g_routedomain.dom_next = &g_arpdomain;
    g_inetdomain.dom_next = NULL;
    g_domains = &g_routedomain;

    for (dp = g_domains; dp != NULL; dp = dp->dom_next) {
        InetProtosw *pr;
        if (dp->dom_init != NULL) {
            dp->dom_init();
        }
        for (pr = dp->dom_protosw; pr < dp->dom_protosw_end; pr++) {
            if (pr->pr_init != NULL) {
                pr->pr_init();
            }
        }
    }

    sceNetIfhandle_lib_5FB31C72(&maxLinkHdr, &maxProtoHdr, &maxHdr, &maxDatelen);
    if (maxLinkHdr < 16U) {
        maxLinkHdr = 16U;
    }
    maxHdr = maxLinkHdr + maxProtoHdr;
    maxDatelen = 208U - maxHdr;
    sceNetIfhandle_lib_62B20015(maxLinkHdr, maxProtoHdr, maxHdr, maxDatelen);

    sceNet_lib_25CC373A(&g_pffast_callout);
    sceNet_lib_25CC373A(&g_pfslow_callout);
    sceNet_lib_7C86FBA4(&g_pffast_callout, 1, inetPfFasttimo, NULL);
    sceNet_lib_7C86FBA4(&g_pfslow_callout, 1, inetPfSlowtimo, NULL);
    return 0;
}

void INET_NOINLINE inetDomainTerm(void)
{
}

InetDomain *INET_NOINLINE inetFindDomain(int family)
{
    InetDomain *dp;
    for (dp = g_domains; dp != NULL; dp = dp->dom_next) {
        if (dp->dom_family == family) {
            return dp;
        }
    }
    return NULL;
}

InetProtosw *INET_NOINLINE inetFindType(int family, int type)
{
    InetDomain *dp;
    InetProtosw *pr;

    for (dp = g_domains; dp != NULL; dp = dp->dom_next) {
        if (dp->dom_family == family) {
            break;
        }
    }
    if (dp == NULL) {
        return NULL;
    }
    for (pr = dp->dom_protosw; pr < dp->dom_protosw_end; pr++) {
        if (pr->pr_type != 0 && pr->pr_type == type) {
            return pr;
        }
    }
    return NULL;
}

InetProtosw *INET_NOINLINE inetFindProto(int family, int protocol, int type)
{
    InetDomain *dp;
    InetProtosw *pr;
    InetProtosw *maybe = NULL;

    if (family == 0) {
        return NULL;
    }
    for (dp = g_domains; dp != NULL; dp = dp->dom_next) {
        if (dp->dom_family == family) {
            break;
        }
    }
    if (dp == NULL) {
        return NULL;
    }
    for (pr = dp->dom_protosw; pr < dp->dom_protosw_end; pr++) {
        if (pr->pr_protocol == protocol && pr->pr_type == type) {
            return pr;
        }
        if (type == INET_SOCK_RAW && pr->pr_type == INET_SOCK_RAW &&
            pr->pr_protocol == 0 && maybe == NULL) {
            maybe = pr;
        }
    }
    return maybe;
}

int INET_NOINLINE inetNetSysctl(int *name, u32 namelen, void *oldp, u32 *oldlenp,
                                void *newp, u32 newlen)
{
    InetDomain *dp;
    InetProtosw *pr;
    int family;
    int protocol;

    if (namelen < 2U) {
        return INET_EISDIR;
    }
    family = name[0];
    if (family == 0) {
        return 0;
    }
    for (dp = g_domains; dp != NULL; dp = dp->dom_next) {
        if (dp->dom_family == family) {
            break;
        }
    }
    if (dp == NULL) {
        return INET_ENOPROTOOPT;
    }
    if (namelen < 3U) {
        return INET_EISDIR;
    }
    protocol = name[1];
    for (pr = dp->dom_protosw; pr < dp->dom_protosw_end; pr++) {
        if (pr->pr_protocol == protocol && pr->pr_sysctl != NULL) {
            return pr->pr_sysctl(name + 2, namelen - 2U, oldp, oldlenp, newp, newlen);
        }
    }
    return INET_ENOPROTOOPT;
}

void INET_NOINLINE inetPfCtlinput(int cmd, InetSockaddr *sa)
{
    InetDomain *dp;
    InetProtosw *pr;

    for (dp = g_domains; dp != NULL; dp = dp->dom_next) {
        for (pr = dp->dom_protosw; pr < dp->dom_protosw_end; pr++) {
            if (pr->pr_ctlinput != NULL) {
                pr->pr_ctlinput(cmd, sa, NULL);
            }
        }
    }
}

void INET_NOINLINE inetPfSlowtimo(void *arg)
{
    InetDomain *dp;
    InetProtosw *pr;
    (void)arg;

    g_inet_ticks++;
    for (dp = g_domains; dp != NULL; dp = dp->dom_next) {
        for (pr = dp->dom_protosw; pr < dp->dom_protosw_end; pr++) {
            if (pr->pr_slowtimo != NULL) {
                pr->pr_slowtimo();
            }
        }
    }
    sceNet_lib_7C86FBA4(&g_pfslow_callout, 50, inetPfSlowtimo, NULL);
}

void INET_NOINLINE inetPfFasttimo(void *arg)
{
    InetDomain *dp;
    InetProtosw *pr;
    (void)arg;

    g_pffast_ticks++;
    for (dp = g_domains; dp != NULL; dp = dp->dom_next) {
        for (pr = dp->dom_protosw; pr < dp->dom_protosw_end; pr++) {
            if (pr->pr_fasttimo != NULL) {
                pr->pr_fasttimo();
            }
        }
    }
    sceNet_lib_7C86FBA4(&g_pffast_callout, 20, inetPfFasttimo, NULL);
}
