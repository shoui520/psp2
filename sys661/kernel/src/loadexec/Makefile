TARGET := loadexec_01g
SRCS   := kl4e.c loadexec.c
EXTRA_CFLAGS := -I$(O)

include ../../build.mak

REBOOT_O   := $(O)/reboot
REBOOT_BIN := $(REBOOT_O)/reboot.bin

$(REBOOT_BIN): FORCE
	$(MAKE) -C reboot O=$(abspath $(REBOOT_O))

$(O)/reboot.h: $(REBOOT_BIN) $(TOOLS_DIR)/mkkl3e.py | $(O)
	$(PYTHON) $(TOOLS_DIR)/mkkl3e.py $< -o $@ --magic KL3E --c-array g_reboot --pad-to 9856

$(O)/loadexec.o: $(O)/reboot.h

.PHONY: FORCE
FORCE:
