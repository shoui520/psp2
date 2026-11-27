#ifndef PSPNET_RESOLVER_NET_H
#define PSPNET_RESOLVER_NET_H

#include "pspnet_resolver_int.h"

typedef struct SceNetInetSockaddrIn {
    u8 sin_len;
    u8 sin_family;
    u16 sin_port;
    u32 sin_addr;
    u8 sin_zero[8];
} SceNetInetSockaddrIn;

typedef struct _sce_pspnet_resolver_dns_header {
    u16 id;
    u16 flags;
    u16 qdcount;
    u16 ancount;
    u16 nscount;
    u16 arcount;
} _sce_pspnet_resolver_dns_header;


typedef struct _sce_pspnet_resolver_dns_header SceDnsHeader;

int _sce_pspnet_resolver_do_resolv(ResolverNode *node, int type, const void *name_or_addr, int out_len, void *out_buf, int timeout, int retry);
int _sce_pspnet_resolver_net_init(void);
int _sce_pspnet_resolver_net_term(void);

#endif /* PSPNET_RESOLVER_NET_H */
