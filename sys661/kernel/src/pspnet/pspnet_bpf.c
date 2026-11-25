#include "pspnet_internal.h"

#define NBPFILTER 8
#define BPF_MAXINSNS 512
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

typedef struct BpfIf {
    struct BpfIf *bif_next;
    struct BpfDesc *bif_dlist;
    void **bif_driverp;
    u32 bif_dlt;
    u32 bif_hdrlen;
    void *bif_ifp;
} BpfIf;

typedef struct BpfDesc {
    struct BpfDesc *bd_next;     /* 0x00 (0) */
    u8 *bd_sbuf;                 /* 0x04 (1) */
    u8 *bd_hbuf;                 /* 0x08 (2) */
    u8 *bd_fbuf;                 /* 0x0C (3) */
    u32 bd_slen;                 /* 0x10 (4) */
    u32 bd_hlen;                 /* 0x14 (5) */
    u32 bd_bufsize;              /* 0x18 (6) */
    BpfIf *bd_bif;               /* 0x1C (7) */
    s32 bd_rtout;                /* 0x20 (8) */
    BpfInsn *bd_filter;          /* 0x24 (9) */
    u32 bd_rcount;               /* 0x28 (10) */
    u32 bd_dcount;               /* 0x2C (11) */
    u8 bd_promisc;               /* 0x30 */
    u8 bd_state;                 /* 0x31 */
    u8 bd_immediate;             /* 0x32 */
    u8 bd_pad;                   /* 0x33 */
    u32 bd_hdrcmplt;             /* 0x34 (13) */
    SceUID bd_sel_thid;          /* 0x38 (14) */
    u32 bd_reserved;             /* 0x3C (15) */
} BpfDesc;

static u32 g_bpf_bufsize = 0x2000;
static BpfIf *g_bpf_iflist = NULL;
static BpfDesc g_bpf_dtab[NBPFILTER];

static u32 m_xword(void *m0, u32 k, int *err)
{
    u32 *m = (u32 *)m0;
    u32 len = m[3];
    while (k >= len) {
        k -= len;
        m = (u32 *)m[0];
        if (m == NULL) {
            *err = 1;
            return 0;
        }
        len = m[3];
    }
    u8 *cp = (u8 *)m[2] + k;
    u32 rem = len - k;
    if (rem >= 4) {
        *err = 0;
        return ((u32)cp[0] << 24) | ((u32)cp[1] << 16) | ((u32)cp[2] << 8) | cp[3];
    }
    u32 *m2 = (u32 *)m[0];
    if (m2 != NULL && m2[3] + rem >= 4) {
        u8 *cp2 = (u8 *)m2[2];
        *err = 0;
        if (rem == 1)
            return ((u32)cp[0] << 24) | ((u32)cp2[0] << 16) | ((u32)cp2[1] << 8) | cp2[2];
        if (rem == 2)
            return ((u32)cp[0] << 24) | ((u32)cp[1] << 16) | ((u32)cp2[0] << 8) | cp2[1];
        return ((u32)cp[0] << 24) | ((u32)cp[1] << 16) | ((u32)cp[2] << 8) | cp2[0];
    }
    *err = 1;
    return 0;
}

static u32 m_xhalf(void *m0, u32 k, int *err)
{
    u32 *m = (u32 *)m0;
    u32 len = m[3];
    while (k >= len) {
        k -= len;
        m = (u32 *)m[0];
        if (m == NULL) {
            *err = 1;
            return 0;
        }
        len = m[3];
    }
    u8 *cp = (u8 *)m[2] + k;
    if (len - k < 2) {
        u32 *m2 = (u32 *)m[0];
        if (m2 != NULL) {
            *err = 0;
            return ((u32)cp[0] << 8) | ((u8 *)m2[2])[0];
        }
        *err = 1;
        return 0;
    }
    *err = 0;
    return ((u32)cp[0] << 8) | cp[1];
}

