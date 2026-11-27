/*
 * Public SceNetInet POSIX/BSD socket APIs, Kermit fast-path wrappers,
 * select/poll helpers, and sysctl marshaling.
 */
#include "pspnet_inet_internal.h"

#define INET_KERMIT_FALLBACK 9999

static const u32 g_select_poll_flags[3] = { 0x58U, 0x1CU, 0x80U };
static char g_ntoa_static_buf[18];

static int INET_NOINLINE inetSockargs(InetMbuf **mp, const void *buf, u32 buflen, int type);
static int INET_NOINLINE inetSendit(InetThreadInfo *ti, int sock, const InetMsghdr *mp, u32 flags, s32 *retsize);
static int INET_NOINLINE inetRecvit(InetThreadInfo *ti, int sock, InetMsghdr *mp, u8 *namelenp, s32 *retsize);
static int INET_NOINLINE inetKermitSend(InetThreadInfo *ti, int sock, const void *buf, s32 len,
                                        u32 flags, const void *to, u32 tolen, s32 *retsize);
static int INET_NOINLINE inetKermitRecv(InetThreadInfo *ti, int sock, void *buf, s32 len,
                                        u32 flags, void *from, u32 *fromlen, s32 *retsize);
static int INET_NOINLINE inetSelscan(InetThreadInfo *ti, const u32 *ibits, u32 *obits, int nfd, s32 *retval);
static void INET_NOINLINE inetSelTrack(InetThreadInfo *ti, const u32 *bits, int nfd, int detach);
static void INET_NOINLINE inetSelAttach(InetThreadInfo *ti, const u32 *bits, int nfd);
static void INET_NOINLINE inetSelDetach(InetThreadInfo *ti, const u32 *bits, int nfd);
static int INET_NOINLINE inetPollscan(InetThreadInfo *ti, InetPollfd *fds, int nfd, s32 *retval);
static void INET_NOINLINE inetPollAttach(InetThreadInfo *ti, const InetPollfd *fds, int nfd);
static void INET_NOINLINE inetPollDetach(InetThreadInfo *ti, const InetPollfd *fds, int nfd);

int sceNetInetBind(int sock, const void *addr, u32 addrLen)
{
    int error = 0;
    InetThreadInfo *ti;
    InetSocket *so;

    if (sceKernelCheckThreadStack() < 4048) {
        return -1;
    }
    ti = sceNet_lib_CA3CF5EB(&error);
    if (ti == NULL) {
        return -1;
    }

    so = inetSockFind(sock);
    if (so == NULL) {
        error = INET_EBADF;
    } else {
        inetThreadAttachSock(ti, so);
        if (so->so_kermit_id == -1) {
            InetMbuf *nam = NULL;
            error = inetSockargs(&nam, addr, addrLen, INET_MT_SONAME);
            if (error == 0) {
                error = inetSoBind(so, nam);
                sceNetIfhandle_lib_F56FAC82(nam);
            }
        } else {
            int spl = sceNet_lib_94B44F26();
            error = sceWlanDrv_lib_107E61D0(so->so_kermit_id, addr, addrLen);
            sceNet_lib_515B2F33(spl);
        }
        inetThreadDetachSock(ti, so);
    }

    return sceNet_lib_757085B0(ti, error);
}

int sceNetInetAccept(int sock, void *addr, u32 *addrLen)
{
    int error = 0;
    u32 namelen = 0;
    InetThreadInfo *ti;
    InetSocket *head;
    InetSocket *accepted = NULL;
    s32 kermitNewId = 0;

    if (sceKernelCheckThreadStack() < 4032) {
        return -1;
    }
    ti = sceNet_lib_CA3CF5EB(&error);
    if (ti == NULL) {
        return -1;
    }
    if (addr != NULL) {
        namelen = *addrLen;
    }

    head = inetSockFind(sock);
    if (head == NULL) {
        error = INET_EBADF;
    } else {
        int spl;
        inetThreadAttachSock(ti, head);
        spl = sceNet_lib_94B44F26();

        if ((head->so_proto->pr_flags & INET_PR_LISTEN) == 0) {
            error = INET_EOPNOTSUPP;
        } else if ((head->so_options & INET_SO_ACCEPTCONN) == 0) {
            error = INET_EINVAL;
        } else if (head->so_kermit_id == -1 && (head->so_state & INET_SS_NBIO) != 0 && head->so_qlen == 0) {
            error = INET_EAGAIN;
        } else {
            while (head->so_qlen == 0 && head->so_error == 0) {
                if (head->so_kermit_id != -1) {
                    error = sceWlanDrv_lib_F7713C18(head->so_kermit_id, addr, addrLen, &kermitNewId);
                    if (error == 0) {
                        accepted = inetSoNewConn(head, 2);
                        accepted->so_state &= (u16)~INET_SS_NOFDREF;
                        head->so_error = 0;
                        head->so_state = (u16)((head->so_state & ~0x20EU) | INET_SS_ISCONNECTED);
                        break;
                    }
                    if (error != INET_EAGAIN || (head->so_state & INET_SS_NBIO) != 0) {
                        head->so_error = (u16)error;
                        break;
                    }
                }
                if ((head->so_state & INET_SS_CANTRCVMORE) != 0) {
                    head->so_error = INET_ECONNABORTED;
                    break;
                }
                if (head->so_kermit_id == -1) {
                    error = sceNet_lib_205E8D17(&head->so_timeo, 0);
                } else {
                    error = sceNet_lib_205E8D17(&head->so_timeo, 25);
                    if (error == INET_EAGAIN) {
                        error = 0;
                    }
                }
                if (error != 0) {
                    break;
                }
            }

            if (error == 0) {
                if (head->so_error != 0) {
                    error = head->so_error;
                    head->so_error = 0;
                } else {
                    int newId = inetSockAllocId();
                    if (newId < 0) {
                        error = INET_EMFILE;
                    } else {
                        InetMbuf *nam;
                        accepted = head->so_q.tqh_first;
                        accepted->so_kermit_id = (head->so_kermit_id != -1) ? kermitNewId : -1;
                        inetSoQremque(accepted, 1);
                        nam = sceNetIfhandle_lib_A493AA5F(INET_M_WAIT, INET_MT_SONAME);
                        if (nam == NULL) {
                            error = INET_ENOMEM;
                        } else {
                            if (head->so_kermit_id == -1) {
                                error = inetSoAccept(accepted, nam);
                                if (error == 0 && addr != NULL) {
                                    if ((u32)nam->m_len < namelen) {
                                        namelen = (u32)nam->m_len;
                                    }
                                    sceNet_lib_2F305274(nam->m_data, addr, (int)namelen);
                                    *addrLen = namelen;
                                }
                            } else {
                                error = 0;
                            }
                            accepted->so_sock_id = (error == 0) ? newId : -1;
                            sceNetIfhandle_lib_F56FAC82(nam);
                        }
                    }
                }
            }
        }

        sceNet_lib_515B2F33(spl);
        inetThreadDetachSock(ti, head);
    }

    if (sceNet_lib_757085B0(ti, error) >= 0) {
        return accepted->so_sock_id;
    }
    return -1;
}

