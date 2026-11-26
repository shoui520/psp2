TARGET := np_service
SRCS   := np_service_main.c np_xml.c np_profile.c np_lookup.c np_roster.c
EXTRA_CFLAGS := -O2 -fno-strict-aliasing

include ../../build.mak

LDFLAGS += -Wl,-u,__udivdi3,-u,__umoddi3