static int bpf_validate(const BpfInsn *f, int len)
{
    int i;
    for (i = 0; i < len; i++) {
        const BpfInsn *p = &f[i];
        if ((p->code & 7) == 5) {
            if ((p->code & 0xF0) != 0) {
                if (i + 1 + p->jt >= len || i + 1 + p->jf >= len)
                    return 0;
            } else {
                s32 k = (s32)p->k;
                if (k < 0 || i + 1 + k >= len || i + 1 + k < 0)
                    return 0;
            }
        }
        if ((p->code & 7) == 2 || (p->code & 0xE7) == 0x60) {
            if (p->k >= 16)
                return 0;
        }
        if (p->code == 0x34 && p->k == 0)
            return 0;
    }
    return (f[len - 1].code & 7) == 6;
}

static u32 bpf_filter(const BpfInsn *pc, void *p, u32 wirelen, u32 buflen)
{
    u32 A = 0, X = 0;
    u32 mem[16];
    int merr;

    if (pc == NULL)
        return (u32)-1;

    pc--;
    for (;;) {
        pc++;
        switch (pc->code) {
        case 0x00: A = pc->k; continue;
        case 0x01: X = pc->k; continue;
        case 0x02: mem[pc->k] = A; continue;
        case 0x03: mem[pc->k] = X; continue;
        case 0x04: A += pc->k; continue;
        case 0x05: pc += pc->k; continue;
        case 0x06: return pc->k;
        case 0x07: X = A; continue;
        case 0x0C: A += X; continue;
        case 0x14: A -= pc->k; continue;
        case 0x15: pc += (A == pc->k) ? pc->jt : pc->jf; continue;
        case 0x16: return A;
        case 0x1C: A -= X; continue;
        case 0x1D: pc += (A == X) ? pc->jt : pc->jf; continue;
        case 0x20: {
            u32 k = pc->k;
            if (k + 4 <= buflen) {
                u8 *cp = (u8 *)p + k;
                A = ((u32)cp[0] << 24) | ((u32)cp[1] << 16) | ((u32)cp[2] << 8) | cp[3];
            } else {
                if (buflen != 0)
                    return 0;
                merr = 0;
                A = m_xword(p, k, &merr);
                if (merr != 0)
                    return 0;
            }
            continue;
        }
        case 0x24: A *= pc->k; continue;
        case 0x25: pc += (A > pc->k) ? pc->jt : pc->jf; continue;
        case 0x28: {
            u32 k = pc->k;
            if (k + 2 <= buflen) {
                u8 *cp = (u8 *)p + k;
                A = ((u32)cp[0] << 8) | cp[1];
            } else {
                if (buflen != 0)
                    return 0;
                merr = 0;
                A = m_xhalf(p, k, &merr);
            }
            continue;
        }
        case 0x2C: A *= X; continue;
        case 0x2D: pc += (A > X) ? pc->jt : pc->jf; continue;
        case 0x30: {
            s32 k = (s32)pc->k;
            if ((u32)k < buflen) {
                A = ((u8 *)p)[k];
            } else {
                if (buflen != 0)
                    return 0;
                u32 *m = (u32 *)p;
                while (k >= (s32)m[3]) {
                    k -= (s32)m[3];
                    m = (u32 *)m[0];
                    if (m == NULL)
                        return 0;
                }
                A = ((u8 *)m[2])[k];
            }
            continue;
        }
        case 0x34: A /= pc->k; continue;
        case 0x35: pc += (A >= pc->k) ? pc->jt : pc->jf; continue;
        case 0x3C:
            if (X == 0)
                return 0;
            A /= X;
            continue;
        case 0x3D: pc += (A >= X) ? pc->jt : pc->jf; continue;
        case 0x40: {
            u32 k = X + pc->k;
            if (k + 4 <= buflen) {
                u8 *cp = (u8 *)p + k;
                A = ((u32)cp[0] << 24) | ((u32)cp[1] << 16) | ((u32)cp[2] << 8) | cp[3];
            } else {
                if (buflen != 0)
                    return 0;
                merr = 0;
                A = m_xword(p, k, &merr);
                if (merr != 0)
                    return 0;
            }
            continue;
        }
        case 0x44: A |= pc->k; continue;
        case 0x45: pc += (A & pc->k) ? pc->jt : pc->jf; continue;
        case 0x48: {
            u32 k = X + pc->k;
            if (k + 2 <= buflen) {
                u8 *cp = (u8 *)p + k;
                A = ((u32)cp[0] << 8) | cp[1];
            } else {
                if (buflen != 0)
                    return 0;
                merr = 0;
                A = m_xhalf(p, k, &merr);
                if (merr != 0)
                    return 0;
            }
            continue;
        }
        case 0x4C: A |= X; continue;
        case 0x4D: pc += (A & X) ? pc->jt : pc->jf; continue;
        case 0x50: {
            s32 k = (s32)(X + pc->k);
            if ((u32)k < buflen) {
                A = ((u8 *)p)[k];
            } else {
                if (buflen != 0)
                    return 0;
                u32 *m = (u32 *)p;
                while (k >= (s32)m[3]) {
                    k -= (s32)m[3];
                    m = (u32 *)m[0];
                    if (m == NULL)
                        return 0;
                }
                A = ((u8 *)m[2])[k];
            }
            continue;
        }
        case 0x54: A &= pc->k; continue;
        case 0x5C: A &= X; continue;
        case 0x60: A = mem[pc->k]; continue;
        case 0x61: X = mem[pc->k]; continue;
        case 0x64: A <<= pc->k; continue;
        case 0x6C: A <<= X; continue;
        case 0x74: A >>= pc->k; continue;
        case 0x7C: A >>= X; continue;
        case 0x80: A = wirelen; continue;
        case 0x81: X = wirelen; continue;
        case 0x84: A = (u32)(-(s32)A); continue;
        case 0x87: A = X; continue;
        case 0xB1: {
            s32 k = (s32)pc->k;
            if ((u32)k < buflen) {
                X = 4 * (((u8 *)p)[k] & 0xF);
            } else {
                if (buflen != 0)
                    return 0;
                u32 *m = (u32 *)p;
                while (k >= (s32)m[3]) {
                    k -= (s32)m[3];
                    m = (u32 *)m[0];
                    if (m == NULL)
                        return 0;
                }
                X = 4 * (((u8 *)m[2])[k] & 0xF);
            }
            continue;
        }
        default:
            return 0;
        }
    }
}

