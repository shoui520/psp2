#include "pspnet_internal.h"

#define NBPFILTER      8
#define BPF_MAXINSNS   512
#define BPF_MAXBUFSIZE 0x8000
#define BPF_MINBUFSIZE 32

typedef struct BpfInsn {
    u16 code;
    u8 jt;
    u8 jf;
    u32 k;
} BpfInsn;

typedef struct BpfProgram {
    u32 bf_len;
    BpfInsn *bf_insns;
} BpfProgram;

typedef struct BpfHdr {
    NetTimeval bh_tstamp;        /* 0x00 */
    u32 bh_caplen;               /* 0x08 */
    u32 bh_datalen;              /* 0x0C */
    u16 bh_hdrlen;               /* 0x10 */
} BpfHdr;

typedef struct BpfStat {
    u32 bs_recv;
    u32 bs_drop;
} BpfStat;

typedef struct BpfVersion {
    u16 bv_major;
    u16 bv_minor;
} BpfVersion;

typedef struct BpfIf {
    struct BpfIf *bif_next;
    struct BpfDesc *bif_dlist;
    void **bif_driverp;
    u32 bif_dlt;
    u32 bif_hdrlen;
    NetIfnet *bif_ifp;
} BpfIf;

typedef struct BpfDesc {
    struct BpfDesc *bd_next;     /* 0x00 */
    u8 *bd_sbuf;                 /* 0x04 */
    u8 *bd_hbuf;                 /* 0x08 */
    u8 *bd_fbuf;                 /* 0x0C */
    u32 bd_slen;                 /* 0x10 */
    u32 bd_hlen;                 /* 0x14 */
    u32 bd_bufsize;              /* 0x18 */
    BpfIf *bd_bif;               /* 0x1C */
    s32 bd_rtout;                /* 0x20 */
    BpfInsn *bd_filter;          /* 0x24 */
    u32 bd_rcount;               /* 0x28 */
    u32 bd_dcount;               /* 0x2C */
    u8 bd_promisc;               /* 0x30 */
    u8 bd_state;                 /* 0x31 */
    u8 bd_immediate;             /* 0x32 */
    u8 bd_pad;                   /* 0x33 */
    u32 bd_hdrcmplt;             /* 0x34 */
    SceUID bd_sel_thid;          /* 0x38 */
    u32 bd_reserved;             /* 0x3C */
} BpfDesc;

static u32 g_bpf_bufsize = 0x2000;
static BpfIf *g_bpf_iflist = NULL;
static BpfDesc g_bpf_dtab[NBPFILTER];

static u32 m_xword(NetMbuf *mbuf, u32 offset, int *err)
{
    u32 len = mbuf->m_len;
    while (offset >= len) {
        offset -= len;
        mbuf = mbuf->m_next;
        if (mbuf == NULL) {
            *err = 1;
            return 0;
        }
        len = mbuf->m_len;
    }
    u8 *cp = mbuf->m_data + offset;
    u32 rem = len - offset;
    if (rem >= 4) {
        *err = 0;
        return ((u32)cp[0] << 24) | ((u32)cp[1] << 16) | ((u32)cp[2] << 8) | cp[3];
    }
    NetMbuf *next = mbuf->m_next;
    if (next != NULL && next->m_len + rem >= 4) {
        u8 *np = next->m_data;
        *err = 0;
        if (rem == 1)
            return ((u32)cp[0] << 24) | ((u32)np[0] << 16) | ((u32)np[1] << 8) | np[2];
        if (rem == 2)
            return ((u32)cp[0] << 24) | ((u32)cp[1] << 16) | ((u32)np[0] << 8) | np[1];
        return ((u32)cp[0] << 24) | ((u32)cp[1] << 16) | ((u32)cp[2] << 8) | np[0];
    }
    *err = 1;
    return 0;
}

static u32 m_xhalf(NetMbuf *mbuf, u32 offset, int *err)
{
    u32 len = mbuf->m_len;
    while (offset >= len) {
        offset -= len;
        mbuf = mbuf->m_next;
        if (mbuf == NULL) {
            *err = 1;
            return 0;
        }
        len = mbuf->m_len;
    }
    u8 *cp = mbuf->m_data + offset;
    if (len - offset < 2) {
        NetMbuf *next = mbuf->m_next;
        if (next != NULL) {
            *err = 0;
            return ((u32)cp[0] << 8) | next->m_data[0];
        }
        *err = 1;
        return 0;
    }
    *err = 0;
    return ((u32)cp[0] << 8) | cp[1];
}

