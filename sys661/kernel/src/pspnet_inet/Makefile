TARGET := pspnet_inet
SRCS   := \
	rijndael_asm.S \
	inet_ifctrl.c \
	uipc_domain.c \
	uipc_socket.c \
	uipc_socket2.c \
	inet_socket_api.c \
	rijndael.c \
	if_util_ether.c \
	radix.c \
	route.c \
	rtsock_raw.c \
	slcompress.c \
	if_arp.c \
	in_pcb.c \
	ip_input.c \
	ip_output.c \
	ip_icmp_igmp_raw.c \
	udp_usrreq.c \
	tcp_input.c \
	tcp_output_subr.c \
	tcp_timer_usrreq.c \
	if_spppsubr.c \
	if_pppoe.c \
	inet_init_addr.c
EXTRA_CFLAGS := -O2 -fno-strict-aliasing -I$(CURDIR)

include ../../build.mak
