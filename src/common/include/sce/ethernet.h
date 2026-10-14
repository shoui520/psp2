/* Ethernet and ARP records extracted from NetBSD 2.1 net/if_ether.h and
 * net/if_arp.h. The IPv4 specialization follows their address ordering.
 */
/*
 * Copyright (c) 1982, 1986, 1993
 * The Regents of the University of California. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#define ETHER_ADDR_LEN 6

struct ether_header {
	uint8_t ether_dhost[ETHER_ADDR_LEN];
	uint8_t ether_shost[ETHER_ADDR_LEN];
	uint16_t ether_type;
} __attribute__((__packed__));

struct arphdr {
	uint16_t ar_hrd;
	uint16_t ar_pro;
	uint8_t ar_hln;
	uint8_t ar_pln;
	uint16_t ar_op;
} __attribute__((__packed__));

struct ether_arp {
	struct arphdr ea_hdr;
	uint8_t arp_sha[ETHER_ADDR_LEN];
	uint8_t arp_spa[4];
	uint8_t arp_tha[ETHER_ADDR_LEN];
	uint8_t arp_tpa[4];
};

struct EthernetArpFrame {
	struct ether_header ethernet;
	struct ether_arp arp;
};

_Static_assert(sizeof(struct ether_header) == 14, "Ethernet wire header");
_Static_assert(sizeof(struct arphdr) == 8, "ARP fixed wire header");
_Static_assert(sizeof(struct ether_arp) == 28, "Ethernet IPv4 ARP payload");
_Static_assert(sizeof(struct EthernetArpFrame) == 42, "Ethernet IPv4 ARP frame");
_Static_assert(offsetof(struct EthernetArpFrame, arp) == 14, "ARP frame payload");
_Static_assert(offsetof(struct ether_arp, arp_sha) == 8, "ARP sender hardware address");
_Static_assert(offsetof(struct ether_arp, arp_spa) == 14, "ARP sender protocol address");
_Static_assert(offsetof(struct ether_arp, arp_tha) == 18, "ARP target hardware address");
_Static_assert(offsetof(struct ether_arp, arp_tpa) == 24, "ARP target protocol address");