int sceNetInetConnect(int sock, const void *addr, u32 addrLen)
{
    int error = 0;
    InetThreadInfo *ti;
    InetSocket *so;

    if (sceKernelCheckThreadStack() < 4048) {
        return -1;
    }
    ti = sceNet_lib_CA3CF5EB(&error);
    if (ti == NULL) {
        return -1;
    }

    so = inetSockFind(sock);
    if (so == NULL) {
        return sceNet_lib_757085B0(ti, INET_EBADF);
    }

    inetThreadAttachSock(ti, so);
    if ((so->so_state & (INET_SS_NBIO | INET_SS_ISCONNECTING)) == (INET_SS_NBIO | INET_SS_ISCONNECTING)) {
        error = INET_EALREADY;
        if (so->so_kermit_id != -1 && (sceWlanDrv_lib_56AE09D2(so->so_kermit_id, 28) & 4) != 0) {
            error = 0;
            so->so_state = (u16)((so->so_state & ~0x20EU) | INET_SS_ISCONNECTED);
        }
    } else {
        InetMbuf *nam = NULL;
        error = inetSockargs(&nam, addr, addrLen, INET_MT_SONAME);
        if (error == 0) {
            if (so->so_kermit_id == -1) {
                error = inetSoConnect(so, nam);
            } else if ((so->so_options & INET_SO_ACCEPTCONN) != 0) {
                error = INET_EOPNOTSUPP;
            } else {
                int spl = sceNet_lib_94B44F26();
                if ((so->so_state & (INET_SS_ISCONNECTED | INET_SS_ISCONNECTING)) != 0 &&
                    ((so->so_proto->pr_flags & INET_PR_CONNREQUIRED) != 0 || inetSoDisconnect(so) != 0)) {
                    error = INET_EISCONN;
                } else {
                    error = sceWlanDrv_lib_5CFFAAB1(so->so_kermit_id, addr, addrLen);
                    if (error == INET_EINPROGRESS) {
                        error = 0;
                    }
                    if (error == 0) {
                        so->so_state = (u16)((so->so_state & ~0x00EU) | INET_SS_ISCONNECTING);
                    }
                }
                sceNet_lib_515B2F33(spl);
            }

            if (error == 0) {
                if ((so->so_state & (INET_SS_NBIO | INET_SS_ISCONNECTING)) == (INET_SS_NBIO | INET_SS_ISCONNECTING)) {
                    sceNetIfhandle_lib_F56FAC82(nam);
                    error = INET_EINPROGRESS;
                    nam = NULL;
                } else {
                    int spl = sceNet_lib_94B44F26();
                    while ((so->so_state & INET_SS_ISCONNECTING) != 0 && so->so_error == 0) {
                        if (so->so_kermit_id == -1) {
                            error = sceNet_lib_205E8D17(&so->so_timeo, 0);
                        } else {
                            error = sceNet_lib_205E8D17(&so->so_timeo, 25);
                            if (error == INET_EAGAIN) {
                                error = 0;
                            }
                        }
                        if (error != 0) {
                            break;
                        }
                        if (so->so_kermit_id != -1) {
                            int pollRes = sceWlanDrv_lib_56AE09D2(so->so_kermit_id, 28);
                            error = pollRes;
                            if ((pollRes & 4) != 0) {
                                error = 0;
                                so->so_state = (u16)((so->so_state & ~0x20EU) | INET_SS_ISCONNECTED);
                            } else if ((pollRes & 0x18) != 0) {
                                break;
                            }
                        }
                    }
                    if (error == 0) {
                        error = so->so_error;
                        so->so_error = 0;
                    }
                    sceNet_lib_515B2F33(spl);
                }
            }

            if (nam != NULL) {
                so->so_state &= (u16)~INET_SS_ISCONNECTING;
                sceNetIfhandle_lib_F56FAC82(nam);
            }
        }
    }

    inetThreadDetachSock(ti, so);
    return sceNet_lib_757085B0(ti, error);
}

int sceNetInetSend(int sock, const void *buf, s32 len, u32 flags)
{
    int error = 0;
    s32 sent;
    InetThreadInfo *ti;

    if (sceKernelCheckThreadStack() < 4000) {
        return -1;
    }
    ti = sceNet_lib_CA3CF5EB(&error);
    if (ti == NULL) {
        return -1;
    }

    error = inetKermitSend(ti, sock, buf, len, flags, NULL, 0, &sent);
    if (error == INET_KERMIT_FALLBACK) {
        InetMsghdr msg;
        InetIovec iov;
        iov.iov_base = (void *)buf;
        iov.iov_len = len;
        msg.msg_name = NULL;
        msg.msg_namelen = 0;
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = NULL;
        msg.msg_controllen = 0;
        msg.msg_flags = 0;
        error = inetSendit(ti, sock, &msg, flags, &sent);
    }

    if (sceNet_lib_757085B0(ti, error) >= 0) {
        return sent;
    }
    return -1;
}

int sceNetInetSendto(int sock, const void *buf, s32 len, u32 flags, const void *to, u32 toLen)
{
    int error = 0;
    s32 sent;
    InetThreadInfo *ti;

    if (sceKernelCheckThreadStack() < 4000) {
        return -1;
    }
    ti = sceNet_lib_CA3CF5EB(&error);
    if (ti == NULL) {
        return -1;
    }

    error = inetKermitSend(ti, sock, buf, len, flags, to, toLen, &sent);
    if (error == INET_KERMIT_FALLBACK) {
        InetMsghdr msg;
        InetIovec iov;
        iov.iov_base = (void *)buf;
        iov.iov_len = len;
        msg.msg_name = (void *)to;
        msg.msg_namelen = toLen;
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = NULL;
        msg.msg_controllen = 0;
        msg.msg_flags = 0;
        error = inetSendit(ti, sock, &msg, flags, &sent);
    }

    if (sceNet_lib_757085B0(ti, error) >= 0) {
        return sent;
    }
    return -1;
}

int sceNetInetSendmsg(int sock, const InetMsghdr *msg, u32 flags)
{
    int error = 0;
    s32 sent;
    InetThreadInfo *ti;
    InetMsghdr localMsg;
    InetIovec stackIov[8];
    InetIovec *iov = stackIov;

    if (sceKernelCheckThreadStack() < 3952) {
        return -1;
    }
    ti = sceNet_lib_CA3CF5EB(&error);
    if (ti == NULL) {
        return -1;
    }

    if (msg == NULL) {
        error = INET_EINVAL;
    } else {
        sceKernelMemcpy(&localMsg, msg, sizeof(localMsg));
        if (localMsg.msg_iovlen >= 9U) {
            if (localMsg.msg_iovlen >= 1025U) {
                error = INET_EMSGSIZE;
            } else {
                iov = (InetIovec *)sceNetIfhandle_lib_15CFE3C0(localMsg.msg_iovlen * sizeof(InetIovec));
                if (iov == NULL) {
                    error = INET_ENOMEM;
                }
            }
        }
        if (error == 0) {
            InetSocket *so;
            if (localMsg.msg_iovlen != 0U) {
                sceKernelMemcpy(iov, localMsg.msg_iov, localMsg.msg_iovlen * sizeof(InetIovec));
            }
            localMsg.msg_iov = iov;
            so = inetSockFind(sock);
            if (so == NULL) {
                error = INET_EBADF;
            } else {
                inetThreadAttachSock(ti, so);
                if (so->so_kermit_id != -1) {
                    error = sceWlanDrv_lib_BFE46C6D(so->so_kermit_id, msg->msg_iov, msg->msg_iovlen,
                                                    flags, localMsg.msg_name, localMsg.msg_namelen, &sent);
                } else {
                    error = INET_KERMIT_FALLBACK;
                }
                inetThreadDetachSock(ti, so);
            }
            if (error == INET_KERMIT_FALLBACK) {
                error = inetSendit(ti, sock, &localMsg, flags, &sent);
            }
            if (iov != stackIov) {
                sceNetIfhandle_lib_76BAD213(iov);
            }
        }
    }

    if (sceNet_lib_757085B0(ti, error) >= 0) {
        return sent;
    }
    return -1;
}

static int INET_NOINLINE inetSendit(InetThreadInfo *ti, int sock, const InetMsghdr *mp,
                                    u32 flags, s32 *retsize)
{
    InetSocket *so;
    InetUio uio;
    InetMbuf *to = NULL;
    InetMbuf *control = NULL;
    s32 totalLen = 0;
    s32 idx;
    int error = 0;

    so = inetSockFind(sock);
    if (so == NULL) {
        return INET_EBADF;
    }

    inetThreadAttachSock(ti, so);
    uio.uio_iov = mp->msg_iov;
    uio.uio_iovcnt = (s32)mp->msg_iovlen;
    uio.uio_offset = 0;
    uio.uio_resid = 0;
    uio.uio_rw = 1;

    for (idx = 0; idx < uio.uio_iovcnt; idx++) {
        s32 iovLen = mp->msg_iov[idx].iov_len;
        if (iovLen < 0 || (uio.uio_resid + iovLen) < 0) {
            error = INET_EINVAL;
            break;
        }
        uio.uio_resid += iovLen;
    }

    if (error == 0 && mp->msg_name != NULL) {
        error = inetSockargs(&to, mp->msg_name, mp->msg_namelen, INET_MT_SONAME);
    }
    if (error == 0 && mp->msg_control != NULL) {
        if (mp->msg_controllen < 12U) {
            error = INET_EINVAL;
        } else {
            error = inetSockargs(&control, mp->msg_control, mp->msg_controllen, INET_MT_CONTROL);
        }
    }

    if (error == 0) {
        totalLen = uio.uio_resid;
        error = so->so_send(so, to, &uio, NULL, control, (int)flags);
        if (error != 0 && uio.uio_resid != totalLen && (error == INET_EINTR || error == INET_EAGAIN)) {
            error = 0;
        }
        if (error == 0) {
            *retsize = totalLen - uio.uio_resid;
        }
    }

    if (to != NULL) {
        sceNetIfhandle_lib_F56FAC82(to);
    }
    inetThreadDetachSock(ti, so);
    return error;
}

int sceNetInetRecv(int sock, void *buf, s32 len, u32 flags)
{
    int error = 0;
    s32 recvd;
    InetThreadInfo *ti;

    if (sceKernelCheckThreadStack() < 4000) {
        return -1;
    }
    ti = sceNet_lib_CA3CF5EB(&error);
    if (ti == NULL) {
        return -1;
    }

    error = inetKermitRecv(ti, sock, buf, len, flags, NULL, NULL, &recvd);
    if (error == INET_KERMIT_FALLBACK) {
        InetMsghdr msg;
        InetIovec iov;
        iov.iov_base = buf;
        iov.iov_len = len;
        msg.msg_name = NULL;
        msg.msg_namelen = 0;
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = NULL;
        msg.msg_controllen = 0;
        msg.msg_flags = flags;
        error = inetRecvit(ti, sock, &msg, NULL, &recvd);
    }

    if (sceNet_lib_757085B0(ti, error) >= 0) {
        return recvd;
    }
    return -1;
}

int sceNetInetRecvfrom(int sock, void *buf, s32 len, u32 flags, void *from, u32 *fromLen)
{
    int error = 0;
    s32 recvd;
    InetThreadInfo *ti;

    if (sceKernelCheckThreadStack() < 4000) {
        return -1;
    }
    ti = sceNet_lib_CA3CF5EB(&error);
    if (ti == NULL) {
        return -1;
    }

    error = inetKermitRecv(ti, sock, buf, len, flags, from, fromLen, &recvd);
    if (error == INET_KERMIT_FALLBACK) {
        InetMsghdr msg;
        InetIovec iov;
        iov.iov_base = buf;
        iov.iov_len = len;
        msg.msg_name = from;
        msg.msg_namelen = (fromLen != NULL) ? *fromLen : 0U;
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = NULL;
        msg.msg_controllen = 0;
        msg.msg_flags = flags;
        error = inetRecvit(ti, sock, &msg, (u8 *)fromLen, &recvd);
    }

    if (sceNet_lib_757085B0(ti, error) >= 0) {
        return recvd;
    }
    return -1;
}

int sceNetInetRecvmsg(int sock, InetMsghdr *msg, u32 flags)
{
    int error = 0;
    s32 recvd;
    InetThreadInfo *ti;
    InetMsghdr localMsg;
    InetIovec stackIov[8];
    InetIovec *iov = stackIov;

    if (sceKernelCheckThreadStack() < 3952) {
        return -1;
    }
    ti = sceNet_lib_CA3CF5EB(&error);
    if (ti == NULL) {
        return -1;
    }

    if (msg == NULL) {
        error = INET_EINVAL;
    } else {
        sceKernelMemcpy(&localMsg, msg, sizeof(localMsg));
        if (localMsg.msg_iovlen >= 9U) {
            if (localMsg.msg_iovlen >= 1025U) {
                error = INET_EMSGSIZE;
            } else {
                iov = (InetIovec *)sceNetIfhandle_lib_15CFE3C0(localMsg.msg_iovlen * sizeof(InetIovec));
                if (iov == NULL) {
                    error = INET_ENOMEM;
                }
            }
        }
        if (error == 0) {
            InetIovec *savedIov = localMsg.msg_iov;
            if (localMsg.msg_iovlen != 0U) {
                sceKernelMemcpy(iov, savedIov, localMsg.msg_iovlen * sizeof(InetIovec));
            }
            localMsg.msg_flags = flags;
            localMsg.msg_iov = iov;
            error = inetRecvit(ti, sock, &localMsg, NULL, &recvd);
            if (error == 0) {
                localMsg.msg_iov = savedIov;
                sceKernelMemcpy(msg, &localMsg, sizeof(localMsg));
            }
            if (iov != stackIov) {
                sceNetIfhandle_lib_76BAD213(iov);
            }
        }
    }

    if (sceNet_lib_757085B0(ti, error) >= 0) {
        return recvd;
    }
    return -1;
}

