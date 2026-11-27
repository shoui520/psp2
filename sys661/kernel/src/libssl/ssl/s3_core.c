/*
 * RSA BSAFE SSL-C SSLv3/TLSv1 client handshake state machine and record layer
 * (s3_clnt.c, s3_pkt.c, s3_both.c — 0x01D610..0x020D2B).
 */
#include "libssl_internal.h"

static int g_ssl3ClientMethodInit = 1;
static SslMethod g_ssl3ClientMethod;

static const SslMethod *LIBSSL_NOINLINE ssl3GetClientMethod(int version);
static int LIBSSL_NOINLINE ssl3ClientHello(Ssl *ssl);
static int LIBSSL_NOINLINE ssl3GetServerHello(Ssl *ssl);
static int LIBSSL_NOINLINE ssl3GetServerCertificate(Ssl *ssl);
static int LIBSSL_NOINLINE ssl3GetKeyExchange(Ssl *ssl);
static int LIBSSL_NOINLINE ssl3GetCertificateRequest(Ssl *ssl);
static int LIBSSL_NOINLINE ssl3CaNameCmp(const void *lhs, const void *rhs);
static int LIBSSL_NOINLINE ssl3GetServerDone(Ssl *ssl);
static int LIBSSL_NOINLINE ssl3SendClientKeyExchange(Ssl *ssl);
static int LIBSSL_NOINLINE ssl3SendClientVerify(Ssl *ssl);
static int LIBSSL_NOINLINE ssl3SendClientCertificate(Ssl *ssl);
static int LIBSSL_NOINLINE ssl3CheckCertAndAlgorithm(Ssl *ssl);
static int LIBSSL_NOINLINE ssl3ReadN(Ssl *ssl, int minBytes, int maxBytes, int extend);
static int LIBSSL_NOINLINE ssl3GetRecord(Ssl *ssl);
static int LIBSSL_NOINLINE ssl3DoWriteRecord(Ssl *ssl, int recType, const u8 *buf, int len);
static int LIBSSL_NOINLINE ssl3WritePending(Ssl *ssl, int recType, const u8 *buf, int len);
static int LIBSSL_NOINLINE ssl3DoChangeCipherSpec(Ssl *ssl);

/* 0x01D610 */
static const SslMethod *LIBSSL_NOINLINE ssl3GetClientMethod(int version)
{
    if (version == 0x0300) {
        return sceSsl_lib_B40D11EA();
    }
    return NULL;
}

/* 0x01D640 */
const SslMethod *LIBSSL_NOINLINE sceSsl_lib_B40D11EA(void)
{
    if (g_ssl3ClientMethodInit != 0) {
        g_ssl3ClientMethodInit = 0;
        g_ssl3ClientMethod = *ssl3BaseMethod();
        g_ssl3ClientMethod.getSslMethod = ssl3GetClientMethod;
        g_ssl3ClientMethod.sslConnect = ssl3Connect;
    }
    return &g_ssl3ClientMethod;
}

/* 0x01D6E8 */
int LIBSSL_NOINLINE ssl3Connect(Ssl *ssl)
{
    SceSslTimeSpec nowTime;
    SslInfoCb infoCb;
    int skipFlush = 0;
    int ret = -1;

    sceSsl_lib_A7BED83B(&nowTime);
    sceSsl_lib_DEA6EDB2(&nowTime, (int)sizeof(nowTime));
    sceSsl_lib_04E677AF();

    infoCb = ssl->infoCallback;
    if (infoCb == NULL) {
        infoCb = ssl->ctx->infoCallback;
    }

    if ((sceSsl_lib_104F749D(ssl) & 0x3000) == 0 ||
        (sceSsl_lib_104F749D(ssl) & 0x4000) != 0) {
        sceSsl_lib_54A7D8F3(ssl);
    }

    ssl->inHandshake++;

    for (;;) {
        int curState = ssl->state;
        int handled = 1;

        switch (curState) {
        case 12292:
            ssl->newSession = 1;
            ssl->state = 4096;
            ssl->ctx->stats.sessConnectRenegotiate++;
            /* fall through */
        case 4096:
        case 4099:
        case 16384:
        case 20480:
            ssl->server = 0;
            if (infoCb != NULL) {
                infoCb(ssl, 16, 1);
            }
            ssl->type = 4096;
            if (ssl->initBuf == NULL) {
                SslBufMem *buf = sslBufMemNew();
                if (buf == NULL) {
                    ret = -1;
                    handled = 0;
                    break;
                }
                if (sslBufMemGrow(buf, 2048) == 0) {
                    ret = -1;
                    handled = 0;
                    break;
                }
                ssl->initBuf = buf;
            }
            if (ssl3SetupBuffers(ssl) == 0) {
                ret = -1;
                handled = 0;
                break;
            }
            if (sslInitWbioBuffer(ssl, 0) == 0) {
                ret = -1;
                handled = 0;
                break;
            }
            if (ssl3InitFinishedMac(ssl) == 0) {
                ret = -1;
                handled = 0;
                break;
            }
            ssl->state = 4368;
            ssl->ctx->stats.sessConnect++;
            ssl->initNum = 0;
            break;

        case 4368:
        case 4369:
            ssl->shutdown = 0;
            ret = ssl3ClientHello(ssl);
            if (ret <= 0) {
                handled = 0;
                break;
            }
            ssl->state = 4384;
            ssl->initNum = 0;
            if (ssl->bbio != ssl->wbio) {
                ssl->wbio = sceSsl_lib_1EC64594(ssl->bbio, ssl->wbio);
            }
            break;

        case 4384:
        case 4385:
            ret = ssl3GetServerHello(ssl);
            if (ret <= 0) {
                handled = 0;
                break;
            }
            ssl->state = (ssl->hit != 0) ? 4560 : 4400;
            ssl->initNum = 0;
            break;

        case 4400:
        case 4401:
            if ((ssl->ssl3State->tmpNewCipher->algorithms & 0x100U) != 0) {
                skipFlush = 1;
            } else {
                ret = ssl3GetServerCertificate(ssl);
                if (ret <= 0) {
                    handled = 0;
                    break;
                }
            }
            ssl->state = 4416;
            ssl->initNum = 0;
            break;

        case 4416:
        case 4417:
            ret = ssl3GetKeyExchange(ssl);
            if (ret <= 0) {
                handled = 0;
                break;
            }
            ssl->state = 4432;
            ssl->initNum = 0;
            if (ssl3CheckCertAndAlgorithm(ssl) == 0) {
                ret = -1;
                handled = 0;
                break;
            }
            break;

        case 4432:
        case 4433:
            ret = ssl3GetCertificateRequest(ssl);
            if (ret <= 0) {
                handled = 0;
                break;
            }
            ssl->state = 4448;
            ssl->initNum = 0;
            break;

        case 4448:
        case 4449:
            ret = ssl3GetServerDone(ssl);
            if (ret <= 0) {
                handled = 0;
                break;
            }
            ssl->state = (ssl->ssl3State->tmpCertReq != 0) ? 4464 : 4480;
            ssl->initNum = 0;
            break;

        case 4464:
        case 4465:
        case 4466:
            ret = ssl3SendClientCertificate(ssl);
            if (ret <= 0) {
                handled = 0;
                break;
            }
            ssl->state = 4480;
            ssl->initNum = 0;
            break;

        case 4480:
        case 4481:
            ret = ssl3SendClientKeyExchange(ssl);
            if (ret <= 0) {
                handled = 0;
                break;
            }
            if (ssl->ssl3State->tmpCertReq == 1) {
                ssl->state = 4496;
            } else {
                ssl->state = 4512;
                ssl->ssl3State->changeCipherSpec = 0;
            }
            ssl->initNum = 0;
            break;

        case 4496:
        case 4497:
            ret = ssl3SendClientVerify(ssl);
            if (ret <= 0) {
                handled = 0;
                break;
            }
            ssl->state = 4512;
            ssl->initNum = 0;
            ssl->ssl3State->changeCipherSpec = 0;
            break;

        case 4512:
        case 4513:
            ret = ssl3SendChangeCipherSpec(ssl, 4512, 4513);
            if (ret <= 0) {
                handled = 0;
                break;
            }
            ssl->state = 4528;
            ssl->initNum = 0;
            ssl->session->cipher = ssl->ssl3State->tmpNewCipher;
            if (ssl->method->ssl3Enc->setupKeyBlock(ssl) == 0) {
                ret = -1;
                handled = 0;
                break;
            }
            if (ssl->method->ssl3Enc->changeCipherState(ssl, 18) == 0) {
                ret = -1;
                handled = 0;
                break;
            }
            break;

        case 4528:
        case 4529:
            ret = ssl3SendFinished(ssl, 4528, 4529,
                                   ssl->method->ssl3Enc->clientFinishedLabel,
                                   ssl->method->ssl3Enc->clientFinishedLabelLen);
            if (ret <= 0) {
                handled = 0;
                break;
            }
            if (ssl->unbuffered == 0) {
                ssl->state = 4352;
            }
            ssl->ssl3State->flags &= ~4U;
            if (ssl->hit != 0) {
                if (ssl->unbuffered != 0) {
                    ssl->state = 3;
                } else {
                    ssl->ssl3State->tmpNextState = 3;
                }
                if ((ssl->options & 0x00400000U) != 0) {
                    ssl->state = 3;
                    ssl->ssl3State->flags |= 4U;
                    ssl->ssl3State->delayBufPopRet = 0;
                }
            } else {
                if (ssl->unbuffered != 0) {
                    ssl->state = 4560;
                } else {
                    ssl->ssl3State->tmpNextState = 4560;
                }
                if ((ssl->options & 0x00800000U) != 0) {
                    ssl->state = 3;
                    ssl->ssl3State->flags |= 0x14U;
                    ssl->ssl3State->delayBufPopRet = 0;
                }
            }
            ssl->initNum = 0;
            break;

        case 4560:
        case 4561:
            ret = ssl3GetFinished(ssl, 4560, 4561);
            if (ret <= 0) {
                handled = 0;
                break;
            }
            ssl->state = (ssl->hit != 0) ? 4512 : 3;
            ssl->initNum = 0;
            break;

        case 4352:
            if (sceSsl_lib_9566709F(ssl->wbio, 3, 0, NULL) > 0) {
                ssl->rwstate = 2;
                if (sceSsl_lib_9566709F(ssl->wbio, 11, 0, NULL) <= 0) {
                    ret = -1;
                    handled = 0;
                    break;
                }
                ssl->rwstate = 1;
            }
            if (ssl->unbuffered != 0) {
                curState = ssl->rstatePrev;
            }
            ssl->state = ssl->ssl3State->tmpNextState;
            break;

        case 3:
            if ((ssl->ssl3State->flags & 0x10U) == 0) {
                ssl3CleanupKeyBlock(ssl);
                if (ssl->initBuf != NULL) {
                    sslBufMemFree(ssl->initBuf);
                    ssl->initBuf = NULL;
                }
                if ((ssl->ssl3State->flags & 4U) == 0) {
                    sslFreeWbioBuffer(ssl);
                }
                ssl->initNum = 0;
                ssl->newSession = 0;
                sslUpdateCache(ssl, 1);
                if (ssl->hit != 0) {
                    ssl->ctx->stats.sessHit++;
                }
                ssl->ctx->stats.sessConnectGood++;
            }
            ssl->handshakeFunc = ssl3Connect;
            ret = 1;
            if (infoCb != NULL) {
                infoCb(ssl, 32, 1);
            }
            handled = 0;
            break;

        default:
            sceSsl_lib_9CDCA0D7(20, 132, 255, NULL, 0);
            ret = -1;
            handled = 0;
            break;
        }

        if (handled == 0) {
            break;
        }

        if (ssl->ssl3State->tmpReuseMessage == 0 && skipFlush == 0) {
            if (ssl->unbuffered != 0) {
                ssl->rwstate = 2;
                ret = sceSsl_lib_9566709F(ssl->wbio, 11, 0, NULL);
                if (ret <= 0) {
                    if (sceSsl_lib_089FF1F1(ssl->wbio) != 0) {
                        ssl->ssl3State->tmpNextState = ssl->state;
                        ssl->rstatePrev = curState;
                        ssl->state = 4352;
                    }
                    break;
                }
                ssl->rwstate = 1;
            }
            if (infoCb != NULL && ssl->state != curState) {
                int savedState = ssl->state;
                ssl->state = curState;
                infoCb(ssl, 4097, 1);
                ssl->state = savedState;
            }
        }
        skipFlush = 0;
    }

    if (infoCb != NULL) {
        infoCb(ssl, 4098, ret);
    }
    ssl->inHandshake--;
    return ret;
}

