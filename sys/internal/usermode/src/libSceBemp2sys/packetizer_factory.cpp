#include "be_mp2sys_internal.h"

BePacketizer *createPacketizer(uint32_t type)
{
	switch (type) {
	case BE_PACKETIZER_TYPE_PES:
		return new PesPacketizer();
	case BE_PACKETIZER_TYPE_PAT:
		return new PatPacketizer();
	case BE_PACKETIZER_TYPE_CAT:
		return new CatPacketizer();
	case BE_PACKETIZER_TYPE_PMT:
		return new PmtPacketizer();
	case BE_PACKETIZER_TYPE_SI:
		return new SiPacketizer();
	case BE_PACKETIZER_TYPE_TS:
		return new TsPacketizer();
	default:
		return 0;
	}
}