static int INET_NOINLINE inetRecvit(InetThreadInfo *ti, int sock, InetMsghdr *mp,
                                    u8 *namelenp, s32 *retsize)
{
    InetSocket *so;
    InetUio uio;
    InetMbuf *from = NULL;
    InetMbuf *control = NULL;
    s32 totalLen = 0;
    s32 idx;
    int error = 0;

    so = inetSockFind(sock);
    if (so == NULL) {
        return INET_EBADF;
    }

    inetThreadAttachSock(ti, so);
    uio.uio_iov = mp->msg_iov;
    uio.uio_iovcnt = (s32)mp->msg_iovlen;
    uio.uio_offset = 0;
    uio.uio_resid = 0;
    uio.uio_rw = 0;

    for (idx = 0; idx < uio.uio_iovcnt; idx++) {
        s32 iovLen = mp->msg_iov[idx].iov_len;
        if (iovLen < 0 || (uio.uio_resid + iovLen) < 0) {
            error = INET_EINVAL;
            break;
        }
        uio.uio_resid += iovLen;
    }

    if (error == 0) {
        totalLen = uio.uio_resid;
        error = so->so_receive(so, &from, &uio, NULL,
                               (mp->msg_control != NULL) ? &control : NULL,
                               &mp->msg_flags);
        if (error != 0 && uio.uio_resid != totalLen && (error == INET_EINTR || error == INET_EAGAIN)) {
            error = 0;
        }
        if (error == 0) {
            *retsize = totalLen - uio.uio_resid;
            if (mp->msg_name != NULL) {
                s32 len = (s32)mp->msg_namelen;
                if (len <= 0 || from == NULL) {
                    len = 0;
                } else {
                    if (len > from->m_len) {
                        len = from->m_len;
                    }
                    sceNet_lib_2F305274(from->m_data, mp->msg_name, len);
                }
                mp->msg_namelen = (u32)len;
                if (namelenp != NULL) {
                    *namelenp = (u8)len;
                }
            }
            if (mp->msg_control != NULL) {
                s32 rem = (s32)mp->msg_controllen;
                u8 *dst = (u8 *)mp->msg_control;
                InetMbuf *cm = control;
                if (rem > 0 && cm != NULL) {
                    while (cm != NULL && rem > 0) {
                        s32 copyLen = cm->m_len;
                        if (rem < copyLen) {
                            copyLen = rem;
                            mp->msg_flags |= 0x20U;
                        }
                        sceNet_lib_2F305274(cm->m_data, dst, copyLen);
                        cm = cm->m_next;
                        if (cm != NULL) {
                            copyLen = (copyLen + 3) & ~3;
                        }
                        rem -= copyLen;
                        dst += copyLen;
                    }
                    mp->msg_controllen = (u32)(dst - (u8 *)mp->msg_control);
                } else {
                    mp->msg_controllen = 0;
                }
            }
        }
    }

    if (from != NULL) {
        sceNetIfhandle_lib_F56FAC82(from);
    }
    if (control != NULL) {
        sceNetIfhandle_lib_F56FAC82(control);
    }
    inetThreadDetachSock(ti, so);
    return error;
}

int sceNetInetSetsockopt(int sock, int level, int optname, const void *optval, u32 optlen)
{
    int error = 0;
    InetThreadInfo *ti;
    InetSocket *so;

    if (sceKernelCheckThreadStack() < 4048) {
        return -1;
    }
    ti = sceNet_lib_CA3CF5EB(&error);
    if (ti == NULL) {
        return -1;
    }

    so = inetSockFind(sock);
    if (so == NULL) {
        error = INET_EBADF;
    } else {
        InetMbuf *m = NULL;
        inetThreadAttachSock(ti, so);
        if (optlen >= 237U) {
            error = INET_EINVAL;
        } else {
            if (optval != NULL) {
                m = sceNetIfhandle_lib_A493AA5F(INET_M_WAIT, INET_MT_SOOPTS);
                if (m == NULL) {
                    error = INET_ENOMEM;
                } else {
                    sceNet_lib_2F305274(optval, m->m_data, (int)optlen);
                    m->m_len = (s32)optlen;
                }
            }
            if (error == 0) {
                if (so->so_kermit_id != -1) {
                    error = sceWlanDrv_lib_9ED787C8(so->so_kermit_id, level, optname, optval, optlen);
                }
                error = inetSoSetopt(so, level, optname, m);
            }
        }
        inetThreadDetachSock(ti, so);
    }

    return sceNet_lib_757085B0(ti, error);
}

int sceNetInetGetsockopt(int sock, int level, int optname, void *optval, u32 *optlen)
{
    int error = 0;
    InetThreadInfo *ti;
    InetSocket *so;

    if (sceKernelCheckThreadStack() < 4032) {
        return -1;
    }
    ti = sceNet_lib_CA3CF5EB(&error);
    if (ti == NULL) {
        return -1;
    }

    so = inetSockFind(sock);
    if (so == NULL) {
        return sceNet_lib_757085B0(ti, INET_EBADF);
    }

    inetThreadAttachSock(ti, so);
    {
        u32 maxLen = (optval != NULL) ? *optlen : 0U;
        if (so->so_kermit_id == -1) {
            InetMbuf *m = NULL;
            error = inetSoGetopt(so, level, optname, &m);
            if (error == 0 && optval != NULL && maxLen != 0U && m != NULL) {
                u32 copied = 0U;
                u8 *dst = (u8 *)optval;
                while (m != NULL && copied < maxLen) {
                    u32 chunk = ((u32)m->m_len < (maxLen - copied)) ? (u32)m->m_len : (maxLen - copied);
                    sceNet_lib_2F305274(m->m_data, dst, (int)chunk);
                    dst += chunk;
                    copied += chunk;
                    m = inetMfreeOne(m);
                }
                if (error == 0) {
                    *optlen = copied;
                }
            }
            if (m != NULL) {
                sceNetIfhandle_lib_F8825DC4(m);
            }
        } else {
            error = sceWlanDrv_lib_E39F7F88(so->so_kermit_id, level, optname, optval, optlen);
        }
    }
    inetThreadDetachSock(ti, so);
    return sceNet_lib_757085B0(ti, error);
}

