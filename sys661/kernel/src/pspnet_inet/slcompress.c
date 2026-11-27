#include "pspnet_inet_internal.h"

#define SL_TYPE_IP              0x40
#define SL_TYPE_UNCOMPRESSED    0x70
#define SL_TYPE_COMPRESSED      0x80

#define SL_NEW_C 0x40
#define SL_NEW_I 0x20
#define SL_NEW_P 0x10
#define SL_NEW_S 0x08
#define SL_NEW_A 0x04
#define SL_NEW_W 0x02
#define SL_NEW_U 0x01

#define SL_SPECIAL_I (SL_NEW_S | SL_NEW_W | SL_NEW_U)
#define SL_SPECIAL_D (SL_NEW_S | SL_NEW_A | SL_NEW_W | SL_NEW_U)
#define SL_SPECIALS_MASK (SL_NEW_S | SL_NEW_A | SL_NEW_W | SL_NEW_U)

static u8 *inetSlEncodeDelta(u8 *cp, u32 delta)
{
    u16 val = (u16)delta;
    if (val >= 256 || val == 0) {
        cp[0] = 0;
        cp[1] = (u8)(val >> 8);
        cp[2] = (u8)(val & 0xFF);
        return cp + 3;
    }
    cp[0] = (u8)val;
    return cp + 1;
}

static u32 inetSlDecodeDelta(const u8 **cpp)
{
    const u8 *cp = *cpp;
    if (cp[0] == 0) {
        u32 val = ((u32)cp[1] << 8) | cp[2];
        *cpp = cp + 3;
        return val;
    }
    *cpp = cp + 1;
    return cp[0];
}

static u16 inetReadBe16(const u8 *p)
{
    return (u16)(((u16)p[0] << 8) | p[1]);
}

static void inetWriteBe16(u8 *p, u16 val)
{
    p[0] = (u8)(val >> 8);
    p[1] = (u8)(val & 0xFF);
}

static u32 inetReadBe32(const u8 *p)
{
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
}

static void inetWriteBe32(u8 *p, u32 val)
{
    p[0] = (u8)(val >> 24);
    p[1] = (u8)(val >> 16);
    p[2] = (u8)(val >> 8);
    p[3] = (u8)(val & 0xFF);
}

