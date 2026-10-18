/* Bluetooth registration interface shared by controller providers. */
#pragma once

/* Private Sony name unavailable. Registers a peer paired by another host;
 * previousHostAddress is six address bytes, not a PIN. The output buffers
 * receive the current six-byte host address and a sixteen-byte link key.
 */
int SceBtForDriver_10D68FAE(const void *peerAddress, const void *deviceClass,
			    const void *previousHostAddress, unsigned int vendorId,
			    unsigned int productId, unsigned int deviceVersion,
			    const char *name, void *hostAddressOut, void *linkKeyOut);

/* SceBtForDriver NID 0x50710281; the two words form the peer address. */
int SceBtForDriver_50710281(unsigned int mac0, unsigned int mac1);