/* 0x01DE94 */
static int LIBSSL_NOINLINE ssl3ClientHello(Ssl *ssl)
{
    u8 *bufBase = (u8 *)ssl->initBuf->data;
    SceSslTimeSpec nowTime;
    u8 *cursor;
    int sessIdLen;
    int cipherBytes;
    u32 bodyLen;

    if (ssl->state != 4368) {
        return ssl3DoWrite(ssl, 22);
    }

    if (ssl->session == NULL ||
        ssl->session->sslVersion != ssl->version ||
        ssl->session->notResumable != 0) {
        if (sslGetNewSession(ssl, 0) == 0) {
            return -1;
        }
    }

    sceSsl_lib_A7BED83B(&nowTime);
    sceSsl_lib_9EF66756(&nowTime, 3, ssl->ssl3State->clientRandom, 8);
    sceSsl_lib_21F2A35C(ssl->ssl3State->clientRandom, 24);

    bufBase[4] = (u8)(ssl->version >> 8);
    bufBase[5] = (u8)(ssl->version & 0xFF);
    ssl->clientVersion = ssl->version;
    sslMemcpy(bufBase + 6, ssl->ssl3State->clientRandom, 32U);

    sessIdLen = (ssl->newSession == 0) ? (int)ssl->session->sessionIdLength : 0;
    bufBase[38] = (u8)sessIdLen;
    cursor = bufBase + 39;
    if (sessIdLen != 0) {
        sslMemcpy(cursor, ssl->session->sessionId, (u32)sessIdLen);
        cursor += sessIdLen;
    }

    cipherBytes = sslCipherListToBytes(ssl, sslGetCiphers(ssl), cursor + 2);
    if (cipherBytes == 0) {
        sceSsl_lib_9CDCA0D7(20, 131, 181, NULL, 0);
        return -1;
    }
    cursor[0] = (u8)(cipherBytes >> 8);
    cursor[1] = (u8)(cipherBytes & 0xFF);
    cursor[cipherBytes + 2] = 1;
    cursor[cipherBytes + 3] = 0;
    cursor += cipherBytes + 4;

    bodyLen = (u32)(cursor - (bufBase + 4));
    bufBase[0] = 1;
    bufBase[1] = (u8)(bodyLen >> 16);
    bufBase[2] = (u8)(bodyLen >> 8);
    bufBase[3] = (u8)(bodyLen & 0xFF);

    ssl->state = 4369;
    ssl->initNum = (int)(cursor - bufBase);
    ssl->initOff = 0;
    return ssl3DoWrite(ssl, 22);
}

/* 0x01E0EC */
static int LIBSSL_NOINLINE ssl3GetServerHello(Ssl *ssl)
{
    int ok = 0;
    int msgLen;
    u8 *basePtr;
    u8 *cursor;
    u32 sessIdLen;
    const SslCipher *cipher;
    int errReason = 0;
    int alertCode = 47;

    msgLen = ssl3GetMessage(ssl, 4384, 4385, 2, 1024U, &ok);
    if (ok == 0) {
        return msgLen;
    }

    basePtr = (u8 *)ssl->initBuf->data;
    if (basePtr[0] != (u8)(ssl->version >> 8) || basePtr[1] != (u8)(ssl->version & 0xFF)) {
        sceSsl_lib_9CDCA0D7(20, 146, 266, NULL, 0);
        ssl->version = (ssl->version & 0xFF00) | basePtr[1];
        ssl3SendAlert(ssl, 2, 70);
        return -1;
    }

    sslMemcpy(ssl->ssl3State->serverRandom, basePtr + 2, 32U);
    cursor = basePtr + 34;
    sessIdLen = cursor[0];
    cursor++;

    if (sessIdLen >= 33U) {
        errReason = 1105;
    } else {
        if (sessIdLen != 0 &&
            sessIdLen == ssl->session->sessionIdLength &&
            sslMemcmp(cursor, ssl->session->sessionId, (int)sessIdLen) == 0) {
            ssl->hit = 1;
        } else {
            ssl->hit = 0;
            if (ssl->session->sessionIdLength != 0) {
                if (sslGetNewSession(ssl, 0) == 0) {
                    ssl3SendAlert(ssl, 2, 80);
                    return -1;
                }
            }
            ssl->session->sessionIdLength = sessIdLen;
            sslMemcpy(ssl->session->sessionId, cursor, sessIdLen);
        }
        cursor += sessIdLen;

        cipher = ssl->method->getCipherByChar(cursor);
        if (cipher == NULL) {
            errReason = 248;
        } else {
            cursor += ssl->method->putCipherByChar(NULL, NULL);
            if (sslSkFind(sslGetCiphersById(ssl), (void *)cipher) < 0) {
                errReason = 261;
            } else if (ssl->hit != 0 && ssl->session->cipher != cipher && (ssl->options & 8U) == 0) {
                errReason = 197;
            } else {
                ssl->ssl3State->tmpNewCipher = cipher;
                if (cursor[0] != 0) {
                    errReason = 257;
                } else {
                    cursor++;
                    if (cursor != basePtr + msgLen) {
                        sceSsl_lib_9CDCA0D7(20, 146, 115, NULL, 0);
                        return -1;
                    }
                    return 1;
                }
            }
        }
    }

    sceSsl_lib_9CDCA0D7(20, 146, errReason, NULL, 0);
    ssl3SendAlert(ssl, 2, alertCode);
    return -1;
}