/* [219] 0x010904 */
INET_NOINLINE u8 inetSlCompressTcp(InetMbuf *m, InetIpHdr *ip, InetSlCompress *comp, int compressCid)
{
    InetSlCstate *lastCs = comp->last_cs;
    InetSlCstate *cs = lastCs->cs_next;
    u32 hlen = (u32)(ip->ip_vhl & 0x0F);
    u32 ipHdrBytes = hlen * 4;
    u8 *ipBytes = (u8 *)ip;
    u8 *thBytes;
    u32 tcpHdrLen;
    u32 totalHdrLen;
    u8 deltaBuf[16];
    u8 *cp = deltaBuf;
    u32 changes = 0;
    u32 deltaS = 0;
    u32 deltaA = 0;
    int found = 0;
    int sendUncompressed = 0;

    if ((inetReadBe16(ipBytes + 6) & 0x3FFF) != 0) {
        return SL_TYPE_IP;
    }
    if (m->m_len < 40) {
        return SL_TYPE_IP;
    }
    thBytes = ipBytes + ipHdrBytes;
    if ((thBytes[13] & 0x17) != 0x10) {
        return SL_TYPE_IP;
    }

    comp->sls_packets++;

    {
        InetSlCstate *prev = lastCs;
        for (;;) {
            u8 *csIp = cs->cs_hdr;
            u32 csIpLen = (u32)(csIp[0] & 0x0F) * 4;
            if (inetReadU32At(ipBytes, 12) == inetReadU32At(csIp, 12) &&
                inetReadU32At(ipBytes, 16) == inetReadU32At(csIp, 16) &&
                inetReadU32At(thBytes, 0) == inetReadU32At(csIp, (s32)csIpLen)) {
                found = 1;
                if (prev != lastCs) {
                    if (cs == lastCs) {
                        comp->last_cs = prev;
                    } else {
                        prev->cs_next = cs->cs_next;
                        cs->cs_next = lastCs->cs_next;
                        lastCs->cs_next = cs;
                    }
                }
                break;
            }
            if (cs == lastCs) {
                comp->last_cs = prev;
                comp->sls_misses++;
                break;
            }
            comp->sls_searches++;
            prev = cs;
            cs = cs->cs_next;
        }
    }

    tcpHdrLen = (u32)(thBytes[12] >> 4);
    totalHdrLen = (hlen + tcpHdrLen) * 4;
    if ((u32)m->m_len < totalHdrLen) {
        return SL_TYPE_IP;
    }

    if (!found) {
        sendUncompressed = 1;
    } else {
        u8 *oldIp = cs->cs_hdr;
        u8 *oldTh = oldIp + ipHdrBytes;
        if (inetReadU16At(ipBytes, 0) != inetReadU16At(oldIp, 0) ||
            inetReadU16At(ipBytes, 6) != inetReadU16At(oldIp, 6) ||
            inetReadU16At(ipBytes, 8) != inetReadU16At(oldIp, 8) ||
            (thBytes[12] & 0xF0) != (oldTh[12] & 0xF0)) {
            sendUncompressed = 1;
        } else if (hlen > 5 && sceNet_lib_4BF83CF4(ipBytes + 20, oldIp + 20, (int)((hlen - 5) * 4)) != 0) {
            sendUncompressed = 1;
        } else if (tcpHdrLen > 5 && sceNet_lib_4BF83CF4(thBytes + 20, oldTh + 20, (int)((tcpHdrLen - 5) * 4)) != 0) {
            sendUncompressed = 1;
        }
    }

    if (!sendUncompressed) {
        u8 *oldIp = cs->cs_hdr;
        u8 *oldTh = oldIp + ipHdrBytes;

        if ((thBytes[13] & 0x20) != 0) {
            cp = inetSlEncodeDelta(cp, inetReadBe16(thBytes + 18));
            changes |= SL_NEW_U;
        } else if (inetReadBe16(thBytes + 18) != inetReadBe16(oldTh + 18)) {
            sendUncompressed = 1;
        }

        if (!sendUncompressed) {
            u16 deltaW = (u16)(inetReadBe16(thBytes + 14) - inetReadBe16(oldTh + 14));
            if (deltaW != 0) {
                cp = inetSlEncodeDelta(cp, deltaW);
                changes |= SL_NEW_W;
            }
        }

        if (!sendUncompressed) {
            deltaA = inetReadBe32(thBytes + 8) - inetReadBe32(oldTh + 8);
            if (deltaA != 0) {
                if (deltaA > 0xFFFF) {
                    sendUncompressed = 1;
                } else {
                    cp = inetSlEncodeDelta(cp, deltaA);
                    changes |= SL_NEW_A;
                }
            }
        }

        if (!sendUncompressed) {
            deltaS = inetReadBe32(thBytes + 4) - inetReadBe32(oldTh + 4);
            if (deltaS != 0) {
                if (deltaS > 0xFFFF) {
                    sendUncompressed = 1;
                } else {
                    cp = inetSlEncodeDelta(cp, deltaS);
                    changes |= SL_NEW_S;
                }
            }
        }

        if (!sendUncompressed) {
            u16 oldIpLen = inetReadBe16(oldIp + 2);
            switch (changes) {
            case 0:
                if (inetReadBe16(ipBytes + 2) != oldIpLen && oldIpLen == totalHdrLen) {
                    break;
                }
                sendUncompressed = 1;
                break;
            case SL_SPECIAL_I:
            case SL_SPECIAL_D:
                sendUncompressed = 1;
                break;
            case SL_NEW_S | SL_NEW_A:
                if (deltaS == deltaA && deltaS == (u32)(oldIpLen - totalHdrLen)) {
                    changes = SL_SPECIAL_I;
                    cp = deltaBuf;
                }
                break;
            case SL_NEW_S:
                if (deltaS == (u32)(oldIpLen - totalHdrLen)) {
                    changes = SL_SPECIAL_D;
                    cp = deltaBuf;
                }
                break;
            default:
                break;
            }
        }
    }

    if (sendUncompressed) {
        sceNet_lib_2F305274(ipBytes, cs->cs_hdr, (int)totalHdrLen);
        ipBytes[9] = cs->cs_id;
        comp->last_xmit = cs->cs_id;
        return SL_TYPE_UNCOMPRESSED;
    }

    {
        u8 *oldIp = cs->cs_hdr;
        u16 deltaId = (u16)(inetReadBe16(ipBytes + 4) - inetReadBe16(oldIp + 4));
        u16 tcpCksum = inetReadBe16(thBytes + 16);
        u32 deltaLen;
        u32 stripLen;
        u8 *hdrOut;

        if (deltaId != 1) {
            cp = inetSlEncodeDelta(cp, deltaId);
            changes |= SL_NEW_I;
        }
        if ((thBytes[13] & 0x08) != 0) {
            changes |= SL_NEW_P;
        }

        sceNet_lib_2F305274(ipBytes, cs->cs_hdr, (int)totalHdrLen);
        deltaLen = (u32)(cp - deltaBuf);

        if (compressCid != 0 && comp->last_xmit == cs->cs_id) {
            stripLen = totalHdrLen - deltaLen - 3;
            hdrOut = ipBytes + stripLen;
            hdrOut[0] = (u8)changes;
        } else {
            comp->last_xmit = cs->cs_id;
            stripLen = totalHdrLen - deltaLen - 4;
            ipBytes[stripLen] = (u8)(changes | SL_NEW_C);
            hdrOut = ipBytes + stripLen + 1;
            hdrOut[0] = cs->cs_id;
        }

        m->m_data += stripLen;
        m->m_len -= (s32)stripLen;
        hdrOut[1] = (u8)(tcpCksum >> 8);
        hdrOut[2] = (u8)(tcpCksum & 0xFF);
        sceNet_lib_2F305274(deltaBuf, hdrOut + 3, (int)deltaLen);
        comp->sls_compressed++;
        return SL_TYPE_COMPRESSED;
    }
}