int sceNetInetGetsockname(int sock, void *name, u32 *nameLen)
{
    int error = 0;
    InetThreadInfo *ti;
    InetSocket *so;

    if (sceKernelCheckThreadStack() < 4048) {
        return -1;
    }
    ti = sceNet_lib_CA3CF5EB(&error);
    if (ti == NULL) {
        return -1;
    }

    so = inetSockFind(sock);
    if (so == NULL) {
        error = INET_EBADF;
    } else {
        u32 len = *nameLen;
        InetMbuf *m;
        inetThreadAttachSock(ti, so);
        m = sceNetIfhandle_lib_59F0D619(INET_M_WAIT, INET_MT_SONAME);
        if (m == NULL) {
            error = INET_ENOMEM;
        } else {
            if (so->so_kermit_id == -1) {
                error = so->so_proto->pr_usrreq(so, INET_PRU_SOCKADDR, NULL, m, NULL, NULL);
            } else {
                error = sceWlanDrv_lib_6417F8E3(so->so_kermit_id, name, nameLen);
            }
            if (error == 0 && so->so_kermit_id == -1) {
                if ((u32)m->m_len < len) {
                    len = (u32)m->m_len;
                }
                sceNet_lib_2F305274(m->m_data, name, (int)len);
                *nameLen = len;
            }
            sceNetIfhandle_lib_F56FAC82(m);
        }
        inetThreadDetachSock(ti, so);
    }

    return sceNet_lib_757085B0(ti, error);
}

int sceNetInetGetpeername(int sock, void *name, u32 *nameLen)
{
    int error = 0;
    InetThreadInfo *ti;
    InetSocket *so;

    if (sceKernelCheckThreadStack() < 4048) {
        return -1;
    }
    ti = sceNet_lib_CA3CF5EB(&error);
    if (ti == NULL) {
        return -1;
    }

    so = inetSockFind(sock);
    if (so == NULL) {
        error = INET_EBADF;
    } else {
        inetThreadAttachSock(ti, so);
        if ((so->so_state & (INET_SS_ISCONNECTED | INET_SS_ISDISCONNECTED)) == 0) {
            error = INET_ENOTCONN;
        } else {
            u32 len = *nameLen;
            InetMbuf *m = sceNetIfhandle_lib_59F0D619(INET_M_WAIT, INET_MT_SONAME);
            if (m == NULL) {
                error = INET_ENOMEM;
            } else {
                if (so->so_kermit_id == -1) {
                    error = so->so_proto->pr_usrreq(so, INET_PRU_PEERADDR, NULL, m, NULL, NULL);
                } else {
                    error = sceWlanDrv_lib_327C9FCE(so->so_kermit_id, name, nameLen);
                }
                if (error == 0 && so->so_kermit_id == -1) {
                    if ((u32)m->m_len < len) {
                        len = (u32)m->m_len;
                    }
                    sceNet_lib_2F305274(m->m_data, name, (int)len);
                    *nameLen = len;
                }
                sceNetIfhandle_lib_F56FAC82(m);
            }
        }
        inetThreadDetachSock(ti, so);
    }

    return sceNet_lib_757085B0(ti, error);
}

static int INET_NOINLINE inetSockargs(InetMbuf **mp, const void *buf, u32 buflen, int type)
{
    InetMbuf *m;

    if (type == INET_MT_SONAME && buflen >= 256U) {
        return INET_EINVAL;
    }
    m = sceNetIfhandle_lib_A493AA5F(INET_M_WAIT, (u16)type);
    if (m == NULL) {
        return INET_ENOMEM;
    }
    if (buflen >= 237U) {
        u8 *ext = (u8 *)sceNetIfhandle_lib_15CFE3C0(buflen);
        M_EXT(m).ext_buf = ext;
        if (ext != NULL) {
            m->m_data = ext;
            m->m_flags = (u16)((m->m_flags & ~(INET_M_EXT | INET_M_CLUSTER)) | INET_M_EXT);
            M_EXT(m).ext_free = NULL;
            M_EXT(m).ext_arg = NULL;
            M_EXT(m).ext_size = buflen;
            M_EXT(m).ext_type = (u32)m->m_type;
            M_EXT(m).ext_nextref = m;
            M_EXT(m).ext_prevref = m;
        }
        if ((m->m_flags & INET_M_EXT) == 0) {
            sceNetIfhandle_lib_F8825DC4(m);
            return INET_ENOMEM;
        }
    }
    m->m_len = (s32)buflen;
    sceNet_lib_2F305274(buf, m->m_data, (int)buflen);
    *mp = m;
    if (type == INET_MT_SONAME) {
        m->m_data[0] = (u8)buflen;
    }
    return 0;
}

int sceNetInetSelect(s32 maxSock, void *readSet, void *writeSet, void *exceptSet,
                     const InetTimeval *timeout)
{
    int error = 0;
    s32 readyCount = 0;
    s64 startTime = 0;
    InetThreadInfo *ti;
    u32 bits[48];

    if (sceKernelCheckThreadStack() < 3824) {
        return -1;
    }
    ti = sceNet_lib_CA3CF5EB(&error);
    if (ti == NULL) {
        return -1;
    }

    if (maxSock < 0) {
        error = INET_EINVAL;
    } else {
        s32 nfd = (maxSock < 256) ? maxSock : 256;
        u32 wordsPerSet = (u32)((nfd + 31) / 32);
        u32 bytesPerSet = wordsPerSet * 4U;
        u32 totalUsec = 0U;

        if (readSet != NULL) {
            sceNet_lib_2F305274(readSet, &bits[0], (int)bytesPerSet);
        } else {
            sceKernelMemset(&bits[0], 0, bytesPerSet);
        }
        if (writeSet != NULL) {
            sceNet_lib_2F305274(writeSet, &bits[wordsPerSet], (int)bytesPerSet);
        } else {
            sceKernelMemset(&bits[wordsPerSet], 0, bytesPerSet);
        }
        if (exceptSet != NULL) {
            sceNet_lib_2F305274(exceptSet, &bits[wordsPerSet * 2U], (int)bytesPerSet);
        } else {
            sceKernelMemset(&bits[wordsPerSet * 2U], 0, bytesPerSet);
        }

        if (timeout != NULL && (timeout->tv_sec >= 0x10C6U || timeout->tv_usec > 999999U)) {
            error = INET_EINVAL;
        } else {
            u32 remainUsec;
            if (timeout != NULL) {
                totalUsec = timeout->tv_sec * 1000000U + timeout->tv_usec;
            }
            remainUsec = totalUsec;

            for (;;) {
                int spl;
                inetSelAttach(ti, bits, nfd);
                spl = sceNet_lib_94B44F26();
                error = inetSelscan(ti, bits, &bits[wordsPerSet * 3U], nfd, &readyCount);
                if (error != 0 || readyCount != 0) {
                    sceNet_lib_515B2F33(spl);
                    break;
                }
                if (timeout != NULL) {
                    s64 now;
                    if (startTime == 0) {
                        startTime = sceKernelGetSystemTimeWide();
                    }
                    now = sceKernelGetSystemTimeWide();
                    if ((u64)(now - startTime) >= (u64)totalUsec) {
                        sceNet_lib_515B2F33(spl);
                        break;
                    }
                    remainUsec = totalUsec - (u32)(now - startTime);
                }

                if (sceWlanDrv_lib_3BE42BCF() != 0) {
                    u32 waitUsec = (remainUsec != 0U && remainUsec <= 200000U) ? remainUsec : 200000U;
                    error = sceNet_lib_F6DB0A0B(sceNet_lib_B68E1EEA(), waitUsec);
                    if (error == INET_EAGAIN) {
                        error = 0;
                    }
                } else {
                    error = sceNet_lib_F6DB0A0B(sceNet_lib_B68E1EEA(), remainUsec);
                }
                sceNet_lib_515B2F33(spl);
                if (error != 0) {
                    break;
                }
                inetSelDetach(ti, bits, nfd);
            }

            inetSelDetach(ti, bits, nfd);
            if (error == INET_EAGAIN) {
                error = 0;
            }
            if (error == 0) {
                if (readSet != NULL) {
                    sceNet_lib_2F305274(&bits[wordsPerSet * 3U], readSet, (int)bytesPerSet);
                }
                if (writeSet != NULL) {
                    sceNet_lib_2F305274(&bits[wordsPerSet * 4U], writeSet, (int)bytesPerSet);
                }
                if (exceptSet != NULL) {
                    sceNet_lib_2F305274(&bits[wordsPerSet * 5U], exceptSet, (int)bytesPerSet);
                }
            }
        }
    }

    if (sceNet_lib_757085B0(ti, error) >= 0) {
        return readyCount;
    }
    return -1;
}

