# os0:sm: the secure modules, MeP programs the Cmep loads at 0x80b000 (SELFs on the
# device). fw/<FW>.list names the modules of each firmware; each module's folder holds
# its sources and includes ../sm.mk (the shared entry, constructors,
# link script and rules).
#
#   tools/mepbuild.sh security/sm FW=<FW> O=<build directory> [MODULES=<names>]
#   -> $(O)/<FW>/<module>/<module>.elf
#
# Skeletons: each module's sm_main is a stub. The firmwares differ only in data (16 to
# 80 bytes near the end of each image; update_service_sm also in 4.00's code), which
# goes into per-firmware data when the modules are written.

GCC ?= mep-elf-
BINUTILS ?= mep-elf-
O ?= build
FW ?= 4.00-DEBUGGER
MODULES ?= $(shell cat fw/$(FW).list)

.PHONY: all $(MODULES)
all: $(MODULES)
$(MODULES):
	$(MAKE) -C $@ GCC=$(GCC) BINUTILS=$(BINUTILS) O=$(abspath $(O)) FW=$(FW)