/* [220] 0x010FA0 */
INET_NOINLINE int inetSlUncompressTcp(u8 *buf, u32 len, u16 totalLen, int type,
                                      InetSlCompress *comp, u8 **outHdr, u32 *outHdrLen)
{
    if (type == SL_TYPE_UNCOMPRESSED) {
        u32 cid = buf[9];
        u32 ipHdrLen;
        u32 fullHdrLen;
        InetSlCstate *cs;

        if (cid >= 16) {
            comp->sls_errorin++;
            comp->flags |= 1;
            return -1;
        }
        comp->last_recv = (u8)cid;
        comp->flags &= (u16)~1;
        buf[9] = INET_IPPROTO_TCP;

        ipHdrLen = (u32)(buf[0] & 0x0F) * 4;
        if (len < ipHdrLen + 20) {
            comp->sls_errorin++;
            comp->flags |= 1;
            return -1;
        }
        fullHdrLen = ipHdrLen + (u32)(buf[ipHdrLen + 12] >> 4) * 4;
        if (fullHdrLen >= 237 || len < fullHdrLen) {
            comp->sls_errorin++;
            comp->flags |= 1;
            return -1;
        }
        cs = &comp->rstate[cid];
        sceNet_lib_2F305274(buf, cs->cs_hdr, (int)fullHdrLen);
        cs->cs_hlen = (u16)fullHdrLen;
        comp->sls_uncompressedin++;
        *outHdr = cs->cs_hdr;
        *outHdrLen = fullHdrLen;
        return 0;
    }

    if (type == SL_TYPE_COMPRESSED) {
        const u8 *cp = buf;
        u8 changes = *cp++;
        InetSlCstate *cs;
        u8 *ipBytes;
        u32 ipHdrLen;
        u8 *thBytes;
        u32 consumed;

        comp->sls_compressedin++;
        if ((changes & SL_NEW_C) != 0) {
            u8 cid = *cp++;
            if (cid >= 16) {
                comp->sls_errorin++;
                comp->flags |= 1;
                return -1;
            }
            comp->flags &= (u16)~1;
            comp->last_recv = cid;
        } else if ((comp->flags & 1) != 0) {
            comp->sls_tossed++;
            return -1;
        }

        cs = &comp->rstate[comp->last_recv];
        ipBytes = cs->cs_hdr;
        ipHdrLen = (u32)(ipBytes[0] & 0x0F) * 4;
        thBytes = ipBytes + ipHdrLen;

        thBytes[16] = cp[0];
        thBytes[17] = cp[1];
        cp += 2;

        if ((changes & SL_NEW_P) != 0) {
            thBytes[13] |= 0x08;
        } else {
            thBytes[13] &= (u8)~0x08;
        }

        switch (changes & SL_SPECIALS_MASK) {
        case SL_SPECIAL_I: {
            u32 delta = (u32)(inetReadBe16(ipBytes + 2) - cs->cs_hlen);
            inetWriteBe32(thBytes + 8, inetReadBe32(thBytes + 8) + delta);
            inetWriteBe32(thBytes + 4, inetReadBe32(thBytes + 4) + delta);
            break;
        }
        case SL_SPECIAL_D: {
            u32 delta = (u32)(inetReadBe16(ipBytes + 2) - cs->cs_hlen);
            inetWriteBe32(thBytes + 4, inetReadBe32(thBytes + 4) + delta);
            break;
        }
        default:
            if ((changes & SL_NEW_U) != 0) {
                thBytes[13] |= 0x20;
                inetWriteBe16(thBytes + 18, (u16)inetSlDecodeDelta(&cp));
            } else {
                thBytes[13] &= (u8)~0x20;
            }
            if ((changes & SL_NEW_W) != 0) {
                u16 win = (u16)(inetReadBe16(thBytes + 14) + inetSlDecodeDelta(&cp));
                inetWriteBe16(thBytes + 14, win);
            }
            if ((changes & SL_NEW_A) != 0) {
                u32 ack = inetReadBe32(thBytes + 8) + inetSlDecodeDelta(&cp);
                inetWriteBe32(thBytes + 8, ack);
            }
            if ((changes & SL_NEW_S) != 0) {
                u32 seq = inetReadBe32(thBytes + 4) + inetSlDecodeDelta(&cp);
                inetWriteBe32(thBytes + 4, seq);
            }
            break;
        }

        if ((changes & SL_NEW_I) != 0) {
            u16 idVal = (u16)(inetReadBe16(ipBytes + 4) + inetSlDecodeDelta(&cp));
            inetWriteBe16(ipBytes + 4, idVal);
        } else {
            inetWriteBe16(ipBytes + 4, (u16)(inetReadBe16(ipBytes + 4) + 1));
        }

        consumed = (u32)(cp - buf);
        if ((s32)(len - consumed) < 0) {
            comp->sls_errorin++;
            comp->flags |= 1;
            return -1;
        }

        ipBytes[10] = 0;
        ipBytes[11] = 0;
        inetWriteBe16(ipBytes + 2, (u16)(totalLen + cs->cs_hlen - consumed));

        {
            const u16 *words = (const u16 *)ipBytes;
            u32 sum = 0;
            u32 rem = ipHdrLen;
            while (rem != 0) {
                sum += *words++;
                rem -= 2;
            }
            sum = (sum & 0xFFFF) + (sum >> 16);
            sum += (sum >> 16);
            inetWriteU16At(ipBytes, 10, (u16)(~sum));
        }

        *outHdr = ipBytes;
        *outHdrLen = cs->cs_hlen;
        return (int)consumed;
    }

    comp->sls_errorin++;
    comp->flags |= 1;
    return -1;
}

/* [221] 0x011544 */
INET_NOINLINE void inetSlCompressInit(InetSlCompress *comp, int maxSlot)
{
    int idx;
    if (maxSlot == -1) {
        sceKernelMemset(comp, 0, sizeof(InetSlCompress));
        maxSlot = 15;
    } else {
        sceKernelMemset(comp->tstate, 0, sizeof(comp->tstate));
        sceKernelMemset(comp->rstate, 0, sizeof(comp->rstate));
    }

    for (idx = maxSlot; idx > 0; idx--) {
        comp->tstate[idx].cs_id = (u8)idx;
        comp->tstate[idx].cs_next = &comp->tstate[idx - 1];
    }
    comp->tstate[0].cs_id = 0;
    comp->tstate[0].cs_next = &comp->tstate[maxSlot];
    comp->flags = 1;
    comp->last_cs = &comp->tstate[0];
    comp->last_recv = 0xFF;
    comp->last_xmit = 0xFF;
}
