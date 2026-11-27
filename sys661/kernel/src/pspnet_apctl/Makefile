TARGET := pspnet_apctl
SRCS := pspnet_apctl.c dhcp/dhclient.c poeclient/poeclient.c apctl_8021x.c apctl_wpa.c apctl_btdun.c
EXTRA_CFLAGS := -O2 -fno-strict-aliasing -I$(CURDIR)

include ../../build.mak

$(shell mkdir -p $(O)/dhcp $(O)/poeclient)