static void *bpf_mcopy(void *dst, void *m0, u32 len)
{
    u8 *dp = (u8 *)dst;
    u32 *m = (u32 *)m0;
    while (len != 0) {
        u32 count = m[3];
        if (count > len)
            count = len;
        sceKernelMemcpy(dp, (void *)m[2], count);
        dp += count;
        len -= count;
        m = (u32 *)m[0];
    }
    return dst;
}

static void bpf_catchpacket(BpfDesc *d, void *pkt, u32 pktlen, u32 snaplen,
                            void *(*cpfn)(void *, void *, u32))
{
    u32 curlen, hdrlen, totlen;
    u32 tv[2];

    if (snaplen > pktlen)
        snaplen = pktlen;

    hdrlen = d->bd_bif->bif_hdrlen;
    totlen = hdrlen + snaplen;
    if (totlen > d->bd_bufsize)
        totlen = d->bd_bufsize;

    curlen = (d->bd_slen + 3) & ~3u;
    if (curlen + totlen > d->bd_bufsize) {
        if (d->bd_fbuf == NULL) {
            d->bd_dcount++;
            return;
        }
        d->bd_hbuf = d->bd_sbuf;
        d->bd_hlen = d->bd_slen;
        d->bd_sbuf = d->bd_fbuf;
        d->bd_fbuf = NULL;
        d->bd_slen = 0;
        curlen = 0;
        sceNet_lib_7574FDA1((u32)d);
        sceNet_lib_B9C780C7(&d->bd_sel_thid);
    } else if (d->bd_immediate) {
        sceNet_lib_7574FDA1((u32)d);
        sceNet_lib_B9C780C7(&d->bd_sel_thid);
    }

    u8 *hp = d->bd_sbuf + curlen;
    sceNet_lib_4B2B3416(tv);
    *(u32 *)(hp + 0) = tv[0];
    *(u32 *)(hp + 4) = tv[1];
    *(u32 *)(hp + 8) = totlen - hdrlen;
    *(u32 *)(hp + 12) = pktlen;
    *(u16 *)(hp + 16) = (u16)hdrlen;
    cpfn(hp + hdrlen, pkt, totlen - hdrlen);
    d->bd_slen = curlen + totlen;
}

void bpf_mtap(void *bp_arg, void *m0)
{
    BpfIf *bp = (BpfIf *)bp_arg;
    u32 *m = (u32 *)m0;
    u32 pktlen = 0;
    while (m != NULL) {
        pktlen += m[3];
        m = (u32 *)m[0];
    }
    int s = sceNet_lib_94B44F26();
    BpfDesc *d = bp->bif_dlist;
    while (d != NULL) {
        d->bd_rcount++;
        u32 slen = bpf_filter(d->bd_filter, m0, pktlen, 0);
        if (slen != 0)
            bpf_catchpacket(d, m0, pktlen, slen, bpf_mcopy);
        d = d->bd_next;
    }
    sceNet_lib_515B2F33(s);
}

void bpf_attach(void **bp_ptr, void *ifp, u32 dlt, u32 hdrlen)
{
    BpfIf *bp = (BpfIf *)sceNetIfhandle_lib_15CFE3C0(sizeof(BpfIf));
    bp->bif_next = g_bpf_iflist;
    g_bpf_iflist = bp;
    bp->bif_dlist = NULL;
    bp->bif_driverp = bp_ptr;
    bp->bif_dlt = dlt;
    bp->bif_hdrlen = ((hdrlen + 23) & ~3u) - hdrlen;
    bp->bif_ifp = ifp;
    *bp_ptr = NULL;
}

static void bpf_detachd(BpfDesc *d)
{
    BpfIf *bp = d->bd_bif;
    BpfDesc **p = &bp->bif_dlist;
    while (*p != d)
        p = &(*p)->bd_next;
    *p = d->bd_next;
    if (bp->bif_dlist == NULL)
        *bp->bif_driverp = NULL;
    d->bd_bif = NULL;
}

void bpf_detach(void *ifp)
{
    int i;
    for (i = 0; i < NBPFILTER; i++) {
        BpfDesc *d = &g_bpf_dtab[i];
        if (d->bd_next != d && d->bd_bif != NULL && d->bd_bif->bif_ifp == ifp) {
            int s = sceNet_lib_94B44F26();
            bpf_detachd(d);
            sceNet_lib_515B2F33(s);
        }
    }
    BpfIf **p = &g_bpf_iflist;
    while (*p != NULL) {
        if ((*p)->bif_ifp == ifp) {
            BpfIf *bp = *p;
            *p = bp->bif_next;
            sceNetIfhandle_lib_76BAD213(bp);
            return;
        }
        p = &(*p)->bif_next;
    }
}

void bpf_init(int n)
{
    int i;
    (void)n;
    g_bpf_bufsize = 0x2000;
    g_bpf_iflist = NULL;
    for (i = 0; i < NBPFILTER; i++)
        g_bpf_dtab[i].bd_next = &g_bpf_dtab[i];
}

static int bpf_allocbufs(BpfDesc *d)
{
    d->bd_fbuf = (u8 *)sceNetIfhandle_lib_15CFE3C0(d->bd_bufsize);
    if (d->bd_fbuf == NULL)
        return 12;
    d->bd_sbuf = (u8 *)sceNetIfhandle_lib_15CFE3C0(d->bd_bufsize);
    if (d->bd_sbuf == NULL) {
        sceNetIfhandle_lib_76BAD213(d->bd_fbuf);
        return 12;
    }
    d->bd_slen = 0;
    d->bd_hlen = 0;
    return 0;
}

static void bpf_ifname(void *ifp, void *ifr)
{
    sceKernelMemcpy(ifr, (u8 *)ifp + 20, 16);
}

static int bpf_setif(BpfDesc *d, const char *ifr_name)
{
    BpfIf *bp = g_bpf_iflist;
    while (bp != NULL) {
        if (bp->bif_ifp != NULL && sceNet_lib_A0F16ABD((const char *)bp->bif_ifp + 20, ifr_name) == 0)
            break;
        bp = bp->bif_next;
    }
    if (bp == NULL)
        return 6;

    if (d->bd_sbuf == NULL) {
        int err = bpf_allocbufs(d);
        if (err != 0)
            return err;
    }

    int s = sceNet_lib_94B44F26();
    if (bp != d->bd_bif) {
        if (d->bd_bif != NULL)
            bpf_detachd(d);
        d->bd_bif = bp;
        d->bd_next = bp->bif_dlist;
        bp->bif_dlist = d;
        *bp->bif_driverp = bp;
    }
    if (d->bd_hbuf != NULL) {
        d->bd_fbuf = d->bd_hbuf;
        d->bd_hbuf = NULL;
    }
    d->bd_slen = 0;
    d->bd_hlen = 0;
    d->bd_rcount = 0;
    d->bd_dcount = 0;
    sceNet_lib_515B2F33(s);
    return 0;
}

static int bpf_setf(BpfDesc *d, BpfProgram *fp)
{
    BpfInsn *old = d->bd_filter;
    int s;

    if (fp->bf_insns == NULL) {
        if (fp->bf_len != 0)
            return 22;
        s = sceNet_lib_94B44F26();
        d->bd_filter = NULL;
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
        s = sceNet_lib_94B44F26();
        d->bd_filter = fcode;
    }

    if (d->bd_hbuf != NULL) {
        d->bd_fbuf = d->bd_hbuf;
        d->bd_hbuf = NULL;
    }
    d->bd_slen = 0;
    d->bd_hlen = 0;
    d->bd_rcount = 0;
    d->bd_dcount = 0;
    sceNet_lib_515B2F33(s);

    if (old != NULL)
        sceNetIfhandle_lib_76BAD213(old);
    return 0;
}

static int bpf_open_dev(u32 unit)
{
    if (unit >= NBPFILTER)
        return 6;
    BpfDesc *d = &g_bpf_dtab[unit];
    if (d->bd_next != d)
        return 16;
    sceKernelMemset(d, 0, sizeof(BpfDesc));
    d->bd_bufsize = g_bpf_bufsize;
    return 0;
}

static int bpf_close_dev(u32 unit)
{
    BpfDesc *d = &g_bpf_dtab[unit];
    int s = sceNet_lib_94B44F26();
    if (d->bd_bif != NULL)
        bpf_detachd(d);
    sceNet_lib_515B2F33(s);

    if (d->bd_sbuf != NULL) {
        sceNetIfhandle_lib_76BAD213(d->bd_sbuf);
        if (d->bd_hbuf != NULL)
            sceNetIfhandle_lib_76BAD213(d->bd_hbuf);
        if (d->bd_fbuf != NULL)
            sceNetIfhandle_lib_76BAD213(d->bd_fbuf);
    }
    if (d->bd_filter != NULL)
        sceNetIfhandle_lib_76BAD213(d->bd_filter);
    d->bd_next = d;
    return 0;
}

static int bpf_read_dev(u32 unit, NetUio *uio)
{
    BpfDesc *d = &g_bpf_dtab[unit];
    int err;

    if (uio->uio_resid != d->bd_bufsize)
        return 22;

    int s = sceNet_lib_94B44F26();
    while (d->bd_hbuf == NULL) {
        if (d->bd_immediate && d->bd_slen != 0) {
            d->bd_hbuf = d->bd_sbuf;
            d->bd_hlen = d->bd_slen;
            d->bd_sbuf = d->bd_fbuf;
            d->bd_fbuf = NULL;
            d->bd_slen = 0;
            break;
        }
        err = 11;
        if (d->bd_rtout != -1)
            err = sceNet_lib_205E8D17((u32)d, (u32)d->bd_rtout);
        if (err == 11) {
            if (d->bd_hbuf != NULL)
                break;
            if (d->bd_slen == 0) {
                sceNet_lib_515B2F33(s);
                return 0;
            }
            d->bd_hbuf = d->bd_sbuf;
            d->bd_hlen = d->bd_slen;
            d->bd_sbuf = d->bd_fbuf;
            d->bd_fbuf = NULL;
            d->bd_slen = 0;
            break;
        }
        if (err != 0) {
            sceNet_lib_515B2F33(s);
            return err;
        }
    }

    sceNet_lib_515B2F33(s);
    err = sceNet_lib_2B42872F(d->bd_hbuf, (int)d->bd_hlen, uio);
    s = sceNet_lib_94B44F26();
    d->bd_fbuf = d->bd_hbuf;
    d->bd_hbuf = NULL;
    d->bd_hlen = 0;
    sceNet_lib_515B2F33(s);
    return err;
}