static int bpf_validate(const BpfInsn *insns, int len)
{
    int idx;
    for (idx = 0; idx < len; idx++) {
        const BpfInsn *insn = &insns[idx];
        if ((insn->code & 7) == 5) {
            if ((insn->code & 0xF0) != 0) {
                if (idx + 1 + insn->jt >= len || idx + 1 + insn->jf >= len)
                    return 0;
            } else {
                s32 rel = (s32)insn->k;
                if (rel < 0 || idx + 1 + rel >= len || idx + 1 + rel < 0)
                    return 0;
            }
        }
        if ((insn->code & 7) == 2 || (insn->code & 0xE7) == 0x60) {
            if (insn->k >= 16)
                return 0;
        }
        if (insn->code == 0x34 && insn->k == 0)
            return 0;
    }
    return (insns[len - 1].code & 7) == 6;
}

static u32 bpf_filter(const BpfInsn *pc, void *pkt, u32 wirelen, u32 buflen)
{
    u32 acc = 0;
    u32 reg_x = 0;
    u32 mem[16];
    int merr;

    if (pc == NULL)
        return (u32)-1;

    pc--;
    for (;;) {
        pc++;
        switch (pc->code) {
        case 0x00: acc = pc->k; continue;
        case 0x01: reg_x = pc->k; continue;
        case 0x02: mem[pc->k] = acc; continue;
        case 0x03: mem[pc->k] = reg_x; continue;
        case 0x04: acc += pc->k; continue;
        case 0x05: pc += pc->k; continue;
        case 0x06: return pc->k;
        case 0x07: reg_x = acc; continue;
        case 0x0C: acc += reg_x; continue;
        case 0x14: acc -= pc->k; continue;
        case 0x15: pc += (acc == pc->k) ? pc->jt : pc->jf; continue;
        case 0x16: return acc;
        case 0x1C: acc -= reg_x; continue;
        case 0x1D: pc += (acc == reg_x) ? pc->jt : pc->jf; continue;
        case 0x20: {
            u32 offset = pc->k;
            if (offset + 4 <= buflen) {
                u8 *cp = (u8 *)pkt + offset;
                acc = ((u32)cp[0] << 24) | ((u32)cp[1] << 16) | ((u32)cp[2] << 8) | cp[3];
            } else {
                if (buflen != 0)
                    return 0;
                merr = 0;
                acc = m_xword((NetMbuf *)pkt, offset, &merr);
                if (merr != 0)
                    return 0;
            }
            continue;
        }
        case 0x24: acc *= pc->k; continue;
        case 0x25: pc += (acc > pc->k) ? pc->jt : pc->jf; continue;
        case 0x28: {
            u32 offset = pc->k;
            if (offset + 2 <= buflen) {
                u8 *cp = (u8 *)pkt + offset;
                acc = ((u32)cp[0] << 8) | cp[1];
            } else {
                if (buflen != 0)
                    return 0;
                merr = 0;
                acc = m_xhalf((NetMbuf *)pkt, offset, &merr);
            }
            continue;
        }
        case 0x2C: acc *= reg_x; continue;
        case 0x2D: pc += (acc > reg_x) ? pc->jt : pc->jf; continue;
        case 0x30: {
            s32 offset = (s32)pc->k;
            if ((u32)offset < buflen) {
                acc = ((u8 *)pkt)[offset];
            } else {
                if (buflen != 0)
                    return 0;
                NetMbuf *mbuf = (NetMbuf *)pkt;
                while (offset >= (s32)mbuf->m_len) {
                    offset -= (s32)mbuf->m_len;
                    mbuf = mbuf->m_next;
                    if (mbuf == NULL)
                        return 0;
                }
                acc = mbuf->m_data[offset];
            }
            continue;
        }
        case 0x34: acc /= pc->k; continue;
        case 0x35: pc += (acc >= pc->k) ? pc->jt : pc->jf; continue;
        case 0x3C:
            if (reg_x == 0)
                return 0;
            acc /= reg_x;
            continue;
        case 0x3D: pc += (acc >= reg_x) ? pc->jt : pc->jf; continue;
        case 0x40: {
            u32 offset = reg_x + pc->k;
            if (offset + 4 <= buflen) {
                u8 *cp = (u8 *)pkt + offset;
                acc = ((u32)cp[0] << 24) | ((u32)cp[1] << 16) | ((u32)cp[2] << 8) | cp[3];
            } else {
                if (buflen != 0)
                    return 0;
                merr = 0;
                acc = m_xword((NetMbuf *)pkt, offset, &merr);
                if (merr != 0)
                    return 0;
            }
            continue;
        }
        case 0x44: acc |= pc->k; continue;
        case 0x45: pc += (acc & pc->k) ? pc->jt : pc->jf; continue;
        case 0x48: {
            u32 offset = reg_x + pc->k;
            if (offset + 2 <= buflen) {
                u8 *cp = (u8 *)pkt + offset;
                acc = ((u32)cp[0] << 8) | cp[1];
            } else {
                if (buflen != 0)
                    return 0;
                merr = 0;
                acc = m_xhalf((NetMbuf *)pkt, offset, &merr);
                if (merr != 0)
                    return 0;
            }
            continue;
        }
        case 0x4C: acc |= reg_x; continue;
        case 0x4D: pc += (acc & reg_x) ? pc->jt : pc->jf; continue;
        case 0x50: {
            s32 offset = (s32)(reg_x + pc->k);
            if ((u32)offset < buflen) {
                acc = ((u8 *)pkt)[offset];
            } else {
                if (buflen != 0)
                    return 0;
                NetMbuf *mbuf = (NetMbuf *)pkt;
                while (offset >= (s32)mbuf->m_len) {
                    offset -= (s32)mbuf->m_len;
                    mbuf = mbuf->m_next;
                    if (mbuf == NULL)
                        return 0;
                }
                acc = mbuf->m_data[offset];
            }
            continue;
        }
        case 0x54: acc &= pc->k; continue;
        case 0x5C: acc &= reg_x; continue;
        case 0x60: acc = mem[pc->k]; continue;
        case 0x61: reg_x = mem[pc->k]; continue;
        case 0x64: acc <<= pc->k; continue;
        case 0x6C: acc <<= reg_x; continue;
        case 0x74: acc >>= pc->k; continue;
        case 0x7C: acc >>= reg_x; continue;
        case 0x80: acc = wirelen; continue;
        case 0x81: reg_x = wirelen; continue;
        case 0x84: acc = (u32)(-(s32)acc); continue;
        case 0x87: acc = reg_x; continue;
        case 0xB1: {
            s32 offset = (s32)pc->k;
            if ((u32)offset < buflen) {
                reg_x = 4 * (((u8 *)pkt)[offset] & 0xF);
            } else {
                if (buflen != 0)
                    return 0;
                NetMbuf *mbuf = (NetMbuf *)pkt;
                while (offset >= (s32)mbuf->m_len) {
                    offset -= (s32)mbuf->m_len;
                    mbuf = mbuf->m_next;
                    if (mbuf == NULL)
                        return 0;
                }
                reg_x = 4 * (mbuf->m_data[offset] & 0xF);
            }
            continue;
        }
        default:
            return 0;
        }
    }
}

static void *bpf_mcopy(void *dst, void *src_mbuf, u32 len)
{
    u8 *dp = (u8 *)dst;
    NetMbuf *mbuf = (NetMbuf *)src_mbuf;
    while (len != 0) {
        u32 count = mbuf->m_len;
        if (count > len)
            count = len;
        sceKernelMemcpy(dp, mbuf->m_data, count);
        dp += count;
        len -= count;
        mbuf = mbuf->m_next;
    }
    return dst;
}

static void bpf_catchpacket(BpfDesc *desc, void *pkt, u32 pktlen, u32 snaplen,
                            void *(*cpfn)(void *, void *, u32))
{
    u32 curlen, hdrlen, totlen, endlen;
    NetTimeval tv;

    if (snaplen > pktlen)
        snaplen = pktlen;

    hdrlen = desc->bd_bif->bif_hdrlen;
    totlen = hdrlen + snaplen;
    if (totlen > desc->bd_bufsize)
        totlen = desc->bd_bufsize;

    curlen = (desc->bd_slen + 3) & ~3u;
    endlen = curlen + totlen;
    if ((s32)endlen > (s32)desc->bd_bufsize) {
        if (desc->bd_fbuf == NULL) {
            desc->bd_dcount++;
            return;
        }
        desc->bd_hlen = desc->bd_slen;
        desc->bd_slen = 0;
        desc->bd_hbuf = desc->bd_sbuf;
        desc->bd_sbuf = desc->bd_fbuf;
        desc->bd_fbuf = NULL;
        curlen = 0;
        endlen = totlen;
        sceNet_lib_7574FDA1((u32)desc);
        sceNet_lib_B9C780C7(&desc->bd_sel_thid);
    } else if (desc->bd_immediate) {
        sceNet_lib_7574FDA1((u32)desc);
        sceNet_lib_B9C780C7(&desc->bd_sel_thid);
    }

    u8 *base = desc->bd_sbuf + curlen;
    BpfHdr *hdr = (BpfHdr *)base;
    sceNet_lib_4B2B3416(&tv);
    hdr->bh_hdrlen = (u16)hdrlen;
    hdr->bh_tstamp.tv_sec = tv.tv_sec;
    hdr->bh_datalen = pktlen;
    hdr->bh_tstamp.tv_usec = tv.tv_usec;
    hdr->bh_caplen = totlen - hdrlen;
    cpfn(base + hdrlen, pkt, totlen - hdrlen);
    desc->bd_slen = endlen;
}

void bpf_mtap(void *bp_arg, NetMbuf *mbuf_head)
{
    BpfIf *bp = (BpfIf *)bp_arg;
    NetMbuf *mbuf = mbuf_head;
    u32 pktlen = 0;
    while (mbuf != NULL) {
        pktlen += mbuf->m_len;
        mbuf = mbuf->m_next;
    }
    int spl = sceNet_lib_94B44F26();
    BpfDesc *desc = bp->bif_dlist;
    while (desc != NULL) {
        desc->bd_rcount++;
        u32 slen = bpf_filter(desc->bd_filter, mbuf_head, pktlen, 0);
        if (slen != 0)
            bpf_catchpacket(desc, mbuf_head, pktlen, slen, bpf_mcopy);
        desc = desc->bd_next;
    }
    sceNet_lib_515B2F33(spl);
}

void bpf_attach(void **bp_ptr, NetIfnet *ifp, u32 dlt, u32 hdrlen)
{
    BpfIf *bp = (BpfIf *)sceNetIfhandle_lib_15CFE3C0(sizeof(BpfIf));
    bp->bif_next = g_bpf_iflist;
    bp->bif_ifp = ifp;
    g_bpf_iflist = bp;
    bp->bif_dlt = dlt;
    *bp_ptr = NULL;
    bp->bif_driverp = bp_ptr;
    bp->bif_hdrlen = ((hdrlen + 23) & ~3u) - hdrlen;
    bp->bif_dlist = NULL;
}

static inline void bpf_detachd_inline(BpfDesc *desc)
{
    BpfIf *bp = desc->bd_bif;
    BpfDesc **pp = &bp->bif_dlist;
    while (*pp != desc)
        pp = &(*pp)->bd_next;
    *pp = desc->bd_next;
    if (bp->bif_dlist == NULL)
        *bp->bif_driverp = NULL;
    desc->bd_bif = NULL;
}

void bpf_detach(NetIfnet *ifp)
{
    int idx;
    for (idx = 0; idx < NBPFILTER; idx++) {
        BpfDesc *desc = &g_bpf_dtab[idx];
        if (desc->bd_next != desc && desc->bd_bif != NULL && desc->bd_bif->bif_ifp == ifp) {
            int spl = sceNet_lib_94B44F26();
            bpf_detachd_inline(desc);
            sceNet_lib_515B2F33(spl);
        }
    }
    BpfIf **pp = &g_bpf_iflist;
    while (*pp != NULL) {
        if ((*pp)->bif_ifp == ifp) {
            BpfIf *bp = *pp;
            *pp = bp->bif_next;
            sceNetIfhandle_lib_76BAD213(bp);
            return;
        }
        pp = &(*pp)->bif_next;
    }
}

void bpf_init(int count)
{
    int idx;
    (void)count;
    g_bpf_bufsize = 0x2000;
    g_bpf_iflist = NULL;
    for (idx = 0; idx < NBPFILTER; idx++)
        g_bpf_dtab[idx].bd_next = &g_bpf_dtab[idx];
}

static int bpf_allocbufs(BpfDesc *desc)
{
    desc->bd_fbuf = (u8 *)sceNetIfhandle_lib_15CFE3C0(desc->bd_bufsize);
    if (desc->bd_fbuf == NULL)
        return 12;
    desc->bd_sbuf = (u8 *)sceNetIfhandle_lib_15CFE3C0(desc->bd_bufsize);
    if (desc->bd_sbuf == NULL) {
        sceNetIfhandle_lib_76BAD213(desc->bd_fbuf);
        return 12;
    }
    desc->bd_slen = 0;
    desc->bd_hlen = 0;
    return 0;
}

static void bpf_ifname(NetIfnet *ifp, NetIfreq *ifr)
{
    sceKernelMemcpy(ifr->ifr_name, ifp->if_xname, 16);
}

static int bpf_setif(BpfDesc *desc, const char *ifr_name)
{
    BpfIf *bp = g_bpf_iflist;
    while (bp != NULL) {
        if (bp->bif_ifp != NULL && sceNet_lib_A0F16ABD(bp->bif_ifp->if_xname, ifr_name) == 0)
            break;
        bp = bp->bif_next;
    }
    if (bp == NULL)
        return 6;

    if (desc->bd_sbuf == NULL) {
        int err = bpf_allocbufs(desc);
        if (err != 0)
            return err;
    }

    int spl = sceNet_lib_94B44F26();
    if (bp != desc->bd_bif) {
        if (desc->bd_bif != NULL)
            bpf_detachd_inline(desc);
        desc->bd_bif = bp;
        desc->bd_next = bp->bif_dlist;
        bp->bif_dlist = desc;
        *bp->bif_driverp = bp;
    }
    if (desc->bd_hbuf != NULL) {
        desc->bd_fbuf = desc->bd_hbuf;
        desc->bd_hbuf = NULL;
    }
    desc->bd_slen = 0;
    desc->bd_hlen = 0;
    desc->bd_rcount = 0;
    desc->bd_dcount = 0;
    sceNet_lib_515B2F33(spl);
    return 0;
}

static int bpf_setf(BpfDesc *desc, BpfProgram *fp)
{
    BpfInsn *old = desc->bd_filter;
    int spl;

    if (fp->bf_insns == NULL) {
        if (fp->bf_len != 0)
            return 22;
        spl = sceNet_lib_94B44F26();
        desc->bd_filter = NULL;
    } else {
        u32 flen = fp->bf_len;
        if (flen > BPF_MAXINSNS)
            return 22;
        BpfInsn *fcode = (BpfInsn *)sceNetIfhandle_lib_15CFE3C0(flen * sizeof(BpfInsn));
        if (fcode == NULL)
            return 12;
        sceNet_lib_2F305274(fp->bf_insns, fcode, (int)(flen * sizeof(BpfInsn)));
        if (!bpf_validate(fcode, (int)flen)) {
            sceNetIfhandle_lib_76BAD213(fcode);
            return 22;
        }
        spl = sceNet_lib_94B44F26();
        desc->bd_filter = fcode;
    }

    if (desc->bd_hbuf != NULL) {
        desc->bd_fbuf = desc->bd_hbuf;
        desc->bd_hbuf = NULL;
    }
    desc->bd_slen = 0;
    desc->bd_hlen = 0;
    desc->bd_rcount = 0;
    desc->bd_dcount = 0;
    sceNet_lib_515B2F33(spl);

    if (old != NULL)
        sceNetIfhandle_lib_76BAD213(old);
    return 0;
}

static int bpf_open_dev(u32 unit)
{
    if (unit >= NBPFILTER)
        return 6;
    BpfDesc *desc = &g_bpf_dtab[unit];
    if (desc->bd_next != desc)
        return 16;
    sceKernelMemset(desc, 0, sizeof(BpfDesc));
    desc->bd_bufsize = g_bpf_bufsize;
    return 0;
}

static int bpf_close_dev(u32 unit)
{
    BpfDesc *desc = &g_bpf_dtab[unit];
    int spl = sceNet_lib_94B44F26();
    if (desc->bd_bif != NULL)
        bpf_detachd_inline(desc);
    sceNet_lib_515B2F33(spl);

    if (desc->bd_sbuf != NULL) {
        sceNetIfhandle_lib_76BAD213(desc->bd_sbuf);
        if (desc->bd_hbuf != NULL)
            sceNetIfhandle_lib_76BAD213(desc->bd_hbuf);
        if (desc->bd_fbuf != NULL)
            sceNetIfhandle_lib_76BAD213(desc->bd_fbuf);
    }
    if (desc->bd_filter != NULL)
        sceNetIfhandle_lib_76BAD213(desc->bd_filter);
    desc->bd_next = desc;
    return 0;
}

static int bpf_read_dev(u32 unit, NetUio *uio)
{
    BpfDesc *desc = &g_bpf_dtab[unit];
    int err;

    if (uio->uio_resid != desc->bd_bufsize)
        return 22;

    int spl = sceNet_lib_94B44F26();
    while (desc->bd_hbuf == NULL) {
        if (desc->bd_immediate && desc->bd_slen != 0) {
            desc->bd_hbuf = desc->bd_sbuf;
            desc->bd_hlen = desc->bd_slen;
            desc->bd_sbuf = desc->bd_fbuf;
            desc->bd_fbuf = NULL;
            desc->bd_slen = 0;
            break;
        }
        err = 11;
        if (desc->bd_rtout != -1)
            err = sceNet_lib_205E8D17((u32)desc, (u32)desc->bd_rtout);
        if (err == 11) {
            if (desc->bd_hbuf != NULL)
                break;
            if (desc->bd_slen == 0) {
                sceNet_lib_515B2F33(spl);
                return 0;
            }
            desc->bd_hbuf = desc->bd_sbuf;
            desc->bd_hlen = desc->bd_slen;
            desc->bd_sbuf = desc->bd_fbuf;
            desc->bd_fbuf = NULL;
            desc->bd_slen = 0;
            break;
        }
        if (err != 0) {
            sceNet_lib_515B2F33(spl);
            return err;
        }
    }

    sceNet_lib_515B2F33(spl);
    err = sceNet_lib_2B42872F(desc->bd_hbuf, (int)desc->bd_hlen, uio);
    spl = sceNet_lib_94B44F26();
    desc->bd_fbuf = desc->bd_hbuf;
    desc->bd_hbuf = NULL;
    desc->bd_hlen = 0;
    sceNet_lib_515B2F33(spl);
    return err;
}

static int bpf_write_dev(u32 unit, NetUio *uio)
{
    BpfDesc *desc = &g_bpf_dtab[unit];
    if (desc->bd_bif == NULL)
        return 6;

    NetIfnet *ifp = desc->bd_bif->bif_ifp;
    u32 len = uio->uio_resid;
    if (len == 0)
        return 0;

    NetMbuf *mbuf = (NetMbuf *)sceNetIfhandle_lib_15CFE3C0(sizeof(NetMbuf));
    if (mbuf == NULL)
        return 105;

    mbuf->m_type = 1;
    mbuf->m_data = mbuf->m_pktdat;
    mbuf->m_flags = 2;
    mbuf->m_next = NULL;
    mbuf->m_nextpkt = NULL;
    mbuf->m_pkthdr_csum = 0;
    mbuf->m_pkthdr_rcvif = NULL;
    mbuf->m_pkthdr_len = len;

    if (len >= 209) {
        mbuf->m_ext.ext_buf = (u8 *)sceNetIfhandle_lib_15CFE3C0(2048);
        if (mbuf->m_ext.ext_buf == NULL)
            mbuf->m_ext.ext_buf = (u8 *)sceNetIfhandle_lib_15CFE3C0(2048);
        if (mbuf->m_ext.ext_buf != NULL) {
            mbuf->m_data = mbuf->m_ext.ext_buf;
            mbuf->m_flags |= 9;
            mbuf->m_ext.ext_size = 2048;
            mbuf->m_ext.ext_free = NULL;
            mbuf->m_ext.ext_arg = NULL;
            mbuf->m_ext.ext_prevref = mbuf;
            mbuf->m_ext.ext_nextref = mbuf;
        }
        if ((mbuf->m_flags & 1) == 0) {
            sceNetIfhandle_lib_F56FAC82(mbuf);
            return 105;
        }
    }

    int err = sceNet_lib_2B42872F(mbuf->m_data, (int)len, uio);
    if (err != 0) {
        sceNetIfhandle_lib_F56FAC82(mbuf);
        return err;
    }

    mbuf->m_len = len;
    int spl = sceNet_lib_94B44F26();
    if (ifp->if_snd.ifq_len >= ifp->if_snd.ifq_maxlen) {
        sceNet_lib_515B2F33(spl);
        sceNetIfhandle_lib_F56FAC82(mbuf);
        return 105;
    }
    mbuf->m_nextpkt = NULL;
    ifp->if_data.ifi_obytes += mbuf->m_pkthdr_len;
    if (ifp->if_snd.ifq_tail != NULL)
        ifp->if_snd.ifq_tail->m_nextpkt = mbuf;
    else
        ifp->if_snd.ifq_head = mbuf;
    ifp->if_snd.ifq_tail = mbuf;
    ifp->if_snd.ifq_len++;
    if ((ifp->if_flags & 0x400) == 0)
        ifp->if_start(ifp);
    sceNet_lib_515B2F33(spl);
    return 0;
}