/* 0x01E3E8 */
static int LIBSSL_NOINLINE ssl3GetServerCertificate(Ssl *ssl)
{
    int ok = 0;
    int msgLen;
    SslStack *certStack = NULL;
    void *parsedCert = NULL;
    int errReason = 0;
    int alertCode = 50;
    int needAlert = 0;
    int ret = -1;

    msgLen = ssl3GetMessage(ssl, 4400, 4401, -1, 0x19000U, &ok);
    if (ok == 0) {
        return msgLen;
    }
    if (ssl->ssl3State->tmpMessageType == 12) {
        ssl->ssl3State->tmpReuseMessage = 1;
        return 1;
    }
    if (ssl->ssl3State->tmpMessageType != 11) {
        errReason = 114;
        alertCode = 10;
        needAlert = 1;
    } else {
        const u8 *cursor = (const u8 *)ssl->initBuf->data;
        u32 totalCertBytes;
        u32 offset = 0;

        certStack = sslSkNew(NULL);
        if (certStack == NULL) {
            sceSsl_lib_9CDCA0D7(20, 144, 33, NULL, 0);
            return -1;
        }
        totalCertBytes = ((u32)cursor[0] << 16) | ((u32)cursor[1] << 8) | cursor[2];
        cursor += 3;
        if (totalCertBytes + 3U != (u32)msgLen) {
            errReason = 159;
            alertCode = 50;
            needAlert = 1;
        } else {
            int parseOk = 1;
            while (offset < totalCertBytes) {
                u32 certLen = ((u32)cursor[0] << 16) | ((u32)cursor[1] << 8) | cursor[2];
                const u8 *certStart = cursor + 3;
                const u8 *derCursor = certStart;

                if (offset + certLen + 3U > totalCertBytes) {
                    errReason = 135;
                    alertCode = 50;
                    needAlert = 1;
                    parseOk = 0;
                    break;
                }
                parsedCert = sceSsl_lib_6038DEDE(NULL, &derCursor, certLen);
                if (parsedCert == NULL) {
                    errReason = 2061;
                    alertCode = 42;
                    needAlert = 1;
                    parseOk = 0;
                    break;
                }
                if (derCursor != certStart + certLen) {
                    errReason = 135;
                    alertCode = 50;
                    needAlert = 1;
                    parseOk = 0;
                    break;
                }
                if (sslSkPush(certStack, parsedCert) == 0) {
                    sceSsl_lib_9CDCA0D7(20, 144, 33, NULL, 0);
                    parseOk = 0;
                    break;
                }
                parsedCert = NULL;
                offset += certLen + 3U;
                cursor = derCursor;
            }

            if (parseOk != 0) {
                if (ssl->verifyMode != 0 && sslVerifyCertChain(ssl, certStack) == 0) {
                    alertCode = sslVerifyAlarmType(ssl->verifyResult);
                    errReason = 134;
                    needAlert = 1;
                } else {
                    SslCert *sessCert = sslCertNew();
                    if (sessCert != NULL) {
                        void *leafCert;
                        SslEvpPkey *pubKey;

                        if (ssl->session->sessCert != NULL) {
                            sslCertFree(ssl->session->sessCert);
                        }
                        sessCert->certChain = certStack;
                        certStack = NULL;
                        leafCert = sessCert->certChain->data[0];
                        ssl->session->sessCert = sessCert;

                        pubKey = sceSsl_lib_B87FFE37(leafCert);
                        if (pubKey == NULL || sslEvpPkeyMissingParams(pubKey) != 0) {
                            errReason = 239;
                            alertCode = 2;
                            needAlert = 1;
                        } else {
                            int certSlot = sslCertType(leafCert, pubKey);
                            if (certSlot < 0) {
                                errReason = 247;
                                alertCode = 2;
                                needAlert = 1;
                            } else {
                                sessCert->peerKeyType = certSlot;
                                sceSsl_lib_999D7614(leafCert);
                                if (sessCert->pkeys[certSlot].certObj != NULL) {
                                    sceSsl_lib_C1A2E14B(sessCert->pkeys[certSlot].certObj);
                                }
                                sessCert->activeKey = &sessCert->pkeys[certSlot];
                                sessCert->pkeys[certSlot].certObj = leafCert;
                                if (ssl->session != NULL && ssl->session->peer != NULL) {
                                    sceSsl_lib_C1A2E14B(ssl->session->peer);
                                }
                                sceSsl_lib_999D7614(leafCert);
                                ssl->session->peer = leafCert;
                                return 1;
                            }
                        }
                    }
                }
            }
        }
    }

    if (needAlert != 0) {
        sceSsl_lib_9CDCA0D7(20, 144, errReason, NULL, 0);
        ssl3SendAlert(ssl, 2, alertCode);
    }
    if (parsedCert != NULL) {
        sceSsl_lib_C1A2E14B(parsedCert);
    }
    if (certStack != NULL) {
        sslSkPopFree(certStack, sceSsl_lib_C1A2E14B);
    }
    return ret;
}

