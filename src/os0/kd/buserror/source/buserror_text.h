/* ES3 bus error attribute text. */
static const char *const ocpCommands[] = {
	"Idle", "Write", "Read", "ReadEx", "ReadLinked", "WriteNonPost",
	"WriteConditional", "Broadcast",
};

static const char *const ocpMasters[] = {
	"Reserved_0x00", "ARM Core0", "ARM Core1", "ARM Core2", "ARM Core3",
	"ARM L2", "Reserved_0x06", "Reserved_0x07", "Reserved_0x08",
	"Reserved_0x09", "Reserved_0x0A", "Reserved_0x0B", "Reserved_0x0C",
	"Reserved_0x0D", "Reserved_0x0E", "Reserved_0x0F",
	"DMAC0", "DMAC1", "DMAC2", "DMAC3", "DMAC4", "DMAC5",
	"Reserved_0x16", "GPU", "Venezia", "VIP vdpd", "VIP vdpm", "VIP bap",
	"Reserved_0x1C", "IFTU0a", "IFTU0b", "IFTU1a", "IFTU1b", "IFTU2",
	"Reserved_0x22", "Reserved_0x23", "USB1 Host EHCI", "USB1 Host OHCI",
	"Reserved_0x26", "Sensor in 0", "Sensor in 1", "Reserved_0x29",
	"LCD DMAC", "Performance Monitor", "USB2 Device DMAC", "Sub LCD",
	"DMAC6", "USB0 Host EHCI", "USB0 Host OHCI", "USB2 Host EHCI", "USB2 Host OHCI",
	"USB0 Device DMAC", "Reserved_0x34", "Reserved_0x35", "Reserved_0x36",
	"USB1 Device DMAC", "Reserved_0x38", "Reserved_0x39", "Reserved_0x3A",
	"SD/HSMMC0", "SD/HSMMC1", "SD/HSMMC2", "SD/HSMMC3", "Memory Stick",
};