static int INET_NOINLINE inetSelscan(InetThreadInfo *ti, const u32 *ibits, u32 *obits,
                                     int nfd, s32 *retval)
{
    s32 total = 0;
    int msk;

    for (msk = 0; msk < 3; msk++) {
        int base;
        for (base = 0; base < nfd; base += 32) {
            u32 inWord = *ibits++;
            u32 outWord = 0U;
            while (inWord != 0U) {
                int bit = 0;
                u32 scan = inWord;
                while ((scan & 1U) == 0U) {
                    scan >>= 1;
                    bit++;
                }
                if (base + bit >= nfd) {
                    break;
                }
                inWord &= ~(1U << bit);
                {
                    InetSocket *so = inetSockFind(base + bit);
                    if (so == NULL) {
                        return INET_EBADF;
                    }
                    if (inetSoPoll(so, (int)g_select_poll_flags[msk], ti) != 0) {
                        outWord |= (1U << bit);
                        total++;
                    }
                }
            }
            *obits++ = outWord;
        }
    }

    *retval = total;
    return 0;
}

static void INET_NOINLINE inetSelAttach(InetThreadInfo *ti, const u32 *bits, int nfd)
{
    inetSelTrack(ti, bits, nfd, 0);
}

static void INET_NOINLINE inetSelDetach(InetThreadInfo *ti, const u32 *bits, int nfd)
{
    inetSelTrack(ti, bits, nfd, 1);
}

int sceNetInetPoll(InetPollfd *fds, s32 nfds, s32 timeoutMs)
{
    int error = 0;
    s32 readyCount = 0;
    s64 startTime = 0;
    InetThreadInfo *ti;
    InetPollfd stackFds[32];
    InetPollfd *workFds = stackFds;
    u32 byteLen;
    int useStack;

    if (sceKernelCheckThreadStack() < 3760) {
        return -1;
    }
    ti = sceNet_lib_CA3CF5EB(&error);
    if (ti == NULL) {
        return -1;
    }

    byteLen = (u32)(nfds * (s32)sizeof(InetPollfd));
    useStack = (byteLen <= sizeof(stackFds));
    if (!useStack) {
        workFds = (InetPollfd *)sceNetIfhandle_lib_15CFE3C0(byteLen);
        if (workFds == NULL) {
            error = INET_ENOMEM;
        }
    }

    if (error == 0) {
        u32 totalUsec = 0U;
        sceNet_lib_2F305274(fds, workFds, (int)byteLen);
        if (timeoutMs != -1) {
            if (timeoutMs > 4294967) {
                error = INET_EINVAL;
            } else {
                totalUsec = (u32)timeoutMs * 1000U;
            }
        }

        if (error == 0) {
            u32 remainUsec = totalUsec;
            for (;;) {
                int spl;
                inetPollAttach(ti, workFds, nfds);
                spl = sceNet_lib_94B44F26();
                error = inetPollscan(ti, workFds, nfds, &readyCount);
                if (error != 0 || readyCount != 0) {
                    sceNet_lib_515B2F33(spl);
                    break;
                }
                if (timeoutMs != -1) {
                    s64 now;
                    if (startTime == 0) {
                        startTime = sceKernelGetSystemTimeWide();
                    }
                    now = sceKernelGetSystemTimeWide();
                    if ((u64)(now - startTime) >= (u64)totalUsec) {
                        sceNet_lib_515B2F33(spl);
                        break;
                    }
                    remainUsec = totalUsec - (u32)(now - startTime);
                }

                if (sceWlanDrv_lib_3BE42BCF() != 0) {
                    u32 waitUsec = (remainUsec != 0U && remainUsec <= 10000U) ? remainUsec : 10000U;
                    error = sceNet_lib_F6DB0A0B(sceNet_lib_B68E1EEA(), waitUsec);
                    if (error == INET_EAGAIN) {
                        error = 0;
                    }
                } else {
                    error = sceNet_lib_F6DB0A0B(sceNet_lib_B68E1EEA(), remainUsec);
                }
                sceNet_lib_515B2F33(spl);
                if (error != 0) {
                    break;
                }
                inetPollDetach(ti, workFds, nfds);
            }
        }

        inetPollDetach(ti, workFds, nfds);
        if (error == INET_EAGAIN) {
            error = 0;
        }
        if (error == 0) {
            sceNet_lib_2F305274(workFds, fds, (int)byteLen);
        }
        if (!useStack) {
            sceNetIfhandle_lib_76BAD213(workFds);
        }
    }

    if (sceNet_lib_757085B0(ti, error) >= 0) {
        return readyCount;
    }
    return -1;
}

static int INET_NOINLINE inetPollscan(InetThreadInfo *ti, InetPollfd *fds, int nfd, s32 *retval)
{
    s32 count = 0;
    int idx;

    for (idx = 0; idx < nfd; idx++) {
        InetSocket *so = inetSockFind(fds[idx].sock);
        if (so != NULL) {
            s16 rev = (s16)inetSoPoll(so, (int)(fds[idx].events | 0x18), ti);
            fds[idx].revents = rev;
            if (rev != 0) {
                count++;
            }
        } else {
            fds[idx].revents = 0x20;
            count++;
        }
    }
    *retval = count;
    return 0;
}

static void INET_NOINLINE inetPollAttach(InetThreadInfo *ti, const InetPollfd *fds, int nfd)
{
    int idx;
    for (idx = 0; idx < nfd; idx++) {
        InetSocket *so = inetSockFind(fds[idx].sock);
        if (so != NULL && so->so_owner_ti == NULL) {
            inetThreadAttachSock(ti, so);
        }
    }
}

static void INET_NOINLINE inetPollDetach(InetThreadInfo *ti, const InetPollfd *fds, int nfd)
{
    int idx;
    for (idx = 0; idx < nfd; idx++) {
        InetSocket *so = inetSockFind(fds[idx].sock);
        if (so != NULL && so->so_owner_ti == ti) {
            inetThreadDetachSock(ti, so);
        }
    }
}

int sceNetInet_lib_AEE60F84(int sock, u32 cmd, void *arg)
{
    int error = 0;
    InetThreadInfo *ti;
    InetSocket *so;
    u32 stackBuf[32];

    ti = sceNet_lib_CA3CF5EB(&error);
    if (ti == NULL) {
        return -1;
    }

    so = inetSockFind(sock);
    if (so == NULL) {
        error = INET_EBADF;
    } else {
        u32 size = (cmd >> 16) & 0x1FFFU;
        void *heapBuf = NULL;
        void *dataPtr = stackBuf;

        inetThreadAttachSock(ti, so);
        if (size > sizeof(stackBuf)) {
            heapBuf = sceNetIfhandle_lib_15CFE3C0(size);
            dataPtr = heapBuf;
            if (heapBuf == NULL) {
                error = INET_ENOMEM;
            }
        }
        if (error == 0) {
            if ((cmd & 0x80000000U) != 0U) {
                if (size != 0U) {
                    sceNet_lib_2F305274(arg, dataPtr, (int)size);
                } else {
                    *(void **)dataPtr = arg;
                }
            } else if ((cmd & 0x40000000U) != 0U && size != 0U) {
                sceKernelMemset(dataPtr, 0, size);
            } else if ((cmd & 0x20000000U) != 0U) {
                *(void **)dataPtr = arg;
            }

            error = inetSooIoctl(so, cmd, dataPtr);
            if (error == 0 && (cmd & 0x40000000U) != 0U && size != 0U) {
                sceNet_lib_2F305274(dataPtr, arg, (int)size);
            }
            if (heapBuf != NULL) {
                sceNetIfhandle_lib_76BAD213(heapBuf);
            }
        }
        inetThreadDetachSock(ti, so);
    }

    return sceNet_lib_757085B0(ti, error);
}

int sceNetInet_lib_B3FF98EB(const int *name, int nameLen, void *oldp, u32 *oldLenp,
                            void *newp, u32 newLen)
{
    int error = 0;
    InetThreadInfo *ti;
    int mib[12];
    u32 oldLen = 0;

    ti = sceNet_lib_CA3CF5EB(&error);
    if (ti == NULL) {
        return -1;
    }

    if ((u32)(nameLen - 2) >= 11U) {
        error = INET_EINVAL;
    } else {
        sceNet_lib_2F305274(name, mib, nameLen * 4);
        if (mib[0] != 4) {
            error = INET_EOPNOTSUPP;
        } else {
            if (oldLenp != NULL) {
                sceNet_lib_2F305274(oldLenp, &oldLen, 4);
            }
            {
                int (*volatile sysctlFn)(int *, u32, void *, u32 *, void *, u32) = inetNetSysctl;
                error = sysctlFn(&mib[1], (u32)(nameLen - 1), oldp, &oldLen, newp, newLen);
            }
            if (error == 0 && oldLenp != NULL) {
                sceNet_lib_2F305274(&oldLen, oldLenp, 4);
            }
        }
    }

    return sceNet_lib_757085B0(ti, error);
}

int sceNetInetSocket(int domain, int type, int protocol)
{
    if (sceKernelCheckThreadStack() < 4048) {
        return -1;
    }
    return inetSockCreateInternal(domain, type, protocol);
}

int sceNetInetListen(int sock, s32 backlog)
{
    int error = 0;
    InetThreadInfo *ti;
    InetSocket *so;

    if (sceKernelCheckThreadStack() < 4064) {
        return -1;
    }
    ti = sceNet_lib_CA3CF5EB(&error);
    if (ti == NULL) {
        return -1;
    }

    so = inetSockFind(sock);
    if (so == NULL) {
        error = INET_EBADF;
    } else {
        inetThreadAttachSock(ti, so);
        error = inetSoListen(so, backlog);
        inetThreadDetachSock(ti, so);
    }

    return sceNet_lib_757085B0(ti, error);
}

int sceNetInetShutdown(int sock, u32 how)
{
    int error = 0;
    InetThreadInfo *ti;
    InetSocket *so;

    if (sceKernelCheckThreadStack() < 4064) {
        return -1;
    }
    ti = sceNet_lib_CA3CF5EB(&error);
    if (ti == NULL) {
        return -1;
    }

    so = inetSockFind(sock);
    if (so == NULL) {
        error = INET_EBADF;
    } else {
        inetThreadAttachSock(ti, so);
        error = inetSoShutdown(so, how);
        inetThreadDetachSock(ti, so);
    }

    return sceNet_lib_757085B0(ti, error);
}

int INET_NOINLINE inetCloseInternal(int sock)
{
    int error = 0;
    InetThreadInfo *ti;
    InetSocket *so;

    ti = sceNet_lib_CA3CF5EB(&error);
    if (ti == NULL) {
        return -1;
    }
    so = inetSockFind(sock);
    error = (so != NULL) ? inetSoClose(so) : INET_EBADF;
    return sceNet_lib_757085B0(ti, error);
}

int sceNetInetClose(int sock)
{
    int error = 0;
    InetThreadInfo *ti;
    InetSocket *so;

    if (sceKernelCheckThreadStack() < 4064) {
        return -1;
    }
    ti = sceNet_lib_CA3CF5EB(&error);
    if (ti == NULL) {
        return -1;
    }
    so = inetSockFind(sock);
    error = (so != NULL) ? inetSoClose(so) : INET_EBADF;
    return sceNet_lib_757085B0(ti, error);
}

int sceNetInetCloseWithRST(int sock)
{
    int error = 0;
    InetThreadInfo *ti;
    InetSocket *so;

    if (sceKernelCheckThreadStack() < 4064) {
        return -1;
    }
    ti = sceNet_lib_CA3CF5EB(&error);
    if (ti == NULL) {
        return -1;
    }
    so = inetSockFind(sock);
    if (so != NULL) {
        inetSoAbort(so);
        error = inetSoClose(so);
    } else {
        error = INET_EBADF;
    }
    return sceNet_lib_757085B0(ti, error);
}

int sceNetInetSocketAbort(int sock)
{
    int error = 0;
    InetThreadInfo *ti;
    InetSocket *so;
    int spl;

    if (sceKernelCheckThreadStack() < 4064) {
        return SCE_NET_INET_ERROR_INSUFFICIENT_STACK;
    }
    ti = sceNet_lib_CA3CF5EB(&error);
    if (ti == NULL) {
        return -1;
    }
    spl = sceNet_lib_94B44F26();
    so = inetSockFind(sock);
    if (so != NULL) {
        sceNet_lib_51C209B2(so);
    } else {
        error = INET_EBADF;
    }
    sceNet_lib_515B2F33(spl);
    return sceNet_lib_757085B0(ti, error);
}