/* 0x01E7A0 */
static int LIBSSL_NOINLINE ssl3GetKeyExchange(Ssl *ssl)
{
    u8 mdBuf[48];
    int ok = 0;
    u32 digestLen = 0;
    int msgLen;
    u8 *basePtr;
    u8 *cursor;
    u32 alg;
    u32 algMask;
    SslCert *sessCert;
    SslRsa *tmpRsa;
    SslEvpPkey *pubKey = NULL;
    int paramLen = 0;
    int errReason = 0;
    int alertCode = 50;
    int sendAlert = 0;

    msgLen = ssl3GetMessage(ssl, 4416, 4417, -1, 10240U, &ok);
    if (ok == 0) {
        return msgLen;
    }
    if (ssl->ssl3State->tmpMessageType != 12) {
        ssl->ssl3State->tmpReuseMessage = 1;
        return 1;
    }

    basePtr = (u8 *)ssl->initBuf->data;
    cursor = basePtr;
    sessCert = ssl->session->sessCert;
    if (sessCert != NULL) {
        if (sessCert->rsaTmp != NULL) {
            sslRsaFree(sessCert->rsaTmp);
            ssl->session->sessCert->rsaTmp = NULL;
        }
        if (ssl->session->sessCert->rsaTmpExport != NULL) {
            sslRsaFree(ssl->session->sessCert->rsaTmpExport);
            ssl->session->sessCert->rsaTmpExport = NULL;
        }
        if (ssl->session->sessCert->ephemeralRsa != NULL) {
            sslRsaFree(ssl->session->sessCert->ephemeralRsa);
            ssl->session->sessCert->ephemeralRsa = NULL;
        }
    } else {
        ssl->session->sessCert = sslCertNew();
    }

    alg = ssl->ssl3State->tmpNewCipher->algorithms;
    algMask = ssl->ssl3State->tmpNewCipher->algStrength;

    if ((alg & 1U) != 0) {
        u32 modLen;
        u32 expLen;

        tmpRsa = sslRsaNew();
        if (tmpRsa == NULL) {
            sceSsl_lib_9CDCA0D7(20, 141, 33, NULL, 0);
            return -1;
        }
        modLen = ((u32)cursor[0] << 8) | cursor[1];
        cursor += 2;
        if (msgLen < (int)(modLen + 2U)) {
            errReason = 121;
            sendAlert = 1;
        } else {
            tmpRsa->modN = sslBnBinToBn(cursor, (int)modLen, tmpRsa->modN);
            if (tmpRsa->modN == NULL) {
                sceSsl_lib_9CDCA0D7(20, 141, 2051, NULL, 0);
                return -1;
            }
            cursor += modLen;
            expLen = ((u32)cursor[0] << 8) | cursor[1];
            cursor += 2;
            paramLen = (int)(modLen + 2U + expLen + 2U);
            if (msgLen < paramLen) {
                errReason = 120;
                sendAlert = 1;
            } else {
                tmpRsa->pubE = sslBnBinToBn(cursor, (int)expLen, tmpRsa->pubE);
                if (tmpRsa->pubE == NULL) {
                    sceSsl_lib_9CDCA0D7(20, 141, 2051, NULL, 0);
                    return -1;
                }
                cursor += expLen;
                msgLen -= paramLen;
                if ((alg & 0x20U) == 0) {
                    sceSsl_lib_9CDCA0D7(20, 141, 157, NULL, 0);
                    return -1;
                }
                pubKey = sceSsl_lib_B87FFE37(ssl->session->sessCert->pkeys[0].certObj);
                if ((alg & 0x100000U) != 0) {
                    if ((algMask & 0xFF00U) == 0x400U) {
                        ssl->session->sessCert->ephemeralRsa = tmpRsa;
                    } else {
                        ssl->session->sessCert->rsaTmpExport = tmpRsa;
                    }
                } else {
                    ssl->session->sessCert->rsaTmp = tmpRsa;
                }
            }
        }
    } else if ((alg & 0x80U) != 0) {
        errReason = 235;
        alertCode = 40;
        sendAlert = 1;
    }

    if (sendAlert == 0) {
        if (pubKey != NULL) {
            u32 sigLen = ((u32)cursor[0] << 8) | cursor[1];
            int maxSigLen = sslEvpPkeySize(pubKey);
            int remBytes = msgLen - 2;

            cursor += 2;
            if ((int)sigLen != remBytes || maxSigLen < remBytes || remBytes <= 0) {
                errReason = 264;
                alertCode = 50;
                sendAlert = 1;
            } else if (pubKey->type == 6) {
                u8 *mdOut = mdBuf;
                int totalDigest = 0;
                int pass;

                for (pass = 0; pass < 2; pass++) {
                    const SslEvpMd *md = (pass == 0) ? ssl->ctx->md5 : ssl->ctx->sha1;
                    if (sslEvpDigestInit(ssl->mdCtx, md) == 0) {
                        ssl3SendAlert(ssl, 2, 80);
                        return -1;
                    }
                    sslEvpDigestUpdate(ssl->mdCtx, ssl->ssl3State->clientRandom, 32U);
                    sslEvpDigestUpdate(ssl->mdCtx, ssl->ssl3State->serverRandom, 32U);
                    sslEvpDigestUpdate(ssl->mdCtx, basePtr, (u32)paramLen);
                    sslEvpDigestFinal(ssl->mdCtx, mdOut, &digestLen);
                    totalDigest += (int)digestLen;
                    mdOut += digestLen;
                }
                {
                    int decLen = sslRsaPublicDecrypt(remBytes, cursor, cursor, (SslRsa *)pubKey->pkey, 1);
                    if (decLen <= 0) {
                        errReason = 118;
                        alertCode = 51;
                        sendAlert = 1;
                    } else if (totalDigest != decLen || sslMemcmp(cursor, mdBuf, totalDigest) != 0) {
                        errReason = 123;
                        alertCode = 51;
                        sendAlert = 1;
                    } else {
                        return 1;
                    }
                }
            } else {
                sceSsl_lib_9CDCA0D7(20, 141, 157, NULL, 0);
                return -1;
            }
        } else if ((alg & 0x100U) != 0) {
            if (msgLen == 0) {
                return 1;
            }
            errReason = 153;
            alertCode = 50;
            sendAlert = 1;
        } else {
            sceSsl_lib_9CDCA0D7(20, 141, 157, NULL, 0);
            return -1;
        }
    }

    sceSsl_lib_9CDCA0D7(20, 141, errReason, NULL, 0);
    ssl3SendAlert(ssl, 2, alertCode);
    return -1;
}

/* 0x01EC24 */
static int LIBSSL_NOINLINE ssl3GetCertificateRequest(Ssl *ssl)
{
    int ok = 0;
    int msgLen;
    SslStack *caList = NULL;
    int errReason = 0;
    int ret = 0;

    msgLen = ssl3GetMessage(ssl, 4432, 4433, -1, 10240U, &ok);
    if (ok == 0) {
        return msgLen;
    }
    ssl->ssl3State->tmpCertReq = 0;
    if (ssl->ssl3State->tmpMessageType == 14) {
        ssl->ssl3State->tmpReuseMessage = 1;
        return 1;
    }
    if (ssl->ssl3State->tmpMessageType != 13) {
        ssl3SendAlert(ssl, 2, 10);
        sceSsl_lib_9CDCA0D7(20, 135, 262, NULL, 0);
        return 0;
    }
    if (ssl->version >= 0x0301 && (ssl->ssl3State->tmpNewCipher->algorithms & 0x100U) != 0) {
        ssl3SendAlert(ssl, 2, 10);
        sceSsl_lib_9CDCA0D7(20, 135, 232, NULL, 0);
        return 0;
    }

    caList = sslSkNew(ssl3CaNameCmp);
    if (caList == NULL) {
        errReason = 33;
    } else {
        const u8 *cursor = (const u8 *)ssl->initBuf->data;
        u32 typeCount = cursor[0];
        u32 caTotalLen;
        u32 idx;

        cursor++;
        if (typeCount >= 8U) {
            typeCount = 7U;
        }
        for (idx = 0; idx < typeCount; idx++) {
            ssl->ssl3State->tmpCertTypes[idx] = cursor[idx];
        }
        cursor += typeCount;
        caTotalLen = ((u32)cursor[0] << 8) | cursor[1];
        cursor += 2;
        if (caTotalLen + typeCount + 3U != (u32)msgLen) {
            ssl3SendAlert(ssl, 2, 50);
            errReason = 159;
        } else {
            u32 parsedBytes = 0;
            int parseOk = 1;

            while (parsedBytes < caTotalLen) {
                u32 dnLen = ((u32)cursor[0] << 8) | cursor[1];
                const u8 *dnStart = cursor + 2;
                const u8 *dnCursor = dnStart;
                void *caName;

                if (parsedBytes + dnLen + 2U > caTotalLen) {
                    if ((ssl->options & 0x20000000U) == 0) {
                        ssl3SendAlert(ssl, 2, 50);
                        errReason = 132;
                        parseOk = 0;
                    } else {
                        sceSsl_lib_04E677AF();
                    }
                    break;
                }
                caName = sceSsl_lib_D579CE04(NULL, &dnCursor, (int)dnLen);
                if (caName == NULL) {
                    if ((ssl->options & 0x20000000U) == 0) {
                        ssl3SendAlert(ssl, 2, 50);
                        errReason = 2061;
                        parseOk = 0;
                    } else {
                        sceSsl_lib_04E677AF();
                    }
                    break;
                }
                if (dnCursor != dnStart + dnLen) {
                    ssl3SendAlert(ssl, 2, 50);
                    errReason = 131;
                    parseOk = 0;
                    break;
                }
                if (sslSkPush(caList, caName) == 0) {
                    errReason = 33;
                    parseOk = 0;
                    break;
                }
                parsedBytes += dnLen + 2U;
                cursor = dnCursor;
            }

            if (parseOk != 0) {
                ssl->ssl3State->tmpCertReq = 1;
                ssl->ssl3State->tmpNumCertTypes = (int)typeCount;
                if (ssl->ssl3State->tmpCaNames != NULL) {
                    sslSkPopFree(ssl->ssl3State->tmpCaNames, sceSsl_lib_ED95A457);
                }
                ssl->ssl3State->tmpCaNames = caList;
                caList = NULL;
                ret = 1;
            }
        }
    }

    if (ret == 0) {
        sceSsl_lib_9CDCA0D7(20, 135, errReason, NULL, 0);
    }
    if (caList != NULL) {
        sslSkPopFree(caList, sceSsl_lib_ED95A457);
    }
    return ret;
}

/* 0x01EF5C */
static int LIBSSL_NOINLINE ssl3CaNameCmp(const void *lhs, const void *rhs)
{
    const void *const *leftPtr = (const void *const *)lhs;
    const void *const *rightPtr = (const void *const *)rhs;
    return sceSsl_lib_7D0E41F4(*leftPtr, *rightPtr);
}

/* 0x01EF7C */
static int LIBSSL_NOINLINE ssl3GetServerDone(Ssl *ssl)
{
    int ok = 0;
    int msgLen = ssl3GetMessage(ssl, 4448, 4449, 14, 30U, &ok);

    if (ok == 0) {
        return msgLen;
    }
    if (msgLen > 0) {
        ssl3SendAlert(ssl, 2, 50);
        sceSsl_lib_9CDCA0D7(20, 145, 159, NULL, 0);
    }
    return 1;
}

