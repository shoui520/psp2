#ifndef IFHANDLE_MBUF_H
#define IFHANDLE_MBUF_H

#include <stdint.h>
#include <stddef.h>

#define M_EXT       0x0001
#define M_PKTHDR    0x0002
#define M_RDONLY    0x0008
#define M_BCAST     0x0100
#define M_MCAST     0x0200

typedef struct SceNetMbuf {
	struct SceNetMbuf *m_next;      /* +0x00 */
	struct SceNetMbuf *m_nextpkt;   /* +0x04 */
	uint8_t *m_data;                /* +0x08 */
	int32_t m_len;                  /* +0x0c */
	uint16_t m_type;                /* +0x10 */
	uint16_t m_flags;               /* +0x12 */
	union {
		struct {
			void *rcvif;                    /* +0x14 */
			int32_t len;                    /* +0x18 */
			uint32_t pad[5];                /* +0x1c..0x2c */
			union {
				struct {
					uint8_t *buf;                   /* +0x30 */
					void *free_fn;                  /* +0x34 */
					void *arg;                      /* +0x38 */
					uint32_t size;                  /* +0x3c */
					uint32_t type;                  /* +0x40 */
					struct SceNetMbuf *nextref;     /* +0x44 */
					struct SceNetMbuf *prevref;     /* +0x48 */
				} ext;
				uint8_t pktdat[0x100 - 0x30];       /* +0x30..0x100 */
			} payload;
		} pkt;
		uint8_t dat[0x100 - 0x14];                  /* +0x14..0x100 */
	} m_store;
} SceNetMbuf;

#define pkthdr_rcvif m_store.pkt.rcvif
#define pkthdr_len   m_store.pkt.len
#define pkthdr_pad   m_store.pkt.pad
#define ext_buf      m_store.pkt.payload.ext.buf
#define ext_free     m_store.pkt.payload.ext.free_fn
#define ext_arg      m_store.pkt.payload.ext.arg
#define ext_size     m_store.pkt.payload.ext.size
#define ext_type     m_store.pkt.payload.ext.type
#define ext_nextref  m_store.pkt.payload.ext.nextref
#define ext_prevref  m_store.pkt.payload.ext.prevref
#define MBUF_DAT(m)     ((m)->m_store.dat)
#define MBUF_PKTDAT(m)  ((m)->m_store.pkt.payload.pktdat)
#define MBUF_END(m)     (&(m)->m_store.dat[sizeof((m)->m_store.dat)])


#endif /* IFHANDLE_MBUF_H */