static int bpf_ioctl_dev(u32 unit, u32 cmd, void *addr)
{
    BpfDesc *desc = &g_bpf_dtab[unit];

    switch (cmd) {
    case 0x40044266u: /* BIOCGBLEN */
        *(u32 *)addr = desc->bd_bufsize;
        return 0;
    case 0xC0044266u: /* BIOCSBLEN */
        if (desc->bd_bif != NULL)
            return 22;
        {
            u32 sz = *(u32 *)addr;
            if (sz > BPF_MAXBUFSIZE) {
                sz = BPF_MAXBUFSIZE;
                *(u32 *)addr = sz;
            } else if (sz < BPF_MINBUFSIZE) {
                sz = BPF_MINBUFSIZE;
                *(u32 *)addr = sz;
            }
            desc->bd_bufsize = sz;
        }
        return 0;
    case 0x80084267u: /* BIOCSETF */
        return bpf_setf(desc, (BpfProgram *)addr);
    case 0x20004268u: /* BIOCFLUSH */ {
        int spl = sceNet_lib_94B44F26();
        if (desc->bd_hbuf != NULL) {
            desc->bd_fbuf = desc->bd_hbuf;
            desc->bd_hbuf = NULL;
        }
        desc->bd_dcount = 0;
        desc->bd_slen = 0;
        desc->bd_hlen = 0;
        desc->bd_rcount = 0;
        sceNet_lib_515B2F33(spl);
        return 0;
    }
    case 0x4004426Au: /* BIOCGDLT */
        if (desc->bd_bif == NULL)
            return 22;
        *(u32 *)addr = desc->bd_bif->bif_dlt;
        return 0;
    case 0x4020426Bu: /* BIOCGETIF */
        if (desc->bd_bif == NULL)
            return 22;
        bpf_ifname(desc->bd_bif->bif_ifp, (NetIfreq *)addr);
        return 0;
    case 0x8020426Cu: /* BIOCSETIF */
        return bpf_setif(desc, (const char *)addr);
    case 0x8008426Du: /* BIOCSRTIMEOUT */ {
        NetTimeval *tv = (NetTimeval *)addr;
        u32 ticks = tv->tv_sec * 100 + tv->tv_usec / 10000;
        desc->bd_rtout = (s32)ticks;
        if (ticks == 0 && tv->tv_usec != 0)
            desc->bd_rtout = 1;
        return 0;
    }
    case 0x4008426Eu: /* BIOCGRTIMEOUT */ {
        NetTimeval *tv = (NetTimeval *)addr;
        u32 ticks = (u32)desc->bd_rtout;
        tv->tv_sec = ticks / 100;
        tv->tv_usec = (ticks % 100) * 10000;
        return 0;
    }
    case 0x4008426Fu: /* BIOCGSTATS */ {
        BpfStat *st = (BpfStat *)addr;
        st->bs_drop = desc->bd_dcount;
        st->bs_recv = desc->bd_rcount;
        return 0;
    }
    case 0x80044270u: /* BIOCIMMEDIATE */
        desc->bd_immediate = *(u8 *)addr;
        return 0;
    case 0x40044271u: /* BIOCVERSION */ {
        BpfVersion *ver = (BpfVersion *)addr;
        ver->bv_minor = 1;
        ver->bv_major = 1;
        return 0;
    }
    case 0x40044274u: /* BIOCGHDRCMPLT */
        *(u32 *)addr = desc->bd_hdrcmplt;
        return 0;
    case 0x80044275u: /* BIOCSHDRCMPLT */
        desc->bd_hdrcmplt = (*(u32 *)addr != 0) ? 1 : 0;
        return 0;
    case 0x8004667Eu: /* FIONBIO */
        desc->bd_rtout = (*(u32 *)addr != 0) ? -1 : 0;
        return 0;
    case 0x4004667Fu: /* FIONREAD */ {
        int spl = sceNet_lib_94B44F26();
        u32 avail = desc->bd_slen;
        if (desc->bd_hbuf != NULL)
            avail += desc->bd_hlen;
        sceNet_lib_515B2F33(spl);
        *(u32 *)addr = avail;
        return 0;
    }
    default:
        return 22;
    }
}

static int bpf_poll_dev(u32 unit, int events, NetThreadInfo *ti)
{
    BpfDesc *desc = &g_bpf_dtab[unit];
    int revents = 0;
    int spl = sceNet_lib_94B44F26();
    if (events & 0x41) {
        if (desc->bd_hlen != 0 || (desc->bd_immediate && desc->bd_slen != 0))
            revents = events & 0x41;
        else
            sceNet_lib_92633D8D(ti, &desc->bd_sel_thid);
    }
    sceNet_lib_515B2F33(spl);
    return revents;
}

/* Shared sceNet / sceNet_lib BPF exports */
int sceNet_BFCFEFF6(u32 unit, int events, int timeout_ms)
{
    int err = 0;
    int revents = 0;
    s64 start_time = 0;
    NetThreadInfo *ti = sceNet_lib_CA3CF5EB(&err);
    if (ti == NULL)
        return -1;

    u32 timeout_us = 0;
    int valid = 1;
    if (timeout_ms != -1) {
        if ((u32)timeout_ms > 4294967u) {
            err = 22;
            valid = 0;
        } else {
            timeout_us = (u32)timeout_ms * 1000u;
        }
    }

    if (valid) {
        u32 rem_us = timeout_us;
        for (;;) {
            int spl = sceNet_lib_94B44F26();
            err = bpf_poll_dev(unit, events, ti);
            if (err != 0) {
                sceNet_lib_515B2F33(spl);
                break;
            }
            if (timeout_ms != -1) {
                if (start_time == 0)
                    start_time = sceKernelGetSystemTimeWide();
                s64 now = sceKernelGetSystemTimeWide();
                if ((u64)(now - start_time) >= (u64)timeout_us) {
                    sceNet_lib_515B2F33(spl);
                    break;
                }
                rem_us = timeout_us - (u32)(now - start_time);
            }
            err = sceNet_lib_F6DB0A0B((u32)&g_selwait, rem_us);
            sceNet_lib_515B2F33(spl);
            if (err != 0)
                break;
        }
    }

    if (err > 0) {
        revents = err;
        err = 0;
    }
    if (sceNet_lib_757085B0(ti, err) < 0)
        return -1;
    return revents;
}

int sceNet_3B617AA0(void)
{
    int err = 0;
    u32 unit = 0;
    NetThreadInfo *ti = sceNet_lib_CA3CF5EB(&err);
    if (ti == NULL)
        return -1;

    for (;;) {
        err = bpf_open_dev(unit);
        if (err == 0)
            break;
        unit++;
        if (err != 16)
            break;
    }
    if (sceNet_lib_757085B0(ti, err) < 0)
        return -1;
    return (int)unit;
}

int sceNet_DB88F458(u32 unit)
{
    int err = 0;
    NetThreadInfo *ti = sceNet_lib_CA3CF5EB(&err);
    if (ti == NULL)
        return -1;
    err = bpf_close_dev(unit);
    return sceNet_lib_757085B0(ti, err);
}

int sceNet_B6FC0A5B(u32 unit, void *buf, u32 len)
{
    int err = 0;
    NetUio uio;
    NetIovec iov;
    NetThreadInfo *ti = sceNet_lib_CA3CF5EB(&err);
    if (ti == NULL)
        return -1;

    iov.iov_base = buf;
    iov.iov_len = len;
    uio.uio_iov = &iov;
    uio.uio_iovcnt = 1;
    uio.uio_offset = 0;
    uio.uio_resid = len;
    uio.uio_rw = 0;

    err = bpf_read_dev(unit, &uio);
    int transferred = (int)(len - uio.uio_resid);
    if (sceNet_lib_757085B0(ti, err) < 0)
        return -1;
    return transferred;
}

int sceNet_C431A214(u32 unit, void *buf, u32 len)
{
    int err = 0;
    NetUio uio;
    NetIovec iov;
    NetThreadInfo *ti = sceNet_lib_CA3CF5EB(&err);
    if (ti == NULL)
        return -1;

    iov.iov_base = buf;
    iov.iov_len = len;
    uio.uio_iov = &iov;
    uio.uio_iovcnt = 1;
    uio.uio_offset = 0;
    uio.uio_resid = len;
    uio.uio_rw = 1;

    err = bpf_write_dev(unit, &uio);
    int transferred = (int)(len - uio.uio_resid);
    if (sceNet_lib_757085B0(ti, err) < 0)
        return -1;
    return transferred;
}

int sceNet_E1F4696F(u32 unit, u32 cmd, void *arg)
{
    int err = 0;
    NetThreadInfo *ti = sceNet_lib_CA3CF5EB(&err);
    if (ti == NULL)
        return -1;
    err = bpf_ioctl_dev(unit, cmd, arg);
    return sceNet_lib_757085B0(ti, err);
}