/* 0x01F000 */
static int LIBSSL_NOINLINE ssl3SendClientKeyExchange(Ssl *ssl)
{
    u8 preMasterSecret[48];
    u8 *bufBase;
    u8 *encDst;
    u32 alg;
    SslRsa *rsaKey = NULL;
    int encLen;
    int masterLen;

    if (ssl->state != 4480) {
        return ssl3DoWrite(ssl, 22);
    }

    bufBase = (u8 *)ssl->initBuf->data;
    encDst = bufBase + 4;
    alg = ssl->ssl3State->tmpNewCipher->algorithms;

    if ((alg & 1U) == 0) {
        ssl3SendAlert(ssl, 2, 40);
        sceSsl_lib_9CDCA0D7(20, 153, 157, NULL, 0);
        return -1;
    }

    if ((alg & 0x100000U) != 0 && (ssl->ssl3State->tmpNewCipher->algStrength & 0xFF00U) == 0x400U) {
        rsaKey = ssl->session->sessCert->ephemeralRsa;
    }
    if (rsaKey == NULL && (alg & 0x100000U) != 0) {
        rsaKey = ssl->session->sessCert->rsaTmpExport;
    }
    if (rsaKey == NULL && (alg & 0x100000U) == 0) {
        rsaKey = ssl->session->sessCert->rsaTmp;
    }
    if (rsaKey == NULL) {
        SslEvpPkey *pubKey = sceSsl_lib_B87FFE37(ssl->session->sessCert->pkeys[0].certObj);
        if (pubKey != NULL && pubKey->type == 6) {
            rsaKey = (SslRsa *)pubKey->pkey;
        }
    }
    if (rsaKey == NULL) {
        sceSsl_lib_9CDCA0D7(20, 153, 157, NULL, 0);
        return -1;
    }

    preMasterSecret[0] = (u8)(ssl->clientVersion >> 8);
    preMasterSecret[1] = (u8)(ssl->clientVersion & 0xFF);
    sceSsl_lib_21F2A35C(preMasterSecret + 2, 46);
    ssl->session->masterKeyLength = 48;

    if (ssl->version >= 0x0301) {
        encDst = bufBase + 6;
    }
    encLen = sslRsaPublicEncrypt(48, preMasterSecret, encDst, rsaKey, 1);
    if ((ssl->options & 0x08000000U) != 0) {
        encDst[1]++;
    }
    if ((ssl->options & 0x10000000U) != 0) {
        preMasterSecret[0] = 112;
    }
    if (encLen <= 0) {
        sceSsl_lib_9CDCA0D7(20, 153, 119, NULL, 0);
        return -1;
    }
    if (ssl->version >= 0x0301) {
        bufBase[4] = (u8)(encLen >> 8);
        bufBase[5] = (u8)(encLen & 0xFF);
        encLen += 2;
    }

    masterLen = ssl->method->ssl3Enc->generateMasterSecret(ssl, ssl->session->masterKey,
                                                           preMasterSecret, 48);
    ssl->session->masterKeyLength = masterLen;
    if (masterLen == 0) {
        return -1;
    }
    sslMemset(preMasterSecret, 0, sizeof(preMasterSecret));

    bufBase[0] = 16;
    bufBase[1] = (u8)(encLen >> 16);
    bufBase[2] = (u8)(encLen >> 8);
    bufBase[3] = (u8)(encLen & 0xFF);
    ssl->state = 4481;
    ssl->initNum = encLen + 4;
    ssl->initOff = 0;
    return ssl3DoWrite(ssl, 22);
}

/* 0x01F2A0 */
static int LIBSSL_NOINLINE ssl3SendClientVerify(Ssl *ssl)
{
    u8 digestPair[36];
    u8 *bufBase;
    SslEvpPkey *privKey;
    int sigLen;

    if (ssl->state != 4496) {
        return ssl3DoWrite(ssl, 22);
    }

    bufBase = (u8 *)ssl->initBuf->data;
    privKey = ssl->cert->activeKey->privateKey;

    if (ssl->method->ssl3Enc->certVerifyMac(ssl, ssl->ssl3State->finishDg2, digestPair + 16) == 0) {
        return -1;
    }
    if (privKey->type != 6) {
        sceSsl_lib_9CDCA0D7(20, 154, 157, NULL, 0);
        return -1;
    }
    if (ssl->method->ssl3Enc->certVerifyMac(ssl, ssl->ssl3State->finishDg1, digestPair) == 0) {
        return -1;
    }

    sigLen = sslRsaPrivateEncrypt(36, digestPair, bufBase + 6, (SslRsa *)privKey->pkey, 1);
    if (sigLen <= 0) {
        sceSsl_lib_9CDCA0D7(20, 154, 2052, NULL, 0);
        return -1;
    }

    bufBase[4] = (u8)(sigLen >> 8);
    bufBase[5] = (u8)(sigLen & 0xFF);
    bufBase[0] = 15;
    bufBase[1] = (u8)((sigLen + 2) >> 16);
    bufBase[2] = (u8)((sigLen + 2) >> 8);
    bufBase[3] = (u8)((sigLen + 2) & 0xFF);
    ssl->initNum = sigLen + 6;
    ssl->initOff = 0;
    return ssl3DoWrite(ssl, 22);
}

/* 0x01F3F0 */
static int LIBSSL_NOINLINE ssl3SendClientCertificate(Ssl *ssl)
{
    void *clientCert = NULL;
    SslEvpPkey *clientKey = NULL;

    if (ssl->state == 4464) {
        if (ssl->cert != NULL &&
            ssl->cert->activeKey->certObj != NULL &&
            ssl->cert->activeKey->privateKey != NULL) {
            ssl->state = 4466;
        } else {
            ssl->state = 4465;
        }
    }

    if (ssl->state == 4465) {
        int cbRet = 0;
        if (ssl->ctx->clientCertCb != NULL) {
            cbRet = ssl->ctx->clientCertCb(ssl, &clientCert, &clientKey);
            if (cbRet < 0) {
                ssl->rwstate = 4;
                return -1;
            }
        }
        ssl->rwstate = 1;
        if (cbRet == 1 && clientKey != NULL && clientCert != NULL) {
            ssl->state = 4465;
            if (sceSsl_lib_C0ABBB57(ssl, clientCert) == 0) {
                cbRet = 0;
            } else {
                cbRet = (sceSsl_lib_8390B321(ssl, clientKey) != 0) ? 1 : 0;
            }
        } else if (cbRet == 1) {
            cbRet = 0;
            sceSsl_lib_9CDCA0D7(20, 152, 106, NULL, 0);
        }

        if (clientCert != NULL) {
            sceSsl_lib_C1A2E14B(clientCert);
        }
        if (clientKey != NULL) {
            sceSsl_lib_9307DE23(clientKey);
        }
        if (cbRet == 0) {
            if (ssl->version == 0x0300) {
                ssl->ssl3State->tmpCertReq = 0;
                return ssl3SendAlert(ssl, 1, 41);
            }
            ssl->ssl3State->tmpCertReq = 2;
        }
        ssl->state = 4466;
    }

    if (ssl->state == 4466) {
        void *certToSend = NULL;
        ssl->state = 4467;
        if (ssl->ssl3State->tmpCertReq != 2) {
            certToSend = ssl->cert->activeKey->certObj;
        }
        ssl->initNum = ssl3OutputCertChain(ssl, certToSend);
        ssl->initOff = 0;
    }

    return ssl3DoWrite(ssl, 22);
}

/* 0x01F5EC */
static int LIBSSL_NOINLINE ssl3CheckCertAndAlgorithm(Ssl *ssl)
{
    SslCert *sessCert = ssl->session->sessCert;
    u32 alg;
    u32 algStrength;
    SslRsa *rsaTmp;
    void *peerCert;
    int sigType;
    SslEvpPkey *pubKey;
    int certMask;
    int errReason = 0;

    if (sessCert == NULL) {
        sceSsl_lib_9CDCA0D7(20, 130, 157, NULL, 0);
        return 0;
    }

    alg = ssl->ssl3State->tmpNewCipher->algorithms;
    algStrength = ssl->ssl3State->tmpNewCipher->algStrength;
    if ((alg & 0x300U) != 0) {
        return 1;
    }

    if ((alg & 0x100000U) == 0) {
        rsaTmp = sessCert->rsaTmp;
    } else if ((algStrength & 0xFF00U) != 0x400U) {
        rsaTmp = sessCert->rsaTmpExport;
    } else {
        rsaTmp = sessCert->ephemeralRsa;
        if (rsaTmp == NULL) {
            rsaTmp = sessCert->rsaTmpExport;
        }
    }

    peerCert = sessCert->pkeys[sessCert->peerKeyType].certObj;
    sigType = sslX509GetSignatureType(peerCert);
    pubKey = sceSsl_lib_B87FFE37(peerCert);
    certMask = sslCertGetMask(sigType, pubKey);

    if ((alg & 0x20U) != 0 && (certMask & 0x11) != 0x11) {
        errReason = 170;
    } else if ((alg & 1U) != 0 && (certMask & 0x21) != 0x21 && rsaTmp == NULL) {
        errReason = 169;
    } else if ((alg & 0x100000U) != 0 && (certMask & 0x1000) == 0) {
        if ((alg & 1U) == 0) {
            errReason = 250;
        } else if (rsaTmp == NULL || (u32)sslRsaSize(rsaTmp) > (algStrength & 0xFF00U)) {
            errReason = 167;
        }
    }

    if (errReason != 0) {
        sceSsl_lib_9CDCA0D7(20, 130, errReason, NULL, 0);
        ssl3SendAlert(ssl, 2, 40);
        return 0;
    }
    return 1;
}