static int bpf_write_dev(u32 unit, NetUio *uio)
{
    BpfDesc *d = &g_bpf_dtab[unit];
    if (d->bd_bif == NULL)
        return 6;

    void *ifp = d->bd_bif->bif_ifp;
    u32 len = uio->uio_resid;
    if (len == 0)
        return 0;

    u32 *m = (u32 *)sceNetIfhandle_lib_15CFE3C0(256);
    if (m == NULL)
        return 105;

    *(u16 *)((u8 *)m + 16) = 1;
    m[2] = (u32)((u8 *)m + 48);
    *(u16 *)((u8 *)m + 18) = 2;
    m[0] = 0;
    m[1] = 0;
    m[7] = 0;
    m[5] = 0;
    m[6] = len;

    if (len >= 209) {
        void *cl = sceNetIfhandle_lib_15CFE3C0(2048);
        if (cl == NULL)
            cl = sceNetIfhandle_lib_15CFE3C0(2048);
        m[12] = (u32)cl;
        if (cl != NULL) {
            m[2] = (u32)cl;
            *(u16 *)((u8 *)m + 18) |= 9;
            m[15] = 2048;
            m[13] = 0;
            m[14] = 0;
            m[18] = (u32)m;
            m[17] = (u32)m;
        }
        if ((*(u16 *)((u8 *)m + 18) & 1) == 0) {
            sceNetIfhandle_lib_F56FAC82(m);
            return 105;
        }
    }

    int err = sceNet_lib_2B42872F((void *)m[2], (int)len, uio);
    if (err != 0) {
        sceNetIfhandle_lib_F56FAC82(m);
        return err;
    }

    m[3] = len;
    int s = sceNet_lib_94B44F26();
    u32 *ifq = (u32 *)ifp;
    if ((s32)ifq[53] >= (s32)ifq[54]) {
        sceNet_lib_515B2F33(s);
        sceNetIfhandle_lib_F56FAC82(m);
        return 105;
    }
    *(u64 *)((u8 *)ifp + 136) += m[6];
    m[1] = 0;
    if (ifq[52] != 0)
        ((u32 *)ifq[52])[1] = (u32)m;
    else
        ifq[51] = (u32)m;
    ifq[52] = (u32)m;
    ifq[53]++;
    if ((*(u16 *)((u8 *)ifp + 48) & 0x400) == 0) {
        void (*if_start)(void *) = *(void (**)(void *))((u8 *)ifp + 184);
        if_start(ifp);
    }
    sceNet_lib_515B2F33(s);
    return 0;
}

