#ifndef ERNIE_AUTHKEYS_H
#define ERNIE_AUTHKEYS_H

/* Expanded factory handshake key: 15 AES-256 round keys. */
extern const unsigned long factoryRoundKeys[60];

/* Shared key for the application nonce generator. */
extern const unsigned long randomKey[4];

/* IV shared by the host handshake and secure NVS. */
extern const unsigned char handshakeIv[16];

/* Serial authentication keys and packet constants. */
extern const unsigned char serialHeader[4];
extern const unsigned char serialIv[16];
extern const unsigned char serialKey0First[16];
extern const unsigned char serialKey0Second[16];
extern const unsigned char serialKey0Check[16];
extern const unsigned char serialKey1First[16];
extern const unsigned char serialKey1Second[16];
extern const unsigned char serialKey1Check[16];
extern const unsigned char serialKey2First[16];
extern const unsigned char serialKey2Second[16];
extern const unsigned char serialKey2Check[16];

#endif