/* 0x01F7B4 */
static int LIBSSL_NOINLINE ssl3ReadN(Ssl *ssl, int minBytes, int maxBytes, int extend)
{
    Ssl3State *ssl3State = ssl->ssl3State;
    int left = ssl3State->rbufLeft;
    int readGoal;
    int baseOff = 0;
    int haveBytes = 0;

    if (left >= minBytes) {
        if (extend != 0) {
            ssl->packetLength += (u32)minBytes;
        } else {
            ssl->packetLength = (u32)minBytes;
            ssl->packet = ssl3State->rbufBuf + ssl3State->rbufOffset;
        }
        ssl3State->rbufOffset += minBytes;
        ssl3State->rbufLeft -= minBytes;
        return minBytes;
    }

    readGoal = (maxBytes < ssl3State->rbufLen) ? maxBytes : ssl3State->rbufLen;

    if (left != 0 || (ssl->packetLength != 0 && extend != 0)) {
        haveBytes = ssl3State->rbufLeft;
        if (extend != 0) {
            baseOff = (int)ssl->packetLength;
            if (ssl->packet != ssl3State->rbufBuf) {
                sslMemcpy(ssl3State->rbufBuf, ssl->packet, (u32)(haveBytes + baseOff));
            }
        } else if (ssl3State->rbufOffset != 0) {
            sslMemcpy(ssl3State->rbufBuf, ssl3State->rbufBuf + ssl3State->rbufOffset, (u32)left);
            ssl3State->rbufOffset = 0;
        }
        ssl3State->rbufLeft = 0;
    }

    ssl->packet = ssl3State->rbufBuf;

    while (haveBytes < minBytes) {
        int got;
        if (ssl->rbio != NULL) {
            ssl->rwstate = 3;
            got = sceSsl_lib_675AC5AA(ssl->rbio, ssl3State->rbufBuf + baseOff + haveBytes,
                                      readGoal - haveBytes);
        } else {
            sceSsl_lib_9CDCA0D7(20, 150, 211, NULL, 0);
            got = -1;
        }
        if (got <= 0) {
            ssl3State->rbufLeft += haveBytes;
            return got;
        }
        haveBytes += got;
    }

    if (minBytes >= haveBytes) {
        ssl3State->rbufLeft = 0;
        ssl3State->rbufOffset = 0;
    } else {
        ssl3State->rbufLeft = haveBytes - minBytes;
        ssl3State->rbufOffset = minBytes + baseOff;
    }
    ssl->packetLength += (u32)minBytes;
    ssl->rwstate = 1;
    return minBytes;
}

/* 0x01F9BC */
static int LIBSSL_NOINLINE ssl3GetRecord(Ssl *ssl)
{
    u8 computedMac[32];
    Ssl3Record *rec = &ssl->ssl3State->rrec;
    u32 extraSlack = ((ssl->options & 0x20U) != 0) ? 16384U : 0U;
    int skipMac = 0;
    int errReason = 0;
    int alertCode = 22;

    for (;;) {
        if (ssl->rstate != 241 || ssl->packetLength < 5U) {
            int nRead = ssl3ReadN(ssl, 5, 18437, 0);
            u8 *hdr;
            int recVersion;

            if (nRead <= 0) {
                return nRead;
            }
            ssl->rstate = 241;
            hdr = ssl->packet;
            rec->type = hdr[0];
            recVersion = ((int)hdr[1] << 8) | hdr[2];
            rec->length = ((u32)hdr[3] << 8) | hdr[4];

            if (ssl->firstPacket != 0) {
                ssl->firstPacket = 0;
            } else if (recVersion != ssl->version) {
                sceSsl_lib_9CDCA0D7(20, 143, 267, NULL, 0);
                ssl->version = recVersion;
                ssl3SendAlert(ssl, 2, 70);
                return -1;
            }

            if ((recVersion >> 8) != 3) {
                sceSsl_lib_9CDCA0D7(20, 143, 267, NULL, 0);
                return -1;
            }
            if (rec->length > extraSlack + 18432U) {
                errReason = 198;
                alertCode = 22;
                break;
            }
            if ((u32)(ssl->ssl3State->rbufLen - 5) < rec->length) {
                if (ssl3ResizeReadBuffer(ssl, (int)(rec->length + 5U)) == 0) {
                    sceSsl_lib_9CDCA0D7(20, 143, 33, NULL, 0);
                    ssl3SendAlert(ssl, 2, 80);
                    return -1;
                }
            }
            ssl->rstate = 241;
        }

        if (ssl->rstate == 241) {
            if (ssl->packetLength - 5U < rec->length) {
                int nBody = ssl3ReadN(ssl, (int)rec->length, (int)rec->length, 1);
                if (nBody <= 0) {
                    return nBody;
                }
            }
            ssl->rstate = 240;
        }

        ssl->rstate = 240;
        rec->input = ssl->packet + 5;
        if (rec->length > extraSlack + 18432U) {
            errReason = 150;
            alertCode = 22;
            break;
        }
        rec->data = rec->input;

        {
            int decBad = (ssl->method->ssl3Enc->enc(ssl, 0) == 0);
            if (ssl->session == NULL || ssl->encReadCtx == NULL || ssl->readHash == NULL) {
                skipMac = 1;
            }
            if (skipMac == 0) {
                u32 macSize = ssl->readMacSize;
                if (rec->length > extraSlack + macSize + 17408U) {
                    errReason = 205;
                    alertCode = 22;
                    break;
                }
                if (rec->length < macSize) {
                    errReason = 160;
                    alertCode = 50;
                    break;
                }
                rec->length -= macSize;
                ssl->method->ssl3Enc->mac(ssl, computedMac, 0);
                if (sslMemcmp(computedMac, rec->data + rec->length, (int)macSize) != 0 || decBad != 0) {
                    errReason = 113;
                    alertCode = 20;
                    break;
                }
            }
        }

        if (rec->length > extraSlack + 16384U) {
            errReason = 146;
            alertCode = 22;
            break;
        }
        rec->off = 0;
        ssl->packetLength = 0;
        if (rec->length != 0) {
            return 1;
        }
    }

    sceSsl_lib_9CDCA0D7(20, 143, errReason, NULL, 0);
    ssl3SendAlert(ssl, 2, alertCode);
    return -1;
}

/* 0x01FD68 */
int LIBSSL_NOINLINE ssl3WriteBytes(Ssl *ssl, int recType, const u8 *buf, int len)
{
    int written = ssl->ssl3State->wnum;
    u32 remaining;

    ssl->rwstate = 1;
    ssl->ssl3State->wnum = 0;

    if ((sceSsl_lib_104F749D(ssl) & 0x3000) != 0 && ssl->inHandshake == 0) {
        int hsRet;
        if (ssl->handshakeFunc == NULL) {
            sceSsl_lib_9CDCA0D7(20, 159, 1101, NULL, 0);
            return -1;
        }
        hsRet = ssl->handshakeFunc(ssl);
        if (hsRet < 0) {
            return hsRet;
        }
        if (hsRet == 0) {
            sceSsl_lib_9CDCA0D7(20, 159, 229, NULL, 0);
            return -1;
        }
    }

    remaining = (u32)(len - written);
    for (;;) {
        u32 chunk = ssl->maxSendFragment;
        int sent;

        if (chunk > remaining) {
            chunk = remaining;
        }
        sent = ssl3DoWriteRecord(ssl, recType, buf + written, (int)chunk);
        if (ssl->inHandshake == 0) {
            SslMsgCb msgCb = NULL;
            void *msgArg = NULL;
            if (ssl->msgCallback != NULL) {
                msgCb = ssl->msgCallback;
                msgArg = ssl->msgCallbackArg;
            } else if (ssl->ctx->msgCallback != NULL) {
                msgCb = ssl->ctx->msgCallback;
                msgArg = ssl->ctx->msgCallbackArg;
            }
            if (msgCb != NULL) {
                msgCb(ssl, 8, msgArg);
            }
        }
        if (sent <= 0) {
            ssl->ssl3State->wnum = written;
            return sent;
        }
        if (recType == 22) {
            ssl3FinishMac(ssl, buf + written, sent);
        }
        written += sent;
        if ((u32)sent == remaining) {
            return written;
        }
        remaining -= (u32)sent;
    }
}