static int INET_NOINLINE inetKermitSend(InetThreadInfo *ti, int sock, const void *buf, s32 len,
                                        u32 flags, const void *to, u32 tolen, s32 *retsize)
{
    InetSocket *so;
    int error = INET_KERMIT_FALLBACK;
    s64 startTime;
    const u8 *cursor = (const u8 *)buf;

    so = inetSockFind(sock);
    if (so == NULL) {
        return INET_EBADF;
    }

    inetThreadAttachSock(ti, so);
    startTime = sceKernelGetSystemTimeWide();

    if (so->so_kermit_id != -1) {
        for (;;) {
            error = sceWlanDrv_lib_C934D53F(so->so_kermit_id, cursor, len, flags, to, tolen, retsize);
            if (error == INET_EAGAIN) {
                if ((flags & 0x80U) != 0U) {
                    break;
                }
                if (so->so_snd.sb_timeo != 0) {
                    s64 elapsed = sceKernelGetSystemTimeWide() - startTime;
                    u64 limit = 10000ULL * (u32)so->so_snd.sb_timeo;
                    if ((u64)elapsed > limit) {
                        break;
                    }
                }
                error = 0;
            }
            if (error != 0) {
                break;
            }
            len -= *retsize;
            if ((flags & 0x40U) != 0U) {
                cursor += *retsize;
            }
            if (len == 0) {
                break;
            }
            error = sceNet_lib_205E8D17(&so->so_timeo, 1);
            if (error == INET_EAGAIN) {
                error = 0;
            }
            if (error != 0) {
                break;
            }
        }
    }

    inetThreadDetachSock(ti, so);
    return error;
}

static int INET_NOINLINE inetKermitRecv(InetThreadInfo *ti, int sock, void *buf, s32 len,
                                        u32 flags, void *from, u32 *fromlen, s32 *retsize)
{
    InetSocket *so;
    int error = INET_KERMIT_FALLBACK;
    s64 startTime;
    u8 *cursor = (u8 *)buf;

    so = inetSockFind(sock);
    if (so == NULL) {
        return INET_EBADF;
    }

    inetThreadAttachSock(ti, so);
    startTime = sceKernelGetSystemTimeWide();

    if (so->so_kermit_id != -1) {
        for (;;) {
            s32 got;
            error = sceWlanDrv_lib_9506F322(so->so_kermit_id, cursor, len, flags, from, fromlen, retsize);
            if (error == INET_EAGAIN) {
                if ((flags & 0x80U) != 0U || (so->so_state & INET_SS_NBIO) != 0) {
                    break;
                }
                if (so->so_rcv.sb_timeo != 0) {
                    s64 elapsed = sceKernelGetSystemTimeWide() - startTime;
                    u64 limit = 10000ULL * (u32)so->so_rcv.sb_timeo;
                    if ((u64)elapsed > limit) {
                        break;
                    }
                }
                error = 0;
                got = 0;
                *retsize = 0;
            } else {
                if (error != 0) {
                    break;
                }
                got = *retsize;
                if (got == 0) {
                    break;
                }
            }

            if ((flags & 0x40U) != 0U) {
                len -= got;
                cursor += got;
                if (len == 0) {
                    break;
                }
            } else if (got != 0) {
                break;
            }

            error = sceNet_lib_205E8D17(&so->so_timeo, 10);
            if (error == INET_EAGAIN) {
                error = 0;
            }
            if (error != 0) {
                break;
            }
        }
    }

    inetThreadDetachSock(ti, so);
    return error;
}

static void INET_NOINLINE inetSelTrack(InetThreadInfo *ti, const u32 *bits, int nfd, int detach)
{
    int msk;

    for (msk = 0; msk < 3; msk++) {
        int base;
        for (base = 0; base < nfd; base += 32) {
            u32 word = *bits++;
            while (word != 0U) {
                int bit = 0;
                u32 scan = word;
                InetSocket *so;
                while ((scan & 1U) == 0U) {
                    scan >>= 1;
                    bit++;
                }
                if (base + bit >= nfd) {
                    break;
                }
                word &= ~(1U << bit);
                so = inetSockFind(base + bit);
                if (so != NULL) {
                    if (detach == 0) {
                        if (so->so_owner_ti == NULL) {
                            inetThreadAttachSock(ti, so);
                        }
                    } else if (detach == 1 && so->so_owner_ti == ti) {
                        inetThreadDetachSock(ti, so);
                    }
                }
            }
        }
    }
}

int INET_NOINLINE inetSysctlInt(void *oldp, u32 *oldlenp, void *newp, u32 newlen, int *valp)
{
    if (newp != NULL && newlen != 4U) {
        return INET_EINVAL;
    }
    if (oldlenp != NULL) {
        if (oldp != NULL) {
            if (*oldlenp < 4U) {
                return INET_ENOMEM;
            }
            *oldlenp = 4U;
            sceNet_lib_2F305274(valp, oldp, 4);
        } else {
            *oldlenp = 4U;
        }
    }
    if (newp != NULL) {
        sceNet_lib_2F305274(newp, valp, 4);
    }
    return 0;
}

int INET_NOINLINE inetSysctlRdInt(void *oldp, u32 *oldlenp, void *newp, int val)
{
    int localVal = val;
    if (newp != NULL) {
        return INET_EPERM;
    }
    if (oldlenp != NULL) {
        if (oldp == NULL) {
            *oldlenp = 4U;
            return 0;
        }
        if (*oldlenp < 4U) {
            return INET_ENOMEM;
        }
        *oldlenp = 4U;
        sceNet_lib_2F305274(&localVal, oldp, 4);
    }
    return 0;
}

int INET_NOINLINE inetSysctlRdStruct(void *oldp, u32 *oldlenp, void *newp, const void *sp, u32 len)
{
    if (newp != NULL) {
        return INET_EPERM;
    }
    if (oldlenp != NULL) {
        if (oldp == NULL) {
            *oldlenp = len;
            return 0;
        }
        if (*oldlenp < len) {
            return INET_ENOMEM;
        }
        *oldlenp = len;
        sceNet_lib_2F305274(sp, oldp, (int)len);
    }
    return 0;
}

char *INET_NOINLINE inetNtoaStatic(u32 addr)
{
    char *cursor = &g_ntoa_static_buf[17];
    u32 hostVal = inetBswap32(addr);
    int octet;

    *cursor = '\0';
    for (octet = 4; octet > 0; octet--) {
        u32 byteVal = hostVal & 0xFFU;
        u32 tens = byteVal / 10U;
        hostVal >>= 8;
        *--cursor = (char)((byteVal % 10U) + '0');
        if (tens != 0U) {
            *--cursor = (char)((tens % 10U) + '0');
            if ((tens / 10U) != 0U) {
                *--cursor = (char)((tens / 10U) + '0');
            }
        }
        *--cursor = '.';
    }
    return cursor + 1;
}