static int bpf_ioctl_dev(u32 unit, u32 cmd, void *addr)
{
    BpfDesc *d = &g_bpf_dtab[unit];

    switch (cmd) {
    case 0x40044266u: /* BIOCGBLEN */
        *(u32 *)addr = d->bd_bufsize;
        return 0;
    case 0xC0044266u: /* BIOCSBLEN */
        if (d->bd_bif != NULL)
            return 22;
        {
            u32 sz = *(u32 *)addr;
            if (sz > BPF_MAXBUFSIZE)
                sz = BPF_MAXBUFSIZE;
            else if (sz < BPF_MINBUFSIZE)
                sz = BPF_MINBUFSIZE;
            *(u32 *)addr = sz;
            d->bd_bufsize = sz;
        }
        return 0;
    case 0x80084267u: /* BIOCSETF */
        return bpf_setf(d, (BpfProgram *)addr);
    case 0x20004268u: /* BIOCFLUSH */ {
        int s = sceNet_lib_94B44F26();
        if (d->bd_hbuf != NULL) {
            d->bd_fbuf = d->bd_hbuf;
            d->bd_hbuf = NULL;
        }
        d->bd_slen = 0;
        d->bd_hlen = 0;
        d->bd_rcount = 0;
        d->bd_dcount = 0;
        sceNet_lib_515B2F33(s);
        return 0;
    }
    case 0x4004426Au: /* BIOCGDLT */
        if (d->bd_bif == NULL)
            return 22;
        *(u32 *)addr = d->bd_bif->bif_dlt;
        return 0;
    case 0x4020426Bu: /* BIOCGETIF */
        if (d->bd_bif == NULL)
            return 22;
        bpf_ifname(d->bd_bif->bif_ifp, addr);
        return 0;
    case 0x8020426Cu: /* BIOCSETIF */
        return bpf_setif(d, (const char *)addr);
    case 0x8008426Du: /* BIOCSRTIMEOUT */ {
        u32 *tv = (u32 *)addr;
        u32 ticks = tv[0] * 100 + tv[1] / 10000;
        if (ticks == 0 && tv[1] != 0)
            ticks = 1;
        d->bd_rtout = (s32)ticks;
        return 0;
    }
    case 0x4008426Eu: /* BIOCGRTIMEOUT */ {
        u32 *tv = (u32 *)addr;
        u32 ticks = (u32)d->bd_rtout;
        tv[0] = ticks / 100;
        tv[1] = (ticks % 100) * 10000;
        return 0;
    }
    case 0x4008426Fu: /* BIOCGSTATS */ {
        u32 *st = (u32 *)addr;
        st[0] = d->bd_rcount;
        st[1] = d->bd_dcount;
        return 0;
    }
    case 0x80044270u: /* BIOCIMMEDIATE */
        d->bd_immediate = *(u8 *)addr;
        return 0;
    case 0x40044271u: /* BIOCVERSION */
        ((u16 *)addr)[0] = 1;
        ((u16 *)addr)[1] = 1;
        return 0;
    case 0x40044274u: /* BIOCGHDRCMPLT */
        *(u32 *)addr = d->bd_hdrcmplt;
        return 0;
    case 0x80044275u: /* BIOCSHDRCMPLT */
        d->bd_hdrcmplt = (*(u32 *)addr != 0) ? 1 : 0;
        return 0;
    case 0x8004667Eu: /* FIONBIO */
        d->bd_rtout = (*(u32 *)addr != 0) ? -1 : 0;
        return 0;
    case 0x4004667Fu: /* FIONREAD */ {
        int s = sceNet_lib_94B44F26();
        u32 n = d->bd_slen;
        if (d->bd_hbuf != NULL)
            n += d->bd_hlen;
        sceNet_lib_515B2F33(s);
        *(u32 *)addr = n;
        return 0;
    }
    default:
        return 22;
    }
}

static int bpf_poll_dev(u32 unit, int events, NetThreadInfo *ti)
{
    BpfDesc *d = &g_bpf_dtab[unit];
    int revents = 0;
    int s = sceNet_lib_94B44F26();
    if (events & 0x41) {
        if (d->bd_hlen != 0 || (d->bd_immediate && d->bd_slen != 0))
            revents = events & 0x41;
        else
            sceNet_lib_92633D8D(ti, &d->bd_sel_thid);
    }
    sceNet_lib_515B2F33(s);
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
    if (timeout_ms != -1) {
        if ((u32)timeout_ms > 4294967u) {
            err = 22;
            goto finish;
        }
        timeout_us = (u32)timeout_ms * 1000u;
    }

    u32 rem_us = timeout_us;
    for (;;) {
        int s = sceNet_lib_94B44F26();
        err = bpf_poll_dev(unit, events, ti);
        if (err != 0) {
            sceNet_lib_515B2F33(s);
            break;
        }
        if (timeout_ms != -1) {
            if (start_time == 0)
                start_time = sceKernelGetSystemTimeWide();
            s64 now = sceKernelGetSystemTimeWide();
            if ((u64)(now - start_time) >= (u64)timeout_us) {
                sceNet_lib_515B2F33(s);
                break;
            }
            rem_us = timeout_us - (u32)(now - start_time);
        }
        err = sceNet_lib_F6DB0A0B((u32)&g_selwait, rem_us);
        sceNet_lib_515B2F33(s);
        if (err != 0)
            break;
    }

finish:
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