/* 0x01FF14 */
static int LIBSSL_NOINLINE ssl3DoWriteRecord(Ssl *ssl, int recType, const u8 *buf, int len)
{
    Ssl3State *ssl3State = ssl->ssl3State;
    Ssl3Record *wrec = &ssl3State->wrec;
    u32 macSize = 0;
    u8 *hdr;
    u8 *payload;

    if (ssl3State->wbufLeft != 0) {
        return ssl3WritePending(ssl, recType, buf, len);
    }
    if (ssl3State->alertDispatch != 0) {
        int alertRet = ssl3DispatchAlert(ssl);
        if (alertRet <= 0) {
            return alertRet;
        }
    }
    if (len == 0) {
        return 0;
    }

    if (ssl->session != NULL && ssl->encWriteCtx != NULL && ssl->writeHash != NULL) {
        macSize = ssl->writeMacSize;
    }

    hdr = ssl3State->wbufBuf;
    hdr[0] = (u8)recType;
    wrec->type = recType;
    hdr[1] = (u8)(ssl->version >> 8);
    hdr[2] = (u8)(ssl->version & 0xFF);
    payload = hdr + 5;
    wrec->data = payload;
    wrec->length = (u32)len;
    wrec->input = (u8 *)buf;
    sslMemcpy(payload, buf, (u32)len);
    wrec->input = wrec->data;

    if (macSize != 0) {
        if (ssl->method->ssl3Enc->mac(ssl, payload + wrec->length, 1) <= 0) {
            return -1;
        }
        wrec->data = payload;
        wrec->length += macSize;
        wrec->input = payload;
    }

    ssl->method->ssl3Enc->enc(ssl, 1);
    hdr[3] = (u8)(wrec->length >> 8);
    hdr[4] = (u8)(wrec->length & 0xFF);

    wrec->type = recType;
    wrec->length += 5U;
    ssl3State->wbufLeft = (int)wrec->length;
    ssl3State->wbufOffset = 0;
    ssl3State->wpendTot = len;
    ssl3State->wpendBuf = buf;
    ssl3State->wpendType = recType;
    ssl3State->wpendRet = len;

    return ssl3WritePending(ssl, recType, buf, len);
}

/* 0x0200EC */
static int LIBSSL_NOINLINE ssl3WritePending(Ssl *ssl, int recType, const u8 *buf, int len)
{
    Ssl3State *ssl3State = ssl->ssl3State;

    if (len < ssl3State->wpendTot || ssl3State->wpendBuf != buf || ssl3State->wpendType != recType) {
        sceSsl_lib_9CDCA0D7(20, 160, 127, NULL, 0);
        return -1;
    }

    for (;;) {
        int written;
        if (ssl->wbio != NULL) {
            ssl->rwstate = 2;
            written = sceSsl_lib_CED07EAE(ssl->wbio, ssl3State->wbufBuf + ssl3State->wbufOffset, ssl3State->wbufLeft);
        } else {
            sceSsl_lib_9CDCA0D7(20, 160, 128, NULL, 0);
            written = -1;
        }
        if (written == ssl3State->wbufLeft) {
            ssl3State->wbufLeft = 0;
            ssl->rwstate = 1;
            return ssl3State->wpendRet;
        }
        if (written <= 0) {
            return written;
        }
        ssl3State->wbufLeft -= written;
        ssl3State->wbufOffset += written;
    }
}

/* 0x0201F0 */
int LIBSSL_NOINLINE ssl3ReadBytes(Ssl *ssl, int recType, u8 *buf, int len)
{
    Ssl3Record *rrec;

    if (ssl->ssl3State->rbufBuf == NULL) {
        if (ssl3SetupBuffers(ssl) == 0) {
            return -1;
        }
    }

    if (ssl->inHandshake == 0 && (sceSsl_lib_104F749D(ssl) & 0x3000) != 0) {
        int hsRet;
        if (ssl->handshakeFunc == NULL) {
            sceSsl_lib_9CDCA0D7(20, 149, 1101, NULL, 0);
            return -1;
        }
        hsRet = ssl->handshakeFunc(ssl);
        if (hsRet < 0) {
            return hsRet;
        }
        if (hsRet == 0) {
            sceSsl_lib_9CDCA0D7(20, 149, 229, NULL, 0);
            return -1;
        }
    }

    for (;;) {
        Ssl3State *ssl3State = ssl->ssl3State;
        rrec = &ssl3State->rrec;
        ssl->rwstate = 1;

        if (rrec->length == 0 || ssl->rstate == 241) {
            int recRet = ssl3GetRecord(ssl);
            if (recRet <= 0) {
                return recRet;
            }
            ssl3State = ssl->ssl3State;
        }

        if (ssl3State->changeCipherSpec != 0 && rrec->type != 22) {
            sceSsl_lib_9CDCA0D7(20, 149, 145, NULL, 0);
            return -1;
        }
        if ((ssl3State->flags & 0x00800000U) != 0 && rrec->type != 20) {
            sceSsl_lib_9CDCA0D7(20, 149, 145, NULL, 0);
            return -1;
        }
        if ((ssl->shutdown & 2) != 0) {
            rrec->length = 0;
            ssl->rwstate = 1;
            return 0;
        }

        if (rrec->type == 22 && rrec->length == 4U && rrec->data[0] == 0) {
            SslInfoCb infoCb = ssl->infoCallback;
            if (rrec->data[1] != 0 || rrec->data[2] != 0 || rrec->data[3] != 0) {
                sceSsl_lib_9CDCA0D7(20, 149, 105, NULL, 0);
                return -1;
            }
            if (infoCb == NULL) {
                infoCb = ssl->ctx->infoCallback;
            }
            if (infoCb != NULL) {
                int savedState = ssl->state;
                ssl->state = 4353;
                infoCb(ssl, 4097, 1);
                ssl->state = savedState;
            }
            if (sceSsl_lib_104F749D(ssl) == 3 &&
                (ssl3State->flags & 1U) == 0 &&
                ssl3State->renegotiate == 0) {
                ssl3Renegotiate(ssl);
                if (ssl3RenegotiateCheck(ssl) != 0) {
                    int hsRet = ssl->handshakeFunc(ssl);
                    if (hsRet < 0) {
                        return hsRet;
                    }
                    if (hsRet == 0) {
                        sceSsl_lib_9CDCA0D7(20, 149, 229, NULL, 0);
                        return -1;
                    }
                }
            }
            rrec->length = 0;
            continue;
        }

        if (rrec->type == recType && (ssl->shutdown & 1) == 0) {
            break;
        }

        if (rrec->type == 21) {
            int alertLevel;
            int alertDesc;
            SslInfoCb infoCb = NULL;
            SslAlertCb alertCb = NULL;
            void *alertArg = NULL;

            if (rrec->length != 2U || rrec->off != 0) {
                sceSsl_lib_9CDCA0D7(20, 149, 101, NULL, 0);
                ssl3SendAlert(ssl, 2, 50);
                return -1;
            }
            alertLevel = rrec->data[0];
            alertDesc = rrec->data[1];
            rrec->length = 0;

            if (ssl->infoCallback != NULL) {
                infoCb = ssl->infoCallback;
            } else if (ssl->ctx->infoCallback != NULL) {
                infoCb = ssl->ctx->infoCallback;
            }
            if (ssl->alertCallback != NULL) {
                alertCb = ssl->alertCallback;
                alertArg = ssl->alertCallbackArg;
            } else if (ssl->ctx->alertCallback != NULL) {
                alertCb = ssl->ctx->alertCallback;
                alertArg = ssl->ctx->alertCallbackArg;
            }
            if (infoCb != NULL) {
                infoCb(ssl, 16388, (alertLevel << 8) | alertDesc);
            }
            if (alertCb != NULL) {
                alertCb(ssl, 16388, (alertLevel << 8) | alertDesc, alertArg);
            }

            if (alertLevel == 1) {
                ssl3State->warnAlert = alertDesc;
                if (alertDesc == 0) {
                    ssl->shutdown |= 2;
                    return 0;
                }
                rrec->length = 0;
                continue;
            }
            if (alertLevel == 2) {
                char numStr[16];
                ssl->rwstate = 1;
                ssl3State->fatalAlert = alertDesc;
                sceSsl_lib_9CDCA0D7(20, 149, alertDesc + 1000, NULL, 0);
                sslSprintf(numStr, "%d", alertDesc);
                sslErrAddErrorData(2, "SSL alert number ", numStr);
                ssl->shutdown |= 2;
                sceSsl_lib_B81AD643(ssl->ctx, ssl->session);
                return 0;
            }
            sceSsl_lib_9CDCA0D7(20, 149, 246, NULL, 0);
            ssl3SendAlert(ssl, 2, 47);
            return -1;
        }

        if ((ssl->shutdown & 1) != 0) {
            ssl->rwstate = 1;
            rrec->length = 0;
            return 0;
        }

        if (rrec->type == 20) {
            if (rrec->length != 1U || rrec->off != 0 || rrec->data[0] != 1U) {
                sceSsl_lib_9CDCA0D7(20, 149, 103, NULL, 0);
                return -1;
            }
            rrec->length = 0;
            ssl3State->changeCipherSpec = 1;
            if (ssl3DoChangeCipherSpec(ssl) == 0) {
                return -1;
            }
            continue;
        }

        if (rrec->type == 22 && ssl->inHandshake == 0) {
            int hsRet;
            if ((ssl->state & 0xFFF) == 3 && (ssl3State->flags & 1U) == 0) {
                if ((ssl3State->flags & 0x10U) != 0) {
                    ssl3State->flags &= ~0x10U;
                    ssl->state = 4560;
                } else {
                    ssl->state = 0x2000;
                    ssl->newSession = 1;
                }
            }
            hsRet = ssl->handshakeFunc(ssl);
            if (hsRet < 0) {
                return hsRet;
            }
            if (hsRet == 0) {
                sceSsl_lib_9CDCA0D7(20, 149, 229, NULL, 0);
                return -1;
            }
            ssl->rwstate = 3;
            {
                SslBio *rbio = sceSsl_lib_2E2E2B09(ssl);
                sceSsl_lib_88A96ED0(rbio);
                sceSsl_lib_8A2F6963(rbio);
            }
            return -1;
        }

        if (rrec->type == 23) {
            if (ssl3State->inReadAppData != 0 && ssl3State->totalRenegotiations != 0) {
                int st = ssl->state;
                if (((st & 0x1000) != 0 && st >= 4368 && st < 4385) ||
                    ((st & 0x2000) != 0 && st >= 8464 && st < 8481)) {
                    ssl3State->inReadAppData = 0;
                    return -1;
                }
            }
            sceSsl_lib_9CDCA0D7(20, 149, 245, NULL, 0);
            ssl3SendAlert(ssl, 2, 10);
            return -1;
        }

        if (rrec->type >= 20 && rrec->type <= 22) {
            sceSsl_lib_9CDCA0D7(20, 149, 245, NULL, 0);
            ssl3SendAlert(ssl, 2, 10);
            return -1;
        }
        if (ssl->version != 0x0301) {
            sceSsl_lib_9CDCA0D7(20, 149, 245, NULL, 0);
            ssl3SendAlert(ssl, 2, 10);
            return -1;
        }
    }

    if ((sceSsl_lib_104F749D(ssl) & 0x3000) != 0 && recType == 23 && ssl->encReadCtx == NULL) {
        sceSsl_lib_9CDCA0D7(20, 149, 100, NULL, 0);
        ssl3SendAlert(ssl, 2, 10);
        return -1;
    }

    if (ssl->inHandshake == 0) {
        SslMsgCb msgCb = NULL;
        void *msgArg = NULL;
        if (ssl->msgCallback != NULL) {
            msgCb = ssl->msgCallback;
            msgArg = ssl->msgCallbackArg;
        } else if (ssl->ctx->msgCallback != NULL) {
            msgCb = ssl->ctx->msgCallback;
            msgArg = ssl->ctx->msgCallbackArg;
        }
        if (msgCb != NULL) {
            msgCb(ssl, 4, msgArg);
        }
    }

    if (len > 0) {
        u32 copyLen = rrec->length;
        if (copyLen > (u32)len) {
            copyLen = (u32)len;
        }
        sslMemcpy(buf, rrec->data + rrec->off, copyLen);
        rrec->off += copyLen;
        rrec->length -= copyLen;
        if (rrec->length == 0) {
            ssl->rstate = 240;
            rrec->off = 0;
        }
        if (recType == 22) {
            ssl3FinishMac(ssl, buf, (int)copyLen);
        }
        return (int)copyLen;
    }
    return len;
}

/* 0x02095C */
static int LIBSSL_NOINLINE ssl3DoChangeCipherSpec(Ssl *ssl)
{
    int which = 33;
    int savedState = ssl->state;
    int notifyState = 4512;
    SslInfoCb infoCb;
    const char *label;
    int labelLen;

    if ((savedState & 0x2000) == 0) {
        which = 17;
        notifyState = 4544;
    }

    infoCb = ssl->infoCallback;
    if (infoCb == NULL) {
        infoCb = ssl->ctx->infoCallback;
    }
    if (infoCb != NULL) {
        ssl->state = notifyState;
        infoCb(ssl, 4097, 1);
        ssl->state = savedState;
    }

    if (ssl->ssl3State->tmpKeyBlock == NULL) {
        ssl->session->cipher = ssl->ssl3State->tmpNewCipher;
        if (ssl->method->ssl3Enc->setupKeyBlock(ssl) == 0) {
            return 0;
        }
    }
    if (ssl->method->ssl3Enc->changeCipherState(ssl, which) == 0) {
        return 0;
    }

    if ((ssl->state & 0x1000) != 0 || (ssl->ssl3State->flags & 0x10U) != 0) {
        label = ssl->method->ssl3Enc->serverFinishedLabel;
        labelLen = ssl->method->ssl3Enc->serverFinishedLabelLen;
    } else {
        label = ssl->method->ssl3Enc->clientFinishedLabel;
        labelLen = ssl->method->ssl3Enc->clientFinishedLabelLen;
    }

    return (ssl->method->ssl3Enc->finalFinishMac(ssl, ssl->ssl3State->finishDg1, ssl->ssl3State->finishDg2,
                                                 label, labelLen, ssl->ssl3State->tmpPeerFinishMd) != 0) ? 1 : 0;
}

/* 0x020AA4 */
int LIBSSL_NOINLINE ssl3SendAlert(Ssl *ssl, int level, int desc)
{
    int mapped = ssl->method->ssl3Enc->alertValue(desc);

    if (mapped < 0) {
        return 1;
    }
    if (level == 2 && ssl->session != NULL) {
        sceSsl_lib_B81AD643(ssl->ctx, ssl->session);
    }
    ssl->ssl3State->sendAlert[0] = (u8)level;
    ssl->ssl3State->sendAlert[1] = (u8)mapped;
    ssl->ssl3State->alertDispatch = 1;
    if (ssl->ssl3State->wbufLeft == 0) {
        return ssl3DispatchAlert(ssl);
    }
    return -1;
}

/* 0x020B54 */
int LIBSSL_NOINLINE ssl3DispatchAlert(Ssl *ssl)
{
    int ret;
    SslInfoCb infoCb = NULL;
    SslAlertCb alertCb = NULL;
    void *alertArg = NULL;
    int alertWord;

    ssl->ssl3State->alertDispatch = 0;
    ret = ssl3DoWriteRecord(ssl, 21, ssl->ssl3State->sendAlert, 2);
    if (ret <= 0) {
        ssl->ssl3State->alertDispatch = 1;
        return ret;
    }

    if (ssl->ssl3State->sendAlert[0] == 2) {
        sceSsl_lib_9566709F(ssl->wbio, 11, 0, NULL);
    }
    if (ssl->unbuffered != 0) {
        ssl->rwstate = 2;
        if (sceSsl_lib_9566709F(ssl->wbio, 11, 0, NULL) < 0) {
            if (sceSsl_lib_089FF1F1(ssl->wbio) != 0) {
                ssl->ssl3State->alertDispatch = 1;
            }
            return -1;
        }
        ssl->rwstate = 1;
    }

    if (ssl->infoCallback != NULL) {
        infoCb = ssl->infoCallback;
    } else if (ssl->ctx->infoCallback != NULL) {
        infoCb = ssl->ctx->infoCallback;
    }
    alertWord = ((int)(signed char)ssl->ssl3State->sendAlert[0] << 8) |
                (int)(signed char)ssl->ssl3State->sendAlert[1];
    if (infoCb != NULL) {
        infoCb(ssl, 16392, alertWord);
    }

    if (ssl->alertCallback != NULL) {
        alertCb = ssl->alertCallback;
        alertArg = ssl->alertCallbackArg;
    } else if (ssl->ctx->alertCallback != NULL) {
        alertCb = ssl->ctx->alertCallback;
        alertArg = ssl->ctx->alertCallbackArg;
    }
    if (alertCb != NULL) {
        alertCb(ssl, 16392, alertWord, alertArg);
    }
    return ret;
}

/* 0x020CC0 */
int LIBSSL_NOINLINE ssl3DoWrite(Ssl *ssl, int recType)
{
    int written = ssl3WriteBytes(ssl, recType,
                                 (const u8 *)ssl->initBuf->data + ssl->initOff,
                                 ssl->initNum);
    if (written == ssl->initNum) {
        return 1;
    }
    if (written < 0) {
        return -1;
    }
    ssl->initOff += written;
    ssl->initNum -= written;
    return 0;
}
